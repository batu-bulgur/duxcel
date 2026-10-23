#ifndef DUXCEL_WORKBOOK_RECOVERY_WORKER_H
#define DUXCEL_WORKBOOK_RECOVERY_WORKER_H
#include "recovery.h"
#include "recovery_catalog.h"
#define DX_RECOVERY_PAGE_ROWS 32u
#define DX_RECOVERY_PAGE_BYTES (4u*1024u*1024u)
typedef struct WorkbookRecoveryWorker WorkbookRecoveryWorker;
typedef enum { RECOVERY_WORK_NONE,RECOVERY_WORK_PAGE,RECOVERY_WORK_PREPARE } WorkbookRecoveryWork;
typedef struct {
    WorkbookRecoveryEntry rows[DX_RECOVERY_PAGE_ROWS]; /* Reply-owned string copies. */
    size_t count,owned_bytes;
    WorkbookRecoveryScanState scan;
    bool restarted;
} WorkbookRecoveryPage;
typedef struct {
    uint64_t id;
    WorkbookRecoveryWork kind;
    WorkbookRecoveryScanResult scan_result;
    WorkbookRecoveryResult prepare_result;
    WorkbookRecoveryPage page;
    WorkbookRecoveryCandidate *candidate; /* Owned until take/free. */
    char error[512];
} WorkbookRecoveryReply;
typedef struct {
    uint64_t id;
    WorkbookRecoveryWork kind;
    bool active,ready,cancel_requested;
    WorkbookRecoveryProgress progress;
    WorkbookRecoveryScanState scan;
    size_t row_reservation_bytes,format_reservation_bytes;
} WorkbookRecoveryWorkerState;
/* One producer and one independent native reader. Root is copied; construction
   starts no file operation. One request/unpolled reply at a time, never latest-
   wins disposal of an owned candidate. No App/model pointers cross this seam.
   Active prepare reserves the loader's maximum format ownership; ready state
   reports actual ownership. These are format/row categories, not total RSS. */
WorkbookRecoveryWorker *workbook_recovery_worker_new(const char *root,char **error);
bool workbook_recovery_worker_page(WorkbookRecoveryWorker *worker,bool restart,uint64_t *id,char **error);
bool workbook_recovery_worker_prepare(WorkbookRecoveryWorker *worker,const char *generation,WorkbookRecoveryMode mode,uint64_t *id,char **error);
/* Cancellation competes with publication under the same mutex. A published
   reply remains owned until poll/free, even if its UI is subsequently dismissed. */
void workbook_recovery_worker_cancel(WorkbookRecoveryWorker *worker);
WorkbookRecoveryWorkerState workbook_recovery_worker_state(WorkbookRecoveryWorker *worker);
WorkbookRecoveryReply *workbook_recovery_worker_poll(WorkbookRecoveryWorker *worker); /* Transfers reply ownership/reservations. */
/* A cancelled page closes its cursor; the next page request restarts explicitly
   in its reply. Successful pages retain a bounded continuation until refresh,
   prepare or shutdown. Only the reading thread owns that cursor. */
WorkbookRecoveryCandidate *workbook_recovery_reply_take_candidate(WorkbookRecoveryReply *reply);
void workbook_recovery_reply_free(WorkbookRecoveryReply *reply);
/* Cooperative cancel and join; frees unpolled results/cursors after retirement.
   Does not delete recovery material. Poll-owned replies outlive the worker. */
void workbook_recovery_worker_free(WorkbookRecoveryWorker *worker);
#endif
