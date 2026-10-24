#ifndef DUXCEL_WORKBOOK_SAVE_H
#define DUXCEL_WORKBOOK_SAVE_H
#include "workbook.h"
typedef enum { SAVE_PHASE_VALIDATE,SAVE_PHASE_PREPARE,SAVE_PHASE_WRITE,SAVE_PHASE_PUBLISH,SAVE_PHASE_COMPLETE } WorkbookSavePhase;
typedef struct {
    WorkbookSavePhase phase;
    uint64_t completed,total; /* Actual completed rows/columns/sheets plus bounded package stages; not elapsed-time estimates. */
} WorkbookSaveProgress;
typedef struct {
    /* Called synchronously on the saving thread. Borrowed context must outlive
       the call; never mutate the snapshot or reenter persistence. False cancels
       before COMMIT, including at PUBLISH. COMPLETE is post-COMMIT observation;
       its return value is ignored. Callback work must be bounded/nonblocking. */
    bool (*update)(void *context,WorkbookSaveProgress progress);
    void *context;
} WorkbookSaveControl;
typedef enum { WORKBOOK_SAVE_FAILED,WORKBOOK_SAVE_COMPLETE,WORKBOOK_SAVE_CANCELLED } WorkbookSaveResult;
/* Native transactional writer. The ordinary workbook_save uses empty control.
   The immutable snapshot is borrowed for the entire operation. Failure/cancel
   preserves an existing committed destination; a failed new destination is
   removed. Progress reaches total only after successful COMMIT. */
WorkbookSaveResult workbook_save_controlled(const Workbook *book,const char *path,WorkbookSaveControl control,char **error);
/* Native core snapshot for version payload staging. Uses the existing v15
   model codec and controlled transaction, with every nonempty primary/Table/
   ledger locator already resolved and stored verbatim as an absolute path.
   The resulting file can move independently of the private capture directory.
   Missing absolute external sources remain references; no source is queried or
   imported. This does not capture history, Data Model assets or analytical
   output outside the current codec, nor publish a user version/index. */
WorkbookSaveResult workbook_save_core_payload(const Workbook *book,const char *path,WorkbookSaveControl control,char **error);
/* Strict native leaf admission: exact v15 core, stored absolute locators and
   no unrelated tables or nested version schema. Reads an independent candidate
   and preserves out on failure. Ordinary workbook_load retains legacy support. */
bool workbook_load_core_payload(Workbook *out,const char *path,char **error);
#endif
