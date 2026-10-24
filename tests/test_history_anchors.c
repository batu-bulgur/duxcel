#include "workbook/history_anchors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history anchors:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookVersionKey key(unsigned n) { WorkbookVersionKey k={{0}}; k.bytes[0]=(unsigned char)n; return k; }
static WorkbookHistoryAnchor anchor(uint64_t id,uint64_t position,unsigned root,uint64_t path) { return (WorkbookHistoryAnchor){.boundary={.anchor=id,.position=position,.root=key(root)},.lineage=path}; }
int main(void) {
    WorkbookVersionRoot roots[3]={{.bytes=1,.encoding=1},{.key={{1}},.bytes=2,.encoding=1},{.key={{2}},.bytes=3,.encoding=1}};
    WorkbookVersionTag tags[3]={{{"V1",2},{{0}},1},{{"Alias",5},{{0}},2},{{"V2",2},{{1}},3}};
    WorkbookVersionIndex v={.document_high=10,.document_low=20,.generation=5,.saved_generation=5,.saved={true,{{2}}},.roots=roots,.root_count=3,.tags=tags,.tag_count=3};
    WorkbookHistoryAnchor anchors[4]={anchor(0,0,0,1),anchor(9,20,1,1),anchor(10,20,2,1),anchor(11,30,2,1)};
    WorkbookHistoryTagAnchor bindings[3]={{1,0},{2,0},{3,9}};
    WorkbookHistoryAnchors h={.document_high=10,.document_low=20,.high_water=10,.saved_anchor=10,.items=anchors,.count=3,.tags=bindings,.tag_count=3};
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK);
    /* Root equality never substitutes another boundary, and presentation-only
       states may share an operation position without aliasing their IDs. */
    anchors[2].boundary.root=key(0); v.saved.key=key(0);
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK); anchors[2].boundary.root=key(2); v.saved.key=key(2);
    anchors[1].lineage=UINT64_MAX; anchors[2].boundary.position=0;
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK); anchors[1].lineage=1; anchors[2].boundary.position=20;
    CHECK(workbook_history_anchors_check(NULL,&h)==HISTORY_ANCHORS_INDEX);
    CHECK(workbook_history_anchors_check(&v,NULL)==HISTORY_ANCHORS_INVALID);
    h.document_low++; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.document_low--;
    h.count=DX_HISTORY_PUBLIC_ANCHORS+1; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_LIMIT); h.count=3;
    h.tag_count=DX_VERSION_TAGS+1; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_LIMIT); h.tag_count=3;
    h.items=NULL; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.items=anchors;
    h.tags=NULL; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.tags=bindings;
    h.tag_count=2; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.tag_count=3;
    h.high_water=9; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.high_water=10;
    h.saved_anchor=8; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.saved_anchor=10;
    anchors[1].boundary.anchor=0; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); anchors[1].boundary.anchor=9;
    anchors[1].boundary.root=key(99); CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); anchors[1].boundary.root=key(1);
    anchors[2].boundary.root=key(1); CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); anchors[2].boundary.root=key(2);
    bindings[1].tag_created=1; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); bindings[1].tag_created=2;
    bindings[1].tag_created=4; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); bindings[1].tag_created=2;
    bindings[1].anchor=8; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); bindings[1].anchor=0;
    bindings[1].anchor=9; CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); bindings[1].anchor=0;
    /* An extra older, valid-root descriptor cannot be smuggled into an admitted
       public directory without a Saved/tag reference. No deletion occurs here. */
    anchors[3]=anchor(8,15,0,1); h.count=4;
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID); h.count=3;
    /* Order of physical rows is irrelevant, identity joins are exact. */
    WorkbookHistoryAnchor swap=anchors[0]; anchors[0]=anchors[2]; anchors[2]=swap;
    WorkbookHistoryTagAnchor map=bindings[0]; bindings[0]=bindings[2]; bindings[2]=map;
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK);
    /* Monotonic issuance can be exhausted without making retained IDs invalid. */
    anchors[0].boundary.anchor=UINT64_MAX; h.saved_anchor=h.high_water=UINT64_MAX;
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK);
    tags[0].label.bytes=0; /* The borrowed version index is validated first. */
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INDEX); tags[0].label.bytes=2;
    WorkbookVersionIndex empty={.document_high=10,.document_low=20};
    WorkbookHistoryAnchors none={.document_high=10,.document_low=20,.high_water=UINT64_MAX};
    CHECK(workbook_history_anchors_check(&empty,&none)==HISTORY_ANCHORS_OK);
    none.saved_anchor=1; CHECK(workbook_history_anchors_check(&empty,&none)==HISTORY_ANCHORS_INVALID);
    /* Saved presence comes from the version index: anchor zero is a real ID,
       not a nullable sentinel, including a new file with no named tags. */
    WorkbookHistoryAnchor zero=anchor(0,0,0,0);
    WorkbookVersionIndex first={.document_high=10,.document_low=20,.generation=1,.saved_generation=1,.saved={true,{{0}}},.roots=roots,.root_count=1};
    WorkbookHistoryAnchors initial={.document_high=10,.document_low=20,.items=&zero,.count=1};
    CHECK(workbook_history_anchors_check(&first,&initial)==HISTORY_ANCHORS_OK);
    initial.count=0; CHECK(workbook_history_anchors_check(&first,&initial)==HISTORY_ANCHORS_INVALID);
    initial.count=1; CHECK(workbook_history_anchors_check(&empty,&initial)==HISTORY_ANCHORS_INVALID);
    /* Maximum public inventory: every tag has its own operation boundary even
       though every content root is the same all-zero opaque key. */
    WorkbookHistoryAnchor many[DX_HISTORY_PUBLIC_ANCHORS]; WorkbookVersionTag labels[DX_VERSION_TAGS];
    WorkbookHistoryTagAnchor maps[DX_VERSION_TAGS]; char names[DX_VERSION_TAGS][16];
    for (size_t i=0;i<DX_HISTORY_PUBLIC_ANCHORS;i++) many[i]=anchor(i,i,0,1);
    for (size_t i=0;i<DX_VERSION_TAGS;i++) {
        int n=snprintf(names[i],sizeof(names[i]),"T%zu",i); labels[i]=(WorkbookVersionTag){.label={names[i],(size_t)n},.root={{0}},.created=i+1}; maps[i]=(WorkbookHistoryTagAnchor){i+1,i};
    }
    v=(WorkbookVersionIndex){.document_high=10,.document_low=20,.generation=DX_HISTORY_PUBLIC_ANCHORS,.saved_generation=DX_HISTORY_PUBLIC_ANCHORS,.saved={true,{{0}}},.roots=roots,.root_count=1,.tags=labels,.tag_count=DX_VERSION_TAGS};
    h=(WorkbookHistoryAnchors){.document_high=10,.document_low=20,.high_water=DX_VERSION_TAGS,.saved_anchor=DX_VERSION_TAGS,.items=many,.count=DX_HISTORY_PUBLIC_ANCHORS,.tags=maps,.tag_count=DX_VERSION_TAGS};
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_OK);
    maps[DX_VERSION_TAGS-1].anchor=DX_VERSION_TAGS;
    CHECK(workbook_history_anchors_check(&v,&h)==HISTORY_ANCHORS_INVALID);
    printf("PASS: %u closed public history-anchor binding checks\n",checks); return 0;
}
