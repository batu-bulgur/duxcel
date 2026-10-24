#ifndef DUXCEL_OBJECT_NAME_COMMAND_H
#define DUXCEL_OBJECT_NAME_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef struct { CellEditTarget owner; uint64_t object; char before[DX_OBJECT_NAME_BYTES+1]; } ObjectNameTarget;
typedef struct { size_t owned,validation_scratch; } ObjectNamePlanInfo;
typedef enum { OBJECT_NAME_APPLIED,OBJECT_NAME_NOOP,OBJECT_NAME_READY,OBJECT_NAME_STALE,
    OBJECT_NAME_INVALID,OBJECT_NAME_PROTECTED,OBJECT_NAME_LIMIT } ObjectNameStatus;
typedef struct { ObjectNameStatus status; uint64_t object,before_revision,after_revision; } ObjectNameResult;
/* Rename drafts bind document/sheet sessions, document/cell clocks, object ID
   and exact prior name, independently from background query/layout completion.
   Output is unchanged on failure. No-op preserves Redo and title/query clocks. */
bool workbook_prepare_object_name(ObjectCommandContext *context,size_t sheet,uint64_t object,ObjectNameTarget *out,char **error);
/* Allocation-free canonical projection/normalization/budget inspection. NOOP or
   failure preserves info; grammar and full source closure are admitted later. */
ObjectNameStatus objects_name_plan_measure(const ObjectStore *store,uint64_t object,const char *name,ObjectNamePlanInfo *info,char **error);
/* Caller retains text ownership. A fresh full object fence is captured after
   draft validation, then one owned after-graph is prepared. Reserve DXDE before
   eviction, recheck both draft/full fences, move once and publish one action.
   plan_reserved covers after-graph+parser scratch; event validation_reserved and
   old/live/global staging are separate. Title counter never wraps; query/Marks/
   allocator/source bindings remain unchanged. App retains focus/cache ownership.
   No IO, durable receipt, retained App activation or background-job cancellation. */
ObjectNameResult workbook_apply_object_name_recorded(ObjectCommandContext *context,const ObjectNameTarget *target,
    const char *name,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
