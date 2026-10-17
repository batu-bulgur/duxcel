#include "cells.h"
#include <stdlib.h>
#include <string.h>
static size_t locate(const CellStore *s,uint64_t row,size_t col) {
    size_t lo=0,hi=s->count;
    while (lo<hi) { size_t m=lo+(hi-lo)/2; SheetCell *c=&s->cells[m]; if (c->row<row || (c->row==row && c->col<col)) lo=m+1; else hi=m; }
    return lo;
}
static void snapshot_free(CellSnapshot *s) { for (size_t i=0;i<s->count;i++) free(s->cells[i].text); free(s->cells); memset(s,0,sizeof(*s)); }
static void clear_stack(CellSnapshot *stack,size_t *count) { while (*count) snapshot_free(&stack[--*count]); }
static CellSnapshot snapshot(const CellStore *s) {
    CellSnapshot out={.count=s->count,.bytes=s->bytes}; out.cells=dx_alloc(s->count*sizeof(SheetCell));
    for (size_t i=0;i<s->count;i++) { out.cells[i]=s->cells[i]; out.cells[i].text=dx_strdup(s->cells[i].text); } return out;
}
static void push(CellSnapshot *stack,size_t *count,CellSnapshot state) {
    size_t bytes=state.bytes;
    for (size_t i=0;i<*count;i++) bytes+=stack[i].bytes;
    while (*count && (*count==32 || bytes>8*1024*1024)) {
        bytes-=stack[0].bytes; snapshot_free(&stack[0]); memmove(stack,stack+1,(--*count)*sizeof(*stack));
    }
    stack[(*count)++]=state;
}
static void remember(CellStore *s) { clear_stack(s->redo,&s->redo_count); push(s->undo,&s->undo_count,snapshot(s)); s->revision++; }
void cells_free(CellStore *s) {
    CellSnapshot data={s->cells,s->count,s->bytes}; snapshot_free(&data);
    clear_stack(s->undo,&s->undo_count); clear_stack(s->redo,&s->redo_count); memset(s,0,sizeof(*s));
}
void cells_copy(CellStore *to,const CellStore *from) {
    CellSnapshot copy=snapshot(from); cells_free(to); to->cells=copy.cells; to->count=to->capacity=copy.count; to->bytes=copy.bytes;
}
const char *cells_get(const CellStore *s,uint64_t row,size_t col) {
    size_t i=locate(s,row,col); return i<s->count && s->cells[i].row==row && s->cells[i].col==col ? s->cells[i].text : "";
}
static bool fail(char **error,const char *message) { free(*error); *error=dx_strdup(message); return false; }
bool cells_set(CellStore *s,uint64_t row,size_t col,const char *text,char **error) {
    if (!text) text="";
    size_t i=locate(s,row,col); bool found=i<s->count && s->cells[i].row==row && s->cells[i].col==col;
    size_t length=strlen(text); if (col>=16384 || row>=DX_SHEET_ROWS || length>65536) return fail(error,"Cell exceeds the worksheet limits.");
    size_t previous=found ? strlen(s->cells[i].text)+1 : 0;
    if (s->bytes-previous+length+1>DX_CELL_BYTES || (!found && length && s->count>=DX_MAX_EDIT_CELLS)) return fail(error,"Editable cells are limited to 20,000 cells and 4 MiB per sheet.");
    if ((!found && !length) || (found && !strcmp(s->cells[i].text,text))) return true;
    char *owned=length ? dx_strdup(text) : NULL;
    remember(s); s->bytes-=previous;
    if (found) free(s->cells[i].text);
    if (!length) { memmove(s->cells+i,s->cells+i+1,(s->count-i-1)*sizeof(SheetCell)); s->count--; return true; }
    if (!found) {
        if (s->count==s->capacity) { s->capacity=s->capacity ? s->capacity*2 : 16; s->cells=dx_realloc(s->cells,s->capacity*sizeof(SheetCell)); }
        memmove(s->cells+i+1,s->cells+i,(s->count-i)*sizeof(SheetCell)); s->count++;
    }
    s->cells[i]=(SheetCell){row,col,owned}; s->bytes+=length+1; return true;
}
static bool inside(const SheetCell *c,uint64_t r0,uint64_t r1,size_t c0,size_t c1) { return c->row>=r0 && c->row<=r1 && c->col>=c0 && c->col<=c1; }
bool cells_clear(CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1) {
    bool changed=false;
    for (size_t i=0;i<s->count;i++) if (inside(&s->cells[i],r0,r1,c0,c1)) { changed=true; break; }
    if (!changed) return false; remember(s); size_t out=0;
    for (size_t i=0;i<s->count;i++) {
        if (inside(&s->cells[i],r0,r1,c0,c1)) { s->bytes-=strlen(s->cells[i].text)+1; free(s->cells[i].text); }
        else s->cells[out++]=s->cells[i];
    }
    s->count=out; return true;
}
static int order(const void *aa,const void *bb) {
    const SheetCell *a=aa,*b=bb; if (a->row!=b->row) return a->row<b->row ? -1 : 1; return (a->col>b->col)-(a->col<b->col);
}
bool cells_paste(CellStore *s,uint64_t row,size_t col,const char *tsv,size_t *rows,size_t *columns,char **error) {
    return cells_paste_checked(s,row,col,tsv,rows,columns,NULL,NULL,error);
}
bool cells_paste_checked(CellStore *s,uint64_t row,size_t col,const char *tsv,size_t *rows,size_t *columns,CellPasteGuard guard,void *context,char **error) {
    CellStore input={0},merged={0}; char *field=NULL; bool ok=false;
    size_t r=0,c=0,width=0,length=strlen(tsv);
    if (length>8*1024*1024) return fail(error,"Paste up to 8 MiB of text.");
    field=dx_alloc(65537); const char *p=tsv;
    for (;;) {
        size_t n=0; bool quoted=*p=='"',closed=!quoted; if (quoted) p++;
        while (*p) {
            if (quoted && *p=='"') {
                p++; if (*p!='"') { closed=true; break; }
            } else if (!quoted && (*p=='\t' || *p=='\r' || *p=='\n')) break;
            if (n==65536) { fail(error,"Cell exceeds the 64 KiB text limit."); goto done; }
            field[n++]=*p++;
        }
        if (!closed || (*p && *p!='\t' && *p!='\r' && *p!='\n')) { fail(error,"Invalid quoted clipboard text."); goto done; }
        field[n]=0;
        if (input.count==DX_MAX_EDIT_CELLS || (uint64_t)r>=DX_SHEET_ROWS || row>=DX_SHEET_ROWS-r || c>=16384 || col>=16384-c) { fail(error,"Paste up to 20,000 cells inside the worksheet."); goto done; }
        if (input.count==input.capacity) { input.capacity=input.capacity ? input.capacity*2 : 32; input.cells=dx_realloc(input.cells,input.capacity*sizeof(SheetCell)); }
        input.cells[input.count++]=(SheetCell){row+r,col+c,dx_strdup(field)};
        if (c+1>width) width=c+1;
        if (!*p) break;
        if (*p++=='\t') { c++; continue; }
        if (p[-1]=='\r' && *p=='\n') p++;
        if (!*p) break;
        r++; c=0;
    }
    if (guard && !guard(context,row,col,r+1,width,error)) goto done;
    merged.capacity=s->count+input.count; merged.cells=dx_alloc(merged.capacity*sizeof(SheetCell));
    size_t old=0,add=0;
    while (old<s->count || add<input.count) {
        const SheetCell *cell;
        int comparison=old==s->count ? 1 : add==input.count ? -1 : order(&s->cells[old],&input.cells[add]);
        if (comparison<0) cell=&s->cells[old++];
        else { cell=&input.cells[add++]; if (!comparison) old++; }
        if (!*cell->text) continue;
        merged.bytes+=strlen(cell->text)+1;
        if (merged.count==DX_MAX_EDIT_CELLS || merged.bytes>DX_CELL_BYTES) { fail(error,"Editable cells are limited to 20,000 cells and 4 MiB per sheet."); goto done; }
        merged.cells[merged.count]=*cell; merged.cells[merged.count++].text=dx_strdup(cell->text);
    }
    bool same=s->count==merged.count;
    for (size_t i=0;same && i<s->count;i++) same=!order(&s->cells[i],&merged.cells[i]) && !strcmp(s->cells[i].text,merged.cells[i].text);
    if (!same) {
        remember(s); CellSnapshot previous={s->cells,s->count,s->bytes}; snapshot_free(&previous);
        s->cells=merged.cells; s->count=merged.count; s->capacity=merged.capacity; s->bytes=merged.bytes; memset(&merged,0,sizeof(merged));
    }
    *rows=r+1; *columns=width; ok=true;
 done:
    free(field); cells_free(&input); cells_free(&merged); return ok;
}
bool cells_move(CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,char **error) {
    if (r0>r1 || c0>c1 || r1>=DX_SHEET_ROWS || c1>=16384 || dr < -(int64_t)DX_SHEET_ROWS || dr>(int64_t)DX_SHEET_ROWS || dc < -16384 || dc>16384) return fail(error,"Move stays inside the worksheet.");
    if (!dr && !dc) return true;
    if ((dr<0 && r0<(uint64_t)-dr) || (dc<0 && c0<(size_t)-dc) || (int64_t)r1+dr>=(int64_t)DX_SHEET_ROWS || (int64_t)c1+dc>=16384) return fail(error,"Move stays inside the worksheet.");
    bool changed=false;
    for (size_t i=0;i<s->count;i++) if (inside(&s->cells[i],r0,r1,c0,c1)) {
        SheetCell target=s->cells[i]; target.row=(uint64_t)((int64_t)target.row+dr); target.col=(size_t)((int)target.col+dc);
        if (*cells_get(s,target.row,target.col) && !inside(&target,r0,r1,c0,c1)) return fail(error,"Destination contains data. Choose an empty destination."); changed=true;
    }
    if (!changed) return true; remember(s);
    for (size_t i=0;i<s->count;i++) if (inside(&s->cells[i],r0,r1,c0,c1)) { s->cells[i].row=(uint64_t)((int64_t)s->cells[i].row+dr); s->cells[i].col=(size_t)((int)s->cells[i].col+dc); }
    qsort(s->cells,s->count,sizeof(SheetCell),order); return true;
}
bool cells_column(CellStore *s,size_t col,bool insert,char **error) {
    if (col>=16384) return fail(error,"Column is outside the worksheet.");
    for (size_t i=0;insert && i<s->count;i++) if (s->cells[i].col>=col && s->cells[i].col==16383) return fail(error,"Insert would move a populated cell beyond the last column.");
    remember(s); size_t out=0;
    for (size_t i=0;i<s->count;i++) {
        SheetCell c=s->cells[i];
        if (!insert && c.col==col) { s->bytes-=strlen(c.text)+1; free(c.text); continue; }
        if (insert && c.col>=col) c.col++; else if (!insert && c.col>col) c.col--;
        s->cells[out++]=c;
    }
    s->count=out; return true;
}
bool cells_row(CellStore *s,uint64_t row,bool insert,char **error) {
    return cells_rows(s,row,1,insert,error);
}
bool cells_rows(CellStore *s,uint64_t row,uint64_t count,bool insert,char **error) {
    if (row>=DX_SHEET_ROWS || !count || count>DX_SHEET_ROWS-row) return fail(error,"Rows are outside the worksheet.");
    bool changed=false;
    for (size_t i=0;i<s->count;i++) if (s->cells[i].row>=row) {
        if (insert && s->cells[i].row>=DX_SHEET_ROWS-count) return fail(error,"Insert would move a populated cell beyond the last row.");
        changed=true;
    }
    if (!changed) return true;
    remember(s); size_t out=0;
    for (size_t i=0;i<s->count;i++) {
        SheetCell cell=s->cells[i];
        if (!insert && cell.row>=row && cell.row-row<count) { s->bytes-=strlen(cell.text)+1; free(cell.text); continue; }
        if (insert && cell.row>=row) cell.row+=count; else if (!insert && cell.row>=row+count) cell.row-=count;
        s->cells[out++]=cell;
    }
    s->count=out; return true;
}
bool cells_history(CellStore *s,bool redo) {
    CellSnapshot *from=redo ? s->redo : s->undo,*to=redo ? s->undo : s->redo;
    size_t *fc=redo ? &s->redo_count : &s->undo_count,*tc=redo ? &s->undo_count : &s->redo_count;
    if (!*fc) return false;
    push(to,tc,(CellSnapshot){s->cells,s->count,s->bytes}); CellSnapshot state=from[--*fc];
    s->cells=state.cells; s->count=s->capacity=state.count; s->bytes=state.bytes; s->revision++; return true;
}
void cells_trim_history(CellStore *s,bool redo,size_t keep) {
    CellSnapshot *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    if (keep>=*count) return;
    size_t drop=*count-keep; for (size_t i=0;i<drop;i++) snapshot_free(&stack[i]);
    memmove(stack,stack+drop,keep*sizeof(*stack)); memset(stack+keep,0,drop*sizeof(*stack)); *count=keep;
}
void cells_region(const CellStore *s,uint64_t row,size_t col,uint64_t *r0,uint64_t *r1,size_t *c0,size_t *c1,bool whole) {
    *r0=*r1=row; *c0=*c1=col;
    if (whole && s->count) { *r0=*r1=s->cells[0].row; *c0=*c1=s->cells[0].col; }
    bool changed;
    do {
        changed=false;
        for (int direction=0;direction<2;direction++) for (size_t step=0;step<s->count;step++) {
            const SheetCell *c=&s->cells[direction ? s->count-step-1 : step];
            if (!whole && ((c->row<*r0 && *r0-c->row>1) || (c->row>*r1 && c->row-*r1>1) || (c->col<*c0 && *c0-c->col>1) || (c->col>*c1 && c->col-*c1>1))) continue;
            if (c->row<*r0) { *r0=c->row; changed=true; } if (c->row>*r1) { *r1=c->row; changed=true; }
            if (c->col<*c0) { *c0=c->col; changed=true; } if (c->col>*c1) { *c1=c->col; changed=true; }
        }
    } while (changed && !whole);
}
char *cells_tsv(const CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,char **error) {
    if (r1<r0 || c1<c0 || c1-c0>=20000 || r1-r0>=20000 || (r1-r0+1)>20000/(c1-c0+1)) { fail(error,"Copy up to 20,000 cells."); return NULL; }
    size_t capacity=1024,length=0; char *out=dx_alloc(capacity);
    for (uint64_t row=r0;row<=r1;row++) {
        for (size_t col=c0;col<=c1;col++) {
            const char *text=cells_get(s,row,col); bool quote=strpbrk(text,"\t\n\r\"")!=NULL;
            size_t max=strlen(text)*2+4;
            if (length+max>8*1024*1024) { free(out); fail(error,"Clipboard output exceeds 8 MiB."); return NULL; }
            if (length+max+1>capacity) { capacity=(length+max+1)*2; out=dx_realloc(out,capacity); }
            if (col>c0) out[length++]='\t'; if (quote) out[length++]='"';
            for (const char *p=text;*p;p++) { if (*p=='"') out[length++]='"'; out[length++]=*p; }
            if (quote) out[length++]='"';
        }
        if (row<r1) out[length++]='\n';
    }
    out[length]=0; return out;
}
