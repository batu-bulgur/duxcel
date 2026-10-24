#include "objects_asset_internal.h"
#include <stdlib.h>
#include <string.h>
typedef char ObjectTagsStable[(OBJECT_PIVOT==0 && OBJECT_CHART==1 && OBJECT_TABLE==2 && MARK_BAR==0 && MARK_LINE==1 && MARK_DOT==2 && MARK_SCATTER==3 && DX_MAX_OBJECTS==32) ? 1 : -1];
typedef struct {
    uint64_t id,row,rows,source;
    size_t column,columns;
    unsigned kind,mark,measure;
    const unsigned char *name,*nested[3];
    size_t name_bytes,lengths[3];
    DefinitionWireMeasure definitions[3];
} ObjectWireRecord;
typedef struct { ObjectsAssetInfo info; ObjectWireRecord rows[DX_MAX_OBJECTS]; } ObjectPlan;
static const size_t inline_bytes[3]={sizeof(Analysis),sizeof(PivotDefinition),sizeof(MarkDefinition)};
static bool geometry(const ObjectWireRecord *o) {
    return o->id && o->row<DX_SHEET_ROWS && o->rows && o->rows<=DX_SHEET_ROWS-o->row && o->column<16384 && o->columns &&
        o->columns<=16384-o->column && o->kind<=OBJECT_TABLE && o->mark<=MARK_SCATTER && (o->kind==OBJECT_CHART || o->mark==MARK_BAR);
}
static bool charge(DefinitionWireMeasure *m,const DefinitionWireMeasure *nested,unsigned family) {
    if (m->owned>DX_OBJECT_CURRENT_BYTES || nested->owned<inline_bytes[family] || nested->owned-inline_bytes[family]>DX_OBJECT_CURRENT_BYTES-m->owned) return false;
    m->owned+=nested->owned-inline_bytes[family]; m->bytes+=nested->bytes;
    if (nested->validation_scratch>m->validation_scratch) m->validation_scratch=nested->validation_scratch;
    return true;
}
static bool links(const ObjectPlan *p) {
    for (size_t i=0;i<p->info.count;i++) if (p->rows[i].source) {
        if (p->rows[i].kind==OBJECT_TABLE) return false;
        bool found=false;
        for (size_t j=0;j<p->info.count;j++) found|=p->rows[j].id==p->rows[i].source && p->rows[j].kind==OBJECT_TABLE;
        if (!found) return false;
    }
    return true;
}
static ObjectsAssetStatus model_plan(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high,ObjectPlan *out) {
    if (!sheet || !objects || objects->count>DX_MAX_OBJECTS || (objects->count && !objects->items)) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={.info={.sheet=sheet,.high_water=high,.count=objects->count,.measure={.bytes=DX_OBJECTS_ASSET_HEADER_BYTES,.owned=objects->count*sizeof(SheetObject)}}};
    DefinitionWireMeasure *m=&p.info.measure;
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *o=objects->items+i; ObjectWireRecord *r=p.rows+i;
        *r=(ObjectWireRecord){.id=o->id,.row=o->row,.rows=o->rows,.source=o->source_id,.column=o->col,.columns=o->columns,.kind=o->kind,.mark=o->mark,.measure=o->chart_measure,.name=(const unsigned char *)o->name};
        if (!geometry(r) || r->id>high || r->source>high) return OBJECTS_ASSET_INVALID;
        for (size_t j=0;j<i;j++) if (p.rows[j].id==r->id) return OBJECTS_ASSET_INVALID;
        m->bytes+=DX_OBJECT_ASSET_HEADER_BYTES-4;
        if (!definition_wire_text_measure(o->name,m,DX_OBJECT_CURRENT_BYTES,&r->name_bytes) || !r->name_bytes || r->name_bytes>4096) return OBJECTS_ASSET_INVALID;
        if (analysis_asset_measure(sheet,o->id,&o->source,high,r->definitions)!=ANALYSIS_ASSET_OK ||
            pivot_asset_measure(sheet,o->id,&o->pivot,high,r->definitions+1)!=PIVOT_ASSET_OK ||
            marks_asset_measure(sheet,o->id,&o->marks,high,r->definitions+2)!=MARKS_ASSET_OK) return OBJECTS_ASSET_INVALID;
        for (unsigned f=0;f<3;f++) {
            r->lengths[f]=r->definitions[f].bytes;
            if (!charge(m,r->definitions+f,f)) return OBJECTS_ASSET_INVALID;
        }
    }
    if (!links(&p) || m->bytes>DX_OBJECTS_ASSET_MAX_BYTES) return OBJECTS_ASSET_INVALID;
    *out=p; return OBJECTS_ASSET_OK;
}
ObjectsAssetStatus objects_asset_measure(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high,ObjectsAssetInfo *out) {
    if (!out) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={0}; ObjectsAssetStatus status=model_plan(sheet,objects,high,&p);
    if (status==OBJECTS_ASSET_OK) *out=p.info; return status;
}
void objects_asset_packet_free(ObjectsAssetPacket *p) { if (p) { free(p->data); *p=(ObjectsAssetPacket){0}; } }
static ObjectsAssetStatus encode_prepared(const ObjectPlan *p,const ObjectSnapshot *objects,uint64_t high,void *arena,size_t *written) {
    const DefinitionWireMeasure *m=&p->info.measure; DefinitionWireWriter w={arena};
    memcpy(w.p,"DXOB",4); w.p+=4; definition_wire_put(&w,DX_OBJECTS_ASSET_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,p->info.sheet,8); definition_wire_put(&w,high,8); definition_wire_put(&w,objects->count,1); definition_wire_put(&w,0,3);
    definition_wire_put(&w,m->bytes-DX_OBJECTS_ASSET_HEADER_BYTES,4);
    for (size_t i=0;i<objects->count;i++) {
        const SheetObject *o=objects->items+i; const ObjectWireRecord *r=p->rows+i;
        definition_wire_put(&w,r->id,8); definition_wire_put(&w,r->row,8); definition_wire_put(&w,r->rows,8); definition_wire_put(&w,r->source,8);
        definition_wire_put(&w,r->column,2); definition_wire_put(&w,r->columns,2); definition_wire_put(&w,r->kind,1); definition_wire_put(&w,r->mark,1); definition_wire_put(&w,r->measure,1); definition_wire_put(&w,0,1);
        for (unsigned f=0;f<3;f++) definition_wire_put(&w,r->lengths[f],4);
        definition_wire_text_write(&w,o->name); size_t written=0;
        bool ok=analysis_asset_encode(p->info.sheet,o->id,&o->source,high,m->validation_scratch,w.p,r->lengths[0],&written)==ANALYSIS_ASSET_OK && written==r->lengths[0]; w.p+=r->lengths[0]; written=0;
        ok=ok && pivot_asset_encode(p->info.sheet,o->id,&o->pivot,high,m->validation_scratch,w.p,r->lengths[1],&written)==PIVOT_ASSET_OK && written==r->lengths[1]; w.p+=r->lengths[1]; written=0;
        ok=ok && marks_asset_encode(p->info.sheet,o->id,&o->marks,high,w.p,r->lengths[2],&written)==MARKS_ASSET_OK && written==r->lengths[2]; w.p+=r->lengths[2];
        if (!ok) { return OBJECTS_ASSET_INVALID; }
    }
    if ((size_t)(w.p-(unsigned char *)arena)!=m->bytes) { return OBJECTS_ASSET_INVALID; }
    *written=m->bytes; return OBJECTS_ASSET_OK;
}
ObjectsAssetStatus objects_asset_encode_unpublished(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={0}; ObjectsAssetStatus status=model_plan(sheet,objects,high,&p); if (status!=OBJECTS_ASSET_OK) return status;
    if (p.info.measure.bytes>capacity || p.info.measure.validation_scratch>reserved) return OBJECTS_ASSET_LIMIT;
    ObjectStore view={.items=objects->items,.count=objects->count,.next_id=high};
    if (!objects_valid(&view,NULL)) return OBJECTS_ASSET_INVALID;
    return encode_prepared(&p,objects,high,arena,written);
}
ObjectsAssetStatus objects_asset_capture(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high,size_t reserved,ObjectsAssetPacket *out) {
    if (!out) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={0}; ObjectsAssetStatus status=model_plan(sheet,objects,high,&p); if (status!=OBJECTS_ASSET_OK) return status;
    const DefinitionWireMeasure *m=&p.info.measure;
    if (m->bytes>reserved || m->validation_scratch>reserved-m->bytes) return OBJECTS_ASSET_LIMIT;
    ObjectStore view={.items=objects->items,.count=objects->count,.next_id=high};
    if (!objects_valid(&view,NULL)) return OBJECTS_ASSET_INVALID;
    ObjectsAssetPacket next={dx_alloc(m->bytes),m->bytes}; size_t written=0;
    status=encode_prepared(&p,objects,high,next.data,&written);
    if (status!=OBJECTS_ASSET_OK || written!=next.bytes) { objects_asset_packet_free(&next); return OBJECTS_ASSET_INVALID; }
    objects_asset_packet_free(out); *out=next; return OBJECTS_ASSET_OK;
}
static const unsigned char *span(DefinitionWireReader *r,size_t n) {
    if (!r->ok || n>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *p=r->data+r->at; r->at+=n; return p;
}
static ObjectsAssetStatus wire_plan(const void *data,size_t bytes,ObjectPlan *out) {
    if (!data || bytes<DX_OBJECTS_ASSET_HEADER_BYTES || bytes>DX_OBJECTS_ASSET_MAX_BYTES || memcmp(data,"DXOB",4)) return OBJECTS_ASSET_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.owned_limit=DX_OBJECT_CURRENT_BYTES,.ok=true};
    unsigned encoding=(unsigned)definition_wire_get(&r,2); if (encoding!=DX_OBJECTS_ASSET_ENCODING) return OBJECTS_ASSET_UNSUPPORTED;
    unsigned flags=(unsigned)definition_wire_get(&r,2); uint64_t sheet=definition_wire_get(&r,8),high=definition_wire_get(&r,8);
    size_t count=(size_t)definition_wire_get(&r,1); unsigned zero=(unsigned)definition_wire_get(&r,3); size_t n=(size_t)definition_wire_get(&r,4);
    if (flags || zero || !sheet || count>DX_MAX_OBJECTS || n!=bytes-DX_OBJECTS_ASSET_HEADER_BYTES) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={.info={.sheet=sheet,.high_water=high,.count=count,.measure={.bytes=bytes,.owned=count*sizeof(SheetObject)}}}; r.owned=p.info.measure.owned;
    for (size_t i=0;i<count;i++) {
        ObjectWireRecord *o=p.rows+i;
        o->id=definition_wire_get(&r,8); o->row=definition_wire_get(&r,8); o->rows=definition_wire_get(&r,8); o->source=definition_wire_get(&r,8);
        o->column=(size_t)definition_wire_get(&r,2); o->columns=(size_t)definition_wire_get(&r,2); o->kind=(unsigned)definition_wire_get(&r,1); o->mark=(unsigned)definition_wire_get(&r,1); o->measure=(unsigned)definition_wire_get(&r,1); zero=(unsigned)definition_wire_get(&r,1);
        for (unsigned f=0;f<3;f++) o->lengths[f]=(size_t)definition_wire_get(&r,4);
        if (!r.ok || zero || !geometry(o) || o->id>high || o->source>high) return OBJECTS_ASSET_INVALID;
        for (size_t j=0;j<i;j++) if (p.rows[j].id==o->id) return OBJECTS_ASSET_INVALID;
        size_t name_at=r.at; if (!definition_wire_text_read(&r,NULL,&o->name_bytes) || !o->name_bytes || o->name_bytes>4096) return OBJECTS_ASSET_INVALID;
        o->name=r.data+name_at+4;
        for (unsigned f=0;f<3;f++) {
            o->nested[f]=span(&r,o->lengths[f]); if (!r.ok) return OBJECTS_ASSET_INVALID;
            DefinitionAssetInfo info={0}; bool ok=false,unsupported=false;
            if (f==0) { AnalysisAssetStatus s=analysis_asset_preflight(o->nested[f],o->lengths[f],high,&info); ok=s==ANALYSIS_ASSET_OK; unsupported=s==ANALYSIS_ASSET_UNSUPPORTED; }
            else if (f==1) { PivotAssetStatus s=pivot_asset_preflight(o->nested[f],o->lengths[f],high,&info); ok=s==PIVOT_ASSET_OK; unsupported=s==PIVOT_ASSET_UNSUPPORTED; }
            else { MarksAssetStatus s=marks_asset_preflight(o->nested[f],o->lengths[f],high,&info); ok=s==MARKS_ASSET_OK; unsupported=s==MARKS_ASSET_UNSUPPORTED; }
            if (unsupported) return OBJECTS_ASSET_UNSUPPORTED;
            if (!ok || info.sheet!=sheet || info.owner!=o->id) return OBJECTS_ASSET_INVALID;
            o->definitions[f]=info.measure;
            DefinitionWireMeasure nested=info.measure; nested.bytes=0;
            DefinitionWireMeasure owned={.owned=r.owned,.validation_scratch=r.scratch};
            if (!charge(&owned,&nested,f)) return OBJECTS_ASSET_INVALID;
            r.owned=owned.owned; r.scratch=owned.validation_scratch;
        }
    }
    if (!r.ok || r.at!=bytes || !links(&p)) return OBJECTS_ASSET_INVALID;
    p.info.measure.owned=r.owned; p.info.measure.validation_scratch=r.scratch; *out=p; return OBJECTS_ASSET_OK;
}
ObjectsAssetStatus objects_asset_preflight(const void *data,size_t bytes,ObjectsAssetInfo *out) {
    if (!out) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={0}; ObjectsAssetStatus status=wire_plan(data,bytes,&p); if (status==OBJECTS_ASSET_OK) *out=p.info; return status;
}
void objects_asset_free(ObjectsAsset *out) { if (out) { objects_snapshot_free(&out->objects); *out=(ObjectsAsset){0}; } }
ObjectsAssetStatus objects_asset_decode(const void *data,size_t bytes,size_t reserved,ObjectsAsset *out) {
    if (!out) return OBJECTS_ASSET_INVALID;
    ObjectPlan p={0}; ObjectsAssetStatus status=wire_plan(data,bytes,&p); if (status!=OBJECTS_ASSET_OK) return status;
    const DefinitionWireMeasure *m=&p.info.measure;
    if (m->owned>reserved || m->validation_scratch>reserved-m->owned) return OBJECTS_ASSET_LIMIT;
    ObjectsAsset next={.sheet=p.info.sheet,.high_water=p.info.high_water,.objects={.count=p.info.count}};
    if (next.objects.count) next.objects.items=dx_alloc(next.objects.count*sizeof(SheetObject));
    for (size_t i=0;i<next.objects.count;i++) {
        const ObjectWireRecord *r=p.rows+i; SheetObject *o=next.objects.items+i;
        *o=(SheetObject){.id=r->id,.row=r->row,.rows=r->rows,.col=r->column,.columns=r->columns,.source_id=r->source,.kind=(ObjectKind)r->kind,.mark=(ChartMark)r->mark,.chart_measure=(unsigned char)r->measure};
        o->name=dx_alloc(r->name_bytes+1); memcpy(o->name,r->name,r->name_bytes); o->name[r->name_bytes]=0;
        AnalysisAsset a={0}; PivotAsset v={0}; MarksAsset k={0};
        bool ok=analysis_asset_decode(r->nested[0],r->lengths[0],next.high_water,r->definitions[0].owned+m->validation_scratch,&a)==ANALYSIS_ASSET_OK &&
            pivot_asset_decode(r->nested[1],r->lengths[1],next.high_water,r->definitions[1].owned+m->validation_scratch,&v)==PIVOT_ASSET_OK &&
            marks_asset_decode(r->nested[2],r->lengths[2],next.high_water,r->definitions[2].owned,&k)==MARKS_ASSET_OK;
        if (ok) { o->source=a.definition; o->pivot=v.definition; o->marks=k.definition; }
        else { analysis_asset_free(&a); pivot_asset_free(&v); marks_asset_free(&k); objects_asset_free(&next); return OBJECTS_ASSET_INVALID; }
    }
    ObjectStore view={.items=next.objects.items,.count=next.objects.count,.next_id=next.high_water};
    if (!objects_valid(&view,NULL)) { objects_asset_free(&next); return OBJECTS_ASSET_INVALID; }
    objects_asset_free(out); *out=next; return OBJECTS_ASSET_OK;
}
