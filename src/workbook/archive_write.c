#include "archive_write.h"
#include "save_native.h"
#include "../platform/platform.h"
#include "../platform/directory.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookArchiveReceipt {
    WorkbookVersionCoreSet *versions;
    char *path;
    WorkbookId document;
    uint64_t revision;
    SourceFileIdentity identity;
    bool identity_valid;
};
struct WorkbookArchiveSaveContext {
    const WorkbookVersionIndex *index;
    const WorkbookCorePayload *const *leaves;
    size_t count;
    const char *parent;
    uint64_t reserved;
    WorkbookArchiveWriteControl control;
    WorkbookArchiveReceipt *receipt;
    bool replaces_versions;
};
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Native archive Save failed."); } return false; }
static bool exec(duckdb_connection c,const char *sql,char **error) {
    duckdb_result r={0}; bool ok=duckdb_query(c,sql,&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok;
}
const WorkbookVersionCoreSet *workbook_archive_receipt_versions(const WorkbookArchiveReceipt *r) { return r ? r->versions : NULL; }
const char *workbook_archive_receipt_path(const WorkbookArchiveReceipt *r) { return r ? r->path : NULL; }
WorkbookId workbook_archive_receipt_document(const WorkbookArchiveReceipt *r) { return r ? r->document : (WorkbookId){0}; }
uint64_t workbook_archive_receipt_revision(const WorkbookArchiveReceipt *r) { return r ? r->revision : 0; }
const SourceFileIdentity *workbook_archive_receipt_identity(const WorkbookArchiveReceipt *r) { return r && r->identity_valid ? &r->identity : NULL; }
WorkbookVersionCoreSet *workbook_archive_receipt_take_versions(WorkbookArchiveReceipt *r) { if (!r) return NULL; WorkbookVersionCoreSet *s=r->versions; r->versions=NULL; return s; }
void workbook_archive_receipt_free(WorkbookArchiveReceipt *r) {
    if (!r) return;
    workbook_version_core_set_free(r->versions); free(r->path); free(r);
}
static bool target_matches(const char *path,const SourceFileIdentity *expected,char **error) {
    if (!expected) return true;
    SourceFileIdentity current; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    if (source_file_identity(path,&current,detail)!=SOURCE_DIGEST_OK || !source_file_identity_same(expected,&current))
        return fail(error,"The saved workbook changed or became unavailable since it was opened/saved. Reopen it or use Save As to keep your current changes.");
    return true;
}
static bool index_matches(const WorkbookVersionIndex *expected,const WorkbookVersionIndex *current,char **error) {
    if (!expected) return true;
    bool equal=workbook_version_index_check(expected)==VERSION_OK && current &&
        expected->document_high==current->document_high && expected->document_low==current->document_low &&
        expected->generation==current->generation && expected->saved_generation==current->saved_generation &&
        expected->saved.present==current->saved.present && !memcmp(expected->saved.key.bytes,current->saved.key.bytes,DX_VERSION_KEY_BYTES) &&
        expected->root_count==current->root_count && expected->tag_count==current->tag_count;
    for (size_t i=0;equal && i<expected->root_count;i++) equal=expected->roots[i].bytes==current->roots[i].bytes && expected->roots[i].encoding==current->roots[i].encoding &&
        !memcmp(expected->roots[i].key.bytes,current->roots[i].key.bytes,DX_VERSION_KEY_BYTES);
    for (size_t i=0;equal && i<expected->tag_count;i++) equal=expected->tags[i].created==current->tags[i].created && expected->tags[i].label.bytes==current->tags[i].label.bytes &&
        !memcmp(expected->tags[i].root.bytes,current->tags[i].root.bytes,DX_VERSION_KEY_BYTES) && !memcmp(expected->tags[i].label.data,current->tags[i].label.data,expected->tags[i].label.bytes);
    return equal || fail(error,"The saved version index changed in another session. Reopen the workbook or use Save As to keep your current changes.");
}
static WorkbookSaveResult inspect(WorkbookArchiveSaveContext *context,duckdb_connection c,const char *target,bool existing,int *core_version,char **error) {
    *core_version=0;
    if (!target_matches(target,context->control.expected_target,error)) return WORKBOOK_SAVE_FAILED;
    if (!existing) return WORKBOOK_SAVE_COMPLETE;
    Workbook previous; workbook_init(&previous); unsigned version=0; WorkbookVersionCoreSet *versions=NULL;
    WorkbookSaveResult result=WORKBOOK_SAVE_FAILED;
    if (!workbook_read_native_core(c,target,&previous,&version,error)) goto done;
    if (version==DX_VERSIONED_WORKSPACE_VERSION) {
        WorkbookVersionRestoreResult restored=workbook_version_core_set_read(c,context->parent,context->reserved,context->control.inspect,&versions,error);
        if (restored!=VERSION_RESTORE_READY) { if (restored==VERSION_RESTORE_CANCELLED) result=WORKBOOK_SAVE_CANCELLED; goto done; }
        const WorkbookVersionIndex *index=workbook_version_core_set_index(versions);
        if (index->document_high!=previous.id.high || index->document_low!=previous.id.low) { fail(error,"The existing version archive does not belong to its authored head."); goto done; }
        if (!index_matches(context->control.expected_index,index,error)) goto done;
        context->replaces_versions=true; *core_version=15;
    } else {
        duckdb_result r={0}; bool ok=duckdb_query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND lower(schema_name)='_duxcel_versions'",&r)==DuckDBSuccess;
        if (!ok) fail(error,duckdb_result_error(&r));
        else if (duckdb_row_count(&r)!=1 || duckdb_value_uint64(&r,0,0)) ok=fail(error,"Legacy version metadata cannot be silently replaced without its discriminator.");
        duckdb_destroy_result(&r); if (!ok) goto done;
        *core_version=(int)version;
    }
    result=WORKBOOK_SAVE_COMPLETE;
done:
    workbook_free(&previous); workbook_version_core_set_free(versions); return result;
}
static WorkbookSaveResult stage(WorkbookArchiveSaveContext *context,duckdb_connection c,char **error) {
    if (context->replaces_versions && !exec(c,"DROP SCHEMA _duxcel_versions CASCADE",error)) return WORKBOOK_SAVE_FAILED;
    WorkbookVersionStageResult staged=workbook_version_stage(c,context->index,context->leaves,context->count,context->reserved,context->control.stage,error);
    if (staged!=VERSION_STAGE_READY) return staged==VERSION_STAGE_CANCELLED ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
    if (!exec(c,"UPDATE _duxcel_workspace.meta SET version=16",error)) return WORKBOOK_SAVE_FAILED;
    WorkbookVersionRestoreResult prepared=workbook_version_core_set_read(c,context->parent,context->reserved,context->control.prepare,&context->receipt->versions,error);
    return prepared==VERSION_RESTORE_READY ? WORKBOOK_SAVE_COMPLETE : prepared==VERSION_RESTORE_CANCELLED ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
}
WorkbookSaveResult workbook_archive_write(const Workbook *head,const WorkbookVersionIndex *index,
    const WorkbookCorePayload *const *leaves,size_t count,const char *path,const char *parent,uint64_t reserved,
    WorkbookArchiveWriteControl control,WorkbookArchiveReceipt **out,char **error) {
    if (!head || workbook_version_index_check(index)!=VERSION_OK || index->document_high!=head->id.high || index->document_low!=head->id.low ||
        count!=index->root_count || (count && !leaves) || !path || !*path || !parent || !*parent || !out || *out || dx_path_kind(parent)!=DX_PATH_DIRECTORY) {
        fail(error,"Choose an immutable authored head, matching version closure, staging folder and empty receipt."); return WORKBOOK_SAVE_FAILED;
    }
#if !defined(__APPLE__) || defined(__EMSCRIPTEN__)
    fail(error,"Versioned archive writing requires native macOS identity/hash admission."); return WORKBOOK_SAVE_FAILED;
#endif
    char *target=workbook_target_path(path);
    if (!target) { fail(error,"The archive destination folder is unavailable."); return WORKBOOK_SAVE_FAILED; }
    if (!target_matches(target,control.expected_target,error)) { free(target); return WORKBOOK_SAVE_FAILED; }
    for (size_t i=0;i<count;i++) {
        const char *leaf=workbook_core_payload_path(leaves[i]);
        if (!leaf || dx_same_file(target,leaf)) { free(target); fail(error,"Choose an archive destination other than an immutable borrowed leaf."); return WORKBOOK_SAVE_FAILED; }
    }
    WorkbookArchiveReceipt *receipt=dx_alloc(sizeof(*receipt)); receipt->path=target;
    receipt->document=head->id; receipt->revision=head->command_revision;
    WorkbookArchiveSaveContext context={.index=index,.leaves=leaves,.count=count,.parent=parent,.reserved=reserved,.control=control,.receipt=receipt};
    WorkbookSaveResult result=workbook_save_native(head,path,control.core,(WorkbookNativeSave){&context,inspect,stage},error);
    if (result!=WORKBOOK_SAVE_COMPLETE) { workbook_archive_receipt_free(receipt); return result; }
    /* COMMIT already succeeded. This optional observation cannot turn it into
       a reported failure, allocate a required receipt, or move a live pointer. */
    char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    receipt->identity_valid=source_file_identity(receipt->path,&receipt->identity,detail)==SOURCE_DIGEST_OK;
    *out=receipt; return WORKBOOK_SAVE_COMPLETE;
}
