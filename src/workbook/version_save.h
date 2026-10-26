#ifndef DUXCEL_VERSION_SAVE_H
#define DUXCEL_VERSION_SAVE_H
#include "archive_write.h"
#include "core_compare.h"
#include "version_tag.h"
typedef struct WorkbookVersionReceipt WorkbookVersionReceipt;
typedef struct WorkbookVersionPublication WorkbookVersionPublication;
typedef struct WorkbookVersionCommittedPublication WorkbookVersionCommittedPublication;
typedef enum { VERSION_PUBLICATION_USER_SAVE,VERSION_PUBLICATION_TAG_SAVE,VERSION_PUBLICATION_CHECKPOINT,
    VERSION_PUBLICATION_STATE_EXPORT,VERSION_PUBLICATION_EXPORT_SAVE,VERSION_PUBLICATION_INVALID } WorkbookVersionPublicationIntent;
typedef struct {
    /* Genuine prepared owner minted by the real capture/compare/plan controller.
       Borrowed through this callback and cancel/returned receipt lifetime only.
       Actual after-roots come from the same archive transaction's root reader;
       this ordering does not require an already-staged history component.
       No mutation/reentry, transaction takeover or retained pointer without its
       owner. All owned preparation must finish BEFORE outer PUBLISH/COMMIT. */
    bool (*prepare)(void *context,const WorkbookVersionPublication *publication,duckdb_connection connection,
        const WorkbookVersionCoreSet *after,char **error);
    /* Called on every failure/cancel after prepare was exposed, before any
       publication/capture owner is released. Cancel dependent borrowed plans. */
    void (*cancel)(void *context,const WorkbookVersionPublication *publication);
    void *context;
} WorkbookVersionPublicationControl;
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
    WorkbookVersionPublicationControl publication;
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
/* Current-state independent export has its own genuine intent even though it
   creates a fresh Saved index. Require fresh origin identity/base=NULL. It cannot
   become a normal User Save publication by matching doc/revision fields. */
WorkbookSaveResult workbook_version_export_save(WorkbookVersionSaveRequest request,WorkbookVersionSaveControl control,
    WorkbookVersionReceipt **out,char **error);
/* Prepared and committed phases have distinct opaque types. Capture is the ONE
   real sealed R created by this controller, retained by the receipt until free.
   It is not recaptured just to mint authority. selected is the ACTUAL chosen body;
   complete-core reuse can make its SHA differ from captured. Internal source/IO
   owners and body duplication remain charged; reserved_bytes still bounds each
   admitted root set, not aggregate disk/RSS. No advertised history is activated.
   Selected borrows require the receipt still to own its versions; take_versions
   ends that getter role. Capture remains owned until receipt disposal. */
WorkbookVersionPublicationIntent workbook_version_publication_intent(const WorkbookVersionPublication *publication);
const WorkbookVersionPlan *workbook_version_publication_plan(const WorkbookVersionPublication *publication);
const WorkbookVersionCoreSet *workbook_version_publication_base(const WorkbookVersionPublication *publication);
/* SAME immutable model used by actual capture. Borrowed only inside the
   active prepare callback only, with BOTH actual callback owners matching.
   Wrong connection/after owner/inactive phase returns NULL. This avoids
   a second decode/capture for model-bound auxiliary association. */
const Workbook *workbook_version_publication_snapshot(const WorkbookVersionPublication *publication,
    duckdb_connection connection,const WorkbookVersionCoreSet *after);
const WorkbookCorePayload *workbook_version_publication_capture(const WorkbookVersionPublication *publication);
const WorkbookCorePayload *workbook_version_publication_selected(const WorkbookVersionPublication *publication);
uint64_t workbook_version_publication_revision(const WorkbookVersionPublication *publication);
bool workbook_version_publication_compared(const WorkbookVersionPublication *publication);
const WorkbookVersionCommittedPublication *workbook_version_receipt_publication(const WorkbookVersionReceipt *receipt);
WorkbookVersionPublicationIntent workbook_version_committed_intent(const WorkbookVersionCommittedPublication *publication);
const WorkbookVersionPlan *workbook_version_committed_plan(const WorkbookVersionCommittedPublication *publication);
const WorkbookCorePayload *workbook_version_committed_capture(const WorkbookVersionCommittedPublication *publication);
const WorkbookCorePayload *workbook_version_committed_selected(const WorkbookVersionCommittedPublication *publication);
uint64_t workbook_version_committed_revision(const WorkbookVersionCommittedPublication *publication);
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
