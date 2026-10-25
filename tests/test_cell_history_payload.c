#include "workbook/cell_history_payload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell payload:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static void denied(WorkbookCellPayloadView v,size_t cap,WorkbookCellPayloadStatus expected) {
    WorkbookCellPayloadInfo out={.bytes=123},before=out;
    CHECK(workbook_cell_payload_check(v,cap,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
static void denied_slot(WorkbookCellHistorySlot v,size_t cap,WorkbookCellSlotStatus expected) {
    WorkbookCellPayloadInfo out={.bytes=456},before=out;
    CHECK(workbook_cell_slot_check(v,cap,&out)==expected && !memcmp(&out,&before,sizeof(out)));
}
static void issued(bool forked,bool maximum) {
    char *large_a=NULL,*large_b=NULL; CellDelta delta={7,10,2,"Old","New",3,3};
    if (maximum) {
        large_a=dx_alloc(DX_CELL_DELTA_TEXT_BYTES); large_b=dx_alloc(DX_CELL_DELTA_TEXT_BYTES);
        memset(large_a,'a',DX_CELL_DELTA_TEXT_BYTES); memset(large_b,'b',DX_CELL_DELTA_TEXT_BYTES);
        delta=(CellDelta){7,DX_SHEET_ROWS-1,16383,large_a,large_b,DX_CELL_DELTA_TEXT_BYTES,DX_CELL_DELTA_TEXT_BYTES};
    }
    WorkbookVersionOwner owner={1,2,99,VERSION_WORKING_HEAD};
    WorkbookHistoryStreamOrigin origin={.owner=owner,.edit_session=44,.command_revision=100,.cursor=5,.tail=forked ? 7 : 5,.lineage=2,
        .action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    CellEditRecorder r=workbook_history_stream_cell_recorder(s);
    CellEditRecord record={.target={.document=44,.sheet=88,.workbook={1,2},.sheet_id=7,.expected_document_revision=100,.row=delta.row,.column=delta.column},.revision=101,.delta=delta};
    void *ticket=NULL; CHECK(r.reserve(r.context,&record,&ticket,NULL) && ticket); r.publish(r.context,ticket);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK && held);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(held); CHECK(frame->action==41 && frame->transition==61 && frame->forked==forked);
    WorkbookCellPayloadView v={frame,workbook_history_event_payload(held),workbook_history_event_dependencies(held),frame->bytes,frame->dependency_bytes,7,2};
    WorkbookCellPayloadInfo out={0}; CHECK(workbook_cell_payload_check(v,v.bytes,&out)==CELL_PAYLOAD_OK && out.sheet==7 && out.row==delta.row && out.column==delta.column && out.before_bytes==delta.before_bytes && out.after_bytes==delta.after_bytes && out.effects==2);
    denied(v,v.bytes-1,CELL_PAYLOAD_LIMIT); denied(v,0,CELL_PAYLOAD_LIMIT);
    WorkbookCellPayloadView bad=v; bad.sheet=8; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); bad=v; bad.effects=8; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    bad=v; bad.dependencies="x"; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); bad=v; bad.dependency_bytes=1; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    bad=v; bad.bytes--; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    WorkbookHistoryEventInfo copy=*frame; bad=v; bad.event=&copy; copy.family=99; denied(bad,SIZE_MAX,CELL_PAYLOAD_UNSUPPORTED);
    copy=*frame; copy.encoding=2; denied(bad,SIZE_MAX,CELL_PAYLOAD_UNSUPPORTED);
    copy=*frame; copy.inactive_scopes=1; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    copy=*frame; copy.dependency_encoding=1; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    copy=*frame; copy.transition=0; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID);
    unsigned char *wire=dx_alloc(v.bytes); memcpy(wire,v.data,v.bytes); bad=v; bad.data=wire;
    wire[6]=1; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); wire[6]=0;
    wire[4]=2; denied(bad,SIZE_MAX,CELL_PAYLOAD_UNSUPPORTED); wire[4]=1;
    wire[DX_CELL_DELTA_HEADER_BYTES]=0; denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); memcpy(wire,v.data,v.bytes);
    put(wire+16,DX_SHEET_ROWS,8); denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); memcpy(wire,v.data,v.bytes);
    put(wire+24,16384,2); denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); memcpy(wire,v.data,v.bytes);
    memcpy(wire+DX_CELL_DELTA_HEADER_BYTES+delta.before_bytes,wire+DX_CELL_DELTA_HEADER_BYTES,delta.before_bytes);
    denied(bad,SIZE_MAX,CELL_PAYLOAD_INVALID); free(wire);
    WorkbookHistorySegment segments[2]={{2,0,6},{8,5,6}}; if (forked) segments[0].through=5;
    WorkbookHistoryEventPosition event={61,frame->lineage,6}; WorkbookHistoryPath path={.document_high=1,.document_low=2,.segments=segments,.segment_count=forked ? 2 : 1,
        .events=&event,.event_count=1,.lineage_high_water=frame->lineage,.operation_high_water=61,.available=5,.cursor=5,.tail=6};
    WorkbookCellHistorySlot slot={&path,0,owner,44,v};
    CHECK(workbook_cell_slot_check(slot,v.bytes,&out)==CELL_SLOT_OK && out.bytes==v.bytes);
    denied_slot(slot,v.bytes-1,CELL_SLOT_LIMIT);
    WorkbookCellHistorySlot changed=slot; changed.index=1; denied_slot(changed,SIZE_MAX,CELL_SLOT_ASSOCIATION);
    changed=slot; changed.owner.session++; denied_slot(changed,SIZE_MAX,CELL_SLOT_ASSOCIATION);
    changed=slot; changed.edit_session++; denied_slot(changed,SIZE_MAX,CELL_SLOT_ASSOCIATION);
    event.operation=frame->action; denied_slot(slot,SIZE_MAX,CELL_SLOT_ASSOCIATION); event.operation=61;
    path.document_low++; denied_slot(slot,SIZE_MAX,CELL_SLOT_ASSOCIATION); path.document_low--;
    event.position=UINT64_MAX; denied_slot(slot,SIZE_MAX,CELL_SLOT_PATH); event.position=6;
    if (forked) {
        copy=*frame; copy.parent_lineage=1; changed=slot; changed.payload.event=&copy; denied_slot(changed,SIZE_MAX,CELL_SLOT_ASSOCIATION);
        WorkbookHistoryPath opaque=path; opaque.origin=5; opaque.segments=segments+1; opaque.segment_count=1; changed=slot; changed.path=&opaque;
        denied_slot(changed,SIZE_MAX,CELL_SLOT_ASSOCIATION);
    }
    WorkbookHistorySegment last={frame->lineage,UINT64_MAX-1,UINT64_MAX}; WorkbookHistoryEventPosition final={UINT64_MAX,frame->lineage,UINT64_MAX};
    WorkbookHistoryPath max=path; max.origin=max.available=max.cursor=UINT64_MAX-1; max.tail=UINT64_MAX; max.segments=&last; max.segment_count=1; max.events=&final; max.operation_high_water=UINT64_MAX;
    copy=*frame; copy.forked=false; copy.parent_lineage=copy.lineage; copy.previous_position=UINT64_MAX-1; copy.position=copy.transition=copy.revision=UINT64_MAX;
    changed=slot; changed.path=&max; changed.payload.event=&copy;
    CHECK(workbook_cell_slot_check(changed,v.bytes,&out)==CELL_SLOT_OK);
    CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    free(large_a); free(large_b); CHECK(out.before_bytes==delta.before_bytes && out.after_bytes==delta.after_bytes);
}
static void clear_literal(void) {
    unsigned char wire[64]; size_t bytes=0; CellDelta delta={7,0,0,NULL,"NULL",0,4}; CHECK(cell_delta_encode(delta,wire,sizeof(wire),&bytes)==CELL_DELTA_CODEC_OK);
    WorkbookHistoryEventInfo frame={.owner={1,2,99,VERSION_WORKING_HEAD},.edit_session=44,.revision=1,.action=1,.transition=2,.previous_position=0,.position=1,.family=1,.encoding=1,.bytes=bytes};
    WorkbookCellPayloadView v={&frame,wire,NULL,bytes,0,7,2}; WorkbookCellPayloadInfo out={0};
    CHECK(workbook_cell_payload_check(v,bytes,&out)==CELL_PAYLOAD_OK && !out.before_bytes && out.after_bytes==4);
    CHECK(!memcmp(wire+DX_CELL_DELTA_HEADER_BYTES,"NULL",4));
}
int main(void) { issued(false,false); issued(true,false); issued(false,true); clear_literal(); printf("PASS: %u actual cell frame and assigned-slot checks\n",checks); return 0; }
