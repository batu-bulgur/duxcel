#include "manual_cells_plan.h"
#include <stdlib.h>
#include <string.h>
typedef struct { uint64_t row; size_t column; ManualValue *value; } Entry;
struct ManualCells { size_t count,raw_bytes,owned; Entry entries[]; };
static bool add(size_t *n,size_t amount) { if (*n>SIZE_MAX-amount) return false; *n+=amount; return true; }
static ManualCellsStatus value_status(ManualValueStatus status) { return status==MANUAL_VALUE_OK ? MANUAL_CELLS_READY : status==MANUAL_VALUE_LIMIT ? MANUAL_CELLS_LIMIT : MANUAL_CELLS_INVALID; }
static ManualValueInput text(const char *raw) { return (ManualValueInput){true,MANUAL_VALUE_TEXT,{0},raw,strlen(raw)}; }
static size_t locate(const ManualCells *cells,uint64_t row,size_t column) {
    size_t lo=0,hi=cells->count;
    while (lo<hi) { size_t m=lo+(hi-lo)/2; const Entry *e=cells->entries+m; if (e->row<row || (e->row==row && e->column<column)) lo=m+1; else hi=m; }
    return lo;
}
static ManualCells *allocate(ManualCellsInfo info) { ManualCells *next=dx_alloc(sizeof(*next)+info.count*sizeof(Entry)); next->count=info.count; next->raw_bytes=info.raw_bytes; next->owned=info.owned; return next; }
static bool admit(size_t coexisting,size_t prior,size_t retained,size_t reserved) { return add(&coexisting,prior) && add(&coexisting,retained) && coexisting<=reserved; }
static bool current_snapshot(const CellSnapshot *source,size_t *owned) {
    if (!source || source->count>DX_MAX_EDIT_CELLS || (source->count && !source->cells) || source->bytes>DX_CELL_BYTES) return false;
    size_t bytes=0;
    for (size_t i=0;i<source->count;i++) {
        const SheetCell *c=source->cells+i;
        if (c->row>=DX_SHEET_ROWS || c->col>=16384 || !c->text || !*c->text || strlen(c->text)>65536 ||
            (i && (c[-1].row>c->row || (c[-1].row==c->row && c[-1].col>=c->col))) || !add(&bytes,strlen(c->text)+1) || bytes>DX_CELL_BYTES) return false;
    }
    if (bytes!=source->bytes) return false;
    *owned=source->count*sizeof(SheetCell)+bytes; return true;
}
ManualCellsStatus manual_cells_import_measure(const CellSnapshot *source,ManualCellsInfo *info) {
    if (!info) return MANUAL_CELLS_INVALID;
    size_t borrowed; if (!current_snapshot(source,&borrowed)) return MANUAL_CELLS_INVALID;
    ManualCellsInfo next={source->count,0,sizeof(ManualCells)+source->count*sizeof(Entry),0};
    for (size_t i=0;i<source->count;i++) {
        ManualValueInfo value; ManualCellsStatus status=value_status(manual_value_measure(text(source->cells[i].text),&value)); if (status!=MANUAL_CELLS_READY) return status;
        if (!add(&next.raw_bytes,value.raw_bytes) || !add(&next.owned,value.owned)) return MANUAL_CELLS_LIMIT;
    }
    next.coexisting=next.owned; if (!add(&next.coexisting,borrowed)) return MANUAL_CELLS_LIMIT;
    *info=next; return MANUAL_CELLS_READY;
}
ManualCellsStatus manual_cells_import(const CellSnapshot *source,size_t retained,size_t reserved,ManualCells **out) {
    if (!out) return MANUAL_CELLS_INVALID;
    ManualCellsInfo info; ManualCellsStatus status=manual_cells_import_measure(source,&info); if (status!=MANUAL_CELLS_READY) return status;
    if (!admit(info.coexisting,manual_cells_owned(*out),retained,reserved)) return MANUAL_CELLS_LIMIT;
    ManualCells *next=allocate(info);
    for (size_t i=0;i<info.count;i++) {
        const SheetCell *c=source->cells+i; Entry *e=next->entries+i; e->row=c->row; e->column=c->col;
        status=value_status(manual_value_build(text(c->text),SIZE_MAX,&e->value)); if (status!=MANUAL_CELLS_READY) { manual_cells_free(&next); return status; }
    }
    manual_cells_free(out); *out=next; return MANUAL_CELLS_READY;
}
static bool same(ManualValueInput a,const ManualValue *b) {
    ManualValueView v=manual_value_view(b);
    return a.present==v.present && a.kind==v.kind && a.number.kind==v.number.kind && a.number.precision==v.number.precision && a.number.scale==v.number.scale &&
        a.raw_bytes==v.raw_bytes && (!a.raw_bytes || !memcmp(a.raw,v.raw,a.raw_bytes));
}
ManualCellsStatus manual_cells_edit_measure(const ManualCells *source,uint64_t row,size_t column,ManualValueInput in,ManualCellsInfo *info) {
    if (!source || !info || row>=DX_SHEET_ROWS || column>=16384) return MANUAL_CELLS_INVALID;
    ManualValueInfo value; ManualCellsStatus status=value_status(manual_value_measure(in,&value)); if (status!=MANUAL_CELLS_READY) return status;
    size_t at=locate(source,row,column); bool found=at<source->count && source->entries[at].row==row && source->entries[at].column==column;
    const ManualValue *old=found ? source->entries[at].value : NULL;
    if (same(in,old)) return MANUAL_CELLS_NOOP;
    ManualValueView before=manual_value_view(old); size_t count=source->count+(in.present ? 1 : 0)-(found ? 1 : 0);
    if (count>DX_MAX_EDIT_CELLS) return MANUAL_CELLS_LIMIT;
    ManualCellsInfo next={count,source->raw_bytes-before.raw_bytes,sizeof(ManualCells)+count*sizeof(Entry),0};
    if (!add(&next.raw_bytes,value.raw_bytes) || next.raw_bytes>DX_CELL_BYTES) return MANUAL_CELLS_LIMIT;
    for (size_t i=0;i<source->count;i++) if (!found || i!=at) if (!add(&next.owned,manual_value_view(source->entries[i].value).owned)) return MANUAL_CELLS_LIMIT;
    if (!add(&next.owned,value.owned)) return MANUAL_CELLS_LIMIT;
    next.coexisting=next.owned; if (!add(&next.coexisting,source->owned)) return MANUAL_CELLS_LIMIT;
    *info=next; return MANUAL_CELLS_READY;
}
ManualCellsStatus manual_cells_edit(const ManualCells *source,uint64_t row,size_t column,ManualValueInput in,size_t retained,size_t reserved,ManualCells **out) {
    if (!out) return MANUAL_CELLS_INVALID;
    ManualCellsInfo info; ManualCellsStatus status=manual_cells_edit_measure(source,row,column,in,&info); if (status!=MANUAL_CELLS_READY) return status;
    if (!admit(info.coexisting,*out==source ? 0 : manual_cells_owned(*out),retained,reserved)) return MANUAL_CELLS_LIMIT;
    ManualCells *next=allocate(info); size_t at=locate(source,row,column),n=0;
    for (size_t i=0;i<=source->count;i++) {
        if (i==at && in.present) { Entry *e=next->entries+n++; e->row=row; e->column=column; status=value_status(manual_value_build(in,SIZE_MAX,&e->value)); if (status!=MANUAL_CELLS_READY) { manual_cells_free(&next); return status; } }
        if (i==source->count) break;
        const Entry *old=source->entries+i; if (old->row==row && old->column==column) continue;
        Entry *e=next->entries+n++; e->row=old->row; e->column=old->column;
        status=value_status(manual_value_clone(old->value,SIZE_MAX,&e->value)); if (status!=MANUAL_CELLS_READY) { manual_cells_free(&next); return status; }
    }
    manual_cells_free(out); *out=next; return MANUAL_CELLS_READY;
}
size_t manual_cells_owned(const ManualCells *cells) { return cells ? cells->owned : 0; }
size_t manual_cells_count(const ManualCells *cells) { return cells ? cells->count : 0; }
const ManualValue *manual_cells_at(const ManualCells *cells,uint64_t row,size_t column) {
    if (!cells) return NULL; size_t at=locate(cells,row,column);
    return at<cells->count && cells->entries[at].row==row && cells->entries[at].column==column ? cells->entries[at].value : NULL;
}
void manual_cells_free(ManualCells **cells) { if (cells && *cells) { for (size_t i=0;i<(*cells)->count;i++) manual_value_free(&(*cells)->entries[i].value); free(*cells); *cells=NULL; } }
ManualCellsStatus manual_cells_range(const ManualCells *source,uint64_t row,size_t column,size_t rows,size_t columns,const ManualValue **slots,size_t capacity,ManualRangeView *out) {
    if (!source || !slots || !out || !rows || !columns || row>=DX_SHEET_ROWS || column>=16384) return MANUAL_CELLS_INVALID;
    if (rows>DX_SHEET_ROWS-row || columns>16384-column || rows>DX_CLIPBOARD_CELLS/columns) return MANUAL_CELLS_LIMIT;
    size_t count=rows*columns; if (capacity<count) return MANUAL_CELLS_LIMIT;
    memset(slots,0,count*sizeof(*slots));
    for (size_t i=locate(source,row,column);i<source->count;i++) {
        const Entry *e=source->entries+i; if (e->row-row>=rows) break;
        if (e->column>=column && e->column-column<columns) slots[(size_t)(e->row-row)*columns+e->column-column]=e->value;
    }
    *out=(ManualRangeView){rows,columns,count,slots}; return MANUAL_CELLS_READY;
}
