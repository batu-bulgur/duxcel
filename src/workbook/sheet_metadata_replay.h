#ifndef DUXCEL_SHEET_METADATA_REPLAY_H
#define DUXCEL_SHEET_METADATA_REPLAY_H
#include "sheet_history_payload.h"
typedef struct {
    Workbook *book;
    const uint64_t *epoch;
    size_t reserved;
    bool ready,read_only;
} SheetMetadataReplayContext;
typedef struct SheetMetadataReplayPlan SheetMetadataReplayPlan;
typedef struct { size_t owned,staging; } SheetMetadataReplayMeasure;
typedef enum { SHEET_METADATA_REPLAY_READY,SHEET_METADATA_REPLAY_INVALID,SHEET_METADATA_REPLAY_STALE,SHEET_METADATA_REPLAY_PROTECTED,SHEET_METADATA_REPLAY_LIMIT,SHEET_METADATA_REPLAY_UNSUPPORTED,SHEET_METADATA_REPLAY_APPLIED } SheetMetadataReplayStatus;
/* Genuine selected SESSION metadata action, original model-owned DXSL packet.
   No invented stream frame/action, retained traversal, App or source activation.
   Reuse current dispatch effect validation and metadata before/after admission.
   Plain packetless session events explicitly return UNSUPPORTED; no default
   packet is manufactured. Reserve complete new decoded plan before allocation;
   old output/live/ring owners remain separate charges. Failure preserves owners.
   Navigation is not an authored effect: preserve current active whenever visible,
   otherwise use the actual lifecycle fallback, then the first visible sheet.
   Source graphs, cells, formats, widths, stored viewport and all journals stay
   owned by the live sheets. Metadata replay does not clone or rewrite them. */
SheetMetadataReplayStatus sheet_metadata_replay_measure(SheetMetadataReplayContext context,bool redo,SheetMetadataReplayMeasure *out);
SheetMetadataReplayStatus sheet_metadata_replay_prepare(SheetMetadataReplayContext context,bool redo,SheetMetadataReplayPlan **out);
SheetMetadataReplayStatus sheet_metadata_replay_check(SheetMetadataReplayContext context,const SheetMetadataReplayPlan *plan);
/* After prepare, metadata history transfer allocates nothing: recheck actual
   owners/selection/catalog/runtime, call genuine workbook_history_step and prove
   exact opposite metadata/packet/chronology before consuming plan. Unexpected
   internal post-transfer invariants abort in release too; input/budget/stale
   rejection never does. No stream publication or durable clock is advanced. */
SheetMetadataReplayStatus sheet_metadata_replay_apply(SheetMetadataReplayContext context,SheetMetadataReplayPlan **plan,char **error);
void sheet_metadata_replay_free(SheetMetadataReplayPlan **plan);
#endif
