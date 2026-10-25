#ifndef DUXCEL_SHEET_PRESENCE_ASSET_H
#define DUXCEL_SHEET_PRESENCE_ASSET_H
#include "workbook.h"
#define DX_SHEET_PRESENCE_OWNED_BYTES (32u*1024u*1024u)
typedef struct {
    WorkbookId document;
    uint64_t sheet_high_water;
    const Worksheet *sheet;
    const Analysis *primary; /* Required actual primary; never inferred from Save cache. */
} SheetPresenceView;
typedef struct { size_t owned,validation_scratch,staging; } SheetPresenceMeasure;
typedef struct SheetPresenceAsset SheetPresenceAsset;
typedef enum { SHEET_PRESENCE_OK,SHEET_PRESENCE_INVALID,SHEET_PRESENCE_LIMIT } SheetPresenceStatus;
/* Current authored single-sheet preimage, not a workbook archive/wire format.
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
SheetPresenceStatus sheet_presence_capture(SheetPresenceView view,size_t reserved,SheetPresenceAsset **out,char **error);
SheetPresenceView sheet_presence_view(const SheetPresenceAsset *asset);
void sheet_presence_free(SheetPresenceAsset **asset);
#endif
