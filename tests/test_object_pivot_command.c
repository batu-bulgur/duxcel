#include "workbook/object_pivot_command.h"
#include "workbook/definition_state.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object Pivot:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"}; PivotDefinition p={0};
    CHECK(workbook_add(&b,"Sheet",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,0,0,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(pivot_assign(&p,PIVOT_ROWS,"Date",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_COUNT,NULL));
    CHECK(objects_add_chart(&s->objects,0,10,"Chart",&p,NULL)==1); pivot_free(&p); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_chart_measure(&s->objects,1,1,NULL)); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectCommandTarget target(ObjectCommandContext *c) { ObjectCommandTarget t; CHECK(workbook_prepare_object_fence(c,0,c->book->sheets[0].objects.items[1].id,&t,NULL)); return t; }
static ObjectPivotResult apply(ObjectCommandContext *c,const PivotDefinition *p,const WorkbookHistoryDefinitionRecorder *recorder) {
    ObjectCommandTarget t=target(c); return workbook_apply_object_pivot_recorded(c,&t,p,recorder,NULL);
}
static void definitions(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false};
    uint64_t query=s->objects.items[1].revision,allocator=s->objects.next_id,titles=s->objects.title_revision;
    PivotDefinition p=pivot_copy(&s->objects.items[1].pivot); p.fields[PIVOT_VALUES][0].number=(NumberType){NUM_DECIMAL,38,4}; ObjectPivotPlanInfo info={.owned=99};
    CHECK(objects_pivot_measure(&s->objects,s->objects.items[1].id,&p,&info,NULL)==OBJECT_PIVOT_NOOP && info.owned==99);
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_NOOP && !b.dirty && !b.command_revision);
    CHECK(pivot_move(&p,PIVOT_VALUES,1,PIVOT_VALUES,0,NULL));
    CHECK(objects_pivot_measure(&s->objects,s->objects.items[1].id,&p,&info,NULL)==OBJECT_PIVOT_READY && info.proposal_owned>=sizeof(p));
    c.plan_reserved=info.owned+info.validation_scratch-1; CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_LIMIT && !b.dirty); c.plan_reserved++;
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_APPLIED && b.command_revision==1 && s->objects.items[1].chart_measure==0 && s->objects.items[1].revision==query+1 && s->objects.next_id==allocator && s->objects.title_revision==titles && !s->objects.marks_revision);
    c.plan_reserved=SIZE_MAX;
    CHECK(definition_pivot_equal(&s->objects.items[1].pivot,&p) && s->objects.items[1].pivot.fields[0][0].name!=p.fields[0][0].name);
    p.fields[PIVOT_VALUES][0].aggregate=AGG_AVG;
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_APPLIED && !s->objects.items[1].chart_measure);
    pivot_remove(&p,PIVOT_VALUES,0); CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_APPLIED && !s->objects.items[1].chart_measure);
    CHECK(pivot_set_grain(&p,PIVOT_ROWS,0,PIVOT_DATE_MONTH,NULL));
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Amount",AGG_SUM,NULL)); p.fields[PIVOT_FILTERS][0].filter=FILTER_IN; p.fields[PIVOT_FILTERS][0].value=dx_strdup("[\"10\",null,\"\"]");
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_APPLIED && s->objects.items[1].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_MONTH && !strcmp(s->objects.items[1].pivot.fields[PIVOT_FILTERS][0].value,"[\"10\",null,\"\"]"));
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false; pivot_free(&p); p=pivot_copy(&s->objects.items[1].pivot); c.plan_reserved=0;
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_NOOP && s->redo_actions_count==1 && !b.dirty); c.plan_reserved=SIZE_MAX;
    p.fields[PIVOT_VALUES][0].aggregate=AGG_MAX;
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_APPLIED && !s->redo_actions_count); pivot_free(&p);
    uint64_t revision=s->objects.items[1].revision; s->objects.items[1].revision=UINT64_MAX; p=pivot_copy(&s->objects.items[1].pivot);
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_NOOP); p.fields[PIVOT_VALUES][0].aggregate=AGG_MIN; CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_LIMIT); s->objects.items[1].revision=revision;
    p.fields[PIVOT_VALUES][1]=p.fields[PIVOT_VALUES][0]; p.fields[PIVOT_VALUES][1].name=dx_strdup(p.fields[PIVOT_VALUES][0].name); p.counts[PIVOT_VALUES]=2;
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_INVALID); pivot_remove(&p,PIVOT_VALUES,1);
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Amount",AGG_SUM,NULL)); p.fields[PIVOT_FILTERS][0].filter=FILTER_BETWEEN; p.fields[PIVOT_FILTERS][0].value=dx_strdup("[\"only-one\"]");
    CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_INVALID); pivot_free(&p); p=pivot_copy(&s->objects.items[1].pivot);
    free(p.fields[PIVOT_ROWS][0].name); p.fields[PIVOT_ROWS][0].name=dx_strdup("bad\xc0\x80"); CHECK(apply(&c,&p,NULL).status==OBJECT_PIVOT_INVALID); pivot_free(&p);
    CHECK(apply(&c,NULL,NULL).status==OBJECT_PIVOT_INVALID); workbook_free(&b);
}
typedef struct { ObjectCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.before.objects->items[1].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_SUM && r->view.after.objects->items[1].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_AVG && !b->dirty && !b->command_revision && b->sheets[0].undo_actions_count==4);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->request_epoch)++;
    if (p->mode==4) b->command_revision++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->read_only=true;
    if (p->mode==7) b->sheets[0].objects.items[0].rows++;
    if (p->mode==8) b->sheets[0].objects.title_revision++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->book->command_revision==1 && p->context->book->dirty && p->context->book->sheets[0].undo_actions_count==5); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; PivotDefinition p=pivot_copy(&b.sheets[0].objects.items[1].pivot); p.fields[PIVOT_VALUES][0].aggregate=AGG_AVG;
        Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; ObjectPivotResult r=apply(&c,&p,&recorder); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_PIVOT_APPLIED && probe.publishes==1 && !probe.cancels);
        else CHECK(r.status==(mode==1 ? OBJECT_PIVOT_LIMIT : mode==2 || mode==5 ? OBJECT_PIVOT_INVALID : mode==6 ? OBJECT_PIVOT_PROTECTED : OBJECT_PIVOT_STALE) && !probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].undo_actions_count==4 && b.sheets[0].objects.items[1].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_SUM);
        pivot_free(&p); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL; uint64_t query=b.sheets[0].objects.items[1].revision;
    for (unsigned i=0;i<40;i++) {
        PivotDefinition p=pivot_copy(&b.sheets[0].objects.items[1].pivot); p.fields[PIVOT_VALUES][0].aggregate=i%2 ? AGG_SUM : AGG_AVG;
        CHECK(apply(&c,&p,&recorder).status==OBJECT_PIVOT_APPLIED && b.command_revision==i+1 && b.sheets[0].objects.items[1].revision==query+i+1); pivot_free(&p);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.before_objects.objects.items[1].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_SUM && event.after_objects.objects.items[1].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_AVG);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK);
    t=target(&c); b.command_revision=UINT64_MAX; t.owner.expected_document_revision=UINT64_MAX; PivotDefinition p=pivot_copy(&b.sheets[0].objects.items[1].pivot); p.fields[PIVOT_VALUES][0].aggregate=AGG_AVG;
    CHECK(workbook_apply_object_pivot_recorded(&c,&t,&p,NULL,NULL).status==OBJECT_PIVOT_LIMIT); pivot_free(&p); workbook_free(&b);
}
int main(void) { definitions(); callbacks(); stream(); printf("PASS: %u admitted Pivot definition checks\n",checks); return 0; }
