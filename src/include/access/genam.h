/* -------------------------------------------------------------------------
 *
 * genam.h
 *	  POSTGRES generalized index access method definitions.
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/genam.h
 *
 * -------------------------------------------------------------------------
 */
#ifndef GENAM_H
#define GENAM_H

#include "access/sdir.h"
#include "access/skey.h"
#include "nodes/tidbitmap.h"
#include "storage/lock.h"
#include "utils/relcache.h"
#include "utils/snapshot.h"
#include "vecexecutor/vectorbatch.h"

/*
 * Struct for statistics returned by ambuild
 */
typedef struct IndexBuildResult {
    double heap_tuples;  /* # of tuples seen in parent table */
    double index_tuples; /* # of tuples inserted into index */
    double* all_part_tuples;
} IndexBuildResult;

/*
 * Struct for input arguments passed to ambulkdelete and amvacuumcleanup
 *
 * num_heap_tuples is accurate only when estimated_count is false;
 * otherwise it's just an estimate (currently, the estimate is the
 * prior value of the relation's pg_class.reltuples field).  It will
 * always just be an estimate during ambulkdelete.
 */
typedef struct IndexVacuumInfo {
    Relation index;                /* the index being vacuumed */
    bool analyze_only;             /* ANALYZE (without any actual vacuum) */
    bool estimated_count;          /* num_heap_tuples is an estimate */
    int message_level;             /* ereport level for progress messages */
    double num_heap_tuples;        /* tuples remaining in heap */
    BufferAccessStrategy strategy; /* access strategy for reads */
} IndexVacuumInfo;

/*
 * Struct for statistics returned by ambulkdelete and amvacuumcleanup
 *
 * This struct is normally allocated by the first ambulkdelete call and then
 * passed along through subsequent ones until amvacuumcleanup; however,
 * amvacuumcleanup must be prepared to allocate it in the case where no
 * ambulkdelete calls were made (because no tuples needed deletion).
 * Note that an index AM could choose to return a larger struct
 * of which this is just the first field; this provides a way for ambulkdelete
 * to communicate additional private data to amvacuumcleanup.
 *
 * Note: pages_removed is the amount by which the index physically shrank,
 * if any (ie the change in its total size on disk).  pages_deleted and
 * pages_free refer to free space within the index file.  Some index AMs
 * may compute num_index_tuples by reference to num_heap_tuples, in which
 * case they should copy the estimated_count field from IndexVacuumInfo.
 */
typedef struct IndexBulkDeleteResult {
    BlockNumber num_pages;     /* pages remaining in index */
    BlockNumber pages_removed; /* # removed during vacuum operation */
    bool estimated_count;      /* num_index_tuples is an estimate */
    double num_index_tuples;   /* tuples remaining */
    double tuples_removed;     /* # removed during vacuum operation */
    BlockNumber pages_deleted; /* # unused pages in index */
    BlockNumber pages_free;    /* # pages available for reuse */
} IndexBulkDeleteResult;

/* Typedef for callback function to determine if a tuple is bulk-deletable */
typedef bool (*IndexBulkDeleteCallback)(ItemPointer itemptr, void* state, Oid partOid);

/* struct definitions appear in relscan.h */
typedef struct IndexScanDescData* IndexScanDesc;
typedef struct SysScanDescData* SysScanDesc;

typedef struct ParallelIndexScanDescData* ParallelIndexScanDesc;

/*
 * Enumeration specifying the type of uniqueness check to perform in
 * index_insert().
 *
 * UNIQUE_CHECK_YES is the traditional Postgres immediate check, possibly
 * blocking to see if a conflicting transaction commits.
 *
 * For deferrable unique constraints, UNIQUE_CHECK_PARTIAL is specified at
 * insertion time.	The index AM should test if the tuple is unique, but
 * should not throw error, block, or prevent the insertion if the tuple
 * appears not to be unique.  We'll recheck later when it is time for the
 * constraint to be enforced.  The AM must return true if the tuple is
 * known unique, false if it is possibly non-unique.  In the "true" case
 * it is safe to omit the later recheck.
 *
 * When it is time to recheck the deferred constraint, a pseudo-insertion
 * call is made with UNIQUE_CHECK_EXISTING.  The tuple is already in the
 * index in this case, so it should not be inserted again.	Rather, just
 * check for conflicting live tuples (possibly blocking).
 */
typedef enum IndexUniqueCheck {
    UNIQUE_CHECK_NO,      /* Don't do any uniqueness checking */
    UNIQUE_CHECK_YES,     /* Enforce uniqueness at insertion time */
    UNIQUE_CHECK_PARTIAL, /* Test uniqueness, but no error */
    UNIQUE_CHECK_EXISTING /* Check if existing tuple is unique */
} IndexUniqueCheck;

/*
 * generalized index_ interface routines (in indexam.c)
 */

/*
 * IndexScanIsValid
 *		True iff the index scan is valid.
 */
#define IndexScanIsValid(scan) PointerIsValid(scan)

extern Relation index_open(Oid relationId, LOCKMODE lockmode, int2 bucketId=-1);
extern void index_close(Relation relation, LOCKMODE lockmode);

extern bool index_insert(Relation indexRelation, Datum* values, const bool* isnull, ItemPointer heap_t_ctid,
    Relation heapRelation, IndexUniqueCheck checkUnique);

extern IndexScanDesc index_beginscan(
    Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, int norderbys);
extern IndexScanDesc index_beginscan_bitmap(Relation indexRelation, Snapshot snapshot, int nkeys);
extern void index_rescan(IndexScanDesc scan, ScanKey keys, int nkeys, ScanKey orderbys, int norderbys);
extern void index_endscan(IndexScanDesc scan);
extern void index_markpos(IndexScanDesc scan);
extern void index_restrpos(IndexScanDesc scan);
extern Size index_parallelscan_estimate(Relation indexRelation, Snapshot snapshot);
extern void index_parallelscan_initialize(Relation heapRelation, Size pscan_len, Relation indexRelation,
        Snapshot snapshot, ParallelIndexScanDesc target);
extern void index_parallelrescan(IndexScanDesc scan);
extern IndexScanDesc index_beginscan_parallel(Relation heaprel, Relation indexrel, int nkeys, int norderbys,
    ParallelIndexScanDesc pscan);
extern ItemPointer index_getnext_tid(IndexScanDesc scan, ScanDirection direction);
extern HeapTuple index_fetch_heap(IndexScanDesc scan);
extern HeapTuple index_getnext(IndexScanDesc scan, ScanDirection direction);
extern int64 index_getbitmap(IndexScanDesc scan, TIDBitmap* bitmap);
extern int64 index_column_getbitmap(IndexScanDesc scandesc, const void* sort, VectorBatch* tids);

extern IndexBulkDeleteResult* index_bulk_delete(
    IndexVacuumInfo* info, IndexBulkDeleteResult* stats, IndexBulkDeleteCallback callback, const void* callback_state);
extern IndexBulkDeleteResult* index_vacuum_cleanup(IndexVacuumInfo* info, IndexBulkDeleteResult* stats);
extern bool index_can_return(Relation indexRelation);
extern RegProcedure index_getprocid(Relation irel, AttrNumber attnum, uint16 procnum);
extern FmgrInfo* index_getprocinfo(Relation irel, AttrNumber attnum, uint16 procnum);

/*
 * index access method support routines (in genam.c)
 */
extern IndexScanDesc RelationGetIndexScan(Relation indexRelation, int nkeys, int norderbys);
extern void IndexScanEnd(IndexScanDesc scan);
extern char* BuildIndexValueDescription(Relation indexRelation, Datum* values, const bool* isnull);

/*
 * heap-or-index access to system catalogs (in genam.c)
 */
extern SysScanDesc systable_beginscan(
    Relation heapRelation, Oid indexId, bool indexOK, Snapshot snapshot, int nkeys, ScanKey key);
extern HeapTuple systable_getnext(SysScanDesc sysscan);
extern bool systable_recheck_tuple(SysScanDesc sysscan, HeapTuple tup);
extern void systable_endscan(SysScanDesc sysscan);
extern SysScanDesc systable_beginscan_ordered(
    Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, ScanKey key);
extern HeapTuple systable_getnext_ordered(SysScanDesc sysscan, ScanDirection direction);
extern void systable_endscan_ordered(SysScanDesc sysscan);

HeapTuple systable_getnext_back(SysScanDesc sysscan);

/*
 * global partition index access method support routines (in genam.c)
 */
typedef struct GPIScanDescData {
    HTAB* fakeRelationTable;     /* fake partition relation and partition hash table */
    Bitmapset* invisiblePartMap; /* cache invisible partition oid in GPI */
    Relation parentRelation;     /* parent relation of partition */
    Relation fakePartRelation;   /* fake-relation using partition */
    Partition partition;         /* partition use to fake partition rel */
    Oid currPartOid;             /* current partition oid in GPI */
} GPIScanDescData;

typedef GPIScanDescData* GPIScanDesc;

/* Check input partition oid is same as global-partition-index current work partition oid */
inline bool GPIScanCheckPartOid(GPIScanDesc gpiScan, Oid currScanPartOid)
{
    if (!PointerIsValid(gpiScan)) {
        return false;
    }

    return gpiScan->currPartOid != currScanPartOid;
}
extern void GPIScanInit(GPIScanDesc* gpiScan);
extern void GPIScanEnd(GPIScanDesc gpiScan);
extern bool GPIGetNextPartRelation(GPIScanDesc gpiScan, MemoryContext cxt, LOCKMODE lmode);
extern void GPISetCurrPartOid(GPIScanDesc gpiScan, Oid partOid);
extern Oid GPIGetCurrPartOid(const GPIScanDesc gpiScan);

#endif /* GENAM_H */
