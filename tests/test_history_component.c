#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/history_component.h"
#include "workbook/version_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history SQL component:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
#include "support/history_native_fixture.h"
static WorkbookHistoryProfileCaps caps(void) { return (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES}; }
static WorkbookHistoryNativeCaps native_caps(void) { return (WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u}; }
static void exec(duckdb_connection c,const char *sql) { duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r); if (state!=DuckDBSuccess) fprintf(stderr,"SQL: %s\n",duckdb_result_error(&r)); CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r); }
static uint64_t schemas(duckdb_connection c) { duckdb_result r={0}; CHECK(duckdb_query(c,"SELECT count(*) FROM information_schema.schemata WHERE catalog_name=current_database() AND schema_name='_duxcel_history'",&r)==DuckDBSuccess); uint64_t n=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); return n; }
static bool stop(void *context,WorkbookHistoryComponentPhase phase,unsigned asset,uint64_t position,uint64_t bytes) { (void)position; (void)bytes; return phase==*(WorkbookHistoryComponentPhase *)context && ((phase!=HISTORY_COMPONENT_CHUNKS && phase!=HISTORY_COMPONENT_ADMIT && phase!=HISTORY_COMPONENT_RECHECK) || asset==DX_HISTORY_COMPONENT_PROFILE); }
static bool stop_hash(void *context,uint64_t bytes) { (void)context; (void)bytes; return true; }
static void rejected(duckdb_connection c,const WorkbookVersionCoreSet *roots,WorkbookHistoryProfileBinding binding,const char *parent,WorkbookHistoryProfileCaps profile,WorkbookHistoryNativeCaps native,
    void *wire,size_t wire_bytes,void *arena,size_t arena_bytes,WorkbookHistoryComponentControl control,WorkbookHistoryNativeCapture *prior,WorkbookHistoryComponentStatus expected) {
    WorkbookHistoryNativeCapture *out=prior; char *error=NULL;
    CHECK(workbook_history_component_read(c,roots,binding,parent,profile,native,wire,wire_bytes,arena,arena_bytes,control,&out,&error)==expected && out==prior); free(error);
}
static void corrupt_packet(duckdb_connection c) {
    duckdb_result r={0}; CHECK(duckdb_query(c,"SELECT data FROM _duxcel_history.chunks WHERE asset=1 AND position=0",&r)==DuckDBSuccess && duckdb_row_count(&r)==1);
    duckdb_blob blob=duckdb_value_blob(&r,0,0); CHECK(blob.data && blob.size>64); ((unsigned char *)blob.data)[63]^=1;
    duckdb_prepared_statement s=NULL; CHECK(duckdb_prepare(c,"UPDATE _duxcel_history.chunks SET data=? WHERE asset=1 AND position=0",&s)==DuckDBSuccess); duckdb_bind_blob(s,1,blob.data,blob.size);
    duckdb_result changed={0}; CHECK(duckdb_execute_prepared(s,&changed)==DuckDBSuccess); duckdb_destroy_result(&changed); duckdb_destroy_prepare(&s); duckdb_free(blob.data); duckdb_destroy_result(&r);
}
int main(void) {
    char parent[]="build/tests/history-component-XXXXXX"; CHECK(mkdtemp(parent)); Workbook book; char *path=NULL,*error=NULL;
    WorkbookHistoryNativeCapture *captured=native_fixture(parent,&book,&path,&error),*out=NULL;
    const WorkbookVersionCoreSet *roots=workbook_history_native_roots(captured); const WorkbookCorePayloadInfo *original=workbook_core_payload_info(workbook_history_native_head(captured));
    SourceDigest baseline={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&baseline,detail)==SOURCE_DIGEST_OK);
    size_t bytes=0; CHECK(workbook_history_profile_encoded_size(workbook_history_native_profile(captured),&bytes)==HISTORY_PROFILE_CODEC_OK);
    void *wire=dx_alloc(bytes),*parse=dx_alloc(DX_HISTORY_PROFILE_PARSE_BYTES); size_t staging=bytes+DX_HISTORY_COMPONENT_IO_BYTES; uint64_t storage=original->bytes+bytes;
    unsigned char digest[32],known[]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    CHECK(source_digest_bytes("abc",3,NULL,NULL,digest,detail)==SOURCE_DIGEST_OK && !memcmp(digest,known,32));
    CHECK(source_digest_bytes("abc",3,stop_hash,NULL,digest,detail)==SOURCE_DIGEST_CANCELLED && !memcmp(digest,known,32));
    CHECK(source_digest_bytes(NULL,1,NULL,NULL,digest,detail)==SOURCE_DIGEST_ERROR && !memcmp(digest,known,32));
    duckdb_database database=NULL; duckdb_connection c=NULL; CHECK(duckdb_open(NULL,&database)==DuckDBSuccess && duckdb_connect(database,&c)==DuckDBSuccess);
    CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_FAILED && !schemas(c));
    exec(c,"BEGIN");
    CHECK(workbook_history_component_stage(c,captured,wire,bytes-1,staging,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_LIMIT && !schemas(c));
    CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging-1,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_LIMIT && !schemas(c));
    CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging,storage-1,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_LIMIT && !schemas(c));
    WorkbookHistoryComponentPhase phase=HISTORY_COMPONENT_CHUNKS;
    CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging,storage,(WorkbookHistoryComponentControl){stop,&phase},&error)==HISTORY_COMPONENT_CANCELLED && schemas(c)==1);
    exec(c,"ROLLBACK"); CHECK(!schemas(c));
    /* Source/enrichment/R+1 can advance the live model. The component consumes
       the SAME leased R body, with its old Saved index still prepublication. */
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,2,3,&target,&error)); CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"Live R+1",8};
    CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){.source_ready=true},&error).status==CELL_EDIT_APPLIED && book.command_revision==2);
    /* Successful admission owns exact copies even when the transaction owner
       deliberately rolls back the unpublished SQL component afterward. */
    exec(c,"BEGIN"); CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_READY);
    WorkbookHistoryNativeCapture *rolled_back=NULL; WorkbookHistoryProfileBinding rollback_binding={{book.id.high,book.id.low,120,VERSION_WORKING_HEAD},121};
    CHECK(workbook_history_component_read(c,roots,rollback_binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},&rolled_back,&error)==HISTORY_COMPONENT_READY);
    exec(c,"ROLLBACK"); CHECK(!schemas(c));
    CHECK(workbook_history_native_position(rolled_back)->revision==1 && !memcmp(workbook_core_payload_info(workbook_history_native_head(rolled_back))->sha256,original->sha256,32));
    CHECK(workbook_core_payload_verify(workbook_history_native_head(rolled_back),NULL,NULL,&error)==CORE_PAYLOAD_READY); workbook_history_native_free(&rolled_back);
    exec(c,"BEGIN"); CHECK(workbook_history_component_stage(c,captured,wire,bytes,staging,storage,(WorkbookHistoryComponentControl){0},&error)==HISTORY_COMPONENT_READY);
    WorkbookHistoryComponentInfo info={.revision=77},before=info;
    CHECK(workbook_history_component_probe(c,original->bytes-1,&info,&error)==HISTORY_COMPONENT_LIMIT && !memcmp(&info,&before,sizeof(info)));
    CHECK(workbook_history_component_probe(c,native_caps().physical_bytes,&info,&error)==HISTORY_COMPONENT_READY && info.kind==HISTORY_PROFILE_RETAINED && info.revision==1 && info.packet_bytes==bytes && !memcmp(info.head_sha256,original->sha256,32));
    exec(c,"COMMIT");
    WorkbookHistoryProfileBinding binding={{book.id.high,book.id.low,90,VERSION_WORKING_HEAD},91};
    exec(c,"BEGIN");
    WorkbookHistoryProfileCaps pressure=caps(); pressure.staging_bytes=0;
    rejected(c,roots,binding,parent,pressure,native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},NULL,HISTORY_COMPONENT_LIMIT);
    WorkbookHistoryNativeCaps physical=native_caps(); physical.physical_bytes=1;
    rejected(c,roots,binding,parent,caps(),physical,wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},NULL,HISTORY_COMPONENT_LIMIT);
    CHECK(workbook_history_component_read(c,roots,binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},&out,&error)==HISTORY_COMPONENT_READY);
    CHECK(workbook_history_native_position(out)->revision==1 && workbook_history_native_position(out)->owner.session==90 && !memcmp(workbook_core_payload_info(workbook_history_native_head(out))->sha256,original->sha256,32));
    exec(c,"COMMIT");
    Workbook restored; workbook_init(&restored); CHECK(workbook_load_core_payload(&restored,workbook_core_payload_path(workbook_history_native_head(out)),&error) && restored.command_revision==1 && !strcmp(cells_get(&restored.sheets[0].cells,2,3),"Captured R")); workbook_free(&restored);
    const char *changes[]={"UPDATE _duxcel_history.meta SET codec=2","ALTER TABLE _duxcel_history.meta ADD COLUMN extra INTEGER","UPDATE _duxcel_history.meta SET head_revision=2","UPDATE _duxcel_history.meta SET kind=0","DELETE FROM _duxcel_history.chunks WHERE asset=1","UPDATE _duxcel_history.chunks SET position=0 WHERE asset=0 AND position=1","UPDATE _duxcel_history.meta SET packet_hash=NULL"};
    for (size_t i=0;i<sizeof(changes)/sizeof(*changes);i++) { exec(c,"BEGIN"); exec(c,changes[i]); rejected(c,roots,binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},out,HISTORY_COMPONENT_FAILED); exec(c,"ROLLBACK"); }
    exec(c,"BEGIN"); corrupt_packet(c); CHECK(workbook_history_component_probe(c,native_caps().physical_bytes,&info,&error)==HISTORY_COMPONENT_READY);
    rejected(c,roots,binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},out,HISTORY_COMPONENT_FAILED); exec(c,"ROLLBACK");
    exec(c,"BEGIN"); phase=HISTORY_COMPONENT_RECHECK;
    rejected(c,roots,binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){stop,&phase},out,HISTORY_COMPONENT_CANCELLED);
    physical=native_caps(); physical.additional_bytes=1;
    rejected(c,roots,binding,parent,caps(),physical,wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},out,HISTORY_COMPONENT_LIMIT);
    CHECK(workbook_history_component_read(c,roots,binding,parent,caps(),native_caps(),wire,bytes,parse,DX_HISTORY_PROFILE_PARSE_BYTES,(WorkbookHistoryComponentControl){0},&out,&error)==HISTORY_COMPONENT_READY); exec(c,"COMMIT");
    SourceDigest after={0}; CHECK(source_digest_file(path,NULL,NULL,&after,detail)==SOURCE_DIGEST_OK && !memcmp(after.sha256,baseline.sha256,32) && source_file_identity_same(&after.identity,&baseline.identity));
    workbook_history_native_free(&captured); workbook_free(&book); duckdb_disconnect(&c); duckdb_close(&database); memset(wire,0,bytes); memset(parse,0,DX_HISTORY_PROFILE_PARSE_BYTES); free(wire); free(parse);
    CHECK(workbook_history_profile_view(workbook_history_native_profile(out))->count==1 && workbook_history_native_position(out)->revision==1);
    CHECK(workbook_core_payload_verify(workbook_history_native_head(out),NULL,NULL,&error)==CORE_PAYLOAD_READY);
    workbook_history_native_free(&out); free(error); CHECK(remove(path)==0 && rmdir(parent)==0); free(path);
    printf("PASS: %u unpublished current history SQL component checks\n",checks); return 0;
}
