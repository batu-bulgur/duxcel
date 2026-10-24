#include "worker_archive.h"
#include "../workbook/version_seed.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookNativeIO {
    QueryWorker *worker;
    JobKind kind;
    char *parent,*label;
    size_t label_bytes;
    uint64_t reserved;
    WorkbookVersionOwner owner;
    WorkbookVersionPurpose purpose;
    WorkbookVersionCoreSet *base,*seed;
    WorkbookVersionReceipt *saved;
    WorkbookArchiveCandidate *loaded;
    WorkbookVersionPreview *preview;
    WorkbookVersionKey preview_root;
    bool preview_requested,expects_target;
    SourceFileIdentity expected_target;
    WorkerNativeFileControl control; /* Borrowed only while execute runs. */
    WorkerNativeRetirement retirement;
};
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static void destroy(WorkbookNativeIO *c) {
    workbook_version_preview_free(c->preview); workbook_version_receipt_free(c->saved); workbook_archive_candidate_free(c->loaded);
    workbook_version_core_set_free(c->base); workbook_version_core_set_free(c->seed);
    free(c->label); free(c->parent); free(c);
}
static void retire(WorkbookNativeIO *c) { if (!worker_retire_native(c->worker,&c->retirement)) destroy(c); }
static bool cancelled(WorkbookNativeIO *c,WorkerSavePreparation phase,size_t root,uint64_t bytes) {
    c->control.prepare(c->control.context,phase,root,bytes);
    return c->control.cancelled(c->control.context);
}
#ifdef DUXCEL_FEATURE_ARCHIVE_WORKER
extern void dx_test_archive_capture_checkpoint(QueryWorker *worker,JobKind kind,WorkbookSaveProgress progress);
extern void dx_test_archive_publish_checkpoint(QueryWorker *worker,WorkbookSaveProgress progress);
#endif
static bool capture_update(void *context,WorkbookSaveProgress progress) {
    (void)progress; WorkbookNativeIO *c=context;
    c->control.prepare(c->control.context,WORKER_SAVE_SNAPSHOT,0,0);
#ifdef DUXCEL_FEATURE_ARCHIVE_WORKER
    dx_test_archive_capture_checkpoint(c->worker,c->kind,progress);
#endif
    return !c->control.cancelled(c->control.context);
}
static bool capture_cancel(void *context,uint64_t bytes) { return cancelled(context,WORKER_SAVE_SNAPSHOT,0,bytes); }
static bool compare_cancel(void *context,WorkbookCoreComparePhase phase,size_t table) {
    (void)phase; (void)table; return cancelled(context,WORKER_SAVE_COMPARE,0,0);
}
static bool inspect_cancel(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) {
    (void)phase; return cancelled(context,WORKER_SAVE_INSPECT,root,bytes);
}
static bool stage_cancel(void *context,WorkbookVersionStagePhase phase,size_t root,uint64_t bytes) {
    (void)phase; return cancelled(context,WORKER_SAVE_ROOTS,root,bytes);
}
static bool prepare_cancel(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) {
    (void)phase; return cancelled(context,WORKER_SAVE_RECEIPT,root,bytes);
}
static bool publish_update(void *context,WorkbookSaveProgress progress) {
    WorkbookNativeIO *c=context;
    bool proceed=c->control.publication.update(c->control.publication.context,progress);
#ifdef DUXCEL_FEATURE_ARCHIVE_WORKER
    dx_test_archive_publish_checkpoint(c->worker,progress);
#endif
    return proceed;
}
static bool load_cancel(void *context,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes) {
    (void)phase; return cancelled(context,WORKER_SAVE_INSPECT,root,bytes);
}
#ifdef DUXCEL_FEATURE_PREVIEW_WORKER
extern void dx_test_preview_checkpoint(QueryWorker *worker,WorkbookVersionPreviewPhase phase);
#endif
static bool preview_cancel(void *context,WorkbookVersionPreviewPhase phase,uint64_t bytes) {
    WorkbookNativeIO *c=context;
#ifdef DUXCEL_FEATURE_PREVIEW_WORKER
    dx_test_preview_checkpoint(c->worker,phase);
#endif
    (void)phase; return cancelled(c,WORKER_SAVE_INSPECT,0,bytes);
}
static WorkbookSaveResult execute(WorkbookNativeIO *c,JobKind kind,const Workbook *snapshot,const char *path,
    WorkerNativeFileControl control,Workbook **loaded,char **error) {
    c->control=control;
    if (kind!=c->kind || (kind!=JOB_SAVE && kind!=JOB_LOAD) || !path || !*path) { fail(error,"The native IO context does not match its file job."); return WORKBOOK_SAVE_FAILED; }
    if (control.cancelled(control.context)) { fail(error,"Native file IO was cancelled before preparation."); return WORKBOOK_SAVE_CANCELLED; }
    if (c->preview_requested) {
        WorkbookVersionPreviewResult result=workbook_version_preview_prepare(c->base,c->preview_root,c->owner,
            (WorkbookVersionPreviewControl){preview_cancel,c},&c->preview,error);
        if (result!=VERSION_PREVIEW_PREPARED) return result==VERSION_PREVIEW_CANCELLED ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
        *loaded=workbook_version_preview_take_book(c->preview); return WORKBOOK_SAVE_COMPLETE;
    }
    if (!dx_make_directories(c->parent)) { fail(error,"Cannot prepare native version working storage."); return WORKBOOK_SAVE_FAILED; }
    WorkbookCorePayloadControl capture={.save={capture_update,c},.cancelled=capture_cancel,.context=c};
    if (kind==JOB_SAVE) {
        WorkbookVersionSaveRequest request={.head=snapshot,.base=c->base,.owner=c->owner,.purpose=c->purpose,.path=path,.parent=c->parent,.reserved_bytes=c->reserved};
        WorkbookVersionSaveControl controls={.capture=capture,.compare={compare_cancel,c},.archive={
            .core={publish_update,c},.inspect={inspect_cancel,c},.stage={stage_cancel,c},.prepare={prepare_cancel,c},.expected_target=c->expects_target ? &c->expected_target : NULL,.expected_index=c->expects_target ? workbook_version_core_set_index(c->base) : NULL}};
        return c->label ? workbook_version_tag_save(request,(WorkbookVersionText){c->label,c->label_bytes},controls,&c->saved,error) : workbook_version_save(request,controls,&c->saved,error);
    }
    WorkbookArchiveReadResult read=workbook_archive_read(path,c->parent,c->reserved,(WorkbookArchiveReadControl){load_cancel,c},&c->loaded,error);
    if (read!=ARCHIVE_READ_READY) return read==ARCHIVE_READ_CANCELLED ? WORKBOOK_SAVE_CANCELLED : WORKBOOK_SAVE_FAILED;
    if (workbook_archive_file_version(c->loaded)<=15) {
        WorkbookSaveResult seeded=workbook_version_seed_legacy(c->loaded,c->parent,c->reserved,(WorkbookVersionSeedControl){.capture=capture,.admit={prepare_cancel,c}},&c->seed,error);
        if (seeded!=WORKBOOK_SAVE_COMPLETE) return seeded;
    }
    *loaded=workbook_archive_take_book(c->loaded); return WORKBOOK_SAVE_COMPLETE;
}
static WorkbookNativeIO *context(const QueryReply *r) {
    return r && r->native_file.context && r->native_file.execute==execute && r->native_file.dispose==retire ? r->native_file.context : NULL;
}
bool worker_archive_is_reply(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c && !c->preview_requested; }
bool worker_archive_is_preview_reply(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c && c->preview_requested; }
const WorkbookVersionPreview *worker_archive_preview(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c && c->preview_requested ? c->preview : NULL; }
WorkbookVersionPreview *worker_archive_take_preview(QueryReply *r) {
    WorkbookNativeIO *c=context(r); if (!c || !c->preview_requested) return NULL;
    WorkbookVersionPreview *preview=c->preview; c->preview=NULL; return preview;
}
bool worker_archive_is_tag_reply(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c && c->label; }
const WorkbookVersionPlan *worker_archive_plan(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c ? workbook_version_receipt_plan(c->saved) : NULL; }
const WorkbookVersionTagPlan *worker_archive_tag_plan(const QueryReply *r) { WorkbookNativeIO *c=context(r); return c ? workbook_version_receipt_tag(c->saved) : NULL; }
const WorkbookVersionCoreSet *worker_archive_versions(const QueryReply *r) {
    WorkbookNativeIO *c=context(r); if (!c) return NULL;
    if (c->preview_requested) return workbook_version_preview_versions(c->preview);
    return c->saved ? workbook_archive_receipt_versions(workbook_version_receipt_archive(c->saved)) : c->seed ? c->seed : workbook_archive_versions(c->loaded);
}
const SourceFileIdentity *worker_archive_identity(const QueryReply *r) {
    WorkbookNativeIO *c=context(r); if (!c) return NULL;
    return c->saved ? workbook_archive_receipt_identity(workbook_version_receipt_archive(c->saved)) : workbook_archive_identity(c->loaded);
}
WorkbookVersionCoreSet *worker_archive_take_versions(QueryReply *r) {
    WorkbookNativeIO *c=context(r); if (!c) return NULL;
    if (c->preview_requested) return NULL;
    if (c->saved) return workbook_version_receipt_take_versions(c->saved);
    if (c->seed) { WorkbookVersionCoreSet *out=c->seed; c->seed=NULL; return out; }
    return workbook_archive_take_versions(c->loaded);
}
static bool attach(QueryWorker *w,QueryJob *job,const WorkbookVersionCoreSet *base,WorkbookVersionOwner owner,
    WorkbookVersionPurpose purpose,const char *parent,uint64_t reserved,bool preview,char **error) {
    if (!w || !w->started || !job || !job->path || !*job->path || (!preview && (!parent || !*parent || strlen(parent)>4096)) ||
        job->native_file.context || job->native_file.execute || job->native_file.dispose || (job->kind!=JOB_SAVE && job->kind!=JOB_LOAD)) return fail(error,"Choose an owned native file job and working-store location.");
    if (job->kind==JOB_SAVE && (!job->workbook || !owner.session || owner.document_high!=job->workbook->id.high || owner.document_low!=job->workbook->id.low)) return fail(error,"The native Save owner does not match its captured head.");
    WorkbookNativeIO *c=dx_alloc(sizeof(*c)); c->worker=w; c->kind=job->kind; c->parent=parent ? dx_strdup(parent) : NULL; c->reserved=reserved; c->preview_requested=preview; c->owner=owner; c->purpose=purpose;
    c->base=workbook_version_core_set_retain(base);
    if (base && !c->base) { destroy(c); return fail(error,"The native working-store reference limit was reached."); }
    c->retirement=(WorkerNativeRetirement){.context=c,.dispose=destroy};
    job->native_file=(WorkerNativeFileTask){c,execute,retire}; return true;
}
bool worker_archive_save_job(QueryWorker *w,QueryJob *job,const WorkbookVersionCoreSet *base,WorkbookVersionOwner owner,
    WorkbookVersionPurpose purpose,const char *parent,uint64_t reserved,char **error) {
    return (job && job->kind==JOB_SAVE) ? attach(w,job,base,owner,purpose,parent,reserved,false,error) : fail(error,"Capture a Save job before attaching its native context.");
}
bool worker_archive_tag_job(QueryWorker *w,QueryJob *job,const WorkbookVersionCoreSet *base,WorkbookVersionOwner owner,
    WorkbookVersionText label,const char *parent,uint64_t reserved,char **error) {
    if (!job || job->kind!=JOB_SAVE || !job->workbook || owner.mode!=VERSION_WORKING_HEAD) return fail(error,"Capture a working-head Save job before creating a named version.");
    WorkbookVersionIndex empty={.document_high=job->workbook->id.high,.document_low=job->workbook->id.low};
    WorkbookVersionStatus status=workbook_version_tag_label_check(base ? workbook_version_core_set_index(base) : &empty,label);
    if (status) return fail(error,workbook_version_message(status));
    char *owned=dx_alloc(label.bytes+1); memcpy(owned,label.data,label.bytes); owned[label.bytes]=0;
    if (!attach(w,job,base,owner,VERSION_USER_SAVE,parent,reserved,false,error)) { free(owned); return false; }
    WorkbookNativeIO *c=job->native_file.context; c->label=owned; c->label_bytes=label.bytes; return true;
}
bool worker_archive_expect_target(QueryJob *job,const SourceFileIdentity *expected,char **error) {
    if (!job || job->kind!=JOB_SAVE || !expected || job->native_file.execute!=execute || job->native_file.dispose!=retire || !job->native_file.context)
        return fail(error,"Bind an observed target identity to an owned native Save/Tag job.");
    WorkbookNativeIO *c=job->native_file.context;
    if (c->preview_requested || c->kind!=JOB_SAVE) return fail(error,"Historical reads do not publish a file target.");
    c->expected_target=*expected; c->expects_target=true; return true;
}
bool worker_archive_load_job(QueryWorker *w,QueryJob *job,const char *parent,uint64_t reserved,char **error) {
    return (job && job->kind==JOB_LOAD) ? attach(w,job,NULL,(WorkbookVersionOwner){0},VERSION_STATE_EXPORT,parent,reserved,false,error) : fail(error,"Choose a Load job before attaching its native context.");
}
bool worker_archive_preview_job(QueryWorker *w,QueryJob *job,const WorkbookVersionCoreSet *store,WorkbookVersionKey root,
    WorkbookVersionOwner reader,char **error) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(store);
    if (!job || job->kind!=JOB_LOAD || job->workbook || !index || workbook_version_index_check(index)!=VERSION_OK || !reader.session || reader.mode!=VERSION_HISTORICAL_VIEW ||
        reader.document_high!=index->document_high || reader.document_low!=index->document_low) return fail(error,"Choose a protected root and matching historical Load owner without a working-model input.");
    size_t at=0; while (at<index->root_count && memcmp(index->roots[at].key.bytes,root.bytes,DX_VERSION_KEY_BYTES)) at++;
    if (at==index->root_count) return fail(error,"The requested preview root is not retained.");
    if (!attach(w,job,store,reader,VERSION_STATE_EXPORT,NULL,0,true,error)) return false;
    WorkbookNativeIO *c=job->native_file.context; c->preview_root=root; return true;
}
void worker_archive_release_versions(QueryWorker *w,WorkbookVersionCoreSet **owned) {
    if (!owned || !*owned) return;
    WorkbookNativeIO *c=dx_alloc(sizeof(*c)); c->worker=w; c->base=*owned;
    c->retirement=(WorkerNativeRetirement){.context=c,.dispose=destroy}; *owned=NULL; retire(c);
}
