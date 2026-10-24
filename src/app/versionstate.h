#ifndef DUXCEL_APP_VERSIONSTATE_H
#define DUXCEL_APP_VERSIONSTATE_H
#include "../data/worker_archive.h"
/* Initial per-core-set disk admission ceiling, not an RSS reservation. Native
   IO can fail below it on low storage; protected roots are never auto-evicted. */
#define DX_APP_VERSION_BYTES (UINT64_C(8)*1024*1024*1024)
typedef struct {
    WorkbookVersionOwner owner;
    WorkbookVersionCoreSet *store;
    SourceFileIdentity identity;
    bool identity_valid;
} AppVersionState;
typedef enum { APP_VERSION_APPLIED,APP_VERSION_STALE,APP_VERSION_INVALID } AppVersionOutcome;
/* Native UI controller: scalar ownership/metadata only, never file/engine IO.
   Root retirement is queued to the live worker. The App retains model/readiness,
   path, dirty, history, focus and document-action ownership. */
void app_versions_reset(AppVersionState *state,QueryWorker *worker);
bool app_versions_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,const char *parent,char **error);
bool app_versions_tag_job(AppVersionState *state,QueryWorker *worker,QueryJob *job,WorkbookVersionText label,const char *parent,char **error);
/* Call after successful IO validation, before non-fallible model replacement.
   Failure preserves state/reply ownership. A new runtime owner is prepared
   before taking any store, even when durable doc IDs match the previous file. */
bool app_versions_loaded(AppVersionState *state,QueryWorker *worker,QueryReply *reply,char **error);
/* A recovered private head uses a fresh runtime write owner. Retains a complete
   supplied store (NULL for legacy checkpoints); no named-file identity is claimed. */
bool app_versions_recovered(AppVersionState *state,QueryWorker *worker,const Workbook *book,
    const WorkbookVersionCoreSet *store,char **error);
bool app_versions_owner(AppVersionState *state,QueryWorker *worker,const Workbook *book,char **error);
/* Publish a prepared saved store only under the original owner/index fence.
   R+1 data edits do not change this fence; App separately computes dirty. */
AppVersionOutcome app_versions_saved(AppVersionState *state,QueryWorker *worker,const Workbook *current,
    QueryReply *reply,char **error);
#endif
