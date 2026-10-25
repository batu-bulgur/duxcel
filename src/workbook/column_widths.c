#include "column_widths.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#if FLT_RADIX!=2 || FLT_MANT_DIG!=24 || FLT_MAX_EXP!=128
#error Column width assets require IEEE754 binary32 float
#endif
typedef char WidthFloat32[(sizeof(float)==4)?1:-1];
uint32_t column_width_bits(float width) { uint32_t bits; memcpy(&bits,&width,4); return bits; }
float column_width_value(uint32_t bits) { float width; memcpy(&width,&bits,4); return width; }
static bool width_valid(uint32_t bits) { return bits>=UINT32_C(0x42500000) && bits<=UINT32_C(0x44960000); }
ColumnWidthView column_width_view(const ColumnWidthState *s) { return s ? (ColumnWidthView){s->entries,s->count} : (ColumnWidthView){0}; }
bool column_width_valid(ColumnWidthView v) {
    if (v.count>DX_COLUMN_WIDTH_COLUMNS || (v.count && !v.entries)) return false;
    for (size_t i=0;i<v.count;i++) if (v.entries[i].column>=DX_COLUMN_WIDTH_COLUMNS || !width_valid(v.entries[i].bits) ||
        v.entries[i].bits==DX_COLUMN_WIDTH_DEFAULT_BITS || (i && v.entries[i-1].column>=v.entries[i].column)) return false;
    return true;
}
bool column_width_equal(ColumnWidthView a,ColumnWidthView b) {
    if (!column_width_valid(a) || !column_width_valid(b) || a.count!=b.count) return false;
    for (size_t i=0;i<a.count;i++) if (a.entries[i].column!=b.entries[i].column || a.entries[i].bits!=b.entries[i].bits) return false;
    return true;
}
size_t column_width_owned(ColumnWidthView v) { return column_width_valid(v) ? v.count*sizeof(ColumnWidthEntry) : SIZE_MAX; }
uint32_t column_width_at(ColumnWidthView v,size_t column) {
    if (column>=DX_COLUMN_WIDTH_COLUMNS || !column_width_valid(v)) return 0;
    size_t lo=0,hi=v.count; while (lo<hi) { size_t at=lo+(hi-lo)/2; if (v.entries[at].column<column) lo=at+1; else hi=at; }
    return lo<v.count && v.entries[lo].column==column ? v.entries[lo].bits : DX_COLUMN_WIDTH_DEFAULT_BITS;
}
void column_width_free(ColumnWidthState *s) { if (s) { free(s->entries); *s=(ColumnWidthState){0}; } }
static void replace(ColumnWidthState *out,ColumnWidthState next) { column_width_free(out); *out=next; }
ColumnWidthStatus column_width_copy(ColumnWidthView v,size_t reserved,ColumnWidthState *out) {
    if (!out || !column_width_valid(v)) return COLUMN_WIDTH_INVALID;
    if (v.count*sizeof(ColumnWidthEntry)>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthState next={.count=v.count};
    if (v.count) { next.entries=dx_alloc(v.count*sizeof(*next.entries)); memcpy(next.entries,v.entries,v.count*sizeof(*next.entries)); }
    replace(out,next); return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_capture(const float *widths,size_t count,size_t reserved,ColumnWidthState *out) {
    if (!out || count>DX_COLUMN_WIDTH_COLUMNS || (count && !widths)) return COLUMN_WIDTH_INVALID;
    size_t n=0;
    for (size_t i=0;i<count;i++) { uint32_t bits=column_width_bits(widths[i]); if (!width_valid(bits)) return COLUMN_WIDTH_INVALID; n+=bits!=DX_COLUMN_WIDTH_DEFAULT_BITS; }
    if (n*sizeof(ColumnWidthEntry)>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthState next={.count=n}; if (n) next.entries=dx_alloc(n*sizeof(*next.entries));
    for (size_t i=0,at=0;i<count;i++) { uint32_t bits=column_width_bits(widths[i]); if (bits!=DX_COLUMN_WIDTH_DEFAULT_BITS) next.entries[at++]=(ColumnWidthEntry){(uint16_t)i,bits}; }
    replace(out,next); return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_resize(ColumnWidthView before,size_t column,float width,size_t reserved,ColumnWidthState *out) {
    uint32_t bits=column_width_bits(width);
    if (!out || !column_width_valid(before) || column>=DX_COLUMN_WIDTH_COLUMNS || !width_valid(bits)) return COLUMN_WIDTH_INVALID;
    uint32_t old=column_width_at(before,column); if (old==bits) return COLUMN_WIDTH_NOOP;
    size_t n=before.count+(old==DX_COLUMN_WIDTH_DEFAULT_BITS)-(bits==DX_COLUMN_WIDTH_DEFAULT_BITS);
    if (n*sizeof(ColumnWidthEntry)>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthState next={.count=n}; if (n) next.entries=dx_alloc(n*sizeof(*next.entries));
    size_t at=0; bool inserted=false;
    for (size_t i=0;i<before.count;i++) {
        ColumnWidthEntry e=before.entries[i];
        if (!inserted && e.column>=column) { if (bits!=DX_COLUMN_WIDTH_DEFAULT_BITS) next.entries[at++]=(ColumnWidthEntry){(uint16_t)column,bits}; inserted=true; }
        if (e.column!=column) next.entries[at++]=e;
    }
    if (!inserted && bits!=DX_COLUMN_WIDTH_DEFAULT_BITS) next.entries[at++]=(ColumnWidthEntry){(uint16_t)column,bits};
    replace(out,next); return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_shift(ColumnWidthView before,size_t column,bool insert,size_t reserved,ColumnWidthState *out) {
    if (!out || !column_width_valid(before) || column>=DX_COLUMN_WIDTH_COLUMNS) return COLUMN_WIDTH_INVALID;
    size_t n=0; bool changed=false;
    for (size_t i=0;i<before.count;i++) { size_t c=before.entries[i].column; bool removed=insert ? c==DX_COLUMN_WIDTH_COLUMNS-1 && c>=column : c==column; changed|=c>=column; n+=!removed; }
    if (!changed) return COLUMN_WIDTH_NOOP;
    if (n*sizeof(ColumnWidthEntry)>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthState next={.count=n}; if (n) next.entries=dx_alloc(n*sizeof(*next.entries));
    for (size_t i=0,at=0;i<before.count;i++) {
        ColumnWidthEntry e=before.entries[i];
        if ((insert && e.column==DX_COLUMN_WIDTH_COLUMNS-1 && e.column>=column) || (!insert && e.column==column)) continue;
        if (e.column>=column) e.column=(uint16_t)(insert ? e.column+1 : e.column-1);
        next.entries[at++]=e;
    }
    replace(out,next); return COLUMN_WIDTH_OK;
}
static void put(unsigned char *p,uint32_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) p[i]=(unsigned char)(value>>(i*8)); }
static uint32_t get(const unsigned char *p,unsigned bytes) { uint32_t v=0; for (unsigned i=0;i<bytes;i++) v|=(uint32_t)p[i]<<(8*i); return v; }
ColumnWidthStatus column_width_delta_size(ColumnWidthView a,ColumnWidthView b,size_t *bytes) {
    if (!bytes || !column_width_valid(a) || !column_width_valid(b)) return COLUMN_WIDTH_INVALID;
    if (column_width_equal(a,b)) return COLUMN_WIDTH_NOOP;
    *bytes=DX_COLUMN_WIDTH_HEADER_BYTES+6*(a.count+b.count); return COLUMN_WIDTH_OK;
}
static bool overlap(const void *a,size_t na,const void *b,size_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if (!na || !nb) return false;
    if (x>UINTPTR_MAX-na || y>UINTPTR_MAX-nb) return true;
    return x<y+nb && y<x+na;
}
ColumnWidthStatus column_width_encode(ColumnWidthView a,ColumnWidthView b,void *arena,size_t capacity,size_t *written) {
    size_t bytes=0; ColumnWidthStatus status=column_width_delta_size(a,b,&bytes); if (status) return status;
    if (!arena || !written) return COLUMN_WIDTH_INVALID; if (bytes>capacity) return COLUMN_WIDTH_LIMIT;
    if (overlap(arena,bytes,a.entries,a.count*sizeof(ColumnWidthEntry)) || overlap(arena,bytes,b.entries,b.count*sizeof(ColumnWidthEntry)) ||
        overlap(arena,bytes,written,sizeof(*written)) || overlap(written,sizeof(*written),a.entries,a.count*sizeof(ColumnWidthEntry)) ||
        overlap(written,sizeof(*written),b.entries,b.count*sizeof(ColumnWidthEntry))) return COLUMN_WIDTH_INVALID;
    unsigned char *p=arena; memcpy(p,"DXCW",4); put(p+4,DX_COLUMN_WIDTH_ENCODING,2); put(p+6,0,2); put(p+8,DX_COLUMN_WIDTH_COLUMNS,4);
    put(p+12,(uint32_t)a.count,2); put(p+14,(uint32_t)b.count,2); put(p+16,0,4); p+=DX_COLUMN_WIDTH_HEADER_BYTES;
    ColumnWidthView sides[]={a,b};
    for (size_t side=0;side<2;side++) for (size_t i=0;i<sides[side].count;i++,p+=6) { put(p,sides[side].entries[i].column,2); put(p+2,sides[side].entries[i].bits,4); }
    *written=bytes; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_preflight(const void *data,size_t bytes,ColumnWidthInfo *out) {
    if (!data || !out || bytes<DX_COLUMN_WIDTH_HEADER_BYTES) return COLUMN_WIDTH_INVALID;
    const unsigned char *p=data; if (memcmp(p,"DXCW",4)) return COLUMN_WIDTH_INVALID;
    if (get(p+4,2)!=DX_COLUMN_WIDTH_ENCODING) return COLUMN_WIDTH_UNSUPPORTED;
    size_t a=get(p+12,2),b=get(p+14,2);
    if (get(p+6,2) || get(p+8,4)!=DX_COLUMN_WIDTH_COLUMNS || get(p+16,4) || a>DX_COLUMN_WIDTH_COLUMNS || b>DX_COLUMN_WIDTH_COLUMNS ||
        bytes!=DX_COLUMN_WIDTH_HEADER_BYTES+6*(a+b)) return COLUMN_WIDTH_INVALID;
    p+=DX_COLUMN_WIDTH_HEADER_BYTES; size_t counts[]={a,b};
    for (size_t side=0;side<2;side++) for (size_t i=0;i<counts[side];i++,p+=6) {
        uint32_t column=get(p,2),bits=get(p+2,4);
        if (column>=DX_COLUMN_WIDTH_COLUMNS || !width_valid(bits) || bits==DX_COLUMN_WIDTH_DEFAULT_BITS || (i && get(p-6,2)>=column)) return COLUMN_WIDTH_INVALID;
    }
    const unsigned char *start=(const unsigned char *)data+DX_COLUMN_WIDTH_HEADER_BYTES;
    if (a==b && !memcmp(start,start+6*a,6*a)) return COLUMN_WIDTH_NOOP;
    *out=(ColumnWidthInfo){bytes,(a+b)*sizeof(ColumnWidthEntry),a,b}; return COLUMN_WIDTH_OK;
}
ColumnWidthPair column_width_delta_view(const ColumnWidthDelta *d) { return d ? (ColumnWidthPair){column_width_view(&d->before),column_width_view(&d->after)} : (ColumnWidthPair){0}; }
ColumnWidthPair column_width_inverse(ColumnWidthPair pair) { return (ColumnWidthPair){pair.after,pair.before}; }
ColumnWidthStatus column_width_extract(const void *data,size_t bytes,bool after,size_t reserved,ColumnWidthState *out) {
    if (!out) return COLUMN_WIDTH_INVALID; ColumnWidthInfo info;
    ColumnWidthStatus status=column_width_preflight(data,bytes,&info); if (status) return status;
    size_t count=after ? info.after_count : info.before_count;
    if (count*sizeof(ColumnWidthEntry)>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthState next={.count=count}; if (count) next.entries=dx_alloc(count*sizeof(*next.entries));
    const unsigned char *p=(const unsigned char *)data+DX_COLUMN_WIDTH_HEADER_BYTES+(after ? info.before_count*6 : 0);
    for (size_t i=0;i<count;i++,p+=6) next.entries[i]=(ColumnWidthEntry){(uint16_t)get(p,2),get(p+2,4)};
    replace(out,next); return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_decode(const void *data,size_t bytes,size_t reserved,ColumnWidthDelta *out) {
    if (!out) return COLUMN_WIDTH_INVALID; ColumnWidthInfo info;
    ColumnWidthStatus status=column_width_preflight(data,bytes,&info); if (status) return status;
    if (info.owned>reserved) return COLUMN_WIDTH_LIMIT;
    ColumnWidthDelta next={.before.count=info.before_count,.after.count=info.after_count}; ColumnWidthState *sides[]={&next.before,&next.after};
    const unsigned char *p=(const unsigned char *)data+DX_COLUMN_WIDTH_HEADER_BYTES;
    for (size_t side=0;side<2;side++) {
        ColumnWidthState *s=sides[side]; if (s->count) s->entries=dx_alloc(s->count*sizeof(*s->entries));
        for (size_t i=0;i<s->count;i++,p+=6) s->entries[i]=(ColumnWidthEntry){(uint16_t)get(p,2),get(p+2,4)};
    }
    column_width_delta_free(out); *out=next; return COLUMN_WIDTH_OK;
}
void column_width_delta_free(ColumnWidthDelta *d) { if (d) { column_width_free(&d->before); column_width_free(&d->after); } }
bool column_width_matches(const void *data,size_t bytes,bool after,ColumnWidthView current) {
    ColumnWidthInfo info; if (!column_width_valid(current) || column_width_preflight(data,bytes,&info)!=COLUMN_WIDTH_OK) return false;
    size_t count=after ? info.after_count : info.before_count; if (count!=current.count) return false;
    const unsigned char *p=(const unsigned char *)data+DX_COLUMN_WIDTH_HEADER_BYTES+(after ? info.before_count*6 : 0);
    for (size_t i=0;i<count;i++,p+=6) if (get(p,2)!=current.entries[i].column || get(p+2,4)!=current.entries[i].bits) return false;
    return true;
}
