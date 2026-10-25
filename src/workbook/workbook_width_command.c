#include "workbook_width_command.h"
#include "cell_command.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static Worksheet *resolve(Workbook *b,uint64_t id) { for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==id) return b->sheets+i; return NULL; }
static bool context(WorkbookWidthContext *c) { return c && c->book && c->request_epoch && workbook_identity_valid(c->book); }
bool workbook_prepare_width_command(WorkbookWidthContext *c,size_t sheet,WorkbookWidthTarget *out,char **error) {
    if (!context(c) || !out || sheet>=c->book->count || !c->ready || c->read_only) return fail(error,"Choose an available physical sheet width owner.");
    Worksheet *s=c->book->sheets+sheet; size_t owned=0;
    ColumnWidthCommandContext wc={&s->view,&s->width_history,&s->width_revision,0};
    ColumnWidthStatus shape=column_width_command_measure(wc,(ColumnWidthView){0},&owned);
    if (shape!=COLUMN_WIDTH_OK && shape!=COLUMN_WIDTH_NOOP && shape!=COLUMN_WIDTH_LIMIT) return fail(error,"Invalid committed physical column widths.");
    if (!workbook_prepare_command_session(c->book,sheet,error)) return false;
    WorkbookWidthTarget t={.workbook=c->book->id,.document=c->book->edit_session_id,.sheet=s->edit_session_id,.sheet_id=s->id,
        .document_revision=c->book->command_revision,.width_revision=s->width_revision,.request=*c->request_epoch,.history_last=s->history_last,
        .generation=column_width_history_info(s->width_history).generation,.branch=c->book->history_branch,.order=worksheet_history_high_water(),.history=s->width_history,.widths=s->view.widths,.columns=s->view.columns,.count=s->view.column_count};
    if (t.count) memcpy(t.values,t.widths,t.count*sizeof(float)); *out=t; return true;
}
static ColumnWidthStatus check(WorkbookWidthContext *c,const WorkbookWidthTarget *t,Worksheet **out,char **error) {
    if (!context(c) || !t) return COLUMN_WIDTH_INVALID;
    Workbook *b=c->book; Worksheet *s=resolve(b,t->sheet_id);
    if (!s || b->id.high!=t->workbook.high || b->id.low!=t->workbook.low || b->edit_session_id!=t->document || s->edit_session_id!=t->sheet ||
        b->command_revision!=t->document_revision || b->history_branch!=t->branch || worksheet_history_high_water()!=t->order || s->width_revision!=t->width_revision || *c->request_epoch!=t->request || s->history_last!=t->history_last ||
        s->width_history!=t->history || column_width_history_info(s->width_history).generation!=t->generation || s->view.widths!=t->widths || s->view.columns!=t->columns || s->view.column_count!=t->count ||
        (t->count && memcmp(t->values,s->view.widths,t->count*sizeof(float)))) { fail(error,"The physical column width command owner changed."); return COLUMN_WIDTH_STALE; }
    if (!c->ready || c->read_only) { fail(error,"The workbook is unavailable or read-only."); return COLUMN_WIDTH_INVALID; }
    *out=s; return COLUMN_WIDTH_OK;
}
WorkbookWidthResult workbook_apply_width_command_recorded(WorkbookWidthContext *c,const WorkbookWidthTarget *t,ColumnWidthView after,const WorkbookHistoryRangeRecorder *recorder,char **error) {
    WorkbookWidthResult r={.status=COLUMN_WIDTH_INVALID}; Worksheet *s=NULL;
    r.status=check(c,t,&s,error); if (r.status) return r;
    Workbook *b=c->book; r.before_revision=r.after_revision=b->command_revision;
    ColumnWidthCommandContext wc={&s->view,&s->width_history,&s->width_revision,c->reserved}; size_t owned=0;
    r.status=column_width_command_measure(wc,after,&owned); if (r.status) return r;
    if (owned>c->reserved || b->command_revision==UINT64_MAX || !worksheet_history_order_available()) { r.status=COLUMN_WIDTH_LIMIT; fail(error,"Physical column width planning exceeds the available budget or revision space."); return r; }
    if (workbook_history_peek(b,false,NULL).status==WORKBOOK_HISTORY_INVALID) { r.status=COLUMN_WIDTH_INVALID; fail(error,"Worksheet history is incomplete."); return r; }
    size_t retained=DX_COLUMN_WIDTH_MAX_BYTES+(s->width_history ? 0 : column_width_history_allocation());
    if (c->admit && !c->admit(c->admission_context,owned,retained,error)) { r.status=COLUMN_WIDTH_LIMIT; return r; }
    r.status=check(c,t,&s,error); if (r.status) return r;
    wc=(ColumnWidthCommandContext){&s->view,&s->width_history,&s->width_revision,c->reserved};
    ColumnWidthCommand *plan=NULL; void *ticket=NULL;
    r.status=column_width_command_prepare(wc,after,&plan); if (r.status) return r;
    RangeEventView view={.sheet=s->id,.effects=16,.widths=column_width_command_pair(plan)};
    if (recorder) {
        if (!recorder->reserve || !recorder->publish || !recorder->cancel) { r.status=COLUMN_WIDTH_INVALID; fail(error,"Column width recording is incomplete."); goto cancel; }
        WorkbookHistoryRangeRecord record={b->id,b->edit_session_id,s->edit_session_id,b->command_revision,b->command_revision+1,0,view};
        bool reserved=recorder->reserve(recorder->context,&record,&ticket,error);
        if (!reserved || !ticket) { r.status=reserved || ticket ? COLUMN_WIDTH_INVALID : COLUMN_WIDTH_LIMIT; goto cancel; }
    }
    r.status=check(c,t,&s,error); if (r.status) goto cancel;
    wc=(ColumnWidthCommandContext){&s->view,&s->width_history,&s->width_revision,c->reserved};
    r.status=column_width_command_adopt(wc,&plan); if (r.status) goto cancel;
    worksheet_record(s,EDIT_WIDTHS); b->command_revision++; b->dirty=true; workbook_history_note_edit(b);
    r.effects=16; r.after_revision=b->command_revision; r.status=COLUMN_WIDTH_OK;
    if (recorder) recorder->publish(recorder->context,ticket); return r;
cancel:
    if (ticket && recorder) recorder->cancel(recorder->context,ticket);
    column_width_command_cancel(&plan); return r;
}
