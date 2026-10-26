#include "browse_preview_component.h"
#include "version_sql.h"
#include <stdlib.h>
#include <string.h>
typedef struct { BrowsePreviewComponentControl control; BrowsePreviewComponentPhase phase; size_t entry; bool stopped; } Progress;
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Browse preview staging failed."); } return false; }
static bool add(size_t *n,size_t x) { if (*n>SIZE_MAX-x) return false; *n+=x; return true; }
static bool stop(Progress *p,uint64_t bytes,char **error) {
    if (!p->stopped && p->control.cancelled) p->stopped=p->control.cancelled(p->control.context,p->phase,p->entry,bytes);
    if (p->stopped) fail(error,"Browse preview staging was cancelled; unpublished changes require rollback.");
    return p->stopped;
}
static bool hash_cancel(void *context,uint64_t bytes) { return stop(context,bytes,NULL); }
static bool query(duckdb_connection c,const char *sql,duckdb_result *r,char **error) { return duckdb_query(c,sql,r)==DuckDBSuccess || fail(error,duckdb_result_error(r)); }
static bool exec(duckdb_connection c,const char *sql,char **error) { duckdb_result r={0}; bool ok=query(c,sql,&r,error); duckdb_destroy_result(&r); return ok; }
static bool prepare(duckdb_connection c,const char *sql,duckdb_prepared_statement *s,char **error) { return duckdb_prepare(c,sql,s)==DuckDBSuccess || fail(error,duckdb_prepare_error(*s)); }
static bool run(duckdb_prepared_statement s,char **error) { duckdb_result r={0}; bool ok=duckdb_execute_prepared(s,&r)==DuckDBSuccess; if (!ok) fail(error,duckdb_result_error(&r)); duckdb_destroy_result(&r); return ok; }
static bool same(const WorkbookCorePayloadInfo *a,const WorkbookCorePayloadInfo *b) {
    return a && b && a->document.high==b->document.high && a->document.low==b->document.low && a->bytes==b->bytes && a->encoding==b->encoding && !memcmp(a->sha256,b->sha256,DX_SOURCE_DIGEST_BYTES);
}
static const Workbook *snapshot(const WorkbookVersionPublication *publication,duckdb_connection c,const WorkbookVersionCoreSet *after,const BrowsePreviewSet *set,char **error) {
    const Workbook *actual=workbook_version_publication_snapshot(publication,c,after);
    const WorkbookCorePayload *capture=workbook_version_publication_capture(publication),*selected=workbook_version_publication_selected(publication);
    const WorkbookCorePayloadInfo *captured=workbook_core_payload_info(capture),*target=workbook_core_payload_info(selected);
    const WorkbookVersionPlan *plan=workbook_version_publication_plan(publication);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(after);
    BrowsePreviewSetInfo info=browse_preview_set_info(set);
    bool valid=actual && set && workbook_identity_valid(actual) && captured && target && plan && index &&
        workbook_version_index_check(index)==VERSION_OK && plan->purpose==VERSION_USER_SAVE && index->saved.present &&
        !memcmp(index->saved.key.bytes,plan->root.key.bytes,DX_VERSION_KEY_BYTES) &&
        info.document.high==actual->id.high && info.document.low==actual->id.low && info.snapshot_revision==actual->command_revision &&
        info.scope==BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT && info.snapshot_revision==workbook_version_publication_revision(publication) &&
        captured->document.high==actual->id.high && captured->document.low==actual->id.low && target->document.high==actual->id.high && target->document.low==actual->id.low &&
        index->document_high==actual->id.high && index->document_low==actual->id.low && captured->encoding==15 && target->encoding==15 &&
        plan->root.bytes==target->bytes && plan->root.encoding==target->encoding && !memcmp(plan->root.key.bytes,target->sha256,DX_VERSION_KEY_BYTES) &&
        (selected==capture || workbook_version_publication_compared(publication));
    bool found=false;
    for (size_t i=0;valid && i<index->root_count;i++) if (!memcmp(index->roots[i].key.bytes,target->sha256,DX_VERSION_KEY_BYTES)) {
        found=index->roots[i].bytes==target->bytes && index->roots[i].encoding==15 && same(target,workbook_core_payload_info(workbook_version_core_set_leaf(after,i))); break;
    }
    if (!valid || !found) { fail(error,"The preview Set does not match the actual active User Save snapshot and selected root."); return NULL; }
    return actual;
}
BrowsePreviewComponentStatus browse_preview_component_measure(const WorkbookVersionPublication *publication,
    duckdb_connection c,const WorkbookVersionCoreSet *after,const BrowsePreviewSet *set,size_t other_owned,BrowsePreviewComponentInfo *out,char **error) {
    if (!out || !c || !publication || !set) return BROWSE_COMPONENT_FAILED;
    if (workbook_version_publication_intent(publication)!=VERSION_PUBLICATION_USER_SAVE) return BROWSE_COMPONENT_UNSUPPORTED;
    const Workbook *actual=snapshot(publication,c,after,set,error); if (!actual) return BROWSE_COMPONENT_FAILED;
    BrowsePreviewSetWireInfo wire={0}; BrowsePreviewSetStatus measured=browse_preview_set_encode_measure(set,NULL,&wire);
    if (measured!=BROWSE_PREVIEW_SET_OK) return measured==BROWSE_PREVIEW_SET_LIMIT ? BROWSE_COMPONENT_LIMIT : BROWSE_COMPONENT_FAILED;
    size_t admission=0;
    for (size_t i=0;i<wire.set.count;i++) {
        const BrowsePreview *asset=browse_preview_set_asset(set,i); size_t bytes=0;
        BrowsePreviewStatus status=browse_preview_workbook_measure(asset,actual,0,&bytes);
        if (status!=BROWSE_PREVIEW_OK) return status==BROWSE_PREVIEW_LIMIT ? BROWSE_COMPONENT_LIMIT : BROWSE_COMPONENT_FAILED;
        size_t owned=browse_preview_info(asset).owned;
        if (bytes<owned) return BROWSE_COMPONENT_FAILED;
        if (bytes-owned>admission) admission=bytes-owned;
    }
    /* Encoding and association are sequential; charge their maximum scratch. */
    size_t total=wire.set.owned;
    if (!add(&total,wire.bytes) || !add(&total,admission>wire.validation_scratch ? admission : wire.validation_scratch) ||
        !add(&total,other_owned) || !add(&total,DX_BROWSE_PREVIEW_COMPONENT_IO_BYTES)) return BROWSE_COMPONENT_LIMIT;
    const WorkbookCorePayloadInfo *target=workbook_core_payload_info(workbook_version_publication_selected(publication));
    BrowsePreviewComponentInfo info={.document=wire.set.document,.snapshot_revision=wire.set.snapshot_revision,.target_bytes=target->bytes,.set_bytes=wire.bytes,.entries=wire.set.count,.coexistence=total};
    memcpy(info.target_sha256,target->sha256,DX_SOURCE_DIGEST_BYTES); *out=info; return BROWSE_COMPONENT_READY;
}
static bool fresh(duckdb_connection c,char **error) {
    duckdb_result r={0}; bool ok=workbook_version_sql_transaction(c,error) && query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND schema_name='_duxcel_previews'",&r,error);
    if (ok) ok=duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0) && !duckdb_value_uint64(&r,0,0);
    duckdb_destroy_result(&r); return ok || fail(error,"Preview staging requires a fresh reserved schema in the actual explicit publication transaction.");
}
static BrowsePreviewComponentStatus failed(Progress *p) { return p->stopped ? BROWSE_COMPONENT_CANCELLED : BROWSE_COMPONENT_FAILED; }
static BrowsePreviewComponentStatus verify(const WorkbookCorePayload *leaf,Progress *p,char **error) {
    WorkbookCorePayloadResult result=workbook_core_payload_verify(leaf,hash_cancel,p,error);
    return result==CORE_PAYLOAD_READY ? BROWSE_COMPONENT_READY : result==CORE_PAYLOAD_UNSUPPORTED ? BROWSE_COMPONENT_UNSUPPORTED : failed(p);
}
BrowsePreviewComponentStatus browse_preview_component_stage(const WorkbookVersionPublication *publication,
    duckdb_connection c,const WorkbookVersionCoreSet *after,const BrowsePreviewSet *set,size_t other_owned,size_t reserved,uint64_t storage_reserved,
    BrowsePreviewComponentControl control,BrowsePreviewComponentInfo *out,char **error) {
    if (!out) return BROWSE_COMPONENT_FAILED;
    BrowsePreviewComponentInfo info={0}; BrowsePreviewComponentStatus status=browse_preview_component_measure(publication,c,after,set,other_owned,&info,error);
    if (status!=BROWSE_COMPONENT_READY) return status;
    if (reserved<info.coexistence || storage_reserved<info.set_bytes) return BROWSE_COMPONENT_LIMIT;
    if (!fresh(c,error)) return BROWSE_COMPONENT_FAILED;
    const Workbook *actual=snapshot(publication,c,after,set,error); if (!actual) return BROWSE_COMPONENT_FAILED;
    const WorkbookCorePayload *capture=workbook_version_publication_capture(publication),*selected=workbook_version_publication_selected(publication);
    Progress p={.control=control,.phase=BROWSE_COMPONENT_VERIFY};
    status=verify(capture,&p,error); if (status!=BROWSE_COMPONENT_READY) return status;
    status=verify(selected,&p,error); if (status!=BROWSE_COMPONENT_READY) return status;
    p.phase=BROWSE_COMPONENT_ASSOCIATE;
    for (size_t i=0;i<info.entries;i++) {
        p.entry=i; if (stop(&p,0,error)) return BROWSE_COMPONENT_CANCELLED;
        BrowsePreviewWorkbookAdmission report={0}; const BrowsePreview *asset=browse_preview_set_asset(set,i); size_t bytes=0;
        if (browse_preview_workbook_measure(asset,actual,0,&bytes)!=BROWSE_PREVIEW_OK) return BROWSE_COMPONENT_FAILED;
        SourceDependencyStatus admitted=browse_preview_workbook_admit(asset,actual,0,bytes,&report);
        if (admitted!=SOURCE_DEPENDENCY_EXACT && admitted!=SOURCE_DEPENDENCY_REFINED) { fail(error,"The optional preview is incompatible with the actual captured source owner."); return admitted==SOURCE_DEPENDENCY_LIMIT ? BROWSE_COMPONENT_LIMIT : BROWSE_COMPONENT_FAILED; }
    }
    BrowsePreviewPacket packet={0}; BrowsePreviewSetStatus encoded=browse_preview_set_encode(set,SIZE_MAX,&packet);
    if (encoded!=BROWSE_PREVIEW_SET_OK) return encoded==BROWSE_PREVIEW_SET_LIMIT ? BROWSE_COMPONENT_LIMIT : BROWSE_COMPONENT_FAILED;
    duckdb_prepared_statement meta=NULL,chunks=NULL,read=NULL; bool ok=false; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    p.phase=BROWSE_COMPONENT_VERIFY; p.entry=0;
    SourceDigestStatus hashed=source_digest_bytes(packet.data,packet.bytes,hash_cancel,&p,info.set_sha256,detail);
    if (hashed!=SOURCE_DIGEST_OK) { status=hashed==SOURCE_DIGEST_UNSUPPORTED ? BROWSE_COMPONENT_UNSUPPORTED : failed(&p); fail(error,detail); goto done; }
    p.phase=BROWSE_COMPONENT_DESCRIPTOR; if (stop(&p,0,error)) goto done;
    if (!exec(c,"CREATE SCHEMA _duxcel_previews; CREATE TABLE _duxcel_previews.meta(codec INTEGER,container_encoding INTEGER,core_encoding INTEGER,set_encoding INTEGER,scope INTEGER,document_high UBIGINT,document_low UBIGINT,snapshot_revision UBIGINT,target_bytes UBIGINT,target_hash BLOB,set_bytes UBIGINT,set_hash BLOB); CREATE TABLE _duxcel_previews.chunks(position UBIGINT,data BLOB)",error) ||
        !prepare(c,"INSERT INTO _duxcel_previews.meta VALUES(1,16,15,1,1,?,?,?,?,?,?,?)",&meta,error) ||
        !prepare(c,"INSERT INTO _duxcel_previews.chunks VALUES(?,?)",&chunks,error) ||
        !prepare(c,"SELECT data FROM _duxcel_previews.chunks WHERE position=? LIMIT 2",&read,error)) goto done;
    if (duckdb_bind_uint64(meta,1,info.document.high)!=DuckDBSuccess || duckdb_bind_uint64(meta,2,info.document.low)!=DuckDBSuccess || duckdb_bind_uint64(meta,3,info.snapshot_revision)!=DuckDBSuccess ||
        duckdb_bind_uint64(meta,4,info.target_bytes)!=DuckDBSuccess || duckdb_bind_blob(meta,5,info.target_sha256,DX_SOURCE_DIGEST_BYTES)!=DuckDBSuccess || duckdb_bind_uint64(meta,6,info.set_bytes)!=DuckDBSuccess || duckdb_bind_blob(meta,7,info.set_sha256,DX_SOURCE_DIGEST_BYTES)!=DuckDBSuccess || !run(meta,error)) goto done;
    WorkbookChunks spans={0}; if (workbook_chunks_begin(packet.bytes,storage_reserved,&spans)!=VERSION_CHUNKS_OK) goto done;
    p.phase=BROWSE_COMPONENT_CHUNKS;
    while (spans.next<spans.count) {
        uint64_t offset=0; size_t bytes=0; p.entry=(size_t)spans.next;
        if (stop(&p,spans.received,error) || !workbook_chunk_span(packet.bytes,spans.next,&offset,&bytes)) goto done;
        const unsigned char *data=(unsigned char *)packet.data+(size_t)offset;
        if (duckdb_bind_uint64(chunks,1,spans.next)!=DuckDBSuccess || duckdb_bind_blob(chunks,2,data,bytes)!=DuckDBSuccess || !run(chunks,error) || duckdb_bind_uint64(read,1,spans.next)!=DuckDBSuccess) goto done;
        duckdb_result r={0}; bool valid=duckdb_execute_prepared(read,&r)==DuckDBSuccess && duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0);
        if (valid) { duckdb_blob blob=duckdb_value_blob(&r,0,0); valid=blob.data && blob.size==bytes && !memcmp(blob.data,data,bytes) && workbook_chunks_offer(&spans,spans.next,blob.data,bytes)==VERSION_CHUNKS_OK; duckdb_free(blob.data); }
        duckdb_destroy_result(&r); if (!valid) { fail(error,"Preview chunk readback differs from its exact immutable payload."); goto done; }
    }
    if (workbook_chunks_finish(&spans)!=VERSION_CHUNKS_OK) goto done;
    p.phase=BROWSE_COMPONENT_RECHECK;
    if (!snapshot(publication,c,after,set,error)) goto done;
    status=verify(capture,&p,error); if (status!=BROWSE_COMPONENT_READY) goto done;
    status=verify(selected,&p,error); if (status!=BROWSE_COMPONENT_READY) goto done;
    unsigned char digest[DX_SOURCE_DIGEST_BYTES];
    hashed=source_digest_bytes(packet.data,packet.bytes,hash_cancel,&p,digest,detail);
    if (hashed!=SOURCE_DIGEST_OK || memcmp(digest,info.set_sha256,sizeof(digest))) { status=hashed==SOURCE_DIGEST_UNSUPPORTED ? BROWSE_COMPONENT_UNSUPPORTED : failed(&p); fail(error,"Preview payload hash changed during staging."); goto done; }
    ok=!stop(&p,packet.bytes,error);
 done:
    if (meta) duckdb_destroy_prepare(&meta); if (chunks) duckdb_destroy_prepare(&chunks); if (read) duckdb_destroy_prepare(&read); browse_preview_packet_free(&packet);
    if (!ok) return p.stopped ? BROWSE_COMPONENT_CANCELLED : status==BROWSE_COMPONENT_UNSUPPORTED ? status : BROWSE_COMPONENT_FAILED;
    *out=info; return BROWSE_COMPONENT_READY;
}

static bool shape(duckdb_connection c,char **error) {
    static const char *meta[]={"codec","container_encoding","core_encoding","set_encoding","scope","document_high","document_low","snapshot_revision","target_bytes","target_hash","set_bytes","set_hash"};
    static const char *types[]={"INTEGER","INTEGER","INTEGER","INTEGER","INTEGER","UBIGINT","UBIGINT","UBIGINT","UBIGINT","BLOB","UBIGINT","BLOB"};
    static const char *chunks[]={"position","data"},*chunk_types[]={"UBIGINT","BLOB"};
    duckdb_result r={0};
    bool ok=query(c,"SELECT CASE WHEN octet_length(encode(c.table_name))<=6 THEN c.table_name END,CASE WHEN octet_length(encode(c.column_name))<=18 THEN c.column_name END,CASE WHEN octet_length(encode(c.data_type))<=7 THEN c.data_type END,c.ordinal_position,CASE WHEN octet_length(encode(t.table_type))<=10 THEN t.table_type END FROM information_schema.columns c JOIN information_schema.tables t USING(table_catalog,table_schema,table_name) WHERE c.table_catalog=current_database() AND c.table_schema='_duxcel_previews' LIMIT 15",&r,error);
    unsigned seen[2]={0};
    if (ok && duckdb_row_count(&r)!=14) ok=false;
    for (idx_t row=0;ok && row<duckdb_row_count(&r);row++) {
        for (idx_t col=0;col<5;col++) if (duckdb_value_is_null(&r,col,row)) ok=false;
        char *table=duckdb_value_varchar(&r,0,row),*name=duckdb_value_varchar(&r,1,row),*type=duckdb_value_varchar(&r,2,row),*kind=duckdb_value_varchar(&r,4,row);
        uint64_t ordinal=duckdb_value_uint64(&r,3,row); unsigned side=table && !strcmp(table,"meta") ? 0 : table && !strcmp(table,"chunks") ? 1 : 2;
        size_t count=side ? 2 : 12; const char *const *names=side ? chunks : meta,*const *expected=side ? chunk_types : types;
        if (side>1 || !ordinal || ordinal>count || !name || !type || !kind || strcmp(kind,"BASE TABLE") || strcmp(name,names[ordinal-1]) || strcmp(type,expected[ordinal-1]) || (seen[side]&(1u<<(ordinal-1)))) ok=false;
        else seen[side]|=1u<<(ordinal-1);
        duckdb_free(table); duckdb_free(name); duckdb_free(type); duckdb_free(kind);
    }
    duckdb_destroy_result(&r); return (ok && seen[0]==4095u && seen[1]==3u) || fail(error,"Unsupported current preview component table/column shape.");
}
static bool hash_column(duckdb_result *r,idx_t column,unsigned char out[DX_SOURCE_DIGEST_BYTES]) {
    duckdb_blob blob=duckdb_value_blob(r,column,0); bool ok=blob.data && blob.size==DX_SOURCE_DIGEST_BYTES;
    if (ok) memcpy(out,blob.data,DX_SOURCE_DIGEST_BYTES); duckdb_free(blob.data); return ok;
}
BrowsePreviewComponentStatus browse_preview_component_probe(duckdb_connection c,uint64_t max_target_bytes,size_t max_set_bytes,BrowsePreviewComponentInfo *out,char **error) {
    if (!c || !out || !workbook_version_sql_transaction(c,error) || !shape(c,error)) return BROWSE_COMPONENT_FAILED;
    duckdb_result r={0}; BrowsePreviewComponentInfo info={0};
    bool ok=query(c,"SELECT codec,container_encoding,core_encoding,set_encoding,scope,document_high,document_low,snapshot_revision,target_bytes,CASE WHEN octet_length(target_hash)=32 THEN target_hash END,set_bytes,CASE WHEN octet_length(set_hash)=32 THEN set_hash END FROM _duxcel_previews.meta LIMIT 2",&r,error);
    if (ok && duckdb_row_count(&r)!=1) ok=false;
    for (idx_t i=0;ok && i<12;i++) if (duckdb_value_is_null(&r,i,0)) ok=false;
    if (ok) {
        ok=duckdb_value_int32(&r,0,0)==DX_BROWSE_PREVIEW_COMPONENT_ENCODING && duckdb_value_int32(&r,1,0)==16 && duckdb_value_int32(&r,2,0)==15 &&
            duckdb_value_int32(&r,3,0)==DX_BROWSE_PREVIEW_SET_ENCODING && duckdb_value_int32(&r,4,0)==BROWSE_PREVIEW_AUXILIARY_CURRENT_ARTIFACT;
        info.document=(WorkbookId){duckdb_value_uint64(&r,5,0),duckdb_value_uint64(&r,6,0)}; info.snapshot_revision=duckdb_value_uint64(&r,7,0);
        info.target_bytes=duckdb_value_uint64(&r,8,0); info.set_bytes=duckdb_value_uint64(&r,10,0);
        ok=ok && (info.document.high|info.document.low) && info.target_bytes && info.set_bytes>=DX_BROWSE_PREVIEW_SET_HEADER_BYTES && hash_column(&r,9,info.target_sha256) && hash_column(&r,11,info.set_sha256);
    }
    duckdb_destroy_result(&r);
    if (!ok) { fail(error,"Invalid current auxiliary preview descriptor."); return BROWSE_COMPONENT_FAILED; }
    if (info.target_bytes>max_target_bytes || info.set_bytes>max_set_bytes || info.set_bytes>SIZE_MAX) return BROWSE_COMPONENT_LIMIT;
    WorkbookChunks spans={0}; if (workbook_chunks_begin(info.set_bytes,info.set_bytes,&spans)!=VERSION_CHUNKS_OK || spans.count==UINT64_MAX) return BROWSE_COMPONENT_LIMIT;
    char *sql=dx_printf("SELECT count(*),coalesce(sum(octet_length(data)),0)=?::HUGEINT,coalesce(bool_or(position IS NULL OR data IS NULL OR octet_length(data)=0 OR octet_length(data)>65536),false) FROM (SELECT * FROM _duxcel_previews.chunks LIMIT %llu)",(unsigned long long)(spans.count+1));
    duckdb_prepared_statement statement=NULL; ok=prepare(c,sql,&statement,error); free(sql);
    if (ok) ok=duckdb_bind_uint64(statement,1,info.set_bytes)==DuckDBSuccess && duckdb_execute_prepared(statement,&r)==DuckDBSuccess && duckdb_row_count(&r)==1 &&
        !duckdb_value_is_null(&r,0,0) && !duckdb_value_is_null(&r,1,0) && !duckdb_value_is_null(&r,2,0) && duckdb_value_uint64(&r,0,0)==spans.count && duckdb_value_boolean(&r,1,0) && !duckdb_value_boolean(&r,2,0);
    duckdb_destroy_result(&r); if (statement) duckdb_destroy_prepare(&statement);
    if (!ok) { fail(error,"Invalid preview chunk inventory/counts/lengths/NULLs."); return BROWSE_COMPONENT_FAILED; }
    *out=info; return BROWSE_COMPONENT_READY;
}
static bool set_cancel(void *context,size_t entry) { Progress *p=context; p->phase=BROWSE_COMPONENT_ASSOCIATE; p->entry=entry; return stop(p,0,NULL); }
BrowsePreviewComponentStatus browse_preview_component_read(WorkbookArchiveReadScope *scope,duckdb_connection c,const Workbook *actual,
    const WorkbookVersionCoreSet *versions,size_t retained_limit,size_t wire_limit,size_t other_owned,size_t reserved,
    BrowsePreviewComponentControl control,BrowsePreviewSet **out,char **error) {
    if (!scope || !c || !actual || !versions || !out) return BROWSE_COMPONENT_FAILED;
    BrowsePreviewComponentInfo info={0}; BrowsePreviewComponentStatus status=browse_preview_component_probe(c,UINT64_MAX,wire_limit,&info,error);
    if (status!=BROWSE_COMPONENT_READY) return status;
    size_t base=other_owned;
    if (!add(&base,DX_BROWSE_PREVIEW_COMPONENT_IO_BYTES) || !add(&base,DX_ARCHIVE_SCOPE_COMPARE_BYTES) || !add(&base,(size_t)info.set_bytes) ||
        !add(&base,browse_preview_set_info(*out).owned) || base>reserved) return BROWSE_COMPONENT_LIMIT;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(versions); const WorkbookCorePayload *target=NULL;
    if (!index || workbook_version_index_check(index)!=VERSION_OK || info.document.high!=actual->id.high || info.document.low!=actual->id.low || info.snapshot_revision!=actual->command_revision) return BROWSE_COMPONENT_FAILED;
    for (size_t i=0;i<index->root_count;i++) if (!memcmp(index->roots[i].key.bytes,info.target_sha256,DX_VERSION_KEY_BYTES)) {
        if (index->roots[i].bytes!=info.target_bytes || index->roots[i].encoding!=15) return BROWSE_COMPONENT_FAILED;
        target=workbook_version_core_set_leaf(versions,i); break;
    }
    if (!target) { fail(error,"The auxiliary preview target is not an actual retained current-body candidate."); return BROWSE_COMPONENT_FAILED; }
    Progress p={.control=control,.phase=BROWSE_COMPONENT_VERIFY}; if (stop(&p,0,error)) return BROWSE_COMPONENT_CANCELLED;
    /* Only the actual read scope may turn this untrusted candidate selection
       into a complete current-body association. No automatic Saved lookup. */
    if (!workbook_archive_scope_associate(scope,c,actual,versions,target,DX_ARCHIVE_SCOPE_COMPARE_BYTES,error) || workbook_archive_scope_target(scope,c,actual,versions)!=target) return BROWSE_COMPONENT_FAILED;
    unsigned char *wire=dx_alloc((size_t)info.set_bytes); duckdb_prepared_statement statement=NULL; BrowsePreviewSet *candidate=NULL; bool ok=false;
    if (!prepare(c,"SELECT data FROM _duxcel_previews.chunks WHERE position=? LIMIT 2",&statement,error)) goto done;
    WorkbookChunks spans={0}; if (workbook_chunks_begin(info.set_bytes,info.set_bytes,&spans)!=VERSION_CHUNKS_OK) goto done;
    p.phase=BROWSE_COMPONENT_CHUNKS;
    while (spans.next<spans.count) {
        uint64_t offset=0; size_t bytes=0; p.entry=(size_t)spans.next;
        if (stop(&p,spans.received,error) || !workbook_chunk_span(info.set_bytes,spans.next,&offset,&bytes) || duckdb_bind_uint64(statement,1,spans.next)!=DuckDBSuccess) goto done;
        duckdb_result r={0}; bool valid=duckdb_execute_prepared(statement,&r)==DuckDBSuccess && duckdb_row_count(&r)==1 && !duckdb_value_is_null(&r,0,0);
        if (valid) { duckdb_blob blob=duckdb_value_blob(&r,0,0); valid=blob.data && blob.size==bytes && workbook_chunks_offer(&spans,spans.next,blob.data,bytes)==VERSION_CHUNKS_OK; if (valid) memcpy(wire+(size_t)offset,blob.data,bytes); duckdb_free(blob.data); }
        duckdb_destroy_result(&r); if (!valid) { fail(error,"Missing, duplicate, out-of-order or malformed preview chunk."); goto done; }
    }
    if (workbook_chunks_finish(&spans)!=VERSION_CHUNKS_OK) goto done;
    unsigned char digest[DX_SOURCE_DIGEST_BYTES]; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; p.phase=BROWSE_COMPONENT_VERIFY;
    SourceDigestStatus hashed=source_digest_bytes(wire,(size_t)info.set_bytes,hash_cancel,&p,digest,detail);
    if (hashed!=SOURCE_DIGEST_OK || memcmp(digest,info.set_sha256,sizeof(digest))) { status=hashed==SOURCE_DIGEST_UNSUPPORTED ? BROWSE_COMPONENT_UNSUPPORTED : BROWSE_COMPONENT_FAILED; fail(error,"Auxiliary preview payload hash mismatch or cancellation."); goto done; }
    BrowsePreviewSetWireInfo measured={0}; size_t charged=other_owned;
    if (!add(&charged,DX_BROWSE_PREVIEW_COMPONENT_IO_BYTES) || !add(&charged,DX_ARCHIVE_SCOPE_COMPARE_BYTES)) { status=BROWSE_COMPONENT_LIMIT; goto done; }
    BrowsePreviewSetStatus decoded=browse_preview_set_read_measure(wire,(size_t)info.set_bytes,actual,*out,retained_limit,charged,&measured);
    if (decoded!=BROWSE_PREVIEW_SET_OK || measured.coexistence>reserved) { status=decoded==BROWSE_PREVIEW_SET_LIMIT || (decoded==BROWSE_PREVIEW_SET_OK && measured.coexistence>reserved) ? BROWSE_COMPONENT_LIMIT : BROWSE_COMPONENT_FAILED; goto done; }
    decoded=browse_preview_set_decode(wire,(size_t)info.set_bytes,actual,retained_limit,charged,reserved,(BrowsePreviewSetControl){set_cancel,&p},&candidate);
    if (decoded!=BROWSE_PREVIEW_SET_OK) { status=decoded==BROWSE_PREVIEW_SET_LIMIT ? BROWSE_COMPONENT_LIMIT : decoded==BROWSE_PREVIEW_SET_UNSUPPORTED ? BROWSE_COMPONENT_UNSUPPORTED : BROWSE_COMPONENT_FAILED; goto done; }
    p.phase=BROWSE_COMPONENT_RECHECK;
    if (stop(&p,info.set_bytes,error) || workbook_archive_scope_target(scope,c,actual,versions)!=target || !workbook_archive_scope_recheck(scope,c,actual,versions,error)) goto done;
    ok=true;
 done:
    if (statement) duckdb_destroy_prepare(&statement); free(wire);
    if (!ok) { browse_preview_set_free(candidate); return p.stopped ? BROWSE_COMPONENT_CANCELLED : status==BROWSE_COMPONENT_LIMIT || status==BROWSE_COMPONENT_UNSUPPORTED ? status : BROWSE_COMPONENT_FAILED; }
    BrowsePreviewSet *old=*out; *out=candidate; browse_preview_set_free(old); return BROWSE_COMPONENT_READY;
}
