#ifndef DUXCEL_OBJECT_SOURCE_PLAN_H
#define DUXCEL_OBJECT_SOURCE_PLAN_H
#include "objects_asset.h"
typedef enum { OBJECT_SOURCE_PLAN_OK,OBJECT_SOURCE_PLAN_NOOP,OBJECT_SOURCE_PLAN_INVALID,
    OBJECT_SOURCE_PLAN_PROTECTED,OBJECT_SOURCE_PLAN_LIMIT } ObjectSourcePlanStatus;
typedef struct {
    size_t owned,validation_scratch;
    uint64_t owner,before_revision,after_revision;
    bool result_changed;
} ObjectSourcePlanInfo;
/* Same-connection Table transforms only, addressed by durable object ID. Exact
   kind/path/schema/table/source_version must survive; acquisition/rebinding and
   default-relation completion have separate owners. Trusted live store/input.
   Allocation-free canonical shape/UTF-8/geometry/query/sort measurement, not full
   grammar/source-ledger/history ownership admission. Authored equality excludes
   derived type caches. Filter display reordering preserves the query epoch.
   Failure/NOOP preserves info. Allocator high-water cannot be changed. */
ObjectSourcePlanStatus objects_source_transform_measure(const ObjectStore *store,uint64_t owner,
    const Analysis *source,ObjectSourcePlanInfo *info);
/* reserved covers the new independent graph plus validation scratch; existing
   live/history/source/out graphs and bounded stack metadata are separate. Admit
   the complete projection before cloning; no history graph is copied or pruned.
   out is zero or independently owned and must not alias any input/history.
   Failure/NOOP preserves out/info. Success replaces out, without model/action/
   dirty/clock/IO changes. The command owner must recheck runtime fences, prove
   dependency/geometry/composite closure and reserve an event BEFORE adoption. */
ObjectSourcePlanStatus objects_plan_source_transform(const ObjectStore *store,uint64_t owner,
    const Analysis *source,size_t reserved,ObjectSnapshot *out,ObjectSourcePlanInfo *info);
/* Explicit connection/binding replacement shares complete graph planning, but
   permits changed kind/path/schema/table/source_version. Both DATABASE ends
   must be resolved: worker default completion is not a user replacement. A
   version change invalidates results even with identical query text. Ledger,
   format coexistence and source ownership remain command/composite admission. */
ObjectSourcePlanStatus objects_source_replacement_measure(const ObjectStore *store,uint64_t owner,
    const Analysis *source,ObjectSourcePlanInfo *info);
ObjectSourcePlanStatus objects_plan_source_replacement(const ObjectStore *store,uint64_t owner,
    const Analysis *source,size_t reserved,ObjectSnapshot *out,ObjectSourcePlanInfo *info);
#endif
