#include "marks_asset.h"
#include <string.h>
typedef char MarksTagsStable[(MARKS_COLOR==0 && MARKS_SIZE==1 && MARKS_LABEL==2 && MARKS_DETAIL==3 && MARKS_TOOLTIP==4 && MARKS_SHAPE==5 && DX_MARK_ROLES==6 && DX_MARK_FIELDS==8) ? 1 : -1];
static size_t role_limit(unsigned role) { return role==MARKS_SIZE || role==MARKS_SHAPE ? 1 : DX_MARK_FIELDS; }
MarksAssetStatus marks_asset_measure(uint64_t sheet,uint64_t owner,const MarkDefinition *d,uint64_t owners,MarksAssetMeasure *out) {
    if (!sheet || !owner || owner>owners || !d || !out) return MARKS_ASSET_INVALID;
    MarksAssetMeasure m={.bytes=DX_MARKS_ASSET_HEADER_BYTES+DX_MARKS_ASSET_BODY_BYTES,.owned=sizeof(MarkDefinition)};
    for (unsigned role=0;role<DX_MARK_ROLES;role++) {
        if (d->counts[role]>role_limit(role)) return MARKS_ASSET_INVALID;
        for (size_t i=0;i<DX_MARK_FIELDS;i++) {
            const MarkField *f=&d->fields[role][i];
            if (i>=d->counts[role]) { if (f->name || f->aggregate || f->grain || f->measure) return MARKS_ASSET_INVALID; continue; }
            m.bytes+=4; size_t n=0;
            if (!definition_wire_text_measure(f->name,&m,DX_MARKS_ASSET_OWNED_BYTES,&n) || !n || n>4096) return MARKS_ASSET_INVALID;
        }
    }
    if (!marks_valid(d,NULL)) return MARKS_ASSET_INVALID;
    *out=m; return MARKS_ASSET_OK;
}
MarksAssetStatus marks_asset_encode(uint64_t sheet,uint64_t owner,const MarkDefinition *d,uint64_t owners,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return MARKS_ASSET_INVALID;
    MarksAssetMeasure m={0}; MarksAssetStatus status=marks_asset_measure(sheet,owner,d,owners,&m);
    if (status!=MARKS_ASSET_OK) return status;
    if (m.bytes>capacity) return MARKS_ASSET_LIMIT;
    DefinitionWireWriter w={arena}; memcpy(w.p,"DXMK",4); w.p+=4;
    definition_wire_put(&w,DX_MARKS_ASSET_ENCODING,2); definition_wire_put(&w,0,2); definition_wire_put(&w,sheet,8); definition_wire_put(&w,owner,8);
    definition_wire_put(&w,m.bytes-DX_MARKS_ASSET_HEADER_BYTES,4); definition_wire_put(&w,0,4);
    for (unsigned role=0;role<DX_MARK_ROLES;role++) definition_wire_put(&w,d->counts[role],1);
    definition_wire_put(&w,0,2);
    for (unsigned role=0;role<DX_MARK_ROLES;role++) for (size_t i=0;i<d->counts[role];i++) {
        const MarkField *f=&d->fields[role][i];
        definition_wire_put(&w,f->aggregate,1); definition_wire_put(&w,f->grain,1); definition_wire_put(&w,f->measure ? 1 : 0,1); definition_wire_put(&w,0,1);
        definition_wire_text_write(&w,f->name);
    }
    *written=m.bytes; return MARKS_ASSET_OK;
}
static bool body(DefinitionWireReader *r,MarkDefinition *out) {
    size_t counts[DX_MARK_ROLES];
    for (unsigned role=0;role<DX_MARK_ROLES;role++) { counts[role]=(size_t)definition_wire_get(r,1); if (counts[role]>role_limit(role)) return false; }
    if (definition_wire_get(r,2) || !r->ok) return false;
    if (out) for (unsigned role=0;role<DX_MARK_ROLES;role++) out->counts[role]=counts[role];
    for (unsigned role=0;role<DX_MARK_ROLES;role++) for (size_t i=0;i<counts[role];i++) {
        unsigned aggregate=(unsigned)definition_wire_get(r,1),grain=(unsigned)definition_wire_get(r,1),measure=(unsigned)definition_wire_get(r,1),zero=(unsigned)definition_wire_get(r,1);
        if (!r->ok || aggregate>AGG_MAX || grain>PIVOT_DATE_MONTH || measure>1 || (measure && grain) || zero) return false;
        MarkField *f=out ? &out->fields[role][i] : NULL;
        if (f) { f->aggregate=(Aggregate)aggregate; f->grain=(PivotDateGrain)grain; f->measure=measure!=0; }
        size_t n=0;
        if (!definition_wire_text_read(r,f ? &f->name : NULL,&n) || !n || n>4096) return false;
    }
    return r->ok && r->at==r->bytes;
}
void marks_asset_free(MarksAsset *a) { if (a) { marks_free(&a->definition); *a=(MarksAsset){0}; } }
MarksAssetStatus marks_asset_decode(const void *data,size_t bytes,uint64_t owners,size_t reserved,MarksAsset *out) {
    if (!data || !out || bytes<DX_MARKS_ASSET_HEADER_BYTES || bytes>DX_MARKS_ASSET_MAX_BYTES || memcmp(data,"DXMK",4)) return MARKS_ASSET_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.owned=sizeof(MarkDefinition),.owned_limit=DX_MARKS_ASSET_OWNED_BYTES,.ok=true};
    unsigned encoding=(unsigned)definition_wire_get(&r,2); if (encoding!=DX_MARKS_ASSET_ENCODING) return MARKS_ASSET_UNSUPPORTED;
    unsigned flags=(unsigned)definition_wire_get(&r,2); uint64_t sheet=definition_wire_get(&r,8),owner=definition_wire_get(&r,8);
    size_t n=(size_t)definition_wire_get(&r,4); unsigned zero=(unsigned)definition_wire_get(&r,4);
    if (flags || zero || !sheet || !owner || owner>owners || n!=bytes-DX_MARKS_ASSET_HEADER_BYTES) return MARKS_ASSET_INVALID;
    DefinitionWireReader start=r; if (!body(&r,NULL)) return MARKS_ASSET_INVALID;
    if (r.owned>reserved) return MARKS_ASSET_LIMIT;
    MarksAsset next={.sheet=sheet,.owner=owner}; r=start; MarksAssetMeasure m={0};
    bool ok=body(&r,&next.definition) && marks_asset_measure(sheet,owner,&next.definition,owners,&m)==MARKS_ASSET_OK && m.bytes==bytes && m.owned==r.owned;
    if (!ok) { marks_asset_free(&next); return MARKS_ASSET_INVALID; }
    marks_asset_free(out); *out=next; return MARKS_ASSET_OK;
}
