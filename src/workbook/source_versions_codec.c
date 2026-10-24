#include "source_versions_codec.h"
#include <string.h>
typedef char SourceVersionCountsFit[(DX_SOURCE_VERSIONS<=UINT16_MAX && DX_SOURCE_SCOPE_TEXT_BYTES<=UINT32_MAX && DX_SOURCE_SCOPE_BYTES<=UINT32_MAX) ? 1 : -1];
static unsigned kind_tag(SourceKind kind) { return kind==SOURCE_CSV ? 1 : kind==SOURCE_PARQUET ? 2 : 0; }
SourceVersionsCodecStatus source_versions_asset_size(uint64_t sheet,const SourceVersions *versions,uint64_t owners,size_t *bytes) {
    if (!sheet || !source_versions_valid(versions,owners,NULL)) return SOURCE_VERSIONS_CODEC_INVALID;
    size_t total=DX_SOURCE_VERSIONS_HEADER_BYTES+versions->count*DX_SOURCE_VERSION_ROW_BYTES;
    for (size_t i=0;i<versions->count;i++) {
        if (!kind_tag(versions->items[i].kind)) return SOURCE_VERSIONS_CODEC_INVALID;
        total+=strlen(versions->items[i].path)+versions->items[i].raw.bytes;
    }
    if (total>DX_SOURCE_VERSIONS_MAX_BYTES) return SOURCE_VERSIONS_CODEC_INVALID;
    if (bytes) *bytes=total; return SOURCE_VERSIONS_CODEC_OK;
}
SourceVersionsCodecStatus source_versions_asset_select(uint64_t sheet,const SourceVersions *ledger,uint64_t owners,
    const bool *selected,size_t selection_count,size_t reserved,SourceVersionsAsset *out) {
    if (!sheet || !out || ledger==&out->versions || !source_versions_valid(ledger,owners,NULL) ||
        selection_count!=ledger->count || (selection_count && !selected)) return SOURCE_VERSIONS_CODEC_INVALID;
    size_t count=0,bytes=0;
    for (size_t i=0;i<ledger->count;i++) if (selected[i]) { count++; bytes+=sizeof(SourceVersion)+strlen(ledger->items[i].path)+1+ledger->items[i].raw.bytes; }
    if (bytes>reserved) return SOURCE_VERSIONS_CODEC_LIMIT;
    SourceVersionsAsset next={.sheet=sheet,.versions={.high_water=ledger->high_water,.count=count,.capacity=count,.bytes=bytes}};
    if (count) next.versions.items=dx_alloc(count*sizeof(SourceVersion));
    size_t at=0;
    for (size_t i=0;i<ledger->count;i++) if (selected[i]) {
        const SourceVersion *from=ledger->items+i; SourceVersion *to=next.versions.items+at++; *to=*from; to->path=dx_strdup(from->path);
        if (from->raw.bytes) { to->raw.data=dx_alloc(from->raw.bytes); memcpy(to->raw.data,from->raw.data,from->raw.bytes); }
    }
    source_versions_asset_free(out); *out=next; return SOURCE_VERSIONS_CODEC_OK;
}
typedef struct { unsigned char *p; } Writer;
static void put(Writer *w,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) { *w->p++=(unsigned char)(value&255u); value>>=8; } }
SourceVersionsCodecStatus source_versions_asset_encode(uint64_t sheet,const SourceVersions *versions,uint64_t owners,void *arena,size_t capacity,size_t *written) {
    if (!written) return SOURCE_VERSIONS_CODEC_INVALID;
    size_t bytes=0; SourceVersionsCodecStatus status=source_versions_asset_size(sheet,versions,owners,&bytes);
    if (status!=SOURCE_VERSIONS_CODEC_OK) return status;
    if (!arena) return SOURCE_VERSIONS_CODEC_INVALID;
    if (bytes>capacity) return SOURCE_VERSIONS_CODEC_LIMIT;
    Writer w={arena}; memcpy(w.p,"DXSV",4); w.p+=4; put(&w,DX_SOURCE_VERSIONS_ENCODING,2); put(&w,0,2);
    put(&w,sheet,8); put(&w,versions->high_water,8); put(&w,versions->count,2); put(&w,0,2); put(&w,0,4);
    for (size_t i=0;i<versions->count;i++) {
        const SourceVersion *e=versions->items+i; size_t n=strlen(e->path);
        put(&w,e->version,8); put(&w,e->owner,8); put(&w,kind_tag(e->kind),1); put(&w,0,1); put(&w,0,2); put(&w,n,4); put(&w,e->raw.bytes,4);
        memcpy(w.p,e->path,n); w.p+=n;
        if (e->raw.bytes) { memcpy(w.p,e->raw.data,e->raw.bytes); w.p+=e->raw.bytes; }
    }
    *written=bytes; return SOURCE_VERSIONS_CODEC_OK;
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
static bool read_rows(Reader *r,size_t count,uint64_t high,uint64_t owners,SourceVersions *out,size_t *owned) {
    size_t total=count*sizeof(SourceVersion); uint64_t previous=0;
    if (total>DX_SOURCE_VERSIONS_BYTES) return false;
    if (out) {
        *out=(SourceVersions){.high_water=high,.count=count,.capacity=count};
        if (count) out->items=dx_alloc(count*sizeof(*out->items));
    }
    for (size_t i=0;i<count;i++) {
        uint64_t version=get(r,8); uint64_t owner=get(r,8); unsigned kind=(unsigned)get(r,1);
        unsigned reserved=(unsigned)get(r,1); reserved|=(unsigned)get(r,2);
        size_t n=(size_t)get(r,4); size_t raw_bytes=(size_t)get(r,4);
        if (!r->ok || !version || version>high || (i && version<=previous) || owner>owners || kind<1 || kind>2 || reserved ||
            !n || n>DX_SOURCE_SCOPE_TEXT_BYTES || raw_bytes>=DX_SOURCE_SCOPE_BYTES || n+1+raw_bytes>DX_SOURCE_VERSIONS_BYTES-total) return false;
        const unsigned char *path=span(r,n); const unsigned char *raw=span(r,raw_bytes);
        if (!r->ok || memchr(path,0,n)) return false;
        previous=version; total+=n+1+raw_bytes;
        if (out) {
            SourceVersion *e=out->items+i; *e=(SourceVersion){.version=version,.owner=owner,.kind=kind==1 ? SOURCE_CSV : SOURCE_PARQUET,.path=dx_alloc(n+1)};
            memcpy(e->path,path,n);
            if (raw_bytes) { e->raw=(SourcePacket){dx_alloc(raw_bytes),raw_bytes}; memcpy(e->raw.data,raw,raw_bytes); }
        }
    }
    if (!r->ok || r->at!=r->bytes) return false;
    if (out) out->bytes=total;
    if (owned) *owned=total; return true;
}
void source_versions_asset_free(SourceVersionsAsset *asset) {
    if (!asset) return;
    source_versions_free(&asset->versions); *asset=(SourceVersionsAsset){0};
}
SourceVersionsCodecStatus source_versions_asset_decode(const void *data,size_t bytes,uint64_t owners,size_t reserved,SourceVersionsAsset *out) {
    if (!data || !out || bytes<6 || bytes>DX_SOURCE_VERSIONS_MAX_BYTES || memcmp(data,"DXSV",4)) return SOURCE_VERSIONS_CODEC_INVALID;
    Reader header={data,bytes,4,true}; if (get(&header,2)!=DX_SOURCE_VERSIONS_ENCODING) return SOURCE_VERSIONS_CODEC_UNSUPPORTED;
    if (bytes<DX_SOURCE_VERSIONS_HEADER_BYTES || get(&header,2)) return SOURCE_VERSIONS_CODEC_INVALID;
    uint64_t sheet=get(&header,8); uint64_t high=get(&header,8); size_t count=(size_t)get(&header,2);
    unsigned zero=(unsigned)get(&header,2); zero|=(unsigned)get(&header,4);
    if (!sheet || zero || count>DX_SOURCE_VERSIONS) return SOURCE_VERSIONS_CODEC_INVALID;
    Reader peek=header; size_t owned=0;
    if (!read_rows(&peek,count,high,owners,NULL,&owned)) return SOURCE_VERSIONS_CODEC_INVALID;
    if (owned>reserved) return SOURCE_VERSIONS_CODEC_LIMIT;
    SourceVersionsAsset next={.sheet=sheet};
    if (!read_rows(&header,count,high,owners,&next.versions,NULL) || !source_versions_valid(&next.versions,owners,NULL)) {
        source_versions_asset_free(&next); return SOURCE_VERSIONS_CODEC_INVALID;
    }
    source_versions_asset_free(out); *out=next; return SOURCE_VERSIONS_CODEC_OK;
}
