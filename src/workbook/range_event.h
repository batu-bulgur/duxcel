#ifndef DUXCEL_RANGE_EVENT_H
#define DUXCEL_RANGE_EVENT_H
#include "cell_delta_codec.h"
#include "format_delta_codec.h"
#include "column_widths.h"
#include "definition_event.h"
#define DX_RANGE_EVENT_ENCODING 4u
#define DX_RANGE_EVENT_HEADER_BYTES 56u
#define DX_RANGE_EVENT_CELLS 40000u
#define DX_RANGE_EVENT_MAX_BYTES (30u*1024u*1024u)
typedef struct {
    uint64_t sheet;
    unsigned effects; /* Nonzero subset of 2 Cells,4 Objects,8 Format,16 Width; every part changes. */
    const CellDelta *cells;
    size_t count;
    FormatSnapshotView before_formats,after_formats;
    ColumnWidthPair widths;
    DefinitionEventView definitions; /* Objects bit: exact closed DXDE4/12. */
    uint64_t object_high_water;
    const SourceVersions *sources; /* Format without Objects: unchanged ledger. */
} RangeEventView;
typedef struct { uint64_t sheet; unsigned effects; size_t count,bytes,owned,validation_scratch,format_bytes,width_bytes,width_owned,definition_bytes,definition_owned,source_bytes,source_owned; uint64_t object_high_water; size_t unbound_sources; } RangeEventInfo;
typedef struct {
    uint64_t sheet; unsigned effects;
    void *data; size_t bytes;
    CellDelta *cells; size_t count; /* Spans borrow ONLY this event's owned data. */
    FormatDelta formats; /* Independent owned graphs; no packet/string aliases. */
    DefinitionEvent definitions; /* Owns Objects and optional formats when bit4. */
    ColumnWidthDelta widths; /* Complete independent sparse virtual states. */
    uint64_t object_high_water; SourceVersionsAsset sources; size_t unbound_sources;
} RangeEvent;
typedef enum { RANGE_EVENT_OK,RANGE_EVENT_NOOP,RANGE_EVENT_INVALID,RANGE_EVENT_LIMIT,RANGE_EVENT_UNSUPPORTED } RangeEventStatus;
/* Current closed range compound. DXRE/u16 encoding4/u16 zero/u64 sheet,
   u32 effects/count/format-bytes/width-bytes/definition-bytes/zero, u64 object
   high-water, u32 source-bytes/zero. Canonical DXCD vector, then DXFD, DXCW,
   DXDE, DXSV parts. Objects require DXDE4/12 and no separate DXFD/DXSV.
   Format without Objects requires DXFD+selected DXSV. Exact source closure,
   sheet/high-water/effect associations and truthful changed parts are mandatory.
   Opaque/inactive format scopes are unsupported. Unbound definitions remain
   explicit inactive metadata, never identity issuance or result/readiness proof.
   Only current
   encoding4 is admitted; no old reader/migration. Decode owns independent graphs
   and keeps all outputs unchanged on failure. No source activation or data IO. */
RangeEventStatus range_event_measure(RangeEventView view,size_t validation_reserved,RangeEventInfo *info);
RangeEventStatus range_event_encode(RangeEventView view,size_t validation_reserved,void *arena,size_t capacity,size_t *written);
/* Inspect all framing/owned requirements before copying or decoding any graph.
   Full semantic Format validation follows decode. Failure preserves outputs.
   Decoded width/format graphs never alias input or owned packet bytes. Encoding
   rejects output overlap with borrowed inputs. */
RangeEventStatus range_event_preflight(const void *data,size_t bytes,RangeEventInfo *info);
RangeEventStatus range_event_decode(const void *data,size_t bytes,size_t reserved,RangeEvent *out);
void range_event_free(RangeEvent *event);
#endif
