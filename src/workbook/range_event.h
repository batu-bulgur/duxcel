#ifndef DUXCEL_RANGE_EVENT_H
#define DUXCEL_RANGE_EVENT_H
#include "cell_delta_codec.h"
#include "format_delta_codec.h"
#include "column_widths.h"
#include "definition_event.h"
#define DX_RANGE_EVENT_ENCODING 3u
#define DX_RANGE_EVENT_HEADER_BYTES 40u
#define DX_RANGE_EVENT_CELLS 40000u
#define DX_RANGE_EVENT_MAX_BYTES (30u*1024u*1024u)
typedef struct {
    uint64_t sheet;
    unsigned effects; /* Nonzero subset of 2 Cells,4 Objects,8 Format,16 Width; every part changes. */
    const CellDelta *cells;
    size_t count;
    FormatSnapshotView before_formats,after_formats;
    ColumnWidthPair widths;
    DefinitionEventView definitions; /* Objects bit: exact DXDE4/12, source-free. */
} RangeEventView;
typedef struct { uint64_t sheet; unsigned effects; size_t count,bytes,owned,validation_scratch,format_bytes,width_bytes,width_owned,definition_bytes,definition_owned; } RangeEventInfo;
typedef struct {
    uint64_t sheet; unsigned effects;
    void *data; size_t bytes;
    CellDelta *cells; size_t count; /* Spans borrow ONLY this event's owned data. */
    FormatDelta formats; /* Independent owned graphs; no packet/string aliases. */
    DefinitionEvent definitions; /* Owns Objects and optional formats when bit4. */
    ColumnWidthDelta widths; /* Complete independent sparse virtual states. */
} RangeEvent;
typedef enum { RANGE_EVENT_OK,RANGE_EVENT_NOOP,RANGE_EVENT_INVALID,RANGE_EVENT_LIMIT,RANGE_EVENT_UNSUPPORTED } RangeEventStatus;
/* Closed source-free range compound: DXRE/u16 encoding/u16 zero/u64
   sheet/u32 effects/u32 changed-cell count/u32 format bytes/u32 width bytes/
   u32 definition bytes/u32 zero;
   repeated u32 byte length + canonical DXCD, then optional complete canonical
   DXFD, DXCW and DXDE before/after pairs. Objects require DXDE4/12 and
   prohibit a separate DXFD. Tables/source links/Analysis are outside this owner.
   Only current encoding3 is admitted; older encodings are unsupported.
   Sorted unique coordinates, same sheet, changed values and truthful effects
   reject partial/duplicate/no-op constituents. Sparse clear is an empty span;
   exact text bytes are retained. NULL/empty source values are outside this owner.
   Canonical format validation requires caller-admitted bounded scratch first. */
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
