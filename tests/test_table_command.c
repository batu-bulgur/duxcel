#include "workbook/table_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL table command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_table(&s->objects,40,2,"Other",&source,NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,20,"Pivot",NULL)==2); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,2,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    SourceVersions next={0}; SourceVersion row={.owner=s->objects.items[0].id,.version=1,.kind=SOURCE_CSV,.path="/source"};
    CHECK(source_versions_plan(&s->source_versions,&row,true,&next,NULL)); s->source_versions=next; s->objects.items[0].source.source_version=1;
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static TableTransformTarget target(TableCommandContext *c,size_t sheet) {
    TableTransformTarget t; CHECK(workbook_prepare_table_transform(c,sheet,c->book->sheets[sheet].objects.items[0].id,&t,NULL)); return t;
}
static Analysis proposal(const Worksheet *s) { Analysis a=analysis_copy(&s->objects.items[0].source); a.unique_rows=!a.unique_rows; return a; }
static void reject(TableCommandContext *c,const TableTransformTarget *t,const Analysis *p,const WorkbookHistoryDefinitionRecorder *recorder,TableTransformStatus expected) {
    Workbook before=*c->book; Worksheet sheets[2]; memcpy(sheets,c->book->sheets,sizeof(sheets));
    SheetObject objects[DX_MAX_OBJECTS]; memcpy(objects,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*objects)); Analysis a=*p; char *error=NULL;
    TableTransformResult r=workbook_apply_table_transform_recorded(c,t,p,recorder,&error);
    CHECK(r.status==expected && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) && !memcmp(&a,p,sizeof(a)) &&
        !memcmp(objects,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*objects))); free(error);
}
static void fences_and_ownership(void) {
    Workbook b=book(); uint64_t epoch=7; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; TableTransformTarget good=target(&c,0),bad;
    Analysis p=proposal(b.sheets);
#define STALE(field) do { memcpy(&bad,&good,sizeof(bad)); bad.field++; reject(&c,&bad,&p,NULL,TABLE_TRANSFORM_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.low); STALE(owner.sheet_id); STALE(owner.expected_revision); STALE(owner.expected_document_revision);
    STALE(request_epoch); STALE(format_revision); STALE(history_last); STALE(objects.next_id); STALE(objects.title_revision); STALE(objects.marks_revision);
    STALE(items[0].rows); STALE(items[1].columns); STALE(items[2].revision); STALE(sources.high_water); STALE(versions[0].raw.bytes);
#undef STALE
    memcpy(&bad,&good,sizeof(bad)); bad.object=UINT64_MAX; reject(&c,&bad,&p,NULL,TABLE_TRANSFORM_STALE);
    memcpy(&bad,&good,sizeof(bad)); bad.owner.row=1; reject(&c,&bad,&p,NULL,TABLE_TRANSFORM_INVALID);
    c.read_only=true; reject(&c,&good,&p,NULL,TABLE_TRANSFORM_PROTECTED); c.read_only=false;
    c.ready=false; reject(&c,&good,&p,NULL,TABLE_TRANSFORM_PROTECTED); c.ready=true;
    p.source_version=0; reject(&c,&good,&p,NULL,TABLE_TRANSFORM_PROTECTED); p.source_version=1;
    p.path[0]='!'; reject(&c,&good,&p,NULL,TABLE_TRANSFORM_PROTECTED); p.path[0]='/';
    c.plan_reserved=1; reject(&c,&good,&p,NULL,TABLE_TRANSFORM_LIMIT); c.plan_reserved=SIZE_MAX;
    CHECK(workbook_reorder(&b,0,1,NULL));
    TableTransformResult r=workbook_apply_table_transform_recorded(&c,&good,&p,NULL,NULL);
    CHECK(r.status==TABLE_TRANSFORM_APPLIED && r.owner==good.object && r.result_changed && r.before_revision==0 && r.after_revision==1 && b.command_revision==1 && b.dirty &&
        b.sheets[1].objects.items[0].source.unique_rows && !b.sheets[1].objects.items[1].source.unique_rows && b.sheets[1].undo_actions_count==5 && b.sheets[1].objects.undo_count==5);
    CHECK(p.path && p.unique_rows && p.path!=b.sheets[1].objects.items[0].source.path && b.sheets[1].objects.next_id==good.objects.next_id && b.sheets[1].source_versions.high_water==1);
    analysis_free(&p);
    CHECK(objects_history(&b.sheets[1].objects,false)); worksheet_transfer_history(b.sheets+1,false); b.command_revision++; b.dirty=false; epoch++;
    good=target(&c,1); p=analysis_copy(&b.sheets[1].objects.items[0].source); p.filter_number=(NumberType){NUM_DECIMAL,38,4};
    CHECK(workbook_apply_table_transform_recorded(&c,&good,&p,NULL,NULL).status==TABLE_TRANSFORM_NOOP && b.command_revision==2 && !b.dirty && b.sheets[1].redo_actions_count==1 && p.path);
    analysis_free(&p); p=proposal(b.sheets+1);
    CHECK(workbook_apply_table_transform_recorded(&c,&good,&p,NULL,NULL).status==TABLE_TRANSFORM_APPLIED && !b.sheets[1].redo_actions_count);
    analysis_free(&p); workbook_free(&b);
}
typedef struct { TableCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; TableCommandContext *c=p->context; Workbook *b=c->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && r->expected_revision==b->command_revision && r->revision==b->command_revision+1 && !b->dirty &&
        !r->view.before.objects->items[0].source.unique_rows && r->view.after.objects->items[0].source.unique_rows && b->sheets[0].undo_actions_count==4);
    if (p->mode==1) return false; if (p->mode==2) return true;
    *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)c->request_epoch)++;
    if (p->mode==4) b->command_revision++;
    if (p->mode==5) return false;
    if (p->mode==6) c->ready=false;
    if (p->mode==7) b->sheets[0].objects.items[1].rows++;
    if (p->mode==8) b->sheets[0].source_versions.items[0].raw=(SourcePacket){dx_alloc(1),1};
    if (p->mode==9) b->sheets[0].objects.items[0].source.filter_number=(NumberType){NUM_DECIMAL,38,4};
    return true;
}
static void publish(void *context,void *ticket) {
    Probe *p=context; p->publishes++; Workbook *b=p->context->book;
    CHECK(ticket && b->command_revision==1 && b->dirty && b->sheets[0].objects.items[0].source.unique_rows && b->sheets[0].undo_actions_count==5 && b->sheets[0].objects.undo_count==5);
    free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=9;mode++) {
        Workbook b=book(); uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; TableTransformTarget t=target(&c,0); Analysis p=proposal(b.sheets);
        Probe probe={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&probe}; char *error=NULL;
        TableTransformResult r=workbook_apply_table_transform_recorded(&c,&t,&p,&recorder,&error); CHECK(probe.reserves==1);
        if (!mode) CHECK(r.status==TABLE_TRANSFORM_APPLIED && probe.publishes==1 && !probe.cancels && p.path && !error);
        else {
            CHECK(r.status==(mode==1 ? TABLE_TRANSFORM_LIMIT : mode==2 || mode==5 ? TABLE_TRANSFORM_INVALID : mode==6 ? TABLE_TRANSFORM_PROTECTED : TABLE_TRANSFORM_STALE));
            CHECK(!probe.publishes && probe.cancels==(mode>=3 ? 1u : 0u) && p.path && !b.sheets[0].objects.items[0].source.unique_rows && !b.dirty && b.sheets[0].undo_actions_count==4 && error);
        }
        free(error); analysis_free(&p); workbook_free(&b);
    }
}
static void limits_and_order(void) {
    Workbook b=book(); uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; TableTransformTarget t=target(&c,0); Analysis p=proposal(b.sheets);
    WorkbookHistoryDefinitionRecorder broken={0}; reject(&c,&t,&p,&broken,TABLE_TRANSFORM_INVALID);
    b.command_revision=UINT64_MAX; t=target(&c,0); reject(&c,&t,&p,NULL,TABLE_TRANSFORM_LIMIT); analysis_free(&p);
    p=analysis_copy(&b.sheets[0].objects.items[0].source); CHECK(workbook_apply_table_transform_recorded(&c,&t,&p,NULL,NULL).status==TABLE_TRANSFORM_NOOP); analysis_free(&p); workbook_free(&b);
    b=book(); c.book=&b; Analysis *live=&b.sheets[0].objects.items[0].source;
    CHECK(analysis_filter_set(live,"City",FILTER_EQUAL,"Ankara",NULL) && analysis_filter_set(live,"Value",FILTER_EQUAL,"",NULL));
    t=target(&c,0); p=analysis_copy(live); CHECK(analysis_filter_move(&p,0,1));
    uint64_t revision=b.sheets[0].objects.items[0].revision;
    TableTransformResult r=workbook_apply_table_transform_recorded(&c,&t,&p,NULL,NULL);
    CHECK(r.status==TABLE_TRANSFORM_APPLIED && !r.result_changed && b.sheets[0].objects.items[0].revision==revision && !strcmp(b.sheets[0].objects.items[0].source.filter_column,"Value"));
    analysis_free(&p); workbook_free(&b);
}
static void stream(void) {
    Workbook b=book(); uint64_t epoch=1; TableCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; target(&c,0);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s); WorkbookHistoryEvent *held=NULL;
    for (unsigned i=0;i<40;i++) {
        TableTransformTarget t=target(&c,0); Analysis p=proposal(b.sheets);
        CHECK(workbook_apply_table_transform_recorded(&c,&t,&p,&recorder,NULL).status==TABLE_TRANSFORM_APPLIED && b.command_revision==i+1 && p.path);
        epoch++; analysis_free(&p); if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32 && workbook_history_stream_state(s).pending==39);
    DefinitionEvent first={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&first)==DEFINITION_EVENT_OK && first.effects==4 &&
        !first.before_objects.objects.items[0].source.unique_rows && first.after_objects.objects.items[0].source.unique_rows && first.sources.versions.count==1);
    definition_event_free(&first); CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK);
    workbook_free(&b);
    b=book(); c.book=&b; TableTransformTarget t=target(&c,0); origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&s)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(s);
    Analysis p=proposal(b.sheets); reject(&c,&t,&p,&recorder,TABLE_TRANSFORM_LIMIT); CHECK(!workbook_history_stream_state(s).owned_bytes);
    analysis_free(&p); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { fences_and_ownership(); callbacks(); limits_and_order(); stream(); printf("PASS: %u fenced Table transform command checks\n",checks); return 0; }
