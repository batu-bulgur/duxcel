#include "workbook/history_sheet_topology.h"
#include "workbook/history_sheet_lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL topology reservation:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SheetTopologyPacket packet(WorkbookId document) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Added",&(Analysis){0},NULL)); b.id=document; b.sheet_high_water=2; b.sheets[0].id=2;
    SheetPresenceAsset *asset=NULL; CHECK(sheet_presence_capture((SheetPresenceView){document,2,b.sheets,&b.sheets[0].analysis},SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
    SheetPresencePacket presence={0}; CHECK(sheet_presence_encode(asset,SIZE_MAX,&presence)==SHEET_PRESENCE_OK);
    SheetLifecycleEntry before[]={{1,"Base",false}},after[]={{1,"Base",false},{2,"Added",false}};
    SheetTopologyPacket out={0}; CHECK(sheet_topology_capture((SheetTopologyView){SHEET_TOPOLOGY_ADD,2,{document,1,1,before,1},{document,2,2,after,2},&presence},SIZE_MAX,&out)==SHEET_TOPOLOGY_OK);
    sheet_presence_packet_free(&presence); sheet_presence_free(&asset); workbook_free(&b); return out;
}
static bool same_origin(WorkbookHistoryStreamOrigin a,WorkbookHistoryStreamOrigin b) {
    return a.owner.document_high==b.owner.document_high && a.owner.document_low==b.owner.document_low && a.owner.session==b.owner.session && a.owner.mode==b.owner.mode &&
        a.edit_session==b.edit_session && a.command_revision==b.command_revision && a.origin==b.origin && a.available==b.available && a.cursor==b.cursor && a.tail==b.tail &&
        a.lineage==b.lineage && a.action_high_water==b.action_high_water && a.transition_high_water==b.transition_high_water && a.lineage_high_water==b.lineage_high_water;
}
static bool same(WorkbookHistoryStreamState a,WorkbookHistoryStreamState b) { return same_origin(a.current,b.current) && a.pending==b.pending && a.owned_bytes==b.owned_bytes && a.reserved==b.reserved && a.in_flight==b.in_flight; }
int main(void) {
    WorkbookId document={1,2}; SheetTopologyPacket topology=packet(document); SheetTopologyInfo topology_info;
    CHECK(sheet_topology_preflight(topology.data,topology.bytes,&topology_info)==SHEET_TOPOLOGY_OK);
    WorkbookHistoryStreamOrigin origin={.owner={1,2,9,VERSION_WORKING_HEAD},.edit_session=3,.command_revision=5,.cursor=1,.tail=2,.lineage=3,.lineage_high_water=3,.action_high_water=7,.transition_high_water=11};
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryTopologyRecord record={document,3,5,6,&topology}; WorkbookHistoryTopologyTicket *ticket=NULL,*other=NULL;
    size_t budget=workbook_history_topology_ticket_allocation()+topology_info.validation_scratch;
    WorkbookHistoryStreamState before=workbook_history_stream_state(stream);
    CHECK(!workbook_history_topology_reserve(stream,&record,budget-1,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream)));
    record.edit_session++; CHECK(!workbook_history_topology_reserve(stream,&record,budget,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream))); record.edit_session--;
    record.expected_revision--; CHECK(!workbook_history_topology_reserve(stream,&record,budget,&ticket,NULL) && !ticket); record.expected_revision++;
    record.revision++; CHECK(!workbook_history_topology_reserve(stream,&record,budget,&ticket,NULL) && !ticket); record.revision--;
    record.workbook.low++; CHECK(!workbook_history_topology_reserve(stream,&record,budget,&ticket,NULL) && !ticket); record.workbook.low--;
    size_t presence_at=topology.bytes-topology_info.presence_bytes; unsigned char *bytes=topology.data; unsigned char first=bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES];
    bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES]='X';
    CHECK(sheet_topology_preflight(topology.data,topology.bytes,&topology_info)==SHEET_TOPOLOGY_OK);
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream)));
    bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES]=first;
    CHECK(DX_HISTORY_SHEET_TOPOLOGY_FAMILY==DX_HISTORY_SHEET_LIFECYCLE_FAMILY && DX_HISTORY_SHEET_TOPOLOGY_PROFILE==2 && DX_SHEET_LIFECYCLE_ENCODING==1);
    /* Profile1 remains a genuine different current payload kind. */
    SheetLifecycleEntry a[]={{1,"Base",false}},b[]={{1,"Renamed",false}};
    SheetLifecyclePacket metadata={0}; CHECK(sheet_lifecycle_capture((SheetLifecycle){{document,1,1,a,1},{document,1,1,b,1}},SIZE_MAX,&metadata)==SHEET_LIFECYCLE_OK);
    SheetTopologyPacket wrong={metadata.data,metadata.bytes}; record.packet=&wrong;
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream))); record.packet=&topology;
    WorkbookHistorySheetLifecycleRecorder old=workbook_history_stream_sheet_lifecycle_recorder(stream);
    SheetLifecyclePacket wrong_metadata={topology.data,topology.bytes};
    WorkbookHistorySheetLifecycleRecord mr={document,3,4,1,5,6,&wrong_metadata}; void *raw=NULL;
    CHECK(!old.reserve(old.context,&mr,&raw,NULL) && !raw && same(before,workbook_history_stream_state(stream)));
    mr.packet=&metadata; CHECK(old.reserve(old.context,&mr,&raw,NULL) && raw);
    const WorkbookHistoryEventInfo *metadata_info=workbook_history_event_info(raw);
    CHECK(metadata_info->family==DX_HISTORY_SHEET_TOPOLOGY_FAMILY && metadata_info->encoding==DX_SHEET_LIFECYCLE_ENCODING && !memcmp(workbook_history_event_payload(raw),"DXSL",4));
    old.cancel(old.context,raw); raw=NULL; sheet_lifecycle_packet_free(&metadata);
    CHECK(same(before,workbook_history_stream_state(stream)));
    CHECK(workbook_history_topology_reserve(stream,&record,budget,&ticket,NULL) && ticket);
    const WorkbookHistoryEventInfo *info=workbook_history_topology_info(ticket);
    CHECK(info->family==DX_HISTORY_SHEET_TOPOLOGY_FAMILY && info->encoding==DX_HISTORY_SHEET_TOPOLOGY_PROFILE && info->encoding!=DX_SHEET_LIFECYCLE_ENCODING && info->bytes==topology.bytes && !info->dependency_bytes);
    CHECK(info->revision==6 && info->action==8 && info->transition==12 && info->forked && info->lineage==4 && info->parent_lineage==3 && info->position==2);
    WorkbookHistoryStreamState held=workbook_history_stream_state(stream);
    CHECK(held.reserved && held.owned_bytes>=info->bytes && !held.pending && same_origin(held.current,before.current));
    CHECK(workbook_history_topology_payload(ticket)!=topology.data && !memcmp(workbook_history_topology_payload(ticket),topology.data,topology.bytes));
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&ticket,NULL) && same(held,workbook_history_stream_state(stream)));
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&other,NULL) && !other && same(held,workbook_history_stream_state(stream)));
    CHECK(workbook_history_stream_free(&stream,true)==HISTORY_STREAM_BUSY && stream);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_EMPTY && !event);
    sheet_topology_packet_free(&topology);
    CHECK(!memcmp(workbook_history_topology_payload(ticket),"DXTP",4) && sheet_topology_validate(workbook_history_topology_payload(ticket),info->bytes,SIZE_MAX,&topology_info)==SHEET_TOPOLOGY_OK);
    workbook_history_topology_cancel(&ticket); CHECK(!ticket && same(before,workbook_history_stream_state(stream)));
    workbook_history_topology_cancel(&ticket); workbook_history_topology_cancel(NULL);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK && !stream);
    topology=packet(document); record.packet=&topology;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&stream)==HISTORY_STREAM_OK); before=workbook_history_stream_state(stream);
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream)));
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); sheet_topology_packet_free(&topology);
    topology=packet(document); record.packet=&topology; origin.action_high_water=UINT64_MAX;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK); before=workbook_history_stream_state(stream);
    CHECK(!workbook_history_topology_reserve(stream,&record,SIZE_MAX,&ticket,NULL) && !ticket && same(before,workbook_history_stream_state(stream)));
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); sheet_topology_packet_free(&topology);
    CHECK(!workbook_history_topology_info(NULL) && !workbook_history_topology_payload(NULL));
    printf("PASS: %u current topology reserve/cancel ownership checks\n",checks); return 0;
}
