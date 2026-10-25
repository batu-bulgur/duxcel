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
    PRIMARY_TOPOLOGY_PROTECTED,PRIMARY_TOPOLOGY_LIMIT,PRIMARY_TOPOLOGY_BUSY } PrimarySourceTopologyStatus;
/* Prepare-only CREATE bridge. Source plan must still own its actual resolved
   probe lease. ADD uses exact ordered current before catalog, appends precisely
   the proposed next ID/high-water and uses the source plan's independent DXSP.
   No new sheet runtime session, ID/action assignment, CREATE, eviction or IO.
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
   IMPORTANT: the current source plan has an empty pre-UI SheetView. That is NOT
   the post-open authored 128/190 source column initialization. A future producer
   must own the actual probe's initial names/widths and adopt them atomically,
   then prove the real model/history/revision before topology publication.
   No publication/adoption API exists here; this blocker cannot be waived with
   a caller flag, metadata loss, a fake session or a new archive encoding. */
PrimarySourceTopologyStatus primary_source_topology_check(PrimarySourceDraftContext context,
    const PrimarySourceTopologyPlan *plan,char **error);
/* Reserve immutable ADD before actual CREATE; recheck source/catalog/stream.
   A wrong owner, pressure, stale lease or duplicate reserve preserves all
   models and clocks. Stream outlives ticket; cancellation restores its state. */
PrimarySourceTopologyStatus primary_source_topology_reserve(PrimarySourceDraftContext context,
    PrimarySourceTopologyPlan *plan,WorkbookHistoryStream *stream,size_t reserved,char **error);
void primary_source_topology_cancel(PrimarySourceTopologyPlan *plan);
void primary_source_topology_free(PrimarySourceTopologyPlan **plan);
const SheetTopologyPacket *primary_source_topology_packet(const PrimarySourceTopologyPlan *plan);
const SheetPresenceAsset *primary_source_topology_presence(const PrimarySourceTopologyPlan *plan);
PrimarySourceTopologyUsage primary_source_topology_usage(const PrimarySourceTopologyPlan *plan);
bool primary_source_topology_reserved(const PrimarySourceTopologyPlan *plan);
#endif
