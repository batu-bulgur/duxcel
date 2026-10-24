#include "workbook/object_move_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object move:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(bool styled) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source.csv",.source_version=1};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets; PivotDefinition p={0};
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,10,20,"Chart",&p,NULL)==1); worksheet_record(s,EDIT_OBJECTS); pivot_free(&p);
    CHECK(objects_bind_source(&s->objects,1,1,NULL)); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,30,"Other",NULL)==2); worksheet_record(s,EDIT_OBJECTS);
    SourceVersion row={.owner=1,.version=1,.kind=SOURCE_CSV,.path="/source.csv"}; SourceVersions next={0};
    CHECK(source_versions_plan(&s->source_versions,&row,true,&next,NULL)); s->source_versions=next;
    if (styled) { CellFormat f=format_default(); f.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){10,10,20,20},FORMAT_BOLD,f,NULL)); worksheet_record(s,EDIT_FORMAT); }
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectMoveTarget target(ObjectMoveContext *c,size_t sheet,uint64_t id) {
    ObjectMoveTarget t; CHECK(workbook_prepare_object_move(c,sheet,id,&t,NULL)); return t;
}
static void reject(ObjectMoveContext *c,const ObjectMoveTarget *t,uint64_t row,size_t col,const WorkbookHistoryDefinitionRecorder *recorder,ObjectMoveStatus status) {
    Workbook before=*c->layout.command.book; Worksheet sheets[2]; memcpy(sheets,c->layout.command.book->sheets,sizeof(sheets)); SheetObject items[DX_MAX_OBJECTS];
    memcpy(items,sheets[0].objects.items,sheets[0].objects.count*sizeof(*items)); char *error=NULL;
    ObjectMoveResult r=workbook_apply_object_move_recorded(c,t,row,col,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->layout.command.book,sizeof(before)) && !memcmp(sheets,c->layout.command.book->sheets,sizeof(sheets)) &&
        !memcmp(items,sheets[0].objects.items,sheets[0].objects.count*sizeof(*items))); free(error);
}
static void plans(void) {
    Workbook b=book(true); uint64_t request=1,rows=4; size_t columns=3; bool pending=false;
    ObjectMoveContext c={.layout={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}}; ObjectMoveTarget t=target(&c,0,2),bad;
#define STALE(field) do { bad=t; bad.field++; reject(&c,&bad,30,20,NULL,OBJECT_MOVE_STALE); } while (0)
    STALE(layout.object.owner.document); STALE(layout.object.owner.sheet); STALE(layout.object.owner.workbook.low); STALE(layout.object.owner.sheet_id); STALE(layout.object.owner.expected_revision); STALE(layout.object.owner.expected_document_revision); STALE(layout.source_rows); STALE(layout.source_columns); STALE(formats.revision); STALE(patches[0].mask);
#undef STALE
    reject(&c,&t,DX_SHEET_ROWS,20,NULL,OBJECT_MOVE_INVALID); reject(&c,&t,30,16384,NULL,OBJECT_MOVE_INVALID);
    reject(&c,&t,10,30,NULL,OBJECT_MOVE_INVALID); reject(&c,&t,10,2,NULL,OBJECT_MOVE_INVALID); reject(&c,&t,0,0,NULL,OBJECT_MOVE_INVALID);
    c.layout.command.read_only=true; reject(&c,&t,30,20,NULL,OBJECT_MOVE_PROTECTED); c.layout.command.read_only=false;
    ObjectMovePlanInfo info={.owned=99}; CHECK(objects_move_measure(&b.sheets[0].objects,2,10,20,&b.sheets[0].cells,rows,columns,&b.sheets[0].formats,&info,NULL)==OBJECT_MOVE_NOOP && info.owned==99);
    CHECK(objects_move_measure(&b.sheets[0].objects,2,30,20,&b.sheets[0].cells,rows,columns,&b.sheets[0].formats,&info,NULL)==OBJECT_MOVE_READY);
    c.layout.command.plan_reserved=info.owned+info.validation_scratch-1; reject(&c,&t,30,20,NULL,OBJECT_MOVE_LIMIT); c.layout.command.plan_reserved++;
    WorkbookHistoryDefinitionRecorder broken={0}; reject(&c,&t,30,20,&broken,OBJECT_MOVE_INVALID);
    b.command_revision=UINT64_MAX; bad=target(&c,0,2); reject(&c,&bad,30,20,NULL,OBJECT_MOVE_LIMIT); b.command_revision=0;
    b.sheets[0].formats.revision=UINT64_MAX; bad=target(&c,0,2); reject(&c,&bad,30,20,NULL,OBJECT_MOVE_LIMIT); b.sheets[0].formats.revision=t.formats.revision;
    uint64_t query=b.sheets[0].objects.items[1].revision;
    CHECK(workbook_reorder(&b,0,1,NULL));
    CHECK(workbook_apply_object_move_recorded(&c,&t,30,20,NULL,NULL).status==OBJECT_MOVE_APPLIED && b.command_revision==1 && b.dirty);
    Worksheet *s=b.sheets+1; CHECK(s->objects.items[1].row==30 && s->objects.items[1].col==20 && s->objects.items[1].revision==query && s->objects.next_id==3 && !s->objects.title_revision && !s->objects.marks_revision && s->undo_actions[s->undo_actions_count-1]==EDIT_OBJECTS_FORMAT);
    CHECK((format_at(&s->formats,30,20).flags&FORMAT_BOLD) && !(format_at(&s->formats,10,20).flags&FORMAT_BOLD));
    CHECK(objects_history(&s->objects,false) && format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    t=target(&c,1,2); c.layout.command.plan_reserved=0;
    CHECK(workbook_apply_object_move_recorded(&c,&t,10,20,NULL,NULL).status==OBJECT_MOVE_NOOP && s->redo_actions_count==1 && !b.dirty && (format_at(&s->formats,10,20).flags&FORMAT_BOLD));
    CHECK(objects_history(&s->objects,true) && format_history(&s->formats,true)); worksheet_transfer_history(s,true);
    CHECK(s->objects.items[1].row==30 && (format_at(&s->formats,30,20).flags&FORMAT_BOLD)); workbook_free(&b);
    b=book(false); c.layout.command.book=&b; c.layout.command.plan_reserved=SIZE_MAX;
    CHECK(cells_set(&b.sheets[0].cells,34,22,"Keep",NULL)); worksheet_record(b.sheets,EDIT_CELLS); t=target(&c,0,2); reject(&c,&t,30,20,NULL,OBJECT_MOVE_INVALID);
    rows=4; columns=22; pending=true; t=target(&c,0,2); reject(&c,&t,30,20,NULL,OBJECT_MOVE_INVALID); pending=false; columns=3;
    /* A changed geometry with unchanged formats does not manufacture effect8. */
    t=target(&c,0,3); b.sheets[0].formats.revision=UINT64_MAX; t=target(&c,0,3);
    CHECK(workbook_apply_object_move_recorded(&c,&t,40,30,NULL,NULL).status==OBJECT_MOVE_APPLIED && b.sheets[0].undo_actions[b.sheets[0].undo_actions_count-1]==EDIT_OBJECTS && b.sheets[0].formats.revision==UINT64_MAX); workbook_free(&b);
}
typedef struct { ObjectMoveContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->layout.command.book; p->reserves++;
    CHECK(!*ticket && r->view.effects==12 && r->view.before.objects->items[1].row==10 && r->view.after.objects->items[1].row==30 && r->view.before_formats.count && r->view.after_formats.count && !b->dirty && !b->command_revision && b->sheets[0].undo_actions_count==5);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->layout.source_rows)++;
    if (p->mode==4) b->sheets[0].objects.next_id++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->layout.command.read_only=true;
    if (p->mode==7) b->sheets[0].formats.items[0].range.r0++;
    if (p->mode==8) b->sheets[0].source_versions.items[0].owner++;
    if (p->mode==9) b->sheets[0].cells.revision++;
    if (p->mode==10) b->sheets[0].formats.revision++;
    if (p->mode==11) *(bool *)p->context->layout.source_count_pending=true;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; Workbook *b=p->context->layout.command.book; CHECK(ticket && b->command_revision==1 && b->dirty && b->sheets[0].objects.items[1].row==30 && (format_at(&b->sheets[0].formats,30,20).flags&FORMAT_BOLD)); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static bool admit(void *context,const FormatStore *next,size_t adopted,size_t scratch,size_t retained,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->layout.command.book;
    CHECK(adopted && scratch && retained && !b->command_revision && next->undo_count==b->sheets[0].formats.undo_count+1 && !next->redo_count && (format_at(next,30,20).flags&FORMAT_BOLD));
    if (p->mode==1) return false;
    if (p->mode==2) b->sheets[0].formats.items[0].mask=FORMAT_ITALIC;
    return true;
}
static void callbacks(void) {
    for (unsigned mode=0;mode<=11;mode++) {
        Workbook b=book(true); uint64_t request=1,rows=4; size_t columns=3; bool pending=false;
        ObjectMoveContext c={.layout={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}}; ObjectMoveTarget t=target(&c,0,2);
        Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p}; ObjectMoveResult r=workbook_apply_object_move_recorded(&c,&t,30,20,&recorder,NULL); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_MOVE_APPLIED && p.publishes==1 && !p.cancels);
        else CHECK(r.status==(mode==1 ? OBJECT_MOVE_LIMIT : mode==2 || mode==5 ? OBJECT_MOVE_INVALID : mode==6 ? OBJECT_MOVE_PROTECTED : OBJECT_MOVE_STALE) && !p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].objects.items[1].row==10 && b.sheets[0].undo_actions_count==5 && b.sheets[0].formats.undo_count==1);
        workbook_free(&b);
    }
    for (unsigned mode=0;mode<3;mode++) {
        Workbook b=book(true); uint64_t request=1,rows=4; size_t columns=3; bool pending=false;
        ObjectMoveContext c={.layout={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending},.admit=admit}; Probe p={&c,mode,0,0,0}; c.admission_context=&p; ObjectMoveTarget t=target(&c,0,2);
        ObjectMoveResult r=workbook_apply_object_move_recorded(&c,&t,30,20,NULL,NULL);
        CHECK(r.status==(mode==0 ? OBJECT_MOVE_APPLIED : mode==1 ? OBJECT_MOVE_LIMIT : OBJECT_MOVE_STALE) && b.sheets[0].objects.items[1].row==(mode==0 ? 30 : 10)); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(true); uint64_t request=1,rows=4; size_t columns=3; bool pending=false;
    ObjectMoveContext c={.layout={{&b,&request,SIZE_MAX,SIZE_MAX,true,false},&rows,&columns,&pending}}; ObjectMoveTarget t=target(&c,0,2);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        t=target(&c,0,2); CHECK(workbook_apply_object_move_recorded(&c,&t,i%2 ? 10 : 30,20,&recorder,NULL).status==OBJECT_MOVE_APPLIED && b.command_revision==i+1);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].formats.undo_count==32 && b.sheets[0].undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && event.effects==12 && event.before_objects.objects.items[1].row==10 && event.after_objects.objects.items[1].row==30 && event.formats.after.count && event.sources.versions.count==1);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { plans(); callbacks(); stream(); printf("PASS: %u admitted object move checks\n",checks); return 0; }
