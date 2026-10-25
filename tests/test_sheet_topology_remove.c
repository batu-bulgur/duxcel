#include "workbook/sheet_topology_remove.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet remove preparation:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(void) {
    Workbook b; workbook_init(&b); Analysis source={.kind=SOURCE_DATABASE,.path="/offline.duckdb"};
    CHECK(workbook_add(&b,"Base",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"Gone",&source,NULL)); CHECK(workbook_add(&b,"Third",&(Analysis){0},NULL)); b.active=1;
    Worksheet *s=b.sheets+1; char *error=NULL;
    CHECK(cells_set(&s->cells,100,8,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
    CellFormat value=format_default(); value.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){100,100,8,8},FORMAT_BOLD,value,&error)); worksheet_record(s,EDIT_FORMAT);
    s->view.column_count=2; s->view.columns=dx_alloc(2*sizeof(char *)); s->view.widths=dx_alloc(2*sizeof(float)); s->view.columns[0]=dx_strdup("A"); s->view.columns[1]=dx_strdup("B"); s->view.widths[0]=120; s->view.widths[1]=96; s->view.top=3;
    CHECK(workbook_prepare_command_session(&b,1,&error) && !error); b.dirty=false; return b;
}
int main(void) {
    Workbook b=fixture(); uint64_t epoch=10,id=b.sheets[1].id; Analysis actual=analysis_copy(&b.sheets[1].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    SheetTopologyRemoveContext c={&b,&epoch,&actual,id,0,true,false}; SheetTopologyRemoveMeasure usage;
    CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_READY && usage.owned && usage.staging>usage.owned && usage.reservation);
    SheetTopologyRemovePlan *plan=NULL; Workbook before=b; uint64_t order=worksheet_history_high_water();
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_LIMIT && !plan && !memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
    c.reserved=usage.staging; CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY && plan && !memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
    CHECK(!b.sheets[1].analysis.schema && !b.sheets[1].analysis.table && !b.lifecycle && !b.dirty);
    CHECK(sheet_topology_remove_usage(plan).owned==usage.owned && sheet_topology_remove_usage(plan).staging==usage.staging);
    SheetPresenceView owned=sheet_topology_remove_presence(plan); CHECK(owned.sheet->id==id && !strcmp(owned.primary->schema,"main") && !strcmp(owned.primary->table,"alpha") && !strcmp(cells_get(&owned.sheet->cells,100,8),"NULL") && owned.sheet->view.widths[0]==120);
    const SheetTopologyPacket *packet=sheet_topology_remove_packet(plan); SheetTopologyEvent event={0};
    CHECK(sheet_topology_decode(packet->data,packet->bytes,SIZE_MAX,&event)==SHEET_TOPOLOGY_OK && event.operation==SHEET_TOPOLOGY_REMOVE && event.sheet==id && event.before.count==3 && event.after.count==2 && event.before.active==id && event.after.active==b.sheets[0].id); sheet_topology_free(&event);
    b.active=2; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_READY); b.active=1;
    Worksheet *moved=dx_alloc(b.count*sizeof(Worksheet)); memcpy(moved,b.sheets,b.count*sizeof(Worksheet)); free(b.sheets); b.sheets=moved;
    CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_READY);
    b.sheets[1].view.widths[0]=121; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE && owned.sheet->view.widths[0]==120); b.sheets[1].view.widths[0]=120;
    b.sheets[1].cells.cells[0].text[0]='Z'; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE && !strcmp(cells_get(&owned.sheet->cells,100,8),"NULL")); b.sheets[1].cells.cells[0].text[0]='N';
    free(actual.path); actual.path=dx_strdup("/changed.duckdb"); CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); free(actual.path); actual.path=dx_strdup("/offline.duckdb");
    CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_READY);
    epoch++; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); epoch--;
    b.command_revision++; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); b.command_revision--;
    b.edit_session_id++; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); b.edit_session_id--;
    b.sheets[1].edit_session_id++; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); b.sheets[1].edit_session_id--;
    b.sheets[1].width_revision++; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); b.sheets[1].width_revision--;
    c.primary_sheet=b.sheets[0].id; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); c.primary_sheet=id;
    c.read_only=true; CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_PROTECTED); c.read_only=false;
    char *error=NULL; CHECK(cells_set(&b.sheets[0].cells,0,0,"Unpaired",&error) && !error);
    CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_INVALID); cells_free(&b.sheets[0].cells); CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_READY);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_remove_reserve(c,plan,stream,usage.reservation-1,NULL)==SHEET_REMOVE_LIMIT && !sheet_topology_remove_reserved(plan) && !workbook_history_stream_state(stream).owned_bytes);
    CHECK(sheet_topology_remove_reserve(c,plan,stream,usage.reservation,NULL)==SHEET_REMOVE_READY && sheet_topology_remove_reserved(plan));
    CHECK(!workbook_history_stream_state(stream).current.command_revision && !workbook_history_stream_state(stream).current.action_high_water && !b.dirty && b.count==3 && b.active==1);
    CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_BUSY);
    CHECK(workbook_history_stream_free(&stream,true)==HISTORY_STREAM_BUSY && stream);
    sheet_topology_remove_cancel(plan); CHECK(!sheet_topology_remove_reserved(plan) && !workbook_history_stream_state(stream).owned_bytes);
    CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_READY);
    epoch++; CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_STALE && sheet_topology_remove_reserved(plan)); epoch--;
    sheet_topology_remove_free(&plan); CHECK(!plan && !workbook_history_stream_state(stream).owned_bytes); CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    unsigned mask=b.sheets[1].formats.items[0].mask; b.sheets[1].formats.items[0].mask=0;
    CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_READY);
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_INVALID && !plan && b.count==3 && !b.dirty); b.sheets[1].formats.items[0].mask=mask;
    b.command_revision=UINT64_MAX; CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_LIMIT); b.command_revision=0;
    uint64_t session=b.sheets[1].edit_session_id; b.sheets[1].edit_session_id=0; CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_INVALID); b.sheets[1].edit_session_id=session;
    char *table=actual.table; actual.table=NULL; CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_PROTECTED); actual.table=table;
    b.sheets[0].hidden=b.sheets[2].hidden=true; CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_PROTECTED); b.sheets[0].hidden=b.sheets[2].hidden=false;
    b.active=0; b.sheets[1].hidden=true; c.reserved=SIZE_MAX;
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY); CHECK(sheet_topology_remove_presence(plan).sheet->hidden); sheet_topology_remove_free(&plan); b.sheets[1].hidden=false;
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY); CHECK(cells_set(&b.sheets[0].cells,0,0,"Real later action",&error)); worksheet_record(b.sheets,EDIT_CELLS);
    CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_STALE); sheet_topology_remove_free(&plan);
    analysis_free(&actual); workbook_free(&b); sheet_topology_remove_free(NULL); sheet_topology_remove_cancel(NULL);
    CHECK(!sheet_topology_remove_packet(NULL) && !sheet_topology_remove_presence(NULL).sheet && !sheet_topology_remove_reserved(NULL));
    printf("PASS: %u owned sheet removal preparation checks\n",checks); return 0;
}
