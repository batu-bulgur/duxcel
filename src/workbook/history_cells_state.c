#include "history_cells_state.h"
#include <string.h>
static const Worksheet *sheet(const Workbook *book,uint64_t id) {
    for (size_t i=0;i<book->count;i++) if (book->sheets[i].id==id) return book->sheets+i;
    return NULL;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_state_measure(const WorkbookHistoryProfile *profile,const Workbook *book,size_t *bytes) {
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile);
    if (!v || !book || !bytes || !workbook_identity_valid(book) || v->document.high!=book->id.high || v->document.low!=book->id.low)
        return HISTORY_CELLS_STATE_INVALID;
    if (v->kind!=HISTORY_PROFILE_RETAINED || !v->current.path) return HISTORY_CELLS_STATE_UNSUPPORTED;
    if (v->current.count>DX_HISTORY_PROFILE_RECORDS || v->current.count>SIZE_MAX/sizeof(WorkbookHistoryCellStateEntry)) return HISTORY_CELLS_STATE_LIMIT;
    for (size_t i=0;i<v->current.count;i++) {
        const WorkbookHistoryProfileRecord *r=v->records+v->current.records[i];
        if (r->event->family!=DX_HISTORY_CELL_FAMILY) return HISTORY_CELLS_STATE_UNSUPPORTED;
        if (!sheet(book,r->sheet)) return HISTORY_CELLS_STATE_MISMATCH;
    }
    *bytes=v->current.count*sizeof(WorkbookHistoryCellStateEntry); return HISTORY_CELLS_STATE_OK;
}
static bool walk(const WorkbookHistoryProfileView *v,const Workbook *book,WorkbookHistoryCellStateEntry *scratch,
    size_t from,size_t to,bool redo,size_t *touched) {
    size_t count=0;
    for (size_t step=from;redo ? step<to : step>to;) {
        size_t i=redo ? step++ : --step;
        const WorkbookHistoryProfileRecord *r=v->records+v->current.records[i]; CellDelta d={0};
        if (cell_delta_decode(r->data,r->bytes,&d)!=CELL_DELTA_CODEC_OK) return false;
        size_t found=0; while (found<count && (scratch[found].sheet!=d.sheet || scratch[found].row!=d.row || scratch[found].column!=d.column)) found++;
        const char *value; size_t bytes;
        if (found<count) { value=scratch[found].value; bytes=scratch[found].bytes; }
        else { value=cells_get(&sheet(book,d.sheet)->cells,d.row,d.column); bytes=strlen(value); }
        const char *expected=redo ? d.before : d.after; size_t expected_bytes=redo ? d.before_bytes : d.after_bytes;
        if (bytes!=expected_bytes || (bytes && memcmp(value,expected,bytes))) return false;
        if (found==count) count++;
        scratch[found]=(WorkbookHistoryCellStateEntry){d.sheet,d.row,d.column,redo ? d.after_bytes : d.before_bytes,redo ? d.after : d.before};
    }
    *touched=count; return true;
}
WorkbookHistoryCellsStateStatus workbook_history_cells_state_check(const WorkbookHistoryProfile *profile,const Workbook *actual,
    WorkbookHistoryCellStateEntry *scratch,size_t reserved,WorkbookHistoryCellsStateReport *out) {
    if (!out) return HISTORY_CELLS_STATE_INVALID;
    size_t bytes=0; WorkbookHistoryCellsStateStatus status=workbook_history_cells_state_measure(profile,actual,&bytes);
    if (status!=HISTORY_CELLS_STATE_OK) return status;
    if (reserved<bytes) return HISTORY_CELLS_STATE_LIMIT;
    if (bytes && !scratch) return HISTORY_CELLS_STATE_INVALID;
    const WorkbookHistoryProfileView *v=workbook_history_profile_view(profile); const WorkbookHistoryPath *p=v->current.path;
    size_t cursor=(size_t)(p->cursor-p->available);
    WorkbookHistoryCellsStateReport next={.undo=cursor,.redo=v->current.count-cursor};
    if (!walk(v,actual,scratch,cursor,0,false,&next.undo_cells) ||
        !walk(v,actual,scratch,cursor,v->current.count,true,&next.redo_cells)) return HISTORY_CELLS_STATE_MISMATCH;
    *out=next; return HISTORY_CELLS_STATE_OK;
}
