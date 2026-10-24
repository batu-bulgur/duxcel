#include "versionstate.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool owner(const Workbook *book,WorkbookVersionOwner *out,char **error) {
    if (!book || !book->count || !workbook_identity_valid(book)) return fail(error,"The workbook needs valid durable identities before native IO.");
    uint64_t session=0;
    for (unsigned i=0;i<8 && !session;i++) if (!dx_random_bytes(&session,sizeof(session))) return fail(error,"Cannot prepare a fresh workbook write owner.");
    if (!session) return fail(error,"Cannot prepare a nonzero workbook write owner.");
    *out=(WorkbookVersionOwner){book->id.high,book->id.low,session,VERSION_WORKING_HEAD}; return true;
}
static bool document(WorkbookVersionOwner owner,const Workbook *book) { return owner.document_high==book->id.high && owner.document_low==book->id.low; }
static bool store_document(const WorkbookVersionCoreSet *store,const Workbook *book) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(store);
    return index && index->document_high==book->id.high && index->document_low==book->id.low;
}
static void identity(AppVersionState *state,const QueryReply *reply) {
    const SourceFileIdentity *observed=worker_archive_identity(reply); state->identity_valid=observed!=NULL;
    state->identity=observed ? *observed : (SourceFileIdentity){0};
}
void app_versions_reset(AppVersionState *state,QueryWorker *worker) {
    if (!state) return;
    worker_archive_release_versions(worker,&state->store); *state=(AppVersionState){0};
}
bool app_versions_owner(AppVersionState *state,QueryWorker *worker,const Workbook *book,char **error) {
    if (!state || !book) return fail(error,"The native version working state is unavailable.");
    if (!state->owner.session || !document(state->owner,book)) {
        WorkbookVersionOwner next;
        if (!owner(book,&next,error)) return false;
        if (state->store && !store_document(state->store,book)) app_versions_reset(state,worker);
        state->owner=next;
    }
    return !state->store || store_document(state->store,book) || fail(error,"The retained native store belongs to another document.");
}
bool app_versions_recovered(AppVersionState *state,QueryWorker *worker,const Workbook *book,const WorkbookVersionCoreSet *store,char **error) {
    WorkbookVersionOwner next;
    if (!state || !book || (store && !store_document(store,book))) return fail(error,"The recovered store does not match its working head.");
    if (!owner(book,&next,error)) return false;
    WorkbookVersionCoreSet *retained=workbook_version_core_set_retain(store);
    if (store && !retained) return fail(error,"Cannot retain the recovered Saved store.");
    app_versions_reset(state,worker); state->store=retained; state->owner=next; return true;
}
static bool current_target(AppVersionState *state,QueryJob *job,char **error) {
    const char *current=job->workbook ? job->workbook->path : NULL;
    if (!current || !*current || !job->path || strcmp(current,job->path)) return true;
    if (!state->identity_valid) return fail(error,"The current saved file cannot be verified. Use Save As to choose an explicit destination.");
    return worker_archive_expect_target(job,&state->identity,error);
}
static bool prepare_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,const char *parent,bool replace,char **error) {
    if (!state || !job || !parent) return fail(error,"The native version working state is unavailable.");
    if (job->kind==JOB_LOAD && !replace) return worker_archive_load_job(worker,job,parent,DX_APP_VERSION_BYTES,error);
    if (job->kind!=JOB_SAVE || !job->workbook) return fail(error,"Capture an owned workbook before native Save.");
    if (!app_versions_owner(state,worker,job->workbook,error)) return false;
    if (!worker_archive_save_job(worker,job,state->store,state->owner,VERSION_USER_SAVE,parent,DX_APP_VERSION_BYTES,error)) return false;
    return replace || current_target(state,job,error);
}
bool app_versions_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,const char *parent,char **error) {
    return prepare_job(state,worker,job,parent,false,error);
}
bool app_versions_replace_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,const char *parent,char **error) {
    return prepare_job(state,worker,job,parent,true,error);
}
bool app_versions_tag_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,WorkbookVersionText label,const char *parent,char **error) {
    if (!job || job->kind!=JOB_SAVE || !job->workbook) return fail(error,"Capture a matching working head before creating a named version.");
    if (!app_versions_owner(state,worker,job->workbook,error)) return false;
    return worker_archive_tag_job(worker,job,state->store,state->owner,label,parent,DX_APP_VERSION_BYTES,error) && current_target(state,job,error);
}
bool app_versions_loaded(AppVersionState *state,QueryWorker *worker,QueryReply *reply,char **error) {
    if (!state || !reply || reply->kind!=JOB_LOAD || reply->error || !reply->workbook || !worker_archive_is_reply(reply) || !store_document(worker_archive_versions(reply),reply->workbook)) return fail(error,"The loaded workbook has no complete matching native store.");
    WorkbookVersionOwner next;
    if (!owner(reply->workbook,&next,error)) return false;
    WorkbookVersionCoreSet *store=worker_archive_take_versions(reply);
    worker_archive_release_versions(worker,&state->store); state->store=store; state->owner=next; identity(state,reply); return true;
}
AppVersionOutcome app_versions_saved(AppVersionState *state,QueryWorker *worker,const Workbook *current,QueryReply *reply,char **error) {
    if (!state || !current || !reply || reply->kind!=JOB_SAVE || reply->error || reply->save_result!=WORKBOOK_SAVE_COMPLETE || !worker_archive_is_reply(reply)) {
        fail(error,"The native Save has no complete prepared receipt."); return APP_VERSION_INVALID;
    }
    const WorkbookVersionTagPlan *tag=worker_archive_tag_plan(reply);
    const WorkbookVersionPlan *plan=tag ? &tag->save : worker_archive_plan(reply);
    WorkbookVersionIndex empty={.document_high=current->id.high,.document_low=current->id.low};
    const WorkbookVersionIndex *before=state->store ? workbook_version_core_set_index(state->store) : &empty;
    if (!plan || !document(state->owner,current) || !(tag ? workbook_version_tag_can_publish(before,state->owner,tag) : workbook_version_can_publish(before,state->owner,plan))) return APP_VERSION_STALE;
    const WorkbookVersionIndex *after=workbook_version_core_set_index(worker_archive_versions(reply));
    uint64_t generation=tag ? tag->next_generation : plan->next_generation;
    uint64_t saved_generation=tag ? tag->next_generation : plan->next_saved_generation;
    if (!after || !store_document(worker_archive_versions(reply),current) || after->generation!=generation || after->saved_generation!=saved_generation ||
        after->root_count!=before->root_count+plan->adds_root || after->tag_count!=before->tag_count+(tag!=NULL) ||
        after->saved.present!=plan->after_saved.present || memcmp(after->saved.key.bytes,plan->after_saved.key.bytes,DX_VERSION_KEY_BYTES)) {
        fail(error,"The prepared native Saved index does not match its publication plan."); return APP_VERSION_INVALID;
    }
    if (tag) {
        const WorkbookVersionTag *created=after->tags+before->tag_count;
        if (created->created!=generation || created->label.bytes!=tag->label_bytes || memcmp(created->label.data,tag->label,tag->label_bytes) ||
            memcmp(created->root.bytes,plan->root.key.bytes,DX_VERSION_KEY_BYTES)) {
            fail(error,"The prepared named version does not match its complete publication plan."); return APP_VERSION_INVALID;
        }
    }
    WorkbookVersionCoreSet *next=worker_archive_take_versions(reply);
    worker_archive_release_versions(worker,&state->store); state->store=next; identity(state,reply); return APP_VERSION_APPLIED;
}
