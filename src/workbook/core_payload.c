#include "core_payload.h"
#include "workbook_codec.h"
#include "../platform/platform.h"
#include "../platform/directory.h"
#include "../platform/file_lease.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct WorkbookCorePayload {
    WorkbookCorePayloadInfo info;
    SourceDigest digest;
    SourceFileIdentity lease_identity;
    char *directory,*path,*lease_path;
    DxFileLease *lease;
    bool sealed,retired;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool inventory(const WorkbookCorePayload *p,char **error) {
    DxDirectory *directory=NULL;
    if (dx_path_kind(p->directory)!=DX_PATH_DIRECTORY || dx_directory_open(p->directory,&directory,error)!=DX_DIRECTORY_OPEN) return fail(error,"The private core payload folder is unavailable.");
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0; bool known=true;
    while ((next=dx_directory_next(directory,&entry,error))==DX_DIRECTORY_ENTRY) {
        if (++count>2 || entry.kind!=DX_PATH_FILE || (strcmp(entry.name,"core.dxls") && strcmp(entry.name,"lease.lock"))) { known=false; break; }
    }
    dx_directory_close(directory);
    return (known && next==DX_DIRECTORY_END) || fail(error,"Unexpected core payload artifacts were kept; no cleanup was performed.");
}
static bool identities(const WorkbookCorePayload *p,char **error) {
    SourceFileIdentity file={0},lease={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    return (p->sealed && p->lease &&
        source_file_identity(p->path,&file,detail)==SOURCE_DIGEST_OK &&
        source_file_identity(p->lease_path,&lease,detail)==SOURCE_DIGEST_OK &&
        source_file_identity_same(&file,&p->digest.identity) && source_file_identity_same(&lease,&p->lease_identity)) ||
        fail(error,"Core payload artifacts changed after sealing; they were kept.");
}
const WorkbookCorePayloadInfo *workbook_core_payload_info(const WorkbookCorePayload *p) { return p && p->sealed && !p->retired ? &p->info : NULL; }
const char *workbook_core_payload_path(const WorkbookCorePayload *p) { return p && p->sealed && !p->retired ? p->path : NULL; }
const char *workbook_core_payload_directory(const WorkbookCorePayload *p) { return p && !p->retired ? p->directory : NULL; }
WorkbookCorePayloadResult workbook_core_payload_verify(const WorkbookCorePayload *p,SourceDigestCancel cancelled,void *context,char **error) {
    if (!p || p->retired || !inventory(p,error) || !identities(p,error)) { fail(error,"A live, unchanged sealed core payload is required."); return CORE_PAYLOAD_FAILED; }
    SourceDigest digest={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    SourceDigestStatus status=source_digest_file(p->path,cancelled,context,&digest,detail);
    if (status!=SOURCE_DIGEST_OK) {
        fail(error,detail);
        return status==SOURCE_DIGEST_CANCELLED ? CORE_PAYLOAD_CANCELLED : status==SOURCE_DIGEST_UNSUPPORTED ? CORE_PAYLOAD_UNSUPPORTED : CORE_PAYLOAD_FAILED;
    }
    if (!source_file_identity_same(&digest.identity,&p->digest.identity) || digest.bytes!=p->info.bytes || memcmp(digest.sha256,p->info.sha256,DX_SOURCE_DIGEST_BYTES) || !identities(p,error) || !inventory(p,error)) {
        fail(error,"The sealed core payload failed its exact-byte integrity check."); return CORE_PAYLOAD_FAILED;
    }
    return CORE_PAYLOAD_READY;
}
bool workbook_core_payload_retire(WorkbookCorePayload *p,char **error) {
    if (!p) return fail(error,"Choose an owned core payload to retire.");
    if (p->retired) return true;
    if (!inventory(p,error) || !identities(p,error)) return false;
    /* Keep the lease held through fixed-file deletion. Never enumerate/delete
       unrelated content, even inside a private directory created by capture. */
    if (remove(p->path) || remove(p->lease_path) || remove(p->directory)) return fail(error,"Some private core payload artifacts could not be removed.");
    p->retired=true; return true;
}
void workbook_core_payload_free(WorkbookCorePayload *p) {
    if (!p) return;
    if (p->sealed && !p->retired) workbook_core_payload_retire(p,NULL);
    dx_file_lease_release(p->lease);
    free(p->directory); free(p->path); free(p->lease_path); free(p);
}
static void cleanup_capture(WorkbookCorePayload *p) {
    /* Only an unpublished child created in this call reaches this path. A
       writer failure may leave no core file. Unknown artifacts block cleanup. */
    if (p->directory && inventory(p,NULL)) {
        if (p->path && dx_path_independent_file(p->path)) remove(p->path);
        if (p->lease_path && p->lease && dx_path_independent_file(p->lease_path)) remove(p->lease_path);
        dx_remove_temp_directory(p->directory);
    }
}
WorkbookCorePayloadResult workbook_core_payload_capture(const Workbook *book,const char *parent,
    WorkbookCorePayloadControl control,WorkbookCorePayload **out,char **error) {
    if (!book || !parent || !*parent || !out || *out) { fail(error,"Choose an immutable workbook, existing staging folder and empty payload handle."); return CORE_PAYLOAD_FAILED; }
#if !defined(__APPLE__) || defined(__EMSCRIPTEN__)
    (void)control; fail(error,"Core payload sealing requires the native macOS SDK."); return CORE_PAYLOAD_UNSUPPORTED;
#endif
    WorkbookCorePayload *p=dx_alloc(sizeof(*p)); WorkbookCorePayloadResult result=CORE_PAYLOAD_FAILED;
    char *resolved=dx_absolute_path(parent);
    if (!resolved || dx_path_kind(resolved)!=DX_PATH_DIRECTORY) { free(resolved); fail(error,"The core payload staging folder is unavailable."); goto done; }
    p->directory=dx_private_directory(resolved); free(resolved);
    if (!p->directory) { fail(error,"Cannot create a private core payload folder."); goto done; }
    p->path=dx_printf("%s/core.dxls",p->directory); p->lease_path=dx_printf("%s/lease.lock",p->directory);
    if (dx_file_lease_acquire(p->lease_path,true,&p->lease,error)!=DX_FILE_LEASE_ACQUIRED) goto done;
    char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    if (source_file_identity(p->lease_path,&p->lease_identity,detail)!=SOURCE_DIGEST_OK) { fail(error,detail); goto done; }
    WorkbookSaveResult saved=workbook_save_core_payload(book,p->path,control.save,error);
    if (saved!=WORKBOOK_SAVE_COMPLETE) { result=saved==WORKBOOK_SAVE_CANCELLED ? CORE_PAYLOAD_CANCELLED : CORE_PAYLOAD_FAILED; goto done; }
    SourceDigestStatus status=source_digest_file(p->path,control.cancelled,control.context,&p->digest,detail);
    if (status!=SOURCE_DIGEST_OK) { fail(error,detail); result=status==SOURCE_DIGEST_CANCELLED ? CORE_PAYLOAD_CANCELLED : status==SOURCE_DIGEST_UNSUPPORTED ? CORE_PAYLOAD_UNSUPPORTED : CORE_PAYLOAD_FAILED; goto done; }
    if (!p->digest.bytes || p->digest.identity.links!=1) { fail(error,"A core payload requires closed independent bytes."); goto done; }
    p->info=(WorkbookCorePayloadInfo){.document=book->id,.bytes=p->digest.bytes,.encoding=workbook_codec_min_write_version(book)};
    memcpy(p->info.sha256,p->digest.sha256,DX_SOURCE_DIGEST_BYTES); p->sealed=true;
    if (!identities(p,error) || !inventory(p,error)) goto done;
    *out=p; return CORE_PAYLOAD_READY;
done:
    cleanup_capture(p); p->sealed=false; workbook_core_payload_free(p); return result;
}
