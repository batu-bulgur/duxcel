#include "workbook/object_presentation_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object presentation:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const ObjectPresentationCommand line={OBJECT_PRESENTATION_MARK,MARK_LINE},measure={OBJECT_PRESENTATION_MEASURE,1};
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"}; PivotDefinition pivot={0};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_COUNT,NULL));
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,10,20,"Chart",&pivot,NULL)==1); worksheet_record(s,EDIT_OBJECTS); pivot_free(&pivot);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectCommandTarget target(ObjectCommandContext *c) {
    ObjectCommandTarget t; CHECK(workbook_prepare_object_fence(c,0,c->book->sheets[0].objects.items[1].id,&t,NULL)); return t;
}
static void reject(ObjectCommandContext *c,const ObjectCommandTarget *t,ObjectPresentationCommand command,const WorkbookHistoryDefinitionRecorder *recorder,ObjectPresentationStatus status) {
    Workbook before=*c->book; Worksheet sheets[2]; memcpy(sheets,c->book->sheets,sizeof(sheets)); SheetObject items[DX_MAX_OBJECTS];
    memcpy(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items)); char *error=NULL;
    ObjectPresentationResult r=workbook_apply_object_presentation_recorded(c,t,command,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) &&
        !memcmp(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items))); free(error);
}
static void commands(void) {
    Workbook b=book(); uint64_t epoch=7; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget good=target(&c),bad;
#define STALE(field) do { bad=good; bad.field++; reject(&c,&bad,line,NULL,OBJECT_PRESENTATION_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.low); STALE(owner.sheet_id); STALE(owner.expected_revision); STALE(owner.expected_document_revision); STALE(object); STALE(request_epoch);
#undef STALE
    reject(&c,&good,(ObjectPresentationCommand){OBJECT_PRESENTATION_MARK,99},NULL,OBJECT_PRESENTATION_INVALID);
    reject(&c,&good,(ObjectPresentationCommand){OBJECT_PRESENTATION_MEASURE,2},NULL,OBJECT_PRESENTATION_INVALID);
    reject(&c,&good,(ObjectPresentationCommand){99,0},NULL,OBJECT_PRESENTATION_INVALID);
    c.read_only=true; reject(&c,&good,line,NULL,OBJECT_PRESENTATION_PROTECTED); c.read_only=false;
    c.ready=false; reject(&c,&good,line,NULL,OBJECT_PRESENTATION_PROTECTED); c.ready=true;
    c.plan_reserved=1; reject(&c,&good,line,NULL,OBJECT_PRESENTATION_LIMIT); c.plan_reserved=SIZE_MAX;
    ObjectPresentationPlanInfo info={.owned=99};
    CHECK(objects_presentation_measure(&b.sheets[0].objects,good.object,(ObjectPresentationCommand){OBJECT_PRESENTATION_MARK,MARK_BAR},&info,NULL)==OBJECT_PRESENTATION_NOOP && info.owned==99);
    CHECK(objects_presentation_measure(&b.sheets[0].objects,good.object,line,&info,NULL)==OBJECT_PRESENTATION_READY && info.owned);
    uint64_t query=b.sheets[0].objects.items[1].revision,next=b.sheets[0].objects.next_id;
    CHECK(workbook_reorder(&b,0,1,NULL));
    CHECK(workbook_apply_object_presentation_recorded(&c,&good,line,NULL,NULL).status==OBJECT_PRESENTATION_APPLIED);
    Worksheet *s=b.sheets+1;
    CHECK(s->objects.items[1].mark==MARK_LINE && !s->objects.title_revision && !s->objects.marks_revision && s->objects.items[1].revision==query && s->objects.next_id==next && b.command_revision==1 && b.dirty && s->undo_actions_count==4);
    CHECK(workbook_prepare_object_fence(&c,1,good.object,&good,NULL));
    CHECK(workbook_apply_object_presentation_recorded(&c,&good,measure,NULL,NULL).status==OBJECT_PRESENTATION_APPLIED && s->objects.items[1].chart_measure==1 && s->objects.title_revision==1 && s->objects.items[1].revision==query);
    CHECK(objects_history(&s->objects,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(workbook_prepare_object_fence(&c,1,good.object,&good,NULL));
    c.plan_reserved=0;
    CHECK(workbook_apply_object_presentation_recorded(&c,&good,(ObjectPresentationCommand){OBJECT_PRESENTATION_MEASURE,0},NULL,NULL).status==OBJECT_PRESENTATION_NOOP && s->redo_actions_count==1 && !b.dirty && b.command_revision==3);
    c.plan_reserved=SIZE_MAX;
    CHECK(workbook_apply_object_presentation_recorded(&c,&good,(ObjectPresentationCommand){OBJECT_PRESENTATION_MARK,MARK_DOT},NULL,NULL).status==OBJECT_PRESENTATION_APPLIED && !s->redo_actions_count);
    CHECK(workbook_prepare_object_fence(&c,1,s->objects.items[0].id,&good,NULL));
    CHECK(workbook_apply_object_presentation_recorded(&c,&good,line,NULL,NULL).status==OBJECT_PRESENTATION_INVALID);
    workbook_free(&b);
}
typedef struct { ObjectCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.before.objects->items[1].mark==MARK_BAR && r->view.after.objects->items[1].mark==MARK_LINE && !b->dirty && b->command_revision==0 && b->sheets[0].undo_actions_count==3);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->request_epoch)++;
    if (p->mode==4) b->command_revision++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->ready=false;
    if (p->mode==7) b->sheets[0].objects.items[0].rows++;
    if (p->mode==8) b->sheets[0].objects.title_revision++;
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; p->publishes++; Workbook *b=p->context->book;
    CHECK(ticket && b->command_revision==1 && b->dirty && b->sheets[0].objects.items[1].mark==MARK_LINE && !b->sheets[0].objects.title_revision && b->sheets[0].undo_actions_count==4); free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c);
        Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p}; char *error=NULL;
        ObjectPresentationResult r=workbook_apply_object_presentation_recorded(&c,&t,line,&recorder,&error); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_PRESENTATION_APPLIED && p.publishes==1 && !p.cancels && !error);
        else {
            CHECK(r.status==(mode==1 ? OBJECT_PRESENTATION_LIMIT : mode==2 || mode==5 ? OBJECT_PRESENTATION_INVALID : mode==6 ? OBJECT_PRESENTATION_PROTECTED : OBJECT_PRESENTATION_STALE));
            CHECK(!p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && b.sheets[0].objects.items[1].mark==MARK_BAR && !b.dirty && b.sheets[0].undo_actions_count==3 && error);
        }
        free(error); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectCommandTarget t=target(&c); WorkbookHistoryDefinitionRecorder broken={0};
    reject(&c,&t,line,&broken,OBJECT_PRESENTATION_INVALID);
    b.command_revision=UINT64_MAX; t=target(&c); reject(&c,&t,line,NULL,OBJECT_PRESENTATION_LIMIT);
    CHECK(workbook_apply_object_presentation_recorded(&c,&t,(ObjectPresentationCommand){OBJECT_PRESENTATION_MARK,MARK_BAR},NULL,NULL).status==OBJECT_PRESENTATION_NOOP); b.command_revision=0;
    b.sheets[0].objects.title_revision=UINT64_MAX; t=target(&c); reject(&c,&t,measure,NULL,OBJECT_PRESENTATION_LIMIT);
    CHECK(workbook_apply_object_presentation_recorded(&c,&t,line,NULL,NULL).status==OBJECT_PRESENTATION_APPLIED); b.sheets[0].objects.title_revision=0;
    workbook_free(&b); b=book(); c.book=&b; t=target(&c);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s); WorkbookHistoryEvent *held=NULL; uint64_t query=b.sheets[0].objects.items[1].revision;
    for (unsigned i=0;i<40;i++) {
        t=target(&c); ObjectPresentationCommand command={i%2 ? OBJECT_PRESENTATION_MEASURE : OBJECT_PRESENTATION_MARK,i%2 ? (i/2+1)%2 : (i/2)%2+1};
        CHECK(workbook_apply_object_presentation_recorded(&c,&t,command,&recorder,NULL).status==OBJECT_PRESENTATION_APPLIED && b.command_revision==i+1 && b.sheets[0].objects.items[1].revision==query);
        if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32 && workbook_history_stream_state(s).pending==39 && b.sheets[0].objects.title_revision==20);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.before_objects.objects.items[1].mark==MARK_BAR && event.after_objects.objects.items[1].mark==MARK_LINE && event.after_objects.objects.items[1].source_id==b.sheets[0].objects.items[0].id);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK); workbook_free(&b);
    b=book(); c.book=&b; t=target(&c); origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&s)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(s);
    reject(&c,&t,line,&recorder,OBJECT_PRESENTATION_LIMIT); CHECK(!workbook_history_stream_state(s).owned_bytes); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { commands(); callbacks(); stream(); printf("PASS: %u admitted object presentation checks\n",checks); return 0; }
