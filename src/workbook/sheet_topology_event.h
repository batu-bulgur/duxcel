#ifndef DUXCEL_SHEET_TOPOLOGY_EVENT_H
#define DUXCEL_SHEET_TOPOLOGY_EVENT_H
#include "sheet_lifecycle.h"
#include "sheet_presence_asset.h"
#define DX_SHEET_TOPOLOGY_ENCODING 1u
#define DX_SHEET_TOPOLOGY_HEADER_BYTES 80u
#define DX_SHEET_TOPOLOGY_MAX_BYTES (DX_SHEET_PRESENCE_WIRE_BYTES+2u*DX_MAX_SHEETS*(16u+DX_SHEET_LIFECYCLE_NAME_BYTES)+DX_SHEET_TOPOLOGY_HEADER_BYTES)
typedef enum { SHEET_TOPOLOGY_ADD=1,SHEET_TOPOLOGY_REMOVE=2 } SheetTopologyOperation;
typedef struct {
    SheetTopologyOperation operation;
    uint64_t sheet;
    SheetLifecycleState before,after;
    const SheetPresencePacket *presence;
} SheetTopologyView;
typedef struct { void *data; size_t bytes; } SheetTopologyPacket;
typedef struct {
    WorkbookId document;
    SheetTopologyOperation operation;
    uint64_t sheet;
    size_t bytes,owned,validation_scratch,before_count,after_count,presence_bytes;
} SheetTopologyInfo;
typedef struct {
    SheetTopologyOperation operation;
    uint64_t sheet;
    SheetLifecycleState before,after;
    SheetPresencePacket presence;
} SheetTopologyEvent;
typedef enum { SHEET_TOPOLOGY_OK,SHEET_TOPOLOGY_INVALID,SHEET_TOPOLOGY_LIMIT,SHEET_TOPOLOGY_UNSUPPORTED,SHEET_TOPOLOGY_STALE } SheetTopologyStatus;
/* Independent one-ID topology component, not the same-ID DXSL metadata format.
   Preserve every retained ID/name/hidden/order exactly. ADD issues precisely the
   next sheet ID/high-water; REMOVE preserves its allocator bound. Each catalog
   has a visible active fallback and at least one visible sheet. The one DXSP
   belongs to the added/deleted side, exact document/ID/high-water/name/hidden.
   Duplicate uses ADD; proof of actual copying belongs to the command owner.
   No App, lifecycle adoption, worksheet action, source activation or stream IO.
   Catalog views, input bytes and presence packet stay immutable during calls. */
/* Measure/preflight prove framing/catalog/identity association without allocation.
   Full DXSP grammar/source closure/name association follows admitted decode.
   owned is decoded arrays/names+independent DXSP bytes. validation_scratch also
   includes one temporary complete DXSP owner; old inputs/out are separate charges.
   Capture reservation covers new packet+validation_scratch; decode reservation
   covers owned+validation_scratch. Failure preserves all outputs. */
SheetTopologyStatus sheet_topology_measure(SheetTopologyView view,SheetTopologyInfo *out);
SheetTopologyStatus sheet_topology_capture(SheetTopologyView view,size_t reserved,SheetTopologyPacket *out);
SheetTopologyStatus sheet_topology_preflight(const void *data,size_t bytes,SheetTopologyInfo *out);
/* Full immutable component proof without copying the event graphs; reserved
   covers validation_scratch from preflight. Useful before stream reservation. */
SheetTopologyStatus sheet_topology_validate(const void *data,size_t bytes,size_t reserved,SheetTopologyInfo *out);
SheetTopologyStatus sheet_topology_decode(const void *data,size_t bytes,size_t reserved,SheetTopologyEvent *out);
SheetTopologyView sheet_topology_view(const SheetTopologyEvent *event);
void sheet_topology_packet_free(SheetTopologyPacket *packet);
void sheet_topology_free(SheetTopologyEvent *event);
/* Pure metadata projection for an already validated event. Match authored
   catalogs independently of navigation; retain live active ID if still visible,
   otherwise use the captured visible fallback. Never rewind live high-water.
   Result borrows the opposite catalog; it is not model/presence adoption. */
SheetTopologyStatus sheet_topology_admit(SheetTopologyView view,SheetLifecycleState current,bool redo,SheetLifecycleState *target);
#endif
