/*
 * Copyright (c) 2020 Huawei Technologies Co.,Ltd.
 *
 * openGauss is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * ---------------------------------------------------------------------------------------
 * 
 * pagewriter.h
 *        Data struct to store pagewriter thread variables.
 * 
 * 
 * IDENTIFICATION
 *        src/include/postmaster/pagewriter.h
 *
 * ---------------------------------------------------------------------------------------
 */
#ifndef _PAGEWRITER_H
#define _PAGEWRITER_H
#include "storage/buf/buf.h"

typedef struct PGPROC PGPROC;
typedef struct BufferDesc BufferDesc;

typedef struct ThrdDwCxt {
    char* dw_buf;
    uint16 write_pos;
    volatile int dw_page_idx;      /* -1 means data files have been flushed. */
    bool contain_hashbucket;
} ThrdDwCxt;

typedef struct PageWriterProc {
    PGPROC* proc;
    volatile uint32 start_loc;
    volatile uint32 end_loc;
    volatile bool need_flush;
    volatile uint32 actual_flush_num;
} PageWriterProc;

typedef struct PageWriterProcs {
    PageWriterProc* writer_proc;
    volatile int num;             /* number of pagewriter thread */
    pg_atomic_uint32 running_num; /* number of pagewriter thread which flushing dirty page */
    ThrdDwCxt thrd_dw_cxt;
} PageWriterProcs;

typedef struct DirtyPageQueueSlot {
    volatile int buffer;
    pg_atomic_uint32 slot_state;
} DirtyPageQueueSlot;

typedef Datum (*incre_ckpt_view_get_data_func)();

const int INCRE_CKPT_VIEW_NAME_LEN = 128;

typedef struct incre_ckpt_view_col {
    char name[INCRE_CKPT_VIEW_NAME_LEN];
    Oid data_type;
    incre_ckpt_view_get_data_func get_val;
} incre_ckpt_view_col;

/*
 * The slot location is pre-occupied. When the slot buffer is set, the state will set
 * to valid. when remove dirty page form queue, don't change the state, only when move
 * the dirty page head, need set the slot state is invalid.
 */
const int SLOT_VALID = 1;

extern bool IsPagewriterProcess(void);
extern void incre_ckpt_pagewriter_cxt_init();
extern void ckpt_pagewriter_main(void);

extern bool push_pending_flush_queue(Buffer buffer);
extern void remove_dirty_page_from_queue(BufferDesc* buf);
extern int64 get_dirty_page_num();
extern uint64 get_dirty_page_queue_tail();
extern int get_pagewriter_thread_id(void);
extern bool is_dirty_page_queue_full(BufferDesc* buf);
extern int get_dirty_page_queue_head_buffer();
/* Shutdown all the page writer threads. */
extern void ckpt_shutdown_pagewriter();
extern uint64 get_dirty_page_queue_rec_lsn();
extern XLogRecPtr ckpt_get_min_rec_lsn(void);
extern uint32 calculate_thread_max_flush_num(bool is_pagewriter);

const int PAGEWRITER_VIEW_COL_NUM = 8;
const int INCRE_CKPT_VIEW_COL_NUM = 7;

extern const incre_ckpt_view_col g_ckpt_view_col[INCRE_CKPT_VIEW_COL_NUM];
extern const incre_ckpt_view_col g_pagewriter_view_col[PAGEWRITER_VIEW_COL_NUM];

#endif /* _PAGEWRITER_H */
