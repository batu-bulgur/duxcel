#include "versionpreview.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool same_owner(WorkbookVersionOwner a,WorkbookVersionOwner b) {
    return a.document_high==b.document_high && a.document_low==b.document_low && a.session==b.session && a.mode==b.mode;
}
static bool document(const Workbook *book,WorkbookVersionOwner owner) { return book && book->count && workbook_identity_valid(book) && book->id.high==owner.document_high && book->id.low==owner.document_low; }
static bool context_valid(AppVersionPreviewState *state,AppPreviewContext context) { return state && context.book && context.versions && context.worker && context.book!=&state->working && context.versions!=&state->working_versions; }
static bool displayed_owner(const AppVersionPreviewState *state,AppPreviewContext context) {
    const WorkbookVersionOwner *reader=workbook_version_preview_owner(state->preview);
    return reader && same_owner(context.versions->owner,*reader) && document(context.book,*reader) && context.versions->store==workbook_version_preview_versions(state->preview) && state->working_versions.store==context.versions->store;
}
static Workbook *working_book(AppVersionPreviewState *state,AppPreviewContext context) { return state->active ? &state->working : context.book; }
static AppVersionState *working_versions(AppVersionPreviewState *state,AppPreviewContext context) { return state->active ? &state->working_versions : context.versions; }
const Workbook *app_preview_working(const AppVersionPreviewState *state) { return state && state->active ? &state->working : NULL; }
const AppVersionState *app_preview_working_versions(const AppVersionPreviewState *state) { return state && state->active ? &state->working_versions : NULL; }
void app_preview_cancel(AppVersionPreviewState *state) { if (state) state->pending=(AppPreviewTicket){0}; }
bool app_preview_job(AppVersionPreviewState *state,AppPreviewContext context,QueryJob *job,WorkbookVersionKey root,char **error) {
    if (!context_valid(state,context) || !job || job->kind!=JOB_LOAD || !job->id || job->workbook || (state->active && !displayed_owner(state,context)))
        return fail(error,"Choose the current document owner and an owned historical Load request.");
    Workbook *book=working_book(state,context); AppVersionState *versions=working_versions(state,context);
    if (!versions->owner.session || versions->owner.mode!=VERSION_WORKING_HEAD || !document(book,versions->owner) || !versions->store)
        return fail(error,"The Working Head needs a matching retained store before historical inspection.");
    WorkbookVersionOwner reader=versions->owner; reader.mode=VERSION_HISTORICAL_VIEW; bool fresh=false;
    const WorkbookVersionOwner *old=workbook_version_preview_owner(state->preview);
    for (unsigned i=0;i<8 && !fresh;i++) {
        if (!dx_random_bytes(&reader.session,sizeof(reader.session))) break;
        fresh=reader.session && reader.session!=versions->owner.session && (!old || reader.session!=old->session);
    }
    if (!fresh) return fail(error,"Cannot prepare a distinct historical reader owner.");
    if (!worker_archive_preview_job(context.worker,job,versions->store,root,reader,error)) return false;
    state->pending=(AppPreviewTicket){job->id,versions->store,versions->owner,reader,root}; return true;
}
AppPreviewOutcome app_preview_adopt(AppVersionPreviewState *state,AppPreviewContext context,QueryReply *reply,char **error) {
    if (!context_valid(state,context) || !reply) { fail(error,"The historical adoption context is unavailable."); return APP_PREVIEW_INVALID; }
    if (!state->pending.request || reply->id!=state->pending.request) return APP_PREVIEW_STALE;
    AppPreviewTicket ticket=state->pending;
    Workbook *working=working_book(state,context); AppVersionState *versions=working_versions(state,context);
    if ((state->active && !displayed_owner(state,context)) || !same_owner(versions->owner,ticket.writer) || !document(working,ticket.writer) || versions->store!=ticket.store) {
        app_preview_cancel(state); return APP_PREVIEW_STALE;
    }
    const WorkbookVersionPreview *candidate=worker_archive_preview(reply);
    const WorkbookVersionOwner *reader=workbook_version_preview_owner(candidate); const WorkbookVersionKey *root=workbook_version_preview_root(candidate);
    if (reply->error || reply->kind!=JOB_LOAD || !worker_archive_is_preview_reply(reply) || reply->save_result!=WORKBOOK_SAVE_COMPLETE || !reply->workbook || !reader || !root ||
        !same_owner(*reader,ticket.reader) || memcmp(root->bytes,ticket.root.bytes,DX_VERSION_KEY_BYTES) ||
        workbook_version_preview_versions(candidate)!=ticket.store || !document(reply->workbook,ticket.reader) || reply->workbook->path || reply->workbook->dirty ||
        reply->workbook==context.book || reply->workbook==&state->working) {
        app_preview_cancel(state); fail(error,reply->error ? reply->error : "The historical reply does not match its complete reader ticket."); return APP_PREVIEW_INVALID;
    }
    WorkbookVersionCoreSet *retained=workbook_version_core_set_retain(workbook_version_preview_versions(candidate));
    if (!retained) { app_preview_cancel(state); fail(error,"Cannot retain the displayed historical store."); return APP_PREVIEW_INVALID; }
    AppVersionState historical={.owner=*reader,.store=retained};
    WorkbookVersionPreview *owned=worker_archive_take_preview(reply);
    if (!state->active) {
        state->working=*context.book; workbook_init(context.book);
        state->working_versions=*context.versions; *context.versions=(AppVersionState){0};
    } else {
        workbook_free(context.book); app_versions_reset(context.versions,context.worker);
        /* The suspended Working Head still owns this same store, so releasing
           the previous reader here cannot retire files on the UI thread. */
        workbook_version_preview_free(state->preview);
    }
    *context.book=*reply->workbook; workbook_init(reply->workbook); *context.versions=historical;
    state->preview=owned; state->active=true; app_preview_cancel(state); return APP_PREVIEW_APPLIED;
}
AppPreviewOutcome app_preview_return(AppVersionPreviewState *state,AppPreviewContext context,char **error) {
    if (!context_valid(state,context)) { fail(error,"The Working Head return context is unavailable."); return APP_PREVIEW_INVALID; }
    app_preview_cancel(state);
    if (!state->active) return APP_PREVIEW_EMPTY;
    if (!displayed_owner(state,context)) return APP_PREVIEW_STALE;
    workbook_free(context.book); app_versions_reset(context.versions,context.worker);
    *context.book=state->working; workbook_init(&state->working);
    *context.versions=state->working_versions; state->working_versions=(AppVersionState){0};
    workbook_version_preview_free(state->preview); state->preview=NULL; state->active=false;
    return APP_PREVIEW_APPLIED;
}
