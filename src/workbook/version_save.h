#ifndef DUXCEL_VERSION_SAVE_H
#define DUXCEL_VERSION_SAVE_H
#include "archive_write.h"
#include "core_compare.h"
typedef struct WorkbookVersionReceipt WorkbookVersionReceipt;
typedef struct {
    const Workbook *head;
    const WorkbookVersionCoreSet *base; /* Borrowed immutable; NULL=new/legacy index. */
    WorkbookVersionOwner owner;
    WorkbookVersionPurpose purpose;
    const char *path,*parent;
    uint64_t reserved_bytes;
} WorkbookVersionSaveRequest;
typedef struct {
    /* Internal capture PUBLISH/COMPLETE are not named-file publication. Worker
       adapters must keep cancellation enabled until archive.core PUBLISH. */
    WorkbookCorePayloadControl capture;
    WorkbookCoreCompareControl compare;
    WorkbookArchiveWriteControl archive;
} WorkbookVersionSaveControl;
/* Native worker controller. User Save captures the supported head, compares
   the complete core against Latest Saved for reuse, prepares the pure owner/
   generation plan and writes one v16 artifact. Checkpoint/export preserve the
   supplied index and all its leaves without moving Saved or capturing a new
   user root. Passing base=NULL explicitly exports no retained history.
   Inputs are borrowed immutable for the call. out must be NULL and is unchanged
   on failure/cancel. Receipt/plan/core-set ownership is prepared before COMMIT;
   no live App index/path/dirty/history is mutated. Caller still verifies current
   write owner/index generation before publication and retains R+1 dirty. */
WorkbookSaveResult workbook_version_save(WorkbookVersionSaveRequest request,WorkbookVersionSaveControl control,
    WorkbookVersionReceipt **out,char **error);
const WorkbookVersionPlan *workbook_version_receipt_plan(const WorkbookVersionReceipt *receipt);
const WorkbookArchiveReceipt *workbook_version_receipt_archive(const WorkbookVersionReceipt *receipt);
bool workbook_version_receipt_reused(const WorkbookVersionReceipt *receipt);
WorkbookVersionCoreSet *workbook_version_receipt_take_versions(WorkbookVersionReceipt *receipt);
void workbook_version_receipt_free(WorkbookVersionReceipt *receipt);
#endif
