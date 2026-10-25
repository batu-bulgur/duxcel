#include "workbook/sheet_metadata_replay.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/lifecycle_topology_internal.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL metadata replay:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(bool hidden) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){.kind=SOURCE_DATABASE,.path="/target-offline.duckdb"},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){.kind=SOURCE_DATABASE,.path="/offline.duckdb"},NULL));
    b.edit_session_id=100; b.sheets[0].edit_session_id=101; b.sheets[1].edit_session_id=102; b.sheets[0].hidden=hidden;
    CHECK(cells_set(&b.sheets[0].cells,10,8,"NULL",NULL)); worksheet_record(b.sheets,EDIT_CELLS); workbook_history_sync(&b); b.dirty=false; return b;
}
static void command(Workbook *b,uint64_t *epoch,SheetMetadataOperation operation) {
    SheetLifecycleCommandContext c={b,epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetLifecycleCommandPlan *p=NULL;
    SheetMetadataCommand command={operation,b->sheets[0].id,operation==SHEET_METADATA_REORDER ? b->sheets[1].id : 0,operation==SHEET_METADATA_RENAME ? "Renamed" : NULL};
    CHECK(sheet_lifecycle_command_prepare(c,command,&p,NULL)==SHEET_METADATA_READY && sheet_lifecycle_command_apply(c,p,NULL,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&p);
}
static void replay(SheetMetadataOperation operation,bool final) {
    uint64_t epoch=10; Workbook b=fixture(operation==SHEET_METADATA_SHOW); uint64_t id=b.sheets[0].id,other=b.sheets[1].id;
    command(&b,&epoch,operation); uint64_t order=worksheet_history_high_water(); b.sheet_high_water+=10; b.dirty=false;
    if (final) b.command_revision=UINT64_MAX-1;
    SheetMetadataReplayContext c={&b,&epoch,0,true,false}; SheetMetadataReplayMeasure usage; SheetMetadataReplayPlan *p=NULL;
    CHECK(sheet_metadata_replay_measure(c,false,&usage)==SHEET_METADATA_REPLAY_READY && usage.owned==usage.staging);
    Workbook before=b; CHECK(sheet_metadata_replay_prepare(c,false,&p)==SHEET_METADATA_REPLAY_LIMIT && !p && !memcmp(&b,&before,sizeof(b)));
    c.reserved=usage.staging; CHECK(sheet_metadata_replay_prepare(c,false,&p)==SHEET_METADATA_REPLAY_READY && p);
    CHECK(sheet_metadata_replay_prepare(c,false,&p)==SHEET_METADATA_REPLAY_INVALID && p);
    Worksheet *moved=dx_alloc(b.count*sizeof(Worksheet)); memcpy(moved,b.sheets,b.count*sizeof(Worksheet)); free(b.sheets); b.sheets=moved;
    size_t target=b.sheets[0].id==id ? 0 : 1; size_t next=b.sheets[target].hidden ? 1-target : target; b.active=next;
    b.sheets[target].view.row=9001; b.sheets[target].view.col=200; b.sheets[target].view.horizontal=123.25f; b.sheets[target].view.canvas_zoom=1.5f;
    CHECK(sheet_metadata_replay_check(c,p)==SHEET_METADATA_REPLAY_READY);
    epoch++; CHECK(sheet_metadata_replay_check(c,p)==SHEET_METADATA_REPLAY_STALE); epoch--;
    b.edit_session_id++; CHECK(sheet_metadata_replay_check(c,p)==SHEET_METADATA_REPLAY_STALE); b.edit_session_id--;
    c.read_only=true; CHECK(sheet_metadata_replay_check(c,p)==SHEET_METADATA_REPLAY_PROTECTED); c.read_only=false;
    uint64_t revision=b.command_revision,high=b.sheet_high_water; SheetCell *cells=b.sheets[target].cells.cells; Analysis source=b.sheets[1-target].analysis;
    c.reserved=0; CHECK(sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_APPLIED && !p && b.command_revision==revision+1 && worksheet_history_high_water()==order && b.sheet_high_water==high);
    target=b.sheets[0].id==id ? 0 : 1;
    CHECK(b.sheets[target].cells.cells==cells && !strcmp(cells_get(&b.sheets[target].cells,10,8),"NULL") && b.sheets[target].view.row==9001 && b.sheets[target].view.col==200 && b.sheets[target].view.horizontal==123.25f);
    CHECK(b.sheets[1-target].id==other && b.sheets[1-target].analysis.path==source.path && !b.sheets[b.active].hidden);
    WorkbookLifecycleMetadata selected; CHECK(workbook_lifecycle_metadata(&b,true,&selected) && selected.packet && selected.order==order);
    if (final) CHECK(sheet_metadata_replay_prepare(c,true,&p)==SHEET_METADATA_REPLAY_LIMIT && !p && b.command_revision==UINT64_MAX);
    else {
        c.reserved=SIZE_MAX; CHECK(sheet_metadata_replay_prepare(c,true,&p)==SHEET_METADATA_REPLAY_READY);
        if (!b.sheets[target].hidden) b.active=target;
        CHECK(sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_APPLIED && !p && b.command_revision==revision+2 && worksheet_history_high_water()==order && b.sheet_high_water==high && !b.sheets[b.active].hidden);
        CHECK(workbook_lifecycle_metadata(&b,false,&selected) && selected.packet && selected.order==order);
    }
    workbook_free(&b);
}
static void corruption_and_lifetime(void) {
    Workbook b=fixture(false); uint64_t epoch=40; command(&b,&epoch,SHEET_METADATA_RENAME);
    WorkbookLifecycleMetadata selected; CHECK(workbook_lifecycle_metadata(&b,false,&selected) && selected.packet);
    size_t bytes=selected.packet->bytes; const void *data=selected.packet->data;
    SheetMetadataReplayContext c={&b,&epoch,SIZE_MAX,true,false}; SheetMetadataReplayPlan *p=NULL;
    CHECK(sheet_metadata_replay_prepare(c,false,&p)==SHEET_METADATA_REPLAY_READY);
    unsigned char *raw=selected.packet->data; unsigned char old=raw[0]; raw[0]='!';
    CHECK(sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_INVALID && p && !strcmp(b.sheets[0].name,"Renamed")); raw[0]=old;
    /* Canonical corruption in the inactive side must not silently replace the
       independently prepared immutable pair. Before-name starts at byte80. */
    old=raw[DX_SHEET_LIFECYCLE_HEADER_BYTES+16]; raw[DX_SHEET_LIFECYCLE_HEADER_BYTES+16]='Z';
    CHECK(sheet_metadata_replay_check(c,p)==SHEET_METADATA_REPLAY_STALE); raw[DX_SHEET_LIFECYCLE_HEADER_BYTES+16]=old;
    CHECK(sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_APPLIED && !p);
    CHECK(workbook_lifecycle_metadata(&b,true,&selected) && selected.packet->data==data && selected.packet->bytes==bytes);
    CHECK(sheet_metadata_replay_prepare(c,true,&p)==SHEET_METADATA_REPLAY_READY);
    bool changed=false; CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=1,.name="New branch"},&changed,NULL) && changed);
    CHECK(sheet_metadata_replay_apply(c,&p,NULL)==SHEET_METADATA_REPLAY_STALE && p); sheet_metadata_replay_free(&p);
    size_t retained=99; CHECK(workbook_lifecycle_format_usage(&b,&retained)==FORMAT_BUDGET_OK && retained==0);
    CHECK(sheet_metadata_replay_prepare(c,false,&p)==SHEET_METADATA_REPLAY_UNSUPPORTED && !p); /* Plain current session metadata has no packet. */
    workbook_free(&b); sheet_metadata_replay_free(NULL);
}
int main(void) {
    for (unsigned i=0;i<4;i++) { replay((SheetMetadataOperation)i,false); replay((SheetMetadataOperation)i,true); }
    corruption_and_lifetime(); printf("PASS: %u guarded metadata session replay checks\n",checks); return 0;
}
