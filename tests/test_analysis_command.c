#include "workbook/analysis_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL analysis command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis a={.kind=SOURCE_CSV,.path="/source"};
    CHECK(workbook_add(&b,"A",&a,NULL) && workbook_add(&b,"B",&a,NULL)); b.dirty=false; return b;
}
static AnalysisTransformTarget target(AnalysisCommandContext *c,size_t index) {
    AnalysisTransformTarget t={0}; CHECK(workbook_prepare_analysis_transform(c,index,&t,NULL)); return t;
}
static Analysis plan(const Analysis *a) { Analysis p=analysis_copy(a); p.unique_rows=!p.unique_rows; return p; }
static void reject(AnalysisCommandContext *c,AnalysisTransformTarget t,Analysis *p,const WorkbookHistoryDefinitionRecorder *recorder,AnalysisTransformStatus status) {
    Workbook before=*c->book; Worksheet sheets[2]={c->book->sheets[0],c->book->sheets[1]}; Analysis current=*c->live,candidate=*p; char *error=NULL;
    AnalysisTransformResult r=workbook_apply_analysis_transform_recorded(c,&t,p,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) &&
        !memcmp(&current,c->live,sizeof(current)) && !memcmp(&candidate,p,sizeof(candidate))); free(error);
}
static void basic(void) {
    Workbook b=book(); uint64_t epoch=10; Analysis live=analysis_copy(&b.sheets[0].analysis);
    AnalysisCommandContext c={&b,&live,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget good=target(&c,0),bad; Analysis p=plan(&live);
#define STALE(field) do { bad=good; bad.field++; reject(&c,bad,&p,NULL,ANALYSIS_TRANSFORM_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.high); STALE(owner.sheet_id); STALE(owner.expected_revision); STALE(owner.expected_document_revision);
    STALE(source_epoch); STALE(format_revision); STALE(history_last); STALE(source_high_water);
#undef STALE
    bad=good; bad.owner.row=1; reject(&c,bad,&p,NULL,ANALYSIS_TRANSFORM_INVALID);
    c.ready=false; reject(&c,good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); c.ready=true;
    c.read_only=true; reject(&c,good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); c.read_only=false;
    p.path[0]='!'; reject(&c,good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); p.path[0]='/';
    p.source_version=1; reject(&c,good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); p.source_version=0;
    p.schema=dx_strdup(""); reject(&c,good,&p,NULL,ANALYSIS_TRANSFORM_PROTECTED); free(p.schema); p.schema=NULL;
    Analysis alias=live; reject(&c,good,&alias,NULL,ANALYSIS_TRANSFORM_INVALID);
    AnalysisTransformTarget untouched=good; CHECK(!workbook_prepare_analysis_transform(&c,2,&untouched,NULL) && !memcmp(&untouched,&good,sizeof(good)));
    CHECK(workbook_reorder(&b,0,1,NULL));
    AnalysisTransformResult r=workbook_apply_analysis_transform_recorded(&c,&good,&p,NULL,NULL);
    CHECK(r.status==ANALYSIS_TRANSFORM_APPLIED && r.before_revision==0 && r.after_revision==1 && !p.path && live.unique_rows && b.dirty && b.command_revision==1 &&
        b.sheets[1].undo_count==1 && b.sheets[1].undo_actions_count==1 && b.sheets[1].undo_actions[0]==EDIT_ANALYSIS && !b.sheets[0].undo_count && !b.sheets[1].analysis.unique_rows);
    /* Real primary authority is independent from the worksheet Save cache. */
    CHECK(worksheet_analysis_history_preflight(b.sheets+1,&live,false,NULL)); worksheet_analysis_history_adopt(b.sheets+1,&live,false); worksheet_transfer_history(b.sheets+1,false);
    b.command_revision++; b.dirty=false; epoch++; good=target(&c,1); p=analysis_copy(&live); p.filter_number=(NumberType){NUM_DECIMAL,38,4};
    CHECK(workbook_apply_analysis_transform_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_NOOP && !p.path && !b.dirty && b.command_revision==2 && b.sheets[1].redo_count==1);
    p=plan(&live); good=target(&c,1); CHECK(workbook_apply_analysis_transform_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !b.sheets[1].redo_count);
    analysis_free(&live); workbook_free(&b);
    /* Embedded authority resolves stable sheet identity after reorder. */
    b=book(); c=(AnalysisCommandContext){&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; good=target(&c,0); p=plan(c.live);
    CHECK(workbook_reorder(&b,0,1,NULL)); c.live=&b.sheets[1].analysis;
    CHECK(workbook_apply_analysis_transform_recorded(&c,&good,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED && b.sheets[1].analysis.unique_rows); workbook_free(&b);
}
typedef struct { AnalysisCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; AnalysisCommandContext *c=p->context; Workbook *b=c->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==1 && r->expected_revision==b->command_revision && r->revision==b->command_revision+1 &&
        r->view.before.primary==c->live && !r->view.before.primary->unique_rows && r->view.after.primary->unique_rows && !b->dirty && !b->sheets[0].undo_count);
    if (p->mode==1) return false;
    if (p->mode==2) return true;
    *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)c->source_epoch)++;
    if (p->mode==4) b->command_revision++;
    if (p->mode==5) return false;
    if (p->mode==6) c->ready=false;
    if (p->mode==7) b->sheets[0].formats.revision++;
    if (p->mode==8) b->sheets[0].history_last++;
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; p->publishes++; Workbook *b=p->context->book;
    CHECK(ticket && p->context->live->unique_rows && b->command_revision==1 && b->dirty && b->sheets[0].undo_count==1 &&
        b->sheets[0].undo_actions_count==1 && !b->sheets[0].undo[0].unique_rows); free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t epoch=1; AnalysisCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
        AnalysisTransformTarget t=target(&c,0); Analysis p=plan(c.live); Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; char *error=NULL;
        AnalysisTransformResult r=workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,&error); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==ANALYSIS_TRANSFORM_APPLIED && probe.publishes==1 && !probe.cancels && !p.path && !error);
        else {
            CHECK(r.status==(mode==1 ? ANALYSIS_TRANSFORM_LIMIT : mode==2 || mode==5 ? ANALYSIS_TRANSFORM_INVALID : mode==6 ? ANALYSIS_TRANSFORM_PROTECTED : ANALYSIS_TRANSFORM_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.path && !c.live->unique_rows && !b.dirty && !b.sheets[0].undo_count && !b.sheets[0].undo_actions_count && error);
        }
        free(error); analysis_free(&p); workbook_free(&b);
    }
}
static void limits_and_sources(void) {
    Workbook b=book(); uint64_t epoch=1; AnalysisCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0); Analysis p=plan(c.live);
    WorkbookHistoryDefinitionRecorder broken={0}; reject(&c,t,&p,&broken,ANALYSIS_TRANSFORM_INVALID); analysis_free(&p);
    b.command_revision=UINT64_MAX; t=target(&c,0); p=plan(c.live); reject(&c,t,&p,NULL,ANALYSIS_TRANSFORM_LIMIT); analysis_free(&p);
    p=analysis_copy(c.live); CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_NOOP); b.command_revision=0;
    c.live->source_version=1; t=target(&c,0); p=plan(c.live); reject(&c,t,&p,NULL,ANALYSIS_TRANSFORM_INVALID); analysis_free(&p); c.live->source_version=0;
    CHECK(analysis_filter_set(c.live,"Amount",FILTER_BETWEEN,"[\"1\",\"2\"]",NULL)); t=target(&c,0); p=plan(c.live); c.validation_reserved=0;
    size_t validation=0; CHECK(workbook_analysis_transform_scratch(b.sheets,c.live,&p,&validation,NULL) && validation>0);
    reject(&c,t,&p,NULL,ANALYSIS_TRANSFORM_LIMIT); c.validation_reserved=validation;
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED);
    workbook_free(&b);
    b=book(); c=(AnalysisCommandContext){&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; c.live->kind=SOURCE_DATABASE;
    CHECK(!workbook_prepare_analysis_transform(&c,0,&t,NULL)); c.live->schema=dx_strdup("main"); c.live->table=dx_strdup("Sales"); t=target(&c,0); p=plan(c.live);
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,NULL,NULL).status==ANALYSIS_TRANSFORM_APPLIED); workbook_free(&b);
}
static void transform_definitions(void) {
    Workbook b=book(); uint64_t epoch=1; AnalysisCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){4,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s); Analysis p=analysis_copy(c.live);
    CHECK(analysis_query_set(&p,"SELECT City, Amount, Value FROM input",NULL));
    CHECK(analysis_filter_set(&p,"City",FILTER_EQUAL,"Ankara",NULL) && analysis_filter_set(&p,"Value",FILTER_EQUAL,"",NULL));
    CHECK(analysis_sort_set(&p,"Amount",true,false,NULL) && analysis_sort_set(&p,"City",false,true,NULL));
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(s,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); DefinitionEvent decoded={0};
    CHECK(definition_event_decode(workbook_history_event_payload(event),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK);
    CHECK(!decoded.before_primary.query && !decoded.before_primary.filter_column && !decoded.before_primary.sort_column &&
        !strcmp(decoded.after_primary.query,"SELECT City, Amount, Value FROM input") && !strcmp(decoded.after_primary.filter_column,"City") &&
        decoded.after_primary.filter_count==1 && decoded.after_primary.filters[0].value && !*decoded.after_primary.filters[0].value &&
        decoded.after_primary.sort_count==1 && decoded.after_primary.descending && !strcmp(decoded.after_primary.sorts[0].column,"City"));
    definition_event_free(&decoded); CHECK(workbook_history_stream_complete(s,&event)==HISTORY_STREAM_OK);
    t=target(&c,0); p=analysis_copy(c.live); p.filter_number=(NumberType){NUM_DECIMAL,38,4}; p.filters[0].number=(NumberType){NUM_I128,0,0};
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_NOOP && b.command_revision==1 && !workbook_history_stream_state(s).pending);
    t=target(&c,0); p=analysis_copy(c.live); CHECK(analysis_filter_move(&p,0,1));
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !strcmp(c.live->filter_column,"Value") && b.command_revision==2);
    t=target(&c,0); p=analysis_copy(c.live); CHECK(analysis_sort_move(&p,0,1));
    CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !strcmp(c.live->sort_column,"City") && b.command_revision==3);
    CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK); workbook_free(&b);
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1; AnalysisCommandContext c={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; target(&c,0);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        AnalysisTransformTarget t=target(&c,0); Analysis p=plan(c.live);
        CHECK(workbook_apply_analysis_transform_recorded(&c,&t,&p,&recorder,NULL).status==ANALYSIS_TRANSFORM_APPLIED && !p.path && b.command_revision==i+1);
        epoch++; if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].undo_count==32 && b.sheets[0].undo_actions_count==32 && workbook_history_stream_state(s).pending==39);
    DefinitionEvent first={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&first)==DEFINITION_EVENT_OK && !first.before_primary.unique_rows && first.after_primary.unique_rows && !strcmp(first.before_primary.path,"/source"));
    definition_event_free(&first); CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK); workbook_free(&b);
    b=book(); c=(AnalysisCommandContext){&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false}; AnalysisTransformTarget t=target(&c,0);
    origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&s)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(s); Analysis p=plan(c.live);
    reject(&c,t,&p,&recorder,ANALYSIS_TRANSFORM_LIMIT); analysis_free(&p); CHECK(!workbook_history_stream_state(s).owned_bytes && !workbook_history_stream_state(s).current.action_high_water);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { basic(); callbacks(); limits_and_sources(); transform_definitions(); stream(); printf("PASS: %u fenced primary Analysis command checks\n",checks); return 0; }
