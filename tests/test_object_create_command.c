#include "workbook/object_create_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object create:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint64_t request=1,primary_epoch=1,rows=0;
static size_t columns=0;
static bool pending;
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis primary={.kind=SOURCE_CSV,.path="/primary.csv"};
    CHECK(workbook_add(&b,"A",&primary,NULL)); b.dirty=false; return b;
}
static ObjectCreateContext context(Workbook *b) {
    return (ObjectCreateContext){.command={b,&request,SIZE_MAX,SIZE_MAX,true,false},.primary=&b->sheets[0].analysis,.primary_epoch=&primary_epoch,
        .source_rows=&rows,.source_columns=&columns,.source_count_pending=&pending};
}
static ObjectCreateTarget target(ObjectCreateContext *c) { ObjectCreateTarget t; CHECK(workbook_prepare_object_create(c,0,&t,NULL)); return t; }
static ObjectCreateCommand pivot(void) { return (ObjectCreateCommand){.kind=OBJECT_PIVOT,.row=10,.column=10,.name="Pivot"}; }
static void rejection(ObjectCreateContext *c,const ObjectCreateTarget *t,ObjectCreateCommand command,const WorkbookHistoryDefinitionRecorder *recorder,ObjectCreateStatus status) {
    Workbook before=*c->command.book; Worksheet sheet=c->command.book->sheets[0]; char *error=NULL;
    ObjectCreateResult r=workbook_apply_object_create_recorded(c,t,command,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->command.book,sizeof(before)) && !memcmp(&sheet,c->command.book->sheets,sizeof(sheet))); free(error);
}
static void plans(void) {
    Workbook b=book(); ObjectCreateContext c=context(&b); ObjectCreateTarget t=target(&c),bad; ObjectCreateCommand p=pivot(); ObjectCreatePlanInfo info={.owned=99};
    CHECK(!workbook_prepare_object_fence(&c.command,0,0,&bad.owner,NULL));
#define STALE(field) do { bad=t; bad.field++; rejection(&c,&bad,p,NULL,OBJECT_CREATE_STALE); } while (0)
    STALE(owner.owner.document); STALE(owner.owner.sheet); STALE(owner.owner.workbook.low); STALE(owner.owner.sheet_id); STALE(owner.owner.expected_revision); STALE(owner.owner.expected_document_revision);
    STALE(owner.objects.next_id); STALE(owner.request_epoch); STALE(owner.format_revision); STALE(owner.history_last); STALE(primary_epoch); STALE(source_rows); STALE(source_columns);
#undef STALE
    bad=t; bad.owner.object=1; rejection(&c,&bad,p,NULL,OBJECT_CREATE_INVALID);
    c.command.read_only=true; rejection(&c,&t,p,NULL,OBJECT_CREATE_PROTECTED); c.command.read_only=false;
    c.command.ready=false; rejection(&c,&t,p,NULL,OBJECT_CREATE_PROTECTED); c.command.ready=true;
    ObjectCreateCommand invalid=p; invalid.source_id=77;
    CHECK(objects_create_measure(&b.sheets[0].objects,invalid,c.primary,&b.sheets[0].cells,0,0,&info,NULL)==OBJECT_CREATE_INVALID && info.owned==99);
    CHECK(objects_create_measure(&b.sheets[0].objects,p,c.primary,&b.sheets[0].cells,0,0,&info,NULL)==OBJECT_CREATE_READY && info.owned);
    c.command.plan_reserved=info.owned+info.validation_scratch-1; rejection(&c,&t,p,NULL,OBJECT_CREATE_LIMIT); c.command.plan_reserved++;
    WorkbookHistoryDefinitionRecorder broken={0}; rejection(&c,&t,p,&broken,OBJECT_CREATE_INVALID);
    b.command_revision=UINT64_MAX; bad=target(&c); rejection(&c,&bad,p,NULL,OBJECT_CREATE_LIMIT); b.command_revision=0;
    CHECK(workbook_apply_object_create_recorded(&c,&t,p,NULL,NULL).status==OBJECT_CREATE_APPLIED);
    Worksheet *s=b.sheets; CHECK(s->objects.count==1 && s->objects.next_id==1 && s->objects.items[0].revision==1 && s->undo_actions_count==1 && b.command_revision==1 && b.dirty);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(!s->objects.count && s->objects.next_id==1 && s->redo_actions_count==1);
    t=target(&c); invalid=p; invalid.name=""; rejection(&c,&t,invalid,NULL,OBJECT_CREATE_INVALID); CHECK(s->redo_actions_count==1);
    CHECK(workbook_apply_object_create_recorded(&c,&t,p,NULL,NULL).status==OBJECT_CREATE_APPLIED && s->objects.next_id==2 && s->objects.items[0].id==2 && !s->redo_actions_count);
    t=target(&c); rejection(&c,&t,p,NULL,OBJECT_CREATE_INVALID); /* occupied rectangle */
    CHECK(cells_set(&s->cells,30,30,"Manual",NULL)); t=target(&c); p.row=30; p.column=30; rejection(&c,&t,p,NULL,OBJECT_CREATE_INVALID);
    pending=true; rows=2; columns=3; t=target(&c); p.row=500; p.column=0; rejection(&c,&t,p,NULL,OBJECT_CREATE_INVALID);
    pending=false; rows=columns=0; s->objects.next_id=UINT64_MAX; t=target(&c); rejection(&c,&t,p,NULL,OBJECT_CREATE_LIMIT);
    workbook_free(&b);
}
typedef struct { ObjectCreateContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->command.book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.object_high_water==1 && !r->view.before.objects->count && r->view.after.objects->count==1 &&
        r->view.after.objects->items[0].id==1 && !b->dirty && !b->command_revision && !b->sheets[0].objects.next_id && !b->sheets[0].undo_actions_count);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->command.request_epoch)++;
    if (p->mode==4) b->sheets[0].objects.next_id++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->command.read_only=true;
    if (p->mode==7) (*(uint64_t *)p->context->source_rows)++;
    if (p->mode==8) (*(uint64_t *)p->context->primary_epoch)++;
    if (p->mode==9) b->sheets[0].cells.revision++;
    if (p->mode==10) b->sheets[0].formats.revision++;
    if (p->mode==11) b->sheets[0].analysis.source_version++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->command.book->command_revision==1 && p->context->command.book->sheets[0].objects.next_id==1); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=11;mode++) {
        Workbook b=book(); ObjectCreateContext c=context(&b); ObjectCreateTarget t=target(&c); Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p};
        ObjectCreateResult r=workbook_apply_object_create_recorded(&c,&t,pivot(),&recorder,NULL); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_CREATE_APPLIED && p.publishes==1 && !p.cancels);
        else CHECK(r.status==(mode==1 ? OBJECT_CREATE_LIMIT : mode==2 || mode==5 ? OBJECT_CREATE_INVALID : mode==6 ? OBJECT_CREATE_PROTECTED : OBJECT_CREATE_STALE) &&
            !p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && !b.sheets[0].objects.count && !b.sheets[0].undo_actions_count);
        workbook_free(&b);
    }
    rows=columns=0;
}
static void sources_and_stream(void) {
    Workbook b=book(); ObjectCreateContext c=context(&b); ObjectCreateTarget t=target(&c); Analysis source={.kind=SOURCE_DATABASE,.path="/db.duckdb"};
    ObjectCreateCommand table={.kind=OBJECT_TABLE,.row=10,.column=10,.name="Table",.source=&source}; rejection(&c,&t,table,NULL,OBJECT_CREATE_PROTECTED);
    source.schema="main"; source.table="Sales";
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    CHECK(workbook_apply_object_create_recorded(&c,&t,table,&recorder,NULL).status==OBJECT_CREATE_APPLIED);
    CHECK(b.sheets[0].objects.items[0].source.path!=source.path && !strcmp(b.sheets[0].objects.items[0].source.table,"Sales"));
    t=target(&c); PivotDefinition definition={0}; CHECK(pivot_assign(&definition,PIVOT_ROWS,"City",AGG_SUM,NULL));
    ObjectCreateCommand chart={.kind=OBJECT_CHART,.row=10,.column=20,.name="Chart",.source_id=1,.pivot=&definition};
    CHECK(workbook_apply_object_create_recorded(&c,&t,chart,&recorder,NULL).status==OBJECT_CREATE_APPLIED);
    CHECK(b.sheets[0].objects.items[1].pivot.fields[PIVOT_ROWS][0].name!=definition.fields[PIVOT_ROWS][0].name); pivot_free(&definition);
    CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && !event.before_objects.objects.count && event.after_objects.objects.count==1 && event.after_objects.high_water==1);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.after_objects.objects.count==2 && event.after_objects.objects.items[1].source_id==1 && event.after_objects.high_water==2);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
    b=book(); c=context(&b); t=target(&c); origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&queue)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(queue);
    rejection(&c,&t,pivot(),&recorder,OBJECT_CREATE_LIMIT); CHECK(!workbook_history_stream_state(queue).owned_bytes); CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
static void bounds_and_dependencies(void) {
    Workbook b=book(); ObjectCreateContext c=context(&b); ObjectCreateTarget t=target(&c);
    for (unsigned i=0;i<32;i++) {
        ObjectCreateCommand command=pivot(); command.row=100+i*10; command.column=10;
        t=target(&c); CHECK(workbook_apply_object_create_recorded(&c,&t,command,NULL,NULL).status==OBJECT_CREATE_APPLIED);
    }
    CHECK(b.sheets[0].objects.count==32 && b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32);
    t=target(&c); rejection(&c,&t,pivot(),NULL,OBJECT_CREATE_LIMIT);
    CHECK(objects_history(&b.sheets[0].objects,false)); worksheet_transfer_history(b.sheets,false); b.command_revision++;
    CHECK(b.sheets[0].objects.next_id==32 && b.sheets[0].objects.count==31 && b.sheets[0].redo_actions_count==1);
    t=target(&c); ObjectCreateCommand command=pivot(); command.row=900;
    CHECK(workbook_apply_object_create_recorded(&c,&t,command,NULL,NULL).status==OBJECT_CREATE_APPLIED && b.sheets[0].objects.next_id==33 && b.sheets[0].objects.items[31].id==33 && !b.sheets[0].redo_actions_count);
    workbook_free(&b);
    b=book(); c=context(&b); t=target(&c);
    Analysis source={.kind=SOURCE_CSV,.path="/table.csv",.source_version=1};
    ObjectCreateCommand table={.kind=OBJECT_TABLE,.row=20,.column=20,.name="Table",.source=&source};
    rejection(&c,&t,table,NULL,OBJECT_CREATE_INVALID); /* Version identity needs complete composite ledger admission. */
    b.sheets[0].analysis.kind=SOURCE_DATABASE; b.sheets[0].analysis.schema=NULL; b.sheets[0].analysis.table=NULL;
    t=target(&c); rejection(&c,&t,pivot(),NULL,OBJECT_CREATE_PROTECTED);
    workbook_free(&b);
}
typedef struct { ObjectCreateContext *context; unsigned mode,reserves,publishes,cancels,admissions; } CompositeProbe;
static bool composite_admit(void *context,const FormatStore *prospective,size_t adopted,size_t scratch,size_t retained,char **error) {
    (void)adopted; (void)scratch; (void)retained; (void)error; CompositeProbe *p=context; p->admissions++;
    CHECK(prospective->source_aware && prospective->undo_count==p->context->command.book->sheets[0].formats.undo_count+1);
    return p->mode!=1;
}
static bool composite_reserve(void *context,const WorkbookHistoryDefinitionRecord *record,void **ticket,char **error) {
    (void)error; CompositeProbe *p=context; p->reserves++; Worksheet *s=p->context->command.book->sheets;
    CHECK(record->view.effects==12 && !record->view.before_formats.source_aware && record->view.after_formats.source_aware &&
        !s->formats.source_aware && !s->objects.count && !s->objects.next_id);
    if (p->mode==2) return false;
    *ticket=dx_alloc(1);
    if (p->mode==3) s->formats.items[0].value.flags^=1;
    return true;
}
static void composite_publish(void *context,void *ticket) { CompositeProbe *p=context; p->publishes++; CHECK(ticket); free(ticket); }
static void composite_cancel(void *context,void *ticket) { CompositeProbe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void manual_creation(void) {
    for (unsigned mode=0;mode<=3;mode++) {
        Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/table.csv"}; CHECK(workbook_add(&b,"Manual",&empty,NULL)); Worksheet *s=b.sheets;
        FormatSnapshot first={0}; CHECK(format_plan_spatial(&s->formats,(FormatRange){20,20,20,20},FORMAT_BOLD,(CellFormat){.flags=1,.size=13,.decimals=2},&first,NULL)); CHECK(format_commit(&s->formats,&first)); worksheet_record(s,EDIT_FORMAT);
        FormatPatch *prior=s->formats.items; uint64_t format_revision=s->formats.revision; b.dirty=false;
        ObjectCreateContext c=context(&b); c.manual_formats=true; CompositeProbe p={&c,mode,0,0,0,0}; c.admit=composite_admit; c.admission_context=&p;
        ObjectCreateCommand table={.kind=OBJECT_TABLE,.row=10,.column=10,.name="Table",.source=&source}; ObjectCreatePlanInfo info={0};
        CHECK(workbook_object_create_measure(&c,0,table,&info,NULL)==OBJECT_CREATE_READY && info.validation_scratch>=DX_FORMAT_VALIDATION_BYTES);
        c.command.plan_reserved=info.owned+info.validation_scratch; c.command.validation_reserved=info.validation_scratch;
        ObjectCreateTarget t=target(&c); WorkbookHistoryDefinitionRecorder recorder={composite_reserve,composite_publish,composite_cancel,&p};
        ObjectCreateResult r=workbook_apply_object_create_recorded(&c,&t,table,&recorder,NULL);
        if (!mode) {
            CHECK(r.status==OBJECT_CREATE_APPLIED && p.publishes==1 && !p.cancels && s->formats.source_aware && s->formats.revision==format_revision+1 && s->undo_actions_count==2 && s->undo_actions[1]==EDIT_OBJECTS_FORMAT);
            CHECK(s->formats.undo[1].items==prior && !s->formats.undo[1].source_aware && !s->formats.undo[0].source_aware && s->formats.items!=prior);
            CHECK(objects_history(&s->objects,false) && format_history(&s->formats,false)); worksheet_transfer_history(s,false);
            CHECK(!s->objects.count && s->objects.next_id==1 && !s->formats.source_aware && s->formats.items==prior && s->redo_actions_count==1);
            CHECK(objects_history(&s->objects,true) && format_history(&s->formats,true)); worksheet_transfer_history(s,true);
            CHECK(s->objects.count==1 && s->formats.source_aware && s->objects.next_id==1);
        } else CHECK(r.status==(mode<=2 ? OBJECT_CREATE_LIMIT : OBJECT_CREATE_STALE) && !p.publishes && p.cancels==(mode==3 ? 1u : 0u) &&
            !s->objects.count && !s->objects.next_id && !s->formats.source_aware && s->formats.items==prior && s->formats.revision==format_revision && s->undo_actions_count==1 && !b.dirty);
        workbook_free(&b);
    }
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/table.csv"}; CHECK(workbook_add(&b,"Manual",&empty,NULL));
    ObjectCreateContext c=context(&b); c.manual_formats=true; ObjectCreateTarget t=target(&c); ObjectCreateCommand table={.kind=OBJECT_TABLE,.row=10,.column=10,.name="Table",.source=&source};
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    CHECK(workbook_apply_object_create_recorded(&c,&t,table,&recorder,NULL).status==OBJECT_CREATE_APPLIED);
    CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.effects==12 && !event.formats.before.source_aware && event.formats.after.source_aware && !event.formats.before.count && !event.formats.after.count);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { plans(); callbacks(); sources_and_stream(); bounds_and_dependencies(); manual_creation(); printf("PASS: %u admitted object create checks\n",checks); return 0; }
