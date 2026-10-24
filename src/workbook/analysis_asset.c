#include "analysis_asset.h"
#include <string.h>
/* Encoding1 explicitly binds the existing finite enum tags. */
typedef char AnalysisTagsStable[(SOURCE_NONE==0 && SOURCE_CSV==1 && SOURCE_PARQUET==2 && SOURCE_DATABASE==3 && SOURCE_DEMO==4 &&
    FILTER_CONTAINS==0 && FILTER_EQUAL==1 && FILTER_GREATER==2 && FILTER_LESS==3 && FILTER_NULL==4 && FILTER_NOT_NULL==5 &&
    FILTER_IN==6 && FILTER_NOT_IN==7 && FILTER_DATE_IN==8 && FILTER_DATE_NOT_IN==9 && FILTER_BETWEEN==10 && FILTER_COMPOSITE==11 &&
    AGG_SUM==0 && AGG_COUNT==1 && AGG_DISTINCT==2 && AGG_AVG==3 && AGG_MIN==4 && AGG_MAX==5 && DX_ANALYSIS_FILTERS==16 && DX_ANALYSIS_SORTS==8) ? 1 : -1];
static bool utf8(const unsigned char *s,size_t n) {
    for (size_t i=0;i<n;) {
        unsigned c=s[i++],v,min; size_t tail;
        if (!c) return false; if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) { v=c&31; min=0x80; tail=1; }
        else if (c>=0xe0 && c<=0xef) { v=c&15; min=0x800; tail=2; }
        else if (c>=0xf0 && c<=0xf4) { v=c&7; min=0x10000; tail=3; } else return false;
        if (tail>n-i) return false;
        while (tail--) { unsigned b=s[i++]; if ((b&0xc0)!=0x80) return false; v=(v<<6)|(b&63); }
        if (v<min || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return false;
    }
    return true;
}
static size_t scratch(FilterOp op,size_t n,bool present) {
    if (!present || (op!=FILTER_BETWEEN && op!=FILTER_COMPOSITE)) return 0;
    /* Same logical parser bound as workbook_codec_read: old2048+new4096
       pointer arrays, input/copy scratch, then nonrecursive composite children. */
    size_t bytes=2*n+4098u+6144u*sizeof(char *);
    if (op==FILTER_COMPOSITE) bytes+=n+4u+16u*sizeof(char *);
    return bytes;
}
static bool text_measure(const char *s,AnalysisAssetMeasure *m,size_t *length) {
    *length=0; m->bytes+=4;
    if (!s) return true;
    size_t room=DX_ANALYSIS_ASSET_OWNED_BYTES-m->owned,n=0;
    while (n<room && s[n]) n++;
    if (n==room || !utf8((const unsigned char *)s,n)) return false;
    m->owned+=n+1; m->bytes+=n; *length=n; return true;
}
AnalysisAssetStatus analysis_asset_measure(uint64_t sheet,uint64_t owner,const Analysis *a,uint64_t owners,AnalysisAssetMeasure *out) {
    if (!sheet || owner>owners || !a || !out || (unsigned)a->kind>SOURCE_DEMO || (unsigned)a->filter_op>FILTER_OP_MAX ||
        (unsigned)a->aggregate>AGG_MAX || a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS) return ANALYSIS_ASSET_INVALID;
    AnalysisAssetMeasure m={.bytes=DX_ANALYSIS_ASSET_HEADER_BYTES+DX_ANALYSIS_ASSET_BODY_BYTES,.owned=sizeof(Analysis)};
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<9;i++) {
        size_t n=0; if (!text_measure(texts[i],&m,&n)) return ANALYSIS_ASSET_INVALID;
        if (i==5 && a->filter_column) m.validation_scratch=scratch(a->filter_op,n,texts[i]!=NULL);
    }
    for (size_t i=0;i<DX_ANALYSIS_FILTERS-1;i++) {
        const AnalysisFilter *f=a->filters+i;
        if (i>=a->filter_count) { if (f->column || f->value || f->op) return ANALYSIS_ASSET_INVALID; continue; }
        if ((unsigned)f->op>FILTER_OP_MAX) return ANALYSIS_ASSET_INVALID;
        m.bytes+=4; size_t n=0;
        if (!text_measure(f->column,&m,&n) || !text_measure(f->value,&m,&n)) return ANALYSIS_ASSET_INVALID;
        size_t next=scratch(f->op,n,f->value!=NULL); if (next>m.validation_scratch) m.validation_scratch=next;
    }
    for (size_t i=0;i<DX_ANALYSIS_SORTS-1;i++) {
        if (i>=a->sort_count) { if (a->sorts[i].column || a->sorts[i].descending) return ANALYSIS_ASSET_INVALID; continue; }
        m.bytes+=4; size_t n=0; if (!text_measure(a->sorts[i].column,&m,&n)) return ANALYSIS_ASSET_INVALID;
    }
    if (!analysis_query_valid(a,NULL) || !analysis_sorts_valid(a,NULL)) return ANALYSIS_ASSET_INVALID;
    *out=m; return ANALYSIS_ASSET_OK;
}
typedef struct { unsigned char *p; } Writer;
static void put(Writer *w,uint64_t v,unsigned n) { for (unsigned i=0;i<n;i++) { *w->p++=(unsigned char)(v&255u); v>>=8; } }
static void text_write(Writer *w,const char *s) {
    size_t n=s ? strlen(s) : 0; put(w,s ? n : UINT32_MAX,4);
    if (n) { memcpy(w->p,s,n); w->p+=n; }
}
AnalysisAssetStatus analysis_asset_encode(uint64_t sheet,uint64_t owner,const Analysis *a,uint64_t owners,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return ANALYSIS_ASSET_INVALID;
    AnalysisAssetMeasure m={0}; AnalysisAssetStatus status=analysis_asset_measure(sheet,owner,a,owners,&m);
    if (status!=ANALYSIS_ASSET_OK) return status;
    if (m.bytes>capacity || m.validation_scratch>reserved) return ANALYSIS_ASSET_LIMIT;
    if (!analysis_filters_valid(a,NULL)) return ANALYSIS_ASSET_INVALID;
    Writer w={arena}; memcpy(w.p,"DXAN",4); w.p+=4; put(&w,DX_ANALYSIS_ASSET_ENCODING,2); put(&w,0,2);
    put(&w,sheet,8); put(&w,owner,8); put(&w,m.bytes-DX_ANALYSIS_ASSET_HEADER_BYTES,4); put(&w,0,4);
    put(&w,a->kind,1); put(&w,(a->unique_rows ? 1u : 0u)|(a->descending ? 2u : 0u),1); put(&w,a->filter_op,1); put(&w,a->aggregate,1);
    put(&w,a->source_version,8); put(&w,a->filter_count,1); put(&w,a->sort_count,1); put(&w,0,2);
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<9;i++) text_write(&w,texts[i]);
    for (size_t i=0;i<a->filter_count;i++) { put(&w,a->filters[i].op,1); put(&w,0,3); text_write(&w,a->filters[i].column); text_write(&w,a->filters[i].value); }
    for (size_t i=0;i<a->sort_count;i++) { put(&w,a->sorts[i].descending ? 1 : 0,1); put(&w,0,3); text_write(&w,a->sorts[i].column); }
    *written=m.bytes; return ANALYSIS_ASSET_OK;
}
typedef struct { const unsigned char *data; size_t bytes,at,owned,scratch; bool ok; } Reader;
static const unsigned char *span(Reader *r,size_t n) {
    if (!r->ok || n>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *p=r->data+r->at; r->at+=n; return p;
}
static uint64_t get(Reader *r,unsigned n) { const unsigned char *p=span(r,n); uint64_t v=0; if (p) for (unsigned i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
static bool text_read(Reader *r,char **out,size_t *length) {
    size_t n=(size_t)get(r,4); *length=0;
    if (!r->ok || n==UINT32_MAX) return false;
    if (r->owned>=DX_ANALYSIS_ASSET_OWNED_BYTES || n>DX_ANALYSIS_ASSET_OWNED_BYTES-r->owned-1) { r->ok=false; return false; }
    const unsigned char *p=span(r,n);
    if (!p || !utf8(p,n)) { r->ok=false; return false; }
    r->owned+=n+1; *length=n;
    if (out) { *out=dx_alloc(n+1); if (n) memcpy(*out,p,n); (*out)[n]=0; }
    return true;
}
static bool body(Reader *r,Analysis *out) {
    unsigned kind=(unsigned)get(r,1),flags=(unsigned)get(r,1),op=(unsigned)get(r,1),aggregate=(unsigned)get(r,1);
    uint64_t version=get(r,8); size_t filters=(size_t)get(r,1),sorts=(size_t)get(r,1); unsigned reserved=(unsigned)get(r,2);
    if (!r->ok || kind>SOURCE_DEMO || flags>3 || op>FILTER_OP_MAX || aggregate>AGG_MAX || reserved || filters>=DX_ANALYSIS_FILTERS || sorts>=DX_ANALYSIS_SORTS) return false;
    if (out) *out=(Analysis){.kind=(SourceKind)kind,.unique_rows=(flags&1)!=0,.descending=(flags&2)!=0,.filter_op=(FilterOp)op,.aggregate=(Aggregate)aggregate,
        .source_version=version,.filter_count=filters,.sort_count=sorts};
    char **slots[9]={0}; if (out) {
        char **values[]={&out->path,&out->schema,&out->table,&out->query,&out->filter_column,&out->filter_value,&out->group_column,&out->measure_column,&out->sort_column};
        memcpy(slots,values,sizeof(slots));
    }
    bool primary=false;
    for (size_t i=0;i<9;i++) {
        size_t n=0; bool present=text_read(r,slots[i],&n);
        if (i==4) primary=present;
        if (i==5 && primary) r->scratch=scratch((FilterOp)op,n,present);
    }
    for (size_t i=0;i<filters && r->ok;i++) {
        op=(unsigned)get(r,1); reserved=(unsigned)get(r,3);
        if (!r->ok || op>FILTER_OP_MAX || reserved) return false;
        if (out) out->filters[i].op=(FilterOp)op;
        size_t n=0; text_read(r,out ? &out->filters[i].column : NULL,&n);
        bool present=text_read(r,out ? &out->filters[i].value : NULL,&n);
        size_t next=scratch((FilterOp)op,n,present); if (next>r->scratch) r->scratch=next;
    }
    for (size_t i=0;i<sorts && r->ok;i++) {
        flags=(unsigned)get(r,1); reserved=(unsigned)get(r,3);
        if (!r->ok || flags>1 || reserved) return false;
        if (out) out->sorts[i].descending=flags!=0;
        size_t n=0; text_read(r,out ? &out->sorts[i].column : NULL,&n);
    }
    return r->ok && r->at==r->bytes;
}
void analysis_asset_free(AnalysisAsset *a) { if (a) { analysis_free(&a->definition); *a=(AnalysisAsset){0}; } }
AnalysisAssetStatus analysis_asset_decode(const void *data,size_t bytes,uint64_t owners,size_t reserved,AnalysisAsset *out) {
    if (!data || !out || bytes<DX_ANALYSIS_ASSET_HEADER_BYTES || bytes>DX_ANALYSIS_ASSET_MAX_BYTES || memcmp(data,"DXAN",4)) return ANALYSIS_ASSET_INVALID;
    Reader r={.data=data,.bytes=bytes,.at=4,.owned=sizeof(Analysis),.ok=true};
    unsigned encoding=(unsigned)get(&r,2); if (encoding!=DX_ANALYSIS_ASSET_ENCODING) return ANALYSIS_ASSET_UNSUPPORTED;
    unsigned flags=(unsigned)get(&r,2); uint64_t sheet=get(&r,8),owner=get(&r,8); size_t n=(size_t)get(&r,4); unsigned zero=(unsigned)get(&r,4);
    if (flags || zero || !sheet || owner>owners || n!=bytes-DX_ANALYSIS_ASSET_HEADER_BYTES) return ANALYSIS_ASSET_INVALID;
    Reader start=r;
    if (!body(&r,NULL)) return ANALYSIS_ASSET_INVALID;
    if (r.owned>reserved || r.scratch>reserved-r.owned) return ANALYSIS_ASSET_LIMIT;
    AnalysisAsset next={.sheet=sheet,.owner=owner}; r=start;
    bool ok=body(&r,&next.definition); AnalysisAssetMeasure measured={0};
    ok=ok && analysis_asset_measure(sheet,owner,&next.definition,owners,&measured)==ANALYSIS_ASSET_OK &&
        measured.bytes==bytes && measured.owned==r.owned && measured.validation_scratch==r.scratch && analysis_filters_valid(&next.definition,NULL);
    if (!ok) { analysis_asset_free(&next); return ANALYSIS_ASSET_INVALID; }
    analysis_asset_free(out); *out=next; return ANALYSIS_ASSET_OK;
}
