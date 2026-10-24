#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_read.h"
#include "workbook/version_stage.h"
#include "workbook/version_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version metadata:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void exec(duckdb_connection c,const char *sql) {
    duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r);
    if (state!=DuckDBSuccess) fprintf(stderr,"SQL detail: %s\n",duckdb_result_error(&r));
    CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r);
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void reject(duckdb_connection c,const char *sql,uint64_t reserved) {
    exec(c,"BEGIN"); exec(c,sql); char *error=NULL; WorkbookVersionMetadata *out=NULL;
    CHECK(workbook_version_metadata_read(c,reserved,(WorkbookVersionReadControl){0},&out,&error)==VERSION_METADATA_FAILED && !out);
    clear(&error); exec(c,"ROLLBACK");
}
static void accepted(duckdb_connection c,const char *sql,uint64_t reserved) {
    exec(c,"BEGIN"); exec(c,sql); char *error=NULL; WorkbookVersionMetadata *out=NULL;
    CHECK(workbook_version_metadata_read(c,reserved,(WorkbookVersionReadControl){0},&out,&error)==VERSION_METADATA_READY && out && !error);
    CHECK(workbook_version_index_check(workbook_version_metadata_index(out))==VERSION_OK);
    workbook_version_metadata_free(out); exec(c,"ROLLBACK");
}
typedef struct { unsigned calls,stop; } Stop;
static bool cancel(void *context) { Stop *s=context; return ++s->calls>=s->stop; }
int main(void) {
    char parent[]="build/tests/version-read-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Index admission",&(Analysis){0},&error));
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_config config=NULL;
    CHECK(duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"memory_limit","64MB")==DuckDBSuccess && duckdb_open_ext(NULL,&db,config,NULL)==DuckDBSuccess);
    duckdb_destroy_config(&config); CHECK(duckdb_connect(db,&c)==DuckDBSuccess);
    WorkbookVersionIndex index={.document_high=book.id.high,.document_low=book.id.low};
    WorkbookCorePayload *a=NULL,*b=NULL; WorkbookVersionRoot roots[2]={0}; const WorkbookCorePayload *leaves[2]={0}; WorkbookVersionTag tags[2]={0}; uint64_t total=0;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(cells_set(&book.sheets[0].cells,0,0,"9007199254740993",&error));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&a,&error)==CORE_PAYLOAD_READY);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"NULL",&error));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&b,&error)==CORE_PAYLOAD_READY);
    leaves[0]=a; leaves[1]=b;
    for (size_t i=0;i<2;i++) { const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaves[i]); roots[i].bytes=info->bytes; roots[i].encoding=info->encoding; memcpy(roots[i].key.bytes,info->sha256,DX_VERSION_KEY_BYTES); total+=info->bytes; }
    tags[0]=(WorkbookVersionTag){{"İstanbul",strlen("İstanbul")},roots[0].key,1};
    tags[1]=(WorkbookVersionTag){{"Alias",5},roots[0].key,2};
    index.generation=index.saved_generation=UINT64_MAX; index.saved=(WorkbookVersionRef){true,roots[1].key};
    index.roots=roots; index.root_count=2; index.tags=tags; index.tag_count=2;
#endif
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,index.root_count,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY && !error);
    exec(c,"COMMIT");
    WorkbookVersionMetadata *out=NULL;
    CHECK(workbook_version_metadata_read(c,total,(WorkbookVersionReadControl){0},&out,&error)==VERSION_METADATA_FAILED && !out);
    clear(&error);
    exec(c,"BEGIN"); CHECK(workbook_version_metadata_read(c,total,(WorkbookVersionReadControl){0},&out,&error)==VERSION_METADATA_READY && out && !error);
    const WorkbookVersionIndex *read=workbook_version_metadata_index(out);
    CHECK(read->document_high==index.document_high && read->document_low==index.document_low && read->generation==index.generation && read->saved_generation==index.saved_generation);
    CHECK(read->root_count==index.root_count && read->tag_count==index.tag_count && read->saved.present==index.saved.present);
    for (size_t i=0;i<read->root_count;i++) CHECK(read->roots[i].bytes==roots[i].bytes && read->roots[i].encoding==roots[i].encoding && !memcmp(read->roots[i].key.bytes,roots[i].key.bytes,DX_VERSION_KEY_BYTES));
    for (size_t i=0;i<read->tag_count;i++) CHECK(read->tags[i].label.bytes==tags[i].label.bytes && !memcmp(read->tags[i].label.data,tags[i].label.data,tags[i].label.bytes) && read->tags[i].label.data!=tags[i].label.data);
    WorkbookVersionMetadata *same=out;
    CHECK(workbook_version_metadata_read(c,total,(WorkbookVersionReadControl){0},&out,&error)==VERSION_METADATA_FAILED && out==same); clear(&error);
    exec(c,"ROLLBACK");
    /* Metadata is owned independently of the connection/transaction results. */
    CHECK(workbook_version_index_check(read)==VERSION_OK); workbook_version_metadata_free(out); out=NULL;
    for (unsigned stop=1;stop<=18;stop++) {
        exec(c,"BEGIN"); Stop state={.stop=stop};
        WorkbookVersionReadResult result=workbook_version_metadata_read(c,total,(WorkbookVersionReadControl){cancel,&state},&out,&error);
        if (result==VERSION_METADATA_CANCELLED) { CHECK(!out && error); clear(&error); }
        else { CHECK(result==VERSION_METADATA_READY && out && !error); workbook_version_metadata_free(out); out=NULL; }
        exec(c,"ROLLBACK");
    }
    const char *bad_shape[]={
        "DROP TABLE _duxcel_versions.tags",
        "CREATE TABLE _duxcel_versions.unexpected(value INTEGER)",
        "DROP TABLE _duxcel_versions.chunks; CREATE VIEW _duxcel_versions.chunks AS SELECT NULL::BLOB AS root,0::UBIGINT AS position,NULL::BLOB AS data",
        "ALTER TABLE _duxcel_versions.meta ADD COLUMN unexpected INTEGER",
        "CREATE OR REPLACE TABLE _duxcel_versions.meta AS SELECT codec,document_high::VARCHAR AS document_high,document_low,generation,saved_generation,saved_present,saved_key FROM _duxcel_versions.meta",
        "CREATE OR REPLACE TABLE _duxcel_versions.chunks AS SELECT root,position,NULL::VARCHAR AS data FROM _duxcel_versions.chunks LIMIT 0",
        "UPDATE _duxcel_versions.meta SET codec=NULL", "UPDATE _duxcel_versions.meta SET codec=2",
        "UPDATE _duxcel_versions.meta SET document_high=NULL", "UPDATE _duxcel_versions.meta SET document_low=NULL",
        "UPDATE _duxcel_versions.meta SET generation=NULL", "UPDATE _duxcel_versions.meta SET saved_generation=NULL",
        "UPDATE _duxcel_versions.meta SET saved_present=NULL", "UPDATE _duxcel_versions.meta SET document_high=0,document_low=0",
        "INSERT INTO _duxcel_versions.meta SELECT * FROM _duxcel_versions.meta", "DELETE FROM _duxcel_versions.meta"};
    for (size_t i=0;i<sizeof(bad_shape)/sizeof(*bad_shape);i++) reject(c,bad_shape[i],total);
    accepted(c,"ALTER TABLE _duxcel_versions.roots RENAME COLUMN bytes TO BYTES",total);
    accepted(c,"CREATE OR REPLACE TABLE _duxcel_versions.meta AS SELECT saved_key,saved_present,saved_generation,generation,document_low,document_high,codec FROM _duxcel_versions.meta",total);
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    const char *bad_rows[]={
        "UPDATE _duxcel_versions.meta SET saved_key=NULL", "UPDATE _duxcel_versions.meta SET saved_key=''::BLOB",
        "UPDATE _duxcel_versions.meta SET saved_present=false", "UPDATE _duxcel_versions.meta SET saved_present=false,saved_key=NULL",
        "UPDATE _duxcel_versions.meta SET saved_generation=0", "UPDATE _duxcel_versions.meta SET generation=0",
        "UPDATE _duxcel_versions.meta SET generation=2,saved_generation=3",
        "UPDATE _duxcel_versions.roots SET bytes=0", "UPDATE _duxcel_versions.roots SET bytes=NULL",
        "UPDATE _duxcel_versions.roots SET encoding=16", "UPDATE _duxcel_versions.roots SET encoding=NULL",
        "UPDATE _duxcel_versions.roots SET key=NULL WHERE position=0",
        "UPDATE _duxcel_versions.roots SET key=''::BLOB WHERE position=0",
        "CREATE OR REPLACE TABLE _duxcel_versions.roots AS SELECT * FROM _duxcel_versions.roots; UPDATE _duxcel_versions.roots SET position=0",
        "CREATE OR REPLACE TABLE _duxcel_versions.roots AS SELECT * FROM _duxcel_versions.roots; UPDATE _duxcel_versions.roots SET key=(SELECT key FROM _duxcel_versions.roots WHERE position=1)",
        "CREATE OR REPLACE TABLE _duxcel_versions.roots AS SELECT * FROM _duxcel_versions.roots; UPDATE _duxcel_versions.roots SET position=NULL",
        "CREATE OR REPLACE TABLE _duxcel_versions.roots AS SELECT * FROM _duxcel_versions.roots; UPDATE _duxcel_versions.roots SET position=-1",
        "CREATE OR REPLACE TABLE _duxcel_versions.roots AS SELECT * FROM _duxcel_versions.roots; INSERT INTO _duxcel_versions.roots SELECT r.position+2,r.key,r.bytes,r.encoding FROM _duxcel_versions.roots r CROSS JOIN range(1024)",
        "UPDATE _duxcel_versions.tags SET label=NULL WHERE position=0", "UPDATE _duxcel_versions.tags SET label='' WHERE position=0",
        "UPDATE _duxcel_versions.tags SET label='   ' WHERE position=0", "UPDATE _duxcel_versions.tags SET label=repeat('x',257) WHERE position=0",
        "UPDATE _duxcel_versions.tags SET label='x'||chr(0)||'y' WHERE position=0", "UPDATE _duxcel_versions.tags SET label='x'||chr(10)||'y' WHERE position=0",
        "UPDATE _duxcel_versions.tags SET root=NULL WHERE position=0", "UPDATE _duxcel_versions.tags SET root=''::BLOB WHERE position=0",
        "UPDATE _duxcel_versions.tags SET root=unhex(repeat('00',32)) WHERE position=0",
        "UPDATE _duxcel_versions.tags SET created=NULL WHERE position=0", "UPDATE _duxcel_versions.tags SET created=0 WHERE position=0",
        "UPDATE _duxcel_versions.tags SET created=1", "UPDATE _duxcel_versions.meta SET saved_generation=1",
        "CREATE OR REPLACE TABLE _duxcel_versions.tags AS SELECT * FROM _duxcel_versions.tags; UPDATE _duxcel_versions.tags SET label='same'",
        "CREATE OR REPLACE TABLE _duxcel_versions.tags AS SELECT * FROM _duxcel_versions.tags; UPDATE _duxcel_versions.tags SET position=0",
        "CREATE OR REPLACE TABLE _duxcel_versions.tags AS SELECT * FROM _duxcel_versions.tags; INSERT INTO _duxcel_versions.tags SELECT t.position+2,t.label,t.root,t.created FROM _duxcel_versions.tags t CROSS JOIN range(1024)"};
    for (size_t i=0;i<sizeof(bad_rows)/sizeof(*bad_rows);i++) reject(c,bad_rows[i],total);
    reject(c,"SELECT 1",total-1);
    reject(c,"UPDATE _duxcel_versions.roots SET bytes=18446744073709551615",UINT64_MAX);
    accepted(c,"UPDATE _duxcel_versions.tags SET root=unhex(repeat('00',32)); UPDATE _duxcel_versions.roots SET key=unhex(repeat('00',32)) WHERE position=0",total);
    /* Framing/content is explicitly NOT admitted by this metadata-only API. */
    accepted(c,"DELETE FROM _duxcel_versions.chunks",total);
#endif
    CHECK(!workbook_version_metadata_index(NULL)); workbook_version_metadata_free(NULL);
    duckdb_disconnect(&c); duckdb_close(&db); workbook_core_payload_free(a); workbook_core_payload_free(b); workbook_free(&book);
    CHECK(!rmdir(parent)); printf("PASS: %u bounded version metadata checks\n",checks); return 0;
}
