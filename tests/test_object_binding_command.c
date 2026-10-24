#include "workbook/object_binding_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object binding:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis primary={.kind=SOURCE_CSV,.path="/primary.csv",.source_version=3},a={.kind=SOURCE_CSV,.path="/a.csv",.source_version=1},other={.kind=SOURCE_CSV,.path="/b.csv",.source_version=2};
    Analysis unbound=primary; unbound.source_version=0;
    CHECK(workbook_add(&b,"A",&primary,NULL) && workbook_add(&b,"B",&unbound,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,10,2,"Raw A",&a,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_table(&s->objects,30,2,"Raw B",&other,NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    PivotDefinition p={0}; CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    CHECK(objects_add_chart(&s->objects,10,20,"Chart",&p,NULL)==2); pivot_free(&p); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,2,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    for (uint64_t version=1;version<=3;version++) { SourceVersions next={0}; SourceVersion row={.owner=version==3 ? 0 : version,.version=version,.kind=SOURCE_CSV,.path=version==1 ? "/a.csv" : version==2 ? "/b.csv" : "/primary.csv"}; CHECK(source_versions_plan(&s->source_versions,&row,true,&next,NULL)); source_versions_free(&s->source_versions); s->source_versions=next; }
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectBindingTarget target(ObjectBindingContext *c) { ObjectBindingTarget t; CHECK(workbook_prepare_object_binding(c,0,c->command.book->sheets[0].objects.items[2].id,&t,NULL)); return t; }
static ObjectBindingResult apply(ObjectBindingContext *c,uint64_t source,const WorkbookHistoryDefinitionRecorder *recorder) { ObjectBindingTarget t=target(c); return workbook_apply_object_binding_recorded(c,&t,source,recorder,NULL); }
static void bindings(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t request=1,epoch=4;
    ObjectBindingContext c={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&s->analysis,&epoch}; ObjectBindingTarget t=target(&c);
    uint64_t query=s->objects.items[2].revision,next=s->objects.next_id,titles=s->objects.title_revision,marks=s->objects.marks_revision;
    ObjectBindingPlanInfo info={.owned=99}; CHECK(objects_binding_measure(&s->objects,t.object.object,s->objects.items[0].id,c.primary,&info,NULL)==OBJECT_BINDING_NOOP && info.owned==99);
    CHECK(apply(&c,s->objects.items[2].id,NULL).status==OBJECT_BINDING_INVALID && !b.dirty);
    CHECK(apply(&c,UINT64_MAX,NULL).status==OBJECT_BINDING_INVALID && !b.dirty);
    CHECK(objects_binding_measure(&s->objects,t.object.object,s->objects.items[1].id,c.primary,&info,NULL)==OBJECT_BINDING_READY);
    c.command.plan_reserved=info.owned+info.validation_scratch-1; CHECK(apply(&c,s->objects.items[1].id,NULL).status==OBJECT_BINDING_LIMIT && !b.dirty); c.command.plan_reserved++;
    CHECK(workbook_reorder(&b,0,1,NULL)); c.primary=&b.sheets[1].analysis;
    CHECK(workbook_apply_object_binding_recorded(&c,&t,b.sheets[1].objects.items[1].id,NULL,NULL).status==OBJECT_BINDING_APPLIED && b.command_revision==1);
    s=b.sheets+1; CHECK(s->objects.items[2].source_id==s->objects.items[1].id && s->objects.items[2].revision==query+1 && s->objects.next_id==next && s->objects.title_revision==titles && s->objects.marks_revision==marks && !strcmp(s->objects.items[2].name,"Chart") && s->objects.items[2].row==10 && s->objects.items[2].col==20);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(workbook_prepare_object_binding(&c,1,s->objects.items[2].id,&t,NULL)); c.command.plan_reserved=0;
    CHECK(workbook_apply_object_binding_recorded(&c,&t,s->objects.items[0].id,NULL,NULL).status==OBJECT_BINDING_NOOP && s->redo_actions_count==1 && !b.dirty);
    c.command.plan_reserved=SIZE_MAX; CHECK(workbook_apply_object_binding_recorded(&c,&t,0,NULL,NULL).status==OBJECT_BINDING_APPLIED && !s->redo_actions_count && s->objects.items[2].source_id==0 && s->source_versions.count==3);
    CHECK(workbook_prepare_object_binding(&c,1,s->objects.items[2].id,&t,NULL)); s->objects.items[2].revision=UINT64_MAX;
    CHECK(workbook_prepare_object_binding(&c,1,s->objects.items[2].id,&t,NULL)); CHECK(workbook_apply_object_binding_recorded(&c,&t,s->objects.items[0].id,NULL,NULL).status==OBJECT_BINDING_LIMIT);
    CHECK(workbook_apply_object_binding_recorded(&c,&t,0,NULL,NULL).status==OBJECT_BINDING_NOOP); workbook_free(&b);
    b=book(); s=b.sheets; c.command.book=&b; Analysis none={0}; c.primary=&none;
    CHECK(apply(&c,0,NULL).status==OBJECT_BINDING_PROTECTED && !b.dirty); c.primary=&s->analysis; t=target(&c); c.command.read_only=true;
    CHECK(workbook_apply_object_binding_recorded(&c,&t,s->objects.items[1].id,NULL,NULL).status==OBJECT_BINDING_PROTECTED); workbook_free(&b);
}
typedef struct { ObjectBindingContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->command.book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.before.objects->items[2].source_id==1 && r->view.after.objects->items[2].source_id==2 && !b->dirty && !b->command_revision && b->sheets[0].undo_actions_count==4);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->primary_epoch)++;
    if (p->mode==4) p->context->command.book->sheets[0].analysis.unique_rows=true;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->command.read_only=true;
    if (p->mode==7) (*(uint64_t *)p->context->command.request_epoch)++;
    if (p->mode==8) b->sheets[0].objects.items[1].rows++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->command.book->command_revision==1 && p->context->command.book->dirty && p->context->command.book->sheets[0].objects.items[2].source_id==2); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t request=1,epoch=1; ObjectBindingContext c={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&b.sheets[0].analysis,&epoch};
        Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; ObjectBindingResult r=apply(&c,2,&recorder); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_BINDING_APPLIED && probe.publishes==1 && !probe.cancels);
        else CHECK(r.status==(mode==1 ? OBJECT_BINDING_LIMIT : mode==2 || mode==5 ? OBJECT_BINDING_INVALID : mode==6 ? OBJECT_BINDING_PROTECTED : OBJECT_BINDING_STALE) && !probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].objects.items[2].source_id==1 && b.sheets[0].undo_actions_count==4);
        workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t request=1,epoch=1; ObjectBindingContext c={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&b.sheets[0].analysis,&epoch}; ObjectBindingTarget t=target(&c);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL; uint64_t query=b.sheets[0].objects.items[2].revision;
    for (unsigned i=0;i<40;i++) { t=target(&c); CHECK(workbook_apply_object_binding_recorded(&c,&t,i%2 ? 1 : 2,&recorder,NULL).status==OBJECT_BINDING_APPLIED && b.command_revision==i+1 && b.sheets[0].objects.items[2].revision==query+i+1); if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32);
    DefinitionEvent decoded={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&decoded)==DEFINITION_EVENT_OK && decoded.before_objects.objects.items[2].source_id==1 && decoded.after_objects.objects.items[2].source_id==2 && decoded.sources.versions.count==2);
    definition_event_free(&decoded); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { bindings(); callbacks(); stream(); printf("PASS: %u admitted object binding checks\n",checks); return 0; }
