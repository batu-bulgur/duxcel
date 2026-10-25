#include "workbook/workbook_width_command.h"
#include "workbook/cell_command.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/sheet_metadata_replay.h"
#include "workbook/sheet_topology_duplicate.h"
#include "workbook/sheet_topology_remove.h"
#include "workbook/sheet_topology_replay.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet chronology:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char *exact="9007199254740993.12345678901234567890";
static size_t index_of(const Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return i; return b->count; }
static Workbook fixture(uint64_t *id) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL)); b.active=0; *id=b.sheets[0].id; b.dirty=false; return b;
}
static void width(Workbook *b,uint64_t *epoch,uint64_t id,float value) {
    WorkbookWidthContext c={.book=b,.request_epoch=epoch,.reserved=SIZE_MAX,.ready=true}; WorkbookWidthTarget target;
    CHECK(workbook_prepare_width_command(&c,index_of(b,id),&target,NULL)); ColumnWidthEntry entry={16383,column_width_bits(value)};
    WorkbookWidthResult applied=workbook_apply_width_command_recorded(&c,&target,(ColumnWidthView){&entry,1},NULL,NULL);
    CHECK(applied.status==COLUMN_WIDTH_OK && applied.effects==16);
}
static void cell(Workbook *b,uint64_t id,const char *text) {
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(b,index_of(b,id),20,8,&target,NULL));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,text,strlen(text)};
    CHECK(workbook_apply_cell_edit(b,&command,(CellEditPolicy){.source_ready=true},NULL).status==CELL_EDIT_APPLIED);
}
static void metadata(Workbook *b,uint64_t *epoch,uint64_t id,SheetMetadataOperation operation,const char *name) {
    SheetLifecycleCommandContext c={b,epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetLifecycleCommandPlan *p=NULL;
    CHECK(sheet_lifecycle_command_prepare(c,(SheetMetadataCommand){operation,id,0,name},&p,NULL)==SHEET_METADATA_READY);
    CHECK(sheet_lifecycle_command_apply(c,p,NULL,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&p);
}
static uint64_t duplicate(Workbook *b,uint64_t *epoch,uint64_t id) {
    size_t at=index_of(b,id); SheetTopologyDuplicateContext c={b,epoch,&b->sheets[at].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *p=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY); uint64_t added=sheet_topology_duplicate_presence(p).sheet->id;
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED); return added;
}
static void remove_sheet(Workbook *b,uint64_t *epoch,uint64_t id) {
    size_t at=index_of(b,id); SheetTopologyRemoveContext c={b,epoch,&b->sheets[at].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyRemovePlan *p=NULL;
    CHECK(sheet_topology_remove_prepare(c,id,&p,NULL)==SHEET_REMOVE_READY && sheet_topology_remove_apply(c,&p,NULL,0,NULL)==SHEET_REMOVE_APPLIED);
}
static void step(Workbook *b,uint64_t *epoch,bool redo,uint64_t expected) {
    WorkbookHistoryResult selected=workbook_history_peek(b,redo,NULL); CHECK(selected.status==WORKBOOK_HISTORY_READY && selected.order==expected);
    uint64_t issued=worksheet_history_high_water(),revision=b->command_revision,high=b->sheet_high_water;
    if (!selected.lifecycle) CHECK(workbook_history_step(b,redo,NULL).status==WORKBOOK_HISTORY_APPLIED);
    else {
        WorkbookLifecycleMetadata metadata;
        if (workbook_lifecycle_metadata(b,redo,&metadata) && metadata.packet) {
            SheetMetadataReplayContext c={b,epoch,SIZE_MAX,true,false}; SheetMetadataReplayPlan *p=NULL;
            CHECK(sheet_metadata_replay_prepare(c,redo,&p)==SHEET_METADATA_REPLAY_READY && sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_APPLIED && !p);
        } else {
            WorkbookLifecyclePresence presence; CHECK(workbook_lifecycle_presence(b,redo,&presence) && presence.packet);
            SheetTopologyReplayContext c={b,epoch,&presence.worksheet->analysis,presence.sheet,SIZE_MAX,SIZE_MAX,true,false}; SheetTopologyReplayPlan *p=NULL;
            CHECK(sheet_topology_replay_prepare(c,presence.packet,redo,presence.order,&p,NULL)==SHEET_REPLAY_READY && sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_APPLIED && !p);
        }
    }
    CHECK(b->command_revision==revision+1 && b->sheet_high_water==high && worksheet_history_high_water()==issued && !b->sheets[b->active].hidden);
}
static void mixed(void) {
    uint64_t a,epoch=1,orders[8]; Workbook b=fixture(&a);
    width(&b,&epoch,a,160); orders[0]=worksheet_history_high_water();
    cell(&b,a,exact); orders[1]=worksheet_history_high_water();
    metadata(&b,&epoch,a,SHEET_METADATA_RENAME,"Renamed"); orders[2]=worksheet_history_high_water();
    uint64_t copied=duplicate(&b,&epoch,a); orders[3]=worksheet_history_high_water();
    size_t copy=index_of(&b,copied); CHECK(!b.sheets[copy].width_history && !b.sheets[copy].undo_actions_count && !strcmp(cells_get(&b.sheets[copy].cells,20,8),exact));
    cell(&b,copied,"NULL"); orders[4]=worksheet_history_high_water();
    width(&b,&epoch,copied,52); orders[5]=worksheet_history_high_water();
    metadata(&b,&epoch,copied,SHEET_METADATA_HIDE,NULL); orders[6]=worksheet_history_high_water();
    remove_sheet(&b,&epoch,a); orders[7]=worksheet_history_high_water();
    CHECK(b.command_revision==8 && b.count==2 && b.sheet_high_water==copied && index_of(&b,a)==b.count);
    for (unsigned i=8;i;i--) step(&b,&epoch,false,orders[i-1]);
    CHECK(b.count==2 && b.sheet_high_water==copied && index_of(&b,copied)==b.count && !strcmp(b.sheets[index_of(&b,a)].name,"A") && !*cells_get(&b.sheets[index_of(&b,a)].cells,20,8));
    CHECK(b.sheets[index_of(&b,a)].view.widths[16383]==96 && workbook_history_peek(&b,false,NULL).status==WORKBOOK_HISTORY_EMPTY);
    WorkbookLifecyclePresence detached; CHECK(workbook_lifecycle_presence(&b,true,&detached)==false); /* Earliest Redo is real width, not topology. */
    for (unsigned i=0;i<8;i++) step(&b,&epoch,true,orders[i]);
    copy=index_of(&b,copied); CHECK(b.count==2 && index_of(&b,a)==b.count && b.sheets[copy].hidden && !strcmp(cells_get(&b.sheets[copy].cells,20,8),"NULL") && b.sheets[copy].view.widths[16383]==52);
    CHECK(workbook_history_peek(&b,true,NULL).status==WORKBOOK_HISTORY_EMPTY && b.command_revision==24);
    Workbook *snapshot=workbook_snapshot(&b); CHECK(snapshot && !snapshot->lifecycle && !snapshot->sheets[0].width_history && !snapshot->sheets[0].undo_actions_count); workbook_free(snapshot); free(snapshot);
    workbook_free(&b);
}
static void abandon_detached(void) {
    uint64_t a,epoch=2; Workbook b=fixture(&a); width(&b,&epoch,a,160); cell(&b,a,exact);
    uint64_t copied=duplicate(&b,&epoch,a),duplicate_order=worksheet_history_high_water();
    cell(&b,copied,"NULL"); uint64_t cell_order=worksheet_history_high_water(); width(&b,&epoch,copied,52); uint64_t width_order=worksheet_history_high_water();
    step(&b,&epoch,false,width_order); step(&b,&epoch,false,cell_order); step(&b,&epoch,false,duplicate_order);
    WorkbookLifecyclePresence detached; CHECK(workbook_lifecycle_presence(&b,true,&detached) && detached.detached && detached.sheet==copied && detached.worksheet->cells.redo_count==1 && column_width_history_info(detached.worksheet->width_history).redo==1);
    SheetTopologyReplayContext c={&b,&epoch,&detached.worksheet->analysis,copied,SIZE_MAX,SIZE_MAX,true,false}; SheetTopologyReplayPlan *held=NULL;
    CHECK(sheet_topology_replay_prepare(c,detached.packet,true,detached.order,&held,NULL)==SHEET_REPLAY_READY);
    metadata(&b,&epoch,a,SHEET_METADATA_RENAME,"New branch");
    c.primary=&b.sheets[index_of(&b,a)].analysis; /* Rebuild borrowed context after branch disposal. */
    CHECK(sheet_topology_replay_check(c,held)==SHEET_REPLAY_STALE && workbook_history_peek(&b,true,NULL).status==WORKBOOK_HISTORY_EMPTY); sheet_topology_replay_free(&held);
    CHECK(b.count==2 && b.sheet_high_water==copied && !strcmp(cells_get(&b.sheets[index_of(&b,a)].cells,20,8),exact));
    uint64_t next=duplicate(&b,&epoch,a); CHECK(next==copied+1 && b.sheet_high_water==next && index_of(&b,copied)==b.count);
    workbook_free(&b);
}
int main(void) {
    mixed(); abandon_detached(); printf("PASS: %u mixed live/detached session chronology checks\n",checks); return 0;
}
