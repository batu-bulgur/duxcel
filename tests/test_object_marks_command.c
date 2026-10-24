#include "workbook/object_presentation_command.h"
#include "workbook/definition_state.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL object Marks:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0},source={.kind=SOURCE_CSV,.path="/source"}; PivotDefinition pivot={0};
    CHECK(workbook_add(&b,"Sheet",&empty,NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add_table(&s->objects,0,0,"Raw",&source,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    CHECK(objects_add_chart(&s->objects,0,10,"Chart",&pivot,NULL)==1); pivot_free(&pivot); worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_bind_source(&s->objects,1,s->objects.items[0].id,NULL)); worksheet_record(s,EDIT_OBJECTS);
    workbook_history_note_edit(&b); b.dirty=false; return b;
}
static ObjectPresentationResult apply(ObjectCommandContext *c,const MarkDefinition *marks,const WorkbookHistoryDefinitionRecorder *recorder) {
    ObjectCommandTarget t; CHECK(workbook_prepare_object_fence(c,0,c->book->sheets[0].objects.items[1].id,&t,NULL));
    return workbook_apply_object_presentation_recorded(c,&t,(ObjectPresentationCommand){OBJECT_PRESENTATION_BINDINGS,0,marks},recorder,NULL);
}
static void bindings(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false};
    MarkDefinition m={0},empty={0}; MarkField city={.name="City"},amount={.name="Amount",.measure=true,.aggregate=AGG_SUM};
    uint64_t query=s->objects.items[1].revision,allocator=s->objects.next_id; CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_NOOP && !b.dirty && b.command_revision==0);
    for (unsigned role=0;role<DX_MARK_ROLES;role++) CHECK(marks_assign(&m,(MarkRole)role,role==MARKS_SIZE || role==MARKS_TOOLTIP ? &amount : &city,NULL));
    ObjectPresentationPlanInfo info={.owned=99}; CHECK(objects_presentation_measure(&s->objects,s->objects.items[1].id,(ObjectPresentationCommand){OBJECT_PRESENTATION_BINDINGS,0,&m},&info,NULL)==OBJECT_PRESENTATION_READY);
    c.plan_reserved=info.owned+info.validation_scratch-1; CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_LIMIT && !s->objects.items[1].marks.counts[0] && !b.dirty);
    c.plan_reserved++; CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_APPLIED && b.command_revision==1 && s->objects.marks_revision==1 && !s->objects.title_revision && s->objects.items[1].revision==query && s->objects.next_id==allocator);
    CHECK(definition_marks_equal(&s->objects.items[1].marks,&m) && s->objects.items[1].marks.fields[0][0].name!=m.fields[0][0].name);
    CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_NOOP && b.command_revision==1);
    /* Latent dimension aggregate is authored state even though display binding
       equivalence deliberately ignores it until the field becomes a measure. */
    m.fields[MARKS_COLOR][0].aggregate=AGG_COUNT;
    CHECK(marks_equal(&s->objects.items[1].marks,&m) && !definition_marks_equal(&s->objects.items[1].marks,&m));
    CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_APPLIED && s->objects.items[1].marks.fields[0][0].aggregate==AGG_COUNT && s->objects.marks_revision==2);
    marks_free(&m); CHECK(!strcmp(s->objects.items[1].marks.fields[0][0].name,"City"));
    s->objects.marks_revision=UINT64_MAX; CHECK(apply(&c,&empty,NULL).status==OBJECT_PRESENTATION_LIMIT);
    CHECK(apply(&c,&s->objects.items[1].marks,NULL).status==OBJECT_PRESENTATION_NOOP); s->objects.marks_revision=2;
    m=marks_copy(&s->objects.items[1].marks); m.fields[MARKS_SIZE][0].grain=PIVOT_DATE_MONTH;
    CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_INVALID); m.fields[MARKS_SIZE][0].grain=PIVOT_DATE_EXACT;
    m.fields[MARKS_DETAIL][1].aggregate=AGG_COUNT; CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_INVALID); m.fields[MARKS_DETAIL][1].aggregate=0;
    free(m.fields[MARKS_DETAIL][0].name); m.fields[MARKS_DETAIL][0].name=dx_strdup("bad\xc0\x80"); CHECK(apply(&c,&m,NULL).status==OBJECT_PRESENTATION_INVALID); marks_free(&m);
    CHECK(apply(&c,NULL,NULL).status==OBJECT_PRESENTATION_INVALID); CHECK(apply(&c,&empty,NULL).status==OBJECT_PRESENTATION_APPLIED);
    workbook_free(&b);
}
static void stream(void) {
    Workbook b=book(); Worksheet *s=b.sheets; uint64_t epoch=1; ObjectCommandContext c={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false};
    ObjectCommandTarget t; CHECK(workbook_prepare_object_fence(&c,0,s->objects.items[1].id,&t,NULL));
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(queue); WorkbookHistoryEvent *held=NULL;
    uint64_t query=s->objects.items[1].revision;
    for (unsigned i=0;i<40;i++) {
        MarkDefinition m={0}; MarkField f={.name=i%2 ? "Amount" : "City",.aggregate=(Aggregate)(i%6),.grain=i%2 ? PIVOT_DATE_EXACT : PIVOT_DATE_MONTH,.measure=i%2!=0};
        CHECK(marks_assign(&m,MARKS_TOOLTIP,&f,NULL)); CHECK(apply(&c,&m,&recorder).status==OBJECT_PRESENTATION_APPLIED); marks_free(&m);
        CHECK(s->objects.items[1].revision==query && !s->objects.title_revision && s->objects.marks_revision==i+1 && b.command_revision==i+1);
        if (!i) CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    }
    CHECK(s->objects.undo_count==32 && s->undo_actions_count==32);
    DefinitionEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
    CHECK(definition_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==DEFINITION_EVENT_OK);
    CHECK(!event.before_objects.objects.items[1].marks.counts[MARKS_TOOLTIP] && !strcmp(event.after_objects.objects.items[1].marks.fields[MARKS_TOOLTIP][0].name,"City") && event.after_objects.objects.items[1].marks.fields[MARKS_TOOLTIP][0].grain==PIVOT_DATE_MONTH);
    definition_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK); CHECK(workbook_history_stream_free(&queue,true)==HISTORY_STREAM_OK);
    workbook_free(&b);
}
int main(void) { bindings(); stream(); printf("PASS: %u admitted Marks binding checks\n",checks); return 0; }
