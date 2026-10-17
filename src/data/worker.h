#ifndef DUXCEL_WORKER_H
#define DUXCEL_WORKER_H
#include "database.h"
#include "../platform/platform.h"
#include "../workbook/workbook.h"
#include "../workbook/clipboard.h"
typedef struct {
    bool is_pivot;
    PivotDefinition pivot;
    size_t source; /* 1-based SourceSnapshot index, or 0 for the primary source */
    uint64_t first;
    size_t rows,column,columns,target_row,target_column;
} CopyPatch;
typedef struct {
    size_t source; /* Same primary/SourceSnapshot indexing as CopyPatch. */
    uint64_t expected_source_stamp;
} CopySourceGuard;
typedef struct {
    SourceSnapshot sources;
    CellClipboard cells;
    CopyPatch patches[DX_MAX_OBJECTS+1];
    size_t count;
    CopySourceGuard guards[DX_MAX_OBJECTS+1];
    size_t guard_count;
} CopyRequest;
void copy_request_free(CopyRequest *request);
typedef enum { JOB_PAGE, JOB_COPY, JOB_SAVE, JOB_LOAD, JOB_DISTINCT, JOB_PIVOT, JOB_PIVOT_COPY, JOB_EXPORT, JOB_COUNT, JOB_RANGE_COPY,JOB_STATISTICS,JOB_VALIDATE_SQL,JOB_STEP_PREVIEW } JobKind;
typedef struct {
    uint64_t id;
    JobKind kind;
    Analysis analysis;
    uint64_t offset, rows;
    uint64_t expected_source_stamp; /* Session-only raw source guard; zero is unbound. */
    size_t first_col, cols;
    int64_t known_total;
    Workbook *workbook;
    char *path;
    char *column,*search,*date_start,*date_end;
    int date_level;
    PivotDefinition pivot;
    uint64_t object_id,revision;
    bool include_header,is_table,include_totals,is_preview;
    CopyRequest *copy;
    StatisticsRequest *statistics;
    bool refresh_source;
} QueryJob;
typedef struct {
    uint64_t id;
    JobKind kind;
    Analysis analysis;
    Page page;
    char *error, *clipboard;
    Workbook *workbook;
    char *path;
    DistinctPage distinct;
    SelectionStats statistics;
    /* Browser export moves one malloc-owned binary allocation into the reply.
       Native file exports leave it NULL. poll transfers it; reply_free owns it. */
    unsigned char *export_data;
    size_t export_bytes;
    uint64_t exported_rows,counted_rows;
    uint64_t object_id,revision;
    bool is_preview;
} QueryReply;
/* One worker owns its DuckDB connection. The UI may only call interrupt under mutex.
   Jobs are latest-wins; replies are immutable and ownership transfers to poll(). */
typedef struct {
    DxThread thread;
    DxMutex mutex;
    DxCond condition;
    bool stop, running, started;
    uint64_t latest_id, active_id, jobs_started, sql_executed, counts_executed;
    QueryJob *pending;
    QueryReply *reply;
    Database db;
} QueryWorker;
bool worker_start(QueryWorker *w);
void worker_submit(QueryWorker *w, QueryJob *job); /* takes job */
QueryReply *worker_poll(QueryWorker *w); /* transfers reply */
void worker_cancel(QueryWorker *w);
void worker_stop(QueryWorker *w);
void reply_free(QueryReply *reply);
#endif
