#ifndef DUXCEL_VERSION_RESTORE_H
#define DUXCEL_VERSION_RESTORE_H
#include "version_read.h"
#include "core_payload.h"
typedef struct WorkbookVersionCoreSet WorkbookVersionCoreSet;
typedef enum { VERSION_RESTORE_INDEX,VERSION_RESTORE_COPY,VERSION_RESTORE_VERIFY,VERSION_RESTORE_ADMIT,VERSION_RESTORE_RECHECK } WorkbookVersionRestorePhase;
typedef struct {
    /* Worker callback: true cancels. No mutation, reentrant SQL/payload calls or
       borrowed pointer retention. root is an index ordinal, bytes is per phase. */
    bool (*cancelled)(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes);
    void *context;
} WorkbookVersionRestoreControl;
typedef enum { VERSION_RESTORE_READY,VERSION_RESTORE_FAILED,VERSION_RESTORE_CANCELLED } WorkbookVersionRestoreResult;
/* Complete core-profile closure for codec-v1. Borrows an exclusively owned
   explicit SQL transaction; owns copied metadata and a private leased leaf for
   every root before returning READY. Preflights chunk counts/aggregate bytes,
   NULL/size/orphan references; exact ordered spans, digest, inner v15 model and
   document association are admitted separately for each leaf. No external
   source query, connection ownership change, live-model adoption or user Save
   publication. Future Data Model/result assets are outside this core profile.
   out must be NULL; failure/cancel preserves it and cleans ordinary new leaves.
   Caller still owns transaction lifetime and validates the outer .dxls format. */
WorkbookVersionRestoreResult workbook_version_core_set_read(duckdb_connection connection,const char *parent,
    uint64_t reserved_bytes,WorkbookVersionRestoreControl control,WorkbookVersionCoreSet **out,char **error);
const WorkbookVersionIndex *workbook_version_core_set_index(const WorkbookVersionCoreSet *set);
const WorkbookCorePayload *workbook_version_core_set_leaf(const WorkbookVersionCoreSet *set,size_t position);
/* Acquire another immutable owning reference while an existing owner/borrow
   keeps set alive. Mutex-protected scalar lifetime only: no IO/model scan.
   NULL/overflow returns NULL without changing ownership. Each successful retain
   requires one free. Index/leaf getters remain borrowed. Last free disposes the
   private leaves on that caller's thread; App adapters must retire off frames. */
WorkbookVersionCoreSet *workbook_version_core_set_retain(const WorkbookVersionCoreSet *set);
void workbook_version_core_set_free(WorkbookVersionCoreSet *set);
#endif
