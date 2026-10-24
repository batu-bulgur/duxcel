#include "../src/workbook/version_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL: version index line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookVersionKey key(unsigned value) {
    WorkbookVersionKey out={{0}};
    for (size_t i=0;i<4;i++) out.bytes[i]=(unsigned char)(value>>(i*8));
    return out;
}
static bool same_key(WorkbookVersionKey a,WorkbookVersionKey b) {
    return !memcmp(a.bytes,b.bytes,DX_VERSION_KEY_BYTES);
}
static WorkbookVersionRoot root(unsigned value,uint64_t bytes) {
    return (WorkbookVersionRoot){.key=key(value),.bytes=bytes,.encoding=15};
}
static WorkbookVersionOwner owner(WorkbookVersionIndex index) {
    return (WorkbookVersionOwner){index.document_high,index.document_low,UINT64_C(9007199254740993),VERSION_WORKING_HEAD};
}
static void reject_plan(WorkbookVersionIndex *index,WorkbookVersionOwner write_owner,
    WorkbookVersionPurpose purpose,const WorkbookVersionRoot *candidate,WorkbookVersionStatus expected) {
    WorkbookVersionIndex before=*index;
    WorkbookVersionPlan plan,unchanged;
    memset(&plan,0xa5,sizeof(plan)); memcpy(&unchanged,&plan,sizeof(plan));
    CHECK(workbook_version_prepare(index,write_owner,purpose,candidate,&plan)==expected);
    CHECK(!memcmp(&plan,&unchanged,sizeof(plan)));
    CHECK(!memcmp(index,&before,sizeof(before)));
}
static void structure(void) {
    WorkbookVersionIndex empty={.document_high=UINT64_MAX,.document_low=UINT64_MAX};
    CHECK(workbook_version_index_check(NULL)==VERSION_INVALID_INDEX);
    CHECK(workbook_version_index_check(&empty)==VERSION_OK);
    WorkbookVersionIndex index=empty;
    index.document_high=index.document_low=0; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index=empty; index.generation=1; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index=empty; index.saved_generation=1; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index=empty; index.saved.key=key(1); CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index=empty; index.saved.present=true; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index=empty; index.root_count=SIZE_MAX; CHECK(workbook_version_index_check(&index)==VERSION_LIMIT);
    index=empty; index.tag_count=SIZE_MAX; CHECK(workbook_version_index_check(&index)==VERSION_LIMIT);
    index=empty; index.root_count=1; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    WorkbookVersionRoot roots[]={root(0,1),root(1,2),root(2,3)};
    index=empty; index.generation=index.saved_generation=10;
    index.roots=roots; index.root_count=3; index.saved=(WorkbookVersionRef){true,key(0)};
    CHECK(workbook_version_index_check(&index)==VERSION_OK); /* Zero key is a real present root. */
    index.saved.present=false; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index.saved.present=true; index.saved.key=key(3); CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index.saved.key=key(0); index.saved_generation=11; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index.saved_generation=0; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index.saved_generation=10; roots[1].bytes=0; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_ROOT);
    roots[1].bytes=2; roots[1].encoding=0; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_ROOT);
    roots[1].encoding=UINT32_MAX; CHECK(workbook_version_index_check(&index)==VERSION_OK); /* Payload decoding is a separate adapter gate. */
    roots[1].key=key(0); CHECK(workbook_version_index_check(&index)==VERSION_ROOT_CONFLICT);
    roots[1]=root(1,2);
    WorkbookVersionTag tags[]={{{"V1",2},key(0),1},{{"V2",2},key(0),2}};
    index.tags=tags; index.tag_count=2;
    CHECK(workbook_version_index_check(&index)==VERSION_OK); /* Distinct labels share one payload. */
    WorkbookVersionRoot first=roots[0]; roots[0]=roots[2]; roots[2]=first;
    WorkbookVersionTag tag=tags[0]; tags[0]=tags[1]; tags[1]=tag;
    CHECK(workbook_version_index_check(&index)==VERSION_OK); /* Physical order is not chronology. */
    tags[1].label=tags[0].label; CHECK(workbook_version_index_check(&index)==VERSION_DUPLICATE_TAG);
    tags[1].label=(WorkbookVersionText){"v2",2}; CHECK(workbook_version_index_check(&index)==VERSION_OK);
    tags[1].created=tags[0].created; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    tags[1].created=0; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    tags[1].created=11; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    tags[1].created=1; tags[1].root=key(55); CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    tags[1].root=key(0); index.tags=NULL; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    index.tags=tags; index.roots=NULL; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
}
static void labels(void) {
    WorkbookVersionRoot payload=root(1,5);
    WorkbookVersionTag tag={{"V1",2},key(1),1};
    WorkbookVersionIndex index={.document_low=1,.generation=1,.saved_generation=1,.saved={true,{{1}}},
        .roots=&payload,.root_count=1,.tags=&tag,.tag_count=1};
    const char *valid[]={"V1","NULL","İstanbul","\xc3\xa9","e\xcc\x81","\xf0\x9f\x93\x8a"," A ","\xc2\xa0"};
    for (size_t i=0;i<sizeof(valid)/sizeof(*valid);i++) {
        tag.label=(WorkbookVersionText){valid[i],strlen(valid[i])};
        CHECK(workbook_version_index_check(&index)==VERSION_OK);
    }
    char label[DX_VERSION_LABEL_BYTES]; memset(label,'x',sizeof(label));
    tag.label=(WorkbookVersionText){label,sizeof(label)};
    CHECK(workbook_version_index_check(&index)==VERSION_OK);
    tag.label.bytes++; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    tag.label=(WorkbookVersionText){NULL,3}; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    tag.label=(WorkbookVersionText){"",0}; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    tag.label=(WorkbookVersionText){"   ",3}; CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    for (unsigned i=0;i<33;i++) {
        char text[]={'V',(char)(i==32 ? 127 : i),'x'};
        tag.label=(WorkbookVersionText){text,sizeof(text)};
        CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    }
    const char *bad[]={"\xc0\x80","\xc1\xbf","\x80","\xc2","\xe0\x80\x80","\xed\xa0\x80",
        "\xe2\x82","\xe2\x82\x41","\xf0\x80\x80\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xff"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        tag.label=(WorkbookVersionText){bad[i],strlen(bad[i])};
        CHECK(workbook_version_index_check(&index)==VERSION_INVALID_LABEL);
    }
    const char *edge[]={"\xc2\x80","\xdf\xbf","\xe0\xa0\x80","\xed\x9f\xbf","\xee\x80\x80",
        "\xef\xbf\xbf","\xf0\x90\x80\x80","\xf4\x8f\xbf\xbf"};
    for (size_t i=0;i<sizeof(edge)/sizeof(*edge);i++) {
        tag.label=(WorkbookVersionText){edge[i],strlen(edge[i])};
        CHECK(workbook_version_index_check(&index)==VERSION_OK);
    }
}
static void publication(void) {
    WorkbookVersionIndex index={.document_low=42};
    WorkbookVersionOwner write_owner=owner(index);
    WorkbookVersionRoot payload=root(0,512);
    WorkbookVersionPlan plan;
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&payload,&plan)==VERSION_OK);
    CHECK(plan.adds_root && plan.updates_saved && plan.next_generation==1 && plan.next_saved_generation==1);
    CHECK(!plan.before_saved.present && plan.after_saved.present && same_key(plan.after_saved.key,key(0)));
    CHECK(!index.root_count && !index.generation && !index.saved.present);
    CHECK(workbook_version_can_publish(&index,write_owner,&plan));
    WorkbookVersionOwner replaced=write_owner; replaced.session++;
    CHECK(!workbook_version_can_publish(&index,replaced,&plan)); /* Reopen/preview can share durable IDs. */
    replaced=write_owner; replaced.document_high=1;
    CHECK(!workbook_version_can_publish(&index,replaced,&plan));
    WorkbookVersionPlan tampered=plan; tampered.next_generation++;
    CHECK(!workbook_version_can_publish(&index,write_owner,&tampered));
    tampered=plan; tampered.after_saved.present=false;
    CHECK(!workbook_version_can_publish(&index,write_owner,&tampered));
    tampered=plan; tampered.adds_root=false;
    CHECK(!workbook_version_can_publish(&index,write_owner,&tampered));
    CHECK(!workbook_version_can_publish(NULL,write_owner,&plan));
    CHECK(!workbook_version_can_publish(&index,write_owner,NULL));
    for (int kind=VERSION_RECOVERY_CHECKPOINT;kind<=VERSION_STATE_EXPORT;kind++) {
        CHECK(workbook_version_prepare(&index,write_owner,(WorkbookVersionPurpose)kind,NULL,&plan)==VERSION_OK);
        CHECK(!plan.adds_root && !plan.updates_saved && !plan.after_saved.present && !plan.next_generation);
        CHECK(workbook_version_can_publish(&index,write_owner,&plan));
        CHECK(!index.saved.present && !index.root_count && !index.generation);
    }
    WorkbookVersionRoot roots[]={payload,root(1,1024)};
    index.roots=roots; index.root_count=2; index.generation=index.saved_generation=8;
    index.saved=(WorkbookVersionRef){true,key(0)};
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&roots[0],&plan)==VERSION_OK);
    CHECK(!plan.updates_saved && !plan.adds_root && plan.next_generation==8 && plan.next_saved_generation==8);
    CHECK(workbook_version_can_publish(&index,write_owner,&plan));
    WorkbookVersionRoot equal_size=root(2,payload.bytes);
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&equal_size,&plan)==VERSION_OK);
    CHECK(plan.adds_root && plan.updates_saved && same_key(plan.after_saved.key,key(2)));
    CHECK(workbook_version_can_publish(&index,write_owner,&plan)); /* Equal size is not equal content. */
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&roots[1],&plan)==VERSION_OK);
    CHECK(plan.updates_saved && !plan.adds_root && plan.next_generation==9 && same_key(plan.after_saved.key,key(1)));
    CHECK(index.generation==8 && same_key(index.saved.key,key(0)));
    CHECK(workbook_version_can_publish(&index,write_owner,&plan));
    index.root_count--; CHECK(!workbook_version_can_publish(&index,write_owner,&plan)); index.root_count++;
    index.generation++; CHECK(!workbook_version_can_publish(&index,write_owner,&plan)); index.generation--;
    index.saved_generation--; CHECK(!workbook_version_can_publish(&index,write_owner,&plan)); index.saved_generation++;
    index.saved.key=key(1); CHECK(!workbook_version_can_publish(&index,write_owner,&plan)); index.saved.key=key(0);
    for (int kind=VERSION_RECOVERY_CHECKPOINT;kind<=VERSION_STATE_EXPORT;kind++) {
        CHECK(workbook_version_prepare(&index,write_owner,(WorkbookVersionPurpose)kind,NULL,&plan)==VERSION_OK);
        CHECK(!plan.updates_saved && !plan.adds_root && plan.next_generation==8 &&
            plan.next_saved_generation==8 && same_key(plan.after_saved.key,key(0)));
        CHECK(workbook_version_can_publish(&index,write_owner,&plan));
        reject_plan(&index,write_owner,(WorkbookVersionPurpose)kind,&payload,VERSION_INVALID_ROOT);
    }
    WorkbookVersionOwner preview=write_owner; preview.mode=VERSION_HISTORICAL_VIEW;
    reject_plan(&index,preview,VERSION_USER_SAVE,&payload,VERSION_READ_ONLY);
    reject_plan(&index,preview,VERSION_RECOVERY_CHECKPOINT,NULL,VERSION_READ_ONLY);
    CHECK(workbook_version_prepare(&index,preview,VERSION_STATE_EXPORT,NULL,&plan)==VERSION_OK);
    CHECK(workbook_version_can_publish(&index,preview,&plan) && !plan.updates_saved);
    CHECK(!workbook_version_can_publish(&index,write_owner,&plan));
    replaced=write_owner; replaced.session=0; reject_plan(&index,replaced,VERSION_USER_SAVE,&payload,VERSION_INVALID_OWNER);
    replaced=write_owner; replaced.mode=(WorkbookVersionMode)99; reject_plan(&index,replaced,VERSION_USER_SAVE,&payload,VERSION_INVALID_OWNER);
    replaced=write_owner; replaced.document_low++; reject_plan(&index,replaced,VERSION_USER_SAVE,&payload,VERSION_INVALID_OWNER);
    reject_plan(&index,write_owner,(WorkbookVersionPurpose)99,&payload,VERSION_INVALID_PURPOSE);
    reject_plan(&index,write_owner,VERSION_USER_SAVE,NULL,VERSION_INVALID_ROOT);
    WorkbookVersionRoot bad=payload; bad.bytes++; reject_plan(&index,write_owner,VERSION_USER_SAVE,&bad,VERSION_ROOT_CONFLICT);
    bad=payload; bad.encoding++; reject_plan(&index,write_owner,VERSION_USER_SAVE,&bad,VERSION_ROOT_CONFLICT);
    bad=payload; bad.bytes=0; reject_plan(&index,write_owner,VERSION_USER_SAVE,&bad,VERSION_INVALID_ROOT);
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&payload,NULL)==VERSION_INVALID_INDEX);
    index.generation=index.saved_generation=UINT64_MAX;
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,&payload,&plan)==VERSION_OK);
    CHECK(!plan.updates_saved && plan.next_generation==UINT64_MAX);
    reject_plan(&index,write_owner,VERSION_USER_SAVE,&roots[1],VERSION_EXHAUSTED);
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_RECOVERY_CHECKPOINT,NULL,&plan)==VERSION_OK);
    roots[0].bytes=UINT64_MAX; index.root_count=1;
    CHECK(workbook_version_index_check(&index)==VERSION_OK);
    bad=root(2,1); reject_plan(&index,write_owner,VERSION_USER_SAVE,&bad,VERSION_LIMIT);
    index.root_count=2; CHECK(workbook_version_index_check(&index)==VERSION_LIMIT);
}
static void full_width_keys(void) {
    const size_t positions[]={0,15,16,31};
    for (size_t i=0;i<sizeof(positions)/sizeof(*positions);i++) {
        WorkbookVersionRoot roots[]={root(0,100),root(0,100)};
        roots[1].key.bytes[positions[i]]=1;
        WorkbookVersionIndex index={.document_low=1,.generation=1,.saved_generation=1,
            .saved={true,{{0}}},.roots=roots,.root_count=2};
        WorkbookVersionOwner write_owner=owner(index); WorkbookVersionPlan plan;
        CHECK(workbook_version_index_check(&index)==VERSION_OK);
        CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,roots+1,&plan)==VERSION_OK);
        CHECK(plan.updates_saved && !plan.adds_root);
        CHECK(workbook_version_can_publish(&index,write_owner,&plan));
        index.saved.key.bytes[positions[i]]=2;
        CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
    }
}
static void capacity(void) {
    WorkbookVersionRoot roots[DX_VERSION_ROOTS];
    WorkbookVersionTag tags[DX_VERSION_TAGS];
    char names[DX_VERSION_TAGS][16];
    for (unsigned i=0;i<DX_VERSION_ROOTS;i++) roots[i]=root(i,1);
    WorkbookVersionIndex index={.document_low=7,.generation=DX_VERSION_TAGS,.saved_generation=DX_VERSION_TAGS,
        .saved={true,{{0}}},.roots=roots,.root_count=DX_VERSION_ROOTS};
    WorkbookVersionOwner write_owner=owner(index); WorkbookVersionPlan plan;
    CHECK(workbook_version_index_check(&index)==VERSION_OK);
    WorkbookVersionRoot next=root(DX_VERSION_ROOTS,1);
    reject_plan(&index,write_owner,VERSION_USER_SAVE,&next,VERSION_LIMIT);
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,roots+DX_VERSION_ROOTS-1,&plan)==VERSION_OK);
    CHECK(plan.updates_saved && !plan.adds_root);
    for (unsigned i=0;i<DX_VERSION_TAGS;i++) {
        int length=snprintf(names[i],sizeof(names[i]),"Version %u",i);
        tags[i]=(WorkbookVersionTag){{names[i],(size_t)length},key(0),i+1};
    }
    index.tags=tags; index.tag_count=DX_VERSION_TAGS;
    CHECK(workbook_version_index_check(&index)==VERSION_OK);
    CHECK(workbook_version_prepare(&index,write_owner,VERSION_USER_SAVE,roots,&plan)==VERSION_OK);
    CHECK(!plan.updates_saved && !plan.adds_root); /* At capacity, identical Save still works. */
    index.tag_count++; CHECK(workbook_version_index_check(&index)==VERSION_LIMIT);
    index.tag_count--; tags[DX_VERSION_TAGS-1].created=tags[0].created;
    CHECK(workbook_version_index_check(&index)==VERSION_INVALID_INDEX);
}
int main(void) {
    structure(); labels(); publication(); full_width_keys(); capacity();
    for (int status=VERSION_OK;status<=VERSION_ROOT_CONFLICT;status++)
        CHECK(workbook_version_message((WorkbookVersionStatus)status) && *workbook_version_message((WorkbookVersionStatus)status));
    CHECK(*workbook_version_message((WorkbookVersionStatus)999));
    printf("PASS: %u workbook version index checks\n",checks);
    return 0;
}
