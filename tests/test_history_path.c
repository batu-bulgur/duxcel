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
static void protection_rejected_at(int line,const WorkbookVersionIndex *v,const WorkbookHistoryAnchors *h,const WorkbookHistoryPath *s,const WorkbookHistoryPath *c,WorkbookHistoryPathStatus status) {
    WorkbookHistoryProtection out; memset(&out,0xa5,sizeof(out)); WorkbookHistoryProtection before=out;
    WorkbookHistoryPathStatus got=workbook_history_protect(v,h,s,c,&out);
    if (got!=status) fprintf(stderr,"Protection case:%d: expected %d, got %d\n",line,status,got);
    CHECK(got==status && !memcmp(&out,&before,sizeof(out)));
}
#define protection_rejected(...) protection_rejected_at(__LINE__,__VA_ARGS__)
static void test_protection(void) {
    WorkbookVersionRoot root={.bytes=1,.encoding=1};
    WorkbookVersionTag tags[2]={{{"V1",2},{{0}},1},{{"V2",2},{{0}},2}};
    WorkbookVersionIndex v={.document_high=1,.document_low=2,.generation=3,.saved_generation=3,
        .saved={true,{{0}}},.roots=&root,.root_count=1,.tags=tags,.tag_count=2};
    WorkbookHistoryAnchor items[3]={anchor(0,1,0),anchor(1,3,0),anchor(2,5,0)};
    WorkbookHistoryTagAnchor bindings[2]={{1,0},{2,1}};
    WorkbookHistoryAnchors h={.document_high=1,.document_low=2,.high_water=2,.saved_anchor=2,
        .items=items,.count=3,.tags=bindings,.tag_count=2};
    WorkbookHistorySegment old_segments[2]={{0,0,6},{8,3,6}},draft_segments[2]={{0,0,2},{8,2,4}};
    WorkbookHistoryEventPosition old_events[6],draft_events[4]={{0,0,1},{1,0,2},{9,8,3},{10,8,4}};
    for (size_t i=0;i<6;i++) old_events[i]=(WorkbookHistoryEventPosition){i,0,i+1};
    WorkbookHistoryPath saved={.document_high=1,.document_low=2,.segments=old_segments,.segment_count=1,
        .events=old_events,.event_count=6,.operation_high_water=5,.cursor=5,.tail=6};
    WorkbookHistoryPath current={.document_high=1,.document_low=2,.segments=draft_segments,.segment_count=2,
        .events=draft_events,.event_count=4,.lineage_high_water=8,.operation_high_water=10,.cursor=4,.tail=4};
    WorkbookHistoryProtection out;
    CHECK(workbook_history_path_window(&v,&h,&current,&out.saved)==HISTORY_PATH_FORKED_SAVED);
    CHECK(workbook_history_protect(&v,&h,&saved,&current,&out)==HISTORY_PATH_OK && out.has_saved &&
        out.saved.from==1 && out.saved.through==6 && out.current_from==1 && out.current_cursor==4 && out.current_through==4 && out.shared_through==2);
    /* The fork predates all selected Saved states: preserve it as the inverse
       of the new draft edit instead of moving the floor to its after-position. */
    items[0].boundary.position=3; items[1].boundary.position=4;
    CHECK(workbook_history_protect(&v,&h,&saved,&current,&out)==HISTORY_PATH_OK && out.saved.from==2 && out.current_from==2 && out.saved.count==3);
    current.cursor=1;
    CHECK(workbook_history_protect(&v,&h,&saved,&current,&out)==HISTORY_PATH_OK && out.current_from==1 && out.saved.from==1);
    current.cursor=4;
    saved.available=2; saved.events=old_events+2; saved.event_count=4;
    current.available=2; current.events=draft_events+2; current.event_count=2;
    CHECK(workbook_history_protect(&v,&h,&saved,&current,&out)==HISTORY_PATH_OK && out.saved.from==2 && out.current_from==2);
    saved.available=3; saved.events=old_events+3; saved.event_count=3;
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_GAP);
    saved.available=2; saved.events=old_events+2; saved.event_count=4;
    current.available=3; current.events=draft_events+3; current.event_count=1;
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_GAP);
    saved.available=current.available=0; saved.events=old_events; saved.event_count=6; current.events=draft_events; current.event_count=4;
    draft_events[1].operation=2; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_SEQUENCE); draft_events[1].operation=1;
    draft_events[2].operation=2; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_SEQUENCE);
    draft_events[2].operation=3; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_SEQUENCE); draft_events[2].operation=9;
    current.document_low=3; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID); current.document_low=2;
    saved.lineage_high_water=20; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID); saved.lineage_high_water=0;
    saved.operation_high_water=20; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID); saved.operation_high_water=5;
    current.event_count=3; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_GAP); current.event_count=4;
    protection_rejected(&v,&h,NULL,&current,HISTORY_PATH_INVALID);
    protection_rejected(&v,&h,&saved,NULL,HISTORY_PATH_INVALID);
    CHECK(workbook_history_protect(&v,&h,&saved,&current,NULL)==HISTORY_PATH_INVALID);
    h.document_low=3; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_ANCHORS); h.document_low=2;
    saved.lineage_high_water=8; items[1]=anchor(1,1,8);
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID);
    items[1].boundary.position=5; protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID);
    items[1]=anchor(1,4,0); saved.lineage_high_water=0;
    /* Same current/Saved path after Undo still protects the older active cursor
       and complete Redo; the two input roles are not inferred from content. */
    WorkbookHistoryPath same=saved; same.cursor=2;
    CHECK(workbook_history_protect(&v,&h,&saved,&same,&out)==HISTORY_PATH_OK && out.shared_through==6 && out.saved.from==2 && out.current_from==2 && out.current_through==6);
    /* A descendant draft shares the whole Saved tail before adding an event. */
    WorkbookHistorySegment extension={0,0,7}; WorkbookHistoryEventPosition extended[7];
    memcpy(extended,old_events,sizeof(old_events)); extended[6]=(WorkbookHistoryEventPosition){6,0,7};
    same=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=&extension,.segment_count=1,
        .events=extended,.event_count=7,.operation_high_water=6,.cursor=7,.tail=7};
    CHECK(workbook_history_protect(&v,&h,&saved,&same,&out)==HISTORY_PATH_OK && out.shared_through==6 && out.saved.from==3 && out.current_from==3 && out.current_through==7);
    /* An older Saved/current fork cannot rejoin a later lineage ID. */
    old_segments[0].through=3; saved.segment_count=2; saved.lineage_high_water=8;
    for (size_t i=3;i<6;i++) old_events[i].lineage=8;
    items[1].lineage=items[2].lineage=8; items[2].boundary.position=4;
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_SEQUENCE);
    saved.segment_count=1; old_segments[0].through=6; saved.lineage_high_water=0;
    for (size_t i=3;i<6;i++) old_events[i].lineage=0;
    items[1].lineage=items[2].lineage=0; items[2].boundary.position=5;
    draft_segments[0].lineage=1; draft_events[0].lineage=draft_events[1].lineage=1;
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_SEQUENCE);
    WorkbookHistorySegment last_segment={UINT64_MAX,UINT64_MAX-1,UINT64_MAX};
    WorkbookHistoryEventPosition last_event={UINT64_MAX,UINT64_MAX,UINT64_MAX};
    saved=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=&last_segment,.segment_count=1,
        .events=&last_event,.event_count=1,.lineage_high_water=UINT64_MAX,.operation_high_water=UINT64_MAX,
        .origin=UINT64_MAX-1,.available=UINT64_MAX-1,.cursor=UINT64_MAX,.tail=UINT64_MAX};
    items[0]=anchor(0,UINT64_MAX-1,UINT64_MAX); items[1]=anchor(1,UINT64_MAX-1,UINT64_MAX); items[2]=anchor(2,UINT64_MAX,UINT64_MAX);
    same=saved; same.cursor=UINT64_MAX-1;
    CHECK(workbook_history_protect(&v,&h,&saved,&same,&out)==HISTORY_PATH_OK && out.shared_through==UINT64_MAX && out.current_from==UINT64_MAX-1);
    last_segment=(WorkbookHistorySegment){0}; saved=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=&last_segment,.segment_count=1};
    for (size_t i=0;i<3;i++) items[i]=anchor(i,0,0);
    WorkbookHistorySegment first_segments[2]={{0,0,0},{8,0,1}}; WorkbookHistoryEventPosition first_event={0,8,1};
    same=(WorkbookHistoryPath){.document_high=1,.document_low=2,.segments=first_segments,.segment_count=2,.events=&first_event,.event_count=1,.lineage_high_water=8,.cursor=1,.tail=1};
    CHECK(workbook_history_protect(&v,&h,&saved,&same,&out)==HISTORY_PATH_OK && !out.shared_through && !out.current_from && out.current_through==1);
    /* Initial draft has no Saved path. A supplied checkpoint descriptor is not
       ignored or converted to fictional old reversible operations. */
    v=(WorkbookVersionIndex){.document_high=1,.document_low=2}; h=(WorkbookHistoryAnchors){.document_high=1,.document_low=2};
    CHECK(workbook_history_protect(&v,&h,NULL,&current,&out)==HISTORY_PATH_OK && !out.has_saved && !out.current_from && out.current_through==4);
    protection_rejected(&v,&h,&saved,&current,HISTORY_PATH_INVALID);
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
    test_protection();
    printf("PASS: %u structural history-path and joined retention checks\n",checks); return 0;
}
