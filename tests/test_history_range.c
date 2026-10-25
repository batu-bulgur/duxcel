#include "workbook/history_range.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history range:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void source_children(void) {
    WorkbookHistoryStreamOrigin origin={.owner={1,2,3,VERSION_WORKING_HEAD},.edit_session=4}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(stream);
    Analysis source={.kind=SOURCE_CSV,.path="/table.csv"}; ObjectStore objects={0}; CHECK(objects_add_table(&objects,2,4,"Unbound",&source,NULL)==0);
    ObjectSnapshot before={objects.items,objects.count}; SheetObject moved=objects.items[0]; moved.row=20; ObjectSnapshot after={&moved,1}; SourceVersions ledger={.high_water=17};
    WorkbookHistoryRangeRecord record={.workbook={1,2},.edit_session=4,.sheet_session=5,.revision=1,.validation_reserved=SIZE_MAX,
        .view={.sheet=6,.effects=4,.object_high_water=objects.next_id,.definitions={.sheet=6,.object_high_water=objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&ledger}}};
    void *ticket=NULL,*blocked=NULL; CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL) && ticket);
    CHECK(!recorder.reserve(recorder.context,&record,&blocked,NULL) && !blocked && !workbook_history_stream_state(stream).current.action_high_water);
    recorder.cancel(recorder.context,ticket); ticket=NULL; CHECK(!workbook_history_stream_state(stream).owned_bytes && !workbook_history_stream_state(stream).current.action_high_water);
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); objects_free(&objects); recorder.publish(recorder.context,ticket); ticket=NULL;
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(info->family==4 && info->encoding==4 && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==4 && event.unbound_sources==2 && !strcmp(event.definitions.after_objects.objects.items[0].source.path,"/table.csv"));
    CHECK(workbook_history_stream_retry(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK); range_event_free(&event);
    record.expected_revision=1; record.revision=2; record.view=(RangeEventView){.sheet=6,.effects=8,.object_high_water=1,.sources=&ledger,.after_formats={.source_aware=true}};
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); recorder.publish(recorder.context,ticket); ticket=NULL;
    CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK); info=workbook_history_event_info(held);
    CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==8 && !event.formats.before.source_aware && event.formats.after.source_aware && !event.definitions.effects && !event.sources.versions.count && event.sources.versions.high_water==17);
    range_event_free(&event); CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK);
}
int main(void) { source_children();
    WorkbookHistoryStream *stream=NULL; WorkbookHistoryStreamOrigin origin={.owner={1,2,9,VERSION_WORKING_HEAD},.edit_session=3};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,1024},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(stream);
    CellDelta delta={.sheet=1,.row=1,.column=1,.before="X",.before_bytes=1};
    WorkbookHistoryRangeRecord record={.workbook={1,2},.edit_session=3,.sheet_session=4,.revision=1,.view={.sheet=1,.effects=2,.cells=&delta,.count=1}};
    void *ticket=NULL; CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL) && ticket); recorder.cancel(recorder.context,ticket); ticket=NULL;
    CHECK(!workbook_history_stream_state(stream).owned_bytes && !workbook_history_stream_state(stream).current.action_high_water);
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); recorder.publish(recorder.context,ticket); ticket=NULL;
    CHECK(workbook_history_stream_state(stream).current.action_high_water==1 && workbook_history_stream_state(stream).pending==1);
    CHECK(!recorder.reserve(recorder.context,&record,&ticket,NULL) && !ticket); record.expected_revision=1; record.revision=2; delta.after="Y"; delta.after_bytes=1;
    ColumnWidthEntry width={3,column_width_bits(120)};
    record.view.effects=18; record.view.widths=(ColumnWidthPair){{0},{&width,1}};
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); recorder.publish(recorder.context,ticket); ticket=NULL;
    record.expected_revision=2; record.revision=3; CHECK(!recorder.reserve(recorder.context,&record,&ticket,NULL));
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(info->family==DX_HISTORY_RANGE_FAMILY && info->encoding==DX_RANGE_EVENT_ENCODING && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.cells[0].before_bytes==1 && !event.cells[0].after_bytes);
    range_event_free(&event); CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
    info=workbook_history_event_info(held);
    CHECK(info->encoding==DX_RANGE_EVENT_ENCODING && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==18 && event.widths.after.count==1 && event.widths.after.entries[0].bits==width.bits);
    range_event_free(&event); CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); printf("PASS: %u complete range recorder checks\n",checks); return 0;
}
