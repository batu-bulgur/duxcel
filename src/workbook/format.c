#include "format.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
CellFormat format_default(void) { return (CellFormat){.size=13,.decimals=2}; }
static void merge(CellFormat *to,CellFormat value,unsigned mask) {
    to->flags=(unsigned char)((to->flags&~(mask&7))|(value.flags&(mask&7)));
    if (mask&FORMAT_ALIGN) to->align=value.align;
    if (mask&FORMAT_COLOR) to->color=value.color;
    if (mask&FORMAT_SIZE) to->size=value.size;
    if (mask&FORMAT_NUMERIC) to->number=value.number;
    if (mask&FORMAT_DECIMALS) to->decimals=value.decimals;
}
static bool same_value(CellFormat a,CellFormat b,unsigned mask) {
    CellFormat x=format_default(),y=x; merge(&x,a,mask); merge(&y,b,mask);
    return x.flags==y.flags && x.align==y.align && x.color==y.color && x.size==y.size && x.number==y.number && x.decimals==y.decimals;
}
static bool range_valid(FormatRange r) { return r.r0<=r.r1 && r.r1<DX_SHEET_ROWS && r.c0<=r.c1 && r.c1<16384; }
static bool overlap(FormatRange a,FormatRange b) { return a.r0<=b.r1 && b.r0<=a.r1 && a.c0<=b.c1 && b.c0<=a.c1; }
static bool contains(FormatRange a,FormatRange b) { return a.r0<=b.r0 && a.r1>=b.r1 && a.c0<=b.c0 && a.c1>=b.c1; }
static bool same_range(FormatRange a,FormatRange b) { return a.r0==b.r0 && a.r1==b.r1 && a.c0==b.c0 && a.c1==b.c1; }
static bool value_valid(CellFormat v) { return v.flags<=7 && v.align<=FORMAT_RIGHT && v.color<=3 && v.size>=11 && v.size<=16 && v.number<=FORMAT_NUMBER && v.decimals<=4; }
CellFormat format_at(const FormatStore *s,uint64_t row,size_t column) {
    CellFormat result=format_default(); unsigned needed=FORMAT_ALL;
    for (size_t i=s->count;i && needed;i--) {
        const FormatPatch *p=&s->items[i-1]; FormatRange r=p->range;
        if (row>=r.r0 && row<=r.r1 && column>=r.c0 && column<=r.c1) { merge(&result,p->value,p->mask&needed); result.specified|=(unsigned char)(p->mask&needed); needed&=~p->mask; }
    }
    return result;
}
bool format_valid(const FormatStore *s,char **error) {
    if (s->count>DX_FORMAT_PATCHES || (s->count && !s->items)) return fail(error,"Cell formats exceed the workbook's bounded range limit.");
    for (size_t i=0;i<s->count;i++) if (!range_valid(s->items[i].range) || !s->items[i].mask || (s->items[i].mask&~FORMAT_ALL) || !value_valid(s->items[i].value)) return fail(error,"Invalid cell format range.");
    return true;
}
static FormatSnapshot snapshot(const FormatStore *s) {
    FormatSnapshot copy={.count=s->count,.items=dx_alloc(s->count*sizeof(FormatPatch))};
    if (s->count) memcpy(copy.items,s->items,s->count*sizeof(FormatPatch)); return copy;
}
void format_snapshot_free(FormatSnapshot *s) { free(s->items); *s=(FormatSnapshot){0}; }
static void clear(FormatSnapshot *stack,size_t *count) { while (*count) format_snapshot_free(&stack[--*count]); }
static void push(FormatSnapshot *stack,size_t *count,FormatSnapshot state) {
    if (*count==DX_FORMAT_HISTORY) { format_snapshot_free(&stack[0]); memmove(stack,stack+1,(--*count)*sizeof(*stack)); }
    stack[(*count)++]=state;
}
void format_free(FormatStore *s) { free(s->items); clear(s->undo,&s->undo_count); clear(s->redo,&s->redo_count); memset(s,0,sizeof(*s)); }
void format_copy(FormatStore *to,const FormatStore *from) {
    FormatSnapshot copy=snapshot(from); uint64_t revision=from->revision; format_free(to); to->items=copy.items; to->count=copy.count; to->revision=revision;
}
static bool same_snapshot(const FormatStore *a,const FormatSnapshot *b) {
    if (a->count!=b->count) return false;
    for (size_t i=0;i<a->count;i++) if (!same_range(a->items[i].range,b->items[i].range) || a->items[i].mask!=b->items[i].mask || !same_value(a->items[i].value,b->items[i].value,a->items[i].mask)) return false;
    return true;
}
bool format_commit(FormatStore *s,FormatSnapshot *owned) {
    if (same_snapshot(s,owned)) { format_snapshot_free(owned); return false; }
    clear(s->redo,&s->redo_count); push(s->undo,&s->undo_count,(FormatSnapshot){s->items,s->count});
    s->items=owned->items; s->count=owned->count; *owned=(FormatSnapshot){0}; s->revision++; return true;
}
static bool append(FormatSnapshot *s,FormatPatch patch,char **error) {
    if (s->count && same_range(s->items[s->count-1].range,patch.range)) {
        FormatPatch *last=&s->items[s->count-1]; merge(&last->value,patch.value,patch.mask); last->mask|=patch.mask; return true;
    }
    size_t at=0;
    for (size_t i=0;i<s->count;i++) {
        FormatPatch old=s->items[i]; if (contains(patch.range,old.range)) old.mask&=~patch.mask;
        if (old.mask) s->items[at++]=old;
    }
    s->count=at; if (at==DX_FORMAT_PATCHES) return fail(error,"Cell formats are limited to 512 range records per sheet. Format a containing range to consolidate them.");
    s->items=dx_realloc(s->items,(at+1)*sizeof(*s->items)); s->items[s->count++]=patch; return true;
}
bool format_apply(FormatStore *s,FormatRange range,unsigned mask,CellFormat value,char **error) {
    if (!range_valid(range) || !mask || (mask&~FORMAT_ALL) || !value_valid(value)) return fail(error,"Choose a valid cell format and worksheet range.");
    FormatSnapshot next=snapshot(s); bool ok=append(&next,(FormatPatch){range,mask,value},error);
    if (!ok) { format_snapshot_free(&next); return false; } format_commit(s,&next); return true;
}
bool format_history(FormatStore *s,bool redo) {
    FormatSnapshot *from=redo ? s->redo : s->undo,*to=redo ? s->undo : s->redo; size_t *fc=redo ? &s->redo_count : &s->undo_count,*tc=redo ? &s->undo_count : &s->redo_count;
    if (!*fc) return false; push(to,tc,(FormatSnapshot){s->items,s->count}); FormatSnapshot state=from[--*fc]; from[*fc]=(FormatSnapshot){0}; s->items=state.items; s->count=state.count; s->revision++; return true;
}
void format_trim_history(FormatStore *s,bool redo,size_t keep) {
    FormatSnapshot *stack=redo ? s->redo : s->undo; size_t *count=redo ? &s->redo_count : &s->undo_count;
    if (keep>=*count) return; size_t drop=*count-keep; for (size_t i=0;i<drop;i++) format_snapshot_free(&stack[i]); memmove(stack,stack+drop,keep*sizeof(*stack)); memset(stack+keep,0,drop*sizeof(*stack)); *count=keep;
}
bool format_plan_move(const FormatStore *s,FormatRange range,int64_t dr,int dc,FormatSnapshot *out,char **error) {
    *out=(FormatSnapshot){0}; if (!range_valid(range) || dr<-(int64_t)DX_SHEET_ROWS || dr>(int64_t)DX_SHEET_ROWS || dc<-16384 || dc>16384 ||
        (dr<0 && range.r0<(uint64_t)-dr) || (dc<0 && range.c0<(size_t)-dc) || (int64_t)range.r1+dr>=(int64_t)DX_SHEET_ROWS || (int64_t)range.c1+dc>=16384) return fail(error,"Formatting moves stay inside the worksheet.");
    *out=snapshot(s); if (!dr && !dc) return true;
    FormatRange destination={(uint64_t)((int64_t)range.r0+dr),(uint64_t)((int64_t)range.r1+dr),(size_t)((int64_t)range.c0+dc),(size_t)((int64_t)range.c1+dc)};
    bool relevant=false; for (size_t i=0;i<s->count;i++) relevant|=overlap(s->items[i].range,range) || overlap(s->items[i].range,destination);
    if (!relevant) return true;
    if (!append(out,(FormatPatch){range,FORMAT_ALL,format_default()},error) || !append(out,(FormatPatch){destination,FORMAT_ALL,format_default()},error)) goto failed;
    for (size_t i=0;i<s->count;i++) if (overlap(s->items[i].range,range)) {
        FormatPatch patch=s->items[i]; FormatRange r=patch.range;
        patch.range=(FormatRange){(r.r0>range.r0 ? r.r0 : range.r0)+dr,(r.r1<range.r1 ? r.r1 : range.r1)+dr,
            (size_t)((int64_t)(r.c0>range.c0 ? r.c0 : range.c0)+dc),(size_t)((int64_t)(r.c1<range.c1 ? r.c1 : range.c1)+dc)};
        if (!append(out,patch,error)) goto failed;
    }
    return true;
failed: format_snapshot_free(out); return false;
}
bool format_plan_axis(const FormatStore *s,bool rows,uint64_t at,uint64_t count,bool insert,FormatSnapshot *out,char **error) {
    *out=(FormatSnapshot){0}; uint64_t limit=rows ? DX_SHEET_ROWS : 16384;
    if (at>=limit || !count || count>limit-at) return fail(error,"Formatting changes stay inside the worksheet.");
    *out=snapshot(s); size_t n=0;
    for (size_t i=0;i<s->count;i++) {
        FormatPatch patch=s->items[i]; uint64_t lo=rows ? patch.range.r0 : patch.range.c0,hi=rows ? patch.range.r1 : patch.range.c1;
        if (insert) { if (lo>=at) lo+=count; if (hi>=at) hi+=count; if (lo>=limit) continue; if (hi>=limit) hi=limit-1; }
        else if (hi>=at) {
            if (lo>=at && hi<at+count) continue;
            if (lo>=at) lo=lo<at+count ? at : lo-count;
            hi=hi<at+count ? at-1 : hi-count;
        }
        if (rows) { patch.range.r0=lo; patch.range.r1=hi; } else { patch.range.c0=(size_t)lo; patch.range.c1=(size_t)hi; }
        out->items[n++]=patch;
    }
    out->count=n; return true;
}
