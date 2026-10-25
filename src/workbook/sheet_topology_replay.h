#ifndef DUXCEL_SHEET_TOPOLOGY_REPLAY_H
#define DUXCEL_SHEET_TOPOLOGY_REPLAY_H
#include "sheet_topology_event.h"
#include "lifecycle.h"
typedef struct {
    Workbook *book;
    const uint64_t *epoch;
    const Analysis *primary; /* Actual primary of the selected live/detached sheet. */
    uint64_t primary_sheet;
    size_t reserved,adoption_reserved;
    bool ready,read_only;
} SheetTopologyReplayContext;
typedef struct SheetTopologyReplayPlan SheetTopologyReplayPlan;
typedef struct { size_t owned,staging; } SheetTopologyReplayMeasure;
typedef enum { SHEET_REPLAY_READY,SHEET_REPLAY_INVALID,SHEET_REPLAY_STALE,SHEET_REPLAY_PROTECTED,SHEET_REPLAY_LIMIT,SHEET_REPLAY_APPLIED } SheetTopologyReplayStatus;
/* Strict genuine SESSION replay, not retained traversal or another authored
   action. The caller associates the original current DXTP with its nonzero
   session lifecycle order and direction. ADD covers actual CREATE/DUPLICATE;
   REMOVE covers DELETE. No reversed ADD issues an old ID; no stream event,
   action ID, source activation, App or file IO is fabricated.
   Preparation owns decoded catalogs/presence and exact full DXSP model; grant
   NEW staging before decoding. Prior output/input/live graph remain separate.
   Match current catalog independently of active navigation; allocator bounds
   may only advance. Exact authored graph, column labels and committed widths
   must match the original presence. Valid current selection/pan/zoom never
   block replay and are preserved through the transfer. Existing captured-active
   lifecycle fallback/reveal semantics remain unchanged. A lagging cache requires
   its owner's typed handoff first.
   A selected current session packet is fenced by original data/size ownership
   and exact canonical content before transfer; no extra resident wire copy is
   retained. Packetless current session events keep the explicit caller packet
   association contract and do not imply a historical file-format reader.
   No session journals are copied: real lifecycle transfers move those owners. */
SheetTopologyReplayStatus sheet_topology_replay_measure(const SheetTopologyPacket *packet,SheetTopologyReplayMeasure *out);
SheetTopologyReplayStatus sheet_topology_replay_prepare(SheetTopologyReplayContext context,const SheetTopologyPacket *packet,bool redo,uint64_t order,SheetTopologyReplayPlan **out,char **error);
SheetTopologyReplayStatus sheet_topology_replay_check(SheetTopologyReplayContext context,const SheetTopologyReplayPlan *plan);
/* Recheck exact selection/owners/runtime and the actual allocation grant before
   genuine workbook_history_step. Failure preserves book/plan. Success consumes
   plan after allocation-free after-state proof; impossible internal invariant
   aborts in release too. No publication API is invoked. */
SheetTopologyReplayStatus sheet_topology_replay_apply(SheetTopologyReplayContext context,SheetTopologyReplayPlan **plan,char **error);
void sheet_topology_replay_free(SheetTopologyReplayPlan **plan);
#endif
