#ifndef DUXCEL_ARCHIVE_WRITE_H
#define DUXCEL_ARCHIVE_WRITE_H
#include "archive_read.h"
#include "version_stage.h"
typedef struct WorkbookArchiveReceipt WorkbookArchiveReceipt;
typedef struct {
    WorkbookSaveControl core;
    WorkbookVersionRestoreControl inspect;
    WorkbookVersionStageControl stage;
    WorkbookVersionRestoreControl prepare;
} WorkbookArchiveWriteControl;
/* Native v16 transactional writer. Borrow immutable authored head/index/leaves
   and an existing staging parent. reserved_bytes bounds each admitted core set
   (existing target/new prepared receipt), not total disk/RSS. Validates an
   existing v1-v16 target including retained closure before replacement, retains
   unrelated tables and protects external/borrowed leaf aliases. Corrupt targets
   reject. Failure/cancel rolls back existing data/removes a new target, leaving
   out unchanged. Every owned receipt/root is prepared before cancellable
   PUBLISH/COMMIT; no fallible post-COMMIT allocation can report a false failure.
   Does not choose Save purpose, advance a live App index or implement Tag UI.
   Caller applies the separate version plan/Save R+1 fence after COMPLETE. */
WorkbookSaveResult workbook_archive_write(const Workbook *head,const WorkbookVersionIndex *index,
    const WorkbookCorePayload *const *leaves,size_t count,const char *path,const char *parent,
    uint64_t reserved_bytes,WorkbookArchiveWriteControl control,WorkbookArchiveReceipt **out,char **error);
const WorkbookVersionCoreSet *workbook_archive_receipt_versions(const WorkbookArchiveReceipt *receipt);
const char *workbook_archive_receipt_path(const WorkbookArchiveReceipt *receipt);
WorkbookId workbook_archive_receipt_document(const WorkbookArchiveReceipt *receipt);
uint64_t workbook_archive_receipt_revision(const WorkbookArchiveReceipt *receipt);
/* Post-close stat is observational. NULL means identity capture was unavailable;
   the committed Save still succeeded and must not be retried as a failure. */
const SourceFileIdentity *workbook_archive_receipt_identity(const WorkbookArchiveReceipt *receipt);
WorkbookVersionCoreSet *workbook_archive_receipt_take_versions(WorkbookArchiveReceipt *receipt);
void workbook_archive_receipt_free(WorkbookArchiveReceipt *receipt);
#endif
