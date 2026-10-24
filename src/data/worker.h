#ifndef DUXCEL_WORKER_H
#define DUXCEL_WORKER_H
#include "database.h"
#include "source_digest.h"
#include "../platform/platform.h"
#include "../workbook/workbook.h"
#ifndef DUXCEL_BROWSER_HOST
#include "../workbook/save.h"
#endif
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
#ifdef DUXCEL_BROWSER_HOST
/* Runtime-only immutable registration identity. Never persisted as a native
   source version, reconstructed from a filename or refreshed during execution. */
#define DX_BROWSER_SOURCE_BINDINGS (DX_MAX_OBJECTS+1)
typedef struct { uint64_t engine_epoch,registration_id; } BrowserSourceBinding;
typedef struct {
    size_t source; /* 0 primary; otherwise 1-based Copy/Statistics SourceSnapshot. */
    BrowserSourceBinding binding;
} BrowserSourceGuard;
#endif
typedef enum { JOB_PAGE, JOB_COPY, JOB_SAVE, JOB_LOAD, JOB_DISTINCT, JOB_PIVOT, JOB_PIVOT_COPY, JOB_EXPORT, JOB_COUNT, JOB_RANGE_COPY,JOB_STATISTICS,JOB_VALIDATE_SQL,JOB_STEP_PREVIEW,JOB_VERIFY_SOURCE,JOB_COLLECT_RECORDS } JobKind;
#ifndef DUXCEL_BROWSER_HOST
typedef struct WorkbookNativeIO WorkbookNativeIO;
typedef enum { WORKER_SAVE_PREPARE_NONE,WORKER_SAVE_SNAPSHOT,WORKER_SAVE_COMPARE,WORKER_SAVE_INSPECT,WORKER_SAVE_ROOTS,WORKER_SAVE_RECEIPT } WorkerSavePreparation;
typedef struct {
    WorkbookSaveControl publication; /* Only named-file PUBLISH disables cancel. */
    bool (*cancelled)(void *context);
    void (*prepare)(void *context,WorkerSavePreparation phase,size_t root,uint64_t bytes);
    void *context;
} WorkerNativeFileControl;
typedef struct {
    WorkbookNativeIO *context; /* Owned opaque native adapter; never an App pointer. */
    WorkbookSaveResult (*execute)(WorkbookNativeIO *context,JobKind kind,const Workbook *snapshot,const char *path,
        WorkerNativeFileControl control,Workbook **loaded,char **error);
    void (*dispose)(WorkbookNativeIO *context);
} WorkerNativeFileTask;
typedef struct WorkerNativeRetirement {
    struct WorkerNativeRetirement *next;
    WorkbookNativeIO *context;
    void (*dispose)(WorkbookNativeIO *context);
    bool queued;
} WorkerNativeRetirement;
#endif
typedef struct {
    uint64_t id;
    JobKind kind;
    Analysis analysis;
    uint64_t offset, rows;
    uint64_t expected_source_stamp; /* Session-only raw source guard; zero is unbound. */
    /* COLLECT_RECORDS requires this captured native Page/file observation and
       nonzero expected_source_stamp. No ownership beyond the job's POD. */
    SourceFileIdentity expected_file_identity;
    bool expected_file_identity_valid;
    size_t first_col, cols;
    int64_t known_total;
    Workbook *workbook;
#ifndef DUXCEL_BROWSER_HOST
    WorkerNativeFileTask native_file;
#endif
    /* VERIFY_SOURCE owns one <=65536-byte file path, expected_source_stamp and
       scalar routing identities only. Zero stamp is unbound; no SQL is run. */
    char *path;
    char *column,*search,*date_start,*date_end;
    int date_level;
    PivotDefinition pivot;
    uint64_t object_id,revision;
    bool include_header,is_table,include_totals,is_preview;
    /* Opt-in non-Table JOB_PIVOT readouts. Compute all eligible Plot rankings
       in this original job; changing presentation never submits an Other job. */
    bool include_chart_other;
    CopyRequest *copy;
    StatisticsRequest *statistics;
    bool refresh_source;
#ifdef DUXCEL_BROWSER_HOST
    BrowserSourceGuard browser_sources[DX_BROWSER_SOURCE_BINDINGS];
    size_t browser_source_count;
    bool browser_sources_invalid; /* Required file binding unavailable at capture. */
#endif
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
    RecordSelection records; /* Only COLLECT_RECORDS; reply_free owns its vector. */
    /* Browser export moves one malloc-owned binary allocation into the reply.
       Native file exports leave it NULL. poll transfers it; reply_free owns it. */
    unsigned char *export_data;
    size_t export_bytes;
    uint64_t exported_rows,counted_rows;
    /* Only JOB_VERIFY_SOURCE: scalar ownership, no additional cleanup. The
       digest is meaningful only on OK without error; other jobs ignore it. */
    SourceDigest source_digest;
    SourceDigestStatus source_digest_status;
    uint64_t object_id,revision;
    bool is_preview;
#ifndef DUXCEL_BROWSER_HOST
    WorkbookSaveResult save_result;
    WorkerNativeFileTask native_file;
#endif
} QueryReply;
/* Conservative native format-pipeline reservation categories, not measured
   allocator/RSS usage or a bound on SQL construction/engine temporary memory.
   An admitted job owns at most 8 MiB of fixed-slot definition text. Its reserve
   also covers the maximum output IDs, fixed job/reply structures and digest
   workspace, plus an error allowance. Diagnostics are NOT truncated to that
   allowance; actual published ownership below includes the complete error.
   The caller separately reserves bounded pipeline/SQL staging and counts a
   polled reply until reply_free. Invalid oversized/nested payloads are not
   admitted to this cap: their reservation is SIZE_MAX until actual cleanup. */
#define DX_WORKER_FORMAT_DEFINITION_BYTES (8u*1024u*1024u)
#define DX_WORKER_FORMAT_RESERVED_BYTES (DX_WORKER_FORMAT_DEFINITION_BYTES + \
    DX_RECORD_SELECTION_ROWS*sizeof(uint64_t) + DX_SOURCE_DIGEST_BUFFER_BYTES + \
    sizeof(QueryJob) + sizeof(QueryReply) + DX_SOURCE_DIGEST_ERROR_BYTES)
typedef struct {
    size_t active_reserved,pending_reserved,published_owned;
    unsigned active_count,pending_count,published_count;
} WorkerFormatUsage;
#ifndef DUXCEL_BROWSER_HOST
typedef struct {
    uint64_t id;
    WorkbookSaveProgress progress;
    bool active,cancellable,cancel_requested;
    WorkerSavePreparation preparation;
    size_t preparation_root;
    uint64_t preparation_bytes;
} WorkerSaveState;
#endif
/* One worker owns its DuckDB connection. The UI may only call interrupt under mutex.
   Reads are latest-wins; native Save owns the lane through poll. Replies are immutable and ownership transfers to poll(). */
typedef struct {
    DxThread thread;
    DxMutex mutex;
    DxCond condition;
    bool stop, running, started;
    uint64_t latest_id, active_id, jobs_started, sql_executed, counts_executed;
    QueryJob *pending;
    QueryReply *reply;
    /* Protected by mutex. Active ownership outlives running=false while its
       canceled/obsolete job/reply allocations retire outside the mutex. */
    JobKind active_kind;
    size_t active_format_reserved,pending_format_reserved,published_format_owned;
    bool pending_format_definition_rejected;
    Database db;
#ifndef DUXCEL_BROWSER_HOST
    WorkerSaveState save; /* Mutex-owned ordered file operation, retained until poll. */
    WorkerNativeRetirement *retired_head,*retired_tail;
#endif
} QueryWorker;
bool worker_start(QueryWorker *w);
void worker_submit(QueryWorker *w, QueryJob *job); /* Takes job; a native Save in progress rejects replacement jobs without losing its receipt. App owns IO admission. */
QueryReply *worker_poll(QueryWorker *w); /* transfers reply */
void worker_cancel(QueryWorker *w);
#ifndef DUXCEL_BROWSER_HOST
WorkerSaveState worker_save_state(QueryWorker *worker);
/* Cooperative request; false means absent/already publishing. Never invalidates
   the Save reply or interrupts an unrelated analytical connection. */
bool worker_save_cancel(QueryWorker *worker);
/* Transfer one initialized owned intrusive node. No allocation, callbacks or
   IO under the mutex. The worker drains nodes before pending jobs and on stop;
   disposal runs outside the mutex. False keeps ownership (not started/stopping
   or invalid/already queued). Node/context must not be touched after transfer.
   The worker object must outlive native tasks/replies that may retire into it. */
bool worker_retire_native(QueryWorker *worker,WorkerNativeRetirement *owned);
#endif
void worker_stop(QueryWorker *w);
/* Main-thread lifecycle contract, like submit/poll: not concurrent with
   start/stop. Native snapshot is mutex-protected and performs no allocations,
   text/vector scans, SQL or filesystem access. Cancel does not clear active
   ownership. Publication may briefly coexist with an active retirement reserve
   and intentionally overcount. Poll transfers the published charge to caller.
   SIZE_MAX means accounting cannot admit that invalid/overflowing payload.
   Browser native-format work is synchronously unsupported and returns zero;
   this is not a measurement of ordinary browser/error-reply heap allocations. */
WorkerFormatUsage worker_format_usage(QueryWorker *w);
void reply_free(QueryReply *reply);
#endif
