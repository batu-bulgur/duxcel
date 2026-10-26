#ifndef DUXCEL_VERSION_SAVE_H
#define DUXCEL_VERSION_SAVE_H
#include "archive_write.h"
#include "core_compare.h"
#include "version_tag.h"
typedef struct WorkbookVersionReceipt WorkbookVersionReceipt;
typedef struct {
    const Workbook *head;
    const WorkbookVersionCoreSet *base; /* Borrowed immutable; NULL=new document index. */
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
/* Explicit user Create Tag also publishes captured Latest Saved. Unique-label
   preflight precedes capture; proof/reuse and label/root publication are atomic.
   request.purpose must be VERSION_USER_SAVE. Ordinary Save creates no tags. */
WorkbookSaveResult workbook_version_tag_save(WorkbookVersionSaveRequest request,WorkbookVersionText label,
    WorkbookVersionSaveControl control,WorkbookVersionReceipt **out,char **error);
/* Ordinary plans and tag plans are disjoint getters; tag callers must fence the
   complete tag plan, including its additional publication generation. */
const WorkbookVersionTagPlan *workbook_version_receipt_tag(const WorkbookVersionReceipt *receipt);
const WorkbookVersionPlan *workbook_version_receipt_plan(const WorkbookVersionReceipt *receipt);
const WorkbookArchiveReceipt *workbook_version_receipt_archive(const WorkbookVersionReceipt *receipt);
bool workbook_version_receipt_reused(const WorkbookVersionReceipt *receipt);
WorkbookVersionCoreSet *workbook_version_receipt_take_versions(WorkbookVersionReceipt *receipt);
void workbook_version_receipt_free(WorkbookVersionReceipt *receipt);
#endif
