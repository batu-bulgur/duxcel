#ifndef DUXCEL_APP_VERSIONPREVIEW_H
#define DUXCEL_APP_VERSIONPREVIEW_H
#include "versionstate.h"
typedef struct {
    uint64_t request;
    /* Borrowed token protected by the attached job/reply. The live Working
       Head must still own this exact immutable store at adoption. */
    const WorkbookVersionCoreSet *store;
    WorkbookVersionOwner writer,reader;
    WorkbookVersionKey root;
} AppPreviewTicket;
typedef struct {
    /* Owned only while active. Do not mutate these ownership fields directly. */
    Workbook working;
    AppVersionState working_versions;
    WorkbookVersionPreview *preview;
    AppPreviewTicket pending;
    bool active;
} AppVersionPreviewState;
typedef struct { Workbook *book; AppVersionState *versions; QueryWorker *worker; } AppPreviewContext;
typedef enum { APP_PREVIEW_APPLIED,APP_PREVIEW_EMPTY,APP_PREVIEW_STALE,APP_PREVIEW_INVALID } AppPreviewOutcome;
/* Scalar/refcount-only request preparation. Caller owns readiness, monotonic
   job ID/diagnostic path, submission and cancellation. A rejected new request
   preserves an existing ticket. No App/render/focus pointer is retained. */
bool app_preview_job(AppVersionPreviewState *state,AppPreviewContext context,QueryJob *job,WorkbookVersionKey root,char **error);
/* Data edits accepted while reading remain in the actual Working Head moved at
   adoption. Owner/store/request fences prevent stale publication. Model/history
   ownership moves, never copies or reconstructs Undo. App separately suspends
   renderer/pages/focus after APPLIED, before rendering the new model. */
AppPreviewOutcome app_preview_adopt(AppVersionPreviewState *state,AppPreviewContext context,QueryReply *reply,char **error);
/* Pure ticket invalidation; caller cancels the corresponding worker request.
   An already displayed preview/held Working Head is left intact. */
void app_preview_cancel(AppVersionPreviewState *state);
/* Nonfallible ownership return after owner validation. App must call before
   replacing/closing the document; STALE retains the protected Working Head and
   refuses to overwrite an unrelated live owner. Caller restores its UI surface.
   Cancel the worker separately if a request was pending. No implicit discard. */
AppPreviewOutcome app_preview_return(AppVersionPreviewState *state,AppPreviewContext context,char **error);
/* Deep snapshot of the displayed state with fresh durable identity and immediate
   origin provenance; caller supplies an empty Save job and owns its reply only.
   No preview/Working Head field changes and no filesystem work occurs here. */
bool app_preview_export_job(AppVersionPreviewState *state,AppPreviewContext context,QueryJob *job,const char *parent,char **error);
const Workbook *app_preview_working(const AppVersionPreviewState *state);
const AppVersionState *app_preview_working_versions(const AppVersionPreviewState *state);
#endif
