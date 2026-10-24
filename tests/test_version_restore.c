#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_restore.h"
#include "workbook/version_stage.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version closure:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void exec(duckdb_connection c,const char *sql) {
    duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r);
    if (state!=DuckDBSuccess) fprintf(stderr,"SQL detail: %s\n",duckdb_result_error(&r));
    CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r);
}
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void reject(duckdb_connection c,const char *parent,const char *sql,uint64_t reserved) {
    size_t before=children(parent); exec(c,"BEGIN"); exec(c,sql);
    WorkbookVersionCoreSet *out=NULL; char *error=NULL;
    CHECK(workbook_version_core_set_read(c,parent,reserved,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_FAILED && !out);
    clear(&error); CHECK(children(parent)==before); exec(c,"ROLLBACK");
}
typedef struct { WorkbookVersionRestorePhase phase; size_t root; uint64_t after; unsigned calls; } Stop;
static bool cancel(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) {
    Stop *s=context; s->calls++; return phase==s->phase && root==s->root && bytes>=s->after;
}
static void values(const WorkbookVersionCoreSet *set,WorkbookId document) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(set);
    CHECK(index && index->root_count==2 && index->tag_count==2 && index->saved.present);
    CHECK(index->document_high==document.high && index->document_low==document.low && index->generation==UINT64_MAX);
    CHECK(!strcmp(index->tags[0].label.data,"İstanbul") && !memcmp(index->tags[0].root.bytes,index->tags[1].root.bytes,DX_VERSION_KEY_BYTES));
    for (size_t i=0;i<2;i++) {
        const WorkbookCorePayload *leaf=workbook_version_core_set_leaf(set,i); CHECK(leaf);
        CHECK(workbook_core_payload_verify(leaf,NULL,NULL,NULL)==CORE_PAYLOAD_READY);
        Workbook loaded; workbook_init(&loaded); char *error=NULL;
        CHECK(workbook_load_core_payload(&loaded,workbook_core_payload_path(leaf),&error) && !error);
        CHECK(loaded.id.high==document.high && loaded.id.low==document.low);
        CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,50,30),i ? "NULL" : "9007199254740993.0123456789"));
        CHECK(!strcmp(loaded.sheets[0].analysis.path,"/offline/source.parquet")); workbook_free(&loaded);
    }
    CHECK(!workbook_version_core_set_leaf(set,2));
}
int main(void) {
    char parent[]="build/tests/version-restore-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Closed versions",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/source.parquet"},&error));
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_config config=NULL;
    CHECK(duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"threads","1")==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"memory_limit","64MB")==DuckDBSuccess && duckdb_open_ext(NULL,&db,config,NULL)==DuckDBSuccess);
    duckdb_destroy_config(&config); CHECK(duckdb_connect(db,&c)==DuckDBSuccess);
    WorkbookVersionIndex index={.document_high=book.id.high,.document_low=book.id.low};
    WorkbookCorePayload *a=NULL,*b=NULL; const WorkbookCorePayload *leaves[2]={0}; WorkbookVersionRoot roots[2]={0}; WorkbookVersionTag tags[2]={0}; uint64_t total=0;
    WorkbookVersionCoreSet *out=NULL;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(cells_set(&book.sheets[0].cells,50,30,"9007199254740993.0123456789",&error));
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&a,&error)==CORE_PAYLOAD_READY);
    CHECK(cells_set(&book.sheets[0].cells,50,30,"NULL",&error)); book.command_revision=UINT64_MAX;
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&b,&error)==CORE_PAYLOAD_READY);
    leaves[0]=a; leaves[1]=b;
    for (size_t i=0;i<2;i++) { const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaves[i]); roots[i].bytes=info->bytes; roots[i].encoding=info->encoding; memcpy(roots[i].key.bytes,info->sha256,DX_VERSION_KEY_BYTES); total+=info->bytes; }
    tags[0]=(WorkbookVersionTag){{"İstanbul",strlen("İstanbul")},roots[0].key,1}; tags[1]=(WorkbookVersionTag){{"Alias",5},roots[0].key,2};
    index.generation=index.saved_generation=UINT64_MAX; index.saved=(WorkbookVersionRef){true,roots[1].key};
    index.roots=roots; index.root_count=2; index.tags=tags; index.tag_count=2;
#endif
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,index.root_count,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY && !error); exec(c,"COMMIT");
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(workbook_version_core_set_read(c,parent,total,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_FAILED && !out); clear(&error);
    exec(c,"BEGIN"); CHECK(workbook_version_core_set_read(c,parent,total,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_READY && out && !error);
    CHECK(children(parent)==4); values(out,book.id);
    WorkbookVersionCoreSet *same=out;
    CHECK(workbook_version_core_set_read(c,parent,total,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_FAILED && out==same); clear(&error);
    exec(c,"ROLLBACK"); values(out,book.id); workbook_version_core_set_free(out); out=NULL; CHECK(children(parent)==2);
    Stop stops[]={{VERSION_RESTORE_INDEX,0,0,0},{VERSION_RESTORE_COPY,0,0,0},{VERSION_RESTORE_COPY,0,65536,0},
        {VERSION_RESTORE_COPY,1,0,0},{VERSION_RESTORE_VERIFY,0,0,0},{VERSION_RESTORE_ADMIT,1,0,0},{VERSION_RESTORE_RECHECK,1,0,0}};
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        exec(c,"BEGIN");
        CHECK(workbook_version_core_set_read(c,parent,total,(WorkbookVersionRestoreControl){cancel,stops+i},&out,&error)==VERSION_RESTORE_CANCELLED && !out && stops[i].calls);
        clear(&error); CHECK(children(parent)==2); exec(c,"ROLLBACK");
    }
    const char *bad[]={
        "DELETE FROM _duxcel_versions.chunks",
        "DELETE FROM _duxcel_versions.chunks WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",
        "DELETE FROM _duxcel_versions.chunks WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0) AND position=0",
        "UPDATE _duxcel_versions.chunks SET data=NULL WHERE position=0",
        "UPDATE _duxcel_versions.chunks SET data=''::BLOB WHERE position=0",
        "UPDATE _duxcel_versions.chunks SET data=repeat('x',65537)::BLOB WHERE position=0",
        "UPDATE _duxcel_versions.chunks SET data=repeat('x',octet_length(data))::BLOB WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0) AND position=0",
        "UPDATE _duxcel_versions.chunks SET root=unhex(repeat('00',32)) WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",
        "UPDATE _duxcel_versions.chunks SET root=unhex(repeat('00',31)) WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",
        "CREATE OR REPLACE TABLE _duxcel_versions.chunks AS SELECT * FROM _duxcel_versions.chunks; UPDATE _duxcel_versions.chunks SET root=NULL WHERE position=0",
        "CREATE OR REPLACE TABLE _duxcel_versions.chunks AS SELECT * FROM _duxcel_versions.chunks; UPDATE _duxcel_versions.chunks SET position=NULL WHERE position=0",
        "CREATE OR REPLACE TABLE _duxcel_versions.chunks AS SELECT * FROM _duxcel_versions.chunks; UPDATE _duxcel_versions.chunks SET position=0 WHERE position=1 AND root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",
        "UPDATE _duxcel_versions.chunks SET position=18446744073709551615 WHERE position=0 AND root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",
        "INSERT INTO _duxcel_versions.roots VALUES (2,unhex(repeat('00',32)),1,15)",
        "UPDATE _duxcel_versions.roots SET bytes=bytes+1 WHERE position=0",
        "UPDATE _duxcel_versions.meta SET document_high=CASE WHEN document_high=1 THEN 2 ELSE 1 END",
        "DROP TABLE _duxcel_versions.chunks; CREATE VIEW _duxcel_versions.chunks AS SELECT NULL::BLOB AS root,0::UBIGINT AS position,NULL::BLOB AS data"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) reject(c,parent,bad[i],UINT64_MAX);
    reject(c,parent,"SELECT 1",total-1);
    /* Results remain independent after closing the outer source connection and
       releasing every original private source leaf. */
    exec(c,"BEGIN"); CHECK(workbook_version_core_set_read(c,parent,total,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_READY && out && !error); exec(c,"ROLLBACK");
    workbook_core_payload_free(a); a=NULL; workbook_core_payload_free(b); b=NULL;
    CHECK(children(parent)==2);
    duckdb_disconnect(&c); duckdb_close(&db); values(out,book.id); workbook_version_core_set_free(out); out=NULL;
    CHECK(!children(parent));
    CHECK(duckdb_open(NULL,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    index=(WorkbookVersionIndex){.document_high=book.id.high,.document_low=book.id.low};
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,NULL,0,0,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY && !error); exec(c,"COMMIT");
#endif
    exec(c,"BEGIN"); CHECK(workbook_version_core_set_read(c,parent,0,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_READY && out && !error);
    CHECK(!workbook_version_core_set_index(out)->root_count && !workbook_version_core_set_leaf(out,0));
    exec(c,"ROLLBACK"); workbook_version_core_set_free(out); out=NULL;
    CHECK(!workbook_version_core_set_index(NULL) && !workbook_version_core_set_leaf(NULL,0)); workbook_version_core_set_free(NULL);
    CHECK(workbook_version_core_set_read(NULL,parent,0,(WorkbookVersionRestoreControl){0},&out,&error)==VERSION_RESTORE_FAILED && !out); clear(&error);
    duckdb_disconnect(&c); duckdb_close(&db); workbook_free(&book);
    CHECK(!children(parent) && !rmdir(parent)); printf("PASS: %u complete version core closure checks\n",checks); return 0;
}
