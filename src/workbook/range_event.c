#include "range_event.h"
#include "definition_event_internal.h"
#include "history_stream.h"
#include <stdlib.h>
#include <string.h>
/* Explicit worst-case frame sum: bounded cell values+framing, two complete
   object assets, manual format pair, empty selected ledger and width pair. */
typedef char RangeFrameFits[(DX_RANGE_EVENT_HEADER_BYTES+8u*1024u*1024u+DX_RANGE_EVENT_CELLS*(4u+DX_CELL_DELTA_HEADER_BYTES)+DX_DEFINITION_EVENT_HEADER_BYTES+2u*DX_OBJECTS_ASSET_MAX_BYTES+DX_FORMAT_DELTA_MAX_BYTES+DX_SOURCE_VERSIONS_HEADER_BYTES+DX_COLUMN_WIDTH_MAX_BYTES<=DX_RANGE_EVENT_MAX_BYTES) ? 1 : -1];
typedef char RangeStreamFits[(DX_RANGE_EVENT_MAX_BYTES<DX_HISTORY_STREAM_BYTES && DX_RANGE_EVENT_MAX_BYTES<=UINT32_MAX) ? 1 : -1];
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static uint64_t get(const unsigned char *p,unsigned n) { uint64_t value=0; for (unsigned i=0;i<n;i++) value|=(uint64_t)p[i]<<(8*i); return value; }
static bool effects(unsigned value) { return value && !(value&~30u); }
static bool manual(FormatSnapshotView v) { return !v.source_aware && !v.scope_count && !v.scopes; }
static bool ordered(CellDelta a,CellDelta b) { return a.row<b.row || (a.row==b.row && a.column<b.column); }
static bool templates(const ObjectSnapshot *s) {
    if (!s || s->count>DX_MAX_OBJECTS || (s->count && !s->items)) return false;
    for (size_t i=0;i<s->count;i++) if (s->items[i].kind==OBJECT_TABLE || s->items[i].source_id || s->items[i].source.kind!=SOURCE_NONE) return false;
    return true;
}
static bool empty_formats(FormatSnapshotView v) { return manual(v) && !v.items && !v.count; }
static bool empty_definition(DefinitionEventView v) {
    return !v.sheet && !v.object_high_water && !v.effects && !v.before.primary && !v.after.primary && !v.before.objects && !v.after.objects && !v.sources && empty_formats(v.before_formats) && empty_formats(v.after_formats);
}
static bool definition_manual(DefinitionEventView v,uint64_t sheet,unsigned effects) {
    return v.sheet==sheet && v.effects==(effects&12u) && !v.before.primary && !v.after.primary && templates(v.before.objects) && templates(v.after.objects) && manual(v.before_formats) && manual(v.after_formats);
}
RangeEventStatus range_event_measure(RangeEventView view,size_t reserved,RangeEventInfo *out) {
    if (!out || !view.sheet || !effects(view.effects) || view.count>DX_RANGE_EVENT_CELLS || (view.count && !view.cells) || !!(view.effects&2)!=!!view.count) return RANGE_EVENT_INVALID;
    DefinitionEventInfo definitions={0};
    if (view.effects&4) {
        if (!definition_manual(view.definitions,view.sheet,view.effects) || !empty_formats(view.before_formats) || !empty_formats(view.after_formats)) return RANGE_EVENT_UNSUPPORTED;
        DefinitionEventStatus status=definition_event_measure(view.definitions,reserved,&definitions);
        if (status!=DEFINITION_EVENT_OK) return status==DEFINITION_EVENT_LIMIT ? RANGE_EVENT_LIMIT : RANGE_EVENT_INVALID;
        if (definitions.inactive_scopes || definitions.unbound_sources) return RANGE_EVENT_UNSUPPORTED;
    } else if (!empty_definition(view.definitions)) return RANGE_EVENT_INVALID;
    if ((view.effects&8) && !(view.effects&4)) {
        if (!manual(view.before_formats) || !manual(view.after_formats)) return RANGE_EVENT_UNSUPPORTED;
        if (reserved<DX_FORMAT_VALIDATION_BYTES) return RANGE_EVENT_LIMIT;
    } else if (view.before_formats.items || view.before_formats.count || view.after_formats.items || view.after_formats.count || !manual(view.before_formats) || !manual(view.after_formats)) return RANGE_EVENT_INVALID;
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
    bytes+=formats; if (widths>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT; bytes+=widths; if (definitions.bytes>DX_RANGE_EVENT_MAX_BYTES-bytes) return RANGE_EVENT_LIMIT; bytes+=definitions.bytes;
    /* Format decoded ownership is bounded conservatively before encoding; wire
       preflight supplies its exact count-sized graph charge during decode. */
    size_t owned=bytes+view.count*sizeof(CellDelta)+(formats ? 2u*DX_FORMAT_CURRENT_BYTES : 0)+(view.widths.before.count+view.widths.after.count)*sizeof(ColumnWidthEntry)+definitions.owned;
    *out=(RangeEventInfo){view.sheet,view.effects,view.count,bytes,owned,definitions.validation_scratch>(((view.effects&8) && !(view.effects&4)) ? DX_FORMAT_VALIDATION_BYTES : 0) ? definitions.validation_scratch : (((view.effects&8) && !(view.effects&4)) ? DX_FORMAT_VALIDATION_BYTES : 0),formats,widths,(view.widths.before.count+view.widths.after.count)*sizeof(ColumnWidthEntry),definitions.bytes,definitions.owned}; return RANGE_EVENT_OK;
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
            for (size_t r=0;r<4;r++) for (size_t j=0;j<o->pivot.counts[r];j++) {
                const PivotField *f=&o->pivot.fields[r][j];
                if ((f->name && overlap(output,bytes,f->name,strlen(f->name)+1)) || (f->value && overlap(output,bytes,f->value,strlen(f->value)+1))) return true;
            }
            for (size_t r=0;r<DX_MARK_ROLES;r++) for (size_t j=0;j<o->marks.counts[r];j++) { const char *name=o->marks.fields[r][j].name; if (name && overlap(output,bytes,name,strlen(name)+1)) return true; }
        }
    }
    const SourceVersions *sources=v.definitions.sources;
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
    unsigned char *data=arena; memcpy(data,"DXRE",4); put(data+4,DX_RANGE_EVENT_ENCODING,2); put(data+6,0,2); put(data+8,view.sheet,8); put(data+16,view.effects,4); put(data+20,view.count,4); put(data+28,info.width_bytes,4); put(data+32,info.definition_bytes,4); put(data+36,0,4);
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
    *written=info.bytes; return RANGE_EVENT_OK;
}
static bool manual_wire(const unsigned char *p,size_t n) { return n>=DX_FORMAT_SNAPSHOT_HEADER_BYTES && !p[0] && !get(p+4,2); }
RangeEventStatus range_event_preflight(const void *input,size_t bytes,RangeEventInfo *out) {
    if (!input || !out || bytes<DX_RANGE_EVENT_HEADER_BYTES || bytes>DX_RANGE_EVENT_MAX_BYTES) return RANGE_EVENT_INVALID;
    const unsigned char *data=input;
    if (memcmp(data,"DXRE",4)) return RANGE_EVENT_INVALID;
    if (get(data+4,2)!=DX_RANGE_EVENT_ENCODING) return RANGE_EVENT_UNSUPPORTED;
    uint64_t sheet=get(data+8,8); unsigned effect=(unsigned)get(data+16,4); size_t count=(size_t)get(data+20,4),format_bytes=(size_t)get(data+24,4),width_bytes=(size_t)get(data+28,4),definition_bytes=(size_t)get(data+32,4);
    if (get(data+6,2) || get(data+36,4) || !sheet || !effects(effect) || count>DX_RANGE_EVENT_CELLS || !!count!=!!(effect&2) || !!format_bytes!=!!((effect&8) && !(effect&4)) || !!definition_bytes!=!!(effect&4) || !!width_bytes!=!!(effect&16) || format_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES || width_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES-format_bytes || definition_bytes>bytes-DX_RANGE_EVENT_HEADER_BYTES-format_bytes-width_bytes) return RANGE_EVENT_INVALID;
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
    if (format_bytes>bytes-at || width_bytes>bytes-at-format_bytes || definition_bytes!=bytes-at-format_bytes-width_bytes || values>2u*4u*1024u*1024u) return RANGE_EVENT_INVALID;
    FormatDeltaInfo f={0};
    if (format_bytes) {
        FormatDeltaStatus status=format_delta_preflight(data+at,format_bytes,&f);
        if (status==FORMAT_DELTA_UNSUPPORTED) return RANGE_EVENT_UNSUPPORTED;
        if (status!=FORMAT_DELTA_OK || f.sheet!=sheet || !manual_wire(data+at+DX_FORMAT_DELTA_HEADER_BYTES,f.before_bytes) ||
            !manual_wire(data+at+DX_FORMAT_DELTA_HEADER_BYTES+f.before_bytes,f.after_bytes)) return RANGE_EVENT_INVALID;
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
        if (status!=DEFINITION_EVENT_OK || d.sheet!=sheet || d.effects!=(effect&12u) || d.inactive_scopes || d.unbound_sources) return RANGE_EVENT_INVALID;
    }
    *out=(RangeEventInfo){sheet,effect,count,bytes,bytes+count*sizeof(CellDelta)+f.owned+w.owned+d.owned,f.validation_scratch>d.validation_scratch ? f.validation_scratch : d.validation_scratch,format_bytes,width_bytes,w.owned,definition_bytes,d.owned}; return RANGE_EVENT_OK;
}
void range_event_free(RangeEvent *event) { if (event) { free(event->data); free(event->cells); format_delta_free(&event->formats); column_width_delta_free(&event->widths); definition_event_free(&event->definitions); *event=(RangeEvent){0}; } }
RangeEventStatus range_event_decode(const void *input,size_t bytes,size_t reserved,RangeEvent *out) {
    if (!out) return RANGE_EVENT_INVALID; RangeEventInfo info={0}; RangeEventStatus status=range_event_preflight(input,bytes,&info); if (status!=RANGE_EVENT_OK) return status;
    if (info.owned>reserved || info.validation_scratch>reserved-info.owned) return RANGE_EVENT_LIMIT;
    RangeEvent next={.sheet=info.sheet,.effects=info.effects,.bytes=bytes,.count=info.count};
    next.data=dx_alloc(bytes); memcpy(next.data,input,bytes); if (next.count) next.cells=dx_alloc(next.count*sizeof(*next.cells));
    unsigned char *data=next.data; size_t at=DX_RANGE_EVENT_HEADER_BYTES;
    for (size_t i=0;i<next.count;i++) { size_t child=(size_t)get(data+at,4); at+=4; cell_delta_decode(data+at,child,next.cells+i); at+=child; }
    if (info.format_bytes) {
        if (format_delta_decode(data+at,info.format_bytes,reserved-bytes-next.count*sizeof(CellDelta)-info.width_owned-info.definition_owned,&next.formats)!=FORMAT_DELTA_OK ||
            !manual(format_delta_snapshot_view(&next.formats.before)) || !manual(format_delta_snapshot_view(&next.formats.after))) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    }
    at+=info.format_bytes;
    if (info.width_bytes && column_width_decode(data+at,info.width_bytes,info.width_owned,&next.widths)!=COLUMN_WIDTH_OK) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    at+=info.width_bytes;
    if (info.definition_bytes) {
        DefinitionEventStatus decoded=definition_event_decode(data+at,info.definition_bytes,info.definition_owned+info.validation_scratch,&next.definitions);
        DefinitionEvent *d=&next.definitions;
        if (decoded!=DEFINITION_EVENT_OK || !templates(&d->before_objects.objects) || !templates(&d->after_objects.objects) || d->sources.versions.count || !manual(format_delta_snapshot_view(&d->formats.before)) || !manual(format_delta_snapshot_view(&d->formats.after))) { range_event_free(&next); return RANGE_EVENT_INVALID; }
    }
    range_event_free(out); *out=next; return RANGE_EVENT_OK;
}
