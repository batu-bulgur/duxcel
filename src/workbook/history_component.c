#include "history_component.h"
#include "version_chunks.h"
#include "version_sql.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { WorkbookHistoryComponentControl control; WorkbookHistoryComponentPhase phase; unsigned asset; uint64_t position; bool stopped; } Control;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "History component admission failed."); } return false; }
static bool stop(Control *c,uint64_t bytes,char **error) {
    if (!c->stopped && c->control.cancelled) c->stopped=c->control.cancelled(c->control.context,c->phase,c->asset,c->position,bytes);
    if (c->stopped) fail(error,"History component work was cancelled; unpublished transaction changes require rollback."); return c->stopped;
}
static bool hash_cancel(void *context,uint64_t bytes) { return stop(context,bytes,NULL); }
static bool native_cancel(void *context,size_t root,uint64_t bytes) { Control *c=context; c->phase=HISTORY_COMPONENT_VERIFY; c->asset=DX_HISTORY_COMPONENT_HEAD; c->position=root; return stop(c,bytes,NULL); }
static bool restore_cancel(void *context,WorkbookCoreRestorePhase phase,uint64_t bytes) { Control *c=context; c->phase=phase==CORE_RESTORE_ADMIT ? HISTORY_COMPONENT_ADMIT : phase==CORE_RESTORE_RECHECK ? HISTORY_COMPONENT_RECHECK : HISTORY_COMPONENT_CHUNKS; return stop(c,bytes,NULL); }
static bool query(duckdb_connection c,const char *sql,duckdb_result *r,char **error) { return duckdb_query(c,sql,r)==DuckDBSuccess || fail(error,duckdb_result_error(r)); }
static bool exec(duckdb_connection c,const char *sql,char **error) { duckdb_result r={0}; bool ok=query(c,sql,&r,error); duckdb_destroy_result(&r); return ok; }
static bool prepared(duckdb_connection c,const char *sql,duckdb_prepared_statement *s,char **error) { return duckdb_prepare(c,sql,s)==DuckDBSuccess || fail(error,duckdb_prepare_error(*s)); }
static bool run(duckdb_prepared_statement s,char **error) { duckdb_result r={0}; bool ok=duckdb_execute_prepared(s,&r)==DuckDBSuccess; if (!ok) fail(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok; }
static WorkbookHistoryComponentStatus result(Control *c,bool ok) { return ok ? HISTORY_COMPONENT_READY : c->stopped ? HISTORY_COMPONENT_CANCELLED : HISTORY_COMPONENT_FAILED; }
static bool shape(duckdb_connection c,char **error) {
    static const char *meta[]={"codec","container_encoding","core_encoding","packet_encoding","kind","document_high","document_low","head_revision","head_bytes","head_hash","packet_bytes","packet_hash"};
    static const char *meta_types[]={"INTEGER","INTEGER","INTEGER","INTEGER","INTEGER","UBIGINT","UBIGINT","UBIGINT","UBIGINT","BLOB","UBIGINT","BLOB"};
    static const char *chunks[]={"asset","position","data"},*chunk_types[]={"INTEGER","UBIGINT","BLOB"};
    duckdb_result r={0};
    bool ok=query(c,"SELECT CASE WHEN octet_length(encode(c.table_name))<=6 THEN c.table_name END,CASE WHEN octet_length(encode(c.column_name))<=18 THEN c.column_name END,CASE WHEN octet_length(encode(c.data_type))<=7 THEN c.data_type END,c.ordinal_position,CASE WHEN octet_length(encode(t.table_type))<=10 THEN t.table_type END FROM information_schema.columns c JOIN information_schema.tables t USING(table_catalog,table_schema,table_name) WHERE c.table_catalog=current_database() AND c.table_schema='_duxcel_history' LIMIT 16",&r,error); unsigned seen[2]={0};
    if (ok && duckdb_row_count(&r)!=15) ok=fail(error,"The current history component has missing or extra fields.");
    for (idx_t i=0;ok && i<duckdb_row_count(&r);i++) {
        for (idx_t col=0;col<5;col++) if (duckdb_value_is_null(&r,col,i)) ok=false;
        char *table=duckdb_value_varchar(&r,0,i),*name=duckdb_value_varchar(&r,1,i),*type=duckdb_value_varchar(&r,2,i),*kind=duckdb_value_varchar(&r,4,i); uint64_t ordinal=duckdb_value_uint64(&r,3,i);
        unsigned side=table && !strcmp(table,"meta") ? 0u : table && !strcmp(table,"chunks") ? 1u : 2u;
        size_t count=side ? 3u : 12u; const char *const *names=side ? chunks : meta,*const *types=side ? chunk_types : meta_types;
        if (side>1 || !ordinal || ordinal>count || !name || !type || !kind || strcmp(kind,"BASE TABLE") || (seen[side]&(1u<<(ordinal-1))) || strcmp(name,names[ordinal-1]) || strcmp(type,types[ordinal-1])) ok=false;
        else seen[side]|=1u<<(ordinal-1);
        duckdb_free(table); duckdb_free(name); duckdb_free(type); duckdb_free(kind);
    }
    duckdb_destroy_result(&r); return (ok && seen[0]==4095u && seen[1]==7u) || fail(error,"Unsupported history component table/column shape.");
}
static bool hash_column(duckdb_result *r,idx_t column,unsigned char *out,char **error) {
    duckdb_blob blob=duckdb_value_blob(r,column,0); bool ok=blob.data && blob.size==DX_SOURCE_DIGEST_BYTES;
    if (ok) memcpy(out,blob.data,DX_SOURCE_DIGEST_BYTES); duckdb_free(blob.data); return ok || fail(error,"Invalid history component digest width.");
}
WorkbookHistoryComponentStatus workbook_history_component_probe(duckdb_connection c,uint64_t max_head_bytes,WorkbookHistoryComponentInfo *out,char **error) {
    if (!c || !out || !workbook_version_sql_transaction(c,error) || !shape(c,error)) return HISTORY_COMPONENT_FAILED;
    duckdb_result r={0}; WorkbookHistoryComponentInfo next={0}; bool ok=query(c,"SELECT codec,container_encoding,core_encoding,packet_encoding,kind,document_high,document_low,head_revision,head_bytes,CASE WHEN octet_length(head_hash)=32 THEN head_hash END,packet_bytes,CASE WHEN octet_length(packet_hash)=32 THEN packet_hash END FROM _duxcel_history.meta LIMIT 2",&r,error);
    if (ok && duckdb_row_count(&r)!=1) ok=false;
    for (idx_t i=0;ok && i<12;i++) if (duckdb_value_is_null(&r,i,0)) ok=false;
    if (ok) {
        ok=duckdb_value_int32(&r,0,0)==DX_HISTORY_COMPONENT_ENCODING && duckdb_value_int32(&r,1,0)==16 && duckdb_value_int32(&r,2,0)==15 && duckdb_value_int32(&r,3,0)==DX_HISTORY_PROFILE_ENCODING &&
            duckdb_value_int32(&r,4,0)>=0 && duckdb_value_int32(&r,4,0)<=HISTORY_PROFILE_RETAINED;
        next.kind=(WorkbookHistoryProfileKind)duckdb_value_int32(&r,4,0); next.document=(WorkbookId){duckdb_value_uint64(&r,5,0),duckdb_value_uint64(&r,6,0)};
        next.revision=duckdb_value_uint64(&r,7,0); next.head_bytes=duckdb_value_uint64(&r,8,0); next.packet_bytes=duckdb_value_uint64(&r,10,0);
        ok=ok && (next.document.high|next.document.low) && next.head_bytes && next.packet_bytes>=DX_HISTORY_PROFILE_HEADER_BYTES && next.packet_bytes<=DX_HISTORY_PROFILE_WIRE_BYTES && hash_column(&r,9,next.head_sha256,error) && hash_column(&r,11,next.packet_sha256,error);
    }
    duckdb_destroy_result(&r); if (!ok) { fail(error,"Invalid current history component descriptor."); return HISTORY_COMPONENT_FAILED; }
    if (next.head_bytes>max_head_bytes) return HISTORY_COMPONENT_LIMIT;
    WorkbookChunks head={0},packet={0};
    if (workbook_chunks_begin(next.head_bytes,next.head_bytes,&head)!=VERSION_CHUNKS_OK || workbook_chunks_begin(next.packet_bytes,next.packet_bytes,&packet)!=VERSION_CHUNKS_OK || head.count>UINT64_MAX-packet.count || next.head_bytes>UINT64_MAX-next.packet_bytes || head.count+packet.count==UINT64_MAX) return HISTORY_COMPONENT_LIMIT;
    char *sql=dx_printf("SELECT count(*),coalesce(sum(octet_length(data)),0)=?::HUGEINT,coalesce(bool_or(asset IS NULL OR asset NOT IN(0,1) OR position IS NULL OR data IS NULL OR octet_length(data)=0 OR octet_length(data)>65536),false) FROM (SELECT * FROM _duxcel_history.chunks LIMIT %llu)",(unsigned long long)(head.count+packet.count+1));
    duckdb_prepared_statement s=NULL; ok=prepared(c,sql,&s,error); free(sql);
    if (ok) { duckdb_bind_uint64(s,1,next.head_bytes+next.packet_bytes); ok=duckdb_execute_prepared(s,&r)==DuckDBSuccess && duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0) && !duckdb_value_is_null(&r,1,0) && !duckdb_value_is_null(&r,2,0) && duckdb_value_uint64(&r,0,0)==head.count+packet.count && duckdb_value_boolean(&r,1,0) && !duckdb_value_boolean(&r,2,0); }
    duckdb_destroy_result(&r); if (s) duckdb_destroy_prepare(&s);
    if (!ok) { fail(error,"History component chunk counts/bytes/NULLs are invalid."); return HISTORY_COMPONENT_FAILED; }
    *out=next; return HISTORY_COMPONENT_READY;
}
static bool fresh(duckdb_connection c,char **error) {
    duckdb_result r={0}; bool ok=workbook_version_sql_transaction(c,error) && query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND schema_name='_duxcel_history'",&r,error);
    if (ok) ok=duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0) && !duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r);
    return ok || fail(error,"History component staging requires a fresh reserved schema and explicit transaction.");
}
WorkbookHistoryComponentStatus workbook_history_component_stage(duckdb_connection c,const WorkbookHistoryNativeCapture *capture,
    void *wire,size_t capacity,size_t reserved,uint64_t storage_reserved,WorkbookHistoryComponentControl control,char **error) {
    if (!c || !capture || !wire || capacity>DX_HISTORY_PROFILE_WIRE_BYTES) return HISTORY_COMPONENT_FAILED;
    if (capacity>SIZE_MAX-DX_HISTORY_COMPONENT_IO_BYTES || capacity+DX_HISTORY_COMPONENT_IO_BYTES>reserved) return HISTORY_COMPONENT_LIMIT;
    size_t bytes=0; if (workbook_history_profile_encoded_size(workbook_history_native_profile(capture),&bytes)!=HISTORY_PROFILE_CODEC_OK || bytes>capacity) return HISTORY_COMPONENT_LIMIT;
    const WorkbookCorePayloadInfo *bounded=workbook_core_payload_info(workbook_history_native_head(capture));
    if (!bounded || bounded->bytes>storage_reserved || bytes>storage_reserved-bounded->bytes) return HISTORY_COMPONENT_LIMIT;
    Control progress={.control=control,.phase=HISTORY_COMPONENT_VERIFY};
    WorkbookHistoryNativeStatus checked=workbook_history_native_check(capture,(WorkbookHistoryNativeControl){native_cancel,&progress},error);
    if (checked!=HISTORY_NATIVE_READY) return progress.stopped ? HISTORY_COMPONENT_CANCELLED : checked==HISTORY_NATIVE_UNSUPPORTED ? HISTORY_COMPONENT_UNSUPPORTED : HISTORY_COMPONENT_FAILED;
    if (workbook_history_profile_encode(workbook_history_native_profile(capture),wire,capacity,&bytes)!=HISTORY_PROFILE_CODEC_OK) return HISTORY_COMPONENT_FAILED;
    unsigned char digest[DX_SOURCE_DIGEST_BYTES]; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; progress.asset=DX_HISTORY_COMPONENT_PROFILE;
    SourceDigestStatus hashed=source_digest_bytes(wire,bytes,hash_cancel,&progress,digest,detail);
    if (hashed!=SOURCE_DIGEST_OK) { fail(error,detail); return hashed==SOURCE_DIGEST_CANCELLED ? HISTORY_COMPONENT_CANCELLED : hashed==SOURCE_DIGEST_UNSUPPORTED ? HISTORY_COMPONENT_UNSUPPORTED : HISTORY_COMPONENT_FAILED; }
    if (!fresh(c,error)) return HISTORY_COMPONENT_FAILED;
    progress.phase=HISTORY_COMPONENT_DESCRIPTOR; if (stop(&progress,0,error)) return HISTORY_COMPONENT_CANCELLED;
    if (!exec(c,"CREATE SCHEMA _duxcel_history; CREATE TABLE _duxcel_history.meta(codec INTEGER,container_encoding INTEGER,core_encoding INTEGER,packet_encoding INTEGER,kind INTEGER,document_high UBIGINT,document_low UBIGINT,head_revision UBIGINT,head_bytes UBIGINT,head_hash BLOB,packet_bytes UBIGINT,packet_hash BLOB); CREATE TABLE _duxcel_history.chunks(asset INTEGER,position UBIGINT,data BLOB)",error)) return HISTORY_COMPONENT_FAILED;
    const WorkbookCorePayload *head=workbook_history_native_head(capture); const WorkbookCorePayloadInfo *info=workbook_core_payload_info(head); const WorkbookHistoryCapturedPosition *position=workbook_history_native_position(capture);
    duckdb_prepared_statement meta=NULL,chunks=NULL; FILE *file=NULL; bool ok=false;
    if (!prepared(c,"INSERT INTO _duxcel_history.meta VALUES (1,16,15,1,?,?,?,?,?,?,?,?)",&meta,error) || !prepared(c,"INSERT INTO _duxcel_history.chunks VALUES (?,?,?)",&chunks,error)) goto done;
    duckdb_bind_int32(meta,1,(int32_t)workbook_history_profile_view(workbook_history_native_profile(capture))->kind); duckdb_bind_uint64(meta,2,info->document.high); duckdb_bind_uint64(meta,3,info->document.low); duckdb_bind_uint64(meta,4,position->revision); duckdb_bind_uint64(meta,5,info->bytes); duckdb_bind_blob(meta,6,info->sha256,DX_SOURCE_DIGEST_BYTES); duckdb_bind_uint64(meta,7,bytes); duckdb_bind_blob(meta,8,digest,DX_SOURCE_DIGEST_BYTES);
    if (!run(meta,error)) goto done;
    file=fopen(workbook_core_payload_path(head),"rb"); if (!file) { fail(error,"Cannot read the actual sealed captured head."); goto done; }
    unsigned char buffer[DX_VERSION_CHUNK_BYTES]; progress.phase=HISTORY_COMPONENT_CHUNKS;
    for (unsigned asset=0;asset<2;asset++) {
        WorkbookChunks spans={0}; uint64_t length=asset ? bytes : info->bytes; if (workbook_chunks_begin(length,length,&spans)!=VERSION_CHUNKS_OK) goto done;
        progress.asset=asset;
        while (spans.next<spans.count) {
            uint64_t offset=0; size_t count=0; progress.position=spans.next;
            if (stop(&progress,spans.received,error) || !workbook_chunk_span(length,spans.next,&offset,&count)) goto done;
            const void *data=asset ? (const unsigned char *)wire+(size_t)offset : buffer;
            if (!asset && fread(buffer,1,count,file)!=count) { fail(error,"The captured head was truncated during staging."); goto done; }
            if (workbook_chunks_offer(&spans,spans.next,data,count)!=VERSION_CHUNKS_OK) goto done;
            duckdb_bind_int32(chunks,1,(int32_t)asset); duckdb_bind_uint64(chunks,2,progress.position); duckdb_bind_blob(chunks,3,data,count); if (!run(chunks,error)) goto done;
        }
        if (workbook_chunks_finish(&spans)!=VERSION_CHUNKS_OK) goto done;
        if (!asset && (fgetc(file)!=EOF || ferror(file))) { fail(error,"The captured head extent changed during staging."); goto done; }
    }
    if (fclose(file)) { file=NULL; fail(error,"Cannot close staged head input."); goto done; } file=NULL;
    progress.phase=HISTORY_COMPONENT_RECHECK;
    checked=workbook_history_native_check(capture,(WorkbookHistoryNativeControl){native_cancel,&progress},error); if (checked!=HISTORY_NATIVE_READY) goto done;
    progress.phase=HISTORY_COMPONENT_RECHECK; progress.asset=DX_HISTORY_COMPONENT_PROFILE;
    unsigned char after[DX_SOURCE_DIGEST_BYTES];
    if (source_digest_bytes(wire,bytes,hash_cancel,&progress,after,detail)!=SOURCE_DIGEST_OK || memcmp(after,digest,sizeof(digest))) { fail(error,"The immutable history packet changed during staging."); goto done; }
    ok=!stop(&progress,bytes,error);
done:
    if (file) fclose(file); if (meta) duckdb_destroy_prepare(&meta); if (chunks) duckdb_destroy_prepare(&chunks); return result(&progress,ok);
}
typedef struct { duckdb_prepared_statement statement; WorkbookChunks spans; unsigned asset; Control *control; } Input;
static bool read_chunk(void *context,uint64_t offset,void *buffer,size_t bytes,char **error) {
    Input *input=context; uint64_t expected=0; size_t count=0;
    if (!workbook_chunk_span(input->spans.bytes,input->spans.next,&expected,&count) || offset!=expected || bytes!=count) return fail(error,"Invalid history chunk read order/extent.");
    input->control->asset=input->asset; input->control->position=input->spans.next;
    if (stop(input->control,input->spans.received,error)) return false;
    duckdb_bind_int32(input->statement,1,(int32_t)input->asset); duckdb_bind_uint64(input->statement,2,input->spans.next);
    duckdb_result r={0}; bool ok=duckdb_execute_prepared(input->statement,&r)==DuckDBSuccess && duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0);
    if (ok) { duckdb_blob blob=duckdb_value_blob(&r,0,0); ok=blob.data && blob.size==bytes && workbook_chunks_offer(&input->spans,input->spans.next,blob.data,bytes)==VERSION_CHUNKS_OK; if (ok) memcpy(buffer,blob.data,bytes); duckdb_free(blob.data); }
    duckdb_destroy_result(&r); return ok || fail(error,"Missing, duplicated or malformed history chunk.");
}
static bool head_cancel(void *context,WorkbookCoreRestorePhase phase,uint64_t bytes) { Input *input=context; return restore_cancel(input->control,phase,bytes); }
WorkbookHistoryComponentStatus workbook_history_component_read(duckdb_connection c,const WorkbookVersionCoreSet *roots,
    WorkbookHistoryProfileBinding binding,const char *parent,WorkbookHistoryProfileCaps profile_caps,WorkbookHistoryNativeCaps native_caps,
    void *wire,size_t capacity,void *parse_arena,size_t parse_capacity,WorkbookHistoryComponentControl control,WorkbookHistoryNativeCapture **out,char **error) {
    if (!c || !roots || !parent || !*parent || !out || !wire || capacity>DX_HISTORY_PROFILE_WIRE_BYTES) return HISTORY_COMPONENT_FAILED;
    WorkbookHistoryComponentInfo info; WorkbookHistoryComponentStatus status=workbook_history_component_probe(c,native_caps.physical_bytes,&info,error); if (status) return status;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots);
    if (!index || info.document.high!=index->document_high || info.document.low!=index->document_low || binding.owner.document_high!=info.document.high || binding.owner.document_low!=info.document.low || !binding.owner.session || binding.owner.mode!=VERSION_WORKING_HEAD || !binding.edit_session) return HISTORY_COMPONENT_FAILED;
    if (parse_capacity>DX_HISTORY_PROFILE_PARSE_BYTES || info.packet_bytes>capacity || capacity>SIZE_MAX-DX_HISTORY_COMPONENT_IO_BYTES) return HISTORY_COMPONENT_LIMIT;
    size_t charge=capacity+DX_HISTORY_COMPONENT_IO_BYTES,prior=*out ? workbook_history_profile_owned(workbook_history_native_profile(*out)) : 0;
    if (prior>SIZE_MAX-charge || charge+prior>profile_caps.staging_bytes) return HISTORY_COMPONENT_LIMIT; profile_caps.staging_bytes-=charge+prior;
    if (parse_capacity>profile_caps.staging_bytes) return HISTORY_COMPONENT_LIMIT;
    uint64_t extent=info.head_bytes;
    for (size_t i=0;i<index->root_count;i++) { if (index->roots[i].bytes>UINT64_MAX-extent) return HISTORY_COMPONENT_LIMIT; extent+=index->roots[i].bytes; }
    if (*out) {
        WorkbookHistoryNativeMeasure old;
        if (workbook_history_native_measure(workbook_history_native_profile(*out),workbook_history_native_roots(*out),workbook_history_native_head(*out),*workbook_history_native_position(*out),(WorkbookHistoryNativeCaps){UINT64_MAX,SIZE_MAX},NULL,&old)!=HISTORY_NATIVE_READY) return HISTORY_COMPONENT_FAILED;
        if (old.physical_bytes>UINT64_MAX-extent) return HISTORY_COMPONENT_LIMIT; extent+=old.physical_bytes;
    }
    if (extent>native_caps.physical_bytes) return HISTORY_COMPONENT_LIMIT;
    Control progress={.control=control,.phase=HISTORY_COMPONENT_CHUNKS,.asset=DX_HISTORY_COMPONENT_HEAD};
    Input input={.control=&progress}; WorkbookCorePayload *head=NULL; WorkbookHistoryProfile *profile=NULL; WorkbookVersionCoreSet *retained=NULL;
    if (!prepared(c,"SELECT data FROM _duxcel_history.chunks WHERE asset=? AND position=? LIMIT 2",&input.statement,error)) return HISTORY_COMPONENT_FAILED;
    bool ok=false;
    if (workbook_chunks_begin(info.head_bytes,info.head_bytes,&input.spans)!=VERSION_CHUNKS_OK) goto done;
    WorkbookCorePayloadInfo expected={.document=info.document,.bytes=info.head_bytes,.encoding=15}; memcpy(expected.sha256,info.head_sha256,DX_SOURCE_DIGEST_BYTES);
    WorkbookCorePayloadResult restored=workbook_core_payload_restore(&expected,parent,info.head_bytes,(WorkbookCorePayloadInput){read_chunk,head_cancel,&input},&head,error);
    if (restored!=CORE_PAYLOAD_READY) { if (restored==CORE_PAYLOAD_UNSUPPORTED) status=HISTORY_COMPONENT_UNSUPPORTED; goto done; }
    if (workbook_chunks_finish(&input.spans)!=VERSION_CHUNKS_OK) goto done;
    uint64_t revision=0; if (!workbook_core_payload_revision(head,&revision) || revision!=info.revision) { fail(error,"The restored head revision does not match its current descriptor."); goto done; }
    input.asset=DX_HISTORY_COMPONENT_PROFILE; progress.phase=HISTORY_COMPONENT_CHUNKS;
    if (workbook_chunks_begin(info.packet_bytes,info.packet_bytes,&input.spans)!=VERSION_CHUNKS_OK) goto done;
    while (input.spans.next<input.spans.count) { uint64_t offset=0; size_t bytes=0; if (!workbook_chunk_span(info.packet_bytes,input.spans.next,&offset,&bytes) || !read_chunk(&input,offset,(unsigned char *)wire+(size_t)offset,bytes,error)) goto done; }
    if (workbook_chunks_finish(&input.spans)!=VERSION_CHUNKS_OK) goto done;
    unsigned char digest[DX_SOURCE_DIGEST_BYTES]; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; progress.phase=HISTORY_COMPONENT_VERIFY;
    SourceDigestStatus hashed=source_digest_bytes(wire,(size_t)info.packet_bytes,hash_cancel,&progress,digest,detail);
    if (hashed!=SOURCE_DIGEST_OK || memcmp(digest,info.packet_sha256,sizeof(digest))) { if (hashed==SOURCE_DIGEST_UNSUPPORTED) status=HISTORY_COMPONENT_UNSUPPORTED; fail(error,"History component packet hash mismatch or cancellation."); goto done; }
    progress.phase=HISTORY_COMPONENT_ADMIT; if (stop(&progress,info.packet_bytes,error)) goto done;
    WorkbookHistoryProfileCodecStatus decoded=workbook_history_profile_decode(wire,(size_t)info.packet_bytes,binding,profile_caps,parse_arena,parse_capacity,&profile);
    if (decoded!=HISTORY_PROFILE_CODEC_OK) { status=decoded==HISTORY_PROFILE_CODEC_LIMIT ? HISTORY_COMPONENT_LIMIT : HISTORY_COMPONENT_FAILED; goto done; }
    if (workbook_history_profile_view(profile)->kind!=info.kind) { fail(error,"Current history advertisement differs from the packet kind."); goto done; }
    retained=workbook_version_core_set_retain(roots); if (!retained) { fail(error,"Cannot retain current root ownership for history admission."); goto done; }
    progress.phase=HISTORY_COMPONENT_RECHECK; progress.asset=DX_HISTORY_COMPONENT_PROFILE;
    if (stop(&progress,info.packet_bytes,error)) goto done;
    WorkbookHistoryNativeStatus adopted=workbook_history_native_take(&profile,&retained,&head,(WorkbookHistoryCapturedPosition){binding.owner,info.revision},native_caps,(WorkbookHistoryNativeControl){native_cancel,&progress},out,error);
    if (adopted!=HISTORY_NATIVE_READY) { status=adopted==HISTORY_NATIVE_LIMIT ? HISTORY_COMPONENT_LIMIT : adopted==HISTORY_NATIVE_UNSUPPORTED ? HISTORY_COMPONENT_UNSUPPORTED : HISTORY_COMPONENT_FAILED; goto done; }
    ok=true;
done:
    if (input.statement) duckdb_destroy_prepare(&input.statement); workbook_history_profile_free(&profile); workbook_version_core_set_free(retained); workbook_core_payload_free(head);
    return ok ? HISTORY_COMPONENT_READY : progress.stopped ? HISTORY_COMPONENT_CANCELLED : status==HISTORY_COMPONENT_LIMIT || status==HISTORY_COMPONENT_UNSUPPORTED ? status : HISTORY_COMPONENT_FAILED;
}
