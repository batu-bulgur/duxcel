#ifndef DUXCEL_SHEET_TOPOLOGY_DUPLICATE_H
#define DUXCEL_SHEET_TOPOLOGY_DUPLICATE_H
#include "history_sheet_topology.h"
typedef struct {
    Workbook *book;
    const uint64_t *epoch;
    const Analysis *primary; /* Required actual primary of primary_sheet. */
    uint64_t primary_sheet;
    size_t reserved;
    bool ready,read_only;
    size_t adoption_reserved;
} SheetTopologyDuplicateContext;
typedef struct SheetTopologyDuplicatePlan SheetTopologyDuplicatePlan;
typedef struct { size_t owned,staging,reservation,packet_bytes; } SheetTopologyDuplicateMeasure;
typedef enum { SHEET_DUPLICATE_READY,SHEET_DUPLICATE_INVALID,SHEET_DUPLICATE_STALE,SHEET_DUPLICATE_PROTECTED,SHEET_DUPLICATE_LIMIT,SHEET_DUPLICATE_BUSY,SHEET_DUPLICATE_APPLIED } SheetTopologyDuplicateStatus;
/* Independent prospective copy, using the existing current DXSP capture. Exactly
   the next stable ID, normalized unique name, visible state and insertion after
   the source match the real producer. Authored graph, full source ledger/counters
   and stored view copy exactly; session journals and runtime identities do not.
   Explicit actual primary can differ from the source cache during preparation.
   Genuine existing document/source sessions are required; no action or session
   is manufactured. Bounded measures are conservative: maximum new name and its
   normalization scratch are admitted BEFORE naming/capture allocations. Old
   plans, live graphs, stream residents and real adoption remain outer charges.
   All failures preserve the book and old output (prepare requires empty output).
   No App, source activation, retained replay, archive/IO or actual adoption. */
SheetTopologyDuplicateStatus sheet_topology_duplicate_measure(SheetTopologyDuplicateContext context,uint64_t source,SheetTopologyDuplicateMeasure *out);
SheetTopologyDuplicateStatus sheet_topology_duplicate_prepare(SheetTopologyDuplicateContext context,uint64_t source,SheetTopologyDuplicatePlan **out,char **error);
/* Allocation-free exact graph/catalog/runtime check; navigation and Worksheet
   relocation are independent. New owner borrows remain valid until free. */
SheetTopologyDuplicateStatus sheet_topology_duplicate_check(SheetTopologyDuplicateContext context,const SheetTopologyDuplicatePlan *plan);
SheetTopologyDuplicateStatus sheet_topology_duplicate_reserve(SheetTopologyDuplicateContext context,SheetTopologyDuplicatePlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error);
/* Real model-only duplicate. Requires the current cached primary to match the
   independently captured actual primary; it never repairs a lagging cache.
   Admit the shared real producer staging BEFORE optional event reservation and
   lifecycle eviction. Navigation refresh affects unpublished fallback only and
   requires the full preparation grant. Genuine SHEET_DUPLICATE issues one new
   ID/action, then allocation-free actual graph/catalog/session/clock proof
   precedes internal publication. Success consumes plan; failure preserves the
   book and plan and cancels its ticket. An impossible closed-producer invariant
   aborts in release too. No App, retained replay or IO activation. */
SheetTopologyDuplicateStatus sheet_topology_duplicate_apply(SheetTopologyDuplicateContext context,SheetTopologyDuplicatePlan **plan,WorkbookHistoryStream *stream,size_t reserved,char **error);
void sheet_topology_duplicate_cancel(SheetTopologyDuplicatePlan *plan);
void sheet_topology_duplicate_free(SheetTopologyDuplicatePlan **plan);
const SheetTopologyPacket *sheet_topology_duplicate_packet(const SheetTopologyDuplicatePlan *plan);
SheetPresenceView sheet_topology_duplicate_presence(const SheetTopologyDuplicatePlan *plan);
SheetTopologyDuplicateMeasure sheet_topology_duplicate_usage(const SheetTopologyDuplicatePlan *plan);
bool sheet_topology_duplicate_reserved(const SheetTopologyDuplicatePlan *plan);
#endif
