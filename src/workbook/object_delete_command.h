#ifndef DUXCEL_OBJECT_DELETE_COMMAND_H
#define DUXCEL_OBJECT_DELETE_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef struct { size_t owned,validation_scratch; } ObjectDeletePlanInfo;
typedef enum { OBJECT_DELETE_APPLIED,OBJECT_DELETE_READY,OBJECT_DELETE_STALE,
    OBJECT_DELETE_INVALID,OBJECT_DELETE_PROTECTED,OBJECT_DELETE_LIMIT } ObjectDeleteStatus;
typedef struct { ObjectDeleteStatus status; uint64_t object,before_revision,after_revision; } ObjectDeleteResult;
/* Allocation-free complete graph projection. A Table with dependents is
   protected. Keep the allocator high-water and source ledger, even for the last
   object. Failure leaves info unchanged. No result-readiness requirement. */
ObjectDeleteStatus objects_delete_measure(const ObjectStore *store,uint64_t object,ObjectDeletePlanInfo *info,char **error);
/* Use the shared full object fence. Admit complete before/after grammar and
   source dependencies, reserve an optional event before eviction, then recheck
   and transfer one independent graph/action. Empty after-graphs are supported.
   App owns result retirement, focus and jobs; no IO or retained App writer. */
ObjectDeleteResult workbook_apply_object_delete_recorded(ObjectCommandContext *context,const ObjectCommandTarget *target,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
