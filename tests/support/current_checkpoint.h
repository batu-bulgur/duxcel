#ifndef DUXCEL_TEST_CURRENT_CHECKPOINT_H
#define DUXCEL_TEST_CURRENT_CHECKPOINT_H
#include "workbook/archive_read.h"
#include "workbook/version_save.h"
#include <stdlib.h>
/* Current checkpoint component helpers. Staging is outside the generation's
   fixed inventory; admitted heads transfer independent ownership to the test. */
static inline bool test_current_checkpoint_load(Workbook *out,const char *path,char **error) {
    WorkbookArchiveCandidate *candidate=NULL;
    if (workbook_archive_read(path,"build/tests",UINT64_C(8)*1024*1024*1024,
            (WorkbookArchiveReadControl){0},&candidate,error)!=ARCHIVE_READ_READY) return false;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(workbook_archive_versions(candidate));
    if (workbook_archive_file_version(candidate)!=16 || !index || index->saved.present || index->root_count || index->tag_count) {
        workbook_archive_candidate_free(candidate); return false;
    }
    Workbook *owned=workbook_archive_take_book(candidate); workbook_archive_candidate_free(candidate);
    if (!owned) return false;
    workbook_free(out); *out=*owned; free(owned); return true;
}
static inline bool test_current_checkpoint_save(const Workbook *book,const char *path,char **error) {
    WorkbookVersionReceipt *receipt=NULL;
    WorkbookSaveResult result=workbook_version_save((WorkbookVersionSaveRequest){
        .head=book,.owner={.document_high=book->id.high,.document_low=book->id.low,.session=1,.mode=VERSION_WORKING_HEAD},
        .purpose=VERSION_RECOVERY_CHECKPOINT,.path=path,.parent="build/tests",.reserved_bytes=UINT64_C(8)*1024*1024*1024
    },(WorkbookVersionSaveControl){0},&receipt,error);
    workbook_version_receipt_free(receipt); return result==WORKBOOK_SAVE_COMPLETE;
}
#endif
