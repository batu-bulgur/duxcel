#include "workbook/analysis_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL primary replacement:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_DATABASE,.path="/a.duckdb",.schema="main",.table="alpha",.group_column="City",.measure_column="Amount",.aggregate=AGG_SUM};
    CHECK(workbook_add(&b,"A",&a,NULL) && workbook_add(&b,"B",&a,NULL)); b.dirty=false; return b;
}
static AnalysisTransformTarget target(AnalysisCommandContext *c,size_t index) { AnalysisTransformTarget t; CHECK(workbook_prepare_analysis_transform(c,index,&t,NULL)); return t; }
static Analysis proposal(void) { return analysis_copy(&(Analysis){.kind=SOURCE_DATABASE,.path="/b.duckdb",.schema="main",.table="beta"}); }
static void reject(AnalysisCommandContext *c,const AnalysisTransformTarget *t,Analysis *p,const WorkbookHistoryDefinitionRecorder *recorder,AnalysisTransformStatus status) {
    Workbook before=*c->book; Worksheet sheets[2]; memcpy(sheets,c->book->sheets,sizeof(sheets)); Analysis live=*c->live,owned=*p; char *error=NULL;
    AnalysisTransformResult r=workbook_apply_analysis_replacement_recorded(c,t,p,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) && !memcmp(&live,c->live,sizeof(live)) && !memcmp(&owned,p,sizeof(owned))); free(error);
}
static void ownership(void) {
    Workbook b=book(); uint64_t epoch=1; Analysis live=analysis_copy(&b.sheets[0].analysis); AnalysisCommandContext c={&b,&live,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget good=target(&c,0),bad;
    Analysis p=proposal();
#define STALE(field) do { bad=good; bad.field++; reject(&c,&bad,&p,NULL,ANALYSIS_TRANSFORM_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.low); STALE(owner.sheet_id); STALE(source_epoch); STALE(object_high_water); STALE(source_high_water); STALE(live_state.unique_rows); STALE(cached_state.unique_rows); STALE(sources.high_water);
#undef STALE
    c.ready=false; reject(&c,&good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); c.ready=true;
    c.read_only=true; reject(&c,&good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); c.read_only=false;
    Analysis q=analysis_copy(&p); free(q.table); q.table=NULL; reject(&c,&good,&q,NULL,ANALYSIS_TRANSFORM_PROTECTED); analysis_free(&q);
    CHECK(workbook_reorder(&b,0,1,NULL));
    CHECK(workbook_apply_analysis_replacement_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !p.path && !strcmp(live.path,"/b.duckdb") && !strcmp(b.sheets[1].analysis.path,"/a.duckdb") && b.command_revision==1 && b.dirty && b.sheets[1].undo_actions_count==1);
    CHECK(worksheet_analysis_history_preflight(b.sheets+1,&live,false,NULL)); worksheet_analysis_history_adopt(b.sheets+1,&live,false); worksheet_transfer_history(b.sheets+1,false); b.command_revision++; b.dirty=false;
    good=target(&c,1); p=analysis_copy(&live); CHECK(workbook_apply_analysis_replacement_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_NOOP && !p.path && b.sheets[1].redo_actions_count==1 && !b.dirty);
    p=proposal(); epoch=UINT64_MAX; good=target(&c,1); reject(&c,&good,&p,NULL,ANALYSIS_TRANSFORM_LIMIT); epoch=1;
    good=target(&c,1); CHECK(workbook_apply_analysis_replacement_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !b.sheets[1].redo_actions_count);
    analysis_free(&live); workbook_free(&b);
}
typedef struct { AnalysisCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==1 && !strcmp(r->view.before.primary->path,"/a.duckdb") && !strcmp(r->view.after.primary->path,"/b.duckdb") && !b->dirty && !b->command_revision && !b->sheets[0].undo_actions_count);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->source_epoch)++;
    if (p->mode==4) p->context->live->unique_rows=true;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->read_only=true;
    if (p->mode==7) b->sheets[0].analysis.unique_rows=true;
    if (p->mode==8) b->sheets[0].objects.next_id++;
    if (p->mode==9) b->sheets[0].source_versions.count=1; /* Header fence before any row access. */
    if (p->mode==10) ((Analysis *)r->view.after.primary)->unique_rows=true;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->book->command_revision==1 && p->context->book->dirty && !strcmp(p->context->live->path,"/b.duckdb")); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=10;mode++) {
        Workbook b=book(); Analysis live=analysis_copy(&b.sheets[0].analysis); uint64_t epoch=1; AnalysisCommandContext c={&b,&live,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0); Analysis p=proposal();
        Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; char *error=NULL;
        AnalysisTransformResult r=workbook_apply_analysis_replacement_recorded(&c,&t,&p,&recorder,&error); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==ANALYSIS_TRANSFORM_APPLIED && probe.publishes==1 && !probe.cancels && !p.path && !error);
        else CHECK(r.status==(mode==1 ? ANALYSIS_TRANSFORM_LIMIT : mode==2 || mode==5 ? ANALYSIS_TRANSFORM_INVALID : mode==6 ? ANALYSIS_TRANSFORM_PROTECTED : ANALYSIS_TRANSFORM_STALE) && !probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.path && !b.dirty && !b.sheets[0].undo_actions_count && error);
        if (mode==9) b.sheets[0].source_versions.count=0;
        free(error); analysis_free(&p); analysis_free(&live); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1; AnalysisCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        Analysis p=i%2 ? analysis_copy(&(Analysis){.kind=SOURCE_DATABASE,.path="/a.duckdb",.schema="main",.table="alpha"}) : proposal();
        t=target(&c,0); CHECK(workbook_apply_analysis_replacement_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !p.path && b.command_revision==i+1);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].undo_count==32 && b.sheets[0].undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && !strcmp(event.before_primary.path,"/a.duckdb") && !strcmp(event.after_primary.path,"/b.duckdb") && event.before_primary.group_column && !event.after_primary.group_column);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK); workbook_free(&b);
}
static void bindings(void) {
    Workbook b=book(); Worksheet *sheet=b.sheets; uint64_t epoch=1;
    Analysis before={.kind=SOURCE_CSV,.path="/a.csv",.source_version=1},after={.kind=SOURCE_CSV,.path="/b.csv",.source_version=2};
    analysis_free(&sheet->analysis); sheet->analysis=analysis_copy(&before);
    for (uint64_t version=1;version<=2;version++) {
        SourceVersions next={0}; SourceVersion row={.version=version,.kind=SOURCE_CSV,.path=version==1 ? "/a.csv" : "/b.csv"};
        CHECK(source_versions_plan(&sheet->source_versions,&row,true,&next,NULL)); source_versions_free(&sheet->source_versions); sheet->source_versions=next;
    }
    AnalysisCommandContext c={&b,&sheet->analysis,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){4,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); Analysis owned=analysis_copy(&after);
    CHECK(workbook_apply_analysis_replacement_recorded(&c,&t,&owned,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !owned.path && sheet->source_versions.count==2 && sheet->source_versions.high_water==2);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(queue,&event)==HISTORY_STREAM_OK); DefinitionEvent decoded={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(event);
    CHECK(definition_event_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && decoded.sources.versions.count==2 && decoded.before_primary.source_version==1 && decoded.after_primary.source_version==2);
    definition_event_free(&decoded); CHECK(workbook_history_stream_complete(queue,&event)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK);
    t=target(&c,0); owned=analysis_copy(&after); owned.source_version=1; reject(&c,&t,&owned,NULL,ANALYSIS_TRANSFORM_INVALID); analysis_free(&owned);
    owned=analysis_copy(&after); owned.source_version=0;
    CHECK(workbook_apply_analysis_replacement_recorded(&c,&t,&owned,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !owned.path && sheet->analysis.source_version==0 && sheet->source_versions.count==2);
    workbook_free(&b);
}
int main(void) { ownership(); callbacks(); stream(); bindings(); printf("PASS: %u admitted primary replacement checks\n",checks); return 0; }
