#include "history_native_capture.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookHistoryNativeCapture {
    WorkbookHistoryProfile *profile;
    WorkbookVersionCoreSet *roots;
    WorkbookCorePayload *head;
    WorkbookHistoryCapturedPosition captured;
    uint64_t physical_bytes;
};
static bool key(WorkbookVersionKey a,WorkbookVersionKey b) { return !memcmp(a.bytes,b.bytes,DX_VERSION_KEY_BYTES); }
static bool index_equal(const WorkbookVersionIndex *a,const WorkbookVersionIndex *b) {
    if (!a || !b || a->document_high!=b->document_high || a->document_low!=b->document_low || a->generation!=b->generation ||
        a->saved_generation!=b->saved_generation || a->saved.present!=b->saved.present || !key(a->saved.key,b->saved.key) ||
        a->root_count!=b->root_count || a->tag_count!=b->tag_count) return false;
    /* Root order is not identity. Tag creation generations are unique in an
       admitted index; compare their exact labels/root bindings independently. */
    for (size_t i=0;i<a->root_count;i++) {
        const WorkbookVersionRoot *root=a->roots+i; size_t j=0;
        while (j<b->root_count && !key(root->key,b->roots[j].key)) j++;
        if (j==b->root_count || root->bytes!=b->roots[j].bytes || root->encoding!=b->roots[j].encoding) return false;
    }
    for (size_t i=0;i<a->tag_count;i++) {
        const WorkbookVersionTag *tag=a->tags+i; size_t j=0;
        while (j<b->tag_count && tag->created!=b->tags[j].created) j++;
        if (j==b->tag_count || !key(tag->root,b->tags[j].root) || tag->label.bytes!=b->tags[j].label.bytes || memcmp(tag->label.data,b->tags[j].label.data,tag->label.bytes)) return false;
    }
    return true;
}
WorkbookHistoryNativeStatus workbook_history_native_measure(const WorkbookHistoryProfile *profile,const WorkbookVersionCoreSet *roots,
    const WorkbookCorePayload *head,WorkbookHistoryCapturedPosition captured,WorkbookHistoryNativeCaps caps,const WorkbookHistoryNativeCapture *prior,WorkbookHistoryNativeMeasure *out) {
    if (!out || !profile || !roots || !head || !captured.owner.session || captured.owner.mode!=VERSION_WORKING_HEAD) return HISTORY_NATIVE_INVALID;
    const WorkbookHistoryProfileView *view=workbook_history_profile_view(profile);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots);
    const WorkbookCorePayloadInfo *info=workbook_core_payload_info(head); uint64_t revision=0;
    if (!view || view->container_version!=16 || view->core_version!=15 || !info || info->encoding!=15) return HISTORY_NATIVE_UNSUPPORTED;
    if (!index_equal(view->versions,index) || captured.owner.document_high!=view->document.high || captured.owner.document_low!=view->document.low ||
        info->document.high!=view->document.high || info->document.low!=view->document.low || !workbook_core_payload_revision(head,&revision) || revision!=captured.revision) return HISTORY_NATIVE_ASSOCIATION;
    WorkbookHistoryNativeMeasure m={.head_bytes=info->bytes,.prior_physical_bytes=prior ? prior->physical_bytes : 0,.additional_bytes=sizeof(WorkbookHistoryNativeCapture)*(prior ? 2u : 1u)};
    if (m.additional_bytes>caps.additional_bytes) return HISTORY_NATIVE_LIMIT;
    for (size_t i=0;i<index->root_count;i++) {
        const WorkbookVersionRoot *root=index->roots+i;
        const WorkbookCorePayload *leaf=workbook_version_core_set_leaf(roots,i); const WorkbookCorePayloadInfo *actual=workbook_core_payload_info(leaf);
        if (leaf==head || !actual || actual->encoding!=15 || root->encoding!=15 || actual->document.high!=view->document.high || actual->document.low!=view->document.low ||
            actual->bytes!=root->bytes || memcmp(actual->sha256,root->key.bytes,DX_VERSION_KEY_BYTES)) return HISTORY_NATIVE_ASSOCIATION;
        if (root->bytes>UINT64_MAX-m.root_bytes) return HISTORY_NATIVE_LIMIT; m.root_bytes+=root->bytes;
    }
    if (m.root_bytes>UINT64_MAX-m.head_bytes) return HISTORY_NATIVE_LIMIT; m.physical_bytes=m.root_bytes+m.head_bytes;
    if (m.prior_physical_bytes>UINT64_MAX-m.physical_bytes) return HISTORY_NATIVE_LIMIT; m.physical_bytes+=m.prior_physical_bytes;
    if (m.physical_bytes>caps.physical_bytes) return HISTORY_NATIVE_LIMIT;
    *out=m; return HISTORY_NATIVE_READY;
}
typedef struct { WorkbookHistoryNativeControl control; size_t root; bool stopped; } Check;
static bool cancelled(void *context,uint64_t bytes) {
    Check *check=context;
    if (!check->stopped && check->control.cancelled) check->stopped=check->control.cancelled(check->control.context,check->root,bytes);
    return check->stopped;
}
static WorkbookHistoryNativeStatus verify(const WorkbookCorePayload *leaf,Check *check,char **error) {
    if (cancelled(check,0)) return HISTORY_NATIVE_CANCELLED;
    WorkbookCorePayloadResult status=workbook_core_payload_verify(leaf,cancelled,check,error);
    if (status==CORE_PAYLOAD_READY) return HISTORY_NATIVE_READY;
    return status==CORE_PAYLOAD_CANCELLED ? HISTORY_NATIVE_CANCELLED : status==CORE_PAYLOAD_UNSUPPORTED ? HISTORY_NATIVE_UNSUPPORTED : HISTORY_NATIVE_FAILED;
}
WorkbookHistoryNativeStatus workbook_history_native_take(WorkbookHistoryProfile **profile,WorkbookVersionCoreSet **roots,
    WorkbookCorePayload **head,WorkbookHistoryCapturedPosition captured,WorkbookHistoryNativeCaps caps,
    WorkbookHistoryNativeControl control,WorkbookHistoryNativeCapture **out,char **error) {
    if (!profile || !roots || !head || !out) return HISTORY_NATIVE_INVALID;
    if (*out && ((*out)->profile==*profile || (*out)->head==*head)) return HISTORY_NATIVE_INVALID;
    WorkbookHistoryNativeMeasure m;
    WorkbookHistoryNativeStatus status=workbook_history_native_measure(*profile,*roots,*head,captured,caps,*out,&m); if (status) return status;
    Check check={.control=control,.root=SIZE_MAX}; status=verify(*head,&check,error); if (status) return status;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(*roots);
    for (size_t i=0;i<index->root_count;i++) { check.root=i; status=verify(workbook_version_core_set_leaf(*roots,i),&check,error); if (status) return status; }
    if (cancelled(&check,0)) return HISTORY_NATIVE_CANCELLED;
    /* Inputs are exclusively owned/immutable through the worker call. Recheck
       their typed association after the final progress callback, before taking
       ownership; no callbacks or fallible operations follow the handoff. */
    status=workbook_history_native_measure(*profile,*roots,*head,captured,caps,*out,&m); if (status) return status;
    WorkbookHistoryNativeCapture *next=dx_alloc(sizeof(*next));
    *next=(WorkbookHistoryNativeCapture){*profile,*roots,*head,captured,m.head_bytes+m.root_bytes}; *profile=NULL; *roots=NULL; *head=NULL;
    workbook_history_native_free(out); *out=next; return HISTORY_NATIVE_READY;
}
const WorkbookHistoryProfile *workbook_history_native_profile(const WorkbookHistoryNativeCapture *c) { return c ? c->profile : NULL; }
const WorkbookVersionCoreSet *workbook_history_native_roots(const WorkbookHistoryNativeCapture *c) { return c ? c->roots : NULL; }
const WorkbookCorePayload *workbook_history_native_head(const WorkbookHistoryNativeCapture *c) { return c ? c->head : NULL; }
const WorkbookHistoryCapturedPosition *workbook_history_native_position(const WorkbookHistoryNativeCapture *c) { return c ? &c->captured : NULL; }
void workbook_history_native_free(WorkbookHistoryNativeCapture **owned) {
    if (!owned || !*owned) return; WorkbookHistoryNativeCapture *c=*owned;
    workbook_history_profile_free(&c->profile); workbook_version_core_set_free(c->roots); workbook_core_payload_free(c->head); free(c); *owned=NULL;
}
