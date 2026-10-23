#include "history.h"
#include "lifecycle.h"
#include <stdlib.h>
#include <string.h>
static uint64_t issued_order;
bool worksheet_history_order_available(void) { return issued_order<UINT64_MAX; }
uint64_t worksheet_history_issue_order(void) { return worksheet_history_order_available() ? ++issued_order : 0; }
static WorkbookHistoryResult fail(WorkbookHistoryResult result,WorkbookHistoryStatus status,char **error,const char *message) {
    result.status=status;
    if (error) { free(*error); *error=dx_strdup(message); }
    return result;
}
static bool journal_valid(const Worksheet *s,bool redo) {
    size_t n=redo ? s->redo_actions_count : s->undo_actions_count;
    const size_t available[]={redo ? s->redo_count : s->undo_count,redo ? s->cells.redo_count : s->cells.undo_count,
        redo ? s->objects.redo_count : s->objects.undo_count,redo ? s->formats.redo_count : s->formats.undo_count};
    if (n>DX_HISTORY || s->history_floor>s->history_last || s->history_ceiling>s->history_last) return false;
    size_t required[4]={0}; const EditKind *actions=redo ? s->redo_actions : s->undo_actions;
    const uint64_t *orders=redo ? s->redo_order : s->undo_order;
    for (size_t i=0;i<n;i++) {
        unsigned effects=worksheet_edit_effects(actions[i]);
        if (!effects || !orders[i] || orders[i]>s->history_last || (i && (redo ? orders[i]>=orders[i-1] : orders[i]<=orders[i-1]))) return false;
        for (unsigned k=0;k<4;k++) required[k]+=!!(effects&(1u<<k));
    }
    for (unsigned k=0;k<4;k++) if (available[k]>DX_HISTORY || required[k]!=available[k]) return false;
    return true;
}
static uint64_t latest_record(const Workbook *book) {
    uint64_t latest=workbook_lifecycle_latest(book); for (size_t i=0;i<book->count;i++) if (book->sheets[i].history_last>latest) latest=book->sheets[i].history_last;
    return latest;
}
WorkbookHistoryResult workbook_history_peek(const Workbook *book,bool redo,char **error) {
    WorkbookHistoryResult result={.status=WORKBOOK_HISTORY_EMPTY};
    if (!workbook_identity_valid(book)) return fail(result,WORKBOOK_HISTORY_INVALID,error,"Invalid workbook history identity.");
    WorkbookHistoryResult lifecycle={.status=WORKBOOK_HISTORY_EMPTY};
    if (!workbook_lifecycle_peek(book,redo,&lifecycle,error)) { result.status=WORKBOOK_HISTORY_INVALID; return result; }
    uint64_t floor=workbook_lifecycle_floor(book),ceiling=0,latest=workbook_lifecycle_latest(book);
    for (size_t i=0;i<book->count;i++) {
        const Worksheet *s=book->sheets+i;
        if (!journal_valid(s,false) || !journal_valid(s,true)) return fail(result,WORKBOOK_HISTORY_INVALID,error,"Worksheet history is incomplete; no edit was replayed.");
        if (s->history_floor>floor) floor=s->history_floor;
        if (s->history_ceiling && (!ceiling || s->history_ceiling<ceiling)) ceiling=s->history_ceiling;
        if (s->history_last>latest) latest=s->history_last;
    }
    if (redo && book->history_branch!=latest) return result;
    for (size_t i=0;i<book->count;i++) {
        const Worksheet *s=book->sheets+i; size_t n=redo ? s->redo_actions_count : s->undo_actions_count;
        if (!n) continue;
        uint64_t order=(redo ? s->redo_order : s->undo_order)[n-1];
        if (order<=floor || (redo && ceiling && order>=ceiling)) continue;
        if (result.status==WORKBOOK_HISTORY_READY && result.order==order) return fail(result,WORKBOOK_HISTORY_INVALID,error,"Ambiguous workbook history order.");
        if (result.status!=WORKBOOK_HISTORY_READY || (redo ? order<result.order : order>result.order)) {
            result=(WorkbookHistoryResult){WORKBOOK_HISTORY_READY,i,s->id,order,(redo ? s->redo_actions : s->undo_actions)[n-1],(redo ? s->redo_columns : s->undo_columns)[n-1],false};
        }
    }
    if (lifecycle.status==WORKBOOK_HISTORY_READY && lifecycle.order>floor && !(redo && ceiling && lifecycle.order>=ceiling)) {
        if (result.status==WORKBOOK_HISTORY_READY && result.order==lifecycle.order) return fail(result,WORKBOOK_HISTORY_INVALID,error,"Ambiguous workbook history order.");
        if (result.status!=WORKBOOK_HISTORY_READY || (redo ? lifecycle.order<result.order : lifecycle.order>result.order)) result=lifecycle;
    }
    if (result.status==WORKBOOK_HISTORY_READY && (book->command_revision==UINT64_MAX ||
        (!result.lifecycle && (worksheet_edit_effects(result.kind)&2) && book->sheets[result.sheet].cells.revision==UINT64_MAX) ||
        (!result.lifecycle && (worksheet_edit_effects(result.kind)&8) && book->sheets[result.sheet].formats.revision==UINT64_MAX)))
        return fail(result,WORKBOOK_HISTORY_LIMIT,error,"Undo/Redo revision space is exhausted.");
    return result;
}
bool workbook_history_available(const Workbook *book,bool redo) { return workbook_history_peek(book,redo,NULL).status==WORKBOOK_HISTORY_READY; }
void worksheet_history_clear_redo(Worksheet *s) {
    for (size_t j=0;j<s->redo_count;j++) analysis_free(s->redo+j);
    memset(s->redo,0,sizeof(s->redo)); s->redo_count=0;
    cells_trim_history(&s->cells,true,0); objects_trim_history(&s->objects,true,0); format_trim_history(&s->formats,true,0);
    s->history_ceiling=0;
    s->redo_actions_count=0; memset(s->redo_actions,0,sizeof(s->redo_actions));
    memset(s->redo_columns,0,sizeof(s->redo_columns)); memset(s->redo_order,0,sizeof(s->redo_order));
}
void workbook_history_note_edit(Workbook *book) {
    for (size_t i=0;i<book->count;i++) worksheet_history_clear_redo(book->sheets+i);
    workbook_lifecycle_clear_redo(book);
    book->history_branch=latest_record(book);
}
void workbook_history_sync(Workbook *book) {
    if (book->history_branch!=latest_record(book)) workbook_history_note_edit(book);
}
WorkbookHistoryResult workbook_history_step(Workbook *book,bool redo,char **error) {
    WorkbookHistoryResult result=workbook_history_peek(book,redo,error);
    if (result.status!=WORKBOOK_HISTORY_READY) return result;
    if (result.lifecycle) return workbook_lifecycle_step(book,redo,error);
    if (result.kind!=EDIT_CELLS && result.kind!=EDIT_FORMAT && result.kind!=EDIT_CELLS_FORMAT) { result.status=WORKBOOK_HISTORY_DELEGATE; return result; }
    workbook_history_sync(book);
    Worksheet *s=book->sheets+result.sheet;
    unsigned effects=worksheet_edit_effects(result.kind);
    if (effects&2) cells_history(&s->cells,redo);
    if (effects&8) format_history(&s->formats,redo);
    worksheet_transfer_history(s,redo);
    book->command_revision++; book->dirty=true; result.status=WORKBOOK_HISTORY_APPLIED;
    return result;
}
