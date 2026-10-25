#include "workbook/sheet_topology_duplicate.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include "workbook/lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet duplicate:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(void) {
    Workbook b; workbook_init(&b);
    CHECK(workbook_add(&b,"Copy",&(Analysis){.kind=SOURCE_DATABASE,.path="/offline.duckdb"},NULL));
    CHECK(workbook_add(&b,"Copy (2)",&(Analysis){0},NULL));
    Worksheet *s=b.sheets; char *error=NULL;
    CHECK(cells_set(&s->cells,100,8,"NULL",&error)); CHECK(cells_set(&s->cells,101,8,"9007199254740993.12345678901234567890",&error)); CHECK(cells_set(&s->cells,102,8,"",&error)); worksheet_record(s,EDIT_CELLS);
    CHECK(objects_add_table(&s->objects,20,4,"Input",&(Analysis){.kind=SOURCE_CSV,.path="/missing.csv"},&error)==0); worksheet_record(s,EDIT_OBJECTS);
    SourceVersion row={.version=1,.owner=1,.kind=SOURCE_CSV,.path="/missing.csv"}; SourceVersions ledger={0};
    CHECK(source_versions_plan(&s->source_versions,&row,true,&ledger,&error)); s->source_versions=ledger; s->objects.items[0].source.source_version=1; s->objects.next_id=99; s->source_versions.high_water=77;
    CellFormat value=format_default(); value.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){100,100,8,8},FORMAT_BOLD,value,&error)); worksheet_record(s,EDIT_FORMAT);
    s->view.column_count=16384; s->view.columns=dx_alloc(16384*sizeof(char *)); s->view.widths=dx_alloc(16384*sizeof(float));
    for (size_t i=0;i<16384;i++) { s->view.columns[i]=dx_strdup(i ? "" : "Old source column"); s->view.widths[i]=i==16383 ? 1199.75f : 96; }
    s->view.top=3; s->view.row=100; s->view.col=8; s->view.fraction=.125; s->view.canvas_zoom=1.25f;
    CHECK(workbook_prepare_command_session(&b,0,&error) && !error); b.dirty=false; return b;
}
static void adopted(void) {
    Workbook b=fixture(); uint64_t epoch=30,id=b.sheets[0].id; Analysis actual=analysis_copy(&b.sheets[0].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    SheetTopologyDuplicateContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *p=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,11,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *stream=NULL,*other=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_duplicate_apply(c,&p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_PROTECTED && p && b.count==2 && !b.lifecycle && !workbook_history_stream_state(stream).owned_bytes);
    /* Fixture-only cache materialization; product handoff remains typed. */
    analysis_free(&b.sheets[0].analysis); b.sheets[0].analysis=analysis_copy(&actual);
    size_t allocation=workbook_lifecycle_duplicate_staging(&b,0);
    CHECK(allocation>workbook_duplicate_staging(&b,0) && allocation-workbook_duplicate_staging(&b,0)==workbook_lifecycle_metadata_staging(&b,SHEET_HIDE));
    c.adoption_reserved=allocation-1;
    CHECK(sheet_topology_duplicate_apply(c,&p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_LIMIT && p && b.count==2 && !b.dirty && !workbook_history_stream_state(stream).owned_bytes);
    c.adoption_reserved=allocation;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&other)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_duplicate_reserve(c,p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_READY);
    CHECK(sheet_topology_duplicate_apply(c,&p,other,SIZE_MAX,NULL)==SHEET_DUPLICATE_INVALID && p && !sheet_topology_duplicate_reserved(p) && b.count==2 && !workbook_history_stream_state(stream).owned_bytes);
    CHECK(workbook_history_stream_free(&other,false)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_duplicate_reserve(c,p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_READY); b.active=0; uint64_t active=b.sheets[0].id;
    c.reserved=0; CHECK(sheet_topology_duplicate_apply(c,&p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_LIMIT && p && b.count==2 && !b.dirty && !workbook_history_stream_state(stream).owned_bytes);
    c.reserved=SIZE_MAX; uint64_t revision=b.command_revision,order=worksheet_history_high_water(),high=b.sheet_high_water;
    CHECK(sheet_topology_duplicate_apply(c,&p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED && !p);
    CHECK(b.count==3 && b.active==1 && b.sheets[1].id==high+1 && b.sheet_high_water==high+1 && !strcmp(b.sheets[1].name,"Copy (3)") && b.command_revision==revision+1 && worksheet_history_high_water()==order+1);
    CHECK(b.sheets[1].cells.cells!=b.sheets[0].cells.cells && b.sheets[1].view.widths!=b.sheets[0].view.widths && b.sheets[1].source_versions.items!=b.sheets[0].source_versions.items && b.sheets[1].source_versions.high_water==77 && b.sheets[1].objects.next_id==99 && !b.sheets[1].edit_session_id && !b.sheets[1].undo_actions_count);
    CHECK(workbook_lifecycle_duplicate_staging(&b,0)==workbook_duplicate_staging(&b,0));
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); SheetTopologyEvent event={0};
    CHECK(info->family==5 && info->encoding==2 && sheet_topology_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==SHEET_TOPOLOGY_OK && event.operation==SHEET_TOPOLOGY_ADD && event.sheet==high+1 && event.before.active==active && event.after.active==high+1);
    SheetPresenceAsset *asset=NULL; CHECK(sheet_presence_decode(event.presence.data,event.presence.bytes,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
    bool matches=false; CHECK(sheet_presence_matches(asset,(SheetPresenceView){b.id,b.sheet_high_water,b.sheets+1,&b.sheets[1].analysis},&matches)==SHEET_PRESENCE_OK && matches);
    CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==2 && b.sheet_high_water==high+1 && b.sheets[b.active].id==active);
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==3 && b.sheets[1].id==high+1 && b.sheets[1].view.widths[16383]==1199.75f && !strcmp(b.sheets[1].analysis.table,"alpha"));
    CHECK(workbook_history_stream_state(stream).pending==0 && workbook_history_stream_state(stream).current.action_high_water==1);
    sheet_presence_free(&asset); sheet_topology_free(&event); CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); analysis_free(&actual); workbook_free(&b);
}
static void ring_and_limits(void) {
    Workbook b=fixture(); uint64_t epoch=60,id=b.sheets[0].id; Analysis actual=analysis_copy(&b.sheets[0].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    analysis_free(&b.sheets[0].analysis); b.sheets[0].analysis=analysis_copy(&actual);
    for (unsigned i=0;i<DX_HISTORY;i++) { char name[32]; snprintf(name,sizeof(name),"Other%u",i); bool changed=false; CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=1,.name=name},&changed,NULL) && changed); }
    b.dirty=false; uint64_t revision=b.command_revision,order=worksheet_history_high_water(),floor=workbook_lifecycle_floor(&b);
    SheetTopologyDuplicateContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *p=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,12,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id,.command_revision=b.command_revision}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&stream)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_duplicate_apply(c,&p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_LIMIT && p && b.count==2 && b.command_revision==revision && !b.dirty && worksheet_history_high_water()==order && workbook_lifecycle_floor(&b)==floor);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED && !p && b.count==3 && b.command_revision==revision+1 && worksheet_history_high_water()==order+1 && workbook_lifecycle_floor(&b)>floor);
    analysis_free(&actual); workbook_free(&b);
    b=fixture(); b.command_revision=UINT64_MAX-1; b.sheet_high_water=UINT64_MAX-1; id=b.sheets[0].id; actual=analysis_copy(&b.sheets[0].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    analysis_free(&b.sheets[0].analysis); b.sheets[0].analysis=analysis_copy(&actual); c=(SheetTopologyDuplicateContext){&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX};
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY && sheet_topology_duplicate_presence(p).sheet->id==UINT64_MAX);
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED && !p && b.sheet_high_water==UINT64_MAX && b.command_revision==UINT64_MAX && workbook_history_peek(&b,false,NULL).status==WORKBOOK_HISTORY_LIMIT);
    CHECK(workbook_duplicate_staging(&b,0)==SIZE_MAX && workbook_lifecycle_duplicate_staging(&b,0)==SIZE_MAX);
    analysis_free(&actual); workbook_free(&b);
}
static void hidden_and_redo(void) {
    Workbook b=fixture(); uint64_t epoch=65,id=b.sheets[0].id; Analysis actual=analysis_copy(&b.sheets[0].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    analysis_free(&b.sheets[0].analysis); b.sheets[0].analysis=analysis_copy(&actual); b.sheets[0].hidden=true;
    bool changed=false; CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=1,.name="Renamed"},&changed,NULL) && changed);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED);
    WorkbookHistoryResult redo=workbook_history_peek(&b,true,NULL); CHECK(redo.status==WORKBOOK_HISTORY_READY && redo.lifecycle);
    b.dirty=false; uint64_t revision=b.command_revision,order=worksheet_history_high_water(),high=b.sheet_high_water;
    SheetTopologyDuplicateContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,0}; SheetTopologyDuplicatePlan *p=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY && !sheet_topology_duplicate_presence(p).sheet->hidden);
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_LIMIT && p && b.count==2 && !b.dirty && b.command_revision==revision && worksheet_history_high_water()==order && workbook_history_peek(&b,true,NULL).order==redo.order);
    c.adoption_reserved=workbook_lifecycle_duplicate_staging(&b,0);
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED && !p && b.count==3 && b.sheets[0].hidden && !b.sheets[1].hidden && b.sheets[1].id==high+1 && workbook_history_peek(&b,true,NULL).status==WORKBOOK_HISTORY_EMPTY);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==2 && b.sheets[0].hidden && !b.sheets[b.active].hidden);
    analysis_free(&actual); workbook_free(&b);
}
static void names_and_capacity(void) {
    Workbook b; workbook_init(&b); char name[4097]; memset(name,'a',4096); name[4096]=0;
    CHECK(workbook_add(&b,name,&(Analysis){0},NULL)); CHECK(workbook_prepare_command_session(&b,0,NULL));
    b.dirty=false; uint64_t epoch=70,id=b.sheets[0].id; Analysis actual={0};
    SheetTopologyDuplicateContext c={&b,&epoch,&actual,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *p=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY);
    const char *prepared=sheet_topology_duplicate_presence(p).sheet->name; CHECK(strlen(prepared)==4084 && !strcmp(prepared+4080," (2)"));
    CHECK(sheet_topology_duplicate_apply(c,&p,NULL,0,NULL)==SHEET_DUPLICATE_APPLIED && !p && b.count==2 && !strcmp(b.sheets[1].name+4080," (2)"));
    for (size_t i=b.count;i<DX_MAX_SHEETS;i++) { char label[32]; snprintf(label,sizeof(label),"Extra%zu",i); CHECK(workbook_add(&b,label,&actual,NULL)); }
    size_t count=b.count; uint64_t high=b.sheet_high_water,revision=b.command_revision; SheetTopologyDuplicateMeasure usage;
    CHECK(sheet_topology_duplicate_measure(c,id,&usage)==SHEET_DUPLICATE_LIMIT && workbook_duplicate_staging(&b,0)==SIZE_MAX && !workbook_duplicate(&b,0,NULL) && b.count==count && b.sheet_high_water==high && b.command_revision==revision);
    workbook_free(&b);
}
int main(void) {
    adopted(); ring_and_limits(); hidden_and_redo(); names_and_capacity();
    Workbook b=fixture(); uint64_t epoch=10,id=b.sheets[0].id; Analysis actual=analysis_copy(&b.sheets[0].analysis); actual.schema=dx_strdup("main"); actual.table=dx_strdup("alpha");
    SheetTopologyDuplicateContext c={&b,&epoch,&actual,id,0,true,false,0}; SheetTopologyDuplicateMeasure usage;
    CHECK(sheet_topology_duplicate_measure(c,id,&usage)==SHEET_DUPLICATE_READY && usage.owned && usage.staging>usage.owned && usage.reservation);
    SheetTopologyDuplicatePlan *p=NULL; Workbook before=b; uint64_t order=worksheet_history_high_water();
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_LIMIT && !p && !memcmp(&before,&b,sizeof(b)) && worksheet_history_high_water()==order);
    c.reserved=usage.staging; CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY && p && !memcmp(&before,&b,sizeof(b)));
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_INVALID);
    SheetPresenceView v=sheet_topology_duplicate_presence(p); const Worksheet *s=v.sheet;
    CHECK(s->id==b.sheet_high_water+1 && v.sheet_high_water==s->id && !strcmp(s->name,"Copy (3)") && !s->hidden && !s->edit_session_id);
    CHECK(s->cells.cells!=b.sheets[0].cells.cells && s->cells.undo_count==0 && s->formats.undo_count==0 && s->objects.undo_count==0 && !s->width_history && !s->undo_count && !s->undo_actions_count && !s->redo_actions_count);
    CHECK(!strcmp(v.primary->table,"alpha") && !b.sheets[0].analysis.table && !strcmp(cells_get(&s->cells,100,8),"NULL") && !strcmp(cells_get(&s->cells,101,8),"9007199254740993.12345678901234567890") && !*cells_get(&s->cells,102,8));
    CHECK(s->view.widths!=b.sheets[0].view.widths && s->view.column_count==16384 && s->view.widths[16383]==1199.75f && s->view.top==3 && s->view.fraction==.125 && s->view.canvas_zoom==1.25f);
    CHECK(s->source_versions.high_water==77 && s->source_versions.items!=b.sheets[0].source_versions.items && s->objects.next_id==99 && s->objects.items[0].source.source_version==1);
    const SheetTopologyPacket *packet=sheet_topology_duplicate_packet(p); SheetTopologyEvent event={0};
    CHECK(sheet_topology_decode(packet->data,packet->bytes,SIZE_MAX,&event)==SHEET_TOPOLOGY_OK && event.operation==SHEET_TOPOLOGY_ADD && event.before.count==2 && event.after.count==3 && event.after.sheets[1].id==s->id && event.after.active==s->id && !strcmp(event.after.sheets[2].name,"Copy (2)")); sheet_topology_free(&event);
    b.active=0; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_READY); b.active=1;
    Worksheet *moved=dx_alloc(b.count*sizeof(Worksheet)); memcpy(moved,b.sheets,b.count*sizeof(Worksheet)); free(b.sheets); b.sheets=moved;
    CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_READY);
    b.sheets[0].view.widths[16383]=1200; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE && s->view.widths[16383]==1199.75f); b.sheets[0].view.widths[16383]=1199.75f;
    b.sheets[0].source_versions.high_water++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); b.sheets[0].source_versions.high_water--;
    epoch++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); epoch--;
    b.command_revision++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); b.command_revision--;
    b.edit_session_id++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); b.edit_session_id--;
    b.sheets[0].edit_session_id++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); b.sheets[0].edit_session_id--;
    b.sheet_high_water++; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_STALE); b.sheet_high_water--;
    c.read_only=true; CHECK(sheet_topology_duplicate_check(c,p)==SHEET_DUPLICATE_PROTECTED); c.read_only=false;
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryTopologyUsage exact; CHECK(workbook_history_topology_measure(packet,&exact) && exact.staging<=usage.reservation);
    CHECK(sheet_topology_duplicate_reserve(c,p,stream,exact.staging-1,NULL)==SHEET_DUPLICATE_LIMIT && !sheet_topology_duplicate_reserved(p) && !workbook_history_stream_state(stream).owned_bytes);
    CHECK(sheet_topology_duplicate_reserve(c,p,stream,exact.staging,NULL)==SHEET_DUPLICATE_READY && sheet_topology_duplicate_reserved(p) && !workbook_history_stream_state(stream).pending);
    CHECK(sheet_topology_duplicate_reserve(c,p,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_BUSY);
    sheet_topology_duplicate_cancel(p); CHECK(!workbook_history_stream_state(stream).owned_bytes && !b.dirty && b.count==2 && worksheet_history_high_water()==order);
    sheet_topology_duplicate_free(&p); CHECK(!p && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    b.sheets[0].hidden=true; CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY && !sheet_topology_duplicate_presence(p).sheet->hidden); sheet_topology_duplicate_free(&p); b.sheets[0].hidden=false;
    b.sheet_high_water=UINT64_MAX; CHECK(sheet_topology_duplicate_measure(c,id,&usage)==SHEET_DUPLICATE_LIMIT); b.sheet_high_water=2;
    b.command_revision=UINT64_MAX; CHECK(sheet_topology_duplicate_measure(c,id,&usage)==SHEET_DUPLICATE_LIMIT); b.command_revision=0;
    char *table=actual.table; actual.table=NULL; CHECK(sheet_topology_duplicate_measure(c,id,&usage)==SHEET_DUPLICATE_PROTECTED); actual.table=table;
    CHECK(sheet_topology_duplicate_prepare(c,id,&p,NULL)==SHEET_DUPLICATE_READY); analysis_free(&actual); workbook_free(&b);
    CHECK(!strcmp(sheet_topology_duplicate_presence(p).primary->table,"alpha") && sheet_topology_duplicate_presence(p).sheet->view.widths[16383]==1199.75f);
    sheet_topology_duplicate_free(&p); sheet_topology_duplicate_free(NULL); sheet_topology_duplicate_cancel(NULL);
    CHECK(!sheet_topology_duplicate_packet(NULL) && !sheet_topology_duplicate_presence(NULL).sheet && !sheet_topology_duplicate_reserved(NULL));
    printf("PASS: %u prospective sheet duplicate checks\n",checks); return 0;
}
