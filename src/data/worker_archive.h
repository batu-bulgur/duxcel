#ifndef DUXCEL_WORKER_ARCHIVE_H
#define DUXCEL_WORKER_ARCHIVE_H
#include "worker.h"
#include "../workbook/version_save.h"
/* UI-side preparation is memory/refcount only. Job already owns its path and
   Save snapshot; failure leaves it unchanged/caller-owned. The worker object
   outlives attached jobs/replies, including their deferred disposal. Native
   execute owns folder creation and all persistence; no App pointer is retained. */
bool worker_archive_save_job(QueryWorker *worker,QueryJob *job,const WorkbookVersionCoreSet *base,
    WorkbookVersionOwner owner,WorkbookVersionPurpose purpose,const char *parent,uint64_t reserved_bytes,char **error);
bool worker_archive_load_job(QueryWorker *worker,QueryJob *job,const char *parent,uint64_t reserved_bytes,char **error);
bool worker_archive_is_reply(const QueryReply *reply);
const WorkbookVersionPlan *worker_archive_plan(const QueryReply *reply);
const WorkbookVersionCoreSet *worker_archive_versions(const QueryReply *reply);
const SourceFileIdentity *worker_archive_identity(const QueryReply *reply);
WorkbookVersionCoreSet *worker_archive_take_versions(QueryReply *reply);
/* Transfer the App's owning store into background retirement. A stopped/null
   worker falls back to synchronous disposal for shutdown; never use it after
   the worker object's lifetime. No disk IO occurs on a live worker's caller. */
void worker_archive_release_versions(QueryWorker *worker,WorkbookVersionCoreSet **owned);
#endif
