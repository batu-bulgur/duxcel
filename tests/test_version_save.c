#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_save.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version Save controller:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static size_t children(const char *path) {
    DxDirectory *d=NULL; CHECK(dx_directory_open(path,&d,NULL)==DX_DIRECTORY_OPEN);
    DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t count=0;
    while ((next=dx_directory_next(d,&entry,NULL))==DX_DIRECTORY_ENTRY) count++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(d); return count;
}
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static SourceDigest digest(const char *path) {
    SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d;
}
static void unchanged(const char *path,SourceDigest before) {
    SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,DX_SOURCE_DIGEST_BYTES));
}
static bool cancel_publish(void *context,WorkbookSaveProgress progress) { (void)context; return progress.phase!=SAVE_PHASE_PUBLISH; }
static bool cancel_compare(void *context,WorkbookCoreComparePhase phase,size_t table) { (void)context; (void)table; return phase==CORE_COMPARE_ROWS; }
static void saved_value(const WorkbookVersionCoreSet *set,const char *text) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(set); size_t position=0;
    while (position<index->root_count && memcmp(index->roots[position].key.bytes,index->saved.key.bytes,DX_VERSION_KEY_BYTES)) position++;
    CHECK(position<index->root_count);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_load_core_payload(&book,workbook_core_payload_path(workbook_version_core_set_leaf(set,position)),&error) && !error);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,50,30),text)); workbook_free(&book);
}
int main(void) {
    char parent[]="build/tests/version-save-XXXXXX"; CHECK(mkdtemp(parent));
    char *path=dx_printf("%s/saved.dxls",parent),*checkpoint=dx_printf("%s/checkpoint.dxls",parent),*export=dx_printf("%s/export.dxls",parent),*history=dx_printf("%s/history.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Save plan",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,50,30,"initial",&error)); book.command_revision=1; book.dirty=true;
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD};
    WorkbookVersionSaveRequest request={.head=&book,.owner=owner,.purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=UINT64_C(64)*1024*1024};
    WorkbookVersionReceipt *out=NULL; WorkbookVersionCoreSet *base=NULL;
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    WorkbookVersionIndex empty={.document_high=book.id.high,.document_low=book.id.low};
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    const WorkbookVersionPlan *plan=workbook_version_receipt_plan(out);
    CHECK(plan && plan->adds_root && plan->updates_saved && plan->next_generation==1 && !workbook_version_receipt_reused(out));
    CHECK(workbook_version_can_publish(&empty,owner,plan));
    WorkbookVersionOwner other_owner=owner; other_owner.session++;
    CHECK(!workbook_version_can_publish(&empty,other_owner,plan));
    CHECK(workbook_archive_receipt_revision(workbook_version_receipt_archive(out))==1 && book.dirty && !book.path);
    base=workbook_version_receipt_take_versions(out); CHECK(base && workbook_version_core_set_index(base)->root_count==1);
    workbook_version_receipt_free(out); out=NULL;
    for (unsigned repeat=0;repeat<2;repeat++) {
        request.base=base;
        CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
        plan=workbook_version_receipt_plan(out);
        CHECK(workbook_version_receipt_reused(out) && !plan->adds_root && !plan->updates_saved && plan->next_generation==1);
        CHECK(workbook_version_can_publish(workbook_version_core_set_index(base),owner,plan));
        WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(out);
        CHECK(workbook_version_core_set_index(next)->root_count==1 && workbook_version_core_set_index(next)->generation==1);
        workbook_version_receipt_free(out); out=NULL; workbook_version_core_set_free(base); base=next;
    }
    /* A changed value with identical command clock must not reuse the old core. */
    CHECK(cells_set(&book.sheets[0].cells,50,30,"captured R+1",&error)); CHECK(book.command_revision==1);
    request.base=base;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    plan=workbook_version_receipt_plan(out); CHECK(!workbook_version_receipt_reused(out) && plan->adds_root && plan->updates_saved && plan->next_generation==2);
    CHECK(workbook_version_core_set_index(base)->generation==1 && workbook_version_core_set_index(base)->root_count==1);
    CHECK(cells_set(&book.sheets[0].cells,50,30,"working R+2",&error)); book.command_revision=2;
    CHECK(workbook_version_can_publish(workbook_version_core_set_index(base),owner,plan)); /* Independent App dirty fence is still required. */
    WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(out); saved_value(next,"captured R+1");
    workbook_version_receipt_free(out); out=NULL; workbook_version_core_set_free(base); base=next;
    request.base=base; request.purpose=VERSION_RECOVERY_CHECKPOINT; request.path=checkpoint;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    plan=workbook_version_receipt_plan(out); CHECK(!plan->adds_root && !plan->updates_saved && plan->next_generation==2);
    CHECK(workbook_version_core_set_index(workbook_archive_receipt_versions(workbook_version_receipt_archive(out)))->root_count==2);
    saved_value(workbook_archive_receipt_versions(workbook_version_receipt_archive(out)),"captured R+1");
    workbook_version_receipt_free(out); out=NULL;
    WorkbookArchiveCandidate *opened=NULL;
    CHECK(workbook_archive_read(checkpoint,parent,request.reserved_bytes,(WorkbookArchiveReadControl){0},&opened,&error)==ARCHIVE_READ_READY);
    CHECK(!strcmp(cells_get(&workbook_archive_book(opened)->sheets[0].cells,50,30),"working R+2"));
    saved_value(workbook_archive_versions(opened),"captured R+1"); workbook_archive_candidate_free(opened);
    request.purpose=VERSION_STATE_EXPORT; request.owner.mode=VERSION_HISTORICAL_VIEW; request.base=NULL; request.path=export;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    const WorkbookVersionIndex *exported=workbook_version_core_set_index(workbook_archive_receipt_versions(workbook_version_receipt_archive(out)));
    CHECK(!exported->root_count && !exported->saved.present && !exported->generation); workbook_version_receipt_free(out); out=NULL;
    Workbook historical; workbook_init(&historical);
    CHECK(workbook_load_core_payload(&historical,workbook_core_payload_path(workbook_version_core_set_leaf(base,0)),&error));
    request.head=&historical; request.base=base; request.path=history;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_COMPLETE && out && !error);
    CHECK(!workbook_version_receipt_plan(out)->updates_saved); saved_value(workbook_archive_receipt_versions(workbook_version_receipt_archive(out)),"captured R+1");
    workbook_version_receipt_free(out); out=NULL; workbook_free(&historical);
    request.head=&book; request.purpose=VERSION_USER_SAVE; request.path=path;
    size_t before_count=children(parent); SourceDigest before=digest(path);
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out); clear(&error);
    CHECK(children(parent)==before_count); unchanged(path,before);
    request.owner=owner;
    WorkbookVersionSaveControl stops[]={ {.capture={.save={cancel_publish,NULL}}}, {.compare={cancel_compare,NULL}}, {.archive={.core={cancel_publish,NULL}}} };
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        CHECK(workbook_version_save(request,stops[i],&out,&error)==WORKBOOK_SAVE_CANCELLED && !out); clear(&error);
        CHECK(children(parent)==before_count); unchanged(path,before);
        CHECK(workbook_version_core_set_index(base)->root_count==2 && workbook_version_core_set_index(base)->generation==2);
    }
    workbook_version_core_set_free(base); base=NULL;
    CHECK(!unlink(path) && !unlink(checkpoint) && !unlink(export) && !unlink(history));
#endif
    request.owner.session=0;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&out,&error)==WORKBOOK_SAVE_FAILED && !out); clear(&error);
    CHECK(!workbook_version_receipt_plan(NULL) && !workbook_version_receipt_archive(NULL) && !workbook_version_receipt_reused(NULL) && !workbook_version_receipt_take_versions(NULL)); workbook_version_receipt_free(NULL);
    workbook_free(&book); free(path); free(checkpoint); free(export); free(history); CHECK(!children(parent) && !rmdir(parent));
    printf("PASS: %u native Save purpose and root reuse checks\n",checks); return 0;
}
