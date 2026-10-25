#include "workbook/primary_source_topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL primary topology:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis current={.kind=SOURCE_CSV,.path="/current.csv"};
    CHECK(workbook_add(&b,"Current",&current,NULL)); CHECK(workbook_add(&b,"Hidden",&(Analysis){0},NULL));
    b.active=0; b.sheets[1].hidden=true; CHECK(workbook_prepare_command_session(&b,0,NULL)); b.dirty=false; return b;
}
static PrimarySourcePublishPlan *source(PrimarySourceDraftContext c) {
    Analysis proposal={.kind=SOURCE_DATABASE,.path="/new.duckdb"}; PrimarySourceDraft *draft=NULL;
    CHECK(primary_source_draft_begin(c,&proposal,"Current",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_READY);
    proposal.schema="main"; proposal.table="Sales";
    CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){*c.request,&proposal,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
    PrimarySourcePublishPlan *plan=NULL; CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY); return plan;
}
static WorkbookHistoryStream *stream(const Workbook *b,bool wrong,size_t bytes) {
    WorkbookHistoryStreamOrigin origin={.owner={b->id.high,b->id.low^(wrong ? 1u : 0u),77,VERSION_WORKING_HEAD},
        .edit_session=b->edit_session_id,.command_revision=b->command_revision,.available=2,.cursor=3,.tail=5,
        .lineage=1,.lineage_high_water=1,.action_high_water=5,.transition_high_water=7};
    WorkbookHistoryStream *out=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,bytes},&out)==HISTORY_STREAM_OK); return out;
}
static bool same_state(WorkbookHistoryStreamState a,WorkbookHistoryStreamState b) {
    return a.current.owner.document_high==b.current.owner.document_high && a.current.owner.document_low==b.current.owner.document_low &&
        a.current.owner.session==b.current.owner.session && a.current.edit_session==b.current.edit_session && a.current.command_revision==b.current.command_revision &&
        a.current.origin==b.current.origin && a.current.available==b.current.available && a.current.cursor==b.current.cursor && a.current.tail==b.current.tail &&
        a.current.lineage==b.current.lineage && a.current.action_high_water==b.current.action_high_water && a.current.transition_high_water==b.current.transition_high_water &&
        a.current.lineage_high_water==b.current.lineage_high_water && a.pending==b.pending && a.owned_bytes==b.owned_bytes && a.reserved==b.reserved && a.in_flight==b.in_flight;
}
static PrimarySourceTopologyPlan *prepare(PrimarySourceDraftContext c) {
    PrimarySourcePublishPlan *candidate=source(c); PrimarySourceTopologyUsage usage={0};
    CHECK(primary_source_topology_measure(c,candidate,&usage,NULL)==PRIMARY_TOPOLOGY_READY && usage.event_bytes && usage.prospective_sheet==3);
    CHECK(usage.total==usage.source+usage.preparation+usage.reservation && usage.source==primary_source_publish_usage(candidate).coexisting);
    Workbook before=*c.book; Worksheet sheet=c.book->sheets[0]; PrimarySourceTopologyPlan *out=NULL;
    CHECK(primary_source_topology_prepare(c,&candidate,usage.total-1,&out,NULL)==PRIMARY_TOPOLOGY_LIMIT && candidate && !out);
    CHECK(!memcmp(&before,c.book,sizeof(before)) && !memcmp(&sheet,c.book->sheets,sizeof(sheet)));
    CHECK(primary_source_topology_prepare(c,&candidate,usage.total,&out,NULL)==PRIMARY_TOPOLOGY_READY && !candidate);
    CHECK(primary_source_topology_check(c,out,NULL)==PRIMARY_TOPOLOGY_READY && !memcmp(&before,c.book,sizeof(before)));
    return out;
}
static void prepare_and_reserve(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; uint64_t order=worksheet_history_high_water();
    PrimarySourceTopologyPlan *plan=prepare(c); PrimarySourceTopologyUsage usage=primary_source_topology_usage(plan);
    const SheetTopologyPacket *packet=primary_source_topology_packet(plan); CHECK(packet && packet->bytes==usage.event_bytes);
    SheetTopologyEvent event={0}; CHECK(sheet_topology_decode(packet->data,packet->bytes,SIZE_MAX,&event)==SHEET_TOPOLOGY_OK);
    CHECK(event.operation==SHEET_TOPOLOGY_ADD && event.sheet==3 && event.before.count==2 && event.after.count==3 && event.before.active==1 && event.after.active==3);
    CHECK(event.before.high_water==2 && event.after.high_water==3 && event.before.sheets[1].hidden && event.after.sheets[1].hidden);
    CHECK(!strcmp(event.after.sheets[0].name,"Current") && !strcmp(event.after.sheets[2].name,"Current (2)")); sheet_topology_free(&event);
    SheetPresenceView proposed=sheet_presence_view(primary_source_topology_presence(plan));
    CHECK(proposed.sheet->id==3 && !proposed.sheet->edit_session_id && !proposed.sheet->view.column_count && b.count==2 && b.sheet_high_water==2 && worksheet_history_high_water()==order);
    /* Explicit blocker: pre-UI empty-view DXSP is not the post-open authored
       source initialization. No publication API may waive this mismatch. */
    Worksheet initialized=*proposed.sheet; char *names[]={"City","Amount"}; float widths[]={128,190};
    initialized.view.column_count=2; initialized.view.columns=names; initialized.view.widths=widths;
    bool matches=true; CHECK(sheet_presence_matches(primary_source_topology_presence(plan),(SheetPresenceView){b.id,3,&initialized,&initialized.analysis},&matches)==SHEET_PRESENCE_OK && !matches);
    WorkbookHistoryStream *history=stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES); WorkbookHistoryStreamState before=workbook_history_stream_state(history);
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total-1,NULL)==PRIMARY_TOPOLOGY_LIMIT && same_state(before,workbook_history_stream_state(history)));
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total,NULL)==PRIMARY_TOPOLOGY_READY && primary_source_topology_reserved(plan));
    WorkbookHistoryStreamState held=workbook_history_stream_state(history); CHECK(held.reserved && held.current.cursor==3 && held.current.tail==5 && held.current.action_high_water==5 && held.current.lineage_high_water==1);
    CHECK(held.owned_bytes-before.owned_bytes==workbook_history_stream_event_allocation(packet->bytes,0));
    CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==PRIMARY_TOPOLOGY_BUSY && same_state(held,workbook_history_stream_state(history)));
    CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_BUSY && history);
    CHECK(!b.dirty && b.count==2 && b.command_revision==0 && worksheet_history_high_water()==order && !b.sheets[1].edit_session_id);
    primary_source_topology_cancel(plan); CHECK(!primary_source_topology_reserved(plan) && same_state(before,workbook_history_stream_state(history)));
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total,NULL)==PRIMARY_TOPOLOGY_READY);
    primary_source_topology_free(&plan); CHECK(!plan && same_state(before,workbook_history_stream_state(history)));
    CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK); analysis_free(&app); workbook_free(&b);
}
static void stale_and_lifetimes(void) {
    for (unsigned mode=0;mode<8;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceTopologyPlan *plan=prepare(c);
        WorkbookHistoryStream *history=stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES); WorkbookHistoryStreamState before=workbook_history_stream_state(history);
        CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==PRIMARY_TOPOLOGY_READY);
        if (mode==0) request++;
        if (mode==1) epoch++;
        if (mode==2) b.command_revision++;
        if (mode==3) b.sheet_high_water++;
        if (mode==4) { char *old=app.path; app.path=dx_strdup(old); free(old); }
        if (mode==5) { char *old=b.sheets[1].name; b.sheets[1].name=dx_strdup(old); free(old); }
        if (mode==6) c.read_only=true;
        if (mode==7) c.ready=false;
        PrimarySourceTopologyStatus expected=mode>=6 ? PRIMARY_TOPOLOGY_PROTECTED : PRIMARY_TOPOLOGY_STALE;
        CHECK(primary_source_topology_check(c,plan,NULL)==expected);
        CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==expected && primary_source_topology_reserved(plan));
        /* Cancellation owns every clone/ticket and need not read destroyed
           borrowed live strings, workbook allocations or old App authority. */
        analysis_free(&app); workbook_free(&b); primary_source_topology_free(&plan);
        CHECK(!plan && same_state(before,workbook_history_stream_state(history)) && workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
    }
}
static void foreign_and_pressure(void) {
    Workbook b=book(); uint64_t request=9,epoch=4; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
    PrimarySourceTopologyPlan *plan=prepare(c);
    for (unsigned mode=0;mode<2;mode++) {
        WorkbookHistoryStream *history=stream(&b,mode==0,mode==1 ? 1 : DX_HISTORY_STREAM_DEFAULT_BYTES); WorkbookHistoryStreamState before=workbook_history_stream_state(history);
        CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==(mode ? PRIMARY_TOPOLOGY_LIMIT : PRIMARY_TOPOLOGY_STALE));
        CHECK(!primary_source_topology_reserved(plan) && same_state(before,workbook_history_stream_state(history)) && primary_source_topology_check(c,plan,NULL)==PRIMARY_TOPOLOGY_READY);
        CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
    }
    primary_source_topology_free(&plan); workbook_free(&b);
    CHECK(!primary_source_topology_packet(NULL) && !primary_source_topology_presence(NULL) && !primary_source_topology_reserved(NULL) && !primary_source_topology_usage(NULL).total);
}
static void catalog_cost_contract(void) {
    SheetLifecycleEntry before[]={{1,"Before",false}},after[]={{1,"After",false}};
    SheetLifecycleState a={{1,2},1,1,before,1},b={{1,2},1,1,after,1}; SheetLifecycleInfo one={0},two={0},pair={0};
    CHECK(sheet_lifecycle_state_measure(a,&one)==SHEET_LIFECYCLE_OK && sheet_lifecycle_state_measure(b,&two)==SHEET_LIFECYCLE_OK);
    CHECK(sheet_lifecycle_measure((SheetLifecycle){a,b},&pair)==SHEET_LIFECYCLE_OK && pair.bytes==DX_SHEET_LIFECYCLE_HEADER_BYTES+one.bytes+two.bytes);
    SheetLifecycleInfo untouched={1,2,3},copy=untouched;
    CHECK(sheet_lifecycle_state_measure((SheetLifecycleState){0},&untouched)==SHEET_LIFECYCLE_INVALID && !memcmp(&untouched,&copy,sizeof(copy)));
    CHECK(sheet_lifecycle_measure((SheetLifecycle){a,a},&untouched)==SHEET_LIFECYCLE_NOOP && !memcmp(&untouched,&copy,sizeof(copy)));
}
int main(void) { prepare_and_reserve(); stale_and_lifetimes(); foreign_and_pressure(); catalog_cost_contract(); printf("PASS: %u prepare-only primary CREATE topology checks\n",checks); return 0; }
