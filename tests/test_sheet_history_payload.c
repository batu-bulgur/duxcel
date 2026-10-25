#include "workbook/sheet_history_payload.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/sheet_topology_duplicate.h"
#include "workbook/sheet_topology_remove.h"
#include "workbook/cell_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet payload:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL)); CHECK(workbook_prepare_command_session(&b,0,NULL)); b.dirty=false; return b;
}
static WorkbookHistoryStream *stream(Workbook *b) {
    WorkbookHistoryStream *s=NULL; WorkbookHistoryStreamOrigin origin={.owner={b->id.high,b->id.low,11,VERSION_WORKING_HEAD},.edit_session=b->edit_session_id,.command_revision=b->command_revision,.lineage=1,.lineage_high_water=1};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK); return s;
}
static void rejected(WorkbookSheetPayloadView v,WorkbookSheetPayload *old) {
    WorkbookSheetPayloadInfo info={.bytes=7},saved=info; WorkbookSheetPayload *out=old;
    CHECK(workbook_sheet_payload_preflight(v,&info)!=SHEET_PAYLOAD_OK && !memcmp(&info,&saved,sizeof(info)));
    CHECK(workbook_sheet_payload_decode(v,SIZE_MAX,&out)!=SHEET_PAYLOAD_OK && out==old);
}
static void envelope_errors(WorkbookSheetPayloadView original,WorkbookSheetPayload *owner) {
    WorkbookHistoryEventInfo info=*original.event; WorkbookSheetPayloadView v=original; v.event=&info;
    info.family=4; rejected(v,owner); info=*original.event;
    info.encoding=99; rejected(v,owner); info=*original.event;
    info.encoding=info.encoding==1 ? 2 : 1; rejected(v,owner); info=*original.event;
    info.owner.document_high++; rejected(v,owner); info=*original.event;
    info.owner.mode=VERSION_HISTORICAL_VIEW; rejected(v,owner); info=*original.event;
    info.owner.session=0; rejected(v,owner); info=*original.event;
    info.edit_session=0; rejected(v,owner); info=*original.event;
    info.action=0; rejected(v,owner); info=*original.event;
    info.revision=0; rejected(v,owner); info=*original.event;
    info.transition=0; rejected(v,owner); info=*original.event;
    info.position++; rejected(v,owner); info=*original.event;
    info.parent_lineage++; rejected(v,owner); info=*original.event;
    info.dependency_encoding=1; info.dependency_bytes=8; rejected(v,owner); info=*original.event;
    info.inactive_scopes=1; rejected(v,owner); info=*original.event;
    v.sheet=UINT64_MAX; rejected(v,owner); v=original; v.event=&info;
    v.operation=original.operation<=SHEET_DELETE ? SHEET_RENAME : SHEET_DELETE; rejected(v,owner); v=original; v.event=&info;
    unsigned char *copy=dx_alloc(v.bytes+1); memcpy(copy,v.data,v.bytes); v.data=copy;
    copy[0]='!'; rejected(v,owner); copy[0]=((unsigned char *)original.data)[0];
    copy[4]=99; rejected(v,owner); copy[4]=1;
    info.bytes=++v.bytes; rejected(v,owner);
    free(copy);
    info=*original.event; v=original; v.event=&info;
    info.revision=info.action=info.transition=UINT64_MAX; info.previous_position=UINT64_MAX-1; info.position=UINT64_MAX;
    info.lineage=info.parent_lineage=UINT64_MAX; WorkbookSheetPayloadInfo checked;
    CHECK(workbook_sheet_payload_validate(v,SIZE_MAX,&checked)==SHEET_PAYLOAD_OK);
    info.forked=true; info.parent_lineage=0; CHECK(workbook_sheet_payload_validate(v,SIZE_MAX,&checked)==SHEET_PAYLOAD_OK);
}
static void metadata(SheetMetadataOperation operation,WorkbookSheetOperation native) {
    Workbook b=fixture(); if (operation==SHEET_METADATA_SHOW) b.sheets[0].hidden=true;
    uint64_t epoch=20,id=b.sheets[0].id; WorkbookHistoryStream *s=stream(&b); SheetLifecycleCommandPlan *plan=NULL;
    SheetLifecycleCommandContext c={&b,&epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetMetadataCommand command={operation,id,operation==SHEET_METADATA_REORDER ? b.sheets[1].id : 0,operation==SHEET_METADATA_RENAME ? "Renamed" : NULL};
    CHECK(sheet_lifecycle_command_prepare(c,command,&plan,NULL)==SHEET_METADATA_READY);
    WorkbookHistorySheetLifecycleRecorder recorder=workbook_history_stream_sheet_lifecycle_recorder(s);
    CHECK(sheet_lifecycle_command_apply(c,plan,&recorder,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&plan);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event); WorkbookSheetPayloadView view={frame,workbook_history_event_payload(event),frame->bytes,id,native}; WorkbookSheetPayloadInfo info;
    CHECK(workbook_sheet_payload_preflight(view,&info)==SHEET_PAYLOAD_OK && info.kind==SHEET_PAYLOAD_METADATA && info.validation_scratch==0 && info.staging==info.owned);
    WorkbookSheetPayload *out=NULL; CHECK(workbook_sheet_payload_decode(view,info.staging-1,&out)==SHEET_PAYLOAD_LIMIT && !out);
    CHECK(workbook_sheet_payload_validate(view,0,&info)==SHEET_PAYLOAD_OK && workbook_sheet_payload_decode(view,info.staging,&out)==SHEET_PAYLOAD_OK);
    CHECK(workbook_sheet_payload_metadata(out) && !workbook_sheet_payload_topology(out) && workbook_sheet_payload_event(out)->action==frame->action);
    envelope_errors(view,out);
    WorkbookSheetPayload *prior=out; CHECK(workbook_sheet_payload_decode(view,info.staging-1,&out)==SHEET_PAYLOAD_LIMIT && out==prior);
    if (native==SHEET_RENAME) {
        const SheetLifecycle *decoded=workbook_sheet_payload_metadata(out);
        SheetLifecycleEntry after[DX_MAX_SHEETS]; memcpy(after,decoded->after.sheets,decoded->after.count*sizeof(*after)); after[1].hidden=!after[1].hidden;
        SheetLifecycle pair=*decoded; pair.after.sheets=after; pair.after.active=id; SheetLifecyclePacket combined={0};
        CHECK(sheet_lifecycle_capture(pair,SIZE_MAX,&combined)==SHEET_LIFECYCLE_OK);
        WorkbookHistoryEventInfo extra=*frame; extra.bytes=combined.bytes; WorkbookSheetPayloadView mixed={&extra,combined.data,combined.bytes,id,native}; rejected(mixed,out);
        sheet_lifecycle_packet_free(&combined);
    }
    /* Declared metadata effects must be exact, not merely the same ID set. */
    WorkbookSheetPayloadView wrong=view; wrong.operation=native==SHEET_RENAME ? SHEET_HIDE : SHEET_RENAME; rejected(wrong,out);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
    CHECK(workbook_sheet_payload_info(out)->sheet==id && workbook_sheet_payload_metadata(out)->before.count==2); workbook_sheet_payload_free(&out); CHECK(!out);
}
static void topology(void) {
    Workbook b=fixture(); uint64_t epoch=30,id=b.sheets[0].id;
    CHECK(cells_set(&b.sheets[0].cells,10,7,"NULL",NULL)); worksheet_record(b.sheets,EDIT_CELLS);
    SheetTopologyDuplicateContext c={&b,&epoch,&b.sheets[0].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *plan=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&plan,NULL)==SHEET_DUPLICATE_READY); uint64_t added=sheet_topology_duplicate_presence(plan).sheet->id;
    WorkbookHistoryStream *s=stream(&b); CHECK(sheet_topology_duplicate_apply(c,&plan,s,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event); WorkbookSheetPayloadView view={frame,workbook_history_event_payload(event),frame->bytes,added,SHEET_DUPLICATE}; WorkbookSheetPayloadInfo info;
    CHECK(workbook_sheet_payload_preflight(view,&info)==SHEET_PAYLOAD_OK && info.kind==SHEET_PAYLOAD_TOPOLOGY && info.validation_scratch && info.staging>info.owned);
    WorkbookSheetPayload *out=NULL; CHECK(workbook_sheet_payload_validate(view,info.validation_scratch-1,&info)==SHEET_PAYLOAD_LIMIT);
    CHECK(workbook_sheet_payload_decode(view,info.staging-1,&out)==SHEET_PAYLOAD_LIMIT && !out);
    CHECK(workbook_sheet_payload_validate(view,info.validation_scratch,&info)==SHEET_PAYLOAD_OK && workbook_sheet_payload_decode(view,info.staging,&out)==SHEET_PAYLOAD_OK);
    CHECK(workbook_sheet_payload_topology(out) && !workbook_sheet_payload_metadata(out) && workbook_sheet_payload_topology(out)->sheet==added);
    envelope_errors(view,out);
    WorkbookSheetPayloadView wrong=view; wrong.operation=SHEET_DELETE; rejected(wrong,out);
    wrong=view; wrong.operation=SHEET_CREATE; CHECK(workbook_sheet_payload_validate(wrong,SIZE_MAX,&info)==SHEET_PAYLOAD_OK); /* ADD cannot prove copy provenance. */
    unsigned char *bad=dx_alloc(view.bytes); memcpy(bad,view.data,view.bytes); SheetTopologyInfo topology;
    CHECK(sheet_topology_preflight(bad,view.bytes,&topology)==SHEET_TOPOLOGY_OK);
    bad[view.bytes-topology.presence_bytes+DX_SHEET_PRESENCE_HEADER_BYTES]^=1; wrong=view; wrong.data=bad;
    CHECK(workbook_sheet_payload_preflight(wrong,&info)==SHEET_PAYLOAD_OK);
    WorkbookSheetPayloadInfo unchanged=info; CHECK(workbook_sheet_payload_validate(wrong,info.validation_scratch,&info)==SHEET_PAYLOAD_INVALID && !memcmp(&info,&unchanged,sizeof(info)));
    WorkbookSheetPayload *prior=out; CHECK(workbook_sheet_payload_decode(wrong,SIZE_MAX,&out)==SHEET_PAYLOAD_INVALID && out==prior); free(bad);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
    SheetPresenceAsset *asset=NULL; const SheetTopologyEvent *decoded=workbook_sheet_payload_topology(out);
    CHECK(sheet_presence_decode(decoded->presence.data,decoded->presence.bytes,SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK && !strcmp(cells_get(&sheet_presence_view(asset).sheet->cells,10,7),"NULL"));
    sheet_presence_free(&asset);
    WorkbookId doc=workbook_sheet_payload_info(out)->document; uint64_t high=decoded->after.high_water;
    SheetLifecycleEntry before_rows[]={{1,"A",false},{2,"B",false}},after_rows[]={{1,"Renamed",false},{2,"B",false}};
    SheetLifecycle pair={{doc,high,1,before_rows,2},{doc,high,1,after_rows,2}}; SheetLifecyclePacket metadata={0};
    CHECK(sheet_lifecycle_capture(pair,SIZE_MAX,&metadata)==SHEET_LIFECYCLE_OK);
    WorkbookHistoryEventInfo replacement=*workbook_sheet_payload_event(out); replacement.encoding=DX_HISTORY_SHEET_LIFECYCLE_PROFILE; replacement.bytes=metadata.bytes;
    WorkbookSheetPayloadView replacing={&replacement,metadata.data,metadata.bytes,1,SHEET_RENAME};
    CHECK(workbook_sheet_payload_preflight(replacing,&info)==SHEET_PAYLOAD_OK);
    prior=out; CHECK(workbook_sheet_payload_decode(replacing,info.staging-1,&out)==SHEET_PAYLOAD_LIMIT && out==prior && workbook_sheet_payload_topology(out));
    CHECK(workbook_sheet_payload_decode(replacing,info.staging,&out)==SHEET_PAYLOAD_OK && workbook_sheet_payload_metadata(out) && !workbook_sheet_payload_topology(out));
    sheet_lifecycle_packet_free(&metadata); CHECK(!strcmp(workbook_sheet_payload_metadata(out)->after.sheets[0].name,"Renamed")); workbook_sheet_payload_free(&out);
}
static void removal(void) {
    Workbook b=fixture(); uint64_t epoch=50,id=b.sheets[0].id; WorkbookHistoryStream *s=stream(&b);
    SheetTopologyRemoveContext c={&b,&epoch,&b.sheets[0].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyRemovePlan *plan=NULL;
    CHECK(sheet_topology_remove_prepare(c,id,&plan,NULL)==SHEET_REMOVE_READY && sheet_topology_remove_apply(c,&plan,s,SIZE_MAX,NULL)==SHEET_REMOVE_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event); WorkbookSheetPayloadView v={frame,workbook_history_event_payload(event),frame->bytes,id,SHEET_DELETE};
    WorkbookSheetPayloadInfo info; CHECK(workbook_sheet_payload_validate(v,SIZE_MAX,&info)==SHEET_PAYLOAD_OK && info.kind==SHEET_PAYLOAD_TOPOLOGY);
    WorkbookSheetPayload *out=NULL; CHECK(workbook_sheet_payload_decode(v,info.staging,&out)==SHEET_PAYLOAD_OK && workbook_sheet_payload_topology(out)->operation==SHEET_TOPOLOGY_REMOVE);
    v.operation=SHEET_CREATE; rejected(v,out); v.operation=SHEET_DUPLICATE; rejected(v,out);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
    CHECK(workbook_sheet_payload_topology(out)->sheet==id && workbook_sheet_payload_topology(out)->before.count==2); workbook_sheet_payload_free(&out);
}
int main(void) {
    metadata(SHEET_METADATA_RENAME,SHEET_RENAME); metadata(SHEET_METADATA_REORDER,SHEET_REORDER); metadata(SHEET_METADATA_HIDE,SHEET_HIDE); metadata(SHEET_METADATA_SHOW,SHEET_SHOW); topology(); removal();
    CHECK(!workbook_sheet_payload_info(NULL) && !workbook_sheet_payload_event(NULL) && !workbook_sheet_payload_metadata(NULL) && !workbook_sheet_payload_topology(NULL)); workbook_sheet_payload_free(NULL);
    printf("PASS: %u current typed sheet payload checks\n",checks); return 0;
}
