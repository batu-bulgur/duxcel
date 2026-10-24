#include "cell_range_plan.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static int order(const void *left,const void *right) {
    const SheetCell *a=left,*b=right; if (a->row!=b->row) return a->row<b->row ? -1 : 1; return (a->col>b->col)-(a->col<b->col);
}
static bool inside(const SheetCell *c,uint64_t r0,uint64_t r1,size_t c0,size_t c1) { return c->row>=r0 && c->row<=r1 && c->col>=c0 && c->col<=c1; }
static bool source_valid(const CellStore *s,char **error) {
    if (!s || s->count>DX_MAX_EDIT_CELLS || s->count>s->capacity || (s->count && !s->cells) || s->bytes>DX_CELL_BYTES) return fail(error,"Invalid bounded manual cell ownership.");
    size_t bytes=0;
    for (size_t i=0;i<s->count;i++) {
        const SheetCell *cell=s->cells+i;
        if (cell->row>=DX_SHEET_ROWS || cell->col>=16384 || !cell->text || !*cell->text || strlen(cell->text)>65536 || (i && order(cell-1,cell)>=0) || strlen(cell->text)+1>DX_CELL_BYTES-bytes) {
            fail(error,"Invalid bounded manual cell ownership."); return false;
        }
        bytes+=strlen(cell->text)+1;
    }
    if (bytes!=s->bytes) { fail(error,"Manual cell byte accounting changed."); return false; }
    return true;
}
static CellRangeStatus project(const CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,bool replace,SheetCell *items,CellRangePlanInfo *out,char **error) {
    if (!s || !out || s->count>DX_MAX_EDIT_CELLS || s->count>s->capacity || (s->count && !s->cells) || s->bytes>DX_CELL_BYTES ||
        r0>r1 || c0>c1 || r1>=DX_SHEET_ROWS || c1>=16384 || dr<-(int64_t)DX_SHEET_ROWS || dr>(int64_t)DX_SHEET_ROWS || dc<-16384 || dc>16384 ||
        (dr<0 && r0<(uint64_t)-dr) || (dc<0 && c0<(size_t)-dc) || (int64_t)r1+dr>=(int64_t)DX_SHEET_ROWS || (int64_t)c1+dc>=16384) {
        fail(error,"Choose a canonical sparse range and a destination inside the worksheet."); return CELL_RANGE_INVALID;
    }
    if (!source_valid(s,error)) return CELL_RANGE_INVALID;
    size_t bytes=0;
    if (!dr && !dc) return CELL_RANGE_NOOP;
    uint64_t to0=(uint64_t)((int64_t)r0+dr),to1=(uint64_t)((int64_t)r1+dr); size_t tc0=(size_t)((int64_t)c0+dc),tc1=(size_t)((int64_t)c1+dc);
    size_t count=0; bytes=0;
    for (size_t i=0;i<s->count;i++) {
        SheetCell cell=s->cells[i]; bool from=inside(&cell,r0,r1,c0,c1),to=inside(&cell,to0,to1,tc0,tc1);
        if (to && !from) {
            if (!replace) { fail(error,"Destination contains data. Choose an empty range or explicitly Replace."); return CELL_RANGE_OCCUPIED; }
            continue;
        }
        if (from) { cell.row=(uint64_t)((int64_t)cell.row+dr); cell.col=(size_t)((int64_t)cell.col+dc); }
        items[count++]=cell; bytes+=strlen(cell.text)+1;
    }
    qsort(items,count,sizeof(*items),order);
    bool same=count==s->count;
    for (size_t i=0;same && i<count;i++) same=!order(items+i,s->cells+i) && !strcmp(items[i].text,s->cells[i].text);
    if (same) return CELL_RANGE_NOOP;
    *out=(CellRangePlanInfo){count*sizeof(*items)+bytes,count,bytes}; return CELL_RANGE_READY;
}
CellRangeStatus cells_range_measure(const CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,bool replace,CellRangePlanInfo *out,char **error) {
    if (!out) return CELL_RANGE_INVALID; SheetCell items[DX_MAX_EDIT_CELLS]; CellRangePlanInfo info={0};
    CellRangeStatus status=project(s,r0,r1,c0,c1,dr,dc,replace,items,&info,error); if (status==CELL_RANGE_READY) *out=info; return status;
}
CellRangeStatus cells_range_plan(const CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,bool replace,size_t reserved,CellSnapshot *out,char **error) {
    if (!out) return CELL_RANGE_INVALID; SheetCell items[DX_MAX_EDIT_CELLS]; CellRangePlanInfo info={0};
    CellRangeStatus status=project(s,r0,r1,c0,c1,dr,dc,replace,items,&info,error); if (status!=CELL_RANGE_READY) return status;
    if (info.owned>reserved) { fail(error,"Range cell ownership exceeds its admitted planning budget."); return CELL_RANGE_LIMIT; }
    CellSnapshot next={.count=info.count,.bytes=info.bytes}; if (next.count) next.cells=dx_alloc(next.count*sizeof(*next.cells));
    for (size_t i=0;i<next.count;i++) { next.cells[i]=items[i]; next.cells[i].text=dx_strdup(items[i].text); }
    cells_snapshot_free(out); *out=next; return status;
}

static CellRangeStatus project_rows(const CellStore *s,uint64_t row,uint64_t count,bool insert,SheetCell *items,CellRangePlanInfo *out,char **error) {
    if (!out || row>=DX_SHEET_ROWS || !count || count>DX_SHEET_ROWS-row || !source_valid(s,error)) {
        if (!error || !*error) fail(error,"Choose rows inside the worksheet."); return CELL_RANGE_INVALID;
    }
    size_t n=0,bytes=0; bool changed=false;
    for (size_t i=0;i<s->count;i++) {
        SheetCell cell=s->cells[i];
        if (cell.row>=row) {
            if (insert && cell.row>=DX_SHEET_ROWS-count) { fail(error,"Insert would move a populated cell beyond the last row."); return CELL_RANGE_INVALID; }
            changed=true;
            if (!insert && cell.row-row<count) continue;
            cell.row=insert ? cell.row+count : cell.row-count;
        }
        items[n++]=cell; bytes+=strlen(cell.text)+1;
    }
    if (!changed) return CELL_RANGE_NOOP;
    *out=(CellRangePlanInfo){n*sizeof(*items)+bytes,n,bytes}; return CELL_RANGE_READY;
}
CellRangeStatus cells_rows_measure(const CellStore *s,uint64_t row,uint64_t count,bool insert,CellRangePlanInfo *out,char **error) {
    if (!out) return CELL_RANGE_INVALID; SheetCell items[DX_MAX_EDIT_CELLS]; CellRangePlanInfo info={0};
    CellRangeStatus status=project_rows(s,row,count,insert,items,&info,error); if (status==CELL_RANGE_READY) *out=info; return status;
}
CellRangeStatus cells_rows_plan(const CellStore *s,uint64_t row,uint64_t count,bool insert,size_t reserved,CellSnapshot *out,char **error) {
    if (!out) return CELL_RANGE_INVALID; SheetCell items[DX_MAX_EDIT_CELLS]; CellRangePlanInfo info={0};
    CellRangeStatus status=project_rows(s,row,count,insert,items,&info,error); if (status!=CELL_RANGE_READY) return status;
    if (info.owned>reserved) { fail(error,"Row cell ownership exceeds its admitted planning budget."); return CELL_RANGE_LIMIT; }
    CellSnapshot next={.count=info.count,.bytes=info.bytes}; if (next.count) next.cells=dx_alloc(next.count*sizeof(*next.cells));
    for (size_t i=0;i<next.count;i++) { next.cells[i]=items[i]; next.cells[i].text=dx_strdup(items[i].text); }
    cells_snapshot_free(out); *out=next; return status;
}
