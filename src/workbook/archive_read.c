#include "archive_read.h"
#include "save_native.h"
#include "../platform/platform.h"
#include "../platform/directory.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookArchiveCandidate { Workbook *book; WorkbookVersionCoreSet *versions; SourceFileIdentity identity; unsigned version; };
typedef struct { WorkbookArchiveReadControl control; bool stopped; } Read;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Native archive admission failed."); } return false; }
static bool stopped(Read *r,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes,char **error) {
    if (!r->stopped && r->control.cancelled) r->stopped=r->control.cancelled(r->control.context,phase,root,bytes);
    if (r->stopped) fail(error,"Native archive admission was cancelled."); return r->stopped;
}
static bool restore_cancel(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) {
    static const WorkbookArchiveReadPhase phases[]={ARCHIVE_READ_INDEX,ARCHIVE_READ_COPY,ARCHIVE_READ_VERIFY,ARCHIVE_READ_ADMIT,ARCHIVE_READ_RECHECK};
    return stopped(context,phases[phase],root,bytes,NULL);
}
const Workbook *workbook_archive_book(const WorkbookArchiveCandidate *c) { return c ? c->book : NULL; }
const WorkbookVersionCoreSet *workbook_archive_versions(const WorkbookArchiveCandidate *c) { return c ? c->versions : NULL; }
unsigned workbook_archive_file_version(const WorkbookArchiveCandidate *c) { return c ? c->version : 0; }
const SourceFileIdentity *workbook_archive_identity(const WorkbookArchiveCandidate *c) { return c ? &c->identity : NULL; }
Workbook *workbook_archive_take_book(WorkbookArchiveCandidate *c) { if (!c) return NULL; Workbook *b=c->book; c->book=NULL; return b; }
WorkbookVersionCoreSet *workbook_archive_take_versions(WorkbookArchiveCandidate *c) { if (!c) return NULL; WorkbookVersionCoreSet *s=c->versions; c->versions=NULL; return s; }
void workbook_archive_candidate_free(WorkbookArchiveCandidate *c) {
    if (!c) return;
    if (c->book) { workbook_free(c->book); free(c->book); }
    workbook_version_core_set_free(c->versions); free(c);
}
static bool query(duckdb_connection c,const char *sql,duckdb_result *r,char **error) {
    return duckdb_query(c,sql,r)==DuckDBSuccess || fail(error,duckdb_result_error(r));
}
static void close_database(duckdb_database *db,duckdb_connection *c) { if (*c) duckdb_disconnect(c); if (*db) duckdb_close(db); }
WorkbookArchiveReadResult workbook_archive_read(const char *path,const char *parent,uint64_t reserved,
    WorkbookArchiveReadControl control,WorkbookArchiveCandidate **out,char **error) {
    if (!path || !*path || !parent || !*parent || !out || *out) { fail(error,"Choose an archive, private staging folder and empty candidate result."); return ARCHIVE_READ_FAILED; }
    Read read={.control=control}; WorkbookArchiveCandidate *candidate=dx_alloc(sizeof(*candidate));
    char *target=NULL,*temporary=NULL,*message=NULL; duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_config config=NULL; duckdb_result r={0};
    WorkbookArchiveReadResult result=ARCHIVE_READ_FAILED; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    if (stopped(&read,ARCHIVE_READ_HEADER,0,0,error)) goto done;
    target=dx_absolute_path(path); if (!target) { fail(error,"The native archive is missing or inaccessible."); goto done; }
    SourceDigestStatus observed=source_file_identity(target,&candidate->identity,detail);
    if (observed!=SOURCE_DIGEST_OK) { fail(error,detail); if (observed==SOURCE_DIGEST_UNSUPPORTED) result=ARCHIVE_READ_UNSUPPORTED; goto done; }
    if (dx_path_kind(parent)!=DX_PATH_DIRECTORY) { fail(error,"The archive staging folder is unavailable."); goto done; }
    temporary=dx_temp_directory();
    if (!temporary || duckdb_create_config(&config)!=DuckDBSuccess || duckdb_set_config(config,"access_mode","READ_ONLY")!=DuckDBSuccess ||
        duckdb_set_config(config,"memory_limit","256MB")!=DuckDBSuccess || duckdb_set_config(config,"threads","2")!=DuckDBSuccess || duckdb_set_config(config,"temp_directory",temporary)!=DuckDBSuccess) { fail(error,"Cannot configure bounded native archive reads."); goto done; }
    if (duckdb_open_ext(target,&db,config,&message)!=DuckDBSuccess) { fail(error,message); goto done; }
    if (duckdb_connect(db,&c)!=DuckDBSuccess) { fail(error,"Cannot connect to the native archive."); goto done; }
    if (!query(c,"BEGIN",&r,error)) goto done; duckdb_destroy_result(&r);
    /* Legacy headers vary, but every supported producer uses a base meta
       table. Prove this before evaluating any candidate header expression. */
    if (!query(c,"SELECT count(*) FROM information_schema.tables WHERE table_catalog=current_database() AND table_schema='_duxcel_workspace' AND table_name='meta' AND table_type='BASE TABLE'",&r,error)) goto done;
    if (duckdb_row_count(&r)!=1 || duckdb_value_uint64(&r,0,0)!=1) { fail(error,"The native archive header is not a known base table."); goto done; }
    duckdb_destroy_result(&r);
    if (!query(c,"SELECT version FROM _duxcel_workspace.meta LIMIT 2",&r,error)) goto done;
    int version=duckdb_value_int32(&r,0,0);
    if (duckdb_row_count(&r)!=1 || duckdb_value_is_null(&r,0,0) || version<1 || version>(int)DX_VERSIONED_WORKSPACE_VERSION || (version>12 && duckdb_column_type(&r,0)!=DUCKDB_TYPE_INTEGER)) { fail(error,"Unsupported or invalid native archive version."); goto done; }
    candidate->version=(unsigned)version; duckdb_destroy_result(&r);
    if (stopped(&read,ARCHIVE_READ_HEAD,0,0,error)) goto done;
    candidate->book=dx_alloc(sizeof(*candidate->book)); workbook_init(candidate->book);
    if (candidate->version<DX_VERSIONED_WORKSPACE_VERSION) {
        if (!query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND lower(schema_name)='_duxcel_versions'",&r,error)) goto done;
        if (duckdb_value_uint64(&r,0,0)) { fail(error,"Version metadata requires its native v16 discriminator; legacy input was kept."); goto done; }
        duckdb_destroy_result(&r); close_database(&db,&c);
        if (!workbook_load(candidate->book,target,error)) goto done;
    } else {
        char *wal=dx_printf("%s.wal",target); bool closed=dx_path_kind(wal)==DX_PATH_MISSING; free(wal);
        if (!closed) { fail(error,"A versioned archive requires closed bytes without a dependent WAL."); goto done; }
        if (!workbook_read_versioned_core(c,target,candidate->book,error)) goto done;
        WorkbookVersionRestoreResult restored=workbook_version_core_set_read(c,parent,reserved,(WorkbookVersionRestoreControl){restore_cancel,&read},&candidate->versions,error);
        if (restored!=VERSION_RESTORE_READY) goto done;
        const WorkbookVersionIndex *index=workbook_version_core_set_index(candidate->versions);
        if (index->document_high!=candidate->book->id.high || index->document_low!=candidate->book->id.low) { fail(error,"The version index belongs to another authored head."); goto done; }
    }
    close_database(&db,&c);
    SourceFileIdentity after={0};
    if (source_file_identity(target,&after,detail)!=SOURCE_DIGEST_OK || !source_file_identity_same(&candidate->identity,&after)) { fail(error,"The native archive changed during admission."); goto done; }
    if (stopped(&read,ARCHIVE_READ_RECHECK,0,candidate->identity.size,error)) goto done;
    result=ARCHIVE_READ_READY;
done:
    duckdb_destroy_result(&r); close_database(&db,&c); if (config) duckdb_destroy_config(&config); duckdb_free(message);
    dx_remove_temp_directory(temporary); free(temporary); free(target);
    if (result==ARCHIVE_READ_READY) { *out=candidate; return result; }
    workbook_archive_candidate_free(candidate); return read.stopped ? ARCHIVE_READ_CANCELLED : result;
}
