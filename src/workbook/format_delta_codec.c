#include "format_delta_codec.h"
#include <string.h>
/* Extending stored style fields/masks or exceeding wire widths requires an
   explicit family-codec decision, never silent struct-field/bit truncation. */
typedef char FormatDeltaStyleFits[(sizeof(CellFormat)==7 && FORMAT_ALL==255) ? 1 : -1];
typedef char FormatDeltaCountsFit[(DX_FORMAT_PATCHES<=UINT16_MAX && DX_FORMAT_GUARDS<=UINT8_MAX &&
    (uint64_t)DX_FORMAT_PATCHES*DX_FORMAT_GUARDS<=UINT16_MAX && DX_FORMAT_RECORDS<=UINT16_MAX &&
    DX_FORMAT_FIELDS<=UINT16_MAX && DX_FORMAT_SCOPE_BYTES<=UINT32_MAX) ? 1 : -1];
static FormatStore store(FormatSnapshotView v) {
    return (FormatStore){.items=(FormatPatch *)v.items,.count=v.count,.scopes=(FormatScope *)v.scopes,.scope_count=v.scope_count,.source_aware=v.source_aware};
}
FormatSnapshotView format_delta_store_view(const FormatStore *s) {
    return s ? (FormatSnapshotView){s->items,s->count,s->scopes,s->scope_count,s->source_aware} : (FormatSnapshotView){0};
}
FormatSnapshotView format_delta_snapshot_view(const FormatSnapshot *s) {
    return s ? (FormatSnapshotView){s->items,s->count,s->scopes,s->scope_count,s->source_aware} : (FormatSnapshotView){0};
}
static bool valid(FormatSnapshotView v) {
    FormatStore s=store(v); if (!format_valid(&s,NULL)) return false;
    for (size_t i=0;i<v.count;i++) if (v.items[i].range.c0>UINT16_MAX || v.items[i].range.c1>UINT16_MAX || v.items[i].selector>FORMAT_RECORDS) return false;
    return true;
}
static bool same(FormatSnapshotView a,FormatSnapshotView b) {
    FormatStore s=store(a); FormatSnapshot t={.items=(FormatPatch *)b.items,.count=b.count,.scopes=(FormatScope *)b.scopes,.scope_count=b.scope_count,.source_aware=b.source_aware};
    return format_snapshot_matches(&s,&t);
}
static size_t snapshot_bytes(FormatSnapshotView v) {
    size_t bytes=DX_FORMAT_SNAPSHOT_HEADER_BYTES+v.count*DX_FORMAT_PATCH_HEADER_BYTES;
    for (size_t i=0;i<v.scope_count;i++) bytes+=12+v.scopes[i].bytes;
    for (size_t i=0;i<v.count;i++) bytes+=2*v.items[i].guard_count+8*v.items[i].record_count+2*v.items[i].field_count;
    return bytes;
}
FormatDeltaStatus format_delta_encoded_size(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,size_t *bytes) {
    if (!sheet || !valid(before) || !valid(after)) return FORMAT_DELTA_INVALID;
    if (same(before,after)) return FORMAT_DELTA_NOOP;
    size_t count=DX_FORMAT_DELTA_HEADER_BYTES+snapshot_bytes(before)+snapshot_bytes(after);
    if (count>DX_FORMAT_DELTA_MAX_BYTES) return FORMAT_DELTA_INVALID;
    if (bytes) *bytes=count; return FORMAT_DELTA_OK;
}
typedef struct { unsigned char *p; } Writer;
static void put(Writer *w,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) { *w->p++=(unsigned char)(value&255u); value>>=8; } }
static void write_snapshot(Writer *w,FormatSnapshotView v) {
    put(w,v.source_aware,1); put(w,0,1); put(w,v.count,2); put(w,v.scope_count,2); put(w,0,2);
    for (size_t i=0;i<v.scope_count;i++) {
        put(w,v.scopes[i].owner,8); put(w,v.scopes[i].bytes,4);
        memcpy(w->p,v.scopes[i].definition,v.scopes[i].bytes); w->p+=v.scopes[i].bytes;
    }
    for (size_t i=0;i<v.count;i++) {
        const FormatPatch *p=v.items+i;
        put(w,p->range.r0,8); put(w,p->range.r1,8); put(w,p->range.c0,2); put(w,p->range.c1,2);
        put(w,p->mask,1); put(w,p->value.flags,1); put(w,p->value.align,1); put(w,p->value.color,1);
        put(w,p->value.size,1); put(w,p->value.number,1); put(w,p->value.decimals,1);
        put(w,p->selector,1); put(w,p->guard_count,1); put(w,0,1); put(w,p->record_count,2); put(w,p->field_count,2);
        for (size_t j=0;j<p->guard_count;j++) put(w,p->guard_ids[j],2);
        for (size_t j=0;j<p->record_count;j++) put(w,p->record_ids[j],8);
        for (size_t j=0;j<p->field_count;j++) put(w,p->fields[j],2);
    }
}
FormatDeltaStatus format_delta_encode(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,void *arena,size_t capacity,size_t *written) {
    if (!written) return FORMAT_DELTA_INVALID;
    size_t bytes=0; FormatDeltaStatus status=format_delta_encoded_size(sheet,before,after,&bytes);
    if (status!=FORMAT_DELTA_OK) return status;
    if (!arena) return FORMAT_DELTA_INVALID;
    if (bytes>capacity) return FORMAT_DELTA_LIMIT;
    Writer w={arena}; memcpy(w.p,"DXFD",4); w.p+=4;
    put(&w,DX_FORMAT_DELTA_ENCODING,2); put(&w,0,2); put(&w,sheet,8);
    put(&w,snapshot_bytes(before),4); put(&w,snapshot_bytes(after),4);
    write_snapshot(&w,before); write_snapshot(&w,after); *written=bytes; return FORMAT_DELTA_OK;
}
typedef struct { const unsigned char *data; size_t bytes,at; bool ok; } Reader;
static const unsigned char *span(Reader *r,size_t bytes) {
    if (!r->ok || bytes>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *data=r->data+r->at; r->at+=bytes; return data;
}
static uint64_t get(Reader *r,unsigned bytes) {
    const unsigned char *data=span(r,bytes); uint64_t value=0;
    if (data) for (unsigned i=0;i<bytes;i++) value|=(uint64_t)data[i]<<(8*i); return value;
}
static bool charge(size_t *total,size_t bytes) {
    if (bytes>DX_FORMAT_CURRENT_BYTES-*total) return false;
    *total+=bytes; return true;
}
static bool read_snapshot(const void *data,size_t bytes,FormatSnapshot *out,size_t *owned,size_t *scratch) {
    Reader r={data,bytes,0,true}; unsigned mode=(unsigned)get(&r,1); unsigned reserved=(unsigned)get(&r,1);
    size_t count=(size_t)get(&r,2); size_t scopes=(size_t)get(&r,2); reserved|=(unsigned)get(&r,2);
    if (!r.ok || mode>1 || reserved || count>DX_FORMAT_PATCHES || scopes>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) return false;
    size_t total=0;
    if (!charge(&total,count*sizeof(FormatPatch)) || !charge(&total,scopes*sizeof(FormatScope))) return false;
    if (out) {
        *out=(FormatSnapshot){.count=count,.scope_count=scopes,.source_aware=mode!=0};
        if (count) out->items=dx_alloc(count*sizeof(*out->items));
        if (scopes) out->scopes=dx_alloc(scopes*sizeof(*out->scopes));
    }
    for (size_t i=0;i<scopes;i++) {
        uint64_t owner=get(&r,8); size_t n=(size_t)get(&r,4);
        if (!r.ok || !n || n>=DX_FORMAT_SCOPE_BYTES || !charge(&total,n+1)) return false;
        const void *definition=span(&r,n); if (!r.ok) return false;
        if (out) { out->scopes[i]=(FormatScope){owner,dx_alloc(n+1),n}; memcpy(out->scopes[i].definition,definition,n); }
    }
    for (size_t i=0;i<count;i++) {
        FormatPatch p={0}; p.range.r0=get(&r,8); p.range.r1=get(&r,8); p.range.c0=(size_t)get(&r,2); p.range.c1=(size_t)get(&r,2);
        p.mask=(unsigned)get(&r,1); p.value.flags=(unsigned char)get(&r,1); p.value.align=(unsigned char)get(&r,1); p.value.color=(unsigned char)get(&r,1);
        p.value.size=(unsigned char)get(&r,1); p.value.number=(unsigned char)get(&r,1); p.value.decimals=(unsigned char)get(&r,1);
        p.selector=(FormatSelector)get(&r,1); p.guard_count=(size_t)get(&r,1); unsigned zero=(unsigned)get(&r,1);
        p.record_count=(size_t)get(&r,2); p.field_count=(size_t)get(&r,2);
        if (!r.ok || zero || p.guard_count>DX_FORMAT_GUARDS || p.record_count>DX_FORMAT_RECORDS || p.field_count>DX_FORMAT_FIELDS ||
            !charge(&total,p.guard_count*sizeof(size_t)) || !charge(&total,p.record_count*sizeof(uint64_t)) || !charge(&total,p.field_count*sizeof(size_t))) return false;
        size_t vector_bytes=2*p.guard_count+8*p.record_count+2*p.field_count;
        if (vector_bytes>r.bytes-r.at) return false;
        if (out) {
            out->items[i]=p;
            if (p.guard_count) out->items[i].guard_ids=dx_alloc(p.guard_count*sizeof(size_t));
            if (p.record_count) out->items[i].record_ids=dx_alloc(p.record_count*sizeof(uint64_t));
            if (p.field_count) out->items[i].fields=dx_alloc(p.field_count*sizeof(size_t));
        }
        for (size_t j=0;j<p.guard_count;j++) { size_t v=(size_t)get(&r,2); if (out) out->items[i].guard_ids[j]=v; }
        for (size_t j=0;j<p.record_count;j++) { uint64_t v=get(&r,8); if (out) out->items[i].record_ids[j]=v; }
        for (size_t j=0;j<p.field_count;j++) { size_t v=(size_t)get(&r,2); if (out) out->items[i].fields[j]=v; }
    }
    if (!r.ok || r.at!=r.bytes) return false;
    if (owned) *owned=total;
    if (scratch) *scratch=scopes>DX_FORMAT_GUARDS ? scopes*sizeof(const FormatScope *) : 0;
    return true;
}
void format_delta_free(FormatDelta *delta) {
    if (!delta) return;
    format_snapshot_free(&delta->before); format_snapshot_free(&delta->after); *delta=(FormatDelta){0};
}
FormatDeltaStatus format_delta_decode(const void *data,size_t bytes,size_t reserved,FormatDelta *out) {
    if (!data || !out || bytes<6 || bytes>DX_FORMAT_DELTA_MAX_BYTES || memcmp(data,"DXFD",4)) return FORMAT_DELTA_INVALID;
    Reader header={data,bytes,4,true}; if (get(&header,2)!=DX_FORMAT_DELTA_ENCODING) return FORMAT_DELTA_UNSUPPORTED;
    if (bytes<DX_FORMAT_DELTA_HEADER_BYTES || get(&header,2)) return FORMAT_DELTA_INVALID;
    uint64_t sheet=get(&header,8); size_t before=(size_t)get(&header,4); size_t after=(size_t)get(&header,4);
    if (!sheet || before<DX_FORMAT_SNAPSHOT_HEADER_BYTES || after<DX_FORMAT_SNAPSHOT_HEADER_BYTES ||
        before>DX_FORMAT_CURRENT_BYTES+DX_FORMAT_SNAPSHOT_HEADER_BYTES || after>DX_FORMAT_CURRENT_BYTES+DX_FORMAT_SNAPSHOT_HEADER_BYTES ||
        before+after!=bytes-DX_FORMAT_DELTA_HEADER_BYTES) return FORMAT_DELTA_INVALID;
    const unsigned char *a=(const unsigned char *)data+DX_FORMAT_DELTA_HEADER_BYTES,*b=a+before;
    size_t owned_a=0,owned_b=0,scratch_a=0,scratch_b=0;
    if (!read_snapshot(a,before,NULL,&owned_a,&scratch_a) || !read_snapshot(b,after,NULL,&owned_b,&scratch_b)) return FORMAT_DELTA_INVALID;
    if (owned_a+owned_b+(scratch_a>scratch_b ? scratch_a : scratch_b)>reserved) return FORMAT_DELTA_LIMIT;
    FormatDelta next={.sheet=sheet};
    if (!read_snapshot(a,before,&next.before,NULL,NULL) || !read_snapshot(b,after,&next.after,NULL,NULL) ||
        !valid(format_delta_snapshot_view(&next.before)) || !valid(format_delta_snapshot_view(&next.after)) ||
        same(format_delta_snapshot_view(&next.before),format_delta_snapshot_view(&next.after))) {
        format_delta_free(&next); return FORMAT_DELTA_INVALID;
    }
    format_delta_free(out); *out=next; return FORMAT_DELTA_OK;
}
