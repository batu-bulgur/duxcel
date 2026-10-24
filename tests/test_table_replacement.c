#include "workbook/table_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Table replacement:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_DATABASE,.path="/a.duckdb",.schema="main",.table="alpha"};
    CHECK(workbook_add(&b,"Sheet",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,20,"Pivot",NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static TableTransformTarget target(TableCommandContext *c) { TableTransformTarget t; CHECK(workbook_prepare_table_transform(c,0,c->book->sheets[0].objects.items[0].id,&t,NULL)); return t; }
static TableTransformResult apply(TableCommandContext *c,const Analysis *p,const WorkbookHistoryDefinitionRecorder *recorder) {
    TableTransformTarget t=target(c); return workbook_apply_table_replacement_recorded(c,&t,p,recorder,NULL);
}
static void plans(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false};
    Analysis p={.kind=SOURCE_DATABASE,.path="/b.duckdb",.schema="main",.table="beta"}; ObjectSourcePlanInfo info={.owned=99}; ObjectSnapshot out={0};
    CHECK(objects_source_transform_measure(&s->objects,s->objects.items[0].id,&p,&info)==OBJECT_SOURCE_PLAN_PROTECTED && info.owned==99);
    CHECK(objects_source_replacement_measure(&s->objects,s->objects.items[0].id,&p,&info)==OBJECT_SOURCE_PLAN_OK && info.result_changed);
    CHECK(objects_plan_source_replacement(&s->objects,s->objects.items[0].id,&p,info.owned+info.validation_scratch-1,&out,&info)==OBJECT_SOURCE_PLAN_LIMIT && !out.items);
    uint64_t query=s->objects.items[0].revision,dependent=s->objects.items[1].revision,next=s->objects.next_id;
    CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_APPLIED && b.command_revision==1 && b.dirty && !strcmp(s->objects.items[0].source.path,p.path) && !strcmp(s->objects.items[0].source.table,"beta") && s->objects.items[0].source.path!=p.path && s->objects.items[0].revision==query+1 && s->objects.items[1].revision==dependent && s->objects.next_id==next);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(!strcmp(s->objects.items[0].source.table,"alpha")); c.plan_reserved=0; CHECK(apply(&c,&s->objects.items[0].source,NULL).status==TABLE_TRANSFORM_NOOP && s->redo_actions_count==1 && !b.dirty); c.plan_reserved=SIZE_MAX;
    Analysis unresolved=p; unresolved.table=NULL; CHECK(apply(&c,&unresolved,NULL).status==TABLE_TRANSFORM_PROTECTED && s->redo_actions_count==1);
    s->objects.items[0].revision=UINT64_MAX; CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_LIMIT); CHECK(apply(&c,&s->objects.items[0].source,NULL).status==TABLE_TRANSFORM_NOOP); s->objects.items[0].revision=query;
    b.command_revision=UINT64_MAX; CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_LIMIT); b.command_revision=2;
    CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_APPLIED && !s->redo_actions_count);
    free(s->objects.items[0].source.table); s->objects.items[0].source.table=NULL; TableTransformTarget t={0}; t.object=99;
    CHECK(!workbook_prepare_table_transform(&c,0,s->objects.items[0].id,&t,NULL) && t.object==99); workbook_free(&b);
}
typedef struct { TableCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && !strcmp(r->view.before.objects->items[0].source.path,"/a.duckdb") && !strcmp(r->view.after.objects->items[0].source.path,"/b.duckdb") && !b->dirty && !b->command_revision && b->sheets[0].undo_actions_count==3);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->request_epoch)++;
    if (p->mode==4) b->sheets[0].objects.items[0].rows++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->read_only=true;
    if (p->mode==7) b->sheets[0].source_versions.high_water++;
    if (p->mode==8) b->sheets[0].formats.revision++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->book->command_revision==1 && p->context->book->dirty && !strcmp(p->context->book->sheets[0].objects.items[0].source.path,"/b.duckdb")); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; Analysis p={.kind=SOURCE_DATABASE,.path="/b.duckdb",.schema="main",.table="beta"};
        Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; TableTransformResult r=apply(&c,&p,&recorder); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==TABLE_TRANSFORM_APPLIED && probe.publishes==1 && !probe.cancels);
        else CHECK(r.status==(mode==1 ? TABLE_TRANSFORM_LIMIT : mode==2 || mode==5 ? TABLE_TRANSFORM_INVALID : mode==6 ? TABLE_TRANSFORM_PROTECTED : TABLE_TRANSFORM_STALE) && !probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].undo_actions_count==3 && !strcmp(b.sheets[0].objects.items[0].source.path,"/a.duckdb"));
        workbook_free(&b);
    }
}
static void versions_and_stream(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false};
    Analysis a={.kind=SOURCE_CSV,.path="/a.csv",.source_version=1},p={.kind=SOURCE_CSV,.path="/b.csv",.source_version=2};
    analysis_free(&s->objects.items[0].source); s->objects.items[0].source=analysis_copy(&a);
    for (uint64_t i=1;i<=3;i++) { SourceVersions next={0}; SourceVersion row={.owner=s->objects.items[0].id,.version=i,.kind=SOURCE_CSV,.path=i==2 ? "/b.csv" : "/a.csv"}; CHECK(source_versions_plan(&s->source_versions,&row,true,&next,NULL)); source_versions_free(&s->source_versions); s->source_versions=next; }
    TableTransformTarget t=target(&c); WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    Analysis invalid=p; invalid.source_version=1; CHECK(apply(&c,&invalid,&recorder).status==TABLE_TRANSFORM_INVALID && !b.dirty && !workbook_history_stream_state(queue).owned_bytes);
    uint64_t query=s->objects.items[0].revision;
    for (unsigned i=0;i<40;i++) {
        CHECK(apply(&c,i%2 ? &a : &p,&recorder).status==TABLE_TRANSFORM_APPLIED && b.command_revision==i+1 && s->objects.items[0].revision==query+i+1 && s->source_versions.count==3 && s->source_versions.high_water==3);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(s->objects.undo_count==32 && s->undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.sources.versions.count==2 && event.before_objects.objects.items[0].source.source_version==1 && event.after_objects.objects.items[0].source.source_version==2);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK);
    p=a; p.source_version=3; CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_APPLIED && s->objects.items[0].revision==query+41);
    p.source_version=0; CHECK(apply(&c,&p,NULL).status==TABLE_TRANSFORM_APPLIED && s->source_versions.count==3 && s->objects.items[0].revision==query+42);
    t=target(&c); (void)t; workbook_free(&b);
}
int main(void) { plans(); callbacks(); versions_and_stream(); printf("PASS: %u admitted Table replacement checks\n",checks); return 0; }
