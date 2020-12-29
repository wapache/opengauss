/* -------------------------------------------------------------------------
 *
 * poolutils.c
 *
 * Utilities for Postgres-XC pooler
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 2010-2012 Postgres-XC Development Group
 *
 * IDENTIFICATION
 *    $$
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"
#include "knl/knl_variable.h"
#include "miscadmin.h"
#include "libpq/pqsignal.h"
#include "libpq/libpq-int.h"

#include "access/gtm.h"
#include "access/xact.h"
#include "commands/dbcommands.h"
#include "commands/prepare.h"
#include "funcapi.h"
#include "gssignal/gs_signal.h"
#include "nodes/nodes.h"
#include "pgxc/locator.h"
#include "pgxc/nodemgr.h"
#include "pgxc/pgxc.h"
#include "pgxc/pgxcnode.h"
#include "pgxc/poolmgr.h"
#include "pgxc/poolutils.h"
#include "storage/procarray.h"
#include "threadpool/threadpool.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "utils/elog.h"

/*
 * pgxc_pool_check
 *
 * Check if Pooler information in catalog is consistent
 * with information cached.
 */
Datum pgxc_pool_check(PG_FUNCTION_ARGS)
{
    if (!superuser() && !(isOperatoradmin(GetUserId()) && u_sess->attr.attr_security.operation_mode))
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
            (errmsg("must be system admin or operator admin in operation mode to manage pooler"))));

    /* A Datanode has no pooler active, so do not bother about that */
    if (IS_PGXC_DATANODE)
        PG_RETURN_BOOL(true);

    /* Simply check with pooler */
    PG_RETURN_BOOL(PoolManagerCheckConnectionInfo());
}

/*
 * pgxc_pool_reload
 *
 * Reload data cached in pooler and reload node connection
 * information in all the server sessions. This aborts all
 * the existing transactions on this node and reinitializes pooler.
 * First a lock is taken on Pooler to keep consistency of node information
 * being updated. If connection information cached is already consistent
 * in pooler, reload is not executed.
 * Reload itself is made in 2 phases:
 * 1) Update database pools with new connection information based on catalog
 *    pgxc_node. Remote node pools are changed as follows:
 *	  - cluster nodes dropped in new cluster configuration are deleted and all
 *      their remote connections are dropped.
 *    - cluster nodes whose port or host value is modified are dropped the same
 *      way, as connection information has changed.
 *    - cluster nodes whose port or host has not changed are kept as is, but
 *      reorganized respecting the new cluster configuration.
 *    - new cluster nodes are added.
 * 2) Reload information in all the sessions of the local node.
 *    All the sessions in server are signaled to reconnect to pooler to get
 *    newest connection information and update connection information related
 *    to remote nodes. This results in losing prepared and temporary objects
 *    in all the sessions of server. All the existing transactions are aborted
 *    and a WARNING message is sent back to client.
 *    Session that invocated the reload does the same process, but no WARNING
 *    message is sent back to client.
 */
Datum pgxc_pool_reload(PG_FUNCTION_ARGS)
{
    MemoryContext old_context;

    if (!superuser() && !(isOperatoradmin(GetUserId()) && u_sess->attr.attr_security.operation_mode))
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
            (errmsg("must be system admin or operator admin in operation mode to manage pooler"))));

    if (IsTransactionBlock())
        ereport(ERROR,
            (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                errmsg("pgxc_pool_reload cannot run inside a transaction block")));

    /* A Datanode has no pooler active, so do not bother about that */
    if (IS_PGXC_DATANODE)
        PG_RETURN_BOOL(true);

    /* Reload connection information in pooler */
    PoolManagerReloadConnectionInfo();

    /* Be sure it is done consistently */
    if (!PoolManagerCheckConnectionInfo()) {
        PG_RETURN_BOOL(false);
    }

    /* Signal other sessions to reconnect to pooler */
    ReloadConnInfoOnBackends();

    /* Session is being reloaded, handle prepared and temporary objects */
    HandlePreparedStatementsForReload();

    /* Now session information is reset in correct memory context */
    old_context = MemoryContextSwitchTo(SESS_GET_MEM_CXT_GROUP(MEMORY_CONTEXT_COMMUNICATION));

    /* Reinitialize session, while old pooler connection is active */
    InitMultinodeExecutor(true);

    /* And reconnect to pool manager */
    PoolManagerReconnect();

    MemoryContextSwitchTo(old_context);

    PG_RETURN_BOOL(true);
}

/*
 * CleanConnection()
 *
 * Utility to clean up Postgres-XC Pooler connections.
 * This utility is launched to all the Coordinators of the cluster
 *
 * Use of CLEAN CONNECTION is limited to a super user.
 * It is advised to clean connections before shutting down a Node or drop a Database.
 *
 * SQL query synopsis is as follows:
 * CLEAN CONNECTION TO
 *		(COORDINATOR num | DATANODE num | ALL {FORCE})
 *		[ FOR DATABASE dbname ]
 *		[ TO USER username ]
 *
 * Connection cleaning can be made on a chosen database called dbname
 * or/and a chosen user.
 * Cleaning is done for all the users of a given database
 * if no user name is specified.
 * Cleaning is done for all the databases for one user
 * if no database name is specified.
 *
 * It is also possible to clean connections of several Coordinators or Datanodes
 * Ex:	CLEAN CONNECTION TO DATANODE dn1,dn2,dn3 FOR DATABASE template1
 *		CLEAN CONNECTION TO COORDINATOR co2,co4,co3 FOR DATABASE template1
 *		CLEAN CONNECTION TO DATANODE dn2,dn5 TO USER postgres
 *		CLEAN CONNECTION TO COORDINATOR co6,co1 FOR DATABASE template1 TO USER postgres
 *
 * Or even to all Coordinators/Datanodes at the same time
 * Ex:	CLEAN CONNECTION TO DATANODE * FOR DATABASE template1
 *		CLEAN CONNECTION TO COORDINATOR * FOR DATABASE template1
 *		CLEAN CONNECTION TO COORDINATOR * TO USER postgres
 *		CLEAN CONNECTION TO COORDINATOR * FOR DATABASE template1 TO USER postgres
 *
 * When FORCE is used, all the transactions using pooler connections are aborted,
 * and pooler connections are cleaned up.
 * Ex:	CLEAN CONNECTION TO ALL FORCE FOR DATABASE template1;
 *		CLEAN CONNECTION TO ALL FORCE TO USER postgres;
 *		CLEAN CONNECTION TO ALL FORCE FOR DATABASE template1 TO USER postgres;
 *
 * FORCE can only be used with TO ALL, as it takes a lock on pooler to stop requests
 * asking for connections, aborts all the connections in the cluster, and cleans up
 * pool connections associated to the given user and/or database.
 */
void CleanConnection(CleanConnStmt* stmt)
{
#ifndef ENABLE_MULTIPLE_NODES
    Assert(false);
    DISTRIBUTED_FEATURE_NOT_SUPPORTED();
    return;
#else
    ListCell* nodelist_item = NULL;
    List* co_list = NIL;
    List* dn_list = NIL;
    List* stmt_nodes = NIL;
    char* dbname = stmt->dbname;
    char* username = stmt->username;
    bool is_coord = stmt->is_coord;
    bool is_force = stmt->is_force;

    /* Only a DB administrator can clean pooler connections */
    if (!superuser())
        ereport(
            ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to clean pool connections")));

    /* Database name or user name is mandatory */
    if (!dbname && !username)
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("must define Database name or user name")));

    /* Check if the Database exists by getting its Oid */
    if (dbname && !OidIsValid(get_database_oid(dbname, true))) {
        ereport(WARNING, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
        return;
    }

    /* Check if role exists */
    if (username && !OidIsValid(get_role_oid(username, false))) {
        ereport(WARNING, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", username)));
        return;
    }

    /*
     * FORCE is activated,
     * Send a SIGTERM signal to all the processes and take a lock on Pooler
     * to avoid backends to take new connections when cleaning.
     * Only Disconnect is allowed.
     */
    if (is_force) {
        int loop = 0;
        int* proc_pids = NULL;
        int num_proc_pids, count;

        num_proc_pids = PoolManagerAbortTransactions(dbname, username, &proc_pids);

        /*
         * Watch the processes that received a SIGTERM.
         * At the end of the timestamp loop, processes are considered as not finished
         * and force the connection cleaning has failed
         */

        while (num_proc_pids > 0 && loop < TIMEOUT_CLEAN_LOOP) {
            for (count = num_proc_pids - 1; count >= 0; count--) {
                switch (kill(proc_pids[count], 0)) {
                    case 0: /* Termination not done yet */
                        break;

                    default:
                        /* Move tail pid in free space */
                        proc_pids[count] = proc_pids[num_proc_pids - 1];
                        num_proc_pids--;
                        break;
                }
            }
            pg_usleep(1000000);
            loop++;
        }

        if (proc_pids)
            pfree(proc_pids);

        if (loop >= TIMEOUT_CLEAN_LOOP)
            ereport(WARNING, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("All Transactions have not been aborted")));
    }

    foreach (nodelist_item, stmt->nodes) {
        char* node_name = strVal(lfirst(nodelist_item));
        Oid nodeoid = get_pgxc_nodeoid(node_name);
        if (!OidIsValid(nodeoid))
            ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("PGXC Node %s: object not defined", node_name)));

        stmt_nodes = lappend_int(stmt_nodes, PGXCNodeGetNodeId(nodeoid, get_pgxc_nodetype(nodeoid)));
    }

    /* Build lists to be sent to Pooler Manager */
    if (stmt->nodes && is_coord)
        co_list = stmt_nodes;
    else if (stmt->nodes && !is_coord)
        dn_list = stmt_nodes;
    else {
        co_list = GetAllCoordNodes();
        dn_list = GetAllDataNodes();
    }

    /*
     * If force is launched, send a signal to all the processes
     * that are in transaction and take a lock.
     * Get back their process number and watch them locally here.
     * Process are checked as alive or not with pg_usleep and when all processes are down
     * go out of the control loop.
     * If at the end of the loop processes are not down send an error to client.
     * Then Make a clean with normal pool cleaner.
     * Always release the lock when calling CLEAN CONNECTION.
     */

    /* Finish by contacting Pooler Manager */
    PoolManagerCleanConnection(dn_list, co_list, dbname, username);

    /* Clean up memory */
    if (co_list)
        list_free(co_list);
    if (dn_list)
        list_free(dn_list);
#endif
}

/*
 * DropDBCleanConnection
 *
 * Clean Connection for given database before dropping it
 * FORCE is not used here
 */
void DropDBCleanConnection(const char* dbname)
{
#ifndef ENABLE_MULTIPLE_NODES
    Assert(false);
    DISTRIBUTED_FEATURE_NOT_SUPPORTED();
    return;
#else
    List* co_list = GetAllCoordNodes();
    List* dn_list = GetAllDataNodes();

    /* Check permissions for this database */
    AclResult aclresult = pg_database_aclcheck(get_database_oid(dbname, true), GetUserId(), ACL_ALTER | ACL_DROP);
    if (aclresult != ACLCHECK_OK && !pg_database_ownercheck(get_database_oid(dbname, true), GetUserId())) {
        aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_DATABASE, dbname);
    }

    PoolManagerCleanConnection(dn_list, co_list, dbname, NULL);

    /* Clean up memory */
    if (co_list)
        list_free(co_list);
    if (dn_list)
        list_free(dn_list);
#endif
}

/*
 * DropRoleCleanConnection
 *
 * Clean Connection for given role before dropping it
 */
void DropRoleCleanConnection(const char* username)
{
    Assert(false);
    DISTRIBUTED_FEATURE_NOT_SUPPORTED();
    return;
}

/*
 * processPoolerReload
 *
 * This is called when PROCSIG_PGXCPOOL_RELOAD is activated.
 * Abort the current transaction if any, then reconnect to pooler.
 * and reinitialize session connection information.
 */
void processPoolerReload(void)
{
    Assert(false);
    DISTRIBUTED_FEATURE_NOT_SUPPORTED();
    return;
}

// Check if Pooler connection status is normal.
//
Datum pgxc_pool_connection_status(PG_FUNCTION_ARGS)
{
    List* co_list = GetAllCoordNodes();
    List* dn_list = GetAllDataNodes();
    bool status = true;

    if (!superuser() && !(isOperatoradmin(GetUserId()) && u_sess->attr.attr_security.operation_mode))
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
            (errmsg("must be system admin or operator admin in operation mode to manage pooler"))));

    /* cannot exec func in transaction or a write query */
    if (IsTransactionBlock() || TransactionIdIsValid(GetTopTransactionIdIfAny()))
        ereport(ERROR,
            (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                (errmsg(
                    "can not execute pgxc_pool_connection_status in a transaction block or a write transaction."))));
    // A Datanode has no pooler active, so do not bother about that.
    //
    if (IS_PGXC_DATANODE)
        PG_RETURN_BOOL(true);

    status = PoolManagerConnectionStatus(dn_list, co_list);

    // Clean up memory.
    //
    if (co_list != NIL)
        list_free(co_list);
    if (dn_list != NIL)
        list_free(dn_list);

    PG_RETURN_BOOL(status);
}

/*
 * Validate pooler connections by comparing the hostis_primary column in pgxc_node
 * with connections info hold in each pooler agent.
 * if co_node_name specified, validate the pooler connections to it.
 *
 * Note: if we do not use co_node_name, we should set co_node_name to ' ', not ''.
 */
Datum pg_pool_validate(PG_FUNCTION_ARGS)
{
    FuncCallContext* funcctx = NULL;
    InvalidBackendEntry* entry = NULL;
    errno_t rc;
    int ret = 0;
    char node_name[NAMEDATALEN];

    rc = memset_s(node_name, NAMEDATALEN, '\0', NAMEDATALEN);
    securec_check(rc, "\0", "\0");

    if (!superuser() && !(isOperatoradmin(GetUserId()) && u_sess->attr.attr_security.operation_mode))
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
            (errmsg("must be system admin or operator admin in operation mode to manage pooler"))));

    if (IsTransactionBlock())
        ereport(ERROR,
            (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                errmsg("pg_pool_validate cannot run inside a transaction block")));

    if (IS_PGXC_DATANODE && !IS_SINGLE_NODE)
        PG_RETURN_NULL();

    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        TupleDesc tupdesc;
        bool clear = PG_GETARG_BOOL(0);
        char* co_node_name = PG_GETARG_CSTRING(1);

        rc = strncpy_s(node_name, NAMEDATALEN, co_node_name, strlen(co_node_name));
        securec_check(rc, "\0", "\0");
        node_name[NAMEDATALEN - 1] = '\0';

        funcctx = SRF_FIRSTCALL_INIT();

        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /* construct a tuple descriptor for the result row. */
        tupdesc = CreateTemplateTupleDesc(2, false, TAM_HEAP);
        TupleDescInitEntry(tupdesc, (AttrNumber)1, "pid", INT8OID, -1, 0);
        TupleDescInitEntry(tupdesc, (AttrNumber)2, "node_name", TEXTOID, -1, 0);
        funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        funcctx->user_fctx = (void*)PoolManagerValidateConnection(clear, node_name, funcctx->max_calls);

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();
    entry = (InvalidBackendEntry*)funcctx->user_fctx;

    if (funcctx->call_cntr < funcctx->max_calls) {
        Datum values[2];
        bool nulls[2];
        char* namestring = NULL;
        HeapTuple tuple;
        char element[MAXPGPATH] = {0};
        int i = 0;

        rc = memset_s(values, sizeof(values), 0, sizeof(values));
        securec_check(rc, "\0", "\0");
        rc = memset_s(nulls, sizeof(nulls), false, sizeof(nulls));
        securec_check(rc, "\0", "\0");

        entry += funcctx->call_cntr;

        namestring = (char*)palloc0(entry->total_len);
        ret = snprintf_s(namestring, entry->total_len, MAXPGPATH - 1, "%s", entry->node_name[0]);
        securec_check_ss(ret, "\0", "\0");
        for (i = 1; i < entry->num_nodes; i++) {
            ret = snprintf_s(element, sizeof(element), MAXPGPATH - 1, ",%s", entry->node_name[i]);
            securec_check_ss(ret, "\0", "\0");
            rc = strncat_s(namestring, entry->total_len, element, entry->total_len - strlen(namestring));
            securec_check(rc, "\0", "\0");
        }

        values[0] = Int64GetDatum(entry->tid);
        values[1] = CStringGetTextDatum(namestring);
        tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
        pfree(namestring);
        namestring = NULL;

        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else
        SRF_RETURN_DONE(funcctx);
}

/* Set pooler ping */
Datum pg_pool_ping(PG_FUNCTION_ARGS)
{
    bool mod = PG_GETARG_BOOL(0);

    if (!superuser() && !(isOperatoradmin(GetUserId()) && u_sess->attr.attr_security.operation_mode))
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
            (errmsg("must be system admin or operator admin in operation mode to manage pooler"))));

    if (IsTransactionBlock())
        ereport(ERROR,
            (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION), errmsg("pg_pool_ping cannot run inside a transaction block")));

    /* A Datanode has no pooler active, so do not bother about that */
    if (IS_PGXC_DATANODE)
        PG_RETURN_BOOL(true);

    set_pooler_ping(mod);

    PG_RETURN_BOOL(true);
}

#ifdef ENABLE_MULTIPLE_NODES
/*
 * HandlePoolerReload
 *
 * This is called when PROCSIG_PGXCPOOL_RELOAD is activated.
 * Abort the current transaction if any, then reconnect to pooler.
 * and reinitialize session connection information.
 */
void HandlePoolerReload(void)
{
    MemoryContext old_context;

    /* A Datanode has no pooler active, so do not bother about that */
    if (IS_PGXC_DATANODE)
        return;

    /* Abort existing xact if any */
    AbortCurrentTransaction();

    /* Session is being reloaded, drop prepared and temporary objects */
    DropAllPreparedStatements();

    /* Now session information is reset in correct memory context */
    old_context = MemoryContextSwitchTo(THREAD_GET_MEM_CXT_GROUP(MEMORY_CONTEXT_COMMUNICATION));

    /* Need to be able to look into catalogs */
    CurrentResourceOwner = ResourceOwnerCreate(NULL, "ForPoolerReload", MEMORY_CONTEXT_COMMUNICATION);

    /* Reinitialize session, while old pooler connection is active */
    InitMultinodeExecutor(true);

    /* And reconnect to pool manager */
    PoolManagerReconnect();

    /* Send a message back to client regarding session being reloaded */
    ereport(WARNING,
        (errcode(ERRCODE_OPERATOR_INTERVENTION),
            errmsg("session has been reloaded due to a cluster configuration modification"),
            errdetail("Temporary and prepared objects hold by session have been"
                      " dropped and current transaction has been aborted.")));

    /* Release everything */
    ResourceOwnerRelease(CurrentResourceOwner, RESOURCE_RELEASE_BEFORE_LOCKS, true, true);
    ResourceOwnerRelease(CurrentResourceOwner, RESOURCE_RELEASE_LOCKS, true, true);
    ResourceOwnerRelease(CurrentResourceOwner, RESOURCE_RELEASE_AFTER_LOCKS, true, true);
    CurrentResourceOwner = NULL;

    MemoryContextSwitchTo(old_context);
}

#endif