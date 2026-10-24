#include "workbook/version_tag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version tag:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void reject(WorkbookVersionIndex index,WorkbookVersionOwner owner,WorkbookVersionRoot root,WorkbookVersionText label,WorkbookVersionStatus status) {
    WorkbookVersionTagPlan out,before; memset(&out,0xa5,sizeof(out)); before=out; WorkbookVersionIndex copy=index;
    CHECK(workbook_version_tag_prepare(&index,owner,&root,label,&out)==status);
    CHECK(!memcmp(&out,&before,sizeof(out)) && !memcmp(&copy,&index,sizeof(index)));
}
int main(void) {
    WorkbookVersionIndex index={.document_high=1,.document_low=2};
    WorkbookVersionOwner owner={1,2,482,VERSION_WORKING_HEAD}; WorkbookVersionRoot root={.bytes=42,.encoding=15};
    char label[]="İlk sürüm"; WorkbookVersionTagPlan first;
    CHECK(workbook_version_tag_prepare(&index,owner,&root,(WorkbookVersionText){label,strlen(label)},&first)==VERSION_OK);
    memset(label,'x',strlen(label)); CHECK(!strcmp(first.label,"İlk sürüm"));
    CHECK(first.save.adds_root && first.save.updates_saved && first.next_generation==1 && first.save.after_saved.present);
    CHECK(workbook_version_tag_can_publish(&index,owner,&first));
    WorkbookVersionTag tag={{first.label,first.label_bytes},root.key,first.next_generation};
    index=(WorkbookVersionIndex){.document_high=1,.document_low=2,.generation=1,.saved_generation=1,.saved={true,root.key},.roots=&root,.root_count=1,.tags=&tag,.tag_count=1};
    CHECK(workbook_version_index_check(&index)==VERSION_OK && !workbook_version_tag_can_publish(&index,owner,&first));
    WorkbookVersionTagPlan second; CHECK(workbook_version_tag_prepare(&index,owner,&root,(WorkbookVersionText){"V2",2},&second)==VERSION_OK);
    CHECK(!second.save.adds_root && !second.save.updates_saved && second.next_generation==2 && second.save.next_generation==1);
    CHECK(workbook_version_tag_can_publish(&index,owner,&second));
    WorkbookVersionTagPlan changed=second; changed.next_generation++; CHECK(!workbook_version_tag_can_publish(&index,owner,&changed));
    changed=second; changed.label_bytes=SIZE_MAX; CHECK(!workbook_version_tag_can_publish(&index,owner,&changed));
    changed=second; changed.save.next_saved_generation++; CHECK(!workbook_version_tag_can_publish(&index,owner,&changed));
    changed=second; changed.save.root.bytes++; CHECK(!workbook_version_tag_can_publish(&index,owner,&changed));
    WorkbookVersionOwner wrong=owner; wrong.session++; CHECK(!workbook_version_tag_can_publish(&index,wrong,&second));
    wrong=owner; wrong.document_low++; reject(index,wrong,root,(WorkbookVersionText){"V2",2},VERSION_INVALID_OWNER);
    wrong=owner; wrong.mode=VERSION_HISTORICAL_VIEW; reject(index,wrong,root,(WorkbookVersionText){"V2",2},VERSION_READ_ONLY);
    reject(index,owner,root,tag.label,VERSION_DUPLICATE_TAG);
    const char *invalid[]={""," ","a\nb","a\tb","\xC0\xAF","\xED\xA0\x80","\xF4\x90\x80\x80"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) reject(index,owner,root,(WorkbookVersionText){invalid[i],strlen(invalid[i])},VERSION_INVALID_LABEL);
    reject(index,owner,root,(WorkbookVersionText){"a\0b",3},VERSION_INVALID_LABEL);
    char maximum[DX_VERSION_LABEL_BYTES+1]; memset(maximum,'x',sizeof(maximum));
    CHECK(workbook_version_tag_prepare(&index,owner,&root,(WorkbookVersionText){maximum,DX_VERSION_LABEL_BYTES},&changed)==VERSION_OK && changed.label[DX_VERSION_LABEL_BYTES]==0);
    reject(index,owner,root,(WorkbookVersionText){maximum,sizeof(maximum)},VERSION_INVALID_LABEL);
    WorkbookVersionRoot conflict=root; conflict.bytes++; reject(index,owner,conflict,(WorkbookVersionText){"V2",2},VERSION_ROOT_CONFLICT);
    conflict=root; conflict.key.bytes[0]=1;
    CHECK(workbook_version_tag_prepare(&index,owner,&conflict,(WorkbookVersionText){"V2",2},&changed)==VERSION_OK && changed.save.adds_root && changed.save.updates_saved && changed.next_generation==2);
    CHECK(workbook_version_tag_can_publish(&index,owner,&changed));
    index.generation=index.saved_generation=UINT64_MAX; reject(index,owner,root,(WorkbookVersionText){"V2",2},VERSION_EXHAUSTED);
    WorkbookVersionTag *tags=calloc(DX_VERSION_TAGS,sizeof(*tags)); char (*names)[32]=calloc(DX_VERSION_TAGS,sizeof(*names)); CHECK(tags && names);
    index.generation=index.saved_generation=DX_VERSION_TAGS; index.tags=tags; index.tag_count=DX_VERSION_TAGS;
    for (size_t i=0;i<DX_VERSION_TAGS;i++) { snprintf(names[i],32,"tag-%zu",i); tags[i]=(WorkbookVersionTag){{names[i],strlen(names[i])},root.key,i+1}; }
    reject(index,owner,root,(WorkbookVersionText){"extra",5},VERSION_LIMIT); free(tags); free(names);
    CHECK(!workbook_version_tag_can_publish(NULL,owner,&second) && !workbook_version_tag_can_publish(&index,owner,NULL));
    printf("PASS: %u pure named-version planning checks\n",checks); return 0;
}
