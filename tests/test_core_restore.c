#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/core_payload.h"
#include "platform/directory.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL core restore:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    FILE *file; uint64_t offset,fail_at; unsigned calls,seen;
    bool fail,flip,cancel; WorkbookCoreRestorePhase phase; uint64_t cancel_after;
} Input;
static bool read_block(void *context,uint64_t offset,void *buffer,size_t bytes,char **error) {
    (void)error; Input *in=context;
    CHECK(bytes && bytes<=DX_SOURCE_DIGEST_BUFFER_BYTES && offset==in->offset); in->calls++;
    if (in->fail && offset>=in->fail_at) return false;
    if (fread(buffer,1,bytes,in->file)!=bytes) return false;
    if (in->flip && !offset) ((unsigned char *)buffer)[0]^=1;
    in->offset+=bytes; return true;
}
static bool cancel(void *context,WorkbookCoreRestorePhase phase,uint64_t bytes) {
    Input *in=context; in->seen|=1u<<phase;
    return in->cancel && in->phase==phase && bytes>=in->cancel_after;
}
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static WorkbookCorePayloadResult restore(const char *path,const char *parent,WorkbookCorePayloadInfo expected,uint64_t reserved,Input *input,WorkbookCorePayload **out) {
    input->file=fopen(path,"rb"); CHECK(input->file); char *error=NULL;
    WorkbookCorePayloadResult result=workbook_core_payload_restore(&expected,parent,reserved,(WorkbookCorePayloadInput){read_block,cancel,input},out,&error);
    CHECK(!fclose(input->file)); input->file=NULL;
    if (result==CORE_PAYLOAD_READY) CHECK(!error && *out); else CHECK(error && !*out);
    free(error); return result;
}
static void mutate(const char *path,const char *sql) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,sql,&r)==DuckDBSuccess); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static WorkbookCorePayloadInfo rehash(const char *path,WorkbookCorePayloadInfo info) {
    SourceDigest digest={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(path,NULL,NULL,&digest,error)==SOURCE_DIGEST_OK); info.bytes=digest.bytes;
    memcpy(info.sha256,digest.sha256,DX_SOURCE_DIGEST_BYTES); return info;
}
int main(void) {
    char parent[]="build/tests/core-restore-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Restored core",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/data.parquet"},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"9007199254740993.0123456789",&error));
    WorkbookCorePayload *source=NULL,*out=NULL;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&source,&error)==CORE_PAYLOAD_READY && !error);
    const char *path=workbook_core_payload_path(source); WorkbookCorePayloadInfo expected=*workbook_core_payload_info(source);
    Input input={0}; CHECK(restore(path,parent,expected,expected.bytes,&input,&out)==CORE_PAYLOAD_READY);
    CHECK(input.seen==15 && input.offset==expected.bytes && input.calls>1 && children(parent)==2);
    CHECK(strcmp(path,workbook_core_payload_path(out)) && !memcmp(workbook_core_payload_info(out)->sha256,expected.sha256,DX_SOURCE_DIGEST_BYTES));
    Workbook loaded; workbook_init(&loaded); CHECK(workbook_load_core_payload(&loaded,workbook_core_payload_path(out),&error) && !error);
    CHECK(loaded.id.high==book.id.high && loaded.id.low==book.id.low && !strcmp(loaded.sheets[0].analysis.path,"/offline/data.parquet"));
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,50,30),"9007199254740993.0123456789")); workbook_free(&loaded);
    workbook_core_payload_free(out); out=NULL; CHECK(children(parent)==1);
    for (unsigned phase=0;phase<4;phase++) {
        input=(Input){.cancel=true,.phase=(WorkbookCoreRestorePhase)phase};
        CHECK(restore(path,parent,expected,expected.bytes,&input,&out)==CORE_PAYLOAD_CANCELLED && children(parent)==1);
    }
    input=(Input){.cancel=true,.phase=CORE_RESTORE_COPY,.cancel_after=DX_SOURCE_DIGEST_BUFFER_BYTES};
    CHECK(restore(path,parent,expected,expected.bytes,&input,&out)==CORE_PAYLOAD_CANCELLED && input.calls==1 && children(parent)==1);
    input=(Input){.fail=true,.fail_at=DX_SOURCE_DIGEST_BUFFER_BYTES};
    CHECK(restore(path,parent,expected,expected.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    input=(Input){.flip=true}; CHECK(restore(path,parent,expected,expected.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    WorkbookCorePayloadInfo wrong=expected; wrong.sha256[31]^=1; input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    wrong=expected; wrong.bytes--; input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    wrong=expected; wrong.document.low^=1; input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    input=(Input){0}; CHECK(restore(path,parent,expected,expected.bytes-1,&input,&out)==CORE_PAYLOAD_FAILED && !input.calls && children(parent)==1);
    wrong=expected; wrong.encoding=16; input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && !input.calls && children(parent)==1);
    /* Rehashing malicious bytes is not inner model/format/closure admission. */
    mutate(path,"UPDATE _duxcel_workspace.sheets SET path='relative.parquet'"); wrong=rehash(path,expected); input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    mutate(path,"UPDATE _duxcel_workspace.sheets SET path='/offline/data.parquet'; CREATE SCHEMA _duxcel_versions"); wrong=rehash(path,expected); input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    mutate(path,"DROP SCHEMA _duxcel_versions; CREATE TABLE unrelated AS SELECT 482 AS value"); wrong=rehash(path,expected); input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    mutate(path,"DROP TABLE unrelated; UPDATE _duxcel_workspace.meta SET version=14; DROP TABLE _duxcel_workspace.workbook_identity; DROP TABLE _duxcel_workspace.sheet_identity"); wrong=rehash(path,expected); input=(Input){0};
    CHECK(restore(path,parent,wrong,wrong.bytes,&input,&out)==CORE_PAYLOAD_FAILED && children(parent)==1);
    /* The test deliberately changed its original leaf; ordinary free keeps it. */
    char *owned_path=dx_strdup(path),*directory=dx_strdup(workbook_core_payload_directory(source)),*lease=dx_printf("%s/lease.lock",directory);
    workbook_core_payload_free(source); source=NULL;
    CHECK(!unlink(owned_path) && !unlink(lease) && !rmdir(directory)); free(owned_path); free(directory); free(lease);
#endif
    CHECK(workbook_core_payload_restore(NULL,parent,0,(WorkbookCorePayloadInput){0},&out,&error)==CORE_PAYLOAD_FAILED && !out && error);
    free(error); workbook_free(&book); CHECK(!children(parent) && !rmdir(parent));
    printf("PASS: %u strict streamed core restoration checks\n",checks); return 0;
}
