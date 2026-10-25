#include "workbook/range_history_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range slot:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
#include "support/range_payload_fixture.h"
static void rejected(WorkbookRangeHistorySlot slot,SourceDependencyLive live,WorkbookRangeSlotStatus expected) {
    WorkbookRangePayloadInfo info={.sheet=91,.owned=92},before=info; WorkbookRangeSlotAdmission result={.compatibility=SOURCE_DEPENDENCY_CONFLICT,.observations={.unbound_sources=93}},held=result;
    CHECK(workbook_range_slot_preflight(slot,&info)==expected && !memcmp(&before,&info,sizeof(info)));
    CHECK(workbook_range_slot_admit(slot,live,SIZE_MAX,&result)==expected && !memcmp(&held,&result,sizeof(result)));
}
static void assigned(bool forked,unsigned profile) {
    Fixture f; WorkbookHistoryStreamOrigin origin={.owner={1,2,9,VERSION_WORKING_HEAD},.edit_session=3,.command_revision=8,
        .available=4,.cursor=4,.tail=forked ? 6 : 4,.lineage=4,.action_high_water=12,.transition_high_water=20,.lineage_high_water=8};
    fixture_origin(&f,profile,origin); const WorkbookHistoryEventInfo *frame=f.view.event;
    CHECK(frame->action==13 && frame->transition==21 && frame->position==5 && frame->forked==forked);
    WorkbookHistorySegment segments[2]={{4,0,forked ? 4 : 5},{frame->lineage,4,5}};
    WorkbookHistoryEventPosition event={frame->transition,frame->lineage,frame->position};
    WorkbookHistoryPath path={.document_high=1,.document_low=2,.segments=segments,.segment_count=forked ? 2 : 1,.events=&event,.event_count=1,
        .lineage_high_water=forked ? frame->lineage : origin.lineage_high_water,.operation_high_water=frame->transition,.available=4,.cursor=4,.tail=5};
    WorkbookRangeHistorySlot slot={&path,0,origin.owner,origin.edit_session,f.view}; SourceDependencyLive live={6,1,profile ? &f.sources : NULL};
    WorkbookHistoryPath old_path=path; WorkbookHistoryEventInfo old_frame=*frame; WorkbookHistoryStreamState old_stream=workbook_history_stream_state(f.stream);
    WorkbookRangePayloadInfo info={0}; WorkbookRangeSlotAdmission report={0};
    CHECK(workbook_range_slot_preflight(slot,&info)==RANGE_SLOT_OK && info.sheet==6 && info.effects==f.view.effects);
    CHECK(workbook_range_slot_admit(slot,live,info.staging,&report)==RANGE_SLOT_OK && report.compatibility==SOURCE_DEPENDENCY_EXACT && report.observations.unbound_sources==(profile==4 ? 2u : 0u));
    WorkbookRangeSlotAdmission held=report;
    CHECK(workbook_range_slot_admit(slot,live,info.staging-1,&report)==RANGE_SLOT_LIMIT && !memcmp(&held,&report,sizeof(report)));
    if (profile) {
        live.versions=NULL; CHECK(workbook_range_slot_admit(slot,live,info.staging,&report)==RANGE_SLOT_OK && report.compatibility==SOURCE_DEPENDENCY_UNAVAILABLE && report.observations.sources.allocator_unavailable); live.versions=&f.sources;
    }
    live.sheet=7; CHECK(workbook_range_slot_admit(slot,live,info.staging,&report)==RANGE_SLOT_OK && report.compatibility==SOURCE_DEPENDENCY_CONFLICT); live.sheet=6;
    for (unsigned fault=0;fault<9;fault++) {
        WorkbookRangeHistorySlot changed=slot; WorkbookHistoryEventInfo bad=*frame;
        switch (fault) {
        case 0: changed.path=NULL; break; case 1: changed.index=SIZE_MAX; break; case 2: changed.owner.session++; break; case 3: changed.edit_session++; break;
        case 4: bad.encoding=3; changed.payload.event=&bad; break; case 5: bad.bytes--; changed.payload.event=&bad; break;
        case 6: changed.payload.effects=0; break; case 7: changed.owner.mode=VERSION_HISTORICAL_VIEW; break; case 8: changed.payload.sheet++; break;
        }
        rejected(changed,live,fault==0 || fault==7 || fault==5 || fault==6 || fault==8 ? RANGE_SLOT_INVALID : fault==4 ? RANGE_SLOT_UNSUPPORTED : RANGE_SLOT_ASSOCIATION);
    }
    event.operation=frame->action; rejected(slot,live,RANGE_SLOT_ASSOCIATION); event.operation=frame->transition;
    path.cursor=6; rejected(slot,live,RANGE_SLOT_PATH); path.cursor=4;
    WorkbookHistoryEventInfo changed=*frame; changed.action++; WorkbookRangeHistorySlot copy=slot; copy.payload.event=&changed;
    CHECK(workbook_range_slot_preflight(copy,&info)==RANGE_SLOT_OK); /* Path operation binds transition, not reusable action. */
    changed.transition++; rejected(copy,live,RANGE_SLOT_ASSOCIATION); changed=*frame;
    if (forked) {
        changed.parent_lineage=3; copy.payload.event=&changed; rejected(copy,live,RANGE_SLOT_ASSOCIATION);
        WorkbookHistoryPath opaque=path; opaque.origin=4; opaque.segments=segments+1; opaque.segment_count=1; copy=slot; copy.path=&opaque; rejected(copy,live,RANGE_SLOT_ASSOCIATION);
    }
    CHECK(!memcmp(&old_path,&path,sizeof(path)) && !memcmp(&old_frame,frame,sizeof(old_frame)));
    WorkbookHistoryStreamState current=workbook_history_stream_state(f.stream); CHECK(!memcmp(&old_stream,&current,sizeof(current)));
    finish(&f); CHECK(report.compatibility==SOURCE_DEPENDENCY_CONFLICT); /* No retained decoder/stream owner in the report. */
}
int main(void) { for (unsigned forked=0;forked<2;forked++) { assigned(forked,0); assigned(forked,4); assigned(forked,6); } printf("PASS: %u assigned actual family4 slot checks\n",checks); return 0; }
