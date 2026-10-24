#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_stage.h"
#include "workbook/version_chunks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version staging:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void exec(duckdb_connection c,const char *sql) {
    duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r);
    if (state!=DuckDBSuccess) fprintf(stderr,"SQL detail: %s\n",duckdb_result_error(&r));
    CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r);
}
static uint64_t scalar(duckdb_connection c,const char *sql) {
    duckdb_result r={0}; CHECK(duckdb_query(c,sql,&r)==DuckDBSuccess && duckdb_row_count(&r)==1);
    uint64_t out=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); return out;
}
static bool absent(duckdb_connection c) { return scalar(c,"SELECT count(*) FROM information_schema.schemata WHERE schema_name='_duxcel_versions'")==0; }
static WorkbookCorePayload *capture(const Workbook *book,const char *parent) {
    WorkbookCorePayload *p=NULL; char *error=NULL;
    CHECK(workbook_core_payload_capture(book,parent,(WorkbookCorePayloadControl){0},&p,&error)==CORE_PAYLOAD_READY && !error && p); return p;
}
static WorkbookVersionRoot root(const WorkbookCorePayload *p) {
    const WorkbookCorePayloadInfo *i=workbook_core_payload_info(p);
    WorkbookVersionRoot r={.bytes=i->bytes,.encoding=i->encoding}; memcpy(r.key.bytes,i->sha256,DX_VERSION_KEY_BYTES); return r;
}
static void bytes(duckdb_connection c,const WorkbookCorePayload *p,WorkbookVersionRoot r) {
    FILE *file=fopen(workbook_core_payload_path(p),"rb"); CHECK(file);
    duckdb_prepared_statement statement=NULL;
    CHECK(duckdb_prepare(c,"SELECT data FROM _duxcel_versions.chunks WHERE root=? AND position=?",&statement)==DuckDBSuccess);
    WorkbookChunks chunks={0}; CHECK(workbook_chunks_begin(r.bytes,r.bytes,&chunks)==VERSION_CHUNKS_OK);
    unsigned char buffer[DX_VERSION_CHUNK_BYTES];
    while (chunks.next<chunks.count) {
        duckdb_bind_blob(statement,1,r.key.bytes,DX_VERSION_KEY_BYTES); duckdb_bind_uint64(statement,2,chunks.next);
        duckdb_result result={0}; CHECK(duckdb_execute_prepared(statement,&result)==DuckDBSuccess && duckdb_row_count(&result)==1);
        duckdb_blob blob=duckdb_value_blob(&result,0,0); CHECK(blob.data && blob.size<=sizeof(buffer));
        CHECK(workbook_chunks_offer(&chunks,chunks.next,blob.data,blob.size)==VERSION_CHUNKS_OK);
        CHECK(fread(buffer,1,blob.size,file)==blob.size && !memcmp(buffer,blob.data,blob.size));
        duckdb_free(blob.data); duckdb_destroy_result(&result);
    }
    CHECK(workbook_chunks_finish(&chunks)==VERSION_CHUNKS_OK && fgetc(file)==EOF && !ferror(file));
    CHECK(!fclose(file)); duckdb_destroy_prepare(&statement);
}
typedef struct { WorkbookVersionStagePhase phase; size_t root; uint64_t after; unsigned calls; } Stop;
static bool cancel(void *context,WorkbookVersionStagePhase phase,size_t root,uint64_t bytes) {
    Stop *s=context; s->calls++; return phase==s->phase && root==s->root && bytes>=s->after;
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
int main(void) {
    char parent[]="build/tests/version-stage-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Version stage",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"9007199254740993.0123456789",&error));
    duckdb_database db=NULL; duckdb_connection c=NULL,observer=NULL; duckdb_config config=NULL;
    CHECK(duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"memory_limit","64MB")==DuckDBSuccess && duckdb_open_ext(NULL,&db,config,NULL)==DuckDBSuccess);
    duckdb_destroy_config(&config); CHECK(duckdb_connect(db,&c)==DuckDBSuccess && duckdb_connect(db,&observer)==DuckDBSuccess);
    exec(c,"CREATE TABLE marker AS SELECT 482 AS value");
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    WorkbookCorePayload *a=capture(&book,parent);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"NULL",&error)); book.command_revision=UINT64_MAX;
    WorkbookCorePayload *b=capture(&book,parent); const WorkbookCorePayload *leaves[]={a,b};
    WorkbookVersionRoot roots[]={root(a),root(b)};
    const char *label="İstanbul 'baseline'";
    WorkbookVersionTag tags[]={{{label,strlen(label)},roots[0].key,1},{{"Same payload",12},roots[0].key,2}};
    WorkbookVersionIndex index={.document_high=book.id.high,.document_low=book.id.low,.generation=3,.saved_generation=3,
        .saved={true,roots[1].key},.roots=roots,.root_count=2,.tags=tags,.tag_count=2};
    uint64_t total=roots[0].bytes+roots[1].bytes;
    CHECK(workbook_version_index_check(&index)==VERSION_OK);
    CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    CHECK(strstr(error,"explicit transaction")); clear(&error); CHECK(absent(c) && scalar(c,"SELECT value FROM marker")==482);
    exec(c,"BEGIN");
    CHECK(workbook_version_stage(c,&index,leaves,2,total-1,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    clear(&error); CHECK(absent(c)); exec(c,"ROLLBACK");
    WorkbookVersionRoot wrong=roots[0]; roots[0].bytes++;
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,2,total+1,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    clear(&error); CHECK(absent(c)); exec(c,"ROLLBACK"); roots[0]=wrong;
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,1,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    clear(&error); CHECK(absent(c)); exec(c,"ROLLBACK");
    Stop stops[]={ {VERSION_STAGE_VERIFY,0,0,0},{VERSION_STAGE_CHUNKS,0,0,0},
        {VERSION_STAGE_CHUNKS,0,DX_VERSION_CHUNK_BYTES,0},{VERSION_STAGE_CHUNKS,1,0,0},
        {VERSION_STAGE_RECHECK,0,0,0},{VERSION_STAGE_RECHECK,1,0,0} };
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        exec(c,"BEGIN");
        CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){cancel,stops+i},&error)==VERSION_STAGE_CANCELLED && stops[i].calls);
        clear(&error); CHECK(absent(observer)); exec(c,"ROLLBACK");
        CHECK(absent(c) && scalar(c,"SELECT value FROM marker")==482);
    }
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY && !error);
    CHECK(absent(observer)); /* READY is still private, never a helper COMMIT. */
    CHECK(scalar(c,"SELECT count(*) FROM _duxcel_versions.meta")==1);
    CHECK(scalar(c,"SELECT codec FROM _duxcel_versions.meta")==1 && scalar(c,"SELECT generation FROM _duxcel_versions.meta")==3);
    CHECK(scalar(c,"SELECT count(*) FROM _duxcel_versions.roots")==2 && scalar(c,"SELECT count(*) FROM _duxcel_versions.tags")==2);
    CHECK(scalar(c,"SELECT sum(octet_length(data)) FROM _duxcel_versions.chunks")==total);
    bytes(c,a,roots[0]); bytes(c,b,roots[1]);
    duckdb_result r={0}; CHECK(duckdb_query(c,"SELECT label,root,created FROM _duxcel_versions.tags ORDER BY position",&r)==DuckDBSuccess && duckdb_row_count(&r)==2);
    char *text=duckdb_value_varchar(&r,0,0); CHECK(text && !strcmp(text,label)); duckdb_free(text);
    for (idx_t row=0;row<2;row++) { duckdb_blob blob=duckdb_value_blob(&r,1,row); CHECK(blob.size==DX_VERSION_KEY_BYTES && !memcmp(blob.data,roots[0].key.bytes,DX_VERSION_KEY_BYTES)); duckdb_free(blob.data); }
    duckdb_destroy_result(&r); exec(c,"COMMIT"); CHECK(!absent(observer));
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    CHECK(strstr(error,"fresh reserved schema")); clear(&error); exec(c,"ROLLBACK");
    CHECK(scalar(c,"SELECT count(*) FROM _duxcel_versions.roots")==2 && scalar(c,"SELECT value FROM marker")==482);
    exec(c,"DROP SCHEMA _duxcel_versions CASCADE");
    /* An unrelated object under the reserved schema is never overwritten. */
    exec(c,"CREATE SCHEMA _duxcel_versions; CREATE TABLE _duxcel_versions.keep AS SELECT 482 AS value");
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED);
    clear(&error); exec(c,"ROLLBACK"); CHECK(scalar(c,"SELECT value FROM _duxcel_versions.keep")==482);
    exec(c,"DROP SCHEMA _duxcel_versions CASCADE");
    CHECK(workbook_core_payload_verify(a,NULL,NULL,&error)==CORE_PAYLOAD_READY && workbook_core_payload_verify(b,NULL,NULL,&error)==CORE_PAYLOAD_READY);
    workbook_core_payload_free(a); workbook_core_payload_free(b);
#endif
    WorkbookVersionIndex empty={.document_high=book.id.high,.document_low=book.id.low};
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&empty,NULL,0,0,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY && !error);
    CHECK(scalar(c,"SELECT count(*) FROM _duxcel_versions.roots")==0 && scalar(c,"SELECT count(*) FROM _duxcel_versions.chunks")==0);
    CHECK(scalar(c,"SELECT count(*) FROM _duxcel_versions.meta WHERE NOT saved_present AND saved_key IS NULL")==1);
    exec(c,"ROLLBACK"); CHECK(absent(c));
    CHECK(workbook_version_stage(NULL,&empty,NULL,0,0,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_FAILED); clear(&error);
    duckdb_disconnect(&observer); duckdb_disconnect(&c); duckdb_close(&db);
    workbook_free(&book); CHECK(!rmdir(parent));
    printf("PASS: %u transactional version staging checks\n",checks); return 0;
}
