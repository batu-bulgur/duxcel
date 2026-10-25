#ifndef DUXCEL_SHEET_PRESENCE_ASSET_H
#define DUXCEL_SHEET_PRESENCE_ASSET_H
#include "workbook.h"
#define DX_SHEET_PRESENCE_ENCODING 1u
#define DX_SHEET_PRESENCE_HEADER_BYTES 136u
#define DX_SHEET_PRESENCE_WIRE_BYTES (33u*1024u*1024u)
#define DX_SHEET_PRESENCE_OWNED_BYTES (32u*1024u*1024u)
typedef struct {
    WorkbookId document;
    uint64_t sheet_high_water;
    const Worksheet *sheet;
    const Analysis *primary; /* Required actual primary; never inferred from Save cache. */
} SheetPresenceView;
typedef struct { size_t owned,validation_scratch,staging; } SheetPresenceMeasure;
typedef struct SheetPresenceAsset SheetPresenceAsset;
typedef enum { SHEET_PRESENCE_OK,SHEET_PRESENCE_INVALID,SHEET_PRESENCE_LIMIT,SHEET_PRESENCE_UNSUPPORTED } SheetPresenceStatus;
/* Current authored single-sheet preimage; no workbook file adapter.
   Preserve stable IDs and allocator obligations, name/hidden, actual Analysis,
   ordered objects, sparse manual cells, full formats/selectors/ledger and stored
   SheetView preferences/names/exact binary32 physical widths (implicit tail96).
   CellStore represents empty manual text as absence; literal NULL is text. SQL
   NULL/empty result cells belong to Pages and are never manufactured here.
   No Pages, datasets, results, sessions, Undo/Redo, width journal, query jobs or
   source activation. External locators and pending/unbound dependencies remain
   declared metadata. Counters are obligations, never live replay assignments.
   Existing snapshot copy semantics retain derived metadata; no admission or
   durable user action is implied. The source graph stays immutable throughout. */
/* Allocation-free bounded model/ownership measure, not full semantic validation.
   owned covers the NEW exact-count snapshot, capped at 32 MiB; validation_scratch bounds existing
   grammar/scope checks. staging includes both. All failures preserve out. */
SheetPresenceStatus sheet_presence_measure(SheetPresenceView view,SheetPresenceMeasure *out);
/* Reserve complete staging BEFORE validation scratch or any clone. Full current
   component semantics/source closure precede copying. Success atomically replaces
   an independent previous asset; rejection leaves every input/old output owned.
   Allocation exhaustion follows the existing process-fatal allocator policy. */
/* Allocation-free exact authored/preference comparison against an owned asset.
   Actual primary is explicit; store clocks, capacity and inference caches are
   excluded. Structural invalidity preserves matches; valid difference returns
   OK/false. Inputs and the independent scalar output must be disjoint. */
SheetPresenceStatus sheet_presence_matches(const SheetPresenceAsset *asset,SheetPresenceView current,bool *matches);
/* Authored replay comparison: validate current navigation, then exclude only
   row/column selection, pan/fraction and zoom scalars. Physical column names and
   exact committed widths remain authored and compare strictly. A trailing run
   of empty labels/default96 widths is implicit virtual capacity, so it may
   grow/shrink without an authored difference. Strict DXSP still retains it.
   No allocation;
   malformed view preserves matches. Strict stored-preference API stays above. */
SheetPresenceStatus sheet_presence_matches_authored(const SheetPresenceAsset *asset,SheetPresenceView current,bool *matches);
SheetPresenceStatus sheet_presence_capture(SheetPresenceView view,size_t reserved,SheetPresenceAsset **out,char **error);
/* Borrowed immutable graph; the asset outlives every reader. */
SheetPresenceView sheet_presence_view(const SheetPresenceAsset *asset);
void sheet_presence_free(SheetPresenceAsset **asset);
typedef struct { void *data; size_t bytes; } SheetPresencePacket;
typedef struct {
    WorkbookId document;
    uint64_t sheet,sheet_high_water,object_high_water,source_high_water;
    size_t bytes,owned,validation_scratch,cells,columns;
} SheetPresenceWireInfo;
/* Current DXSP retained-sheet component, not a .dxls archive. Encoding1 binds
   document/sheet/counter metadata and exact stored view scalar bits, then the
   name, ordered column name/width rows, sorted sparse current cell rows, and
   existing DXAN/DXOB/DXFD-snapshot-body/DXSV components. Cells are complete
   current values, not fabricated user edits; empty manual values stay absent.
   No native layouts, query/NumberType caches, sessions, history or datasets.
   Numeric schema metadata is re-established by future activation, not decoded.
   Unknown encodings, reserved bits, partial components and trailing bytes reject.
   Input bytes remain immutable throughout preflight/decode.
   Capture reserve covers NEW packet+validation scratch; decode reserve covers
   NEW complete model+validation scratch. Input/previous output remain separate
   coexistence charges. Cancellation is freeing the unpublished owned result. */
/* Exact packet/owned bounds from a borrowed structural view, before capture.
   This does not replace full capture/decoded source closure validation. */
SheetPresenceStatus sheet_presence_wire_view_measure(SheetPresenceView view,SheetPresenceWireInfo *out);
SheetPresenceStatus sheet_presence_wire_measure(const SheetPresenceAsset *asset,SheetPresenceWireInfo *out);
SheetPresenceStatus sheet_presence_encode(const SheetPresenceAsset *asset,size_t reserved,SheetPresencePacket *out);
SheetPresenceStatus sheet_presence_preflight(const void *data,size_t bytes,SheetPresenceWireInfo *out);
SheetPresenceStatus sheet_presence_decode(const void *data,size_t bytes,size_t reserved,SheetPresenceAsset **out,char **error);
void sheet_presence_packet_free(SheetPresencePacket *packet);
#endif
