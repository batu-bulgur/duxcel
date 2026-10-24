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
static RangeCommand command(void) { return (RangeCommand){{1,3,1,3},9,9,true}; }
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
    RangeCommand empty={{50,52,50,52},10,10,false}; c.command.plan_reserved=0;
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
    free(t); t=target(&c); RangeCommand reverse={{10,12,10,12},-9,-9,false};
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
int main(void) { plans(); callbacks(); stream(); effects_and_noop(); printf("PASS: %u admitted manual range checks\n",checks); return 0; }
