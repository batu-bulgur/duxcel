#ifndef DUXCEL_WORKBOOK_RECOVERY_CATALOG_H
#define DUXCEL_WORKBOOK_RECOVERY_CATALOG_H
#include "workbook.h"
typedef struct WorkbookRecoveryScan WorkbookRecoveryScan;
typedef enum { RECOVERY_ENTRY_BOUND,RECOVERY_ENTRY_UNBOUND,RECOVERY_ENTRY_CHECKPOINT_ONLY,RECOVERY_ENTRY_ACTIVE,RECOVERY_ENTRY_LEGACY,RECOVERY_ENTRY_INCOMPLETE,RECOVERY_ENTRY_INVALID } WorkbookRecoveryEntryKind;
typedef struct {
    const char *generation_name,*original_path,*detail; /* Borrowed only during visit. */
    WorkbookRecoveryEntryKind kind;
    WorkbookId document,generation,parent;
    uint64_t baseline_revision,declared_head_revision;
} WorkbookRecoveryEntry;
typedef struct { uint64_t visited,offered,active,legacy,invalid; bool done; } WorkbookRecoveryScanState;
typedef struct {
    /* False pauses after this offered row, keeping the cursor available for the
       next page. Copy needed fields before returning; no handles escape. */
    bool (*visit)(void *context,const WorkbookRecoveryEntry *entry);
    bool (*cancelled)(void *context);
    void *context;
} WorkbookRecoveryScanControl;
typedef enum { RECOVERY_SCAN_MORE,RECOVERY_SCAN_DONE,RECOVERY_SCAN_CANCELLED,RECOVERY_SCAN_FAILED } WorkbookRecoveryScanResult;
/* Native reading-thread, non-reentrant cursor. Callbacks must not advance or
   free this scan. A missing root is an empty completed scan and
   is never created. At most 128 physical entries per step; no sorting/pruning,
   recursion or mtime-derived branch ordering. New filesystem changes require a
   fresh scan; a failed enumeration requires a fresh cursor. Each inactive generation is leased only for metadata inspection;
   adoption must re-claim and verify through recovery_prepare. */
bool workbook_recovery_scan_open(const char *root,WorkbookRecoveryScan **out,char **error);
WorkbookRecoveryScanResult workbook_recovery_scan_step(WorkbookRecoveryScan *scan,size_t budget,WorkbookRecoveryScanControl control,char **error);
WorkbookRecoveryScanState workbook_recovery_scan_state(const WorkbookRecoveryScan *scan);
void workbook_recovery_scan_free(WorkbookRecoveryScan *scan);
#endif
