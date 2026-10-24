#include "version_stage.h"
#include "version_chunks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { WorkbookVersionStageControl control; WorkbookVersionStagePhase phase; size_t root; bool stopped; } Stage;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Version staging failed."); } return false; }
static bool stopped(Stage *s,uint64_t bytes,char **error) {
    if (!s->stopped && s->control.cancelled) s->stopped=s->control.cancelled(s->control.context,s->phase,s->root,bytes);
    if (s->stopped) fail(error,"Version staging was cancelled; roll back the private transaction."); return s->stopped;
}
static bool hash_cancel(void *context,uint64_t bytes) { return stopped(context,bytes,NULL); }
static bool exec(duckdb_connection c,const char *sql,char **error) {
    duckdb_result result={0}; bool ok=duckdb_query(c,sql,&result)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&result)); duckdb_destroy_result(&result); return ok;
}
static bool transaction_id(duckdb_connection c,uint64_t *out,char **error) {
    duckdb_result r={0}; bool ok=duckdb_query(c,"SELECT current_transaction_id()",&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r));
    else if (duckdb_row_count(&r)!=1 || duckdb_column_count(&r)!=1 || duckdb_column_type(&r,0)!=DUCKDB_TYPE_UBIGINT || duckdb_value_is_null(&r,0,0)) ok=fail(error,"Cannot verify the version transaction owner.");
    if (ok) *out=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); return ok;
}
static bool fresh_transaction(duckdb_connection c,char **error) {
    uint64_t a=0,b=0;
    if (!transaction_id(c,&a,error) || !transaction_id(c,&b,error)) return false;
    if (a!=b) return fail(error,"Version staging requires a caller-owned explicit transaction; autocommit was rejected.");
    duckdb_result r={0};
    bool ok=duckdb_query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND schema_name='_duxcel_versions'",&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r));
    else if (duckdb_row_count(&r)!=1 || duckdb_value_uint64(&r,0,0)) ok=fail(error,"Version staging requires a fresh reserved schema; existing metadata was kept.");
    duckdb_destroy_result(&r); return ok;
}
static bool prepare(duckdb_connection c,const char *sql,duckdb_prepared_statement *statement,char **error) {
    if (duckdb_prepare(c,sql,statement)==DuckDBSuccess) return true;
    return fail(error,duckdb_prepare_error(*statement));
}
static bool run(duckdb_prepared_statement statement,char **error) {
    duckdb_result r={0}; bool ok=duckdb_execute_prepared(statement,&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok;
}
static bool verify(const WorkbookCorePayload *p,Stage *s,char **error) {
    if (stopped(s,0,error)) return false;
    WorkbookCorePayloadResult result=workbook_core_payload_verify(p,hash_cancel,s,error);
    if (result==CORE_PAYLOAD_CANCELLED) s->stopped=true;
    return result==CORE_PAYLOAD_READY;
}
WorkbookVersionStageResult workbook_version_stage(duckdb_connection c,const WorkbookVersionIndex *index,
    const WorkbookCorePayload *const *leaves,size_t count,uint64_t reserved,WorkbookVersionStageControl control,char **error) {
    Stage stage={.control=control,.phase=VERSION_STAGE_VERIFY};
    if (!c || workbook_version_index_check(index)!=VERSION_OK || count!=index->root_count || (count && !leaves)) {
        fail(error,"Choose a valid version index and its complete ordered core leaves."); return VERSION_STAGE_FAILED;
    }
    uint64_t bytes=0;
    for (size_t i=0;i<count;i++) {
        const WorkbookVersionRoot *root=index->roots+i; const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaves[i]);
        if (!info || info->document.high!=index->document_high || info->document.low!=index->document_low ||
            info->bytes!=root->bytes || info->encoding!=root->encoding || memcmp(info->sha256,root->key.bytes,DX_VERSION_KEY_BYTES) || root->bytes>reserved-bytes) {
            fail(error,"Version leaves must match every root, document and reserved byte bound."); return VERSION_STAGE_FAILED;
        }
        bytes+=root->bytes;
    }
    if (stopped(&stage,0,error)) return VERSION_STAGE_CANCELLED;
    if (!fresh_transaction(c,error)) return VERSION_STAGE_FAILED;
    for (size_t i=0;i<count;i++) { stage.root=i; if (!verify(leaves[i],&stage,error)) return stage.stopped ? VERSION_STAGE_CANCELLED : VERSION_STAGE_FAILED; }
    if (!exec(c,"CREATE SCHEMA _duxcel_versions;"
        "CREATE TABLE _duxcel_versions.meta(codec INTEGER,document_high UBIGINT,document_low UBIGINT,generation UBIGINT,saved_generation UBIGINT,saved_present BOOLEAN,saved_key BLOB);"
        "CREATE TABLE _duxcel_versions.roots(position INTEGER,key BLOB,bytes UBIGINT,encoding INTEGER,PRIMARY KEY(position),UNIQUE(key));"
        "CREATE TABLE _duxcel_versions.tags(position INTEGER,label VARCHAR,root BLOB,created UBIGINT,PRIMARY KEY(position),UNIQUE(label));"
        "CREATE TABLE _duxcel_versions.chunks(root BLOB,position UBIGINT,data BLOB,PRIMARY KEY(root,position));",error)) return VERSION_STAGE_FAILED;
    const char *sql[]={"INSERT INTO _duxcel_versions.meta VALUES (1,?,?,?,?,?,?)","INSERT INTO _duxcel_versions.roots VALUES (?,?,?,?)","INSERT INTO _duxcel_versions.tags VALUES (?,?,?,?)","INSERT INTO _duxcel_versions.chunks VALUES (?,?,?)"};
    duckdb_prepared_statement statements[4]={0}; FILE *file=NULL; bool ok=false;
    for (size_t i=0;i<4;i++) if (!prepare(c,sql[i],statements+i,error)) goto done;
    duckdb_prepared_statement meta=statements[0],root_statement=statements[1],tag=statements[2],chunk=statements[3];
    duckdb_bind_uint64(meta,1,index->document_high); duckdb_bind_uint64(meta,2,index->document_low);
    duckdb_bind_uint64(meta,3,index->generation); duckdb_bind_uint64(meta,4,index->saved_generation);
    duckdb_bind_boolean(meta,5,index->saved.present);
    if (index->saved.present) duckdb_bind_blob(meta,6,index->saved.key.bytes,DX_VERSION_KEY_BYTES); else duckdb_bind_null(meta,6);
    if (!run(meta,error)) goto done;
    for (size_t i=0;i<index->tag_count;i++) {
        if (stopped(&stage,0,error)) goto done;
        const WorkbookVersionTag *t=index->tags+i;
        duckdb_bind_int32(tag,1,(int32_t)i); duckdb_bind_varchar_length(tag,2,t->label.data,t->label.bytes);
        duckdb_bind_blob(tag,3,t->root.bytes,DX_VERSION_KEY_BYTES); duckdb_bind_uint64(tag,4,t->created);
        if (!run(tag,error)) goto done;
    }
    unsigned char buffer[DX_VERSION_CHUNK_BYTES];
    for (size_t i=0;i<count;i++) {
        stage.root=i; stage.phase=VERSION_STAGE_CHUNKS;
        if (stopped(&stage,0,error)) goto done;
        const WorkbookVersionRoot *r=index->roots+i;
        duckdb_bind_int32(root_statement,1,(int32_t)i); duckdb_bind_blob(root_statement,2,r->key.bytes,DX_VERSION_KEY_BYTES);
        duckdb_bind_uint64(root_statement,3,r->bytes); duckdb_bind_int32(root_statement,4,(int32_t)r->encoding);
        if (!run(root_statement,error)) goto done;
        file=fopen(workbook_core_payload_path(leaves[i]),"rb");
        if (!file) { fail(error,"Cannot read the sealed version leaf."); goto done; }
        WorkbookChunks chunks={0}; if (workbook_chunks_begin(r->bytes,r->bytes,&chunks)!=VERSION_CHUNKS_OK) { fail(error,"Invalid version chunk extent."); goto done; }
        while (chunks.next<chunks.count) {
            if (stopped(&stage,chunks.received,error)) goto done;
            uint64_t offset=0; size_t length=0; uint64_t position=chunks.next;
            if (!workbook_chunk_span(chunks.bytes,position,&offset,&length) || fread(buffer,1,length,file)!=length ||
                workbook_chunks_offer(&chunks,position,buffer,length)!=VERSION_CHUNKS_OK) { fail(error,"The version leaf was truncated or unreadable."); goto done; }
            duckdb_bind_blob(chunk,1,r->key.bytes,DX_VERSION_KEY_BYTES); duckdb_bind_uint64(chunk,2,position); duckdb_bind_blob(chunk,3,buffer,length);
            if (!run(chunk,error)) goto done;
        }
        if (fgetc(file)!=EOF || ferror(file) || workbook_chunks_finish(&chunks)!=VERSION_CHUNKS_OK) { fail(error,"The version leaf extent changed while staging."); goto done; }
        int closed=fclose(file); file=NULL; if (closed) { fail(error,"Cannot close the staged version leaf reader."); goto done; }
        stage.phase=VERSION_STAGE_RECHECK;
        if (!verify(leaves[i],&stage,error)) goto done;
    }
    if (stopped(&stage,bytes,error)) goto done;
    ok=true;
done:
    if (file) fclose(file);
    for (size_t i=0;i<4;i++) if (statements[i]) duckdb_destroy_prepare(statements+i);
    return ok ? VERSION_STAGE_READY : stage.stopped ? VERSION_STAGE_CANCELLED : VERSION_STAGE_FAILED;
}
