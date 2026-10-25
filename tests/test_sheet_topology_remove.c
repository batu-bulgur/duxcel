#include "workbook/sheet_topology_remove.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include "workbook/lifecycle.h"
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
static void adopted(void) {
    Workbook b=fixture(); uint64_t epoch=30,id=b.sheets[1].id; Analysis actual=analysis_copy(&b.sheets[1].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    SheetTopologyRemoveContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyRemovePlan *plan=NULL;
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY);
    size_t allocation=workbook_lifecycle_remove_staging(&b,1);
    CHECK(allocation==sizeof(Worksheet)+workbook_lifecycle_metadata_staging(&b,SHEET_HIDE));
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,11,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *stream=NULL,*other=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_PROTECTED && plan && b.count==3 && !b.lifecycle && !workbook_history_stream_state(stream).owned_bytes);
    /* Fixture-only cache materialization; product origins require typed handoff. */
    analysis_free(&b.sheets[1].analysis); b.sheets[1].analysis=analysis_copy(&actual);
    CHECK(sheet_topology_remove_check(c,plan)==SHEET_REMOVE_READY);
    c.adoption_reserved=allocation-1;
    CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_LIMIT && plan && b.count==3 && !b.dirty && !workbook_history_stream_state(stream).owned_bytes);
    c.adoption_reserved=allocation;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&other)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_READY);
    CHECK(sheet_topology_remove_apply(c,&plan,other,SIZE_MAX,NULL)==SHEET_REMOVE_INVALID && plan && b.count==3 && !sheet_topology_remove_reserved(plan) && !workbook_history_stream_state(stream).owned_bytes && !workbook_history_stream_state(other).owned_bytes);
    CHECK(workbook_history_stream_free(&other,false)==HISTORY_STREAM_OK);
    uint64_t revision=b.command_revision,order=worksheet_history_high_water(),high=b.sheet_high_water;
    CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_READY); b.active=2; uint64_t active=b.sheets[2].id;
    c.reserved=0; CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_LIMIT && plan && b.count==3 && !b.dirty && !workbook_history_stream_state(stream).owned_bytes);
    c.reserved=SIZE_MAX; CHECK(sheet_topology_remove_reserve(c,plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_READY);
    CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_APPLIED && !plan);
    CHECK(b.count==2 && b.command_revision==revision+1 && b.sheet_high_water==high && b.dirty && !strcmp(b.sheets[b.active].name,"Third") && worksheet_history_high_water()==order+1);
    WorkbookHistoryResult selected=workbook_history_peek(&b,false,NULL); CHECK(selected.status==WORKBOOK_HISTORY_READY && selected.lifecycle && selected.sheet_id==id && selected.order==order+1);
    CHECK(workbook_history_stream_state(stream).pending==1 && !workbook_history_stream_state(stream).reserved && workbook_history_stream_state(stream).current.command_revision==b.command_revision && workbook_history_stream_state(stream).current.action_high_water==1);
    CHECK(workbook_lifecycle_remove_staging(&b,0)==sizeof(Worksheet));
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); SheetTopologyEvent event={0};
    CHECK(info->family==5 && info->encoding==2 && sheet_topology_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==SHEET_TOPOLOGY_OK && event.operation==SHEET_TOPOLOGY_REMOVE && event.sheet==id);
    CHECK(event.before.active==active && event.after.active==active && b.sheets[b.active].id==active);
    CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK);
    SheetPresenceAsset *before=NULL; CHECK(sheet_presence_decode(event.presence.data,event.presence.bytes,SIZE_MAX,&before,NULL)==SHEET_PRESENCE_OK);
    CHECK(!strcmp(sheet_presence_view(before).primary->table,"alpha") && !strcmp(cells_get(&sheet_presence_view(before).sheet->cells,100,8),"NULL"));
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==3 && b.sheets[1].id==id && b.sheets[1].view.widths[0]==120 && !strcmp(b.sheets[1].analysis.table,"alpha") && !strcmp(cells_get(&b.sheets[1].cells,100,8),"NULL"));
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==2 && b.sheet_high_water==high);
    sheet_presence_free(&before); sheet_topology_free(&event); CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); analysis_free(&actual); workbook_free(&b);
    /* The last valid command revision remains a valid completed adoption even
       though subsequent Undo/Redo is exhausted; release proof must not abort. */
    b=fixture(); b.command_revision=UINT64_MAX-1; id=b.sheets[1].id; actual=analysis_copy(&b.sheets[1].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    analysis_free(&b.sheets[1].analysis); b.sheets[1].analysis=analysis_copy(&actual);
    c=(SheetTopologyRemoveContext){&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX};
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY);
    CHECK(sheet_topology_remove_apply(c,&plan,NULL,0,NULL)==SHEET_REMOVE_APPLIED && !plan && b.command_revision==UINT64_MAX && workbook_history_peek(&b,false,NULL).status==WORKBOOK_HISTORY_LIMIT);
    analysis_free(&actual); workbook_free(&b);
}
static void ring_rejection(void) {
    Workbook b=fixture(); Analysis actual=analysis_copy(&b.sheets[1].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    analysis_free(&b.sheets[1].analysis); b.sheets[1].analysis=analysis_copy(&actual);
    for (unsigned i=0;i<DX_HISTORY;i++) { char name[32]; snprintf(name,sizeof(name),"Base%u",i); bool changed=false; CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name=name},&changed,NULL) && changed); }
    b.dirty=false; uint64_t epoch=60,id=b.sheets[1].id,revision=b.command_revision,order=worksheet_history_high_water(),floor=workbook_lifecycle_floor(&b),latest=workbook_lifecycle_latest(&b);
    SheetTopologyRemoveContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,sizeof(Worksheet)}; SheetTopologyRemovePlan *plan=NULL;
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,12,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id,.command_revision=b.command_revision}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_LIMIT && plan && b.count==3 && b.command_revision==revision && !b.dirty && worksheet_history_high_water()==order && workbook_lifecycle_floor(&b)==floor && workbook_lifecycle_latest(&b)==latest);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_remove_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_REMOVE_APPLIED && !plan && b.count==2 && b.command_revision==revision+1 && worksheet_history_high_water()==order+1 && workbook_lifecycle_floor(&b)>floor);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK && workbook_history_event_info(event)->revision==revision+1);
    SheetTopologyInfo info; CHECK(sheet_topology_validate(workbook_history_event_payload(event),workbook_history_event_info(event)->bytes,SIZE_MAX,&info)==SHEET_TOPOLOGY_OK && info.sheet==id);
    CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    analysis_free(&actual); workbook_free(&b);
}
int main(void) {
    adopted(); ring_rejection();
    Workbook b=fixture(); uint64_t epoch=10,id=b.sheets[1].id; Analysis actual=analysis_copy(&b.sheets[1].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    SheetTopologyRemoveContext c={&b,&epoch,&actual,id,0,true,false,0}; SheetTopologyRemoveMeasure usage;
    CHECK(sheet_topology_remove_measure(c,id,&usage)==SHEET_REMOVE_READY && usage.owned && usage.staging>usage.owned && usage.reservation);
    SheetTopologyRemovePlan *plan=NULL; Workbook before=b; uint64_t order=worksheet_history_high_water();
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_LIMIT && !plan && !memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
    c.reserved=usage.staging; CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY && plan && !memcmp(&b,&before,sizeof(b)) && worksheet_history_high_water()==order);
    CHECK(!b.sheets[1].analysis.schema && !b.sheets[1].analysis.table && !b.lifecycle && !b.dirty);
    CHECK(sheet_topology_remove_usage(plan).owned==usage.owned && sheet_topology_remove_usage(plan).staging==usage.staging);
    SheetPresenceView owned=sheet_topology_remove_presence(plan); CHECK(owned.sheet->id==id && !strcmp(owned.primary->schema,"main") && !strcmp(owned.primary->table,"alpha") && !strcmp(cells_get(&owned.sheet->cells,100,8),"NULL") && owned.sheet->view.widths[0]==120);
    const SheetTopologyPacket *packet=sheet_topology_remove_packet(plan); SheetTopologyEvent event={0}; WorkbookHistoryTopologyUsage reservation;
    CHECK(workbook_history_topology_measure(packet,&reservation) && reservation.staging==usage.reservation && reservation.owned==workbook_history_topology_owned_bytes(packet->bytes));
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
