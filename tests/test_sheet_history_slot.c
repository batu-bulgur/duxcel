#include "workbook/sheet_history_slot.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/sheet_topology_duplicate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet slot:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void reject(WorkbookSheetHistorySlot slot,WorkbookSheetPayload *owner,WorkbookSheetSlotStatus status) {
    WorkbookSheetPayloadInfo out={.bytes=17},before=out; WorkbookSheetPayload *old=owner;
    CHECK(workbook_sheet_slot_preflight(slot,&out)==status && !memcmp(&out,&before,sizeof(out)));
    CHECK(workbook_sheet_slot_validate(slot,SIZE_MAX,&out)==status && !memcmp(&out,&before,sizeof(out)));
    CHECK(workbook_sheet_slot_decode(slot,SIZE_MAX,&old)==status && old==owner);
}
static void binding(bool forked) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL));
    b.edit_session_id=100; b.sheets[0].edit_session_id=101; b.sheets[1].edit_session_id=102;
    uint64_t epoch=10,id=b.sheets[0].id; WorkbookVersionOwner owner={b.id.high,b.id.low,77,VERSION_WORKING_HEAD};
    WorkbookHistoryStreamOrigin origin={.owner=owner,.edit_session=100,.cursor=4,.tail=forked ? 8 : 4,.lineage=4,.lineage_high_water=forked ? 7 : 4,.action_high_water=12,.transition_high_water=20}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    SheetLifecycleCommandContext c={&b,&epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetLifecycleCommandPlan *plan=NULL;
    CHECK(sheet_lifecycle_command_prepare(c,(SheetMetadataCommand){SHEET_METADATA_RENAME,id,0,"Renamed"},&plan,NULL)==SHEET_METADATA_READY);
    WorkbookHistorySheetLifecycleRecorder recorder=workbook_history_stream_sheet_lifecycle_recorder(stream);
    CHECK(sheet_lifecycle_command_apply(c,plan,&recorder,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&plan);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(held);
    CHECK(frame->action==13 && frame->transition==21 && frame->position==5 && frame->forked==forked);
    WorkbookHistorySegment segments[2]={{4,0,5},{8,4,5}}; if (forked) segments[0].through=4;
    WorkbookHistoryEventPosition event={21,frame->lineage,5}; WorkbookHistoryPath path={.document_high=b.id.high,.document_low=b.id.low,.segments=segments,.segment_count=forked ? 2 : 1,
        .events=&event,.event_count=1,.lineage_high_water=frame->lineage,.operation_high_water=21,.available=4,.cursor=4,.tail=5};
    WorkbookSheetHistorySlot slot={&path,0,owner,100,{frame,workbook_history_event_payload(held),frame->bytes,id,SHEET_RENAME}};
    WorkbookSheetPayloadInfo info; CHECK(workbook_sheet_slot_preflight(slot,&info)==SHEET_SLOT_OK && info.kind==SHEET_PAYLOAD_METADATA);
    WorkbookSheetPayload *decoded=NULL; CHECK(workbook_sheet_slot_decode(slot,info.staging-1,&decoded)==SHEET_SLOT_LIMIT && !decoded);
    CHECK(workbook_sheet_slot_validate(slot,0,&info)==SHEET_SLOT_OK && workbook_sheet_slot_decode(slot,info.staging,&decoded)==SHEET_SLOT_OK);
    WorkbookSheetHistorySlot changed=slot; changed.index=1; reject(changed,decoded,SHEET_SLOT_ASSOCIATION);
    changed=slot; changed.owner.session++; reject(changed,decoded,SHEET_SLOT_ASSOCIATION);
    changed=slot; changed.edit_session++; reject(changed,decoded,SHEET_SLOT_ASSOCIATION);
    changed=slot; changed.payload.sheet++; reject(changed,decoded,SHEET_SLOT_INVALID);
    changed=slot; changed.payload.operation=SHEET_HIDE; reject(changed,decoded,SHEET_SLOT_INVALID);
    event.operation=frame->action; reject(slot,decoded,SHEET_SLOT_ASSOCIATION); event.operation=21;
    event.position=UINT64_MAX; reject(slot,decoded,SHEET_SLOT_PATH); event.position=5;
    path.document_low++; reject(slot,decoded,SHEET_SLOT_ASSOCIATION); path.document_low--;
    WorkbookHistoryEventInfo copy=*frame; changed=slot; changed.payload.event=&copy;
    copy.owner.document_high++; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
    copy.transition=13; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
    copy.previous_position=UINT64_MAX; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
    copy.dependency_encoding=1; copy.dependency_bytes=1; reject(changed,decoded,SHEET_SLOT_INVALID); copy=*frame;
    copy.encoding=99; reject(changed,decoded,SHEET_SLOT_UNSUPPORTED); copy=*frame;
    if (forked) {
        copy.parent_lineage=3; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
        copy.forked=false; copy.parent_lineage=copy.lineage; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
        WorkbookHistoryPath opaque=path; opaque.origin=4; opaque.segments=segments+1; opaque.segment_count=1; changed.path=&opaque;
        reject(changed,decoded,SHEET_SLOT_ASSOCIATION); changed=slot; changed.payload.event=&copy;
    } else {
        copy.forked=true; copy.parent_lineage=3; reject(changed,decoded,SHEET_SLOT_ASSOCIATION); copy=*frame;
    }
    /* Counter maxima remain readable without wrapping position arithmetic. */
    WorkbookHistorySegment last={frame->lineage,UINT64_MAX-1,UINT64_MAX}; WorkbookHistoryEventPosition end={UINT64_MAX,frame->lineage,UINT64_MAX};
    WorkbookHistoryPath max=path; max.origin=max.available=max.cursor=UINT64_MAX-1; max.tail=UINT64_MAX; max.segments=&last; max.segment_count=1; max.events=&end; max.operation_high_water=UINT64_MAX;
    copy=*frame; copy.forked=false; copy.parent_lineage=copy.lineage; copy.previous_position=UINT64_MAX-1; copy.position=copy.transition=UINT64_MAX; changed=slot; changed.path=&max; changed.payload.event=&copy;
    CHECK(workbook_sheet_slot_validate(changed,SIZE_MAX,&info)==SHEET_SLOT_OK);
    copy.previous_position=UINT64_MAX; reject(changed,decoded,SHEET_SLOT_ASSOCIATION);
    CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); workbook_free(&b);
    CHECK(workbook_sheet_payload_event(decoded)->transition==21 && workbook_sheet_payload_event(decoded)->action==13 && !strcmp(workbook_sheet_payload_metadata(decoded)->after.sheets[0].name,"Renamed"));
    workbook_sheet_payload_free(&decoded);
}
static void topology_slot(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL));
    b.edit_session_id=400; b.sheets[0].edit_session_id=401; b.sheets[1].edit_session_id=402;
    uint64_t epoch=50,id=b.sheets[0].id; WorkbookVersionOwner owner={b.id.high,b.id.low,88,VERSION_WORKING_HEAD}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=owner,.edit_session=400},(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    SheetTopologyDuplicateContext c={&b,&epoch,&b.sheets[0].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *plan=NULL;
    CHECK(sheet_topology_duplicate_prepare(c,id,&plan,NULL)==SHEET_DUPLICATE_READY); uint64_t added=sheet_topology_duplicate_presence(plan).sheet->id;
    CHECK(sheet_topology_duplicate_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(held); WorkbookHistorySegment segment={0,0,1}; WorkbookHistoryEventPosition event={frame->transition,0,1};
    WorkbookHistoryPath path={.document_high=b.id.high,.document_low=b.id.low,.segments=&segment,.segment_count=1,.events=&event,.event_count=1,.operation_high_water=frame->transition,.tail=1};
    WorkbookSheetHistorySlot slot={&path,0,owner,400,{frame,workbook_history_event_payload(held),frame->bytes,added,SHEET_DUPLICATE}};
    WorkbookSheetPayloadInfo info; CHECK(workbook_sheet_slot_preflight(slot,&info)==SHEET_SLOT_OK && info.kind==SHEET_PAYLOAD_TOPOLOGY && frame->lineage==0);
    WorkbookSheetPayloadInfo saved=info; CHECK(workbook_sheet_slot_validate(slot,info.validation_scratch-1,&info)==SHEET_SLOT_LIMIT && !memcmp(&info,&saved,sizeof(info)));
    WorkbookSheetPayload *out=NULL; CHECK(workbook_sheet_slot_decode(slot,info.staging,&out)==SHEET_SLOT_OK);
    WorkbookSheetPayload *old=out; CHECK(workbook_sheet_slot_decode(slot,info.staging-1,&out)==SHEET_SLOT_LIMIT && out==old);
    WorkbookSheetHistorySlot mismatch=slot; mismatch.payload.sheet=added+1; reject(mismatch,out,SHEET_SLOT_INVALID);
    CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); workbook_free(&b);
    CHECK(workbook_sheet_payload_event(out)->transition==event.operation && workbook_sheet_payload_topology(out)->sheet==added); workbook_sheet_payload_free(&out);
}
int main(void) { binding(false); binding(true); topology_slot(); printf("PASS: %u assigned family5 history slot checks\n",checks); return 0; }
