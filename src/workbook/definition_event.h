#ifndef DUXCEL_DEFINITION_EVENT_H
#define DUXCEL_DEFINITION_EVENT_H
#include "definition_dependencies.h"
#include "definition_state.h"
#include "format_dependencies.h"
#define DX_DEFINITION_EVENT_ENCODING 1u
#define DX_DEFINITION_EVENT_HEADER_BYTES 64u
#define DX_DEFINITION_EVENT_MAX_BYTES (DX_DEFINITION_EVENT_HEADER_BYTES+2u*DX_OBJECTS_ASSET_MAX_BYTES+DX_FORMAT_DELTA_MAX_BYTES+DX_SOURCE_VERSIONS_MAX_BYTES)
typedef struct {
    uint64_t sheet,object_high_water;
    unsigned effects; /* 1 Analysis,4 Objects,9 Analysis+Format,12 Objects+Format. */
    DefinitionDependencyView before,after;
    FormatSnapshotView before_formats,after_formats;
    const SourceVersions *sources; /* Same immutable candidate ledger throughout. */
} DefinitionEventView;
typedef struct {
    uint64_t sheet,object_high_water,source_high_water;
    unsigned effects;
    size_t bytes,owned,validation_scratch,inactive_scopes,unbound_sources;
} DefinitionEventInfo;
typedef struct { void *data; size_t bytes; } DefinitionEventPacket;
typedef struct {
    uint64_t sheet,object_high_water;
    unsigned effects;
    Analysis before_primary,after_primary;
    ObjectsAsset before_objects,after_objects;
    FormatDelta formats;
    SourceVersionsAsset sources;
    size_t inactive_scopes,unbound_sources;
} DefinitionEvent;
typedef enum { DEFINITION_EVENT_OK,DEFINITION_EVENT_NOOP,DEFINITION_EVENT_INVALID,DEFINITION_EVENT_LIMIT,DEFINITION_EVENT_UNSUPPORTED } DefinitionEventStatus;
/* Closed compound definition payload, not a .dxls file profile or operation ID.
   DXDE/u16 encoding/u16 zero/u64 sheet/u64 object high-water/u32 effects,
   six u32 lengths(before/after Analysis, before/after Objects, format delta,
   source metadata),u32 inactive-scope/unbound observations/u32 zero, then parts.
   Only the declared complete family pairs are present. Each declared family
   must have an authored change; all-no-op input creates no event. Nested sheet/
   owner/allocator associations and the EXACT union of source dependencies close
   together, without dataset bytes or source execution. Counter maxima remain
   outside Undo; runtime rebinding/baseline completion stay with the App owner. */
/* Bounded semantic preparation, not allocation-free: validation_reserved admits
   grammar/uniqueness/scope-marker scratch BEFORE validation can allocate. Borrowed
   inputs stay immutable. owned includes the complete decoded wrapper/graphs;
   source and object child encoders avoid intermediate full-asset/ledger clones.
   Failure/NOOP preserves info. Full source activation and model fences are separate. */
DefinitionEventStatus definition_event_measure(DefinitionEventView view,size_t validation_reserved,DefinitionEventInfo *info);
/* reserved covers new final packet bytes plus validation scratch. Existing input
   graphs/old packet are separately owned. Failure/NOOP preserves packet; success
   replaces only after complete encoding. No IO/model/action/clock publication. */
DefinitionEventStatus definition_event_capture(DefinitionEventView view,size_t reserved,DefinitionEventPacket *packet);
void definition_event_packet_free(DefinitionEventPacket *packet);
/* Allocation-free inspection of ALL child framing/associations/owned budgets,
   not full semantic/source-closure admission. Decode performs every full check
   before replacing an independent graph. Input remains immutable throughout. */
DefinitionEventStatus definition_event_preflight(const void *data,size_t bytes,DefinitionEventInfo *info);
DefinitionEventStatus definition_event_decode(const void *data,size_t bytes,size_t reserved,DefinitionEvent *out);
void definition_event_free(DefinitionEvent *event);
#endif
