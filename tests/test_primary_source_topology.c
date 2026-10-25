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
    Column columns[]={{.name="City",.type=DUCKDB_TYPE_VARCHAR},{.name="When",.type=DUCKDB_TYPE_TIMESTAMP},{.name="Amount",.type=DUCKDB_TYPE_DECIMAL}};
    SheetSourceView *initial=NULL; CHECK(sheet_source_view_build((SheetSourceViewSpec){columns,3,{.canvas_zoom=1.5f}},SIZE_MAX,&initial)==SHEET_SOURCE_VIEW_OK);
    PrimarySourceInitialSchema schema={*c.request,*c.primary_epoch,primary_source_draft_source(draft),columns,3,NULL};
    PrimarySourcePublishPlan *plan=NULL; CHECK(primary_source_publish_prepare_initial(c,&draft,&initial,schema,SIZE_MAX,&plan,NULL)==PRIMARY_PUBLISH_READY); return plan;
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
    CHECK(primary_source_publish_owned(candidate)>0 && primary_source_publish_owned(candidate)<usage.source);
    Workbook before=*c.book; Worksheet sheet=c.book->sheets[0]; PrimarySourceTopologyPlan *out=NULL;
    CHECK(primary_source_topology_prepare(c,&candidate,usage.total-1,&out,NULL)==PRIMARY_TOPOLOGY_LIMIT && candidate && !out);
    CHECK(!memcmp(&before,c.book,sizeof(before)) && !memcmp(&sheet,c.book->sheets,sizeof(sheet)));
    CHECK(primary_source_topology_prepare(c,&candidate,usage.total,&out,NULL)==PRIMARY_TOPOLOGY_READY && !candidate);
    CHECK(primary_source_topology_owned(out)>usage.owned && primary_source_topology_owned(out)<usage.total);
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
    CHECK(proposed.sheet->id==3 && !proposed.sheet->edit_session_id && proposed.sheet->view.column_count==52 && b.count==2 && b.sheet_high_water==2 && worksheet_history_high_water()==order);
    CHECK(proposed.sheet->view.widths[0]==128 && proposed.sheet->view.widths[1]==190 && proposed.sheet->view.widths[51]==96 && proposed.sheet->view.canvas_zoom==1.5f);
    Worksheet changed=*proposed.sheet; float widths[52]; memcpy(widths,changed.view.widths,sizeof(widths)); widths[1]=128; changed.view.widths=widths;
    bool matches=true; CHECK(sheet_presence_matches(primary_source_topology_presence(plan),(SheetPresenceView){b.id,3,&changed,&changed.analysis},&matches)==SHEET_PRESENCE_OK && !matches);
    WorkbookHistoryStream *history=stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES); WorkbookHistoryStreamState before=workbook_history_stream_state(history);
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total-1,NULL)==PRIMARY_TOPOLOGY_LIMIT && same_state(before,workbook_history_stream_state(history)));
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total,NULL)==PRIMARY_TOPOLOGY_READY && primary_source_topology_reserved(plan));
    WorkbookHistoryStreamState held=workbook_history_stream_state(history); CHECK(held.reserved && held.current.cursor==3 && held.current.tail==5 && held.current.action_high_water==5 && held.current.lineage_high_water==1);
    CHECK(held.owned_bytes-before.owned_bytes==workbook_history_stream_event_allocation(packet->bytes,0));
    size_t resident_with_ticket=primary_source_topology_owned(plan);
    CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==PRIMARY_TOPOLOGY_BUSY && same_state(held,workbook_history_stream_state(history)));
    CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_BUSY && history);
    CHECK(!b.dirty && b.count==2 && b.command_revision==0 && worksheet_history_high_water()==order && !b.sheets[1].edit_session_id);
    primary_source_topology_cancel(plan); CHECK(primary_source_topology_owned(plan)+workbook_history_topology_ticket_allocation()==resident_with_ticket); CHECK(!primary_source_topology_reserved(plan) && same_state(before,workbook_history_stream_state(history)));
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
static void provisional_view_rejected(void) {
    Workbook b=book(); uint64_t request=9,epoch=4; PrimarySourceDraftContext c={&b,&b.sheets[0].analysis,&request,&epoch,true,false};
    Analysis proposal={.kind=SOURCE_DATABASE,.path="/new.duckdb"}; PrimarySourceDraft *draft=NULL;
    CHECK(primary_source_draft_begin(c,&proposal,"New",SIZE_MAX,&draft,NULL)==PRIMARY_SOURCE_READY); proposal.schema="main"; proposal.table="Sales";
    CHECK(primary_source_draft_resolve(c,draft,(PrimarySourceDraftReply){request,&proposal,NULL},SIZE_MAX,NULL)==PRIMARY_SOURCE_RESOLVED);
    PrimarySourcePublishPlan *candidate=NULL; CHECK(primary_source_publish_prepare(c,&draft,SIZE_MAX,&candidate,NULL)==PRIMARY_PUBLISH_READY);
    CHECK(!primary_source_publish_initial_view(candidate)); PrimarySourceTopologyUsage usage={0},unchanged=usage; PrimarySourceTopologyPlan *plan=NULL; Workbook before=b;
    CHECK(primary_source_topology_measure(c,candidate,&usage,NULL)==PRIMARY_TOPOLOGY_PROTECTED && !memcmp(&usage,&unchanged,sizeof(usage)));
    CHECK(primary_source_topology_prepare(c,&candidate,SIZE_MAX,&plan,NULL)==PRIMARY_TOPOLOGY_PROTECTED && candidate && !plan && !memcmp(&before,&b,sizeof(b)));
    primary_source_publish_cancel(&candidate); workbook_free(&b);
}
static void actual_create_adoption(bool retained,bool reserved_first,bool abandon) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceTopologyPlan *plan=prepare(c);
    const SheetTopologyPacket *packet=primary_source_topology_packet(plan); size_t bytes=packet->bytes; unsigned char *expected=malloc(bytes); memcpy(expected,packet->data,bytes);
    WorkbookHistoryStream *history=retained ? stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES) : NULL;
    if (reserved_first) CHECK(primary_source_topology_reserve(c,plan,history,SIZE_MAX,NULL)==PRIMARY_TOPOLOGY_READY);
    PrimarySourcePublication out={0}; uint64_t order=worksheet_history_high_water(),revision=b.command_revision;
    CHECK(primary_source_topology_apply(c,&plan,history,SIZE_MAX,&out,NULL)==PRIMARY_TOPOLOGY_APPLIED && !plan);
    CHECK(b.count==3 && b.active==2 && b.sheet_high_water==3 && b.command_revision==revision+1 && worksheet_history_high_water()==order+1 && out.sheet==3);
    CHECK(b.sheets[2].view.column_count==52 && b.sheets[2].view.widths[0]==128 && b.sheets[2].view.widths[1]==190 && b.sheets[2].view.canvas_zoom==1.5f && !b.sheets[2].width_revision);
    size_t resident=0; CHECK(workbook_lifecycle_format_usage(&b,&resident)==FORMAT_BUDGET_OK && resident==bytes);
    Workbook *saved=workbook_snapshot(&b); CHECK(saved && !saved->lifecycle); size_t saved_bytes=1;
    CHECK(workbook_lifecycle_format_usage(saved,&saved_bytes)==FORMAT_BUDGET_OK && !saved_bytes); workbook_free(saved); free(saved);
    if (retained) {
        WorkbookHistoryStreamState state=workbook_history_stream_state(history);
        CHECK(!state.reserved && state.pending==1 && state.current.command_revision==revision+1 && state.current.cursor==4 && state.current.tail==4 && state.current.action_high_water==6 && state.current.lineage==2);
        WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(history,&event)==HISTORY_STREAM_OK && event);
        const WorkbookHistoryEventInfo *info=workbook_history_event_info(event);
        CHECK(info->family==5 && info->encoding==2 && info->bytes==bytes && info->revision==revision+1 && info->forked);
        CHECK(!memcmp(workbook_history_event_payload(event),expected,bytes));
        CHECK(workbook_history_stream_complete(history,&event)==HISTORY_STREAM_OK && !event);
    }
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==2 && b.sheet_high_water==3);
    CHECK(workbook_lifecycle_format_usage(&b,&resident)==FORMAT_BUDGET_OK && resident==bytes);
    if (abandon) {
        CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name="Branch"},NULL,NULL));
        CHECK(workbook_lifecycle_format_usage(&b,&resident)==FORMAT_BUDGET_OK && !resident && b.count==2 && b.sheet_high_water==3);
    } else {
        CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && b.count==3 && b.sheets[b.active].id==3 && b.sheets[b.active].view.widths[1]==190);
        CHECK(workbook_lifecycle_format_usage(&b,&resident)==FORMAT_BUDGET_OK && resident==bytes);
    }
    free(expected); primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
    if (retained) CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
}
static void apply_failure_preservation(void) {
    for (unsigned mode=0;mode<5;mode++) {
        Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
        PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceTopologyPlan *plan=prepare(c);
        WorkbookHistoryStream *history=stream(&b,mode==1,mode==2 ? 1 : DX_HISTORY_STREAM_DEFAULT_BYTES); WorkbookHistoryStreamState state=workbook_history_stream_state(history);
        PrimarySourcePublication out={0}; if (mode==3) out.candidate.source.query=dx_strdup("occupied"); if (mode==4) epoch++;
        Workbook before=b; Worksheet sheet=b.sheets[0]; uint64_t order=worksheet_history_high_water();
        size_t budget=mode==0 ? primary_source_topology_usage(plan).total-1 : SIZE_MAX;
        PrimarySourceTopologyStatus expected=mode==0 || mode==2 ? PRIMARY_TOPOLOGY_LIMIT : mode==3 ? PRIMARY_TOPOLOGY_INVALID : PRIMARY_TOPOLOGY_STALE;
        CHECK(primary_source_topology_apply(c,&plan,history,budget,&out,NULL)==expected && plan);
        CHECK(!memcmp(&b,&before,sizeof(b)) && !memcmp(b.sheets,&sheet,sizeof(sheet)) && worksheet_history_high_water()==order && !out.sheet);
        CHECK(same_state(state,workbook_history_stream_state(history)) && !primary_source_topology_reserved(plan));
        primary_source_publication_free(&out); primary_source_topology_free(&plan); CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
        analysis_free(&app); workbook_free(&b);
    }
}
static void held_ticket_pressure_retry(void) {
    Workbook b=book(); Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceTopologyPlan *plan=prepare(c);
    const SheetTopologyPacket *packet=primary_source_topology_packet(plan);
    const SheetPresenceAsset *presence=primary_source_topology_presence(plan);
    const void *payload=packet->data; size_t resident=primary_source_topology_owned(plan);
    PrimarySourceTopologyUsage usage=primary_source_topology_usage(plan);
    WorkbookHistoryStream *history=stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES);
    WorkbookHistoryStreamState before=workbook_history_stream_state(history);
    CHECK(primary_source_topology_reserve(c,plan,history,usage.total,NULL)==PRIMARY_TOPOLOGY_READY);
    WorkbookHistoryStreamState held=workbook_history_stream_state(history);
    Workbook unchanged=b; Worksheet old=b.sheets[0]; Analysis primary=app; uint64_t order=worksheet_history_high_water();
    PrimarySourcePublication out={0};
    CHECK(primary_source_topology_apply(c,&plan,history,usage.total-1,&out,NULL)==PRIMARY_TOPOLOGY_LIMIT);
    CHECK(primary_source_topology_apply(c,&plan,NULL,usage.total,&out,NULL)==PRIMARY_TOPOLOGY_STALE);
    CHECK(primary_source_topology_reserved(plan) && same_state(held,workbook_history_stream_state(history)));
    CHECK(primary_source_topology_packet(plan)==packet && packet->data==payload && primary_source_topology_presence(plan)==presence);
    CHECK(!memcmp(&unchanged,&b,sizeof(b)) && !memcmp(&old,b.sheets,sizeof(old)) && !memcmp(&primary,&app,sizeof(app)));
    CHECK(!out.sheet && !out.presence && !out.origin && worksheet_history_high_water()==order && request==9 && epoch==4);
    /* A live global resident can prevent adoption after preparation. Releasing
       that pressure must not require a fresh source preimage or new reply. */
    size_t bytes=b.sheets[0].source_versions.bytes; b.sheets[0].source_versions.bytes=SIZE_MAX;
    CHECK(primary_source_topology_apply(c,&plan,history,usage.total,&out,NULL)==PRIMARY_TOPOLOGY_LIMIT);
    CHECK(primary_source_topology_reserved(plan) && same_state(held,workbook_history_stream_state(history)) && !out.sheet);
    b.sheets[0].source_versions.bytes=bytes;
    CHECK(primary_source_topology_check(c,plan,NULL)==PRIMARY_TOPOLOGY_READY);
    primary_source_topology_cancel(plan);
    CHECK(same_state(before,workbook_history_stream_state(history)) && primary_source_topology_owned(plan)==resident);
    CHECK(primary_source_topology_packet(plan)==packet && packet->data==payload && primary_source_topology_presence(plan)==presence);
    CHECK(primary_source_topology_apply(c,&plan,NULL,usage.total,&out,NULL)==PRIMARY_TOPOLOGY_APPLIED && !plan);
    CHECK(b.count==3 && b.command_revision==unchanged.command_revision+1 && worksheet_history_high_water()==order+1 && out.sheet==3);
    CHECK(same_state(before,workbook_history_stream_state(history)));
    size_t packet_bytes=0; CHECK(workbook_lifecycle_format_usage(&b,&packet_bytes)==FORMAT_BUDGET_OK && packet_bytes==usage.event_bytes);
    CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
    primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
}
static void final_valid_revision(bool retained) {
    Workbook b=book(); b.command_revision=UINT64_MAX-1;
    Analysis app=analysis_copy(&b.sheets[0].analysis); uint64_t request=9,epoch=4;
    PrimarySourceDraftContext c={&b,&app,&request,&epoch,true,false}; PrimarySourceTopologyPlan *plan=prepare(c);
    WorkbookHistoryStream *history=retained ? stream(&b,false,DX_HISTORY_STREAM_DEFAULT_BYTES) : NULL;
    size_t bytes=primary_source_topology_packet(plan)->bytes; uint64_t order=worksheet_history_high_water(); PrimarySourcePublication out={0};
    CHECK(primary_source_topology_apply(c,&plan,history,SIZE_MAX,&out,NULL)==PRIMARY_TOPOLOGY_APPLIED && !plan);
    CHECK(b.command_revision==UINT64_MAX && b.count==3 && out.sheet==3 && worksheet_history_high_water()==order+1);
    WorkbookHistoryResult action=workbook_history_peek(&b,false,NULL);
    CHECK(action.status==WORKBOOK_HISTORY_LIMIT && action.lifecycle && action.sheet_id==out.sheet && action.order==order+1);
    size_t resident=0; CHECK(workbook_lifecycle_format_usage(&b,&resident)==FORMAT_BUDGET_OK && resident==bytes);
    Workbook before=b; CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_LIMIT && !memcmp(&b,&before,sizeof(b)));
    if (retained) {
        WorkbookHistoryStreamState state=workbook_history_stream_state(history);
        CHECK(state.current.command_revision==UINT64_MAX && state.pending==1 && !state.reserved);
        WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(history,&event)==HISTORY_STREAM_OK && event);
        CHECK(workbook_history_event_info(event)->revision==UINT64_MAX && workbook_history_event_info(event)->bytes==bytes);
        CHECK(workbook_history_stream_complete(history,&event)==HISTORY_STREAM_OK && !event);
        CHECK(workbook_history_stream_free(&history,false)==HISTORY_STREAM_OK);
    }
    primary_source_publication_free(&out); analysis_free(&app); workbook_free(&b);
}
int main(void) { prepare_and_reserve(); stale_and_lifetimes(); foreign_and_pressure(); catalog_cost_contract(); provisional_view_rejected(); actual_create_adoption(false,false,false); actual_create_adoption(true,false,false); actual_create_adoption(true,true,true); apply_failure_preservation(); held_ticket_pressure_retry(); final_valid_revision(false); final_valid_revision(true); printf("PASS: %u owned primary CREATE topology checks\n",checks); return 0; }
