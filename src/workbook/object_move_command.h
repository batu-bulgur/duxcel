#ifndef DUXCEL_OBJECT_MOVE_COMMAND_H
#define DUXCEL_OBJECT_MOVE_COMMAND_H
#include "object_layout_command.h"
/* Outer native governor checks actual prospective retained formats plus the
   incoming Objects+Format history charge. Borrowed views expire on return;
   callback cannot reenter or mutate owned graphs. False rejects before eviction. */
typedef bool (*ObjectMoveAdmission)(void *context,const FormatStore *prospective,size_t adopted,size_t scratch,size_t retained,char **error);
typedef struct { ObjectLayoutContext layout; ObjectMoveAdmission admit; void *admission_context; } ObjectMoveContext;
typedef struct {
    ObjectLayoutTarget layout;
    FormatStore formats;
    FormatPatch patches[DX_FORMAT_PATCHES];
    FormatScope scopes[DX_FORMAT_PATCHES*DX_FORMAT_GUARDS];
} ObjectMoveTarget;
typedef struct { size_t owned,validation_scratch; } ObjectMovePlanInfo;
typedef enum { OBJECT_MOVE_APPLIED,OBJECT_MOVE_NOOP,OBJECT_MOVE_READY,OBJECT_MOVE_STALE,
    OBJECT_MOVE_INVALID,OBJECT_MOVE_PROTECTED,OBJECT_MOVE_LIMIT } ObjectMoveStatus;
typedef struct { ObjectMoveStatus status; uint64_t object,before_revision,after_revision; } ObjectMoveResult;
bool workbook_prepare_object_move(ObjectMoveContext *context,size_t sheet,uint64_t object,ObjectMoveTarget *out,char **error);
/* Allocation-free graph/destination and coarse format shape inspection. owned
   reserves graph plus a bounded format-planner peak, not exact allocator/RSS.
   Scratch separately covers object grammar and scope/dependency validation.
   No-op/failure preserves info. Manual cells under the object are protected;
   mixed/manual ranges belong to the range command owner. */
ObjectMoveStatus objects_move_measure(const ObjectStore *store,uint64_t object,uint64_t row,size_t column,const CellStore *cells,
    uint64_t source_rows,size_t source_columns,const FormatStore *formats,ObjectMovePlanInfo *info,char **error);
/* One full graph, optional positional format delta, one history action and
   complete DXDE reservation before either history can evict. Recheck all graph,
   format/owner/source-extent fences after admission and event reservation.
   Preserve ID/query/Marks/title/allocator and no-op Redo. App owns focus/jobs/
   result readiness and cache lifetime. No retained App event IO here. */
ObjectMoveResult workbook_apply_object_move_recorded(ObjectMoveContext *context,const ObjectMoveTarget *target,uint64_t row,size_t column,
    const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
