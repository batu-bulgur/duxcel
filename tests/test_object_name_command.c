#include "workbook/object_name_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object name:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"};
    CHECK(workbook_add(&b,"A",&empty,NULL) && workbook_add(&b,"B",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,10,2,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add(&s->objects,10,20,"View",NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectNameTarget target(ObjectCommandContext *c,size_t sheet) {
    ObjectNameTarget t; CHECK(workbook_prepare_object_name(c,sheet,c->book->sheets[sheet].objects.items[1].id,&t,NULL)); return t;
}
static void reject(ObjectCommandContext *c,const ObjectNameTarget *t,const char *name,const WorkbookHistoryDefinitionRecorder *recorder,ObjectNameStatus status) {
    Workbook before=*c->book; Worksheet sheets[2]; memcpy(sheets,c->book->sheets,sizeof(sheets)); SheetObject items[DX_MAX_OBJECTS];
    memcpy(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items)); char *error=NULL;
    ObjectNameResult r=workbook_apply_object_name_recorded(c,t,name,recorder,&error);
    CHECK(r.status==status && error && !memcmp(&before,c->book,sizeof(before)) && !memcmp(sheets,c->book->sheets,sizeof(sheets)) &&
        !memcmp(items,c->book->sheets[0].objects.items,c->book->sheets[0].objects.count*sizeof(*items))); free(error);
}
static void names_and_drafts(void) {
    Workbook b=book(); uint64_t epoch=7; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectNameTarget good=target(&c,0),bad;
#define STALE(field) do { bad=good; bad.field++; reject(&c,&bad,"New",NULL,OBJECT_NAME_STALE); } while (0)
    STALE(owner.document); STALE(owner.sheet); STALE(owner.workbook.low); STALE(owner.sheet_id); STALE(owner.expected_revision); STALE(owner.expected_document_revision); STALE(object);
#undef STALE
    bad=good; bad.owner.row=1; reject(&c,&bad,"New",NULL,OBJECT_NAME_INVALID);
    bad=good; memset(bad.before,'x',sizeof(bad.before)); reject(&c,&bad,"New",NULL,OBJECT_NAME_INVALID);
    reject(&c,&good,"Raw",NULL,OBJECT_NAME_INVALID); reject(&c,&good,"rAw",NULL,OBJECT_NAME_INVALID);
    reject(&c,&good," \t\n",NULL,OBJECT_NAME_INVALID); reject(&c,&good,"new\nname",NULL,OBJECT_NAME_INVALID); reject(&c,&good,"bad\xc0\x80",NULL,OBJECT_NAME_INVALID);
    char long_name[DX_OBJECT_NAME_BYTES+2]; memset(long_name,'a',sizeof(long_name)-1); long_name[sizeof(long_name)-1]=0; reject(&c,&good,long_name,NULL,OBJECT_NAME_INVALID);
    c.read_only=true; reject(&c,&good,"New",NULL,OBJECT_NAME_PROTECTED); c.read_only=false;
    c.ready=false; reject(&c,&good,"New",NULL,OBJECT_NAME_PROTECTED); c.ready=true;
    c.plan_reserved=1; reject(&c,&good,"New",NULL,OBJECT_NAME_LIMIT); c.plan_reserved=SIZE_MAX;
    ObjectNamePlanInfo info={.owned=99}; CHECK(objects_name_plan_measure(&b.sheets[0].objects,good.object,"  View  ",&info,NULL)==OBJECT_NAME_NOOP && info.owned==99);
    CHECK(workbook_apply_object_name_recorded(&c,&good,"  View  ",NULL,NULL).status==OBJECT_NAME_NOOP && !b.dirty && b.command_revision==0 && !b.sheets[0].objects.title_revision);
    /* Background result/schema completion is not a conflicting rename draft.
       The fresh transaction fence must capture and preserve these latest fields. */
    uint64_t query=b.sheets[0].objects.items[1].revision,rows=b.sheets[0].objects.items[1].rows;
    b.sheets[0].objects.items[1].rows++; b.sheets[0].objects.items[0].source.filter_number=(NumberType){NUM_DECIMAL,38,4}; epoch++;
    CHECK(workbook_reorder(&b,0,1,NULL));
    ObjectNameResult r=workbook_apply_object_name_recorded(&c,&good,"  İstanbul view  ",NULL,NULL);
    CHECK(r.status==OBJECT_NAME_APPLIED && r.before_revision==0 && r.after_revision==1 && b.command_revision==1 && b.dirty &&
        !strcmp(b.sheets[1].objects.items[1].name,"İstanbul view") && b.sheets[1].objects.items[1].rows==rows+1 && b.sheets[1].objects.items[1].revision==query &&
        b.sheets[1].objects.items[0].source.filter_number.kind==NUM_DECIMAL && b.sheets[1].objects.title_revision==1 && b.sheets[1].undo_actions_count==4);
    CHECK(objects_history(&b.sheets[1].objects,false)); worksheet_transfer_history(b.sheets+1,false); b.command_revision++; b.dirty=false;
    good=target(&c,1); uint64_t titles=b.sheets[1].objects.title_revision;
    CHECK(workbook_apply_object_name_recorded(&c,&good,"View",NULL,NULL).status==OBJECT_NAME_NOOP && b.sheets[1].redo_actions_count==1 && b.command_revision==2 && !b.dirty && b.sheets[1].objects.title_revision==titles);
    CHECK(workbook_apply_object_name_recorded(&c,&good,"Branch",NULL,NULL).status==OBJECT_NAME_APPLIED && !b.sheets[1].redo_actions_count);
    workbook_free(&b);
    b=book(); c.book=&b; good=target(&c,0); CHECK(objects_rename(&b.sheets[0].objects,1,"External",NULL));
    reject(&c,&good,"New",NULL,OBJECT_NAME_STALE); workbook_free(&b);
}
typedef struct { ObjectCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    (void)error; Probe *p=context; Workbook *b=p->context->book; p->reserves++;
    CHECK(!*ticket && r->view.effects==4 && !strcmp(r->view.before.objects->items[1].name,"View") && !strcmp(r->view.after.objects->items[1].name,"New") && !b->dirty && b->command_revision==0 && b->sheets[0].undo_actions_count==3);
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
    CHECK(ticket && b->command_revision==1 && b->dirty && !strcmp(b->sheets[0].objects.items[1].name,"New") && b->sheets[0].objects.title_revision==1 && b->sheets[0].undo_actions_count==4);
    free(ticket);
}
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=8;mode++) {
        Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectNameTarget t=target(&c,0);
        Probe p={&c,mode,0,0,0}; WorkbookHistoryDefinitionRecorder recorder={reserve,publish,cancel,&p}; char *error=NULL;
        ObjectNameResult r=workbook_apply_object_name_recorded(&c,&t,"New",&recorder,&error); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==OBJECT_NAME_APPLIED && p.publishes==1 && !p.cancels && !error);
        else {
            CHECK(r.status==(mode==1 ? OBJECT_NAME_LIMIT : mode==2 || mode==5 ? OBJECT_NAME_INVALID : mode==6 ? OBJECT_NAME_PROTECTED : OBJECT_NAME_STALE));
            CHECK(!p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && !strcmp(b.sheets[0].objects.items[1].name,"View") && !b.dirty && b.sheets[0].undo_actions_count==3 && error);
        }
        free(error); workbook_free(&b);
    }
}
static void limits_and_stream(void) {
    Workbook b=book(); uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectNameTarget t=target(&c,0); WorkbookHistoryDefinitionRecorder broken={0};
    reject(&c,&t,"New",&broken,OBJECT_NAME_INVALID);
    b.command_revision=UINT64_MAX; t=target(&c,0); reject(&c,&t,"New",NULL,OBJECT_NAME_LIMIT);
    CHECK(workbook_apply_object_name_recorded(&c,&t,"View",NULL,NULL).status==OBJECT_NAME_NOOP); b.command_revision=0;
    b.sheets[0].objects.title_revision=UINT64_MAX; t=target(&c,0); reject(&c,&t,"New",NULL,OBJECT_NAME_LIMIT);
    CHECK(workbook_apply_object_name_recorded(&c,&t,"View",NULL,NULL).status==OBJECT_NAME_NOOP); b.sheets[0].objects.title_revision=0;
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *s=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&s)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s); WorkbookHistoryEvent *held=NULL; uint64_t query=b.sheets[0].objects.items[1].revision;
    for (unsigned i=0;i<40;i++) {
        t=target(&c,0); char label[32]; snprintf(label,sizeof(label),"Label %u",i);
        CHECK(workbook_apply_object_name_recorded(&c,&t,label,&recorder,NULL).status==OBJECT_NAME_APPLIED && b.command_revision==i+1 && b.sheets[0].objects.items[1].revision==query);
        if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(b.sheets[0].objects.undo_count==32 && b.sheets[0].undo_actions_count==32 && workbook_history_stream_state(s).pending==39 && b.sheets[0].objects.title_revision==40);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK && !strcmp(event.before_objects.objects.items[1].name,"View") && !strcmp(event.after_objects.objects.items[1].name,"Label 0"));
    definition_event_free(&event); CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_OK); workbook_free(&b);
    b=book(); c.book=&b; t=target(&c,0); origin=(WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,1},&s)==HISTORY_STREAM_OK); recorder=workbook_history_stream_definition_recorder(s);
    reject(&c,&t,"New",&recorder,OBJECT_NAME_LIMIT); CHECK(!workbook_history_stream_state(s).owned_bytes); CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) { names_and_drafts(); callbacks(); limits_and_stream(); printf("PASS: %u admitted object rename checks\n",checks); return 0; }
