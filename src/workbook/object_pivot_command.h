#ifndef DUXCEL_OBJECT_PIVOT_COMMAND_H
#define DUXCEL_OBJECT_PIVOT_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef struct { size_t owned,validation_scratch,proposal_owned; } ObjectPivotPlanInfo;
typedef enum { OBJECT_PIVOT_APPLIED,OBJECT_PIVOT_NOOP,OBJECT_PIVOT_READY,OBJECT_PIVOT_STALE,
    OBJECT_PIVOT_INVALID,OBJECT_PIVOT_PROTECTED,OBJECT_PIVOT_LIMIT } ObjectPivotStatus;
typedef struct { ObjectPivotStatus status; uint64_t object,before_revision,after_revision; } ObjectPivotResult;
/* Complete allocation-free projection; proposal and runtime NumberType remain
   borrowed. Authored no-op ignores derived types, preserving Redo/query clocks.
   info unchanged on failure/no-op; caller staging is measured separately. */
ObjectPivotStatus objects_pivot_measure(const ObjectStore *store,uint64_t object,const PivotDefinition *proposal,ObjectPivotPlanInfo *info,char **error);
/* Full object fence and bounded independent after-graph. Canonical grammar and
   source closure precede optional complete DXDE reserve; recheck before eviction.
   Move one graph/action and increment document/query revisions once without wrap.
   Preserve selected measure identity/slot, Marks/name/source/layout/allocator.
   plan_reserved covers graph+max parser scratch. App owns schema availability,
   caller proposal, cache invalidation, focus/jobs and native global admission.
   No IO, retained App writer or durable Undo activation here. */
ObjectPivotResult workbook_apply_object_pivot_recorded(ObjectCommandContext *context,const ObjectCommandTarget *target,
    const PivotDefinition *proposal,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
