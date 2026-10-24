#ifndef DUXCEL_SAVE_NATIVE_H
#define DUXCEL_SAVE_NATIVE_H
#include "save.h"
typedef struct WorkbookArchiveSaveContext WorkbookArchiveSaveContext;
/* Internal native transaction boundary. Typed borrowed context belongs to the
   versioned-file adapter, not App or a plugin. Hooks run synchronously under
   the core writer's transaction and may not commit/rollback/close it. COMPLETE
   here means prepared, never user-visible Save publication. inspect returns a
   validated v1-v15 core-layout version (0 for new); stage prepares all owned
   receipts before the core writer's cancellable PUBLISH and final COMMIT. */
typedef struct {
    WorkbookArchiveSaveContext *context;
    WorkbookSaveResult (*inspect)(WorkbookArchiveSaveContext *context,duckdb_connection connection,
        const char *target,bool existing,int *core_version,char **error);
    WorkbookSaveResult (*stage)(WorkbookArchiveSaveContext *context,duckdb_connection connection,char **error);
} WorkbookNativeSave;
WorkbookSaveResult workbook_save_native(const Workbook *book,const char *path,WorkbookSaveControl control,
    WorkbookNativeSave adapter,char **error);
/* Read an independent native core on a borrowed selected transaction, exposing
   actual v1-v16 family version. V16 requires its exact v15 head layout. Caller
   separately admits the retained index/core closure and doc association. No
   transaction/file ownership or live-model adoption occurs here. */
bool workbook_read_native_core(duckdb_connection connection,const char *path,Workbook *out,unsigned *version,char **error);
/* Exact v16 variant used by the staged read-only archive adapter. */
bool workbook_read_versioned_core(duckdb_connection connection,const char *path,Workbook *out,char **error);
#endif
