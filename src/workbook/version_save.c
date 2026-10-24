#include "version_save.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookVersionReceipt { WorkbookVersionPlan plan; WorkbookVersionTagPlan tag; WorkbookArchiveReceipt *archive; bool reused,tagged; };
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Version Save preparation failed."); } return false; }
const WorkbookVersionPlan *workbook_version_receipt_plan(const WorkbookVersionReceipt *r) { return r && !r->tagged ? &r->plan : NULL; }
const WorkbookArchiveReceipt *workbook_version_receipt_archive(const WorkbookVersionReceipt *r) { return r ? r->archive : NULL; }
const WorkbookVersionTagPlan *workbook_version_receipt_tag(const WorkbookVersionReceipt *r) { return r && r->tagged ? &r->tag : NULL; }
bool workbook_version_receipt_reused(const WorkbookVersionReceipt *r) { return r && r->reused; }
WorkbookVersionCoreSet *workbook_version_receipt_take_versions(WorkbookVersionReceipt *r) { return r ? workbook_archive_receipt_take_versions(r->archive) : NULL; }
void workbook_version_receipt_free(WorkbookVersionReceipt *r) { if (r) { workbook_archive_receipt_free(r->archive); free(r); } }
static size_t root_at(const WorkbookVersionIndex *index,WorkbookVersionKey key) {
    for (size_t i=0;i<index->root_count;i++) if (!memcmp(index->roots[i].key.bytes,key.bytes,DX_VERSION_KEY_BYTES)) return i;
    return SIZE_MAX;
}
static WorkbookSaveResult save(WorkbookVersionSaveRequest request,const WorkbookVersionText *label,WorkbookVersionSaveControl control,
    WorkbookVersionReceipt **out,char **error) {
    const Workbook *head=request.head;
    if (!head || !workbook_identity_valid(head) || !head->count || !out || *out || !request.path || !*request.path || !request.parent || !*request.parent ||
        !request.owner.session || request.owner.document_high!=head->id.high || request.owner.document_low!=head->id.low ||
        (unsigned)request.owner.mode>VERSION_HISTORICAL_VIEW || (unsigned)request.purpose>VERSION_STATE_EXPORT ||
        (label && request.purpose!=VERSION_USER_SAVE) ||
        (request.owner.mode==VERSION_HISTORICAL_VIEW && request.purpose!=VERSION_STATE_EXPORT)) {
        fail(error,"Choose a supported immutable head, valid write owner/purpose, destination and empty receipt."); return WORKBOOK_SAVE_FAILED;
    }
    WorkbookVersionIndex empty={.document_high=head->id.high,.document_low=head->id.low};
    const WorkbookVersionIndex *base=request.base ? workbook_version_core_set_index(request.base) : &empty;
    if (workbook_version_index_check(base)!=VERSION_OK || base->document_high!=head->id.high || base->document_low!=head->id.low) {
        fail(error,"The retained version index does not belong to this authored head."); return WORKBOOK_SAVE_FAILED;
    }
    if (label) {
        WorkbookVersionStatus status=workbook_version_tag_label_check(base,*label);
        if (status) { fail(error,workbook_version_message(status)); return WORKBOOK_SAVE_FAILED; }
    }
    WorkbookVersionReceipt *receipt=dx_alloc(sizeof(*receipt)); receipt->tagged=label!=NULL; WorkbookCorePayload *capture=NULL;
    WorkbookVersionRoot candidate={0}; WorkbookVersionRoot *roots=NULL; const WorkbookCorePayload **leaves=NULL; WorkbookVersionTag *tags=NULL;
    WorkbookSaveResult result=WORKBOOK_SAVE_FAILED;
    if (request.purpose==VERSION_USER_SAVE) {
        WorkbookCorePayloadResult captured=workbook_core_payload_capture(head,request.parent,control.capture,&capture,error);
        if (captured!=CORE_PAYLOAD_READY) { if (captured==CORE_PAYLOAD_CANCELLED) result=WORKBOOK_SAVE_CANCELLED; goto done; }
        const WorkbookCorePayloadInfo *info=workbook_core_payload_info(capture);
        candidate.bytes=info->bytes; candidate.encoding=info->encoding; memcpy(candidate.key.bytes,info->sha256,DX_VERSION_KEY_BYTES);
        if (base->saved.present) {
            size_t saved=root_at(base,base->saved.key); bool equal=false;
            WorkbookCorePayloadResult compared=workbook_core_payload_equal(capture,workbook_version_core_set_leaf(request.base,saved),control.compare,&equal,error);
            if (compared!=CORE_PAYLOAD_READY) { if (compared==CORE_PAYLOAD_CANCELLED) result=WORKBOOK_SAVE_CANCELLED; goto done; }
            if (equal) { candidate=base->roots[saved]; receipt->reused=true; }
        }
        /* Explicit tags share any completely equivalent retained root. User
           Save also reuses named roots, rather than duplicating a tagged state. */
        for (size_t i=0;i<base->root_count && !receipt->reused;i++) {
            if (base->saved.present && i==root_at(base,base->saved.key)) continue;
            bool retained=label!=NULL;
            for (size_t t=0;t<base->tag_count && !retained;t++) retained=!memcmp(base->tags[t].root.bytes,base->roots[i].key.bytes,DX_VERSION_KEY_BYTES);
            if (!retained) continue;
            bool equal=false;
            WorkbookCorePayloadResult compared=workbook_core_payload_equal(capture,workbook_version_core_set_leaf(request.base,i),control.compare,&equal,error);
            if (compared!=CORE_PAYLOAD_READY) { if (compared==CORE_PAYLOAD_CANCELLED) result=WORKBOOK_SAVE_CANCELLED; goto done; }
            if (equal) { candidate=base->roots[i]; receipt->reused=true; }
        }
    }
    WorkbookVersionStatus planned;
    if (label) {
        planned=workbook_version_tag_prepare(base,request.owner,&candidate,*label,&receipt->tag);
        if (!planned) receipt->plan=receipt->tag.save;
    } else planned=workbook_version_prepare(base,request.owner,request.purpose,
        request.purpose==VERSION_USER_SAVE ? &candidate : NULL,&receipt->plan);
    if (planned!=VERSION_OK) { fail(error,workbook_version_message(planned)); goto done; }
    WorkbookVersionIndex next=*base;
    next.generation=receipt->plan.next_generation; next.saved_generation=receipt->plan.next_saved_generation; next.saved=receipt->plan.after_saved;
    next.root_count+=receipt->plan.adds_root;
    if (label) {
        next.generation=next.saved_generation=receipt->tag.next_generation; next.tag_count++;
        tags=dx_alloc(next.tag_count*sizeof(*tags));
        for (size_t i=0;i<base->tag_count;i++) tags[i]=base->tags[i];
        tags[base->tag_count]=(WorkbookVersionTag){.label={receipt->tag.label,receipt->tag.label_bytes},.root=candidate.key,.created=next.generation};
        next.tags=tags;
    }
    roots=dx_alloc(next.root_count*sizeof(*roots)); leaves=dx_alloc(next.root_count*sizeof(*leaves));
    for (size_t i=0;i<base->root_count;i++) { roots[i]=base->roots[i]; leaves[i]=workbook_version_core_set_leaf(request.base,i); }
    if (receipt->plan.adds_root) { roots[base->root_count]=candidate; leaves[base->root_count]=capture; }
    next.roots=roots;
    result=workbook_archive_write(head,&next,leaves,next.root_count,request.path,request.parent,request.reserved_bytes,control.archive,&receipt->archive,error);
done:
    workbook_core_payload_free(capture); free(roots); free(leaves); free(tags);
    if (result==WORKBOOK_SAVE_COMPLETE) { *out=receipt; return result; }
    workbook_version_receipt_free(receipt); return result;
}

WorkbookSaveResult workbook_version_save(WorkbookVersionSaveRequest request,WorkbookVersionSaveControl control,WorkbookVersionReceipt **out,char **error) {
    return save(request,NULL,control,out,error);
}
WorkbookSaveResult workbook_version_tag_save(WorkbookVersionSaveRequest request,WorkbookVersionText label,WorkbookVersionSaveControl control,WorkbookVersionReceipt **out,char **error) {
    return save(request,&label,control,out,error);
}
