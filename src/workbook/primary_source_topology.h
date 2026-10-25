#ifndef DUXCEL_PRIMARY_SOURCE_TOPOLOGY_H
#define DUXCEL_PRIMARY_SOURCE_TOPOLOGY_H
#include "primary_source_publish.h"
#include "history_sheet_topology.h"
typedef struct PrimarySourceTopologyPlan PrimarySourceTopologyPlan;
typedef struct {
    size_t source,owned,preparation,reservation,total,event_bytes;
    uint64_t prospective_sheet;
} PrimarySourceTopologyUsage;
typedef enum { PRIMARY_TOPOLOGY_READY,PRIMARY_TOPOLOGY_INVALID,PRIMARY_TOPOLOGY_STALE,
    PRIMARY_TOPOLOGY_PROTECTED,PRIMARY_TOPOLOGY_LIMIT,PRIMARY_TOPOLOGY_BUSY,PRIMARY_TOPOLOGY_APPLIED,PRIMARY_TOPOLOGY_APPLIED_PENDING,PRIMARY_TOPOLOGY_FAILED } PrimarySourceTopologyStatus;
/* CREATE topology owner. Source plan must still own its actual resolved
   probe lease. ADD uses exact ordered current before catalog, appends precisely
   the proposed next ID/high-water and uses the source plan's independent DXSP.
   Preparation assigns no new sheet runtime session/ID/action and performs no
   CREATE, eviction or IO.
   Reserve total BEFORE catalog copying, presence/event encoding or validation.
   source is the source producer's captured complete coexistence obligation;
   owned is this bridge/catalog/event; preparation adds transient presence and
   validation; reservation adds the actual stream wrapper/frame/payload/scratch.
   total conservatively sums them. Existing stream residents, App reply/registry,
   worker/retirement and held heads remain outer charges. */
PrimarySourceTopologyStatus primary_source_topology_measure(PrimarySourceDraftContext context,
    const PrimarySourcePublishPlan *source,PrimarySourceTopologyUsage *out,char **error);
/* Success alone takes *source. Failure preserves source/out/book. The resulting
   bridge owns the source lifetime; do not retain a mutable alias and apply it. */
PrimarySourceTopologyStatus primary_source_topology_prepare(PrimarySourceDraftContext context,
    PrimarySourcePublishPlan **source,size_t reserved,PrimarySourceTopologyPlan **out,char **error);
/* READY proves only the still-current PREPARATION lease, not actual adoption.
   Requires the actual-probe initial-view source constructor; provisional empty
   views are PROTECTED. Its complete names/128/190 widths/default96 tail and
   explicit navigation/zoom are owned in the ADD. The source producer adopts
   that same AFTER view atomically with actual CREATE. A future compound owner
   must still prove real model/history/revision before topology publication,
   and native surface adoption must reset/reproject Grid deterministically.
   The closed apply below proves real adoption synchronously; caller flags,
   metadata loss, fake sessions and new archive encodings cannot replace it. */
PrimarySourceTopologyStatus primary_source_topology_check(PrimarySourceDraftContext context,
    const PrimarySourceTopologyPlan *plan,char **error);
/* Reserve immutable ADD before actual CREATE; recheck source/catalog/stream.
   A wrong owner, pressure, stale lease or duplicate reserve preserves all
   models and clocks. Stream outlives ticket; cancellation restores its state. */
PrimarySourceTopologyStatus primary_source_topology_reserve(PrimarySourceDraftContext context,
    PrimarySourceTopologyPlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error);
/* Complete aggregate admission and optional immutable stream reservation precede
   the real source CREATE. After actual catalog/presence/session/revision/action
   proof, move the original DXTP into that genuine lifecycle event without an
   allocation or separate App journal. Only then may the closed private stream
   boundary publish a supplied ticket. NULL stream is session-only; an already
   reserved ticket requires its same stream. Before-publication failure preserves
   model/inputs/output; caller can cancel/free a terminal rejected source plan.
   APPLIED consumes the plan. APPLIED_PENDING still owns the usable created
   model/session event, but cancels the retained ticket and never acknowledges
   origin readiness. No App retained writer is activated by this model API. */
PrimarySourceTopologyStatus primary_source_topology_apply(PrimarySourceDraftContext context,
    PrimarySourceTopologyPlan **plan,WorkbookHistoryStream *stream,size_t reserved,
    PrimarySourcePublication *out,char **error);
void primary_source_topology_cancel(PrimarySourceTopologyPlan *plan);
void primary_source_topology_free(PrimarySourceTopologyPlan **plan);
const SheetTopologyPacket *primary_source_topology_packet(const PrimarySourceTopologyPlan *plan);
const SheetPresenceAsset *primary_source_topology_presence(const PrimarySourceTopologyPlan *plan);
PrimarySourceTopologyUsage primary_source_topology_usage(const PrimarySourceTopologyPlan *plan);
bool primary_source_topology_reserved(const PrimarySourceTopologyPlan *plan);
#endif
