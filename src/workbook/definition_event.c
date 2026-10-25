#include "definition_event_internal.h"
#include "objects_asset_internal.h"
#include <stdlib.h>
#include <string.h>
static size_t maximum(size_t a,size_t b) { return a>b ? a : b; }
static bool effects_valid(unsigned e) { return e==1 || e==4 || e==9 || e==12; }
static bool empty_format(FormatSnapshotView f) { return !f.items && !f.count && !f.scopes && !f.scope_count && !f.source_aware; }
static FormatStore format_view(FormatSnapshotView f) { return (FormatStore){.items=(FormatPatch *)f.items,.count=f.count,.scopes=(FormatScope *)f.scopes,.scope_count=f.scope_count,.source_aware=f.source_aware}; }
static size_t format_scratch(size_t scopes) {
    if (!scopes) return 0;
    size_t proof=analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+scopes;
    return maximum(proof,scopes>DX_FORMAT_GUARDS ? scopes*sizeof(const FormatScope *) : 0);
}
static bool changed(DefinitionEventView v,bool format_changed) {
    bool a=(v.effects&1) && !definition_analysis_equal(v.before.primary,v.after.primary);
    bool o=(v.effects&4) && !definition_objects_equal(v.before.objects,v.after.objects);
    return a || o || format_changed;
}
static bool exact_effects(DefinitionEventView v,bool format_changed) {
    return (!(v.effects&1) || !definition_analysis_equal(v.before.primary,v.after.primary)) &&
        (!(v.effects&4) || !definition_objects_equal(v.before.objects,v.after.objects)) && (!(v.effects&8) || format_changed);
}
static DefinitionEventStatus select_sources(DefinitionEventView v,bool selected[DX_SOURCE_VERSIONS],size_t *inactive,size_t *unbound) {
    DefinitionDependencySelection d={0}; FormatDependencySelection f={0};
    DefinitionDependenciesStatus ds=definition_dependencies_select(v.sheet,v.before,v.after,v.sources,v.object_high_water,&d);
    if (ds!=DEFINITION_DEPENDENCIES_OK) return ds==DEFINITION_DEPENDENCIES_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
    if (v.effects&8) {
        FormatDependenciesStatus fs=format_dependencies_select(v.sheet,v.before_formats,v.after_formats,v.sources,v.object_high_water,&f);
        if (fs!=FORMAT_DEPENDENCIES_OK) return fs==FORMAT_DEPENDENCIES_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
    }
    for (size_t i=0;i<DX_SOURCE_VERSIONS;i++) selected[i]=d.rows[i] || f.rows[i];
    *inactive=f.legacy_scopes; *unbound=d.unbound_sources; return DEFINITION_EVENT_OK;
}
DefinitionEventStatus definition_event_prepare(DefinitionEventView v,size_t reserved,DefinitionEventPlan *out) {
    if (!out || !v.sheet || !effects_valid(v.effects) || !v.sources ||
        (!!v.before.primary!=!!(v.effects&1)) || (!!v.after.primary!=!!(v.effects&1)) ||
        (!!v.before.objects!=!!(v.effects&4)) || (!!v.after.objects!=!!(v.effects&4)) ||
        (!(v.effects&8) && (!empty_format(v.before_formats) || !empty_format(v.after_formats)))) return DEFINITION_EVENT_INVALID;
    DefinitionEventPlan p={.view=v,.info={.sheet=v.sheet,.object_high_water=v.object_high_water,.source_high_water=v.sources->high_water,.effects=v.effects,
        .bytes=DX_DEFINITION_EVENT_HEADER_BYTES,.owned=sizeof(DefinitionEvent)}};
    if ((v.effects&8) && (v.before_formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || v.after_formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS)) return DEFINITION_EVENT_INVALID;
    p.info.validation_scratch=(v.effects&8) ? format_scratch(maximum(v.before_formats.scope_count,v.after_formats.scope_count)) : 0;
    const Analysis *analyses[]={v.before.primary,v.after.primary}; const ObjectSnapshot *objects[]={v.before.objects,v.after.objects};
    for (unsigned i=0;i<2;i++) {
        if (v.effects&1) {
            AnalysisAssetMeasure m={0}; if (analysis_asset_measure(v.sheet,0,analyses[i],v.object_high_water,&m)!=ANALYSIS_ASSET_OK) return DEFINITION_EVENT_INVALID;
            p.lengths[i]=m.bytes; p.info.owned+=m.owned-sizeof(Analysis); p.info.validation_scratch=maximum(p.info.validation_scratch,m.validation_scratch);
        }
        if (v.effects&4) {
            ObjectsAssetInfo m={0}; if (objects_asset_measure(v.sheet,objects[i],v.object_high_water,&m)!=OBJECTS_ASSET_OK) return DEFINITION_EVENT_INVALID;
            p.lengths[2+i]=m.measure.bytes; p.info.owned+=m.measure.owned; p.info.validation_scratch=maximum(p.info.validation_scratch,m.measure.validation_scratch);
        }
    }
    if (p.info.validation_scratch>reserved) return DEFINITION_EVENT_LIMIT;
    bool format_changed=false;
    if (v.effects&8) {
        FormatDeltaStatus f=format_delta_encoded_size(v.sheet,v.before_formats,v.after_formats,p.lengths+4);
        if (f!=FORMAT_DELTA_OK && f!=FORMAT_DELTA_NOOP) return DEFINITION_EVENT_INVALID;
        format_changed=f==FORMAT_DELTA_OK;
        FormatStore a=format_view(v.before_formats),b=format_view(v.after_formats);
        p.info.owned+=format_owned_bytes(&a,false)+format_owned_bytes(&b,false);
    }
    for (unsigned i=0;i<2;i++) {
        if ((v.effects&1) && !analysis_filters_valid(analyses[i],NULL)) return DEFINITION_EVENT_INVALID;
        if (v.effects&4) {
            ObjectStore s={.items=objects[i]->items,.count=objects[i]->count,.next_id=v.object_high_water};
            if (!objects_valid(&s,NULL)) return DEFINITION_EVENT_INVALID;
        }
    }
    if (!changed(v,format_changed)) return DEFINITION_EVENT_NOOP;
    if (!exact_effects(v,format_changed)) return DEFINITION_EVENT_INVALID;
    DefinitionEventStatus status=select_sources(v,p.selected,&p.info.inactive_scopes,&p.info.unbound_sources);
    if (status!=DEFINITION_EVENT_OK) return status;
    SourceVersionsSelectionMeasure source={0};
    if (source_versions_selection_measure(v.sheet,v.sources,v.object_high_water,p.selected,v.sources->count,&source)!=SOURCE_VERSIONS_CODEC_OK) return DEFINITION_EVENT_INVALID;
    p.lengths[5]=source.bytes; p.info.owned+=source.owned;
    for (unsigned i=0;i<6;i++) p.info.bytes+=p.lengths[i];
    if (p.info.bytes>DX_DEFINITION_EVENT_MAX_BYTES) return DEFINITION_EVENT_INVALID;
    *out=p; return DEFINITION_EVENT_OK;
}
DefinitionEventStatus definition_event_measure(DefinitionEventView v,size_t reserved,DefinitionEventInfo *out) {
    if (!out) return DEFINITION_EVENT_INVALID;
    DefinitionEventPlan p={0}; DefinitionEventStatus s=definition_event_prepare(v,reserved,&p); if (s==DEFINITION_EVENT_OK) *out=p.info; return s;
}
DefinitionEventStatus definition_event_encode_unpublished(const DefinitionEventPlan *p,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!p || !arena || !written || p->info.bytes<DX_DEFINITION_EVENT_HEADER_BYTES || p->info.bytes>DX_DEFINITION_EVENT_MAX_BYTES ||
        p->info.sheet!=p->view.sheet || p->info.object_high_water!=p->view.object_high_water || p->info.effects!=p->view.effects ||
        !effects_valid(p->view.effects) || !p->view.sources || p->info.source_high_water!=p->view.sources->high_water) return DEFINITION_EVENT_INVALID;
    if (p->info.bytes>capacity || p->info.validation_scratch>reserved) return DEFINITION_EVENT_LIMIT;
    const DefinitionEventView *v=&p->view; DefinitionWireWriter w={arena};
    memcpy(w.p,"DXDE",4); w.p+=4; definition_wire_put(&w,DX_DEFINITION_EVENT_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,v->sheet,8); definition_wire_put(&w,v->object_high_water,8); definition_wire_put(&w,v->effects,4);
    for (unsigned i=0;i<6;i++) definition_wire_put(&w,p->lengths[i],4);
    definition_wire_put(&w,p->info.inactive_scopes,4); definition_wire_put(&w,p->info.unbound_sources,4); definition_wire_put(&w,0,4);
    const Analysis *a[]={v->before.primary,v->after.primary}; const ObjectSnapshot *o[]={v->before.objects,v->after.objects};
    for (unsigned i=0;i<6;i++) if (p->lengths[i]) {
        size_t n=0; bool ok=false;
        if (i<2) ok=analysis_asset_encode(v->sheet,0,a[i],v->object_high_water,reserved,w.p,p->lengths[i],&n)==ANALYSIS_ASSET_OK;
        else if (i<4) ok=objects_asset_encode_unpublished(v->sheet,o[i-2],v->object_high_water,reserved,w.p,p->lengths[i],&n)==OBJECTS_ASSET_OK;
        else if (i==4) ok=format_delta_encode(v->sheet,v->before_formats,v->after_formats,w.p,p->lengths[i],&n)==FORMAT_DELTA_OK;
        else ok=source_versions_selection_encode(v->sheet,v->sources,v->object_high_water,p->selected,v->sources->count,w.p,p->lengths[i],&n)==SOURCE_VERSIONS_CODEC_OK;
        if (!ok || n!=p->lengths[i]) return DEFINITION_EVENT_INVALID;
        w.p+=n;
    }
    if ((size_t)(w.p-(unsigned char *)arena)!=p->info.bytes) return DEFINITION_EVENT_INVALID;
    *written=p->info.bytes; return DEFINITION_EVENT_OK;
}
void definition_event_packet_free(DefinitionEventPacket *out) { if (out) { free(out->data); *out=(DefinitionEventPacket){0}; } }
DefinitionEventStatus definition_event_capture(DefinitionEventView v,size_t reserved,DefinitionEventPacket *out) {
    if (!out) return DEFINITION_EVENT_INVALID;
    DefinitionEventPlan p={0}; DefinitionEventStatus s=definition_event_prepare(v,reserved,&p); if (s!=DEFINITION_EVENT_OK) return s;
    if (p.info.bytes>reserved || p.info.validation_scratch>reserved-p.info.bytes) return DEFINITION_EVENT_LIMIT;
    DefinitionEventPacket next={dx_alloc(p.info.bytes),p.info.bytes}; size_t written=0;
    s=definition_event_encode_unpublished(&p,reserved-next.bytes,next.data,next.bytes,&written);
    if (s!=DEFINITION_EVENT_OK || written!=next.bytes) { definition_event_packet_free(&next); return s==DEFINITION_EVENT_OK ? DEFINITION_EVENT_INVALID : s; }
    definition_event_packet_free(out); *out=next; return DEFINITION_EVENT_OK;
}
typedef struct {
    DefinitionEventInfo info;
    const unsigned char *parts[6]; size_t lengths[6];
    DefinitionAssetInfo analyses[2]; ObjectsAssetInfo objects[2]; FormatDeltaInfo formats; SourceVersionsAssetInfo sources;
} WirePlan;
static DefinitionEventStatus wire_plan(const void *data,size_t bytes,WirePlan *out) {
    if (!data || bytes<DX_DEFINITION_EVENT_HEADER_BYTES || bytes>DX_DEFINITION_EVENT_MAX_BYTES || memcmp(data,"DXDE",4)) return DEFINITION_EVENT_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.ok=true};
    unsigned encoding=(unsigned)definition_wire_get(&r,2); if (encoding!=DX_DEFINITION_EVENT_ENCODING) return DEFINITION_EVENT_UNSUPPORTED;
    unsigned flags=(unsigned)definition_wire_get(&r,2); WirePlan p={0};
    p.info.sheet=definition_wire_get(&r,8); p.info.object_high_water=definition_wire_get(&r,8); p.info.effects=(unsigned)definition_wire_get(&r,4);
    for (unsigned i=0;i<6;i++) p.lengths[i]=(size_t)definition_wire_get(&r,4);
    p.info.inactive_scopes=(size_t)definition_wire_get(&r,4); p.info.unbound_sources=(size_t)definition_wire_get(&r,4); unsigned zero=(unsigned)definition_wire_get(&r,4);
    if (!r.ok || flags || zero || !p.info.sheet || !effects_valid(p.info.effects) || p.info.inactive_scopes>2u*DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || p.info.unbound_sources>2u*(DX_MAX_OBJECTS+1)) return DEFINITION_EVENT_INVALID;
    for (unsigned i=0;i<6;i++) {
        bool required=i<2 ? !!(p.info.effects&1) : i<4 ? !!(p.info.effects&4) : i==4 ? !!(p.info.effects&8) : true;
        if (!!p.lengths[i]!=required || p.lengths[i]>r.bytes-r.at) return DEFINITION_EVENT_INVALID;
        p.parts[i]=r.data+r.at; r.at+=p.lengths[i];
    }
    if (r.at!=bytes) return DEFINITION_EVENT_INVALID;
    p.info.bytes=bytes; p.info.owned=sizeof(DefinitionEvent);
    for (unsigned i=0;i<6;i++) if (p.lengths[i]) {
        if (i<2) {
            AnalysisAssetStatus s=analysis_asset_preflight(p.parts[i],p.lengths[i],p.info.object_high_water,p.analyses+i);
            if (s!=ANALYSIS_ASSET_OK) return s==ANALYSIS_ASSET_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
            if (p.analyses[i].sheet!=p.info.sheet || p.analyses[i].owner) return DEFINITION_EVENT_INVALID;
            p.info.owned+=p.analyses[i].measure.owned-sizeof(Analysis); p.info.validation_scratch=maximum(p.info.validation_scratch,p.analyses[i].measure.validation_scratch);
        } else if (i<4) {
            ObjectsAssetStatus s=objects_asset_preflight(p.parts[i],p.lengths[i],p.objects+(i-2));
            if (s!=OBJECTS_ASSET_OK) return s==OBJECTS_ASSET_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
            if (p.objects[i-2].sheet!=p.info.sheet || p.objects[i-2].high_water!=p.info.object_high_water) return DEFINITION_EVENT_INVALID;
            p.info.owned+=p.objects[i-2].measure.owned; p.info.validation_scratch=maximum(p.info.validation_scratch,p.objects[i-2].measure.validation_scratch);
        } else if (i==4) {
            FormatDeltaStatus s=format_delta_preflight(p.parts[i],p.lengths[i],&p.formats);
            if (s!=FORMAT_DELTA_OK) return s==FORMAT_DELTA_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
            if (p.formats.sheet!=p.info.sheet) return DEFINITION_EVENT_INVALID;
            p.info.owned+=p.formats.owned;
            p.info.validation_scratch=maximum(p.info.validation_scratch,maximum(p.formats.validation_scratch,
                format_scratch(maximum(p.formats.before_scopes,p.formats.after_scopes))));
        } else {
            SourceVersionsCodecStatus s=source_versions_asset_preflight(p.parts[i],p.lengths[i],p.info.object_high_water,&p.sources);
            if (s!=SOURCE_VERSIONS_CODEC_OK) return s==SOURCE_VERSIONS_CODEC_UNSUPPORTED ? DEFINITION_EVENT_UNSUPPORTED : DEFINITION_EVENT_INVALID;
            if (p.sources.sheet!=p.info.sheet) return DEFINITION_EVENT_INVALID;
            p.info.source_high_water=p.sources.high_water; p.info.owned+=p.sources.owned;
        }
    }
    *out=p; return DEFINITION_EVENT_OK;
}
DefinitionEventStatus definition_event_preflight(const void *data,size_t bytes,DefinitionEventInfo *out) {
    if (!out) return DEFINITION_EVENT_INVALID;
    WirePlan p={0}; DefinitionEventStatus s=wire_plan(data,bytes,&p); if (s==DEFINITION_EVENT_OK) *out=p.info; return s;
}
void definition_event_free(DefinitionEvent *out) {
    if (!out) return;
    analysis_free(&out->before_primary); analysis_free(&out->after_primary); objects_asset_free(&out->before_objects); objects_asset_free(&out->after_objects);
    format_delta_free(&out->formats); source_versions_asset_free(&out->sources); *out=(DefinitionEvent){0};
}
DefinitionEventStatus definition_event_decode(const void *data,size_t bytes,size_t reserved,DefinitionEvent *out) {
    if (!out) return DEFINITION_EVENT_INVALID;
    WirePlan p={0}; DefinitionEventStatus s=wire_plan(data,bytes,&p); if (s!=DEFINITION_EVENT_OK) return s;
    if (p.info.owned>reserved || p.info.validation_scratch>reserved-p.info.owned) return DEFINITION_EVENT_LIMIT;
    DefinitionEvent next={.sheet=p.info.sheet,.object_high_water=p.info.object_high_water,.effects=p.info.effects,
        .inactive_scopes=p.info.inactive_scopes,.unbound_sources=p.info.unbound_sources};
    bool ok=true;
    for (unsigned i=0;i<6 && ok;i++) if (p.lengths[i]) {
        if (i<2) {
            AnalysisAsset a={0}; ok=analysis_asset_decode(p.parts[i],p.lengths[i],p.info.object_high_water,p.analyses[i].measure.owned+p.info.validation_scratch,&a)==ANALYSIS_ASSET_OK;
            if (ok) { if (i) next.after_primary=a.definition; else next.before_primary=a.definition; }
        } else if (i<4) ok=objects_asset_decode(p.parts[i],p.lengths[i],p.objects[i-2].measure.owned+p.info.validation_scratch,i==2 ? &next.before_objects : &next.after_objects)==OBJECTS_ASSET_OK;
        else if (i==4) ok=format_delta_decode(p.parts[i],p.lengths[i],p.formats.owned+p.info.validation_scratch,&next.formats)==FORMAT_DELTA_OK;
        else ok=source_versions_asset_decode(p.parts[i],p.lengths[i],p.info.object_high_water,p.sources.owned,&next.sources)==SOURCE_VERSIONS_CODEC_OK;
    }
    DefinitionEventView view={.sheet=next.sheet,.object_high_water=next.object_high_water,.effects=next.effects,.sources=&next.sources.versions};
    if (next.effects&1) { view.before.primary=&next.before_primary; view.after.primary=&next.after_primary; }
    if (next.effects&4) { view.before.objects=&next.before_objects.objects; view.after.objects=&next.after_objects.objects; }
    if (next.effects&8) { view.before_formats=format_delta_snapshot_view(&next.formats.before); view.after_formats=format_delta_snapshot_view(&next.formats.after); }
    bool selected[DX_SOURCE_VERSIONS]={0}; size_t inactive=0,unbound=0;
    if (ok) ok=changed(view,!!(next.effects&8)) && exact_effects(view,!!(next.effects&8)) && select_sources(view,selected,&inactive,&unbound)==DEFINITION_EVENT_OK &&
        inactive==next.inactive_scopes && unbound==next.unbound_sources;
    if (ok) for (size_t i=0;i<next.sources.versions.count;i++) if (!selected[i]) ok=false;
    if (!ok) { definition_event_free(&next); return DEFINITION_EVENT_INVALID; }
    definition_event_free(out); *out=next; return DEFINITION_EVENT_OK;
}
