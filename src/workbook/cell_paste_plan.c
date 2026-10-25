#include "cell_paste_plan.h"
#include "cells_tsv_scan.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
static int order(uint64_t ar,size_t ac,uint64_t br,size_t bc) { if (ar!=br) return ar<br ? -1 : 1; return (ac>bc)-(ac<bc); }
static bool valid(const CellStore *s,char **error) {
    if (!s || s->count>DX_MAX_EDIT_CELLS || s->count>s->capacity || (s->count && !s->cells)) return fail(error,"Invalid bounded manual paste owner.");
    size_t bytes=0;
    for (size_t i=0;i<s->count;i++) {
        const SheetCell *c=s->cells+i;
        if (!c->text || !*c->text || c->row>=DX_SHEET_ROWS || c->col>=16384 ||
            (i && order(s->cells[i-1].row,s->cells[i-1].col,c->row,c->col)>=0)) return fail(error,"Invalid sorted manual paste cells.");
        size_t n=strlen(c->text)+1;
        if (n>65537 || n>DX_CELL_BYTES-bytes) return fail(error,"Manual paste cells exceed their bounded text budget.");
        bytes+=n;
    }
    return bytes==s->bytes || fail(error,"Manual paste text accounting is invalid.");
}
typedef struct { const CellStore *before; size_t old,count,bytes,capacity; SheetCell *items; bool changed,limited; } Projection;
static bool emit(Projection *p,uint64_t row,size_t col,const char *text,size_t bytes,char **error) {
    if (!bytes) return true;
    if (p->count==DX_MAX_EDIT_CELLS || bytes+1>DX_CELL_BYTES-p->bytes || (p->items && p->count==p->capacity)) {
        p->limited=true; return fail(error,"Editable cells are limited to 20,000 cells and 4 MiB per sheet.");
    }
    if (p->items) p->items[p->count]=(SheetCell){row,col,dx_strdup(text)};
    p->count++; p->bytes+=bytes+1; return true;
}
static bool field(void *context,uint64_t row,size_t col,const char *text,size_t bytes,char **error) {
    Projection *p=context;
    while (p->old<p->before->count) {
        const SheetCell *old=p->before->cells+p->old; int comparison=order(old->row,old->col,row,col);
        if (comparison>=0) break;
        if (!emit(p,old->row,old->col,old->text,strlen(old->text),error)) return false;
        p->old++;
    }
    const SheetCell *old=p->old<p->before->count ? p->before->cells+p->old : NULL;
    bool found=old && !order(old->row,old->col,row,col);
    p->changed|=found ? strcmp(old->text,text)!=0 : bytes!=0;
    if (found) p->old++;
    return emit(p,row,col,text,bytes,error);
}
static CellRangeStatus project(Projection *p,uint64_t row,size_t col,const char *text,CellPastePlanInfo *out,char **error) {
    size_t rows=0,columns=0;
    if (!cells_tsv_scan(row,col,text,field,p,&rows,&columns,error)) return p->limited ? CELL_RANGE_LIMIT : CELL_RANGE_INVALID;
    while (p->old<p->before->count) {
        const SheetCell *old=p->before->cells+p->old++;
        if (!emit(p,old->row,old->col,old->text,strlen(old->text),error)) return CELL_RANGE_LIMIT;
    }
    if (!p->changed) return CELL_RANGE_NOOP;
    *out=(CellPastePlanInfo){p->count*sizeof(SheetCell)+p->bytes,p->count,p->bytes,rows,columns}; return CELL_RANGE_READY;
}
CellRangeStatus cells_paste_measure(const CellStore *s,uint64_t row,size_t col,const char *text,CellPastePlanInfo *out,char **error) {
    if (!out || !valid(s,error)) return CELL_RANGE_INVALID;
    Projection p={.before=s}; CellPastePlanInfo info={0}; CellRangeStatus status=project(&p,row,col,text,&info,error);
    if (status==CELL_RANGE_READY) *out=info; return status;
}
CellRangeStatus cells_paste_plan(const CellStore *s,uint64_t row,size_t col,const char *text,size_t reserved,CellSnapshot *out,char **error) {
    if (!out || (s && out->cells && out->cells==s->cells)) { fail(error,"Choose independent paste output ownership."); return CELL_RANGE_INVALID; }
    CellPastePlanInfo info={0}; CellRangeStatus status=cells_paste_measure(s,row,col,text,&info,error); if (status!=CELL_RANGE_READY) return status;
    if (info.owned>reserved) { fail(error,"Paste cell ownership exceeds its admitted planning budget."); return CELL_RANGE_LIMIT; }
    CellSnapshot next={0}; if (info.count) next.cells=dx_alloc(info.count*sizeof(SheetCell));
    Projection p={.before=s,.capacity=info.count,.items=next.cells}; CellPastePlanInfo actual={0};
    status=project(&p,row,col,text,&actual,error); next.count=p.count; next.bytes=p.bytes;
    if (status!=CELL_RANGE_READY || actual.count!=info.count || actual.bytes!=info.bytes) { cells_snapshot_free(&next); return status==CELL_RANGE_READY ? CELL_RANGE_INVALID : status; }
    cells_snapshot_free(out); *out=next; return CELL_RANGE_READY;
}
