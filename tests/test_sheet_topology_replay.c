#include "workbook/sheet_topology_replay.h"
#include "workbook/sheet_topology_duplicate.h"
#include "workbook/sheet_topology_remove.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL topology replay:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SheetTopologyPacket copy(const SheetTopologyPacket *p) { SheetTopologyPacket out={dx_alloc(p->bytes),p->bytes}; memcpy(out.data,p->data,p->bytes); return out; }
static Workbook fixture(void) {
    Workbook b; workbook_init(&b); Analysis actual={.kind=SOURCE_DATABASE,.path="/offline.duckdb",.schema="main",.table="alpha"};
    CHECK(workbook_add(&b,"Base",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"Data",&actual,NULL));
    Worksheet *s=b.sheets+1; CHECK(cells_set(&s->cells,10,8,"NULL",NULL)); CHECK(cells_set(&s->cells,11,8,"9007199254740993.123456789",NULL)); worksheet_record(s,EDIT_CELLS);
    CellFormat value=format_default(); value.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){10,10,8,8},FORMAT_BOLD,value,NULL)); worksheet_record(s,EDIT_FORMAT);
    s->view.column_count=1; s->view.columns=dx_alloc(sizeof(char *)); s->view.widths=dx_alloc(sizeof(float)); s->view.columns[0]=dx_strdup("Field"); s->view.widths[0]=1199.75f;
    CHECK(workbook_prepare_command_session(&b,1,NULL)); b.dirty=false; return b;
}
static SheetTopologyPacket command(Workbook *b,WorkbookSheetOperation operation,uint64_t *id,uint64_t *order,Analysis *actual,uint64_t *epoch) {
    *actual=analysis_copy(&b->sheets[1].analysis); *id=b->sheets[1].id; SheetTopologyPacket packet={0};
    if (operation==SHEET_DELETE) {
        SheetTopologyRemovePlan *p=NULL; SheetTopologyRemoveContext c={b,epoch,actual,*id,SIZE_MAX,true,false,SIZE_MAX};
        CHECK(sheet_topology_remove_prepare(c,*id,&p,NULL)==SHEET_REMOVE_READY); packet=copy(sheet_topology_remove_packet(p)); CHECK(sheet_topology_remove_apply(c,&p,NULL,0,NULL)==SHEET_REMOVE_APPLIED);
    } else if (operation==SHEET_DUPLICATE) {
        SheetTopologyDuplicatePlan *p=NULL; SheetTopologyDuplicateContext c={b,epoch,actual,*id,SIZE_MAX,true,false,SIZE_MAX};
        CHECK(sheet_topology_duplicate_prepare(c,*id,&p,NULL)==SHEET_DUPLICATE_READY); packet=copy(sheet_topology_duplicate_packet(p)); *id=sheet_topology_duplicate_presence(p).sheet->id; CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED);
    } else {
        SheetLifecycleEntry rows[DX_MAX_SHEETS],after_rows[DX_MAX_SHEETS]; SheetLifecycleState before,after; char names[DX_MAX_SHEETS][32];
        CHECK(sheet_lifecycle_from_workbook(b,rows,&before)==SHEET_LIFECYCLE_OK);
        for (size_t i=0;i<before.count;i++) { snprintf(names[i],sizeof(names[i]),"%s",rows[i].name); rows[i].name=names[i]; }
        bool changed=false; CHECK(workbook_sheet_command(b,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Created",.analysis=actual},&changed,NULL) && changed);
        *id=b->sheets[b->active].id; CHECK(sheet_lifecycle_from_workbook(b,after_rows,&after)==SHEET_LIFECYCLE_OK);
        SheetPresenceAsset *asset=NULL; SheetPresencePacket presence={0}; CHECK(sheet_presence_capture((SheetPresenceView){b->id,b->sheet_high_water,b->sheets+b->active,actual},SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
        CHECK(sheet_presence_encode(asset,SIZE_MAX,&presence)==SHEET_PRESENCE_OK); CHECK(sheet_topology_capture((SheetTopologyView){SHEET_TOPOLOGY_ADD,*id,before,after,&presence},SIZE_MAX,&packet)==SHEET_TOPOLOGY_OK);
        sheet_presence_free(&asset); sheet_presence_packet_free(&presence);
    }
    *order=worksheet_history_high_water(); return packet;
}
static void replay(WorkbookSheetOperation operation,bool final_revision) {
    Workbook b=fixture(); uint64_t epoch=10,id,order; Analysis actual={0}; SheetTopologyPacket packet=command(&b,operation,&id,&order,&actual,&epoch);
    b.sheet_high_water+=10; b.active=0; b.dirty=false; if (final_revision) b.command_revision=UINT64_MAX-1;
    WorkbookLifecyclePresence navigated; CHECK(workbook_lifecycle_presence(&b,false,&navigated));
    Worksheet *current=(Worksheet *)navigated.worksheet;
    current->view.row=123; current->view.col=21; current->view.top=100; current->view.horizontal=43.25f; current->view.fraction=.25; current->view.canvas_zoom=1.5f;
    SheetTopologyReplayContext c={&b,&epoch,&actual,id,0,0,true,false}; SheetTopologyReplayPlan *p=NULL; SheetTopologyReplayMeasure usage;
    CHECK(sheet_topology_replay_measure(&packet,&usage)==SHEET_REPLAY_READY && usage.staging>usage.owned);
    Workbook before=b; CHECK(sheet_topology_replay_prepare(c,&packet,false,order,&p,NULL)==SHEET_REPLAY_LIMIT && !p && !memcmp(&b,&before,sizeof(b)));
    c.reserved=usage.staging; CHECK(sheet_topology_replay_prepare(c,&packet,false,order+1,&p,NULL)==SHEET_REPLAY_STALE && !p);
    CHECK(sheet_topology_replay_prepare(c,&packet,false,order,&p,NULL)==SHEET_REPLAY_READY && p);
    current->view.row=456; current->view.col=22; current->view.anchor_row=400; current->view.anchor_col=20; current->view.end_row=500; current->view.end_col=24; current->view.selection=1; current->view.horizontal=64.5f; current->view.canvas_zoom=2;
    CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_READY);
    epoch++; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); epoch--;
    c.read_only=true; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_PROTECTED); c.read_only=false;
    WorkbookLifecyclePresence selected; CHECK(workbook_lifecycle_presence(&b,false,&selected));
    Worksheet *s=(Worksheet *)selected.worksheet; char *table=s->analysis.table; s->analysis.table=NULL;
    CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_PROTECTED); s->analysis.table=table;
    if (s->view.column_count) { char first=s->view.columns[0][0]; s->view.columns[0][0]='Z'; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); s->view.columns[0][0]=first; s->view.widths[0]=96; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); s->view.widths[0]=1199.75f; }
    c.adoption_reserved=workbook_lifecycle_presence_staging(&b,false)-1;
    CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_LIMIT && p && !memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
    c.adoption_reserved=workbook_lifecycle_presence_staging(&b,false); uint64_t revision=b.command_revision,high=b.sheet_high_water; size_t count=b.count;
    CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_APPLIED && !p && b.command_revision==revision+1 && b.sheet_high_water==high && worksheet_history_high_water()==order && b.count==(operation==SHEET_DELETE ? count+1 : count-1));
    CHECK(workbook_lifecycle_presence(&b,true,&selected) && selected.order==order && selected.sheet==id);
    CHECK(selected.worksheet->view.row==456 && selected.worksheet->view.col==22 && selected.worksheet->view.horizontal==64.5f && selected.worksheet->view.canvas_zoom==2 && selected.worksheet->view.selection==1);
    CHECK(!b.sheets[b.active].hidden);
    if (final_revision) { CHECK(b.command_revision==UINT64_MAX && sheet_topology_replay_prepare(c,&packet,true,order,&p,NULL)==SHEET_REPLAY_LIMIT && !p); }
    else {
        CHECK(sheet_topology_replay_prepare(c,&packet,true,order,&p,NULL)==SHEET_REPLAY_READY);
        /* Input packet ownership is independent once preparation succeeds. */
        sheet_topology_packet_free(&packet);
        c.adoption_reserved=workbook_lifecycle_presence_staging(&b,true);
        CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_APPLIED && !p && b.count==count && b.command_revision==revision+2 && b.sheet_high_water==high && worksheet_history_high_water()==order);
        CHECK(workbook_lifecycle_presence(&b,false,&selected));
        CHECK(selected.worksheet->id==id && !strcmp(selected.worksheet->analysis.table,"alpha"));
    }
    sheet_topology_packet_free(&packet); analysis_free(&actual); workbook_free(&b);
}
static void fences(void) {
    Workbook b=fixture(); uint64_t epoch=30,id,order; Analysis actual={0}; SheetTopologyPacket packet=command(&b,SHEET_DELETE,&id,&order,&actual,&epoch);
    SheetTopologyReplayContext c={&b,&epoch,&actual,id,SIZE_MAX,SIZE_MAX,true,false}; SheetTopologyReplayPlan *p=NULL;
    CHECK(sheet_topology_replay_prepare(c,&packet,false,order,&p,NULL)==SHEET_REPLAY_READY);
    Worksheet *moved=dx_alloc(b.count*sizeof(Worksheet)); memcpy(moved,b.sheets,b.count*sizeof(Worksheet)); free(b.sheets); b.sheets=moved;
    CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_READY);
    b.edit_session_id++; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); b.edit_session_id--;
    b.command_revision++; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); b.command_revision--;
    b.sheet_high_water++; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); b.sheet_high_water--;
    uint64_t primary=c.primary_sheet; c.primary_sheet=b.sheets[0].id; CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); c.primary_sheet=primary;
    WorkbookLifecyclePresence selected; CHECK(workbook_lifecycle_presence(&b,false,&selected) && selected.detached);
    Worksheet *detached=(Worksheet *)selected.worksheet;
    CHECK(cells_set(&detached->cells,30,9,"Unpaired",NULL));
    CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_INVALID && p && b.count==1);
    cells_history(&detached->cells,false); /* Fixture restores the unpaired store. */
    cells_trim_history(&detached->cells,true,0);
    CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_STALE); /* Store revision advanced. */
    sheet_topology_replay_free(&p);
    CHECK(sheet_topology_replay_prepare(c,&packet,false,order,&p,NULL)==SHEET_REPLAY_READY);
    CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_APPLIED);
    CHECK(sheet_topology_replay_prepare(c,&packet,true,order,&p,NULL)==SHEET_REPLAY_READY);
    /* Live Worksheet relocation is independent of its logical owner. */
    moved=dx_alloc(b.count*sizeof(Worksheet)); memcpy(moved,b.sheets,b.count*sizeof(Worksheet)); free(b.sheets); b.sheets=moved;
    CHECK(sheet_topology_replay_check(c,p)==SHEET_REPLAY_READY);
    bool changed=false; CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name="Later"},&changed,NULL) && changed);
    CHECK(sheet_topology_replay_apply(c,&p,NULL)==SHEET_REPLAY_STALE && p && b.count==2); sheet_topology_replay_free(&p);
    CHECK(sheet_topology_replay_prepare(c,&packet,false,order,&p,NULL)==SHEET_REPLAY_STALE && !p);
    sheet_topology_packet_free(&packet); analysis_free(&actual); workbook_free(&b);
}
int main(void) {
    fences(); replay(SHEET_CREATE,false); replay(SHEET_DELETE,false); replay(SHEET_DUPLICATE,false); replay(SHEET_CREATE,true); replay(SHEET_DELETE,true); replay(SHEET_DUPLICATE,true);
    SheetTopologyReplayMeasure untouched={7,9}; CHECK(sheet_topology_replay_measure(&(SheetTopologyPacket){"bad",3},&untouched)==SHEET_REPLAY_INVALID && untouched.owned==7 && untouched.staging==9);
    sheet_topology_replay_free(NULL); printf("PASS: %u genuine topology session replay checks\n",checks); return 0;
}
