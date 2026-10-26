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
