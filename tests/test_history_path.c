#include "workbook/history_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history path:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryAnchor anchor(uint64_t id,uint64_t position,uint64_t lineage) {
    return (WorkbookHistoryAnchor){.boundary={.anchor=id,.position=position},.lineage=lineage};
}
static void rejected(const WorkbookVersionIndex *v,const WorkbookHistoryAnchors *h,const WorkbookHistoryPath *p,WorkbookHistoryPathStatus status) {
    WorkbookHistoryWindow out; memset(&out,0xa5,sizeof(out)); WorkbookHistoryWindow before=out;
    CHECK(workbook_history_path_window(v,h,p,&out)==status && !memcmp(&out,&before,sizeof(out)));
}
int main(void) {
    WorkbookHistorySegment segments[3]={{0,0,3},{8,3,5},{10,5,5}};
    WorkbookHistoryEventPosition events[5]={{0,0,1},{1,0,2},{2,0,3},{9,8,4},{11,8,5}};
    WorkbookHistoryPath p={.document_high=1,.document_low=2,.segments=segments,.segment_count=3,.events=events,.event_count=5,
        .lineage_high_water=10,.operation_high_water=11,.cursor=4,.tail=5};
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    CHECK(workbook_history_path_check(NULL)==HISTORY_PATH_INVALID);
    p.document_high=p.document_low=0; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.document_high=1; p.document_low=2;
    p.segments=NULL; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.segments=segments;
    p.segment_count=0; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.segment_count=3;
    p.events=NULL; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.events=events;
    p.cursor=6; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.cursor=4;
    p.available=5; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.available=0;
    p.origin=1; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_INVALID); p.origin=0;
    segments[1].from=2; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); segments[1].from=3;
    segments[1].through=2; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); segments[1].through=5;
    segments[2].through=6; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); segments[2].through=5;
    p.lineage_high_water=9; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); p.lineage_high_water=10;
    segments[1].lineage=0; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); segments[1].lineage=8;
    segments[2].lineage=7; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); segments[2].lineage=10;
    p.segment_count=1; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); p.segment_count=3;
    p.event_count=4; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_GAP); p.event_count=5;
    p.event_count=0; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_GAP); p.event_count=5;
    events[1].position=1; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); events[1].position=2;
    events[3].lineage=0; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); events[3].lineage=8;
    events[2].lineage=8; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); events[2].lineage=0;
    events[3].operation=2; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); events[3].operation=9;
    events[3].operation=1; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); events[3].operation=9;
    p.operation_high_water=10; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); p.operation_high_water=11;
    /* Pruned prefixes need no fabricated events, but the full retained suffix
       (including Redo) remains contiguous and tied to its owning lineage. */
    p.available=3; p.events=events+3; p.event_count=2;
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    p.available=4; p.cursor=5; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_GAP);
    p.available=5; p.event_count=0; p.events=NULL; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    p.available=0; p.cursor=4; p.events=events; p.event_count=5;
    WorkbookVersionRoot root={.bytes=1,.encoding=1};
    WorkbookVersionTag tags[3]={{{"V1",2},{{0}},1},{{"V2",2},{{0}},2},{{"Other branch",12},{{0}},3}};
    WorkbookVersionIndex v={.document_high=1,.document_low=2,.generation=4,.saved_generation=4,
        .saved={true,{{0}}},.roots=&root,.root_count=1,.tags=tags,.tag_count=3};
    WorkbookHistoryAnchor items[4]={anchor(0,1,0),anchor(1,3,0),anchor(2,4,4),anchor(3,5,8)};
    WorkbookHistoryTagAnchor bindings[3]={{1,0},{2,1},{3,2}};
    WorkbookHistoryAnchors h={.document_high=1,.document_low=2,.high_water=3,.saved_anchor=3,
        .items=items,.count=4,.tags=bindings,.tag_count=3};
    WorkbookHistoryWindow w={0};
    /* All roots deliberately match. Only ancestry, not content equality,
       admits V1/V2; the unrelated tagged branch remains a checkpoint. */
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.count==3 && w.from==1 && w.through==5 && w.cursor==4);
    items[2].lineage=0; /* Tag on abandoned ancestor suffix, after fork. */
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.count==3 && w.from==1);
    items[2].lineage=4;
    p.available=1; p.events=events+1; p.event_count=4;
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.from==1);
    p.available=2; p.events=events+2; p.event_count=3; rejected(&v,&h,&p,HISTORY_PATH_GAP);
    p.available=0; p.events=events; p.event_count=5;
    items[3].lineage=0; /* Unsaved draft forked before the Saved endpoint. */
    rejected(&v,&h,&p,HISTORY_PATH_FORKED_SAVED); items[3].lineage=8;
    items[1].lineage=11; rejected(&v,&h,&p,HISTORY_PATH_INVALID); items[1].lineage=0;
    items[3].lineage=10; items[3].boundary.position=6;
    rejected(&v,&h,&p,HISTORY_PATH_INVALID); items[3]=anchor(3,5,8);
    items[3].boundary.position=2; rejected(&v,&h,&p,HISTORY_PATH_INVALID); items[3].boundary.position=5;
    h.document_low=3; rejected(&v,&h,&p,HISTORY_PATH_ANCHORS); h.document_low=2;
    p.document_low=3; rejected(&v,&h,&p,HISTORY_PATH_INVALID); p.document_low=2;
    events[4].operation=9; rejected(&v,&h,&p,HISTORY_PATH_SEQUENCE); events[4].operation=11;
    CHECK(workbook_history_path_window(&v,&h,&p,NULL)==HISTORY_PATH_INVALID);
    /* Save after Undo stays on this path and keeps its complete Redo tail. */
    items[3].boundary.position=2; items[3].lineage=0; p.cursor=2;
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.from==1 && w.cursor==2 && w.through==5);
    /* Presentation-only boundaries at one position and labels sharing an
       anchor remain distinct from branch identity and operation identity. */
    items[3]=anchor(3,3,8); items[1].boundary.position=3; p.cursor=3;
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.count==3 && w.from==1);
    bindings[1].anchor=0; h.count=3; items[1]=items[2]; items[2]=items[3];
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && w.count==2 && !w.from);
    /* No Saved/labels: preserve the initial draft interval without inventing
       old history from an absent checkpoint. */
    v=(WorkbookVersionIndex){.document_high=1,.document_low=2};
    h=(WorkbookHistoryAnchors){.document_high=1,.document_low=2};
    CHECK(workbook_history_path_window(&v,&h,&p,&w)==HISTORY_PATH_OK && !w.count && !w.from);
    /* Empty and exhausted positions are legal; iteration never wraps. */
    WorkbookHistorySegment end={UINT64_MAX,UINT64_MAX-1,UINT64_MAX};
    WorkbookHistoryEventPosition last={UINT64_MAX,UINT64_MAX,UINT64_MAX};
    p=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=&end,.segment_count=1,.events=&last,.event_count=1,
        .lineage_high_water=UINT64_MAX,.operation_high_water=UINT64_MAX,
        .origin=UINT64_MAX-1,.available=UINT64_MAX-1,.cursor=UINT64_MAX,.tail=UINT64_MAX};
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    p.origin=p.available=UINT64_MAX; end.from=UINT64_MAX; p.events=NULL; p.event_count=0;
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    end=(WorkbookHistorySegment){0}; p=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=&end,.segment_count=1};
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    const size_t n=100000; WorkbookHistoryEventPosition *many=calloc(n,sizeof(*many)); CHECK(many);
    for (size_t i=0;i<n;i++) many[i]=(WorkbookHistoryEventPosition){i,0,i+1};
    end.through=n; p.events=many; p.event_count=n; p.tail=p.cursor=n; p.operation_high_water=n-1;
    CHECK(workbook_history_path_check(&p)==HISTORY_PATH_OK);
    many[n/2].position++; CHECK(workbook_history_path_check(&p)==HISTORY_PATH_SEQUENCE); free(many);
    printf("PASS: %u structural history-path and joined retention checks\n",checks); return 0;
}
