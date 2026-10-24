#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/archive_read.h"
#include "workbook/version_stage.h"
#include "platform/platform.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL archive read:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void exec(duckdb_connection c,const char *sql) {
    duckdb_result r={0}; duckdb_state state=duckdb_query(c,sql,&r);
    if (state!=DuckDBSuccess) fprintf(stderr,"SQL detail: %s\n",duckdb_result_error(&r));
    CHECK(state==DuckDBSuccess); duckdb_destroy_result(&r);
}
static void mutate(const char *path,const char *sql) {
    duckdb_database db=NULL; duckdb_connection c=NULL;
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    exec(c,sql); duckdb_disconnect(&c); duckdb_close(&db);
}
static SourceDigest digest(const char *path) {
    SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d;
}
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
typedef struct { WorkbookArchiveReadPhase phase; size_t root; unsigned calls; } Stop;
static bool cancel(void *context,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes) {
    (void)bytes; Stop *s=context; s->calls++; return phase==s->phase && root==s->root;
}
static void unchanged(const char *path,SourceDigest before) {
    SourceDigest after=digest(path);
    CHECK(after.bytes==before.bytes && !memcmp(before.sha256,after.sha256,DX_SOURCE_DIGEST_BYTES));
    CHECK(source_file_identity_same(&before.identity,&after.identity));
}
int main(void) {
    char parent[]="build/tests/archive-read-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/versioned.dxls",parent),*unwrapped=dx_printf("%s/unwrapped-core.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Authored head",&(Analysis){.kind=SOURCE_PARQUET,.path="/offline/source.parquet"},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"baseline",&error));
    WorkbookArchiveCandidate *out=NULL; WorkbookCorePayload *a=NULL,*b=NULL;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&a,&error)==CORE_PAYLOAD_READY);
    CHECK(cells_set(&book.sheets[0].cells,50,30,"saved R",&error)); book.command_revision=5;
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&b,&error)==CORE_PAYLOAD_READY);
    const WorkbookCorePayload *leaves[]={a,b}; WorkbookVersionRoot roots[2]={0}; uint64_t total=0;
    for (size_t i=0;i<2;i++) { const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaves[i]); roots[i].bytes=info->bytes; roots[i].encoding=info->encoding; memcpy(roots[i].key.bytes,info->sha256,DX_VERSION_KEY_BYTES); total+=info->bytes; }
    WorkbookVersionTag tag={{"Baseline",8},roots[0].key,1};
    WorkbookVersionIndex index={.document_high=book.id.high,.document_low=book.id.low,.generation=2,.saved_generation=2,.saved={true,roots[1].key},.roots=roots,.root_count=2,.tags=&tag,.tag_count=1};
    CHECK(cells_set(&book.sheets[0].cells,50,30,"checkpoint R+1",&error)); book.command_revision=6;
    CHECK(workbook_save_core_payload(&book,path,(WorkbookSaveControl){0},&error)==WORKBOOK_SAVE_COMPLETE);
    CHECK(workbook_save(&book,unwrapped,&error));
    duckdb_database db=NULL; duckdb_connection c=NULL;
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    exec(c,"BEGIN"); CHECK(workbook_version_stage(c,&index,leaves,2,total,(WorkbookVersionStageControl){0},&error)==VERSION_STAGE_READY);
    exec(c,"UPDATE _duxcel_workspace.meta SET version=16; COMMIT"); duckdb_disconnect(&c); duckdb_close(&db);
    size_t baseline=children(parent); SourceDigest before=digest(path);
    CHECK(workbook_archive_read(path,parent,total,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_READY && out && !error);
    CHECK(children(parent)==baseline+2 && workbook_archive_file_version(out)==16);
    const Workbook *head=workbook_archive_book(out); const WorkbookVersionCoreSet *versions=workbook_archive_versions(out);
    CHECK(head && head->id.high==book.id.high && head->id.low==book.id.low && head->command_revision==6 && !head->dirty);
    CHECK(!strcmp(cells_get(&head->sheets[0].cells,50,30),"checkpoint R+1"));
    const WorkbookVersionIndex *loaded=workbook_version_core_set_index(versions);
    CHECK(loaded && loaded->root_count==2 && loaded->tag_count==1 && !memcmp(loaded->saved.key.bytes,roots[1].key.bytes,DX_VERSION_KEY_BYTES));
    Workbook saved; workbook_init(&saved);
    CHECK(workbook_load_core_payload(&saved,workbook_core_payload_path(workbook_version_core_set_leaf(versions,1)),&error));
    CHECK(saved.command_revision==5 && !strcmp(cells_get(&saved.sheets[0].cells,50,30),"saved R")); workbook_free(&saved);
    CHECK(source_file_identity_same(workbook_archive_identity(out),&before.identity)); unchanged(path,before);
    WorkbookArchiveCandidate *same=out;
    CHECK(workbook_archive_read(path,parent,total,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_FAILED && out==same); clear(&error);
    Workbook *taken=workbook_archive_take_book(out); WorkbookVersionCoreSet *taken_versions=workbook_archive_take_versions(out);
    CHECK(taken && taken_versions && !workbook_archive_book(out) && !workbook_archive_versions(out)); workbook_archive_candidate_free(out); out=NULL;
    CHECK(!strcmp(cells_get(&taken->sheets[0].cells,50,30),"checkpoint R+1")); workbook_free(taken); free(taken); workbook_version_core_set_free(taken_versions);
    CHECK(children(parent)==baseline);
    Stop stops[]={{ARCHIVE_READ_HEADER,0,0},{ARCHIVE_READ_HEAD,0,0},{ARCHIVE_READ_INDEX,0,0},{ARCHIVE_READ_COPY,1,0},{ARCHIVE_READ_VERIFY,0,0},{ARCHIVE_READ_ADMIT,1,0},{ARCHIVE_READ_RECHECK,1,0}};
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        CHECK(workbook_archive_read(path,parent,total,(WorkbookArchiveReadControl){cancel,stops+i},&out,&error)==ARCHIVE_READ_CANCELLED && !out && stops[i].calls);
        clear(&error); CHECK(children(parent)==baseline); unchanged(path,before);
    }
    /* Ordinary entry points still reject unsupported v16; this slice only
       introduces an independently reviewable staged archive reader. */
    CHECK(!workbook_load(&book,path,&error)); clear(&error); CHECK(book.command_revision==6);
    CHECK(workbook_archive_read(path,parent,total-1,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_FAILED && !out); clear(&error);
    CHECK(children(parent)==baseline); unchanged(path,before);
    /* A current internal core is not an external native container. */
    SourceDigest old=digest(unwrapped);
    CHECK(workbook_archive_read(unwrapped,parent,0,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_FAILED && !out);
    clear(&error); unchanged(unwrapped,old);
    mutate(path,"DELETE FROM _duxcel_versions.chunks WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)"); before=digest(path);
    CHECK(workbook_archive_read(path,parent,total,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_FAILED && !out); clear(&error);
    CHECK(children(parent)==baseline); unchanged(path,before);
    CHECK(!unlink(path) && !unlink(unwrapped)); workbook_core_payload_free(a); a=NULL; workbook_core_payload_free(b); b=NULL;
#endif
    CHECK(!workbook_archive_book(NULL) && !workbook_archive_versions(NULL) && !workbook_archive_file_version(NULL) && !workbook_archive_identity(NULL));
    CHECK(!workbook_archive_take_book(NULL) && !workbook_archive_take_versions(NULL)); workbook_archive_candidate_free(NULL);
    CHECK(workbook_archive_read(NULL,parent,0,(WorkbookArchiveReadControl){0},&out,&error)==ARCHIVE_READ_FAILED && !out); clear(&error);
    workbook_free(&book); free(path); free(unwrapped); CHECK(!children(parent) && !rmdir(parent));
    printf("PASS: %u staged native archive reader checks\n",checks); return 0;
}
