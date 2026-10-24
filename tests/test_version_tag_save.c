#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_save.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks,captures;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native tag:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SourceDigest digest(const char *path) { SourceDigest d={0}; CHECK(source_digest_file(path,NULL,NULL,&d,NULL)==SOURCE_DIGEST_OK); return d; }
static void unchanged(const char *path,SourceDigest before) { SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,DX_SOURCE_DIGEST_BYTES)); }
static bool cancel_publish(void *context,WorkbookSaveProgress p) { (void)context; return p.phase!=SAVE_PHASE_PUBLISH; }
static bool capture_count(void *context,WorkbookSaveProgress p) { (void)context; (void)p; captures++; return true; }
static size_t root_at(const WorkbookVersionIndex *index,WorkbookVersionKey key) {
    for (size_t i=0;i<index->root_count;i++) if (!memcmp(index->roots[i].key.bytes,key.bytes,DX_VERSION_KEY_BYTES)) return i;
    CHECK(false); return 0;
}
static void tag_value(const WorkbookVersionCoreSet *set,size_t position,const char *value) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(set); CHECK(position<index->tag_count);
    size_t root=root_at(index,index->tags[position].root); Workbook b; workbook_init(&b); char *error=NULL;
    CHECK(workbook_load_core_payload(&b,workbook_core_payload_path(workbook_version_core_set_leaf(set,root)),&error) && !error);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,9000,30),value)); workbook_free(&b);
}
static WorkbookVersionCoreSet *tag(WorkbookVersionSaveRequest request,const char *label,bool reused,uint64_t generation,size_t roots,size_t tags) {
    char *error=NULL; WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){label,strlen(label)},(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    const WorkbookVersionTagPlan *plan=workbook_version_receipt_tag(receipt); CHECK(plan && !workbook_version_receipt_plan(receipt));
    WorkbookVersionIndex empty={.document_high=request.head->id.high,.document_low=request.head->id.low};
    CHECK(workbook_version_tag_can_publish(request.base ? workbook_version_core_set_index(request.base) : &empty,request.owner,plan));
    CHECK(workbook_version_receipt_reused(receipt)==reused && plan->next_generation==generation);
    WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(receipt); const WorkbookVersionIndex *index=workbook_version_core_set_index(next);
    CHECK(index && index->generation==generation && index->saved_generation==generation && index->root_count==roots && index->tag_count==tags);
    CHECK(!strcmp(index->tags[tags-1].label.data,label) && index->tags[tags-1].created==generation);
    workbook_version_receipt_free(receipt); return next;
}
int main(void) {
    char parent[]="build/tests/version-tag-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/tags.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Tagged",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,9000,30,"R1",&error)); book.dirty=true;
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD};
    WorkbookVersionSaveRequest request={&book,NULL,owner,VERSION_USER_SAVE,path,parent,UINT64_C(128)*1024*1024};
    WorkbookVersionCoreSet *base=tag(request,"İlk sürüm",false,1,1,1); CHECK(book.dirty && !book.path);
    request.base=base; WorkbookVersionCoreSet *next=tag(request,"V2",true,2,1,2); workbook_version_core_set_free(base); base=next;
    CHECK(cells_set(&book.sheets[0].cells,9000,30,"R2",&error)); request.base=base;
    next=tag(request,"V3",false,3,2,3); workbook_version_core_set_free(base); base=next;
    tag_value(base,0,"R1"); tag_value(base,1,"R1"); tag_value(base,2,"R2");
    request.base=base; WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    CHECK(!workbook_version_receipt_tag(receipt) && workbook_version_receipt_plan(receipt) && workbook_version_receipt_reused(receipt));
    next=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); receipt=NULL; workbook_version_core_set_free(base); base=next;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(base); CHECK(index->tag_count==3 && index->root_count==2 && index->generation==3);
    /* Restore the complete original supported state, including its clocks;
       matching just a value is insufficient for safe retained-root reuse. */
    size_t original=root_at(index,index->tags[0].root); Workbook restored; workbook_init(&restored);
    CHECK(workbook_load_core_payload(&restored,workbook_core_payload_path(workbook_version_core_set_leaf(base,original)),&error));
    workbook_free(&book); book=restored; request.base=base;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    CHECK(workbook_version_receipt_reused(receipt) && !workbook_version_receipt_plan(receipt)->adds_root);
    next=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); receipt=NULL; workbook_version_core_set_free(base); base=next;
    CHECK(workbook_version_core_set_index(base)->generation==4 && workbook_version_core_set_index(base)->tag_count==3);
    request.base=base; next=tag(request,"V4",true,5,2,4); workbook_version_core_set_free(base); base=next; request.base=base;
    SourceDigest before=digest(path); WorkbookVersionSaveControl control={.archive.core.update=cancel_publish};
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){"Cancelled",9},control,&receipt,&error)==WORKBOOK_SAVE_CANCELLED && !receipt && error);
    free(error); error=NULL; unchanged(path,before);
    control=(WorkbookVersionSaveControl){.capture.save.update=capture_count};
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){"V2",2},control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && error && !captures);
    free(error); error=NULL; unchanged(path,before);
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){"bad\n",4},control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && error && !captures);
    free(error); error=NULL;
    WorkbookVersionSaveRequest limited=request; limited.reserved_bytes=1;
    CHECK(workbook_version_tag_save(limited,(WorkbookVersionText){"Too large",9},(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && error);
    free(error); error=NULL; unchanged(path,before); CHECK(workbook_version_core_set_index(base)->tag_count==4);
    WorkbookArchiveCandidate *candidate=NULL;
    CHECK(workbook_archive_read(path,parent,request.reserved_bytes,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY && !error);
    const WorkbookVersionCoreSet *loaded=workbook_archive_versions(candidate);
    CHECK(workbook_version_core_set_index(loaded)->tag_count==4 && workbook_version_core_set_index(loaded)->root_count==2);
    tag_value(loaded,0,"R1"); tag_value(loaded,1,"R1"); tag_value(loaded,2,"R2"); tag_value(loaded,3,"R1");
    workbook_archive_candidate_free(candidate); workbook_version_core_set_free(base); workbook_free(&book);
    CHECK(!unlink(path) && !rmdir(parent)); free(path);
    printf("PASS: %u native atomic tagged-state checks\n",checks); return 0;
}
