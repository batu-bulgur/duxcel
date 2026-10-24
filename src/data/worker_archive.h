#ifndef DUXCEL_WORKER_ARCHIVE_H
#define DUXCEL_WORKER_ARCHIVE_H
#include "worker.h"
#include "../workbook/version_save.h"
#include "../workbook/version_preview.h"
/* UI-side preparation is memory/refcount only. Job already owns its path and
   Save snapshot; failure leaves it unchanged/caller-owned. The worker object
   outlives attached jobs/replies, including their deferred disposal. Native
   execute owns folder creation and all persistence; no App pointer is retained. */
bool worker_archive_save_job(QueryWorker *worker,QueryJob *job,const WorkbookVersionCoreSet *base,
    WorkbookVersionOwner owner,WorkbookVersionPurpose purpose,const char *parent,uint64_t reserved_bytes,char **error);
/* Current-head Tag uses the Save lane and owns a copy of the validated label.
   It is not a user Save that silently creates an automatic tag. */
/* Fresh current-state-only export snapshot; no base roots/tags, ordinary Save
   adoption or recovery authority. Origin document targets are protected. */
bool worker_archive_export_job(QueryWorker *worker,QueryJob *job,const char *parent,uint64_t reserved_bytes,char **error);
bool worker_archive_is_export_reply(const QueryReply *reply);
bool worker_archive_tag_job(QueryWorker *worker,QueryJob *job,const WorkbookVersionCoreSet *base,
    WorkbookVersionOwner owner,WorkbookVersionText label,const char *parent,uint64_t reserved_bytes,char **error);
/* Bind a copied same-destination expectation to an already attached Save/Tag.
   No IO; the transaction checks it before altering an artifact. Explicit Save
   As replacements deliberately omit this current-file expectation. */
bool worker_archive_expect_target(QueryJob *job,const SourceFileIdentity *expected,char **error);
bool worker_archive_load_job(QueryWorker *worker,QueryJob *job,const char *parent,uint64_t reserved_bytes,char **error);
/* A preview Load carries a non-file diagnostic path, no input Workbook and an
   explicit historical reader/root. It retains admitted bytes without creating
   working folders or treating the named artifact as the authority. */
bool worker_archive_preview_job(QueryWorker *worker,QueryJob *job,const WorkbookVersionCoreSet *store,
    WorkbookVersionKey root,WorkbookVersionOwner reader,char **error);
bool worker_archive_is_preview_reply(const QueryReply *reply);
/* Reply owns the decoded workbook; this candidate retains reader/root/store
   metadata independently. These getters never identify an ordinary Open. */
const WorkbookVersionPreview *worker_archive_preview(const QueryReply *reply);
WorkbookVersionPreview *worker_archive_take_preview(QueryReply *reply);
/* Ordinary Save/Open/Tag only: preview and export replies cannot enter normal adoption. */
bool worker_archive_is_reply(const QueryReply *reply);
/* True even when Tag preparation failed/cancelled and has no receipt yet. */
bool worker_archive_is_tag_reply(const QueryReply *reply);
const WorkbookVersionPlan *worker_archive_plan(const QueryReply *reply);
const WorkbookVersionTagPlan *worker_archive_tag_plan(const QueryReply *reply);
const WorkbookVersionCoreSet *worker_archive_versions(const QueryReply *reply);
const SourceFileIdentity *worker_archive_identity(const QueryReply *reply);
WorkbookVersionCoreSet *worker_archive_take_versions(QueryReply *reply);
/* Transfer the App's owning store into background retirement. A stopped/null
   worker falls back to synchronous disposal for shutdown; never use it after
   the worker object's lifetime. No disk IO occurs on a live worker's caller. */
void worker_archive_release_versions(QueryWorker *worker,WorkbookVersionCoreSet **owned);
#endif
