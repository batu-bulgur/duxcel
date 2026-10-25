#include "workbook/source_history_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source slot:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void rejected(WorkbookSourceHistorySlot slot,SourceDependencyLive live,WorkbookSourceSlotStatus expected) {
    WorkbookSourcePayloadInfo info={.owned=17},prior=info;
    WorkbookSourceSlotAdmission out={.compatibility=SOURCE_DEPENDENCY_REFINED,.observations={.unbound_sources=19}},before=out;
    CHECK(workbook_source_slot_preflight(slot,&info)==expected && !memcmp(&info,&prior,sizeof(info)));
    CHECK(workbook_source_slot_admit(slot,live,SIZE_MAX,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
static SourcePacket raw(void) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page page={.columns=&column,.column_count=1}; Analysis a={.kind=SOURCE_CSV,.path="/source.csv"}; SourceDigest digest={.bytes=42,.identity={.size=42}};
    SourcePacket out={0}; CHECK(source_scope_raw_build(0,1,&a,&page,&digest,"engine",&out,NULL)); return out;
}
static void binding(bool definition,bool forked) {
    WorkbookVersionOwner owner={1,2,99,VERSION_WORKING_HEAD};
    WorkbookHistoryStreamOrigin origin={.owner=owner,.edit_session=44,.command_revision=100,.cursor=5,.tail=forked ? 7 : 5,.lineage=2,
        .action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_BYTES},&stream)==HISTORY_STREAM_OK);
    SourceVersions ledger={.high_water=20}; FormatStore before={0},after={0};
    if (definition) {
        ledger.count=ledger.capacity=1; ledger.items=dx_alloc(sizeof(SourceVersion)); ledger.items[0]=(SourceVersion){0,1,SOURCE_CSV,dx_strdup("/source.csv"),{0}};
        ledger.bytes=sizeof(SourceVersion)+strlen(ledger.items[0].path)+1;
        Analysis a={.kind=SOURCE_CSV,.path="/source.csv",.source_version=1},b=a; b.query="SELECT Id FROM input";
        WorkbookHistoryDefinitionRecorder r=workbook_history_stream_definition_recorder(stream);
        WorkbookHistoryDefinitionRecord record={.workbook={1,2},.edit_session=44,.sheet_session=88,.expected_revision=100,.revision=101,.validation_reserved=SIZE_MAX,
            .view={.sheet=7,.effects=1,.before={&a,NULL},.after={&b,NULL},.sources=&ledger}};
        void *ticket=NULL; CHECK(r.reserve(r.context,&record,&ticket,NULL) && ticket); r.publish(r.context,ticket);
    } else {
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&after,(FormatRange){0},FORMAT_BOLD,bold,NULL));
        WorkbookHistoryFormatRecorder r=workbook_history_stream_format_recorder(stream);
        WorkbookHistoryFormatRecord record={.workbook={1,2},.edit_session=44,.sheet_session=88,.sheet=7,.expected_revision=100,.revision=101,.effects=8,
            .before=format_delta_store_view(&before),.after=format_delta_store_view(&after),.sources=&ledger};
        void *ticket=NULL; CHECK(r.reserve(r.context,&record,&ticket,NULL) && ticket); r.publish(r.context,ticket);
    }
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(held); CHECK(frame->action==41 && frame->transition==61 && frame->forked==forked);
    WorkbookHistorySegment segments[2]={{2,0,6},{8,5,6}}; if (forked) segments[0].through=5;
    WorkbookHistoryEventPosition event={frame->transition,frame->lineage,6};
    WorkbookHistoryPath path={.document_high=1,.document_low=2,.segments=segments,.segment_count=forked ? 2 : 1,.events=&event,.event_count=1,
        .lineage_high_water=frame->lineage,.operation_high_water=61,.available=5,.cursor=5,.tail=6};
    WorkbookSourceHistorySlot slot={&path,0,owner,44,{frame,workbook_history_event_payload(held),workbook_history_event_dependencies(held),frame->bytes,frame->dependency_bytes,7,definition ? 1u : 8u}};
    SourceDependencyLive live={7,0,&ledger}; WorkbookSourcePayloadInfo info={0};
    CHECK(workbook_source_slot_preflight(slot,&info)==SOURCE_SLOT_OK && info.sheet==7);
    WorkbookSourceSlotAdmission out={0}; CHECK(workbook_source_slot_admit(slot,live,info.staging,&out)==SOURCE_SLOT_OK && out.compatibility==SOURCE_DEPENDENCY_EXACT && out.observations.sources.captured_pending==(definition ? 1u : 0u));
    WorkbookSourceSlotAdmission prior=out;
    CHECK(workbook_source_slot_admit(slot,live,info.staging-1,&out)==SOURCE_SLOT_LIMIT && !memcmp(&prior,&out,sizeof(out)));
    WorkbookSourceHistorySlot changed=slot; changed.index=1; rejected(changed,live,SOURCE_SLOT_ASSOCIATION);
    changed=slot; changed.owner.session++; rejected(changed,live,SOURCE_SLOT_ASSOCIATION);
    changed=slot; changed.edit_session++; rejected(changed,live,SOURCE_SLOT_ASSOCIATION);
    changed=slot; changed.payload.sheet++; rejected(changed,live,SOURCE_SLOT_INVALID);
    changed=slot; changed.payload.effects^=4; rejected(changed,live,SOURCE_SLOT_INVALID);
    event.operation=frame->action; rejected(slot,live,SOURCE_SLOT_ASSOCIATION); event.operation=frame->transition;
    event.position=UINT64_MAX; rejected(slot,live,SOURCE_SLOT_PATH); event.position=6;
    path.document_low++; rejected(slot,live,SOURCE_SLOT_ASSOCIATION); path.document_low--;
    WorkbookHistoryEventInfo copy=*frame; changed=slot; changed.payload.event=&copy; copy.encoding++;
    rejected(changed,live,SOURCE_SLOT_UNSUPPORTED); copy=*frame;
    if (forked) {
        copy.parent_lineage=1; rejected(changed,live,SOURCE_SLOT_ASSOCIATION); copy=*frame;
        copy.forked=false; copy.parent_lineage=copy.lineage; rejected(changed,live,SOURCE_SLOT_ASSOCIATION); copy=*frame;
        WorkbookHistoryPath opaque=path; opaque.origin=5; opaque.segments=segments+1; opaque.segment_count=1; changed.path=&opaque;
        rejected(changed,live,SOURCE_SLOT_ASSOCIATION); changed=slot;
    }
    if (definition) {
        ledger.items[0].raw=raw(); ledger.bytes+=ledger.items[0].raw.bytes;
        CHECK(workbook_source_slot_admit(slot,live,info.staging,&out)==SOURCE_SLOT_OK && out.compatibility==SOURCE_DEPENDENCY_REFINED && out.observations.sources.captured_pending==1);
        ledger.items[0].path[1]='S';
        CHECK(workbook_source_slot_admit(slot,live,info.staging,&out)==SOURCE_SLOT_OK && out.compatibility==SOURCE_DEPENDENCY_CONFLICT && out.observations.sources.conflicting==1);
        ledger.items[0].path[1]='s';
    }
    SourceDependencyLive unavailable=live; unavailable.versions=NULL;
    CHECK(workbook_source_slot_admit(slot,unavailable,info.staging,&out)==SOURCE_SLOT_OK && out.compatibility==SOURCE_DEPENDENCY_UNAVAILABLE && out.observations.sources.allocator_unavailable);
    /* Immutable maximal counters remain readable; no operation is issued. */
    WorkbookHistorySegment last={frame->lineage,UINT64_MAX-1,UINT64_MAX}; WorkbookHistoryEventPosition final={UINT64_MAX,frame->lineage,UINT64_MAX};
    WorkbookHistoryPath max=path; max.origin=max.available=max.cursor=UINT64_MAX-1; max.tail=UINT64_MAX; max.segments=&last; max.segment_count=1; max.events=&final; max.operation_high_water=UINT64_MAX;
    copy=*frame; copy.forked=false; copy.parent_lineage=copy.lineage; copy.previous_position=UINT64_MAX-1; copy.position=copy.transition=UINT64_MAX;
    changed=slot; changed.path=&max; changed.payload.event=&copy;
    CHECK(workbook_source_slot_admit(changed,live,info.staging,&out)==SOURCE_SLOT_OK);
    CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
    format_free(&before); format_free(&after); source_versions_free(&ledger);
    CHECK(out.compatibility==(definition ? SOURCE_DEPENDENCY_REFINED : SOURCE_DEPENDENCY_EXACT));
}
int main(void) { binding(false,false); binding(false,true); binding(true,false); binding(true,true); printf("PASS: %u assigned actual source history slot checks\n",checks); return 0; }
