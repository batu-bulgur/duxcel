#include "range_event.h"
#include "definition_event_internal.h"
#include "history_stream.h"
#include <stdlib.h>
#include <string.h>
/* Branch-specific worst-case sums; never allocate these maxima speculatively. */
#define RANGE_CELL_FRAME_MAX (8u*1024u*1024u+DX_RANGE_EVENT_CELLS*(4u+DX_CELL_DELTA_HEADER_BYTES))
typedef char RangeObjectFrameFits[(DX_RANGE_EVENT_HEADER_BYTES+RANGE_CELL_FRAME_MAX+DX_DEFINITION_EVENT_MAX_BYTES+DX_COLUMN_WIDTH_MAX_BYTES<=DX_RANGE_EVENT_MAX_BYTES) ? 1 : -1];
typedef char RangeFormatFrameFits[(DX_RANGE_EVENT_HEADER_BYTES+RANGE_CELL_FRAME_MAX+DX_FORMAT_DELTA_MAX_BYTES+DX_SOURCE_VERSIONS_MAX_BYTES+DX_COLUMN_WIDTH_MAX_BYTES<=DX_RANGE_EVENT_MAX_BYTES) ? 1 : -1];
typedef char RangeStreamFits[(DX_RANGE_EVENT_MAX_BYTES<DX_HISTORY_STREAM_BYTES && DX_RANGE_EVENT_MAX_BYTES<=UINT32_MAX) ? 1 : -1];
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static uint64_t get(const unsigned char *p,unsigned n) { uint64_t value=0; for (unsigned i=0;i<n;i++) value|=(uint64_t)p[i]<<(8*i); return value; }
static bool effects(unsigned value) { return value && !(value&~30u); }
static bool manual(FormatSnapshotView v) { return !v.source_aware && !v.scope_count && !v.scopes; }
static bool ordered(CellDelta a,CellDelta b) { return a.row<b.row || (a.row==b.row && a.column<b.column); }
static bool empty_formats(FormatSnapshotView v) { return manual(v) && !v.items && !v.count; }
static bool empty_definition(DefinitionEventView v) {
    return !v.sheet && !v.object_high_water && !v.effects && !v.before.primary && !v.after.primary && !v.before.objects && !v.after.objects && !v.sources && empty_formats(v.before_formats) && empty_formats(v.after_formats);
}
static bool definition_pair(DefinitionEventView v,uint64_t sheet,unsigned effect,uint64_t high) {
    return v.sheet==sheet && v.object_high_water==high && v.effects==(effect&12u) && !v.before.primary && !v.after.primary && v.before.objects && v.after.objects;
}
static size_t source_scratch(size_t scopes) { return DX_FORMAT_VALIDATION_BYTES+(scopes ? analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_SOURCE_SCOPE_TEXT_BYTES,true)+scopes : 0); }
RangeEventStatus range_event_measure(RangeEventView view,size_t reserved,RangeEventInfo *out) {
    if (!out || !view.sheet || !effects(view.effects) || view.count>DX_RANGE_EVENT_CELLS || (view.count && !view.cells) || !!(view.effects&2)!=!!view.count) return RANGE_EVENT_INVALID;
    DefinitionEventInfo definitions={0};
    if (view.effects&4) {
        if (!definition_pair(view.definitions,view.sheet,view.effects,view.object_high_water) || view.sources || !empty_formats(view.before_formats) || !empty_formats(view.after_formats)) return RANGE_EVENT_UNSUPPORTED;
        DefinitionEventStatus status=definition_event_measure(view.definitions,reserved,&definitions);
        if (status!=DEFINITION_EVENT_OK) return status==DEFINITION_EVENT_LIMIT ? RANGE_EVENT_LIMIT : RANGE_EVENT_INVALID;
        if (definitions.inactive_scopes) return RANGE_EVENT_UNSUPPORTED;
    } else if (!empty_definition(view.definitions)) return RANGE_EVENT_INVALID;
    FormatDependencySelection dependencies={0}; SourceVersionsSelectionMeasure source={0}; size_t scratch=definitions.validation_scratch;
    if ((view.effects&8) && !(view.effects&4)) {
        if (!view.sources || view.before_formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS || view.after_formats.scope_count>DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) return RANGE_EVENT_INVALID;
        size_t need=source_scratch(view.before_formats.scope_count+view.after_formats.scope_count); if (scratch<need) scratch=need;
        if (reserved<scratch) return RANGE_EVENT_LIMIT;
        FormatDependenciesStatus selected=format_dependencies_select(view.sheet,view.before_formats,view.after_formats,view.sources,view.object_high_water,&dependencies);
        if (selected!=FORMAT_DEPENDENCIES_OK) return RANGE_EVENT_INVALID;
        if (dependencies.legacy_scopes) return RANGE_EVENT_UNSUPPORTED;
        if (source_versions_selection_measure(view.sheet,view.sources,view.object_high_water,dependencies.rows,view.sources->count,&source)!=SOURCE_VERSIONS_CODEC_OK) return RANGE_EVENT_INVALID;
    } else if (!empty_formats(view.before_formats) || !empty_formats(view.after_formats) || view.sources) return RANGE_EVENT_INVALID;
    size_t widths=0;
    if (view.effects&16) {
        if (column_width_delta_size(view.widths.before,view.widths.after,&widths)!=COLUMN_WIDTH_OK) return RANGE_EVENT_INVALID;
    } else if (view.widths.before.entries || view.widths.before.count || view.widths.after.entries || view.widths.after.count) return RANGE_EVENT_INVALID;
    size_t bytes=DX_RANGE_EVENT_HEADER_BYTES,values=0,before_count=0,after_count=0,before_text=0,after_text=0;
    for (size_t i=0;i<view.count;i++) {
        CellDelta delta=view.cells[i]; size_t child=0;
        if (delta.sheet!=view.sheet || (i && !ordered(view.cells[i-1],delta)) || cell_delta_encoded_size(delta,&child)!=CELL_DELTA_CODEC_OK || bytes>DX_RANGE_EVENT_MAX_BYTES-4 || child>DX_RANGE_EVENT_MAX_BYTES-bytes-4) return RANGE_EVENT_INVALID;
        if ((delta.before_bytes && (++before_count>20000 || delta.before_bytes+1>4u*1024u*1024u-before_text)) ||
            (delta.after_bytes && (++after_count>20000 || delta.after_bytes+1>4u*1024u*1024u-after_text))) return RANGE_EVENT_LIMIT;
        if (delta.before_bytes) before_text+=delta.before_bytes+1; if (delta.after_bytes) after_text+=delta.after_bytes+1;
        bytes+=4+child; if (delta.before_bytes+delta.after_bytes>2u*4u*1024u*1024u-values) return RANGE_EVENT_LIMIT; values+=delta.before_bytes+delta.after_bytes;
    }
    if (values>2u*4u*1024u*1024u) return RANGE_EVENT_LIMIT;
    size_t formats=0;
    if ((view.effects&8) && !(view.effects&4) && format_delta_encoded_size(view.sheet,view.before_formats,view.after_formats,&formats)!=FORMAT_DELTA_OK) return RANGE_EVENT_INVALID;
    if (formats>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT;
    bytes+=formats; if (widths>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT; bytes+=widths; if (definitions.bytes>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT; bytes+=definitions.bytes; if (source.bytes>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT; bytes+=source.bytes;
    /* Format decoded ownership is bounded conservatively before encoding; wire
       preflight supplies its exact count-sized graph charge during decode. */
    size_t owned=bytes+view.count*sizeof(CellDelta)+(formats ? 2u*DX_FORMAT_CURRENT_BYTES : 0)+(view.widths.before.count+view.widths.after.count)*sizeof(ColumnWidthEntry)+definitions.owned+source.owned;
    *out=(RangeEventInfo){.sheet=view.sheet,.effects=view.effects,.count=view.count,.bytes=bytes,.owned=owned,.validation_scratch=scratch,.format_bytes=formats,.width_bytes=widths,.width_owned=(view.widths.before.count+view.widths.after.count)*sizeof(ColumnWidthEntry),.definition_bytes=definitions.bytes,.definition_owned=definitions.owned,.source_bytes=source.bytes,.source_owned=source.owned,.object_high_water=view.object_high_water,.unbound_sources=definitions.unbound_sources}; return RANGE_EVENT_OK;
}
static bool overlap(const void *a,size_t na,const void *b,size_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if (!na || !nb) return false;
    if (x>UINTPTR_MAX-na || y>UINTPTR_MAX-nb) return true;
    return x<y+nb && y<x+na;
}
static bool aliases(RangeEventView v,const void *output,size_t bytes) {
    if (overlap(output,bytes,v.cells,v.count*sizeof(*v.cells)) ||
        overlap(output,bytes,v.widths.before.entries,v.widths.before.count*sizeof(ColumnWidthEntry)) ||
        overlap(output,bytes,v.widths.after.entries,v.widths.after.count*sizeof(ColumnWidthEntry))) return true;
    for (size_t i=0;i<v.count;i++) if (overlap(output,bytes,v.cells[i].before,v.cells[i].before_bytes) || overlap(output,bytes,v.cells[i].after,v.cells[i].after_bytes)) return true;
    FormatSnapshotView sides[]={v.before_formats,v.after_formats,v.definitions.before_formats,v.definitions.after_formats};
    for (size_t k=0;k<4;k++) {
        FormatSnapshotView side=sides[k];
        if (overlap(output,bytes,side.scopes,side.scope_count*sizeof(FormatScope))) return true;
        for (size_t i=0;i<side.scope_count;i++) if (overlap(output,bytes,side.scopes[i].definition,side.scopes[i].bytes)) return true;
        if (overlap(output,bytes,side.items,side.count*sizeof(FormatPatch))) return true;
        for (size_t i=0;i<side.count;i++) {
            const FormatPatch *p=side.items+i;
            if (overlap(output,bytes,p->guard_ids,p->guard_count*sizeof(size_t)) || overlap(output,bytes,p->record_ids,p->record_count*sizeof(uint64_t)) || overlap(output,bytes,p->fields,p->field_count*sizeof(size_t))) return true;
        }
    }
    const ObjectSnapshot *objects[]={v.definitions.before.objects,v.definitions.after.objects};
    for (size_t k=0;k<2;k++) if (objects[k]) {
        const ObjectSnapshot *s=objects[k];
        if (overlap(output,bytes,s,sizeof(*s)) || overlap(output,bytes,s->items,s->count*sizeof(SheetObject))) return true;
        for (size_t i=0;i<s->count;i++) {
            const SheetObject *o=s->items+i;
            if (o->name && overlap(output,bytes,o->name,strlen(o->name)+1)) return true;
            const Analysis *a=&o->source; const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
            for (size_t t=0;t<sizeof(texts)/sizeof(*texts);t++) if (texts[t] && overlap(output,bytes,texts[t],strlen(texts[t])+1)) return true;
            for (size_t t=0;t<a->filter_count;t++) {
                const AnalysisFilter *f=a->filters+t;
                if ((f->column && overlap(output,bytes,f->column,strlen(f->column)+1)) || (f->value && overlap(output,bytes,f->value,strlen(f->value)+1))) return true;
            }
            for (size_t t=0;t<a->sort_count;t++) if (a->sorts[t].column && overlap(output,bytes,a->sorts[t].column,strlen(a->sorts[t].column)+1)) return true;
            for (size_t r=0;r<4;r++) for (size_t j=0;j<o->pivot.counts[r];j++) {
                const PivotField *f=&o->pivot.fields[r][j];
                if ((f->name && overlap(output,bytes,f->name,strlen(f->name)+1)) || (f->value && overlap(output,bytes,f->value,strlen(f->value)+1))) return true;
            }
            for (size_t r=0;r<DX_MARK_ROLES;r++) for (size_t j=0;j<o->marks.counts[r];j++) { const char *name=o->marks.fields[r][j].name; if (name && overlap(output,bytes,name,strlen(name)+1)) return true; }
        }
    }
    const SourceVersions *sources=(v.effects&4) ? v.definitions.sources : v.sources;
    if (sources) {
        if (overlap(output,bytes,sources,sizeof(*sources)) || overlap(output,bytes,sources->items,sources->count*sizeof(SourceVersion))) return true;
        for (size_t i=0;i<sources->count;i++) { const SourceVersion *s=sources->items+i;
            if ((s->path && overlap(output,bytes,s->path,strlen(s->path)+1)) || overlap(output,bytes,s->raw.data,s->raw.bytes)) return true;
        }
    }
    return false;
}
RangeEventStatus range_event_encode(RangeEventView view,size_t reserved,void *arena,size_t capacity,size_t *written) {
    if (!arena || !written) return RANGE_EVENT_INVALID; RangeEventInfo info={0}; RangeEventStatus status=range_event_measure(view,reserved,&info); if (status!=RANGE_EVENT_OK) return status;
    if (info.bytes>capacity) return RANGE_EVENT_LIMIT;
    if (overlap(arena,info.bytes,written,sizeof(*written)) || aliases(view,arena,info.bytes) || aliases(view,written,sizeof(*written))) return RANGE_EVENT_INVALID;
    DefinitionEventPlan definition={0};
    if (info.definition_bytes && definition_event_prepare(view.definitions,reserved,&definition)!=DEFINITION_EVENT_OK) return RANGE_EVENT_INVALID;
    unsigned char *data=arena; memcpy(data,"DXRE",4); put(data+4,DX_RANGE_EVENT_ENCODING,2); put(data+6,0,2); put(data+8,view.sheet,8); put(data+16,view.effects,4); put(data+20,view.count,4); put(data+28,info.width_bytes,4); put(data+32,info.definition_bytes,4); put(data+36,0,4); put(data+40,view.object_high_water,8); put(data+48,info.source_bytes,4); put(data+52,0,4);
    size_t at=DX_RANGE_EVENT_HEADER_BYTES;
    for (size_t i=0;i<view.count;i++) {
        size_t child=0,n=0; cell_delta_encoded_size(view.cells[i],&child); put(data+at,child,4); at+=4;
        if (cell_delta_encode(view.cells[i],data+at,child,&n)!=CELL_DELTA_CODEC_OK || n!=child) return RANGE_EVENT_INVALID; at+=child;
    }
    size_t format_bytes=info.format_bytes,n=0; put(data+24,format_bytes,4);
    if (format_bytes && (format_delta_encode(view.sheet,view.before_formats,view.after_formats,data+at,format_bytes,&n)!=FORMAT_DELTA_OK || n!=format_bytes)) return RANGE_EVENT_INVALID;
    at+=format_bytes;
    if (info.width_bytes && (column_width_encode(view.widths.before,view.widths.after,data+at,info.width_bytes,&n)!=COLUMN_WIDTH_OK || n!=info.width_bytes)) return RANGE_EVENT_INVALID;
    at+=info.width_bytes;
    if (info.definition_bytes && (definition_event_encode_unpublished(&definition,reserved,data+at,info.definition_bytes,&n)!=DEFINITION_EVENT_OK || n!=info.definition_bytes)) return RANGE_EVENT_INVALID;
    at+=info.definition_bytes;
    if (info.source_bytes) {
        FormatDependencySelection selected={0};
        if (format_dependencies_select(view.sheet,view.before_formats,view.after_formats,view.sources,view.object_high_water,&selected)!=FORMAT_DEPENDENCIES_OK || selected.legacy_scopes ||
            source_versions_selection_encode(view.sheet,view.sources,view.object_high_water,selected.rows,view.sources->count,data+at,info.source_bytes,&n)!=SOURCE_VERSIONS_CODEC_OK || n!=info.source_bytes) return RANGE_EVENT_INVALID;
    }
    *written=info.bytes; return RANGE_EVENT_OK;
}
RangeEventStatus range_event_preflight(const void *input,size_t bytes,RangeEventInfo *out) {
    if (!input || !out || bytes<DX_RANGE_EVENT_HEADER_BYTES || bytes>DX_RANGE_EVENT_MAX_BYTES) return RANGE_EVENT_INVALID;
    const unsigned char *data=input;
    if (memcmp(data,"DXRE",4)) return RANGE_EVENT_INVALID;
    if (get(data+4,2)!=DX_RANGE_EVENT_ENCODING) return RANGE_EVENT_UNSUPPORTED;
    uint64_t sheet=get(data+8,8); unsigned effect=(unsigned)get(data+16,4); size_t count=(size_t)get(data+20,4),format_bytes=(size_t)get(data+24,4),width_bytes=(size_t)get(data+28,4),definition_bytes=(size_t)get(data+32,4),source_bytes=(size_t)get(data+48,4); uint64_t high=get(data+40,8);
    if (get(data+6,2) || get(data+36,4) || get(data+52,4) || !sheet || !effects(effect) || count>DX_RANGE_EVENT_CELLS || !!count!=!!(effect&2) || !!format_bytes!=!!((effect&8) && !(effect&4)) || !!definition_bytes!=!!(effect&4) || !!width_bytes!=!!(effect&16) || !!source_bytes!=!!format_bytes || format_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES || width_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES-format_bytes || definition_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES-format_bytes-width_bytes || source_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES-format_bytes-width_bytes-definition_bytes) return RANGE_EVENT_INVALID;
    size_t at=DX_RANGE_EVENT_HEADER_BYTES,values=0,before_count=0,after_count=0,before_text=0,after_text=0; CellDelta prior={0};
    for (size_t i=0;i<count;i++) {
        if (bytes-at<4) return RANGE_EVENT_INVALID; size_t child=(size_t)get(data+at,4); at+=4;
        if (child>bytes-at) return RANGE_EVENT_INVALID; CellDelta current={0}; CellDeltaCodecStatus decoded=cell_delta_decode(data+at,child,&current);
        if (decoded==CELL_DELTA_CODEC_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (decoded!=CELL_DELTA_CODEC_OK || current.sheet!=sheet || (i && !ordered(prior,current))) return RANGE_EVENT_INVALID;
        if ((current.before_bytes && (++before_count>20000 || current.before_bytes+1>4u*1024u*1024u-before_text)) ||
            (current.after_bytes && (++after_count>20000 || current.after_bytes+1>4u*1024u*1024u-after_text))) return RANGE_EVENT_INVALID;
        if (current.before_bytes) before_text+=current.before_bytes+1; if (current.after_bytes) after_text+=current.after_bytes+1;
        if (current.before_bytes+current.after_bytes>2u*4u*1024u*1024u-values) return RANGE_EVENT_INVALID; values+=current.before_bytes+current.after_bytes; prior=current; at+=child;
    }
    if (format_bytes>bytes-at || width_bytes>bytes-at-format_bytes || definition_bytes>bytes-at-format_bytes-width_bytes || source_bytes!=bytes-at-format_bytes-width_bytes-definition_bytes || values>2u*4u*1024u*1024u) return RANGE_EVENT_INVALID;
    FormatDeltaInfo f={0};
    if (format_bytes) {
        FormatDeltaStatus status=format_delta_preflight(data+at,format_bytes,&f);
        if (status==FORMAT_DELTA_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (status!=FORMAT_DELTA_OK || f.sheet!=sheet) return RANGE_EVENT_INVALID;
    }
    ColumnWidthInfo w={0};
    if (width_bytes) {
        ColumnWidthStatus status=column_width_preflight(data+at+format_bytes,width_bytes,&w);
        if (status==COLUMN_WIDTH_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (status!=COLUMN_WIDTH_OK) return RANGE_EVENT_INVALID;
    }
    DefinitionEventInfo d={0};
    if (definition_bytes) {
        DefinitionEventStatus status=definition_event_preflight(data+at+format_bytes+width_bytes,definition_bytes,&d);
        if (status==DEFINITION_EVENT_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (status!=DEFINITION_EVENT_OK || d.sheet!=sheet || d.object_high_water!=high || d.effects!=(effect&12u) || d.inactive_scopes) return RANGE_EVENT_INVALID;
    }
    SourceVersionsAssetInfo sources={0}; size_t scratch=f.validation_scratch>d.validation_scratch ? f.validation_scratch : d.validation_scratch;
    if (source_bytes) {
        SourceVersionsCodecStatus status=source_versions_asset_preflight(data+at+format_bytes+width_bytes+definition_bytes,source_bytes,high,&sources);
        if (status==SOURCE_VERSIONS_CODEC_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (status!=SOURCE_VERSIONS_CODEC_OK || sources.sheet!=sheet) return RANGE_EVENT_INVALID;
        const unsigned char *before=data+at+DX_FORMAT_DELTA_HEADER_BYTES,*after=before+f.before_bytes;
        size_t need=source_scratch((size_t)get(before+4,2)+(size_t)get(after+4,2)); if (scratch<need) scratch=need;
    }
    *out=(RangeEventInfo){.sheet=sheet,.effects=effect,.count=count,.bytes=bytes,.owned=bytes+count*sizeof(CellDelta)+f.owned+w.owned+d.owned+sources.owned,.validation_scratch=scratch,.format_bytes=format_bytes,.width_bytes=width_bytes,.width_owned=w.owned,.definition_bytes=definition_bytes,.definition_owned=d.owned,.source_bytes=source_bytes,.source_owned=sources.owned,.object_high_water=high,.unbound_sources=d.unbound_sources}; return RANGE_EVENT_OK;
}
void range_event_free(RangeEvent *event) { if (event) { free(event->data); free(event->cells); format_delta_free(&event->formats); column_width_delta_free(&event->widths); definition_event_free(&event->definitions); source_versions_asset_free(&event->sources); *event=(RangeEvent){0}; } }
RangeEventStatus range_event_decode(const void *input,size_t bytes,size_t reserved,RangeEvent *out) {
    if (!out) return RANGE_EVENT_INVALID; RangeEventInfo info={0}; RangeEventStatus status=range_event_preflight(input,bytes,&info); if (status!=RANGE_EVENT_OK) return status;
    if (info.owned>reserved || info.validation_scratch>reserved-info.owned) return RANGE_EVENT_LIMIT;
    RangeEvent next={.sheet=info.sheet,.effects=info.effects,.bytes=bytes,.count=info.count,.object_high_water=info.object_high_water};
    next.data=dx_alloc(bytes); memcpy(next.data,input,bytes); if (next.count) next.cells=dx_alloc(next.count*sizeof(*next.cells));
    unsigned char *data=next.data; size_t at=DX_RANGE_EVENT_HEADER_BYTES;
    for (size_t i=0;i<next.count;i++) { size_t child=(size_t)get(data+at,4); at+=4; cell_delta_decode(data+at,child,next.cells+i); at+=child; }
    if (info.format_bytes) {
        if (format_delta_decode(data+at,info.format_bytes,reserved-bytes-next.count*sizeof(CellDelta)-info.width_owned-info.definition_owned-info.source_owned,&next.formats)!=FORMAT_DELTA_OK) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    }
    at+=info.format_bytes;
    if (info.width_bytes && column_width_decode(data+at,info.width_bytes,info.width_owned,&next.widths)!=COLUMN_WIDTH_OK) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    at+=info.width_bytes;
    if (info.definition_bytes) {
        DefinitionEventStatus decoded=definition_event_decode(data+at,info.definition_bytes,info.definition_owned+info.validation_scratch,&next.definitions);
        DefinitionEvent *d=&next.definitions;
        if (decoded!=DEFINITION_EVENT_OK || d->sheet!=info.sheet || d->object_high_water!=info.object_high_water || d->effects!=(info.effects&12u) || d->before_primary.kind!=SOURCE_NONE || d->after_primary.kind!=SOURCE_NONE || d->inactive_scopes) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    }
    at+=info.definition_bytes;
    if (info.source_bytes) {
        FormatDependencySelection selected={0};
        if (source_versions_asset_decode(data+at,info.source_bytes,info.object_high_water,info.source_owned,&next.sources)!=SOURCE_VERSIONS_CODEC_OK || next.sources.sheet!=info.sheet ||
            format_dependencies_select(info.sheet,format_delta_snapshot_view(&next.formats.before),format_delta_snapshot_view(&next.formats.after),&next.sources.versions,info.object_high_water,&selected)!=FORMAT_DEPENDENCIES_OK || selected.legacy_scopes) { range_event_free(&next); return RANGE_EVENT_INVALID; }
        for (size_t i=0;i<next.sources.versions.count;i++) if (!selected.rows[i]) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    }
    next.unbound_sources=next.definitions.unbound_sources;
    range_event_free(out); *out=next; return RANGE_EVENT_OK;
}
