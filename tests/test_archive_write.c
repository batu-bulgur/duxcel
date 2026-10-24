#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/archive_write.h"
#include "platform/platform.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL archive write:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void sql(const char *path,const char *text,bool marker) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,text,&r)==DuckDBSuccess);
    if (marker) CHECK(duckdb_row_count(&r)==1 && duckdb_value_uint64(&r,0,0)==482);
    duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static SourceDigest digest(const char *path) {
    SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d;
}
static void unchanged(const char *path,SourceDigest before) {
    SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,DX_SOURCE_DIGEST_BYTES));
}
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static void receipt(const WorkbookArchiveReceipt *r,const Workbook *head) {
    CHECK(r && workbook_archive_receipt_path(r) && workbook_archive_receipt_identity(r));
    WorkbookId id=workbook_archive_receipt_document(r);
    CHECK(id.high==head->id.high && id.low==head->id.low && workbook_archive_receipt_revision(r)==head->command_revision);
    const WorkbookVersionCoreSet *set=workbook_archive_receipt_versions(r); const WorkbookVersionIndex *index=workbook_version_core_set_index(set);
    CHECK(index && index->root_count==2 && index->tag_count==1 && index->generation==2 && index->saved_generation==2);
    Workbook old; workbook_init(&old); char *error=NULL;
    CHECK(workbook_load_core_payload(&old,workbook_core_payload_path(workbook_version_core_set_leaf(set,0)),&error));
    CHECK(!strcmp(cells_get(&old.sheets[0].cells,50,30),"baseline")); workbook_free(&old);
    CHECK(workbook_load_core_payload(&old,workbook_core_payload_path(workbook_version_core_set_leaf(set,1)),&error));
    CHECK(!strcmp(cells_get(&old.sheets[0].cells,50,30),"saved R") && old.command_revision==2); workbook_free(&old);
}
static void opened(const char *path,const char *parent,uint64_t reserved,const char *text,uint64_t revision) {
    WorkbookArchiveCandidate *candidate=NULL; char *error=NULL;
    CHECK(workbook_archive_read(path,parent,reserved,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY && candidate && !error);
    CHECK(workbook_archive_file_version(candidate)==16 && workbook_archive_versions(candidate));
    const Workbook *head=workbook_archive_book(candidate);
    CHECK(head->command_revision==revision && !strcmp(cells_get(&head->sheets[0].cells,50,30),text));
    workbook_archive_candidate_free(candidate);
}
static bool cancel_core(void *context,WorkbookSaveProgress progress) { return progress.phase!=*(WorkbookSavePhase *)context; }
static bool cancel_stage(void *context,WorkbookVersionStagePhase phase,size_t root,uint64_t bytes) {
    (void)context; return phase==VERSION_STAGE_CHUNKS && root==0 && bytes>=65536;
}
static bool cancel_restore(void *context,WorkbookVersionRestorePhase phase,size_t root,uint64_t bytes) {
    (void)context; (void)bytes; return phase==VERSION_RESTORE_COPY && root==1;
}
int main(void) {
    char parent[]="build/tests/archive-write-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/workbook.dxls",parent),*fresh=dx_printf("%s/fresh.dxls",parent),*unwrapped=dx_printf("%s/unwrapped.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Versioned Save",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"baseline",&error)); book.command_revision=1;
    WorkbookArchiveReceipt *out=NULL; WorkbookCorePayload *a=NULL,*b=NULL;
    WorkbookVersionIndex index={.document_high=book.id.high,.document_low=book.id.low};
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&a,&error)==CORE_PAYLOAD_READY);
    CHECK(cells_set(&book.sheets[0].cells,50,30,"saved R",&error)); book.command_revision=2; book.dirty=true;
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&b,&error)==CORE_PAYLOAD_READY);
    const WorkbookCorePayload *leaves[]={a,b}; WorkbookVersionRoot roots[2]={0}; uint64_t total=0;
    for (size_t i=0;i<2;i++) { const WorkbookCorePayloadInfo *info=workbook_core_payload_info(leaves[i]); roots[i].bytes=info->bytes; roots[i].encoding=info->encoding; memcpy(roots[i].key.bytes,info->sha256,DX_VERSION_KEY_BYTES); total+=info->bytes; }
    WorkbookVersionTag tag={{"Baseline",8},roots[0].key,1}; index.generation=index.saved_generation=2; index.saved=(WorkbookVersionRef){true,roots[1].key};
    index.roots=roots; index.root_count=2; index.tags=&tag; index.tag_count=1;
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    receipt(out,&book); CHECK(book.dirty && !book.path && book.command_revision==2);
    opened(path,parent,total,"saved R",2); workbook_archive_receipt_free(out); out=NULL;
    sql(path,"CREATE TABLE unrelated AS SELECT 482 AS value",false);
    size_t baseline=children(parent); SourceDigest before=digest(path);
    WorkbookSavePhase phases[]={SAVE_PHASE_VALIDATE,SAVE_PHASE_PREPARE,SAVE_PHASE_WRITE,SAVE_PHASE_PUBLISH};
    for (size_t i=0;i<sizeof(phases)/sizeof(*phases);i++) for (unsigned existing=0;existing<2;existing++) {
        CHECK(workbook_archive_write(&book,&index,leaves,2,existing ? path : fresh,parent,total,(WorkbookArchiveWriteControl){.core={cancel_core,phases+i}},&out,&error)==WORKBOOK_SAVE_CANCELLED && !out);
        clear(&error); CHECK(access(fresh,F_OK) && children(parent)==baseline); unchanged(path,before); sql(path,"SELECT value FROM unrelated",true);
    }
    WorkbookArchiveWriteControl stops[]={ {.stage={cancel_stage,NULL}}, {.prepare={cancel_restore,NULL}} };
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) for (unsigned existing=0;existing<2;existing++) {
        CHECK(workbook_archive_write(&book,&index,leaves,2,existing ? path : fresh,parent,total,stops[i],&out,&error)==WORKBOOK_SAVE_CANCELLED && !out);
        clear(&error); CHECK(access(fresh,F_OK) && children(parent)==baseline); unchanged(path,before);
    }
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total,(WorkbookArchiveWriteControl){.inspect={cancel_restore,NULL}},&out,&error)==WORKBOOK_SAVE_CANCELLED && !out);
    clear(&error); CHECK(children(parent)==baseline); unchanged(path,before);
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total-1,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out);
    clear(&error); unchanged(path,before); CHECK(children(parent)==baseline);
    SourceDigest leaf_before=digest(workbook_core_payload_path(a));
    CHECK(workbook_archive_write(&book,&index,leaves,2,workbook_core_payload_path(a),parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out);
    clear(&error); unchanged(workbook_core_payload_path(a),leaf_before);
    char *source=dx_absolute_path("tests/fixtures/transactions.csv"); SourceDigest source_before=digest(source);
    book.sheets[0].analysis=(Analysis){.kind=SOURCE_CSV,.path=source};
    CHECK(workbook_archive_write(&book,&index,leaves,2,source,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out);
    clear(&error); unchanged(source,source_before); analysis_free(&book.sheets[0].analysis);
    CHECK(cells_set(&book.sheets[0].cells,50,30,"checkpoint R+1",&error)); book.command_revision=3;
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    receipt(out,&book); sql(path,"SELECT value FROM unrelated",true); opened(path,parent,total,"checkpoint R+1",3);
    WorkbookArchiveReceipt *same=out;
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && out==same); clear(&error);
    WorkbookVersionCoreSet *owned=workbook_archive_receipt_take_versions(out); CHECK(owned && !workbook_archive_receipt_versions(out));
    workbook_archive_receipt_free(out); out=NULL; CHECK(workbook_version_core_set_index(owned)->root_count==2); workbook_version_core_set_free(owned);
    CHECK(children(parent)==baseline);
    CHECK(workbook_save(&book,unwrapped,&error));
    SourceDigest prior=digest(unwrapped);
    CHECK(workbook_archive_write(&book,&index,leaves,2,unwrapped,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out);
    CHECK(error); free(error); error=NULL; unchanged(unwrapped,prior);
    sql(path,"DELETE FROM _duxcel_versions.chunks WHERE root=(SELECT key FROM _duxcel_versions.roots WHERE position=0)",false); before=digest(path);
    CHECK(workbook_archive_write(&book,&index,leaves,2,path,parent,total,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out);
    clear(&error); unchanged(path,before); CHECK(children(parent)==baseline+1);
    CHECK(!unlink(path) && !unlink(unwrapped)); workbook_core_payload_free(a); a=NULL; workbook_core_payload_free(b); b=NULL;
#else
    CHECK(workbook_archive_write(&book,&index,NULL,0,path,parent,0,(WorkbookArchiveWriteControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out && error); clear(&error);
#endif
    CHECK(!workbook_archive_receipt_path(NULL) && !workbook_archive_receipt_versions(NULL) && !workbook_archive_receipt_identity(NULL));
    CHECK(!workbook_archive_receipt_revision(NULL) && !workbook_archive_receipt_document(NULL).high && !workbook_archive_receipt_take_versions(NULL)); workbook_archive_receipt_free(NULL);
    workbook_free(&book); free(path); free(fresh); free(unwrapped); CHECK(!children(parent) && !rmdir(parent));
    printf("PASS: %u native versioned archive Save checks\n",checks); return 0;
}
