#include "version_restore.h"
#include "version_chunks.h"
#include "../platform/platform.h"
#include "../platform/directory.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookVersionCoreSet { WorkbookVersionMetadata *metadata; WorkbookCorePayload **leaves; size_t count; };
typedef struct {
    WorkbookVersionRestoreControl control;
    WorkbookVersionRestorePhase phase;
    size_t root; bool stopped;
    duckdb_prepared_statement statement;
    WorkbookVersionKey key;
    WorkbookChunks chunks;
} Restore;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Version core restoration failed."); } return false; }
static bool stopped(Restore *r,uint64_t bytes,char **error) {
    if (!r->stopped && r->control.cancelled) r->stopped=r->control.cancelled(r->control.context,r->phase,r->root,bytes);
    if (r->stopped) fail(error,"Version core restoration was cancelled."); return r->stopped;
}
static bool metadata_cancel(void *context) { return stopped(context,0,NULL); }
static bool leaf_cancel(void *context,WorkbookCoreRestorePhase phase,uint64_t bytes) {
    Restore *r=context;
    static const WorkbookVersionRestorePhase phases[]={VERSION_RESTORE_COPY,VERSION_RESTORE_VERIFY,VERSION_RESTORE_ADMIT,VERSION_RESTORE_RECHECK};
    r->phase=phases[phase]; return stopped(r,bytes,NULL);
}
const WorkbookVersionIndex *workbook_version_core_set_index(const WorkbookVersionCoreSet *s) { return s ? workbook_version_metadata_index(s->metadata) : NULL; }
const WorkbookCorePayload *workbook_version_core_set_leaf(const WorkbookVersionCoreSet *s,size_t position) { return s && position<s->count ? s->leaves[position] : NULL; }
void workbook_version_core_set_free(WorkbookVersionCoreSet *s) {
    if (!s) return;
    for (size_t i=0;i<s->count;i++) workbook_core_payload_free(s->leaves[i]);
    free(s->leaves); workbook_version_metadata_free(s->metadata); free(s);
}
static bool prepare(duckdb_connection c,const char *sql,duckdb_prepared_statement *statement,char **error) {
    return duckdb_prepare(c,sql,statement)==DuckDBSuccess || fail(error,duckdb_prepare_error(*statement));
}
static bool chunk_inventory(duckdb_connection c,const WorkbookVersionIndex *index,Restore *restore,char **error) {
    uint64_t expected=0,total=0;
    for (size_t i=0;i<index->root_count;i++) {
        WorkbookChunks chunks={0};
        if (workbook_chunks_begin(index->roots[i].bytes,index->roots[i].bytes,&chunks)!=VERSION_CHUNKS_OK || expected>UINT64_MAX-chunks.count || total>UINT64_MAX-index->roots[i].bytes) return fail(error,"Invalid aggregate version chunk extent.");
        expected+=chunks.count; total+=index->roots[i].bytes;
    }
    if (expected==UINT64_MAX || stopped(restore,0,error)) return false;
    const char *sql="SELECT count(*)=? AND coalesce(sum(octet_length(data)),0)=?::HUGEINT AND NOT coalesce(bool_or(root IS NULL OR octet_length(root)<>32 OR position IS NULL OR data IS NULL OR octet_length(data)=0 OR octet_length(data)>65536 OR NOT EXISTS(SELECT 1 FROM _duxcel_versions.roots r WHERE r.key=chunks.root)),false) FROM (SELECT root,position,data FROM _duxcel_versions.chunks LIMIT ?) chunks";
    duckdb_prepared_statement statement=NULL; duckdb_result r={0}; bool ok=prepare(c,sql,&statement,error);
    if (ok) {
        duckdb_bind_uint64(statement,1,expected); duckdb_bind_uint64(statement,2,total); duckdb_bind_uint64(statement,3,expected+1);
        ok=duckdb_execute_prepared(statement,&r)==DuckDBSuccess;
        if (!ok) fail(error,duckdb_result_error(&r));
        else if (duckdb_row_count(&r)!=1 || duckdb_value_is_null(&r,0,0) || !duckdb_value_boolean(&r,0,0)) ok=fail(error,"Version chunk counts, bytes, NULLs or root closure are invalid.");
    }
    duckdb_destroy_result(&r); if (statement) duckdb_destroy_prepare(&statement);
    return ok && !stopped(restore,0,error);
}
static bool read_chunk(void *context,uint64_t offset,void *buffer,size_t bytes,char **error) {
    Restore *restore=context; uint64_t expected_offset=0; size_t expected_bytes=0;
    if (!workbook_chunk_span(restore->chunks.bytes,restore->chunks.next,&expected_offset,&expected_bytes) || offset!=expected_offset || bytes!=expected_bytes) return fail(error,"Invalid sequential version chunk request.");
    duckdb_bind_blob(restore->statement,1,restore->key.bytes,DX_VERSION_KEY_BYTES);
    duckdb_bind_uint64(restore->statement,2,restore->chunks.next);
    duckdb_result r={0}; bool ok=duckdb_execute_prepared(restore->statement,&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r));
    else if (duckdb_row_count(&r)!=1 || duckdb_value_is_null(&r,0,0)) ok=fail(error,"A version chunk is missing or duplicated.");
    if (ok) {
        duckdb_blob blob=duckdb_value_blob(&r,0,0);
        ok=blob.size<=DX_VERSION_CHUNK_BYTES && blob.size==bytes && workbook_chunks_offer(&restore->chunks,restore->chunks.next,blob.data,(size_t)blob.size)==VERSION_CHUNKS_OK;
        if (ok) memcpy(buffer,blob.data,bytes); else fail(error,"A version chunk has an invalid length.");
        duckdb_free(blob.data);
    }
    duckdb_destroy_result(&r); return ok;
}
WorkbookVersionRestoreResult workbook_version_core_set_read(duckdb_connection c,const char *parent,uint64_t reserved,
    WorkbookVersionRestoreControl control,WorkbookVersionCoreSet **out,char **error) {
    if (!c || !parent || !*parent || !out || *out) { fail(error,"Choose an exclusive version transaction, staging folder and empty core-set result."); return VERSION_RESTORE_FAILED; }
    char *resolved=dx_absolute_path(parent);
    if (!resolved || dx_path_kind(resolved)!=DX_PATH_DIRECTORY) { free(resolved); fail(error,"The version core-set staging folder is unavailable."); return VERSION_RESTORE_FAILED; }
    WorkbookVersionCoreSet *set=dx_alloc(sizeof(*set)); Restore restore={.control=control,.phase=VERSION_RESTORE_INDEX}; bool ok=false;
    WorkbookVersionReadResult read=workbook_version_metadata_read(c,reserved,(WorkbookVersionReadControl){metadata_cancel,&restore},&set->metadata,error);
    if (read!=VERSION_METADATA_READY) goto done;
    const WorkbookVersionIndex *index=workbook_version_metadata_index(set->metadata);
    if (!chunk_inventory(c,index,&restore,error)) goto done;
    set->count=index->root_count; set->leaves=dx_alloc(set->count*sizeof(*set->leaves));
    if (set->count && !prepare(c,"SELECT data FROM _duxcel_versions.chunks WHERE root=? AND position=? LIMIT 2",&restore.statement,error)) goto done;
    for (size_t i=0;i<set->count;i++) {
        restore.root=i; restore.key=index->roots[i].key; restore.phase=VERSION_RESTORE_COPY;
        if (workbook_chunks_begin(index->roots[i].bytes,index->roots[i].bytes,&restore.chunks)!=VERSION_CHUNKS_OK) { fail(error,"Invalid version leaf extent."); goto done; }
        WorkbookCorePayloadInfo expected={.document={index->document_high,index->document_low},.bytes=index->roots[i].bytes,.encoding=index->roots[i].encoding};
        memcpy(expected.sha256,index->roots[i].key.bytes,DX_VERSION_KEY_BYTES);
        WorkbookCorePayloadResult result=workbook_core_payload_restore(&expected,resolved,expected.bytes,
            (WorkbookCorePayloadInput){read_chunk,leaf_cancel,&restore},set->leaves+i,error);
        if (result!=CORE_PAYLOAD_READY || workbook_chunks_finish(&restore.chunks)!=VERSION_CHUNKS_OK) goto done;
    }
    if (stopped(&restore,0,error)) goto done;
    ok=true;
done:
    if (restore.statement) duckdb_destroy_prepare(&restore.statement); free(resolved);
    if (ok) { *out=set; return VERSION_RESTORE_READY; }
    workbook_version_core_set_free(set); return restore.stopped ? VERSION_RESTORE_CANCELLED : VERSION_RESTORE_FAILED;
}
