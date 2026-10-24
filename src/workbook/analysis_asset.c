#include "analysis_asset.h"
#include <string.h>
/* Encoding1 explicitly binds the existing finite enum tags. */
typedef char AnalysisTagsStable[(SOURCE_NONE==0 && SOURCE_CSV==1 && SOURCE_PARQUET==2 && SOURCE_DATABASE==3 && SOURCE_DEMO==4 &&
    FILTER_CONTAINS==0 && FILTER_EQUAL==1 && FILTER_GREATER==2 && FILTER_LESS==3 && FILTER_NULL==4 && FILTER_NOT_NULL==5 &&
    FILTER_IN==6 && FILTER_NOT_IN==7 && FILTER_DATE_IN==8 && FILTER_DATE_NOT_IN==9 && FILTER_BETWEEN==10 && FILTER_COMPOSITE==11 &&
    AGG_SUM==0 && AGG_COUNT==1 && AGG_DISTINCT==2 && AGG_AVG==3 && AGG_MIN==4 && AGG_MAX==5 && DX_ANALYSIS_FILTERS==16 && DX_ANALYSIS_SORTS==8) ? 1 : -1];
AnalysisAssetStatus analysis_asset_measure(uint64_t sheet,uint64_t owner,const Analysis *a,uint64_t owners,AnalysisAssetMeasure *out) {
    if (!sheet || owner>owners || !a || !out || (unsigned)a->kind>SOURCE_DEMO || (unsigned)a->filter_op>FILTER_OP_MAX ||
        (unsigned)a->aggregate>AGG_MAX || a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS) return ANALYSIS_ASSET_INVALID;
    AnalysisAssetMeasure m={.bytes=DX_ANALYSIS_ASSET_HEADER_BYTES+DX_ANALYSIS_ASSET_BODY_BYTES,.owned=sizeof(Analysis)};
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<9;i++) {
        size_t n=0; if (!definition_wire_text_measure(texts[i],&m,DX_ANALYSIS_ASSET_OWNED_BYTES,&n)) return ANALYSIS_ASSET_INVALID;
        if (i==5 && a->filter_column) m.validation_scratch=definition_wire_filter_scratch(a->filter_op,n,texts[i]!=NULL);
    }
    for (size_t i=0;i<DX_ANALYSIS_FILTERS-1;i++) {
        const AnalysisFilter *f=a->filters+i;
        if (i>=a->filter_count) { if (f->column || f->value || f->op) return ANALYSIS_ASSET_INVALID; continue; }
        if ((unsigned)f->op>FILTER_OP_MAX) return ANALYSIS_ASSET_INVALID;
        m.bytes+=4; size_t n=0;
        if (!definition_wire_text_measure(f->column,&m,DX_ANALYSIS_ASSET_OWNED_BYTES,&n) || !definition_wire_text_measure(f->value,&m,DX_ANALYSIS_ASSET_OWNED_BYTES,&n)) return ANALYSIS_ASSET_INVALID;
        size_t next=definition_wire_filter_scratch(f->op,n,f->value!=NULL); if (next>m.validation_scratch) m.validation_scratch=next;
    }
    for (size_t i=0;i<DX_ANALYSIS_SORTS-1;i++) {
        if (i>=a->sort_count) { if (a->sorts[i].column || a->sorts[i].descending) return ANALYSIS_ASSET_INVALID; continue; }
        m.bytes+=4; size_t n=0; if (!definition_wire_text_measure(a->sorts[i].column,&m,DX_ANALYSIS_ASSET_OWNED_BYTES,&n)) return ANALYSIS_ASSET_INVALID;
    }
    if (!analysis_query_valid(a,NULL) || !analysis_sorts_valid(a,NULL)) return ANALYSIS_ASSET_INVALID;
    *out=m; return ANALYSIS_ASSET_OK;
}
AnalysisAssetStatus analysis_asset_encode(uint64_t sheet,uint64_t owner,const Analysis *a,uint64_t owners,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return ANALYSIS_ASSET_INVALID;
    AnalysisAssetMeasure m={0}; AnalysisAssetStatus status=analysis_asset_measure(sheet,owner,a,owners,&m);
    if (status!=ANALYSIS_ASSET_OK) return status;
    if (m.bytes>capacity || m.validation_scratch>reserved) return ANALYSIS_ASSET_LIMIT;
    if (!analysis_filters_valid(a,NULL)) return ANALYSIS_ASSET_INVALID;
    DefinitionWireWriter w={arena}; memcpy(w.p,"DXAN",4); w.p+=4; definition_wire_put(&w,DX_ANALYSIS_ASSET_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,sheet,8); definition_wire_put(&w,owner,8); definition_wire_put(&w,m.bytes-DX_ANALYSIS_ASSET_HEADER_BYTES,4); definition_wire_put(&w,0,4);
    definition_wire_put(&w,a->kind,1); definition_wire_put(&w,(a->unique_rows ? 1u : 0u)|(a->descending ? 2u : 0u),1); definition_wire_put(&w,a->filter_op,1); definition_wire_put(&w,a->aggregate,1);
    definition_wire_put(&w,a->source_version,8); definition_wire_put(&w,a->filter_count,1); definition_wire_put(&w,a->sort_count,1); definition_wire_put(&w,0,2);
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<9;i++) definition_wire_text_write(&w,texts[i]);
    for (size_t i=0;i<a->filter_count;i++) { definition_wire_put(&w,a->filters[i].op,1); definition_wire_put(&w,0,3); definition_wire_text_write(&w,a->filters[i].column); definition_wire_text_write(&w,a->filters[i].value); }
    for (size_t i=0;i<a->sort_count;i++) { definition_wire_put(&w,a->sorts[i].descending ? 1 : 0,1); definition_wire_put(&w,0,3); definition_wire_text_write(&w,a->sorts[i].column); }
    *written=m.bytes; return ANALYSIS_ASSET_OK;
}
static bool body(DefinitionWireReader *r,Analysis *out) {
    unsigned kind=(unsigned)definition_wire_get(r,1),flags=(unsigned)definition_wire_get(r,1),op=(unsigned)definition_wire_get(r,1),aggregate=(unsigned)definition_wire_get(r,1);
    uint64_t version=definition_wire_get(r,8); size_t filters=(size_t)definition_wire_get(r,1),sorts=(size_t)definition_wire_get(r,1); unsigned reserved=(unsigned)definition_wire_get(r,2);
    if (!r->ok || kind>SOURCE_DEMO || flags>3 || op>FILTER_OP_MAX || aggregate>AGG_MAX || reserved || filters>=DX_ANALYSIS_FILTERS || sorts>=DX_ANALYSIS_SORTS) return false;
    if (out) *out=(Analysis){.kind=(SourceKind)kind,.unique_rows=(flags&1)!=0,.descending=(flags&2)!=0,.filter_op=(FilterOp)op,.aggregate=(Aggregate)aggregate,
        .source_version=version,.filter_count=filters,.sort_count=sorts};
    char **slots[9]={0}; if (out) {
        char **values[]={&out->path,&out->schema,&out->table,&out->query,&out->filter_column,&out->filter_value,&out->group_column,&out->measure_column,&out->sort_column};
        memcpy(slots,values,sizeof(slots));
    }
    bool primary=false;
    for (size_t i=0;i<9;i++) {
        size_t n=0; bool present=definition_wire_text_read(r,slots[i],&n);
        if (i==4) primary=present;
        if (i==5 && primary) r->scratch=definition_wire_filter_scratch((FilterOp)op,n,present);
    }
    for (size_t i=0;i<filters && r->ok;i++) {
        op=(unsigned)definition_wire_get(r,1); reserved=(unsigned)definition_wire_get(r,3);
        if (!r->ok || op>FILTER_OP_MAX || reserved) return false;
        if (out) out->filters[i].op=(FilterOp)op;
        size_t n=0; definition_wire_text_read(r,out ? &out->filters[i].column : NULL,&n);
        bool present=definition_wire_text_read(r,out ? &out->filters[i].value : NULL,&n);
        size_t next=definition_wire_filter_scratch((FilterOp)op,n,present); if (next>r->scratch) r->scratch=next;
    }
    for (size_t i=0;i<sorts && r->ok;i++) {
        flags=(unsigned)definition_wire_get(r,1); reserved=(unsigned)definition_wire_get(r,3);
        if (!r->ok || flags>1 || reserved) return false;
        if (out) out->sorts[i].descending=flags!=0;
        size_t n=0; definition_wire_text_read(r,out ? &out->sorts[i].column : NULL,&n);
    }
    return r->ok && r->at==r->bytes;
}
void analysis_asset_free(AnalysisAsset *a) { if (a) { analysis_free(&a->definition); *a=(AnalysisAsset){0}; } }
AnalysisAssetStatus analysis_asset_preflight(const void *data,size_t bytes,uint64_t owners,DefinitionAssetInfo *out) {
    if (!data || !out || bytes<DX_ANALYSIS_ASSET_HEADER_BYTES || bytes>DX_ANALYSIS_ASSET_MAX_BYTES || memcmp(data,"DXAN",4)) return ANALYSIS_ASSET_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.owned=sizeof(Analysis),.owned_limit=DX_ANALYSIS_ASSET_OWNED_BYTES,.ok=true};
    unsigned encoding=(unsigned)definition_wire_get(&r,2); if (encoding!=DX_ANALYSIS_ASSET_ENCODING) return ANALYSIS_ASSET_UNSUPPORTED;
    unsigned flags=(unsigned)definition_wire_get(&r,2); uint64_t sheet=definition_wire_get(&r,8),owner=definition_wire_get(&r,8); size_t n=(size_t)definition_wire_get(&r,4); unsigned zero=(unsigned)definition_wire_get(&r,4);
    if (flags || zero || !sheet || owner>owners || n!=bytes-DX_ANALYSIS_ASSET_HEADER_BYTES) return ANALYSIS_ASSET_INVALID;
    if (!body(&r,NULL)) return ANALYSIS_ASSET_INVALID;
    *out=(DefinitionAssetInfo){sheet,owner,{bytes,r.owned,r.scratch}}; return ANALYSIS_ASSET_OK;
}
AnalysisAssetStatus analysis_asset_decode(const void *data,size_t bytes,uint64_t owners,size_t reserved,AnalysisAsset *out) {
    if (!out) return ANALYSIS_ASSET_INVALID;
    DefinitionAssetInfo info={0}; AnalysisAssetStatus status=analysis_asset_preflight(data,bytes,owners,&info);
    if (status!=ANALYSIS_ASSET_OK) return status;
    if (info.measure.owned>reserved || info.measure.validation_scratch>reserved-info.measure.owned) return ANALYSIS_ASSET_LIMIT;
    AnalysisAsset next={.sheet=info.sheet,.owner=info.owner};
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=DX_ANALYSIS_ASSET_HEADER_BYTES,.owned=sizeof(Analysis),.owned_limit=DX_ANALYSIS_ASSET_OWNED_BYTES,.ok=true};
    AnalysisAssetMeasure m={0};
    bool ok=body(&r,&next.definition) && analysis_asset_measure(info.sheet,info.owner,&next.definition,owners,&m)==ANALYSIS_ASSET_OK &&
        m.bytes==bytes && m.owned==r.owned && m.validation_scratch==r.scratch && analysis_filters_valid(&next.definition,NULL);
    if (!ok) { analysis_asset_free(&next); return ANALYSIS_ASSET_INVALID; }
    analysis_asset_free(out); *out=next; return ANALYSIS_ASSET_OK;
}
