#include "workbook/range_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook book(void) {
    Workbook b; workbook_init(&b); Analysis empty={0}; CHECK(workbook_add(&b,"A",&empty,NULL)); Worksheet *s=b.sheets; char *error=NULL;
    const struct { uint64_t row; size_t col; const char *text; } cells[]={{1,1,"NULL"},{3,3,"12345678901234567890.123456789"},{10,10,"Occupied"},{11,11,"Destination hole"}};
    for (size_t i=0;i<4;i++) { CHECK(cells_set(&s->cells,cells[i].row,cells[i].col,cells[i].text,&error)); worksheet_record(s,EDIT_CELLS); }
    CellFormat style=format_default(); style.flags=1; CHECK(format_apply(&s->formats,(FormatRange){1,3,1,3},FORMAT_BOLD,style,&error)); worksheet_record(s,EDIT_FORMAT);
    style.flags=0; style.color=2; CHECK(format_apply(&s->formats,(FormatRange){10,12,10,12},FORMAT_COLOR,style,&error)); worksheet_record(s,EDIT_FORMAT);
    b.dirty=false; free(error); return b;
}
static RangeCommand command(void) { return (RangeCommand){.range={1,3,1,3},.dr=9,.dc=9,.replace=true}; }
static RangeCommandContext context(Workbook *b,uint64_t *request,uint64_t *epoch) { return (RangeCommandContext){.command={b,request,SIZE_MAX,SIZE_MAX,true,false},.primary=&b->sheets[0].analysis,.primary_epoch=epoch}; }
static RangeCommandTarget *target(RangeCommandContext *c) { RangeCommandTarget *t=dx_alloc(sizeof(*t)); CHECK(workbook_prepare_range_command(c,0,t,NULL)); return t; }
static void reject(RangeCommandContext *c,const RangeCommandTarget *t,RangeCommand cmd,const WorkbookHistoryRangeRecorder *recorder,RangeCommandStatus expected) {
    Workbook b=*c->command.book; Worksheet sheet=b.sheets[0]; SheetCell cells[4]; memcpy(cells,sheet.cells.cells,sizeof(cells)); char *error=NULL;
    RangeCommandResult result=workbook_apply_range_command_recorded(c,t,cmd,recorder,&error);
    CHECK(result.status==expected && error && !memcmp(&b,c->command.book,sizeof(b)) && !memcmp(&sheet,b.sheets,sizeof(sheet)) && !memcmp(cells,b.sheets[0].cells.cells,sizeof(cells))); free(error);
}
static void plans(void) {
    Workbook b=book(); uint64_t request=7,epoch=2; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c); RangeCommand cmd=command(); RangeCommandPlanInfo info={0};
    CHECK(workbook_range_command_measure(&c,0,cmd,&info,NULL)==RANGE_COMMAND_READY && info.owned);
    c.command.plan_reserved=info.owned+info.validation_scratch-1; reject(&c,t,cmd,NULL,RANGE_COMMAND_LIMIT); c.command.plan_reserved++;
    c.command.read_only=true; reject(&c,t,cmd,NULL,RANGE_COMMAND_PROTECTED); c.command.read_only=false;
    cmd.replace=false; reject(&c,t,cmd,NULL,RANGE_COMMAND_OCCUPIED); cmd.replace=true;
#define STALE(field) do { t->field++; reject(&c,t,cmd,NULL,RANGE_COMMAND_STALE); t->field--; } while (0)
    STALE(owner.owner.document); STALE(owner.owner.sheet); STALE(owner.owner.expected_revision); STALE(owner.owner.expected_document_revision); STALE(owner.owner.workbook.low); STALE(owner.owner.sheet_id);
    STALE(owner.request_epoch); STALE(owner.format_revision); STALE(owner.history_last); STALE(primary_epoch); STALE(cells.capacity); STALE(entries[0].row); STALE(formats.count);
#undef STALE
    WorkbookHistoryRangeRecorder broken={0}; reject(&c,t,cmd,&broken,RANGE_COMMAND_INVALID);
    RangeCommand empty={.range={50,52,50,52},.dr=10,.dc=10}; c.command.plan_reserved=0;
    CHECK(workbook_apply_range_command_recorded(&c,t,empty,NULL,NULL).status==RANGE_COMMAND_NOOP && !b.dirty);
    request++; reject(&c,t,empty,NULL,RANGE_COMMAND_STALE); request--; c.command.plan_reserved=SIZE_MAX;
    CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_APPLIED);
    Worksheet *s=b.sheets; CHECK(b.command_revision==1 && b.dirty && s->cells.count==2 && !strcmp(cells_get(&s->cells,10,10),"NULL") && !*cells_get(&s->cells,11,11) && !*cells_get(&s->cells,1,1) &&
        !strcmp(cells_get(&s->cells,12,12),"12345678901234567890.123456789") && (format_at(&s->formats,10,10).flags&1) && !format_at(&s->formats,10,10).color && !format_at(&s->formats,1,1).flags && s->undo_actions[s->undo_actions_count-1]==EDIT_CELLS_FORMAT);
    CHECK(cells_history(&s->cells,false) && format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.command_revision++; b.dirty=false;
    CHECK(!strcmp(cells_get(&s->cells,10,10),"Occupied") && !strcmp(cells_get(&s->cells,11,11),"Destination hole") && format_at(&s->formats,10,10).color==2 && s->redo_actions_count==1);
    free(t); t=target(&c); cmd.dr=cmd.dc=0; c.command.plan_reserved=0;
    CHECK(workbook_apply_range_command_recorded(&c,t,cmd,&broken,NULL).status==RANGE_COMMAND_NOOP && !b.dirty && s->redo_actions_count==1 && b.command_revision==2);
    c.command.plan_reserved=SIZE_MAX; cmd=command(); CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_APPLIED && !s->redo_actions_count);
    free(t); workbook_free(&b);
}
typedef struct { RangeCommandContext *context; unsigned mode,reserves,publishes,cancels; } Probe;
static bool reserve(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    (void)error; Probe *p=context; p->reserves++; Workbook *b=p->context->command.book; Worksheet *s=b->sheets;
    CHECK(!*ticket && record->view.effects==10 && record->view.count==5 && !b->dirty && !b->command_revision && s->cells.count==4 && s->undo_actions_count==6);
    CHECK(record->view.cells[0].row==1 && record->view.cells[0].before_bytes==4 && !record->view.cells[0].after_bytes);
    if (p->mode==1) return false; if (p->mode==2) return true; *ticket=dx_alloc(1);
    if (p->mode==3) (*(uint64_t *)p->context->command.request_epoch)++;
    if (p->mode==4) s->cells.cells[0].row++;
    if (p->mode==5) return false;
    if (p->mode==6) p->context->command.read_only=true;
    if (p->mode==7) s->formats.items[0].value.flags^=1;
    if (p->mode==8) b->command_revision++;
    if (p->mode==9) (*(uint64_t *)p->context->primary_epoch)++;
    return true;
}
static void publish(void *context,void *ticket) { Probe *p=context; p->publishes++; CHECK(ticket && p->context->command.book->command_revision==1 && p->context->command.book->dirty); free(ticket); }
static void cancel(void *context,void *ticket) { Probe *p=context; p->cancels++; CHECK(ticket); free(ticket); }
static void callbacks(void) {
    for (unsigned mode=0;mode<=9;mode++) {
        Workbook b=book(); uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c); Probe p={&c,mode,0,0,0}; WorkbookHistoryRangeRecorder recorder={reserve,publish,cancel,&p};
        RangeCommandResult r=workbook_apply_range_command_recorded(&c,t,command(),&recorder,NULL); CHECK(p.reserves==1);
        if (!mode) CHECK(r.status==RANGE_COMMAND_APPLIED && p.publishes==1 && !p.cancels);
        else CHECK(r.status==(mode==1 ? RANGE_COMMAND_LIMIT : mode==2 || mode==5 ? RANGE_COMMAND_INVALID : mode==6 ? RANGE_COMMAND_PROTECTED : RANGE_COMMAND_STALE) &&
            !p.publishes && p.cancels==(mode>=3 ? 1u : 0u) && !b.dirty && b.sheets[0].cells.count==4 && b.sheets[0].undo_actions_count==6);
        free(t); workbook_free(&b);
    }
}
static void stream(void) {
    Workbook b=book(); uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue); CHECK(workbook_apply_range_command_recorded(&c,t,command(),&recorder,NULL).status==RANGE_COMMAND_APPLIED);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(info->family==4 && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==10 && event.count==5 && event.formats.before.count==2);
    CHECK(workbook_history_stream_retry(queue,&held)==HISTORY_STREAM_OK && workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK);
    free(t); t=target(&c); RangeCommand reverse={.range={10,12,10,12},.dr=-9,.dc=-9};
    CHECK(workbook_apply_range_command_recorded(&c,t,reverse,&recorder,NULL).status==RANGE_COMMAND_LIMIT && b.command_revision==1 && b.sheets[0].cells.count==2);
    CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_apply_range_command_recorded(&c,t,reverse,&recorder,NULL).status==RANGE_COMMAND_APPLIED && b.command_revision==2);
    CHECK(!memcmp(event.cells[0].before,"NULL",4)); range_event_free(&event);
    CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK && workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK);
    free(t); workbook_free(&b);
}
static void effects_and_noop(void) {
    for (unsigned mode=0;mode<3;mode++) {
        Workbook b; workbook_init(&b); Analysis empty={0}; CHECK(workbook_add(&b,"Manual",&empty,NULL)); Worksheet *s=b.sheets;
        if (mode==1) { CHECK(cells_set(&s->cells,1,1,"X",NULL)); worksheet_record(s,EDIT_CELLS); }
        if (mode==2) { CellFormat style=format_default(); style.flags=1; CHECK(format_apply(&s->formats,(FormatRange){1,3,1,3},FORMAT_BOLD,style,NULL)); worksheet_record(s,EDIT_FORMAT); }
        b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        if (!mode) c.command.plan_reserved=c.command.validation_reserved=0;
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command(),NULL,NULL);
        CHECK(result.status==(!mode ? RANGE_COMMAND_NOOP : RANGE_COMMAND_APPLIED) && result.effects==(!mode ? 0u : mode==1 ? 2u : 8u));
        if (!mode) CHECK(!b.command_revision && !b.dirty && !s->undo_actions_count);
        else CHECK(s->undo_actions[s->undo_actions_count-1]==(mode==1 ? EDIT_CELLS : EDIT_FORMAT));
        free(t); workbook_free(&b);
    }
}
static void rows(void) {
    Workbook b=book(); uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    RangeCommand insert={.operation=RANGE_OPERATION_INSERT_ROWS,.row=2,.row_count=2};
    WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id}; WorkbookHistoryStream *queue=NULL;
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){8,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
    CHECK(workbook_apply_range_command_recorded(&c,t,insert,&recorder,NULL).status==RANGE_COMMAND_APPLIED && b.command_revision==1 && !strcmp(cells_get(&b.sheets[0].cells,5,3),"12345678901234567890.123456789") && !strcmp(cells_get(&b.sheets[0].cells,12,10),"Occupied"));
    CHECK(format_at(&b.sheets[0].formats,5,3).flags&1);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==10 && event.count==6); range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    CHECK(cells_history(&b.sheets[0].cells,false) && format_history(&b.sheets[0].formats,false)); worksheet_transfer_history(b.sheets,false); b.command_revision++;
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,3,3),"12345678901234567890.123456789") && !strcmp(cells_get(&b.sheets[0].cells,10,10),"Occupied"));
    free(t); t=target(&c); RangeCommand deletion={.operation=RANGE_OPERATION_DELETE_ROWS,.row=1,.row_count=3};
    CHECK(workbook_apply_range_command_recorded(&c,t,deletion,NULL,NULL).status==RANGE_COMMAND_APPLIED && b.sheets[0].cells.count==2 && !strcmp(cells_get(&b.sheets[0].cells,7,10),"Occupied") && !format_at(&b.sheets[0].formats,1,1).flags);
    CHECK(cells_history(&b.sheets[0].cells,false) && format_history(&b.sheets[0].formats,false)); worksheet_transfer_history(b.sheets,false); b.command_revision++;
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"NULL") && !strcmp(cells_get(&b.sheets[0].cells,3,3),"12345678901234567890.123456789"));
    free(t); t=target(&c); deletion.row_count=0; reject(&c,t,deletion,NULL,RANGE_COMMAND_INVALID); free(t); CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); workbook_free(&b);
    workbook_init(&b); Analysis empty={0}; CHECK(workbook_add(&b,"Manual",&empty,NULL)); CellFormat style=format_default(); style.flags=1;
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){0,DX_SHEET_ROWS-1,2,2},FORMAT_BOLD,style,NULL)); worksheet_record(b.sheets,EDIT_FORMAT); b.dirty=false;
    c=context(&b,&request,&epoch); c.command.plan_reserved=c.command.validation_reserved=0; t=target(&c);
    CHECK(workbook_apply_range_command_recorded(&c,t,insert,NULL,NULL).status==RANGE_COMMAND_NOOP && !b.command_revision && !b.dirty && b.sheets[0].undo_actions_count==1);
    free(t); workbook_free(&b);
}
static void columns_compound(void) {
    const unsigned effects[]={2,8,10,16,18,24,26};
    for (size_t mode=0;mode<sizeof(effects)/sizeof(*effects);mode++) {
        unsigned expected=effects[mode]; Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Columns",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
        s->view.column_count=16384; s->view.widths=dx_alloc(16384*sizeof(float)); s->view.columns=dx_alloc(16384*sizeof(char *));
        for (size_t i=0;i<16384;i++) { s->view.widths[i]=96; s->view.columns[i]=dx_strdup(""); }
        if (expected&16) { s->view.widths[1]=123; s->view.widths[16383]=333; }
        if (expected&2) { CHECK(cells_set(&s->cells,1,1,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); CHECK(cells_set(&s->cells,2,16382,"9007199254740993.0123456789",NULL)); worksheet_record(s,EDIT_CELLS); }
        if (expected&8) { CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){1,2,1,1},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); }
        b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        RangeCommand command={.operation=RANGE_OPERATION_INSERT_COLUMNS,.column=0,.column_count=1}; RangeCommandPlanInfo planned={0};
        CHECK(workbook_range_command_measure(&c,0,command,&planned,NULL)==RANGE_COMMAND_READY);
        s->view.widths[0]=97;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE && !b.command_revision && !s->width_revision);
        s->view.widths[0]=96;
        c.command.plan_reserved=planned.owned+planned.validation_scratch-1;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && !b.dirty && !b.command_revision && !s->width_revision);
        c.command.plan_reserved=SIZE_MAX;
        WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){4,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
        WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
        CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==expected && b.command_revision==1 && b.dirty && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==expected);
        if (expected&16) CHECK(s->view.widths[1]==96 && s->view.widths[2]==123 && s->view.widths[16383]==96 && s->width_revision==1);
        if (expected&2) CHECK(!*cells_get(&s->cells,1,1) && !strcmp(cells_get(&s->cells,1,2),"NULL") && !strcmp(cells_get(&s->cells,2,16383),"9007199254740993.0123456789"));
        if (expected&8) CHECK(format_at(&s->formats,1,2).flags&FORMAT_BOLD);
        WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); RangeEvent event={0};
        const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
        CHECK(info->encoding==DX_RANGE_EVENT_ENCODING && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==expected);
        if (expected&16) CHECK(event.widths.before.entries[1].column==16383 && event.widths.before.entries[1].bits==column_width_bits(333));
        range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
        CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED);
        if (expected&16) CHECK(s->view.widths[1]==123 && s->view.widths[16383]==333 && s->width_revision==2);
        if (expected&2) CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,2,16382),"9007199254740993.0123456789"));
        if (expected&8) CHECK(format_at(&s->formats,1,1).flags&FORMAT_BOLD);
        CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED);
        if (expected&16) CHECK(s->view.widths[2]==123 && s->view.widths[16383]==96 && s->width_revision==3);
        free(t); t=target(&c); command.operation=RANGE_OPERATION_DELETE_COLUMNS; command.column=2;
        uint64_t cell_revision=s->cells.revision,format_revision=s->formats.revision,width_revision=s->width_revision; size_t actions=s->undo_actions_count;
        /* Recorder is still at revision1 while Undo/Redo advanced the model.
           Rejection occurs before any constituent or chronology can adopt. */
        CHECK(workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL).status==RANGE_COMMAND_LIMIT && b.command_revision==3 && s->cells.revision==cell_revision && s->formats.revision==format_revision && s->width_revision==width_revision && s->undo_actions_count==actions);
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_APPLIED);
        if (expected&2) CHECK(!*cells_get(&s->cells,1,2));
        CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED);
        if (expected&16) CHECK(s->view.widths[2]==123);
        CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); workbook_free(&b);
    }
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Empty",&(Analysis){0},NULL)); b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    RangeCommand command={.operation=RANGE_OPERATION_INSERT_COLUMNS,.column=0,.column_count=1}; c.command.plan_reserved=c.command.validation_reserved=0;
    CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_NOOP && !b.dirty && !b.command_revision);
    request++; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); request--;
    b.sheets[0].width_revision++; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); b.sheets[0].width_revision--;
    free(t); workbook_free(&b);
}
typedef struct { RangeCommandContext *context; char *text; bool stale,deny; unsigned cancel,publish; } PasteProbe;
static bool paste_reserve(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    (void)error; PasteProbe *p=context; CHECK(record->view.effects==2 && record->view.count && !record->view.widths.before.count && !record->view.before_formats.count);
    if (p->deny) return false;
    *ticket=dx_alloc(1); memset(p->text,'Z',strlen(p->text));
    if (p->stale) (*(uint64_t *)p->context->command.request_epoch)++;
    return true;
}
static void paste_publish(void *context,void *ticket) { PasteProbe *p=context; p->publish++; free(ticket); }
static void paste_cancel(void *context,void *ticket) { PasteProbe *p=context; p->cancel++; free(ticket); }
static void paste_compound(void) {
    for (unsigned mode=0;mode<3;mode++) {
        Workbook b=book(); uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c); Worksheet *s=b.sheets;
        char *text=dx_strdup("New\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789\n");
        RangeCommand command={.operation=RANGE_OPERATION_PASTE,.row=1,.column=1,.text=text};
        RangeCommandPlanInfo info={0}; CHECK(workbook_range_command_measure(&c,0,command,&info,NULL)==RANGE_COMMAND_READY && !info.validation_scratch && info.rows==2 && info.columns==2);
        FormatStore formats=s->formats; CellStore cells=s->cells; size_t actions=s->undo_actions_count;
        c.command.plan_reserved=info.owned-1;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && !memcmp(&cells,&s->cells,sizeof(cells)) && !memcmp(&formats,&s->formats,sizeof(formats)));
        c.command.plan_reserved=SIZE_MAX;
        PasteProbe probe={&c,text,mode==1,mode==2,0,0}; WorkbookHistoryRangeRecorder recorder={paste_reserve,paste_publish,paste_cancel,&probe};
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
        if (!mode) {
            CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==2 && result.rows==2 && result.columns==2 && probe.publish==1 && !probe.cancel);
            CHECK(!strcmp(cells_get(&s->cells,1,1),"New") && !*cells_get(&s->cells,1,2) && !strcmp(cells_get(&s->cells,2,1),"two\tparts") && !strcmp(cells_get(&s->cells,2,2),"9007199254740993.0123456789"));
            CHECK(!memcmp(&formats,&s->formats,sizeof(formats)) && !s->width_revision && s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_CELLS);
            CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,1,1),"NULL") && !*cells_get(&s->cells,2,1));
            free(t); t=target(&c); command.text="NULL"; c.command.plan_reserved=c.command.validation_reserved=0; size_t redo=s->redo_actions_count; uint64_t revision=b.command_revision;
            CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==redo && b.command_revision==revision);
            request++; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); request--;
            CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,2,1),"two\tparts"));
        } else CHECK(result.status==(mode==1 ? RANGE_COMMAND_STALE : RANGE_COMMAND_LIMIT) && !probe.publish && probe.cancel==(mode==1) && !b.command_revision && !b.dirty && !memcmp(&cells,&s->cells,sizeof(cells)) && !memcmp(&formats,&s->formats,sizeof(formats)) && s->undo_actions_count==actions);
        free(text); free(t); workbook_free(&b);
    }
    Workbook b=book(); uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    RangeCommand command={.operation=RANGE_OPERATION_PASTE,.row=0,.column=0,.text="Good\t\"unterminated"};
    CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_INVALID && !b.command_revision && !b.dirty);
    b.sheets[0].analysis.kind=SOURCE_DEMO; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); b.sheets[0].analysis.kind=SOURCE_NONE;
    free(t); workbook_free(&b);
    b=book(); c=context(&b,&request,&epoch); t=target(&c);
    char *owned=dx_strdup("New\tx"); command=(RangeCommand){.operation=RANGE_OPERATION_PASTE,.row=1,.column=1,.text=owned};
    WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
    CHECK(workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL).status==RANGE_COMMAND_APPLIED); free(owned);
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(info->encoding==DX_RANGE_EVENT_ENCODING && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==2 && event.count==2);
    CHECK(event.cells[0].before_bytes==4 && !memcmp(event.cells[0].before,"NULL",4) && event.cells[0].after_bytes==3 && !memcmp(event.cells[0].after,"New",3) && !event.cells[1].before_bytes && event.cells[1].after_bytes==1 && event.cells[1].after[0]=='x');
    range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK && workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK);
    free(t); workbook_free(&b);
}
static void mixed_commands(void) {
    for (unsigned mode=0;mode<4;mode++) {
        Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Mixed",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
        CHECK(objects_add(&s->objects,2,2,"Template",NULL)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        if (mode&1) { CHECK(cells_set(&s->cells,1,1,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); }
        if (mode&2) { CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){1,4,1,3},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); }
        b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        RangeCommand command={.range={1,5,1,4},.dr=10,.dc=10}; RangeCommandPlanInfo measured={0};
        CHECK(workbook_range_command_measure(&c,0,command,&measured,NULL)==RANGE_COMMAND_READY);
        c.command.plan_reserved=measured.owned+measured.validation_scratch-1;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && s->objects.items[0].row==2 && !b.dirty && !b.command_revision);
        c.command.plan_reserved=SIZE_MAX;
        WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){4,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
        unsigned effects=4u|((mode&1)?2u:0u)|((mode&2)?8u:0u); uint64_t high=s->objects.next_id;
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
        CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==effects && b.command_revision==1 && s->objects.items[0].row==12 && s->objects.items[0].col==12 && s->objects.next_id==high && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==effects);
        if (mode&1) CHECK(!strcmp(cells_get(&s->cells,11,11),"NULL") && !*cells_get(&s->cells,1,1));
        if (mode&2) CHECK(format_at(&s->formats,11,11).flags&FORMAT_BOLD);
        WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); RangeEvent event={0}; const WorkbookHistoryEventInfo *info=workbook_history_event_info(held);
        CHECK(info->encoding==DX_RANGE_EVENT_ENCODING && range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==effects && event.definitions.after_objects.objects.items[0].row==12);
        range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
        CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_DELEGATE);
        if (effects&2) CHECK(cells_history(&s->cells,false)); CHECK(objects_history(&s->objects,false)); if (effects&8) CHECK(format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.command_revision++;
        CHECK(s->objects.items[0].row==2 && s->objects.items[0].col==2 && s->objects.next_id==high);
        if (mode&1) CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL"));
        if (mode&2) CHECK(format_at(&s->formats,1,1).flags&FORMAT_BOLD);
        free(t); t=target(&c); command.dr=command.dc=0; c.command.plan_reserved=c.command.validation_reserved=0; size_t redo=s->redo_actions_count; uint64_t revision=b.command_revision;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==redo && b.command_revision==revision);
        request++; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); request--;
        if (effects&2) CHECK(cells_history(&s->cells,true)); CHECK(objects_history(&s->objects,true)); if (effects&8) CHECK(format_history(&s->formats,true)); worksheet_transfer_history(s,true); b.command_revision++;
        CHECK(s->objects.items[0].row==12 && s->objects.items[0].col==12 && s->objects.next_id==high);
        free(t); t=target(&c); command=(RangeCommand){.operation=RANGE_OPERATION_PASTE,.row=12,.column=12,.text="Protected"};
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_PROTECTED);
        CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); workbook_free(&b);
    }
}
static void clear_command_case(bool templates) {
    Workbook b=book(); Worksheet *s=b.sheets;
    if (templates) { CHECK(objects_add(&s->objects,2,2,"Clear retains painted result",NULL)==0); s->objects.items[0].rows=s->objects.items[0].columns=1; worksheet_record(s,EDIT_OBJECTS); }
    ObjectStore objects=s->objects; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    RangeCommand command={.operation=RANGE_OPERATION_CLEAR,.range={1,3,1,3}}; RangeCommandPlanInfo measured={0};
    CHECK(workbook_range_command_measure(&c,0,command,&measured,NULL)==RANGE_COMMAND_READY && measured.owned);
    c.command.plan_reserved=measured.owned-1; FormatStore formats=s->formats; CellStore cells=s->cells;
    CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && !memcmp(&cells,&s->cells,sizeof(cells)) && !memcmp(&formats,&s->formats,sizeof(formats)));
    c.command.plan_reserved=SIZE_MAX;
    WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
    RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
    CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==2 && !result.rows && !result.columns && b.command_revision==1 && s->cells.count==2 && !*cells_get(&s->cells,1,1) && !*cells_get(&s->cells,3,3));
    CHECK(!memcmp(&formats,&s->formats,sizeof(formats)) && (format_at(&s->formats,1,1).flags&FORMAT_BOLD) && !s->width_revision && !memcmp(&objects,&s->objects,sizeof(objects)));
    WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
    CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==2 && event.count==2 && event.cells[0].before_bytes==4 && !memcmp(event.cells[0].before,"NULL",4) && !event.cells[0].after_bytes && !event.cells[1].after_bytes);
    range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,3,3),"12345678901234567890.123456789"));
    free(t); t=target(&c); command.range=(SheetRange){100,200,100,200}; c.command.plan_reserved=c.command.validation_reserved=0; size_t redo=s->redo_actions_count; uint64_t revision=b.command_revision;
    CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==redo && b.command_revision==revision && !memcmp(&formats,&s->formats,sizeof(formats)));
    request++; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE); request--;
    c.command.plan_reserved=SIZE_MAX; command.range=(SheetRange){1,3,1,3};
    CHECK(workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL).status==RANGE_COMMAND_LIMIT && s->redo_actions_count==redo && b.command_revision==revision && !strcmp(cells_get(&s->cells,1,1),"NULL"));
    command.range.r1=UINT64_MAX; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_INVALID);
    CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); workbook_free(&b);
}
static void mixed_rows_command(void) {
    for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Rows",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
        CHECK(objects_add(&s->objects,5,2,"Template",NULL)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        CHECK(cells_set(&s->cells,1,1,"9007199254740993.0123456789",NULL)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){1,6,1,3},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT);
        b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        RangeCommand command={.operation=deletion ? RANGE_OPERATION_DELETE_ROWS : RANGE_OPERATION_INSERT_ROWS,.row=0,.row_count=2};
        WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
        uint64_t high=s->objects.next_id; RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
        CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==14 && b.command_revision==1 && s->objects.items[0].row==(deletion ? 3u : 7u) && s->objects.next_id==high && !s->width_revision);
        CHECK(deletion ? !s->cells.count : !strcmp(cells_get(&s->cells,3,1),"9007199254740993.0123456789"));
        WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
        CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==14 && event.definitions.before_objects.objects.items[0].row==5 && event.definitions.after_objects.objects.items[0].row==(deletion ? 3u : 7u));
        range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
        CHECK(cells_history(&s->cells,false) && objects_history(&s->objects,false) && format_history(&s->formats,false)); worksheet_transfer_history(s,false); b.command_revision++;
        CHECK(s->objects.items[0].row==5 && !strcmp(cells_get(&s->cells,1,1),"9007199254740993.0123456789") && (format_at(&s->formats,1,1).flags&FORMAT_BOLD) && s->objects.next_id==high && !s->width_revision);
        free(t); t=target(&c); command.row=1000; command.row_count=1; c.command.plan_reserved=c.command.validation_reserved=0; size_t redo=s->redo_actions_count;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==redo && b.command_revision==2);
        c.command.plan_reserved=c.command.validation_reserved=SIZE_MAX; command.row=6;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_INVALID && s->objects.items[0].row==5 && s->redo_actions_count==redo && b.command_revision==2);
        CHECK(cells_history(&s->cells,true) && objects_history(&s->objects,true) && format_history(&s->formats,true)); worksheet_transfer_history(s,true); b.command_revision++;
        CHECK(s->objects.items[0].row==(deletion ? 3u : 7u) && s->objects.next_id==high && !s->width_revision);
        CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); workbook_free(&b);
    }
}
typedef struct { PasteProbe paste; bool object_stale; } TemplatePasteProbe;
static bool template_paste_reserve(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    TemplatePasteProbe *p=context; bool reserved=paste_reserve(&p->paste,record,ticket,error);
    CHECK(!record->view.definitions.effects);
    if (reserved && p->object_stale) p->paste.context->command.book->sheets[0].objects.items[0].col++;
    return reserved;
}
static void template_paste_command(void) {
    for (unsigned mode=0;mode<4;mode++) {
        Workbook b=book(); Worksheet *s=b.sheets; CHECK(objects_add(&s->objects,30,30,"Protected template",NULL)==0); worksheet_record(s,EDIT_OBJECTS);
        uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        char *text=dx_strdup("New\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789\n"); RangeCommand cmd={.operation=RANGE_OPERATION_PASTE,.row=1,.column=1,.text=text};
        RangeCommandPlanInfo info={0}; CHECK(workbook_range_command_measure(&c,0,cmd,&info,NULL)==RANGE_COMMAND_READY && info.rows==2 && info.columns==2);
        CellStore before=s->cells; ObjectStore objects=s->objects; FormatStore formats=s->formats; size_t actions=s->undo_actions_count;
        c.command.plan_reserved=info.owned+info.validation_scratch-1;
        CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_LIMIT && !b.command_revision && !memcmp(&before,&s->cells,sizeof(before)));
        c.command.plan_reserved=SIZE_MAX;
        TemplatePasteProbe probe={{&c,text,mode==1,mode==2,0,0},mode==3}; WorkbookHistoryRangeRecorder recorder={template_paste_reserve,paste_publish,paste_cancel,&probe};
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,cmd,&recorder,NULL);
        CHECK(!memcmp(&objects,&s->objects,sizeof(objects)) && !memcmp(&formats,&s->formats,sizeof(formats)) && !s->width_revision);
        if (!mode) {
            CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==2 && result.rows==2 && result.columns==2 && probe.paste.publish==1 && !probe.paste.cancel && b.command_revision==1);
            CHECK(!strcmp(cells_get(&s->cells,1,1),"New") && !strcmp(cells_get(&s->cells,2,2),"9007199254740993.0123456789") && s->undo_actions_count==actions+1 && s->undo_actions[actions]==EDIT_CELLS);
            CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,1,1),"NULL"));
            free(t); t=target(&c); cmd.text="NULL"; c.command.plan_reserved=c.command.validation_reserved=0;
            CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==1 && b.command_revision==2);
            request++; CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_STALE); request--;
            CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,2,1),"two\tparts"));
        } else CHECK(result.status==(mode==2 ? RANGE_COMMAND_LIMIT : RANGE_COMMAND_STALE) && !probe.paste.publish && probe.paste.cancel==(mode!=2) && !b.command_revision && !b.dirty && !memcmp(&before,&s->cells,sizeof(before)) && s->undo_actions_count==actions);
        free(t); free(text); workbook_free(&b);
    }
    Workbook b=book(); Worksheet *s=b.sheets; CHECK(objects_add(&s->objects,100,2,"Ragged guard",NULL)==0); worksheet_record(s,EDIT_OBJECTS);
    uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
    RangeCommand cmd={.operation=RANGE_OPERATION_PASTE,.row=98,.column=1,.text="A\tB\tC\nD\nE"};
    CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_PROTECTED && !b.command_revision && !b.dirty);
    cmd.row=100; cmd.column=2; cmd.text=""; CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_PROTECTED);
    free(t); workbook_free(&b);
}
typedef struct { PasteProbe paste; unsigned mode; size_t other_columns; RangeSourcePolicy other_policy; } SourcePasteProbe;
static bool source_paste_reserve(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    SourcePasteProbe *p=context; bool ok=paste_reserve(&p->paste,record,ticket,error); RangeCommandContext *c=p->paste.context;
    if (!ok) return false;
    switch (p->mode) {
    case 1: (*(uint64_t *)c->source->rows)++; break;
    case 2: (*(size_t *)c->source->columns)++; break;
    case 3: (*(bool *)c->source->count_pending)=true; break;
    case 4: (*(bool *)c->source->fetch_failed)=true; break;
    case 5: (*(bool *)c->source->ready)=false; break;
    case 6: (*(uint64_t *)c->primary_epoch)++; break;
    case 7: c->command.book->sheets[0].source_versions.high_water++; break;
    case 8: c->command.book->sheets[0].analysis.source_version++; break;
    case 9: c->command.book->sheets[0].formats.scopes[0].owner++; break;
    case 10: p->other_columns=*c->source->columns; ((RangeSourcePolicy *)c->source)->columns=&p->other_columns; break;
    case 11: c->source=NULL; break;
    case 12: p->other_policy=*c->source; c->source=&p->other_policy; break;
    }
    return true;
}
static void source_cells_command(void) {
    for (unsigned mode=0;mode<=12;mode++) {
        Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Actual source",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
        Analysis primary={.kind=SOURCE_DEMO}; CHECK(objects_add_table(&s->objects,30,8,"Protected Table",&primary,NULL)==0); worksheet_record(s,EDIT_OBJECTS);
        CHECK(cells_set(&s->cells,1,4,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); s->formats.source_aware=true;
        s->formats.scope_count=1; s->formats.scopes=dx_alloc(sizeof(*s->formats.scopes));
        s->formats.scopes[0]=(FormatScope){0,dx_strdup("opaque-result"),13}; b.dirty=false;
        uint64_t request=1,epoch=1,rows=10; size_t columns=3; bool pending=false,failed=false,ready=true;
        RangeSourcePolicy policy={&rows,&columns,&pending,&failed,&ready}; RangeCommandContext c=context(&b,&request,&epoch); c.primary=&primary; c.source=&policy;
        RangeCommandTarget *t=target(&c); char *text=dx_strdup("New\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789");
        RangeCommand cmd={.operation=RANGE_OPERATION_PASTE,.row=1,.column=4,.text=text}; RangeCommandPlanInfo info={0};
        CHECK(workbook_range_command_measure(&c,0,cmd,&info,NULL)==RANGE_COMMAND_READY && info.rows==2 && info.columns==2 && info.validation_scratch);
        CellStore cells=s->cells; ObjectStore objects=s->objects; FormatStore formats=s->formats;
        c.command.plan_reserved=info.owned+info.validation_scratch-1; CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_LIMIT && !b.command_revision); c.command.plan_reserved=SIZE_MAX;
        SourcePasteProbe probe={.paste={&c,text,false,false,0,0},.mode=mode}; WorkbookHistoryRangeRecorder recorder={source_paste_reserve,paste_publish,paste_cancel,&probe};
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,cmd,&recorder,NULL);
        CHECK(!memcmp(&objects,&s->objects,sizeof(objects)) && !memcmp(&formats,&s->formats,sizeof(formats)) && !s->width_revision);
        if (mode) CHECK(result.status==RANGE_COMMAND_STALE && probe.paste.cancel==1 && !probe.paste.publish && !b.command_revision && !b.dirty && !memcmp(&cells,&s->cells,sizeof(cells)));
        else {
            CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==2 && probe.paste.publish==1 && !probe.paste.cancel && !strcmp(cells_get(&s->cells,1,4),"New") && !strcmp(cells_get(&s->cells,2,5),"9007199254740993.0123456789"));
            CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && !strcmp(cells_get(&s->cells,1,4),"NULL")); free(t); t=target(&c);
            cmd.text="NULL"; c.command.plan_reserved=c.command.validation_reserved=0;
            CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==1 && b.command_revision==2);
            rows++; CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_STALE); rows--;
            c.command.plan_reserved=c.command.validation_reserved=SIZE_MAX; cmd=(RangeCommand){.operation=RANGE_OPERATION_CLEAR,.range={0,100,0,20}};
            CHECK(workbook_apply_range_command_recorded(&c,t,cmd,NULL,NULL).status==RANGE_COMMAND_APPLIED && !s->cells.count && b.command_revision==3 && !memcmp(&objects,&s->objects,sizeof(objects)));
            free(t); t=target(&c); CHECK(workbook_apply_range_command_recorded(&c,t,(RangeCommand){.operation=RANGE_OPERATION_INSERT_COLUMNS,.column_count=1},NULL,NULL).status==RANGE_COMMAND_PROTECTED && b.command_revision==3);
        }
        if (mode==8) s->analysis.source_version=0;
        free(text); free(t); workbook_free(&b);
    }
}
static void mixed_history_transfer(Worksheet *s,bool redo) {
    EditKind kind=(redo ? s->redo_actions : s->undo_actions)[(redo ? s->redo_actions_count : s->undo_actions_count)-1]; unsigned effects=worksheet_edit_effects(kind);
    ColumnWidthCommandContext wc={&s->view,&s->width_history,&s->width_revision,SIZE_MAX}; ColumnWidthCommand *widths=NULL;
    if (effects&16) CHECK(column_width_command_replay(wc,redo,&widths)==COLUMN_WIDTH_OK);
    if (widths) CHECK(column_width_command_adopt(wc,&widths)==COLUMN_WIDTH_OK);
    if (effects&2) CHECK(cells_history(&s->cells,redo));
    if (effects&4) CHECK(objects_history(&s->objects,redo));
    if (effects&8) CHECK(format_history(&s->formats,redo));
    worksheet_transfer_history(s,redo);
}
static void mixed_columns_command(void) {
    for (unsigned mode=0;mode<8;mode++) for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Columns",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
        CHECK(objects_add(&s->objects,5,2,"Template",NULL)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        if (mode&1) { CHECK(cells_set(&s->cells,1,1,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); CHECK(cells_set(&s->cells,12,16382,"9007199254740993.0123456789",NULL)); worksheet_record(s,EDIT_CELLS); }
        if (mode&2) { CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){1,12,1,3},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); }
        s->view.column_count=16384; s->view.widths=dx_alloc(16384*sizeof(float)); s->view.columns=dx_alloc(16384*sizeof(char *));
        for (size_t i=0;i<16384;i++) { s->view.widths[i]=96; s->view.columns[i]=dx_strdup(i==1 ? "Fixed" : ""); }
        if (mode&4) { s->view.widths[1]=123; s->view.widths[16383]=333; }
        b.dirty=false; uint64_t request=1,epoch=1; RangeCommandContext c=context(&b,&request,&epoch); RangeCommandTarget *t=target(&c);
        RangeCommand command={.operation=deletion ? RANGE_OPERATION_DELETE_COLUMNS : RANGE_OPERATION_INSERT_COLUMNS,.column=0,.column_count=1}; RangeCommandPlanInfo measured={0};
        CHECK(workbook_range_command_measure(&c,0,command,&measured,NULL)==RANGE_COMMAND_READY);
        s->view.widths[0]=97; CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_STALE && !b.command_revision); s->view.widths[0]=96;
        c.command.plan_reserved=measured.owned+measured.validation_scratch-1;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && !b.command_revision && !s->width_revision && s->objects.items[0].col==2);
        c.command.plan_reserved=SIZE_MAX;
        if (mode&4) { free(t); s->width_revision=UINT64_MAX; t=target(&c); CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_LIMIT && !b.command_revision); s->width_revision=0; free(t); t=target(&c); }
        WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={b.id.high,b.id.low,9,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id};
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(queue);
        unsigned effects=4u|((mode&1)?2u:0u)|((mode&2)?8u:0u)|((mode&4)?16u:0u); uint64_t high=s->objects.next_id;
        RangeCommandResult result=workbook_apply_range_command_recorded(&c,t,command,&recorder,NULL);
        CHECK(result.status==RANGE_COMMAND_APPLIED && result.effects==effects && b.command_revision==1 && s->objects.items[0].col==(deletion ? 1u : 3u) && s->objects.next_id==high && !strcmp(s->view.columns[1],"Fixed"));
        if (mode&4) CHECK(s->view.widths[deletion ? 0 : 2]==123 && s->view.widths[16383]==96 && s->width_revision==1);
        WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
        CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==effects && event.definitions.before_objects.objects.items[0].col==2 && event.definitions.after_objects.objects.items[0].col==(deletion ? 1u : 3u));
        if (mode&4) CHECK(event.widths.before.entries[1].column==16383 && event.widths.before.entries[1].bits==column_width_bits(333));
        range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
        mixed_history_transfer(s,false); b.command_revision++;
        CHECK(s->objects.items[0].col==2 && s->objects.next_id==high && !strcmp(s->view.columns[1],"Fixed"));
        if (mode&4) CHECK(s->view.widths[1]==123 && s->view.widths[16383]==333 && s->width_revision==2);
        if (mode&1) CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,12,16382),"9007199254740993.0123456789"));
        mixed_history_transfer(s,true); b.command_revision++;
        CHECK(s->objects.items[0].col==(deletion ? 1u : 3u) && s->objects.next_id==high);
        if (mode&4) CHECK(s->view.widths[deletion ? 0 : 2]==123 && s->view.widths[16383]==96 && s->width_revision==3);
        free(t); t=target(&c); command.column=deletion ? 1 : 4;
        CHECK(workbook_apply_range_command_recorded(&c,t,command,NULL,NULL).status==RANGE_COMMAND_INVALID && b.command_revision==3);
        CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); workbook_free(&b);
    }
}
typedef struct {
    Workbook b; Analysis primary; Page primary_page,table_page,alternate,pivot_page;
    uint64_t pivot_id,pivot_epoch,pivot_source_epoch; bool pivot_ready,pivot_failed; const void *pivot_owner;
    Page *live_primary,*live_table; const void *table_owner;
    uint64_t request,epoch,table_epoch,table_source_epoch,proof_id; bool pending,failed,ready,table_failed,table_ready;
    RangeSourcePolicy source; RangeResultContext results,other_results; RangeCommandContext c;
    unsigned mode,published,cancelled; WorkbookHistoryRangeRecorder inner;
} ResultFixture;
static bool result_resolve(void *context,uint64_t object,RangeResultProof *out) {
    ResultFixture *f=context; Worksheet *s=f->b.sheets;
    if (!object) *out=(RangeResultProof){0,f->request,f->epoch,f->epoch,f->live_primary,f->c.primary,f->live_primary,f->ready,f->failed};
    else if (object==f->pivot_id) *out=(RangeResultProof){object,f->request,f->pivot_epoch,f->pivot_source_epoch,f->pivot_owner,&s->objects.items[0].source,&f->pivot_page,f->pivot_ready,f->pivot_failed};
    else *out=(RangeResultProof){f->proof_id,f->request,f->table_epoch,f->table_source_epoch,f->table_owner,&s->objects.items[0].source,f->live_table,f->table_ready,f->table_failed};
    return true;
}
static void result_fixture(ResultFixture *f) {
    *f=(ResultFixture){0}; workbook_init(&f->b); CHECK(workbook_add(&f->b,"Results",&(Analysis){0},NULL)); Worksheet *s=f->b.sheets;
    f->primary=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("/primary.csv")}; Analysis table={.kind=SOURCE_CSV,.path="/table.csv"};
    CHECK(objects_add_table(&s->objects,20,8,"Complete Table",&table,NULL)==0); s->objects.items[0].rows=302; s->objects.items[0].columns=3; s->objects.items[0].revision=33; worksheet_record(s,EDIT_OBJECTS);
    CHECK(cells_set(&s->cells,12,4,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&s->formats,(FormatRange){12,321,4,10},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); s->formats.source_aware=true;
    f->primary_page=(Page){.total=10,.column_count=3,.columns=dx_alloc(3*sizeof(Column))};
    f->table_page=(Page){.total=300,.offset=299,.row_count=1,.column_count=2,.columns=dx_alloc(2*sizeof(Column)),.cells=dx_alloc(2*sizeof(Cell)),.sql=dx_strdup("RUNTIME_QUERY_MUST_NOT_BE_RETAINED")};
    f->table_page.cells[0].is_null=true; f->table_page.cells[1].text=dx_strdup("RUNTIME_CELL_MUST_NOT_BE_RETAINED");
    f->request=7; f->epoch=4; f->table_epoch=f->table_source_epoch=33; f->proof_id=s->objects.items[0].id; f->ready=f->table_ready=true; f->live_primary=&f->primary_page; f->live_table=&f->table_page; f->table_owner=&f->table_page;
    f->source=(RangeSourcePolicy){&f->primary_page.total,&f->primary_page.column_count,&f->primary_page.count_pending,&f->failed,&f->ready};
    f->results=(RangeResultContext){result_resolve,f}; f->other_results=f->results;
    f->c=context(&f->b,&f->request,&f->epoch); f->c.primary=&f->primary; f->c.source=&f->source; f->c.results=&f->results; f->b.dirty=false;
}
static void result_fixture_free(ResultFixture *f) {
    if (f->primary_page.column_count && !f->primary_page.columns) f->primary_page.columns=dx_alloc(f->primary_page.column_count*sizeof(Column));
    if (f->table_page.column_count && !f->table_page.columns) f->table_page.columns=dx_alloc(f->table_page.column_count*sizeof(Column));
    page_free(&f->primary_page); page_free(&f->table_page); page_free(&f->alternate); page_free(&f->pivot_page); analysis_free(&f->primary); workbook_free(&f->b); }
static void result_pivot(ResultFixture *f) {
    Worksheet *s=f->b.sheets; CHECK(objects_add(&s->objects,500,30,"Active obstacle",NULL)==1); SheetObject *o=s->objects.items+1;
    CHECK(pivot_assign(&o->pivot,PIVOT_VALUES,"Amount",AGG_SUM,NULL)); o->source_id=s->objects.items[0].id; o->revision=8; o->rows=5; o->columns=3; worksheet_record(s,EDIT_OBJECTS);
    f->pivot_id=o->id; f->pivot_epoch=8; f->pivot_source_epoch=33; f->pivot_ready=true; f->pivot_owner=&f->pivot_page;
    f->pivot_page=(Page){.total=2,.offset=2,.column_count=1,.columns=dx_alloc(sizeof(Column)),.totals=dx_alloc(sizeof(PivotTotals))};
    f->pivot_page.totals->has_columns=true;
}
static bool result_reserve(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    ResultFixture *f=context; if (!f->inner.reserve(f->inner.context,record,ticket,error)) return false;
    switch (f->mode) {
    case 1: f->table_page.total++; break;
    case 2: f->table_owner=&f->alternate; break;
    case 3:
        f->alternate=f->table_page; f->alternate.columns=dx_alloc(2*sizeof(Column)); f->alternate.cells=NULL; f->alternate.row_count=0; f->alternate.sql=NULL;
        f->live_table=&f->alternate; free(f->table_page.columns); f->table_page.columns=NULL; break;
    case 4: f->alternate=f->primary_page; f->alternate.columns=dx_alloc(3*sizeof(Column)); f->live_primary=&f->alternate; free(f->primary_page.columns); f->primary_page.columns=NULL; break;
    case 5: f->request++; break;
    case 6: f->table_epoch++; break;
    case 7: f->table_failed=true; break;
    case 8: f->c.results=&f->other_results; break;
    case 9: free(f->b.sheets[0].objects.items[0].source.path); f->b.sheets[0].objects.items[0].source.path=dx_strdup("/replacement.csv"); break;
    case 10: f->primary_page.count_pending=true; break;
    case 11: f->table_page.count_pending=true; break;
    case 12: f->proof_id++; break;
    case 13: f->table_source_epoch++; break;
    case 14: f->table_ready=false; break;
    case 15: {
        Cell *replacement=dx_alloc(2*sizeof(Cell)); free(f->table_page.cells[1].text); free(f->table_page.cells); f->table_page.cells=replacement;
        f->table_page.cells[0].is_null=true; f->table_page.cells[1].text=dx_strdup("replacement viewport"); break;
    }
    case 16: { Column *replacement=dx_alloc(3*sizeof(Column)); free(f->primary_page.columns); f->primary_page.columns=replacement; break; }
    case 17: f->pivot_owner=&f->alternate; break;
    case 18: f->pivot_ready=false; break;
    case 19: f->pivot_epoch++; break;
    case 20: f->pivot_failed=true; break;
    case 21: f->pivot_page.count_pending=true; break;
    case 22: f->pivot_source_epoch++; break;
    }
    return true;
}
static void result_publish(void *context,void *ticket) { ResultFixture *f=context; f->published++; f->inner.publish(f->inner.context,ticket); }
static void result_cancel(void *context,void *ticket) { ResultFixture *f=context; f->cancelled++; f->inner.cancel(f->inner.context,ticket); }
static bool contains(const void *data,size_t bytes,const char *text) { size_t n=strlen(text); for (size_t i=0;n<=bytes && i<=bytes-n;i++) if (!memcmp((const char *)data+i,text,n)) return true; return false; }
static void source_move_controller(void) {
    for (unsigned mode=0;mode<=22;mode++) {
        ResultFixture *f=dx_alloc(sizeof(*f)); result_fixture(f); if (mode>=17) result_pivot(f); f->mode=mode; Worksheet *s=f->b.sheets; RangeCommandTarget *t=target(&f->c);
        RangeCommand cmd={.operation=RANGE_OPERATION_MOVE,.range={12,321,4,10},.dr=400,.dc=10}; RangeCommandPlanInfo plan={0};
        CHECK(workbook_range_command_measure(&f->c,0,cmd,&plan,NULL)==RANGE_COMMAND_READY);
        f->c.command.plan_reserved=plan.owned+plan.validation_scratch-1;
        CHECK(workbook_apply_range_command_recorded(&f->c,t,cmd,NULL,NULL).status==RANGE_COMMAND_LIMIT && !f->b.command_revision); f->c.command.plan_reserved=SIZE_MAX;
        WorkbookHistoryStream *queue=NULL; WorkbookHistoryStreamOrigin origin={.owner={f->b.id.high,f->b.id.low,9,VERSION_WORKING_HEAD},.edit_session=f->b.edit_session_id};
        CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&queue)==HISTORY_STREAM_OK); f->inner=workbook_history_stream_range_recorder(queue);
        WorkbookHistoryRangeRecorder recorder={result_reserve,result_publish,result_cancel,f}; CellStore cells=s->cells; FormatStore formats=s->formats;
        RangeCommandResult applied=workbook_apply_range_command_recorded(&f->c,t,cmd,&recorder,NULL);
        if (mode) CHECK(applied.status==RANGE_COMMAND_STALE && f->cancelled==1 && !f->published && !f->b.command_revision && !f->b.dirty && s->objects.items[0].row==20 && !memcmp(&cells,&s->cells,sizeof(cells)) && !memcmp(&formats,&s->formats,sizeof(formats)) && !workbook_history_stream_state(queue).owned_bytes);
        else {
            CHECK(applied.status==RANGE_COMMAND_APPLIED && applied.effects==14 && f->published==1 && !f->cancelled && s->objects.items[0].row==420 && !strcmp(cells_get(&s->cells,412,14),"NULL"));
            WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(queue,&held)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); RangeEvent event={0};
            CHECK(range_event_decode(workbook_history_event_payload(held),info->bytes,SIZE_MAX,&event)==RANGE_EVENT_OK && event.effects==14 && event.unbound_sources==2 && event.definitions.after_objects.objects.items[0].rows==302);
            CHECK(!contains(workbook_history_event_payload(held),info->bytes,"RUNTIME_QUERY_MUST_NOT_BE_RETAINED") && !contains(workbook_history_event_payload(held),info->bytes,"RUNTIME_CELL_MUST_NOT_BE_RETAINED"));
            range_event_free(&event); CHECK(workbook_history_stream_complete(queue,&held)==HISTORY_STREAM_OK);
            mixed_history_transfer(s,false); f->b.command_revision++; CHECK(s->objects.items[0].row==20 && !strcmp(cells_get(&s->cells,12,4),"NULL"));
            free(t); t=target(&f->c); cmd.dr=cmd.dc=0; f->c.command.plan_reserved=f->c.command.validation_reserved=0;
            CHECK(workbook_apply_range_command_recorded(&f->c,t,cmd,NULL,NULL).status==RANGE_COMMAND_NOOP && s->redo_actions_count==1);
            f->table_owner=&f->alternate; CHECK(workbook_apply_range_command_recorded(&f->c,t,cmd,NULL,NULL).status==RANGE_COMMAND_STALE);
        }
        CHECK(workbook_history_stream_free(&queue,false)==HISTORY_STREAM_OK); free(t); result_fixture_free(f); free(f);
    }
}
static bool result_prepares(ResultFixture *f) { RangeCommandTarget *t=dx_alloc(sizeof(*t)); bool ready=workbook_prepare_range_command(&f->c,0,t,NULL); free(t); return ready; }
static void result_readiness(void) {
    ResultFixture *f=dx_alloc(sizeof(*f)); result_fixture(f); CHECK(result_prepares(f));
    f->proof_id++; CHECK(!result_prepares(f)); f->proof_id--;
    f->table_epoch++; CHECK(!result_prepares(f)); f->table_epoch--;
    f->table_page.count_pending=true; CHECK(!result_prepares(f)); f->table_page.count_pending=false;
    f->table_page.total=UINT64_MAX; CHECK(!result_prepares(f)); f->table_page.total=300;
    f->table_page.column_count=16385; CHECK(!result_prepares(f)); f->table_page.column_count=2;
    f->primary_page.count_pending=true; CHECK(!result_prepares(f)); f->primary_page.count_pending=false;
    f->alternate=(Page){.column_count=2,.columns=dx_alloc(2*sizeof(Column))}; f->live_table=&f->alternate; f->table_owner=&f->alternate;
    f->b.sheets[0].objects.items[0].rows=5; CHECK(result_prepares(f));
    f->b.sheets[0].objects.items[0].rows=3; CHECK(!result_prepares(f)); f->b.sheets[0].objects.items[0].rows=5;
    f->b.sheets[0].objects.items[0].columns=2; CHECK(!result_prepares(f)); f->b.sheets[0].objects.items[0].columns=3;
    result_fixture_free(f); free(f);
    f=dx_alloc(sizeof(*f)); result_fixture(f); result_pivot(f); CHECK(result_prepares(f));
    f->pivot_ready=false; CHECK(!result_prepares(f)); f->pivot_ready=true;
    f->pivot_failed=true; CHECK(!result_prepares(f)); f->pivot_failed=false;
    f->pivot_source_epoch++; CHECK(!result_prepares(f)); f->pivot_source_epoch--;
    f->b.sheets[0].objects.items[1].rows=6; CHECK(!result_prepares(f)); f->b.sheets[0].objects.items[1].rows=5;
    CHECK(result_prepares(f)); result_fixture_free(f); free(f);
}
int main(void) { plans(); callbacks(); stream(); effects_and_noop(); rows(); columns_compound(); paste_compound(); mixed_commands(); clear_command_case(false); clear_command_case(true); mixed_rows_command(); mixed_columns_command(); template_paste_command(); source_cells_command(); source_move_controller(); result_readiness(); printf("PASS: %u admitted manual range checks\n",checks); return 0; }
