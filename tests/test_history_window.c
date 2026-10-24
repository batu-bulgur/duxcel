#include "workbook/history_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history window:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryBoundary boundary(uint64_t at,unsigned key) { WorkbookHistoryBoundary b={.anchor=at,.position=at}; b.root.bytes[0]=(unsigned char)key; return b; }
static void unchanged(WorkbookHistoryWindowInput input,WorkbookHistoryWindowStatus expected) {
    WorkbookHistoryWindow out; memset(&out,0xa5,sizeof(out)); WorkbookHistoryWindow before=out;
    CHECK(workbook_history_window(input,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
int main(void) {
    WorkbookHistoryBoundary named[DX_VERSION_TAGS];
    for (size_t i=0;i<10;i++) named[i]=boundary((i+1)*10,(unsigned)i+1);
    WorkbookHistoryWindowInput in={.named=named,.count=10,.has_saved=true,.saved=named[9],.origin=0,.available=0,.cursor=107,.tail=110}; WorkbookHistoryWindow w={0};
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==3 && w.from==80 && w.through==110 && w.cursor==107);
    CHECK(w.states[0].position==80 && w.states[1].position==90 && w.states[2].position==100);
    /* Ordinary Save advances the one endpoint, without multiplying versions. */
    in.saved=boundary(107,11); CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==90 && w.states[1].position==100 && w.states[2].position==107);
    in.saved=boundary(109,12); CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==90 && w.through==110);
    /* Save after Undo is a later version boundary at an older operation
       position. Keep the latest named states and their Redo payloads; an even
       older active cursor stays reachable too. */
    in.saved=named[5]; in.saved.anchor=200; in.cursor=60; CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==60 && w.through==110 && w.states[0].position==90 && w.states[1].position==100);
    in.cursor=25; CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==25 && w.states[0].position==90);
    in.available=26; unchanged(in,HISTORY_WINDOW_INVALID); in.available=0;
    in.cursor=107; in.available=61; unchanged(in,HISTORY_WINDOW_GAP); in.available=0;
    /* Aliased labels share an explicit boundary even in arbitrary array order. */
    named[0]=boundary(20,0); named[1]=boundary(10,1); named[2]=named[0]; named[3]=boundary(30,2); named[4]=named[3];
    in=(WorkbookHistoryWindowInput){.named=named,.count=5,.has_saved=true,.saved=named[3],.cursor=35,.tail=40};
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==3 && w.from==10 && w.states[1].position==20);
    /* Equal content is not an operation anchor: A -> B -> A keeps both spans. */
    named[3].root=named[1].root; named[4]=named[3]; in.saved=named[3];
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==3 && w.from==10 && w.states[2].position==30);
    named[4].root.bytes[1]=1; unchanged(in,HISTORY_WINDOW_INVALID); named[4]=named[3];
    in.saved.root.bytes[2]=1; unchanged(in,HISTORY_WINDOW_INVALID); in.saved=named[3];
    /* Fewer boundaries retain every available action from the actual origin. */
    in.count=1; in.origin=5; in.available=5;
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==2 && w.from==5);
    in.available=6; unchanged(in,HISTORY_WINDOW_GAP); in.available=5;
    in.count=0; CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==1 && w.from==5);
    in.has_saved=false; CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && !w.count && w.from==5 && w.through==40);
    in.count=1; unchanged(in,HISTORY_WINDOW_INVALID); in.count=0;
    in.available=41; unchanged(in,HISTORY_WINDOW_INVALID); in.available=5;
    in.origin=6; unchanged(in,HISTORY_WINDOW_INVALID); in.origin=5;
    in.cursor=41; unchanged(in,HISTORY_WINDOW_INVALID); in.cursor=35;
    in.has_saved=true; in.saved=boundary(4,1); unchanged(in,HISTORY_WINDOW_INVALID);
    in.saved=boundary(41,1); unchanged(in,HISTORY_WINDOW_INVALID); in.saved=boundary(30,1);
    in.count=1; in.named=NULL; unchanged(in,HISTORY_WINDOW_INVALID); in.named=named;
    in.count=DX_VERSION_TAGS+1; unchanged(in,HISTORY_WINDOW_LIMIT);
    in.count=1; named[0]=boundary(41,1); unchanged(in,HISTORY_WINDOW_INVALID); named[0]=boundary(4,1); unchanged(in,HISTORY_WINDOW_INVALID);
    CHECK(workbook_history_window(in,NULL)==HISTORY_WINDOW_INVALID);
    /* Presentation-only states can have different roots at one operation
       position, while aliases of an explicit anchor must agree exactly. */
    named[0]=boundary(10,1); named[0].position=20;
    named[1]=boundary(20,2); named[1].position=20;
    in=(WorkbookHistoryWindowInput){.named=named,.count=2,.has_saved=true,.saved=boundary(30,3),.cursor=30,.tail=40};
    in.saved.position=20;
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==3 && w.from==20);
    /* A new named boundary after Undo can move backwards on this path. Protect
       the minimum position across all selected states, not the oldest label. */
    named[1].position=10; in.saved.position=30;
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==10 && w.states[0].position==20 && w.states[1].position==10);
    named[1].anchor=31; unchanged(in,HISTORY_WINDOW_INVALID); named[1].anchor=20;
    named[1].anchor=10; unchanged(in,HISTORY_WINDOW_INVALID);
    /* Exhausted clocks and all-zero opaque roots remain valid, no +/-1 wraps. */
    named[0]=boundary(0,0); named[1]=boundary(UINT64_MAX-1,1);
    in=(WorkbookHistoryWindowInput){.named=named,.count=2,.has_saved=true,.saved=boundary(UINT64_MAX,2),.cursor=UINT64_MAX,.tail=UINT64_MAX};
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.count==3 && !w.from && w.through==UINT64_MAX);
    for (size_t i=0;i<DX_VERSION_TAGS;i++) named[i]=boundary(i+1,(unsigned)i);
    in=(WorkbookHistoryWindowInput){.named=named,.count=DX_VERSION_TAGS,.has_saved=true,.saved=boundary(DX_VERSION_TAGS+1,0),.cursor=DX_VERSION_TAGS+2,.tail=DX_VERSION_TAGS+3};
    CHECK(workbook_history_window(in,&w)==HISTORY_WINDOW_OK && w.from==DX_VERSION_TAGS-1 && w.count==3);
    printf("PASS: %u exact three-state history retention checks\n",checks); return 0;
}
