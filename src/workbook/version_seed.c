#include "version_seed.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool source_matches(const WorkbookArchiveCandidate *candidate,char **error) {
    const Workbook *book=workbook_archive_book(candidate); SourceFileIdentity current={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    if (source_file_identity(book->path,&current,detail)==SOURCE_DIGEST_OK && source_file_identity_same(workbook_archive_identity(candidate),&current)) return true;
    return fail(error,"The legacy source changed before its Saved baseline was retained.");
}
WorkbookSaveResult workbook_version_seed_legacy(const WorkbookArchiveCandidate *candidate,const char *parent,
    uint64_t reserved,WorkbookVersionSeedControl control,WorkbookVersionCoreSet **out,char **error) {
    const Workbook *book=workbook_archive_book(candidate); unsigned version=workbook_archive_file_version(candidate);
    if (!book || !book->path || !*book->path || book->dirty || !version || version>15 || workbook_archive_versions(candidate) || !out || *out || !parent || !*parent) {
        fail(error,"Choose an unchanged admitted legacy saved head and empty baseline result."); return WORKBOOK_SAVE_FAILED;
    }
    if (!source_matches(candidate,error)) return WORKBOOK_SAVE_FAILED;
    WorkbookCorePayload *leaf=NULL; WorkbookVersionMetadata *metadata=NULL; WorkbookVersionCoreSet *set=NULL;
    WorkbookCorePayloadResult captured=workbook_core_payload_capture(book,parent,control.capture,&leaf,error);
    if (captured!=CORE_PAYLOAD_READY) return captured==CORE_PAYLOAD_CANCELLED ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
    WorkbookSaveResult result=WORKBOOK_SAVE_FAILED; const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaf);
    WorkbookVersionRoot root={.bytes=info->bytes,.encoding=info->encoding}; memcpy(root.key.bytes,info->sha256,DX_VERSION_KEY_BYTES);
    WorkbookVersionIndex index={.document_high=book->id.high,.document_low=book->id.low,.generation=1,.saved_generation=1,.saved={true,root.key},.roots=&root,.root_count=1};
    if (!workbook_version_metadata_copy(&index,reserved,&metadata,error)) goto done;
    WorkbookVersionRestoreResult adopted=workbook_version_core_set_take(&metadata,&leaf,1,reserved,control.admit,&set,error);
    if (adopted!=VERSION_RESTORE_READY) { if (adopted==VERSION_RESTORE_CANCELLED) result=WORKBOOK_SAVE_CANCELLED; goto done; }
    if (!source_matches(candidate,error)) goto done;
    *out=set; set=NULL; result=WORKBOOK_SAVE_COMPLETE;
done:
    workbook_version_metadata_free(metadata); workbook_core_payload_free(leaf); workbook_version_core_set_free(set); return result;
}
