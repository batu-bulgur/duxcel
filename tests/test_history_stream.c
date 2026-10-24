#include "workbook/history_stream.h"
#include "workbook/cell_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history stream:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryStreamOrigin origin(void) {
    return (WorkbookHistoryStreamOrigin){.owner={1,2,99,VERSION_WORKING_HEAD},.edit_session=44,.command_revision=100,
        .cursor=5,.tail=5,.lineage=2,.action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
}
static WorkbookHistoryStream *create(WorkbookHistoryStreamOrigin o,size_t entries,size_t bytes) {
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(o,(WorkbookHistoryStreamOptions){entries,bytes},&s)==HISTORY_STREAM_OK && s); return s;
}
static CellEditRecord record(WorkbookHistoryStream *s) {
    WorkbookHistoryStreamOrigin o=workbook_history_stream_state(s).current;
    return (CellEditRecord){.target={.document=o.edit_session,.sheet=88,.workbook={o.owner.document_high,o.owner.document_low},
        .sheet_id=7,.expected_document_revision=o.command_revision,.row=10,.column=2},.revision=o.command_revision+1,
        .delta={7,10,2,"Old","New",3,3}};
}
static bool same_origin(WorkbookHistoryStreamOrigin a,WorkbookHistoryStreamOrigin b) {
    return a.owner.document_high==b.owner.document_high && a.owner.document_low==b.owner.document_low && a.owner.session==b.owner.session && a.owner.mode==b.owner.mode &&
        a.edit_session==b.edit_session && a.command_revision==b.command_revision && a.origin==b.origin && a.available==b.available && a.cursor==b.cursor && a.tail==b.tail &&
        a.lineage==b.lineage && a.action_high_water==b.action_high_water && a.transition_high_water==b.transition_high_water && a.lineage_high_water==b.lineage_high_water;
}
static void reject(WorkbookHistoryStream *s,CellEditRecord r) {
    WorkbookHistoryStreamState before=workbook_history_stream_state(s); CellEditRecorder adapter=workbook_history_stream_cell_recorder(s);
    void *ticket=NULL; char *error=NULL;
    CHECK(!adapter.reserve(adapter.context,&r,&ticket,&error) && error && !ticket); free(error);
    WorkbookHistoryStreamState after=workbook_history_stream_state(s);
    CHECK(same_origin(before.current,after.current) && before.pending==after.pending && before.owned_bytes==after.owned_bytes && before.reserved==after.reserved && before.in_flight==after.in_flight);
}
static void publish(WorkbookHistoryStream *s) {
    CellEditRecorder adapter=workbook_history_stream_cell_recorder(s); CellEditRecord r=record(s); void *ticket=NULL; char *error=NULL;
    CHECK(adapter.reserve(adapter.context,&r,&ticket,&error) && ticket && !error); adapter.publish(adapter.context,ticket);
}
static void test_construction(void) {
    WorkbookHistoryStreamOrigin base=origin(),o; WorkbookHistoryStream *s=NULL;
    WorkbookHistoryStreamOptions options={2,4096};
    CHECK(workbook_history_stream_new(base,options,NULL)==HISTORY_STREAM_INVALID);
    o=base; o.owner.document_high=o.owner.document_low=0; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.owner.session=0; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.edit_session=0; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.owner.mode=VERSION_HISTORICAL_VIEW; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_READ_ONLY && !s);
    o=base; o.origin=1; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.available=6; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.cursor=6; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    o=base; o.lineage=8; CHECK(workbook_history_stream_new(o,options,&s)==HISTORY_STREAM_INVALID && !s);
    CHECK(workbook_history_stream_new(base,(WorkbookHistoryStreamOptions){0,4096},&s)==HISTORY_STREAM_LIMIT && !s);
    CHECK(workbook_history_stream_new(base,(WorkbookHistoryStreamOptions){257,4096},&s)==HISTORY_STREAM_LIMIT && !s);
    CHECK(workbook_history_stream_new(base,(WorkbookHistoryStreamOptions){2,0},&s)==HISTORY_STREAM_LIMIT && !s);
    CHECK(workbook_history_stream_new(base,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_BYTES+1},&s)==HISTORY_STREAM_LIMIT && !s);
    s=create(base,2,4096); WorkbookHistoryStream *same=s;
    CHECK(workbook_history_stream_new(base,options,&s)==HISTORY_STREAM_INVALID && s==same);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK && !s);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_free(NULL,false)==HISTORY_STREAM_INVALID);
}
static void test_reservation_and_fences(void) {
    WorkbookHistoryStream *s=create(origin(),2,4096); CellEditRecorder adapter=workbook_history_stream_cell_recorder(s);
    CellEditRecord r=record(s); void *ticket=NULL; char *error=NULL; WorkbookHistoryStreamState before=workbook_history_stream_state(s);
    char text[]={'N','e','w'}; r.delta.after=text;
    CHECK(adapter.reserve(adapter.context,&r,&ticket,&error) && ticket && !error);
    WorkbookHistoryStreamState reserved=workbook_history_stream_state(s);
    CHECK(reserved.reserved && !reserved.pending && reserved.owned_bytes>DX_CELL_DELTA_HEADER_BYTES+6 && same_origin(before.current,reserved.current));
    CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_BUSY && s);
    reject(s,record(s)); adapter.cancel(adapter.context,ticket); ticket=NULL;
    CHECK(!workbook_history_stream_state(s).owned_bytes && same_origin(before.current,workbook_history_stream_state(s).current));
    CHECK(adapter.reserve(adapter.context,&r,&ticket,&error) && ticket && !error); memset(text,'x',3); adapter.publish(adapter.context,ticket);
    WorkbookHistoryStreamState published=workbook_history_stream_state(s);
    CHECK(!published.reserved && published.pending==1 && published.owned_bytes==reserved.owned_bytes && published.current.command_revision==101 &&
        published.current.cursor==6 && published.current.tail==6 && published.current.action_high_water==41 && published.current.transition_high_water==61 && published.current.lineage==2 && published.current.lineage_high_water==7);
    reject(s,r); r=record(s); r.target.workbook.low++; reject(s,r);
    r=record(s); r.target.document++; reject(s,r);
    r=record(s); r.target.sheet=0; reject(s,r);
    r=record(s); r.target.sheet_id++; reject(s,r);
    r=record(s); r.target.row++; reject(s,r);
    r=record(s); r.target.column++; reject(s,r);
    r=record(s); r.revision++; reject(s,r);
    r=record(s); r.delta.after=r.delta.before; reject(s,r);
    r=record(s); r.delta.after=NULL; reject(s,r);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); CellDelta value;
    CHECK(info && info->action==41 && info->transition==61 && info->revision==101 && info->position==6 && info->previous_position==5 && !info->forked && info->parent_lineage==2 && info->owner.session==99 && info->edit_session==44 && info->family==DX_HISTORY_CELL_FAMILY && info->encoding==DX_CELL_DELTA_ENCODING);
    CHECK(cell_delta_decode(workbook_history_event_payload(event),info->bytes,&value)==CELL_DELTA_CODEC_OK && !memcmp(value.after,"New",3));
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && !event && !workbook_history_stream_state(s).owned_bytes);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    CHECK(!workbook_history_event_info(NULL) && !workbook_history_event_payload(NULL));
}
static void test_fifo_budget_and_handoff(void) {
    WorkbookHistoryStream *s=create(origin(),2,4096),*other=create(origin(),2,4096); WorkbookHistoryEvent *event=NULL,*empty=NULL;
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_EMPTY && !event);
    CHECK(workbook_history_stream_dispatch(NULL,&event)==HISTORY_STREAM_INVALID);
    publish(s); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_BUSY && s);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK); const WorkbookHistoryEvent *identity=event;
    size_t charge=workbook_history_stream_state(s).owned_bytes;
    CHECK(workbook_history_stream_dispatch(s,&empty)==HISTORY_STREAM_BUSY && !empty);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_INVALID && event==identity);
    CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_BUSY && s);
    CHECK(workbook_history_stream_complete(other,&event)==HISTORY_STREAM_STALE && event==identity);
    CHECK(workbook_history_stream_retry(other,&event)==HISTORY_STREAM_STALE && event==identity);
    publish(s); WorkbookHistoryStreamState state=workbook_history_stream_state(s);
    CHECK(state.pending==1 && state.in_flight && state.owned_bytes==2*charge); reject(s,record(s));
    CHECK(workbook_history_stream_retry(s,&event)==HISTORY_STREAM_OK && !event);
    state=workbook_history_stream_state(s); CHECK(state.pending==2 && !state.in_flight && state.owned_bytes==2*charge);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK && event==identity && workbook_history_event_info(event)->action==41);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && !event && workbook_history_stream_state(s).owned_bytes==charge);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK && workbook_history_event_info(event)->action==42);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && !event && !workbook_history_stream_state(s).owned_bytes);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_INVALID);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK && workbook_history_stream_free(&other,false)==HISTORY_STREAM_OK);
    s=create(origin(),2,charge-1); reject(s,record(s)); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    s=create(origin(),2,charge); publish(s); CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK); reject(s,record(s));
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK); publish(s);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_BUSY && workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK);
}
static void test_fork_and_clocks(void) {
    WorkbookHistoryStreamOrigin o=origin(); o.cursor=3; o.tail=8;
    WorkbookHistoryStream *s=create(o,2,4096); CellEditRecorder adapter=workbook_history_stream_cell_recorder(s);
    CellEditRecord r=record(s); void *ticket=NULL; char *error=NULL;
    CHECK(adapter.reserve(adapter.context,&r,&ticket,&error)); adapter.cancel(adapter.context,ticket);
    CHECK(same_origin(o,workbook_history_stream_state(s).current)); publish(s);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event);
    CHECK(info->forked && info->lineage==8 && info->parent_lineage==2 && info->previous_position==3 && info->position==4 && info->action==41 && info->transition==61);
    WorkbookHistoryStreamState state=workbook_history_stream_state(s); CHECK(state.current.cursor==4 && state.current.tail==4 && state.current.lineage==8 && state.current.lineage_high_water==8);
    publish(s); CHECK(info->forked && info->action==41 && info->position==4); /* Immutable first event. */
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK && !workbook_history_event_info(event)->forked && workbook_history_event_info(event)->lineage==8 && workbook_history_event_info(event)->position==5);
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    for (unsigned kind=0;kind<5;kind++) {
        o=origin();
        if (kind==0) o.action_high_water=UINT64_MAX;
        if (kind==1) o.transition_high_water=UINT64_MAX;
        if (kind==2) o.command_revision=UINT64_MAX;
        if (kind==3) o.cursor=o.tail=UINT64_MAX;
        if (kind==4) { o.lineage_high_water=UINT64_MAX; o.cursor=3; }
        s=create(o,2,4096); reject(s,record(s)); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    }
    o=origin(); o.lineage_high_water=UINT64_MAX; s=create(o,2,4096); publish(s); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK);
    o=origin(); o.action_high_water=o.transition_high_water=o.command_revision=o.cursor=o.tail=UINT64_MAX-1;
    s=create(o,2,4096); publish(s); state=workbook_history_stream_state(s);
    CHECK(state.current.action_high_water==UINT64_MAX && state.current.transition_high_water==UINT64_MAX && state.current.cursor==UINT64_MAX && state.current.command_revision==UINT64_MAX);
    reject(s,record(s)); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK);
}
static void test_large_payload(void) {
    size_t n=DX_CELL_DELTA_TEXT_BYTES; char *before=malloc(n),*after=malloc(n); CHECK(before && after);
    memset(before,'A',n); memset(after,'B',n);
    WorkbookHistoryStream *s=create(origin(),256,DX_HISTORY_STREAM_BYTES); CellEditRecorder adapter=workbook_history_stream_cell_recorder(s); CellEditRecord r=record(s);
    r.delta.before=before; r.delta.after=after; r.delta.before_bytes=r.delta.after_bytes=n;
    void *ticket=NULL; char *error=NULL; CHECK(adapter.reserve(adapter.context,&r,&ticket,&error) && ticket && !error);
    memset(before,'x',n); memset(after,'y',n); adapter.publish(adapter.context,ticket);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK); CellDelta decoded;
    CHECK(cell_delta_decode(workbook_history_event_payload(event),workbook_history_event_info(event)->bytes,&decoded)==CELL_DELTA_CODEC_OK && decoded.before_bytes==n && decoded.after_bytes==n && decoded.before[0]=='A' && decoded.after[n-1]=='B');
    CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    free(before); free(after);
}
int main(void) {
    test_construction(); test_reservation_and_fences(); test_fifo_budget_and_handoff(); test_fork_and_clocks(); test_large_payload();
    printf("PASS: %u bounded owned history-stream checks\n",checks); return 0;
}
