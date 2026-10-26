#include "history_native_capture.h"
#include "history_after_save.h"
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
        if (!actual || actual->encoding!=15 || root->encoding!=15 || actual->document.high!=view->document.high || actual->document.low!=view->document.low ||
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
WorkbookHistoryNativeStatus workbook_history_native_check_parts(const WorkbookHistoryProfile *profile,const WorkbookVersionCoreSet *roots,
    const WorkbookCorePayload *head,WorkbookHistoryCapturedPosition captured,WorkbookHistoryNativeControl control,char **error) {
    WorkbookHistoryNativeMeasure m; WorkbookHistoryNativeCaps caps={UINT64_MAX,SIZE_MAX};
    WorkbookHistoryNativeStatus status=workbook_history_native_measure(profile,roots,head,captured,caps,NULL,&m); if (status) return status;
    Check check={.control=control,.root=SIZE_MAX}; status=verify(head,&check,error); if (status) return status;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots);
    for (size_t i=0;i<index->root_count;i++) { check.root=i; status=verify(workbook_version_core_set_leaf(roots,i),&check,error); if (status) return status; }
    if (cancelled(&check,0)) return HISTORY_NATIVE_CANCELLED;
    return workbook_history_native_measure(profile,roots,head,captured,caps,NULL,&m);
}
WorkbookHistoryNativeStatus workbook_history_native_check(const WorkbookHistoryNativeCapture *c,WorkbookHistoryNativeControl control,char **error) {
    return c ? workbook_history_native_check_parts(c->profile,c->roots,c->head,c->captured,control,error) : HISTORY_NATIVE_INVALID;
}
WorkbookHistoryNativeStatus workbook_history_native_take(WorkbookHistoryProfile **profile,WorkbookVersionCoreSet **roots,
    WorkbookCorePayload **head,WorkbookHistoryCapturedPosition captured,WorkbookHistoryNativeCaps caps,
    WorkbookHistoryNativeControl control,WorkbookHistoryNativeCapture **out,char **error) {
    if (!profile || !roots || !head || !out) return HISTORY_NATIVE_INVALID;
    if (*out && ((*out)->profile==*profile || (*out)->head==*head)) return HISTORY_NATIVE_INVALID;
    WorkbookHistoryNativeMeasure m;
    WorkbookHistoryNativeStatus status=workbook_history_native_measure(*profile,*roots,*head,captured,caps,*out,&m); if (status) return status;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(*roots);
    for (size_t i=0;i<index->root_count;i++) if (workbook_version_core_set_leaf(*roots,i)==*head) return HISTORY_NATIVE_ASSOCIATION;
    status=workbook_history_native_check_parts(*profile,*roots,*head,captured,control,error); if (status) return status;
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

struct WorkbookHistoryAfterSave {
    const void *borrowed_source;
    WorkbookHistoryProfile *profile;
    WorkbookVersionCoreSet *roots;
    WorkbookHistoryCapturedPosition captured;
    WorkbookVersionPlan save;
    size_t selected;
    bool semantic_reuse;
};
static size_t root_at(const WorkbookVersionIndex *index,WorkbookVersionKey id) {
    size_t i=0; while (i<index->root_count && !key(index->roots[i].key,id)) i++; return i;
}
static bool same_root(WorkbookVersionRoot a,WorkbookVersionRoot b) { return key(a.key,b.key) && a.bytes==b.bytes && a.encoding==b.encoding; }
static bool after_index(const WorkbookVersionIndex *before,const WorkbookVersionIndex *after,const WorkbookVersionPlan *save) {
    if (!after || after->document_high!=before->document_high || after->document_low!=before->document_low ||
        after->generation!=save->next_generation || after->saved_generation!=save->next_saved_generation ||
        after->saved.present!=save->after_saved.present || !key(after->saved.key,save->after_saved.key) ||
        after->root_count!=before->root_count+(save->adds_root ? 1u : 0u) || after->tag_count!=before->tag_count) return false;
    for (size_t i=0;i<before->root_count;i++) { size_t at=root_at(after,before->roots[i].key); if (at==after->root_count || !same_root(before->roots[i],after->roots[at])) return false; }
    size_t selected=root_at(after,save->root.key); if (selected==after->root_count || !same_root(after->roots[selected],save->root)) return false;
    for (size_t i=0;i<before->tag_count;i++) {
        const WorkbookVersionTag *a=before->tags+i; size_t j=0; while (j<after->tag_count && a->created!=after->tags[j].created) j++;
        if (j==after->tag_count || !key(a->root,after->tags[j].root) || a->label.bytes!=after->tags[j].label.bytes || memcmp(a->label.data,after->tags[j].label.data,a->label.bytes)) return false;
    }
    return true;
}
static bool staging_add(size_t *total,size_t count,size_t item) { if (count && item>SIZE_MAX/count) return false; size_t n=count*item; if (n>SIZE_MAX-*total) return false; *total+=n; return true; }
static uint64_t cursor_lineage(const WorkbookHistoryPath *path) {
    uint64_t lineage=path->segments[0].lineage;
    for (size_t i=1;i<path->segment_count;i++) if (path->cursor>path->segments[i].from) lineage=path->segments[i].lineage;
    return lineage;
}
typedef struct {
    const WorkbookHistoryProfile *profile;
    const WorkbookVersionCoreSet *roots;
    const WorkbookCorePayload *head;
    WorkbookHistoryCapturedPosition captured;
    uint64_t physical_bytes;
    const void *borrowed;
    const WorkbookVersionPublication *publication;
} AfterSource;
static WorkbookHistoryNativeStatus after_save_prepare(const AfterSource *source,
    const WorkbookVersionCoreSet *prepared_after,const WorkbookVersionPlan *save,WorkbookHistoryProfileCaps profile_caps,
    WorkbookHistoryNativeCaps native_caps,WorkbookHistoryAfterSaveControl control,WorkbookHistoryAfterSave **out,char **error) {
    if (!source || !prepared_after || !save || !out || *out || save->purpose!=VERSION_USER_SAVE) return HISTORY_NATIVE_INVALID;
    WorkbookVersionPlan frozen=*save; save=&frozen;
    const WorkbookHistoryProfileView *old=workbook_history_profile_view(source->profile);
    const WorkbookVersionIndex *before=workbook_version_core_set_index(source->roots),*after=workbook_version_core_set_index(prepared_after);
    if (!workbook_version_can_publish(before,source->captured.owner,save) || !after_index(before,after,save)) return HISTORY_NATIVE_ASSOCIATION;
    size_t selected=root_at(after,save->root.key); const WorkbookCorePayload *leaf=workbook_version_core_set_leaf(prepared_after,selected);
    const WorkbookCorePayloadInfo *head=workbook_core_payload_info(source->head),*chosen=workbook_core_payload_info(leaf);
    if (!head || !chosen || chosen->document.high!=source->captured.owner.document_high || chosen->document.low!=source->captured.owner.document_low || chosen->encoding!=15) return HISTORY_NATIVE_ASSOCIATION;
    bool same=head->bytes==chosen->bytes && head->encoding==chosen->encoding && !memcmp(head->sha256,chosen->sha256,DX_SOURCE_DIGEST_BYTES);
    if (save->adds_root && !same) return HISTORY_NATIVE_ASSOCIATION;
    size_t count=old->kind==HISTORY_PROFILE_RETAINED ? old->current.count : 0,anchor_capacity=old->kind==HISTORY_PROFILE_RETAINED ? old->anchors->count+1u : 0;
    size_t scratch=0,fixed=sizeof(WorkbookHistoryAfterSave)+workbook_history_profile_owned(source->profile);
    if (!staging_add(&scratch,count,sizeof(WorkbookHistoryProfileRecord)+sizeof(size_t)) || !staging_add(&scratch,anchor_capacity,sizeof(WorkbookHistoryAnchor)) ||
        !staging_add(&fixed,1,scratch) || fixed>profile_caps.staging_bytes || sizeof(WorkbookHistoryAfterSave)+scratch>native_caps.additional_bytes) return HISTORY_NATIVE_LIMIT;
    uint64_t physical=head->bytes;
    for (size_t i=0;i<after->root_count;i++) { if (after->roots[i].bytes>UINT64_MAX-physical) return HISTORY_NATIVE_LIMIT; physical+=after->roots[i].bytes; }
    if (source->physical_bytes>UINT64_MAX-physical || physical+source->physical_bytes>native_caps.physical_bytes) return HISTORY_NATIVE_LIMIT;
    WorkbookHistoryNativeStatus checked=workbook_history_native_check_parts(source->profile,source->roots,source->head,source->captured,control.verify,error); if (checked) return checked;
    Check proof={.control=control.verify};
    for (size_t i=0;i<after->root_count;i++) { proof.root=i; checked=verify(workbook_version_core_set_leaf(prepared_after,i),&proof,error); if (checked) return checked; }
    if (!same) {
        if (source->publication) {
            /* Consume the actual controller's completed comparison witness,
               bound to its actual selected body, never a caller boolean. */
            const WorkbookCorePayloadInfo *witness=workbook_core_payload_info(workbook_version_publication_selected(source->publication));
            if (!workbook_version_publication_compared(source->publication) || !witness || witness->bytes!=chosen->bytes || witness->encoding!=chosen->encoding || memcmp(witness->sha256,chosen->sha256,32)) return HISTORY_NATIVE_ASSOCIATION;
        } else {
            bool equal=false; WorkbookCorePayloadResult compared=workbook_core_payload_equal(source->head,leaf,control.compare,&equal,error);
            if (compared!=CORE_PAYLOAD_READY) return compared==CORE_PAYLOAD_CANCELLED ? HISTORY_NATIVE_CANCELLED : compared==CORE_PAYLOAD_UNSUPPORTED ? HISTORY_NATIVE_UNSUPPORTED : HISTORY_NATIVE_FAILED;
            if (!equal) return HISTORY_NATIVE_ASSOCIATION;
        }
    }
    WorkbookHistoryAnchor *anchors=anchor_capacity ? dx_alloc(anchor_capacity*sizeof(*anchors)) : NULL;
    WorkbookHistoryProfileRecord *records=count ? dx_alloc(count*sizeof(*records)) : NULL; size_t *map=count ? dx_alloc(count*sizeof(*map)) : NULL;
    WorkbookHistoryAnchors public={0}; WorkbookHistoryProfileView view={.document=old->document,.container_version=16,.core_version=15,.kind=old->kind,.versions=after};
    WorkbookHistoryNativeStatus status=HISTORY_NATIVE_ASSOCIATION;
    if (old->kind==HISTORY_PROFILE_RETAINED) {
        const WorkbookHistoryPath *path=old->current.path; uint64_t lineage=cursor_lineage(path),saved_anchor=old->anchors->saved_anchor,high_water=old->anchors->high_water; size_t n=0;
        if (!save->updates_saved) {
            const WorkbookHistoryAnchor *existing=NULL;
            for (size_t i=0;i<old->anchors->count;i++) if (old->anchors->items[i].boundary.anchor==saved_anchor) existing=old->anchors->items+i;
            if (!existing || existing->boundary.position!=path->cursor || !key(existing->boundary.root,save->root.key)) goto done;
            memcpy(anchors,old->anchors->items,old->anchors->count*sizeof(*anchors)); n=old->anchors->count;
        } else {
            if (!before->saved.present && !high_water) saved_anchor=0;
            else { if (high_water==UINT64_MAX) { status=HISTORY_NATIVE_LIMIT; goto done; } saved_anchor=++high_water; }
            for (size_t i=0;i<old->anchors->count;i++) {
                bool named=false; for (size_t j=0;j<old->anchors->tag_count;j++) if (old->anchors->tags[j].anchor==old->anchors->items[i].boundary.anchor) named=true;
                if (named) anchors[n++]=old->anchors->items[i];
            }
            anchors[n++]=(WorkbookHistoryAnchor){{saved_anchor,path->cursor,save->root.key},lineage};
        }
        public=(WorkbookHistoryAnchors){old->document.high,old->document.low,high_water,saved_anchor,anchors,n,old->anchors->tags,old->anchors->tag_count};
        view.anchors=&public; view.current=(WorkbookHistoryProfilePath){path,map,count}; view.saved=view.current; view.records=records; view.count=count;
        for (size_t i=0;i<count;i++) { records[i]=old->records[old->current.records[i]]; map[i]=i; }
    }
    WorkbookHistoryAfterSave *plan=dx_alloc(sizeof(*plan)); plan->borrowed_source=source->borrowed; plan->captured=source->captured; plan->save=*save; plan->selected=selected; plan->semantic_reuse=!same;
    plan->roots=workbook_version_core_set_retain(prepared_after); if (!plan->roots) { free(plan); status=HISTORY_NATIVE_LIMIT; goto done; }
    profile_caps.staging_bytes-=fixed;
    WorkbookHistoryProfileStatus admitted=workbook_history_profile_prepare(view,profile_caps,&plan->profile);
    if (admitted!=HISTORY_PROFILE_OK) { status=admitted==HISTORY_PROFILE_LIMIT ? HISTORY_NATIVE_LIMIT : HISTORY_NATIVE_ASSOCIATION; workbook_history_after_save_free(&plan); goto done; }
    status=workbook_history_native_check_parts(plan->profile,plan->roots,leaf,plan->captured,control.verify,error);
    if (status) { workbook_history_after_save_free(&plan); goto done; }
    *out=plan; status=HISTORY_NATIVE_READY;
done:
    free(anchors); free(records); free(map); return status;
}
WorkbookHistoryNativeStatus workbook_history_after_save_prepare(const WorkbookHistoryNativeCapture *source,
    const WorkbookVersionCoreSet *prepared_after,const WorkbookVersionPlan *save,WorkbookHistoryProfileCaps profile_caps,
    WorkbookHistoryNativeCaps native_caps,WorkbookHistoryAfterSaveControl control,WorkbookHistoryAfterSave **out,char **error) {
    if (!source) return HISTORY_NATIVE_INVALID;
    AfterSource borrowed={source->profile,source->roots,source->head,source->captured,source->physical_bytes,source,NULL};
    return after_save_prepare(&borrowed,prepared_after,save,profile_caps,native_caps,control,out,error);
}
WorkbookHistoryNativeStatus workbook_history_after_save_prepare_publication(const WorkbookHistoryProfile *before,
    const WorkbookVersionPublication *publication,duckdb_connection connection,const WorkbookVersionCoreSet *prepared_after,
    WorkbookHistoryProfileCaps profile_caps,WorkbookHistoryNativeCaps native_caps,
    WorkbookHistoryAfterSaveControl control,WorkbookHistoryAfterSave **out,char **error) {
    if (!before || !publication || workbook_version_publication_intent(publication)!=VERSION_PUBLICATION_USER_SAVE) return HISTORY_NATIVE_INVALID;
    if (!workbook_version_publication_snapshot(publication,connection,prepared_after)) return HISTORY_NATIVE_ASSOCIATION;
    const WorkbookVersionPlan *save=workbook_version_publication_plan(publication);
    const WorkbookVersionCoreSet *base=workbook_version_publication_base(publication);
    const WorkbookCorePayload *head=workbook_version_publication_capture(publication);
    if (!save || !base || !head) return HISTORY_NATIVE_INVALID;
    WorkbookHistoryCapturedPosition captured={save->owner,workbook_version_publication_revision(publication)};
    WorkbookHistoryNativeMeasure measure;
    WorkbookHistoryNativeStatus checked=workbook_history_native_measure(before,base,head,captured,(WorkbookHistoryNativeCaps){UINT64_MAX,SIZE_MAX},NULL,&measure);
    if (checked) return checked;
    AfterSource borrowed={before,base,head,captured,measure.physical_bytes,publication,publication};
    return after_save_prepare(&borrowed,prepared_after,save,profile_caps,native_caps,control,out,error);
}
const WorkbookHistoryProfile *workbook_history_after_save_profile(const WorkbookHistoryAfterSave *p) { return p ? p->profile : NULL; }
const WorkbookVersionCoreSet *workbook_history_after_save_roots(const WorkbookHistoryAfterSave *p) { return p ? p->roots : NULL; }
const WorkbookCorePayload *workbook_history_after_save_selected(const WorkbookHistoryAfterSave *p) { return p ? workbook_version_core_set_leaf(p->roots,p->selected) : NULL; }
const WorkbookHistoryCapturedPosition *workbook_history_after_save_position(const WorkbookHistoryAfterSave *p) { return p ? &p->captured : NULL; }
bool workbook_history_after_save_semantic_reuse(const WorkbookHistoryAfterSave *p) { return p && p->semantic_reuse; }
void workbook_history_after_save_free(WorkbookHistoryAfterSave **owned) { if (!owned || !*owned) return; WorkbookHistoryAfterSave *p=*owned; workbook_history_profile_free(&p->profile); workbook_version_core_set_free(p->roots); free(p); *owned=NULL; }
