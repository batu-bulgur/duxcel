#ifndef DUXCEL_WORKBOOK_RECOVERY_H
#define DUXCEL_WORKBOOK_RECOVERY_H
#include "workbook.h"
typedef struct WorkbookRecoveryCandidate WorkbookRecoveryCandidate;
typedef enum { RECOVERY_REPLAY_TAIL,RECOVERY_OPEN_CHECKPOINT } WorkbookRecoveryMode;
typedef enum { RECOVERY_CLAIM,RECOVERY_VERIFY,RECOVERY_LOAD,RECOVERY_REPLAY,RECOVERY_READY } WorkbookRecoveryPhase;
typedef struct { WorkbookRecoveryPhase phase; uint64_t completed,total; } WorkbookRecoveryProgress;
typedef struct {
    /* Saving/reading thread only; false requests cooperative cancellation. */
    bool (*update)(void *context,WorkbookRecoveryProgress progress);
    void *context;
} WorkbookRecoveryControl;
typedef enum { RECOVERY_PREPARED,RECOVERY_BUSY,RECOVERY_UNBOUND,RECOVERY_CANCELLED,RECOVERY_FAILED } WorkbookRecoveryResult;
typedef struct {
    WorkbookId document,generation,parent;
    uint64_t baseline_revision,head_revision;
    bool bound,tail_deferred,tail_unknown;
    const char *directory,*original_path; /* Candidate-owned, never an automatic Save destination. */
} WorkbookRecoveryInfo;
/* Native isolated candidate (currently requires the macOS streaming digest
   adapter; other native hosts fail without modifying artifacts). Only a direct session-<32 lowercase hex> child of
   the resolved private root is accepted; aliases outside that root reject.
   Projects persisted state without allocating live draft/history order or
   reconstructing retained Undo. Holds the existing generation lease through
   candidate release. Never removes
   artifacts or mutates the current workbook. Failure/cancel leaves *out intact.
   Tail replay requires a bound and verified baseline; checkpoint inspection of
   a leased legacy/unreadable tail is explicitly marked unbound/deferred. */
WorkbookRecoveryResult workbook_recovery_prepare(const char *root,const char *generation,WorkbookRecoveryMode mode,WorkbookRecoveryControl control,WorkbookRecoveryCandidate **out,char **error);
const Workbook *workbook_recovery_book(const WorkbookRecoveryCandidate *candidate);
const WorkbookRecoveryInfo *workbook_recovery_info(const WorkbookRecoveryCandidate *candidate);
/* Transfers the complete owned model, retaining the candidate's lease/info.
   A caller adopting it must keep the candidate until its lineage is resolved. */
Workbook *workbook_recovery_take_book(WorkbookRecoveryCandidate *candidate);
/* Explicit disposition after a complete named Save or user discard. Only a
   full bound candidate is eligible; deferred/unknown tails cannot be retired.
   Changed artifacts or extra files reject before deletion. Filesystem deletion
   can still partially fail; the caller must not equate cleanup with Save. */
bool workbook_recovery_retire(WorkbookRecoveryCandidate *candidate,char **error);
void workbook_recovery_free(WorkbookRecoveryCandidate *candidate);
#endif
