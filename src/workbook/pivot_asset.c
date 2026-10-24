#include "pivot_asset.h"
#include <string.h>
typedef char PivotTagsStable[(PIVOT_ROWS==0 && PIVOT_COLUMNS==1 && PIVOT_VALUES==2 && PIVOT_FILTERS==3 &&
    PIVOT_DATE_EXACT==0 && PIVOT_DATE_YEAR==1 && PIVOT_DATE_QUARTER==2 && PIVOT_DATE_MONTH==3 && DX_PIVOT_FIELDS==8) ? 1 : -1];
PivotAssetStatus pivot_asset_measure(uint64_t sheet,uint64_t owner,const PivotDefinition *p,uint64_t owners,PivotAssetMeasure *out) {
    if (!sheet || !owner || owner>owners || !p || !out) return PIVOT_ASSET_INVALID;
    PivotAssetMeasure m={.bytes=DX_PIVOT_ASSET_HEADER_BYTES+DX_PIVOT_ASSET_BODY_BYTES,.owned=sizeof(PivotDefinition)};
    for (unsigned role=0;role<4;role++) {
        if (p->counts[role]>DX_PIVOT_FIELDS) return PIVOT_ASSET_INVALID;
        for (size_t i=0;i<DX_PIVOT_FIELDS;i++) {
            const PivotField *f=&p->fields[role][i];
            if (i>=p->counts[role]) { if (f->name || f->value || f->aggregate || f->filter || f->grain) return PIVOT_ASSET_INVALID; continue; }
            if ((unsigned)f->aggregate>AGG_MAX || (unsigned)f->filter>FILTER_OP_MAX || (unsigned)f->grain>PIVOT_DATE_MONTH || (role>=PIVOT_VALUES && f->grain!=PIVOT_DATE_EXACT)) return PIVOT_ASSET_INVALID;
            m.bytes+=4; size_t n=0;
            if (!definition_wire_text_measure(f->name,&m,DX_DEFINITION_OWNED_BYTES,&n) || !n || n>4096 ||
                !definition_wire_text_measure(f->value,&m,DX_DEFINITION_OWNED_BYTES,&n)) return PIVOT_ASSET_INVALID;
            size_t scratch=definition_wire_filter_scratch(f->filter,n,f->value!=NULL); if (scratch>m.validation_scratch) m.validation_scratch=scratch;
        }
    }
    *out=m; return PIVOT_ASSET_OK;
}
PivotAssetStatus pivot_asset_encode(uint64_t sheet,uint64_t owner,const PivotDefinition *p,uint64_t owners,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return PIVOT_ASSET_INVALID;
    PivotAssetMeasure m={0}; PivotAssetStatus status=pivot_asset_measure(sheet,owner,p,owners,&m);
    if (status!=PIVOT_ASSET_OK) return status;
    if (m.bytes>capacity || m.validation_scratch>reserved) return PIVOT_ASSET_LIMIT;
    if (!pivot_valid(p,NULL)) return PIVOT_ASSET_INVALID;
    DefinitionWireWriter w={arena}; memcpy(w.p,"DXPV",4); w.p+=4;
    definition_wire_put(&w,DX_PIVOT_ASSET_ENCODING,2); definition_wire_put(&w,0,2); definition_wire_put(&w,sheet,8); definition_wire_put(&w,owner,8);
    definition_wire_put(&w,m.bytes-DX_PIVOT_ASSET_HEADER_BYTES,4); definition_wire_put(&w,0,4);
    for (unsigned role=0;role<4;role++) definition_wire_put(&w,p->counts[role],1);
    definition_wire_put(&w,0,4);
    for (unsigned role=0;role<4;role++) for (size_t i=0;i<p->counts[role];i++) {
        const PivotField *f=&p->fields[role][i];
        definition_wire_put(&w,f->aggregate,1); definition_wire_put(&w,f->filter,1); definition_wire_put(&w,f->grain,1); definition_wire_put(&w,0,1);
        definition_wire_text_write(&w,f->name); definition_wire_text_write(&w,f->value);
    }
    *written=m.bytes; return PIVOT_ASSET_OK;
}
static bool body(DefinitionWireReader *r,PivotDefinition *out) {
    size_t counts[4]; for (unsigned role=0;role<4;role++) { counts[role]=(size_t)definition_wire_get(r,1); if (counts[role]>DX_PIVOT_FIELDS) return false; }
    if (definition_wire_get(r,4) || !r->ok) return false;
    if (out) for (unsigned role=0;role<4;role++) out->counts[role]=counts[role];
    for (unsigned role=0;role<4;role++) for (size_t i=0;i<counts[role];i++) {
        unsigned aggregate=(unsigned)definition_wire_get(r,1),filter=(unsigned)definition_wire_get(r,1),grain=(unsigned)definition_wire_get(r,1),zero=(unsigned)definition_wire_get(r,1);
        if (!r->ok || aggregate>AGG_MAX || filter>FILTER_OP_MAX || grain>PIVOT_DATE_MONTH || (role>=PIVOT_VALUES && grain!=PIVOT_DATE_EXACT) || zero) return false;
        PivotField *f=out ? &out->fields[role][i] : NULL;
        if (f) { f->aggregate=(Aggregate)aggregate; f->filter=(FilterOp)filter; f->grain=(PivotDateGrain)grain; }
        size_t n=0;
        if (!definition_wire_text_read(r,f ? &f->name : NULL,&n) || !n || n>4096) return false;
        bool present=definition_wire_text_read(r,f ? &f->value : NULL,&n);
        size_t scratch=definition_wire_filter_scratch((FilterOp)filter,n,present); if (scratch>r->scratch) r->scratch=scratch;
    }
    return r->ok && r->at==r->bytes;
}
void pivot_asset_free(PivotAsset *a) { if (a) { pivot_free(&a->definition); *a=(PivotAsset){0}; } }
PivotAssetStatus pivot_asset_preflight(const void *data,size_t bytes,uint64_t owners,DefinitionAssetInfo *out) {
    if (!data || !out || bytes<DX_PIVOT_ASSET_HEADER_BYTES || bytes>DX_PIVOT_ASSET_MAX_BYTES || memcmp(data,"DXPV",4)) return PIVOT_ASSET_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.owned=sizeof(PivotDefinition),.owned_limit=DX_DEFINITION_OWNED_BYTES,.ok=true};
    unsigned encoding=(unsigned)definition_wire_get(&r,2); if (encoding!=DX_PIVOT_ASSET_ENCODING) return PIVOT_ASSET_UNSUPPORTED;
    unsigned flags=(unsigned)definition_wire_get(&r,2); uint64_t sheet=definition_wire_get(&r,8),owner=definition_wire_get(&r,8);
    size_t n=(size_t)definition_wire_get(&r,4); unsigned zero=(unsigned)definition_wire_get(&r,4);
    if (flags || zero || !sheet || !owner || owner>owners || n!=bytes-DX_PIVOT_ASSET_HEADER_BYTES) return PIVOT_ASSET_INVALID;
    if (!body(&r,NULL)) return PIVOT_ASSET_INVALID;
    *out=(DefinitionAssetInfo){sheet,owner,{bytes,r.owned,r.scratch}}; return PIVOT_ASSET_OK;
}
PivotAssetStatus pivot_asset_decode(const void *data,size_t bytes,uint64_t owners,size_t reserved,PivotAsset *out) {
    if (!out) return PIVOT_ASSET_INVALID;
    DefinitionAssetInfo info={0}; PivotAssetStatus status=pivot_asset_preflight(data,bytes,owners,&info);
    if (status!=PIVOT_ASSET_OK) return status;
    if (info.measure.owned>reserved || info.measure.validation_scratch>reserved-info.measure.owned) return PIVOT_ASSET_LIMIT;
    PivotAsset next={.sheet=info.sheet,.owner=info.owner};
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=DX_PIVOT_ASSET_HEADER_BYTES,.owned=sizeof(PivotDefinition),.owned_limit=DX_DEFINITION_OWNED_BYTES,.ok=true};
    PivotAssetMeasure m={0};
    bool ok=body(&r,&next.definition) && pivot_asset_measure(info.sheet,info.owner,&next.definition,owners,&m)==PIVOT_ASSET_OK &&
        m.bytes==bytes && m.owned==r.owned && m.validation_scratch==r.scratch && pivot_valid(&next.definition,NULL);
    if (!ok) { pivot_asset_free(&next); return PIVOT_ASSET_INVALID; }
    pivot_asset_free(out); *out=next; return PIVOT_ASSET_OK;
}
