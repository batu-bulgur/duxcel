#include "workbook/cell_command.h"
#include "workbook/history.h"
#include "workbook/history_stream.h"
#include "workbook/cell_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const CellEditPolicy manual={0,0,true};
static CellEditTarget prepare(Workbook *book,size_t sheet,uint64_t row,size_t col) {
    CellEditTarget target={0}; char *error=NULL;
    CHECK(workbook_prepare_cell_edit(book,sheet,row,col,&target,&error)); CHECK(!error);
    return target;
}
static CellEditResult apply(Workbook *book,CellEditTarget target,const char *text,CellEditStatus expected) {
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,text,text ? strlen(text) : 0}; char *error=NULL;
    CellEditResult result=workbook_apply_cell_edit(book,&command,manual,&error);
    CHECK(result.status==expected); CHECK((expected<=CELL_EDIT_NOOP)==(error==NULL)); free(error); return result;
}
static void reject(Workbook *book,CellEditCommand command,CellEditPolicy policy,CellEditStatus expected) {
    Workbook before=*book; Worksheet *copy=dx_alloc(book->count*sizeof(*copy));
    memcpy(copy,book->sheets,book->count*sizeof(*copy)); char *error=NULL;
    CellEditResult result=workbook_apply_cell_edit(book,&command,policy,&error);
    CHECK(result.status==expected); CHECK(error!=NULL);
    CHECK(!memcmp(&before,book,sizeof(before))); CHECK(!memcmp(copy,book->sheets,book->count*sizeof(*copy)));
    free(error); free(copy);
}
static void basic_and_history(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"First",&blank,&error)); book.dirty=false;
    CellEditTarget target=prepare(&book,0,1,1); CHECK(!book.dirty && target.document && target.sheet);
    char value[]="12345678901234567890.0123456789";
    CellEditResult result=apply(&book,target,value,CELL_EDIT_APPLIED); value[0]='x';
    Worksheet *s=&book.sheets[0]; CHECK(result.before_revision==0 && result.after_revision==1);
    CHECK(book.dirty && s->undo_actions_count==1 && s->cells.undo_count==1);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123456789"));
    target=prepare(&book,0,1,1); book.dirty=false;
    result=apply(&book,target,cells_get(&s->cells,1,1),CELL_EDIT_NOOP);
    CHECK(!book.dirty && s->undo_actions_count==1 && s->cells.undo_count==1);
    CHECK(result.before_revision==result.after_revision);
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false);
    CHECK(s->redo_actions_count==1 && !*cells_get(&s->cells,1,1));
    target=prepare(&book,0,1,1); apply(&book,target,NULL,CELL_EDIT_NOOP);
    CHECK(!book.dirty && s->redo_actions_count==1 && s->cells.redo_count==1);
    CHECK(cells_history(&s->cells,true)); worksheet_transfer_history(s,true);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123456789"));
    target=prepare(&book,0,1,1); apply(&book,target,"NULL",CELL_EDIT_APPLIED);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL"));
    /* All cell-store changes fence a draft, including Undo and unrelated cells. */
    CellEditTarget stale=target; CellEditCommand command={CELL_EDIT_COMMAND_VERSION,stale,"NULL",4};
    reject(&book,command,manual,CELL_EDIT_STALE);
    target=prepare(&book,0,1,1); apply(&book,prepare(&book,0,2,2),"Other",CELL_EDIT_APPLIED);
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    target=prepare(&book,0,1,1); apply(&book,target,"",CELL_EDIT_APPLIED);
    CHECK(!*cells_get(&s->cells,1,1));
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL"));
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    workbook_free(&book); free(error);
}
static void identity_and_validation(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"First",&blank,&error));
    CellEditTarget target=prepare(&book,0,1,1);
    CHECK(workbook_add(&book,"Second",&blank,&error));
    CellEditTarget other=prepare(&book,1,1,1); CHECK(target.sheet!=other.sheet);
    CHECK(workbook_rename(&book,0,"Renamed",&error)); CHECK(workbook_reorder(&book,0,1,&error));
    apply(&book,target,"Original owner",CELL_EDIT_APPLIED);
    CHECK(!strcmp(cells_get(&book.sheets[1].cells,1,1),"Original owner"));
    CHECK(!*cells_get(&book.sheets[0].cells,1,1));
    CHECK(workbook_duplicate(&book,1,&error));
    CellEditTarget duplicate=prepare(&book,book.active,1,1); CHECK(duplicate.sheet!=target.sheet);
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot!=NULL);
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,duplicate,"No",2};
    reject(snapshot,command,manual,CELL_EDIT_STALE);
    CellEditTarget copied=prepare(snapshot,snapshot->active,1,1); CHECK(copied.document!=duplicate.document);
    reject(snapshot,command,manual,CELL_EDIT_STALE); workbook_free(snapshot); free(snapshot);
    command.target=prepare(&book,1,3,3);
    command.version=0; reject(&book,command,manual,CELL_EDIT_INVALID); command.version=CELL_EDIT_COMMAND_VERSION;
    command.text=NULL; reject(&book,command,manual,CELL_EDIT_INVALID);
    char embedded[]={'A',0,'B'}; command.text=embedded; command.bytes=3; reject(&book,command,manual,CELL_EDIT_INVALID);
    command.text="X"; command.bytes=65537; reject(&book,command,manual,CELL_EDIT_INVALID);
    command.bytes=1; CellEditPolicy blocked={0,0,false}; reject(&book,command,blocked,CELL_EDIT_PROTECTED);
    blocked=(CellEditPolicy){5,5,true}; reject(&book,command,blocked,CELL_EDIT_PROTECTED);
    uint64_t original_row=command.target.row; command.target.row=DX_SHEET_ROWS; reject(&book,command,manual,CELL_EDIT_INVALID); command.target.row=original_row;
    command.target.column=16384; reject(&book,command,manual,CELL_EDIT_INVALID); command.target.column=3;
    Worksheet *s=&book.sheets[1]; s->cells.revision=UINT64_MAX;
    command.target=prepare(&book,1,3,3); reject(&book,command,manual,CELL_EDIT_LIMIT);
    apply(&book,command.target,"",CELL_EDIT_NOOP);
    CellEditTarget unchanged={0}; CHECK(!workbook_prepare_cell_edit(&book,99,0,0,&unchanged,&error)); CHECK(!unchanged.document); free(error); error=NULL;
    uint64_t doc=target.document; workbook_free(&book); workbook_init(&book);
    CHECK(workbook_add(&book,"New document",&blank,&error));
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    CHECK(prepare(&book,0,1,1).document!=doc); workbook_free(&book); free(error);
}
static void object_and_capacity(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Bounded",&blank,&error)); Worksheet *s=&book.sheets[0];
    s->objects.items=dx_alloc(sizeof(*s->objects.items)); s->objects.count=1; s->objects.next_id=1;
    s->objects.items[0]=(SheetObject){.id=1,.row=10,.col=10,.rows=5,.columns=5,.kind=OBJECT_PIVOT,.name=dx_strdup("Pivot")};
    CHECK(objects_valid(&s->objects,&error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,prepare(&book,0,11,11),"X",1};
    reject(&book,command,manual,CELL_EDIT_PROTECTED);
    command.target=prepare(&book,0,9,9); apply(&book,command.target,"Outside",CELL_EDIT_APPLIED);
    workbook_free(&book); workbook_init(&book); CHECK(workbook_add(&book,"Full",&blank,&error)); s=&book.sheets[0];
    s->cells.cells=dx_alloc(DX_MAX_EDIT_CELLS*sizeof(*s->cells.cells)); s->cells.count=s->cells.capacity=DX_MAX_EDIT_CELLS;
    for (size_t i=0;i<s->cells.count;i++) s->cells.cells[i]=(SheetCell){i,0,dx_strdup("X")};
    s->cells.bytes=2*s->cells.count;
    command.target=prepare(&book,0,DX_MAX_EDIT_CELLS,0); reject(&book,command,manual,CELL_EDIT_LIMIT);
    /* A byte span is copied before mutation; its source need not terminate. */
    char span[]={'O','K'}; command=(CellEditCommand){CELL_EDIT_COMMAND_VERSION,prepare(&book,0,0,0),span,2};
    CHECK(workbook_apply_cell_edit(&book,&command,manual,&error).status==CELL_EDIT_APPLIED);
    span[0]='N'; CHECK(!strcmp(cells_get(&s->cells,0,0),"OK"));
    workbook_free(&book); free(error);
}
static void pure_admission(void) {
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Pure",&(Analysis){0},&error));
    CellEditCommand c={CELL_EDIT_COMMAND_VERSION,prepare(&book,0,1,1),"New",3};
    Workbook before=book; Worksheet sheet=book.sheets[0];
    CellEditResult ready=workbook_check_cell_edit(&book,&c,manual,&error);
    CHECK(ready.status==CELL_EDIT_READY && !error && ready.before_revision==ready.after_revision && ready.before_document_revision==ready.after_document_revision);
    CHECK(!memcmp(&before,&book,sizeof(book)) && !memcmp(&sheet,book.sheets,sizeof(sheet)));
    CHECK(workbook_apply_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_APPLIED);
    before=book; sheet=book.sheets[0];
    CHECK(workbook_check_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_STALE && error); free(error); error=NULL;
    CHECK(!memcmp(&before,&book,sizeof(book)) && !memcmp(&sheet,book.sheets,sizeof(sheet)));
    c.target=prepare(&book,0,1,1);
    CHECK(workbook_check_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_NOOP && !error);
    workbook_free(&book);
}
typedef struct { CellEditRecord record; unsigned char arena[128]; bool live; } RecordedCell;
typedef struct {
    Workbook *book;
    RecordedCell events[48];
    unsigned attempts,published,cancelled;
    bool reject,race,empty,invalid_ticket;
} RecorderTrace;
static bool trace_reserve(void *context,const CellEditRecord *record,void **ticket,char **error) {
    RecorderTrace *t=context; t->attempts++;
    CHECK(!*ticket && record->revision==t->book->command_revision+1 && record->target.workbook.high==t->book->id.high && record->target.workbook.low==t->book->id.low);
    CHECK(record->delta.sheet==record->target.sheet_id && record->delta.row==record->target.row && record->delta.column==record->target.column);
    const char *current=cells_get(&t->book->sheets[0].cells,record->delta.row,record->delta.column);
    CHECK(strlen(current)==record->delta.before_bytes && !memcmp(current,record->delta.before,record->delta.before_bytes));
    if (t->reject) { if (error) { free(*error); *error=dx_strdup("Recorder capacity is full."); } return false; }
    if (t->empty) return true; /* Deliberately malformed adapter. */
    CHECK(t->published<48); RecordedCell *r=t->events+t->published; CHECK(!r->live);
    r->record=*record; CHECK(cell_delta_capture(record->delta,r->arena,sizeof(r->arena),&r->record.delta)==CELL_DELTA_READY);
    r->live=true; *ticket=r;
    if (t->race) t->book->command_revision++; /* Test-only stale-owner injection. */
    return !t->invalid_ticket;
}
static void trace_publish(void *context,void *ticket) {
    RecorderTrace *t=context; RecordedCell *r=ticket; CHECK(r==t->events+t->published && r->live);
    CHECK(t->book->command_revision==r->record.revision && t->book->dirty && t->book->sheets[0].undo_actions_count);
    const char *current=cells_get(&t->book->sheets[0].cells,r->record.delta.row,r->record.delta.column);
    CHECK(strlen(current)==r->record.delta.after_bytes && !memcmp(current,r->record.delta.after,r->record.delta.after_bytes));
    r->live=false; t->published++;
}
static void trace_cancel(void *context,void *ticket) {
    RecorderTrace *t=context; RecordedCell *r=ticket; CHECK(r==t->events+t->published && r->live);
    r->live=false; t->cancelled++;
}
static void recorded_admission(void) {
    Workbook b; workbook_init(&b); char *error=NULL; CHECK(workbook_add(&b,"Recorded",&(Analysis){0},&error)); b.dirty=false;
    RecorderTrace t={.book=&b}; CellEditRecorder sink={trace_reserve,trace_publish,trace_cancel,&t};
    CellEditCommand c={CELL_EDIT_COMMAND_VERSION,prepare(&b,0,1,1),"9007199254740993.12",sizeof("9007199254740993.12")-1};
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_APPLIED && !error);
    CHECK(t.attempts==1 && t.published==1 && !t.cancelled && !t.events[0].record.delta.before_bytes && !memcmp(t.events[0].record.delta.after,c.text,c.bytes));
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_STALE && error); free(error); error=NULL;
    c.target=prepare(&b,0,1,1);
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_NOOP && !error && t.attempts==1);
    CHECK(workbook_history_step(&b,false,&error).status==WORKBOOK_HISTORY_APPLIED); b.dirty=false;
    c.target=prepare(&b,0,1,1); c.text="Next"; c.bytes=4;
    Workbook before=b; Worksheet sheet=b.sheets[0]; t.reject=true;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_LIMIT && error && strstr(error,"capacity")); free(error); error=NULL;
    CHECK(!memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,b.sheets,sizeof(sheet)) && t.published==1 && !t.cancelled && workbook_history_available(&b,true)); t.reject=false;
    CellEditRecorder malformed=sink; malformed.publish=NULL; unsigned attempts=t.attempts;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&malformed,&error).status==CELL_EDIT_INVALID && error); free(error); error=NULL;
    CHECK(t.attempts==attempts && !memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,b.sheets,sizeof(sheet)));
    t.empty=true;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_INVALID && error); free(error); error=NULL; t.empty=false;
    CHECK(t.published==1 && !t.cancelled && !memcmp(&before,&b,sizeof(b)));
    t.invalid_ticket=true;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_INVALID && error); free(error); error=NULL; t.invalid_ticket=false;
    CHECK(t.cancelled==1 && t.published==1 && !memcmp(&before,&b,sizeof(b)));
    t.race=true;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_STALE && error); free(error); error=NULL; t.race=false;
    CHECK(t.cancelled==2 && t.published==1 && b.command_revision==before.command_revision+1 && !memcmp(&sheet,b.sheets,sizeof(sheet)) && workbook_history_available(&b,true));
    workbook_free(&b); workbook_init(&b); CHECK(workbook_add(&b,"Full",&(Analysis){0},&error)); b.dirty=false; t=(RecorderTrace){.book=&b};
    Worksheet *s=b.sheets; s->cells.cells=dx_alloc(DX_MAX_EDIT_CELLS*sizeof(*s->cells.cells)); s->cells.count=s->cells.capacity=DX_MAX_EDIT_CELLS;
    for (size_t i=0;i<s->cells.count;i++) s->cells.cells[i]=(SheetCell){i,0,dx_strdup("X")}; s->cells.bytes=2*s->cells.count;
    c=(CellEditCommand){CELL_EDIT_COMMAND_VERSION,prepare(&b,0,DX_MAX_EDIT_CELLS,0),"New",3}; before=b; sheet=*s;
    CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_LIMIT && error); free(error); error=NULL;
    CHECK(t.attempts==1 && t.cancelled==1 && !t.published && !memcmp(&before,&b,sizeof(b)) && !memcmp(&sheet,s,sizeof(sheet)));
    workbook_free(&b); workbook_init(&b); CHECK(workbook_add(&b,"Beyond rings",&(Analysis){0},&error)); t=(RecorderTrace){.book=&b};
    for (unsigned i=0;i<40;i++) {
        char text[32]; int n=snprintf(text,sizeof(text),"Value %u",i); c=(CellEditCommand){CELL_EDIT_COMMAND_VERSION,prepare(&b,0,1,1),text,(size_t)n};
        CHECK(workbook_apply_cell_edit_recorded(&b,&c,manual,&sink,&error).status==CELL_EDIT_APPLIED && !error); memset(text,'x',(size_t)n);
    }
    CHECK(t.attempts==40 && t.published==40 && !t.cancelled && b.sheets[0].undo_actions_count==32 && b.sheets[0].history_floor);
    CHECK(t.events[0].record.delta.after_bytes==7 && !memcmp(t.events[0].record.delta.after,"Value 0",7) && !memcmp(t.events[39].record.delta.before,"Value 38",8) && !memcmp(t.events[39].record.delta.after,"Value 39",8));
    workbook_free(&b);
}
static void owned_stream_admission(void) {
    Workbook b; workbook_init(&b); char *error=NULL; CHECK(workbook_add(&b,"Owned stream",&(Analysis){0},&error));
    CellEditTarget target=prepare(&b,0,1,1);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,99,VERSION_WORKING_HEAD},.edit_session=target.document,
        .command_revision=b.command_revision,.transition_high_water=100};
    WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){64,65536},&stream)==HISTORY_STREAM_OK);
    CellEditRecorder recorder=workbook_history_stream_cell_recorder(stream); WorkbookHistoryEvent *first=NULL;
    for (unsigned i=0;i<40;i++) {
        char text[32]; int n=snprintf(text,sizeof(text),"V%u",i); target=prepare(&b,0,1,1);
        CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,text,(size_t)n};
        CHECK(workbook_apply_cell_edit_recorded(&b,&command,manual,&recorder,&error).status==CELL_EDIT_APPLIED && !error);
        memset(text,'x',(size_t)n);
        if (!i) CHECK(workbook_history_stream_dispatch(stream,&first)==HISTORY_STREAM_OK && first);
    }
    WorkbookHistoryStreamState state=workbook_history_stream_state(stream);
    CHECK(state.pending==39 && state.in_flight && state.current.action_high_water==40 && state.current.transition_high_water==140 && state.current.cursor==40 && b.sheets[0].undo_actions_count==32);
    CellDelta value; const WorkbookHistoryEventInfo *info=workbook_history_event_info(first);
    CHECK(info->action==1 && info->transition==101 && info->revision==1 && cell_delta_decode(workbook_history_event_payload(first),info->bytes,&value)==CELL_DELTA_CODEC_OK);
    CHECK(!value.before_bytes && value.after_bytes==2 && !memcmp(value.after,"V0",2));
    /* Protocol-only mock acknowledgement after inspection, no durable IO claim. */
    CHECK(workbook_history_stream_complete(stream,&first)==HISTORY_STREAM_OK && !first);
    for (unsigned i=1;i<40;i++) {
        WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK);
        info=workbook_history_event_info(event); char before[32],after[32]; int nb=snprintf(before,sizeof(before),"V%u",i-1),na=snprintf(after,sizeof(after),"V%u",i);
        CHECK(info->action==i+1 && info->transition==101+i && info->revision==i+1 && cell_delta_decode(workbook_history_event_payload(event),info->bytes,&value)==CELL_DELTA_CODEC_OK);
        CHECK(value.sheet==target.sheet_id && value.row==1 && value.column==1 && value.before_bytes==(size_t)nb && value.after_bytes==(size_t)na && !memcmp(value.before,before,(size_t)nb) && !memcmp(value.after,after,(size_t)na));
        CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK && !event);
    }
    CHECK(!workbook_history_stream_state(stream).owned_bytes && workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK && !stream);
    workbook_free(&b);
}
static void command_sessions(void) {
    Workbook b; workbook_init(&b); char *error=NULL;
    CHECK(workbook_add(&b,"Read-only source",&(Analysis){.kind=SOURCE_CSV,.path="/offline/source.csv"},&error)); b.dirty=false;
    CHECK(!b.edit_session_id && !b.sheets[0].edit_session_id);
    CellEditTarget invalid={0};
    CHECK(!workbook_prepare_cell_edit(&b,0,DX_SHEET_ROWS,0,&invalid,&error) && error);
    CHECK(!b.edit_session_id && !b.sheets[0].edit_session_id && !b.dirty && !b.command_revision); free(error); error=NULL;
    CHECK(workbook_prepare_command_session(&b,0,&error) && !error);
    CHECK(b.edit_session_id && b.sheets[0].edit_session_id && !b.dirty && !b.command_revision && !b.sheets[0].undo_actions_count && !b.sheets[0].cells.revision && !b.lifecycle);
    Workbook prior=b; Worksheet sheet=b.sheets[0];
    CHECK(workbook_prepare_command_session(&b,0,&error) && !memcmp(&prior,&b,sizeof(b)) && !memcmp(&sheet,b.sheets,sizeof(sheet)));
    CHECK(!workbook_prepare_command_session(&b,1,&error) && error && !memcmp(&prior,&b,sizeof(b))); free(error); error=NULL;
    CellEditTarget target={0}; CHECK(!workbook_prepare_cell_edit(&b,0,DX_SHEET_ROWS,0,&target,&error) && !memcmp(&prior,&b,sizeof(b))); free(error);
    workbook_free(&b);
}
int main(void) {
    command_sessions(); basic_and_history(); identity_and_validation(); object_and_capacity(); pure_admission(); recorded_admission(); owned_stream_admission();
    printf("PASS: %u cell command checks\n",checks); return 0;
}
