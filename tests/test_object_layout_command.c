#include "workbook/object_layout_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object layout:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const ObjectLayoutCommand line={OBJECT_LAYOUT_REPRESENTATION,OBJECT_CHART,MARK_LINE,12,6},size={OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,14,7},matrix={OBJECT_LAYOUT_REPRESENTATION,OBJECT_PIVOT,MARK_BAR,5,3};
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"}; PivotDefinition pivot={0};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,10,20,"Chart",&pivot,NULL)==1); worksheet_record(s,EDIT_OBJECTS); pivot_free(&pivot);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,30,"Other",NULL)==2); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectLayoutTarget target(ObjectLayoutContext *c) {
    ObjectLayoutTarget t; CHECK(workbook_prepare_object_layout(c,0,c->command.book->sheets[0].objects.items[1].id,&t,NULL)); return t;
}
static void reject(ObjectLayoutContext *c,const ObjectLayoutTarget *t,ObjectLayoutCommand command,const WorkbookHistoryDefinitionRecorder *recorder,ObjectLayoutStatus status) {
    Workbook before=*c->command.book; Worksheet sheets[2]; memcpy(sheets,c->command.book->sheets,sizeof(sheets)); SheetObject items[DX_MAX_OBJECTS];
    memcpy(items,c->command.book->sheets[0].objects.items,c->command.book->sheets[0].objects.count*sizeof(*items)); char *error=NULL;
    ObjectLayoutResult r=workbook_apply_object_layout_recorded(c,t,command,recorder,&error);
    if (r.status!=status) fprintf(stderr,"Layout rejection: actual=%d expected=%d op=%d extent=%llu/%zu checks=%u error=%s\n",r.status,status,command.operation,(unsigned long long)command.rows,command.columns,checks,error ? error : "");
    CHECK(r.status==status && error && !memcmp(&before,c->command.book,sizeof(before)) && !memcmp(sheets,c->command.book->sheets,sizeof(sheets)) &&
        !memcmp(items,c->command.book->sheets[0].objects.items,c->command.book->sheets[0].objects.count*sizeof(*items))); free(error);
}
static void plans(void) {
    Workbook b=book(); uint64_t epoch=7,rows=4; size_t columns=3; bool pending=false;
    ObjectLayoutContext c={{&b,&epoch,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}; ObjectLayoutTarget good=target(&c),bad;
#define STALE(field) do { bad=good; bad.field++; reject(&c,&bad,line,NULL,OBJECT_LAYOUT_STALE); } while (0)
    STALE(object.owner.document); STALE(object.owner.sheet); STALE(object.owner.workbook.low); STALE(object.owner.sheet_id); STALE(object.owner.expected_revision); STALE(object.owner.expected_document_revision); STALE(object.request_epoch); STALE(source_rows); STALE(source_columns);
#undef STALE
    bad=good; bad.object.object=UINT64_MAX; reject(&c,&bad,line,NULL,OBJECT_LAYOUT_STALE);
    bad=good; bad.source_count_pending=true; reject(&c,&bad,line,NULL,OBJECT_LAYOUT_STALE);
    ObjectLayoutCommand invalids[]={ {99,OBJECT_CHART,MARK_BAR,12,6},{OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_LINE,12,6},{OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,7,6},
        {OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,12,2},{OBJECT_LAYOUT_REPRESENTATION,OBJECT_TABLE,MARK_BAR,12,6},{OBJECT_LAYOUT_REPRESENTATION,OBJECT_PIVOT,MARK_LINE,5,3},
        {OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,4097,6},{OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,12,257},{OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,12,11} };
    for (size_t i=0;i<sizeof(invalids)/sizeof(*invalids);i++) reject(&c,&good,invalids[i],NULL,OBJECT_LAYOUT_INVALID);
    c.command.read_only=true; reject(&c,&good,line,NULL,OBJECT_LAYOUT_PROTECTED); c.command.read_only=false;
    c.command.ready=false; reject(&c,&good,line,NULL,OBJECT_LAYOUT_PROTECTED); c.command.ready=true;
    ObjectLayoutPlanInfo info={.owned=99}; CHECK(objects_layout_measure(&b.sheets[0].objects,good.object.object,size,&b.sheets[0].cells,rows,columns,&info,NULL)==OBJECT_LAYOUT_READY);
    c.command.plan_reserved=info.owned+info.validation_scratch-1; reject(&c,&good,size,NULL,OBJECT_LAYOUT_LIMIT); c.command.plan_reserved++;
    uint64_t query=b.sheets[0].objects.items[1].revision,next=b.sheets[0].objects.next_id;
    CHECK(workbook_apply_object_layout_recorded(&c,&good,size,NULL,NULL).status==OBJECT_LAYOUT_APPLIED && b.command_revision==1 && b.dirty && b.sheets[0].objects.items[1].rows==14 && b.sheets[0].objects.items[1].columns==7 && b.sheets[0].objects.items[1].revision==query && b.sheets[0].objects.next_id==next && !b.sheets[0].objects.title_revision && !b.sheets[0].objects.marks_revision);
    CHECK(objects_history(&b.sheets[0].objects,false)); worksheet_transfer_history(b.sheets,false); b.command_revision++; b.dirty=false; good=target(&c); c.command.plan_reserved=0;
    CHECK(workbook_apply_object_layout_recorded(&c,&good,(ObjectLayoutCommand){OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,12,6},NULL,NULL).status==OBJECT_LAYOUT_NOOP && b.sheets[0].redo_actions_count==1 && b.command_revision==2 && !b.dirty);
    c.command.plan_reserved=SIZE_MAX; CHECK(workbook_reorder(&b,0,1,NULL));
    CHECK(workbook_apply_object_layout_recorded(&c,&good,matrix,NULL,NULL).status==OBJECT_LAYOUT_APPLIED && b.sheets[1].objects.items[1].kind==OBJECT_PIVOT && b.sheets[1].objects.items[1].rows==5 && !b.sheets[1].redo_actions_count);
    workbook_free(&b);
    /* Existing source and manual occupancy are checked against the full new
       rectangle, including pending source counts and chart growth fringes. */
    b=book(); c.command.book=&b; rows=11; columns=21; good=target(&c); reject(&c,&good,line,NULL,OBJECT_LAYOUT_INVALID);
    rows=4; pending=true; good=target(&c); reject(&c,&good,line,NULL,OBJECT_LAYOUT_INVALID);
    pending=false; columns=3;
    CHECK(cells_set(&b.sheets[0].cells,23,26,"Keep",NULL)); worksheet_record(b.sheets,EDIT_CELLS); good=target(&c); reject(&c,&good,size,NULL,OBJECT_LAYOUT_INVALID);
    workbook_free(&b);
}
typedef struct { ObjectLayoutContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->command.book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->view.before.objects->items[1].rows==12 && r->view.after.objects->items[1].rows==14 && !b->dirty && b->command_revision==0 && b->sheets[0].undo_actions_count==4);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->source_rows)++;
    if (p->mode==4) (*(size_t *)p->context->source_columns)++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->command.ready=false;
    if (p->mode==7) b->sheets[0].objects.items[0].rows++;
    if (p->mode==8) *(bool *)p->context->source_count_pending=true;
    if (p->mode==9) b->sheets[0].cells.revision++;
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; p->publishes++; Workbook *b=p->context->command.book;
    CHECK(ticket && b->command_revision==1 && b->dirty && b->sheets[0].objects.items[1].rows==14 && b->sheets[0].undo_actions_count==5); free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=9;mode++) {
        Workbook b=book(); uint64_t epoch=1,rows=4; size_t columns=3; bool pending=false;
        ObjectLayoutContext c={{&b,&epoch,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}; ObjectLayoutTarget t=target(&c);
        Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p}; char *error=NULL;
        ObjectLayoutResult r=workbook_apply_object_layout_recorded(&c,&t,size,&recorder,&error); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_LAYOUT_APPLIED && p.publishes==1 && !p.cancels && !error);
        else {
            CHECK(r.status==(mode==1 ? OBJECT_LAYOUT_LIMIT : mode==2 || mode==5 ? OBJECT_LAYOUT_INVALID : mode==6 ? OBJECT_LAYOUT_PROTECTED : OBJECT_LAYOUT_STALE));
            CHECK(!p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && b.sheets[0].objects.items[1].rows==12 && !b.dirty && b.sheets[0].undo_actions_count==4 && error);
        }
        free(error); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1,rows=4; size_t columns=3; bool pending=false;
    ObjectLayoutContext c={{&b,&epoch,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}; ObjectLayoutTarget t=target(&c); WorkbookHistoryDefinitionRecorder broken={0};
    reject(&c,&t,size,&broken,OBJECT_LAYOUT_INVALID);
    b.command_revision=UINT64_MAX; t=target(&c); reject(&c,&t,size,NULL,OBJECT_LAYOUT_LIMIT);
    CHECK(workbook_apply_object_layout_recorded(&c,&t,(ObjectLayoutCommand){OBJECT_LAYOUT_REPRESENTATION,OBJECT_CHART,MARK_BAR,12,6},NULL,NULL).status==OBJECT_LAYOUT_NOOP); b.command_revision=0;
    t=target(&c); WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        t=target(&c); ObjectLayoutCommand command=i%2 ? (ObjectLayoutCommand){OBJECT_LAYOUT_SIZE,OBJECT_CHART,MARK_BAR,12,6} : size;
        CHECK(workbook_apply_object_layout_recorded(&c,&t,command,&recorder,NULL).status==OBJECT_LAYOUT_APPLIED && b.command_revision==i+1);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.before_objects.objects.items[1].rows==12 && event.after_objects.objects.items[1].rows==14 && event.after_objects.objects.items[1].columns==7);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { plans(); callbacks(); stream(); printf("PASS: %u admitted object layout checks\n",checks); return 0; }
