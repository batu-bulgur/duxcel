#include "workbook/object_delete_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object delete:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source.csv",.source_version=1};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,20,"View",NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,1,NULL)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,30,"Other",NULL)==2); worksheet_record(s,EDIT_OBJECTS);
    SourceVersion row={.owner=1,.version=1,.kind=SOURCE_CSV,.path="/source.csv"};
    SourceVersions next={0}; CHECK(source_versions_plan(&s->source_versions,&row,true,&next,NULL)); s->source_versions=next;
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectCommandTarget target(ObjectCommandContext *c,size_t sheet,uint64_t id) {
    ObjectCommandTarget t; CHECK(workbook_prepare_object_fence(c,sheet,id,&t,NULL)); return t;
}
static void reject(ObjectCommandContext *c,const ObjectCommandTarget *t,const WorkbookHistoryDefinitionRecorder *recorder,ObjectDeleteStatus status) {
    Workbook before=*c->book; Worksheet sheets[2]; memcpy(sheets,c->book->sheets,sizeof(sheets)); SheetObject items[DX_MAX_OBJECTS];
    memcpy(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items)); char *error=NULL;
    ObjectDeleteResult r=workbook_apply_object_delete_recorded(c,t,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) &&
        !memcmp(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items))); free(error);
}
static void plans(void) {
    Workbook b=book(); uint64_t request=1; ObjectCommandContext c={&b,&request,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c,0,2),bad;
#define STALE(field) do { bad=t; bad.field++; reject(&c,&bad,NULL,OBJECT_DELETE_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.low); STALE(owner.sheet_id); STALE(owner.expected_revision); STALE(owner.expected_document_revision); STALE(request_epoch); STALE(format_revision); STALE(history_last); STALE(objects.next_id);
#undef STALE
    ObjectDeletePlanInfo info={.owned=99};
    CHECK(objects_delete_measure(&b.sheets[0].objects,UINT64_MAX,&info,NULL)==OBJECT_DELETE_INVALID && info.owned==99);
    bad=target(&c,0,1); reject(&c,&bad,NULL,OBJECT_DELETE_PROTECTED);
    CHECK(objects_delete_measure(&b.sheets[0].objects,1,&info,NULL)==OBJECT_DELETE_PROTECTED && info.owned==99);
    c.read_only=true; reject(&c,&t,NULL,OBJECT_DELETE_PROTECTED); c.read_only=false;
    c.ready=false; reject(&c,&t,NULL,OBJECT_DELETE_PROTECTED); c.ready=true;
    CHECK(objects_delete_measure(&b.sheets[0].objects,2,&info,NULL)==OBJECT_DELETE_READY && info.owned);
    c.plan_reserved=info.owned+info.validation_scratch-1; reject(&c,&t,NULL,OBJECT_DELETE_LIMIT); c.plan_reserved++;
    WorkbookHistoryDefinitionRecorder broken={0}; reject(&c,&t,&broken,OBJECT_DELETE_INVALID);
    b.command_revision=UINT64_MAX; bad=target(&c,0,2); reject(&c,&bad,NULL,OBJECT_DELETE_LIMIT); b.command_revision=0;
    CHECK(workbook_reorder(&b,0,1,NULL));
    CHECK(workbook_apply_object_delete_recorded(&c,&t,NULL,NULL).status==OBJECT_DELETE_APPLIED && b.command_revision==1 && b.dirty);
    Worksheet *s=b.sheets+1; CHECK(s->objects.count==2 && s->objects.items[0].id==1 && s->objects.items[1].id==3 && s->objects.next_id==3 && !s->objects.title_revision && !s->objects.marks_revision && s->source_versions.count==1);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(s->objects.count==3 && s->objects.items[1].id==2 && s->objects.items[1].source_id==1 && s->redo_actions_count==1);
    t=target(&c,1,1); CHECK(workbook_apply_object_delete_recorded(&c,&t,NULL,NULL).status==OBJECT_DELETE_PROTECTED && s->redo_actions_count==1 && !b.dirty);
    t=target(&c,1,2); c.plan_reserved=SIZE_MAX; CHECK(workbook_apply_object_delete_recorded(&c,&t,NULL,NULL).status==OBJECT_DELETE_APPLIED && !s->redo_actions_count);
    t=target(&c,1,1); CHECK(workbook_apply_object_delete_recorded(&c,&t,NULL,NULL).status==OBJECT_DELETE_APPLIED && s->source_versions.count==1);
    t=target(&c,1,3); CHECK(objects_delete_measure(&s->objects,3,&info,NULL)==OBJECT_DELETE_READY && !info.owned); c.plan_reserved=info.validation_scratch;
    CHECK(workbook_apply_object_delete_recorded(&c,&t,NULL,NULL).status==OBJECT_DELETE_APPLIED && !s->objects.count && !s->objects.items && s->objects.next_id==3);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false);
    CHECK(s->objects.count==1 && s->objects.items[0].id==3 && s->objects.next_id==3);
    CHECK(objects_history(&s->objects,true)); worksheet_transfer_history(s,true);
    CHECK(!s->objects.count && s->objects.next_id==3 && s->source_versions.count==1);
    CHECK(objects_add(&s->objects,20,20,"New",NULL)==0 && s->objects.items[0].id==4);
    workbook_free(&b);
    b=book(); c.book=&b; t=target(&c,0,2); b.sheets[0].source_versions.items[0].owner=3; t=target(&c,0,2); c.plan_reserved=SIZE_MAX;
    reject(&c,&t,NULL,OBJECT_DELETE_INVALID); workbook_free(&b);
}
typedef struct { ObjectCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.object_high_water==3 && r->view.before.objects->count==3 && r->view.after.objects->count==2 && r->view.after.objects->items[1].id==3 && !b->dirty && !b->command_revision && b->sheets[0].undo_actions_count==4);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->request_epoch)++;
    if (p->mode==4) b->sheets[0].objects.next_id++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->read_only=true;
    if (p->mode==7) b->sheets[0].objects.items[2].row++;
    if (p->mode==8) b->sheets[0].source_versions.items[0].owner++;
    if (p->mode==9) b->sheets[0].cells.revision++;
    if (p->mode==10) b->sheets[0].formats.revision++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->book->command_revision==1 && p->context->book->dirty && p->context->book->sheets[0].objects.count==2); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=10;mode++) {
        Workbook b=book(); uint64_t request=1; ObjectCommandContext c={&b,&request,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c,0,2);
        Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p}; ObjectDeleteResult r=workbook_apply_object_delete_recorded(&c,&t,&recorder,NULL); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_DELETE_APPLIED && p.publishes==1 && !p.cancels);
        else CHECK(r.status==(mode==1 ? OBJECT_DELETE_LIMIT : mode==2 || mode==5 ? OBJECT_DELETE_INVALID : mode==6 ? OBJECT_DELETE_PROTECTED : OBJECT_DELETE_STALE) && !p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].objects.count==3 && b.sheets[0].undo_actions_count==4);
        workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); Worksheet *s=b.sheets;
    for (unsigned i=3;i<32;i++) { char name[32]; snprintf(name,sizeof(name),"View %u",i); CHECK(objects_add(&s->objects,10+i*10,20,name,NULL)==(int)i); worksheet_record(s,EDIT_OBJECTS); }
    workbook_history_note_edit(&b); uint64_t request=1; ObjectCommandContext c={&b,&request,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c,0,32);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<32;i++) {
        t=target(&c,0,s->objects.items[s->objects.count-1].id);
        CHECK(workbook_apply_object_delete_recorded(&c,&t,&recorder,NULL).status==OBJECT_DELETE_APPLIED && b.command_revision==i+1 && s->objects.next_id==32);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(!s->objects.count && s->objects.undo_count==32 && s->undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.before_objects.objects.count==32 && event.after_objects.objects.count==31 && event.before_objects.objects.items[31].id==32 && event.sources.versions.count==1);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    /* The final Table's metadata survives its removal to an empty graph. */
    for (unsigned i=1;i<32;i++) {
        CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
        if (i==31) { info=workbook_history_event_info(held); CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.before_objects.objects.count==1 && !event.after_objects.objects.count && event.sources.versions.count==1); definition_event_free(&event); }
        CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
    b=book(); c.book=&b; t=target(&c,0,2); origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&queue)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(queue);
    reject(&c,&t,&recorder,OBJECT_DELETE_LIMIT); CHECK(!workbook_history_stream_state(queue).owned_bytes); CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { plans(); callbacks(); stream(); printf("PASS: %u admitted object delete checks\n",checks); return 0; }
