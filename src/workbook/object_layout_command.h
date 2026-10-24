#ifndef DUXCEL_OBJECT_LAYOUT_COMMAND_H
#define DUXCEL_OBJECT_LAYOUT_COMMAND_H
#include "object_fence.h"
#include "history_definition.h"
typedef struct {
    ObjectCommandContext command;
    const uint64_t *source_rows;
    const size_t *source_columns;
    const bool *source_count_pending;
} ObjectLayoutContext;
typedef struct { ObjectCommandTarget object; uint64_t source_rows; size_t source_columns; bool source_count_pending; } ObjectLayoutTarget;
typedef enum { OBJECT_LAYOUT_REPRESENTATION,OBJECT_LAYOUT_SIZE } ObjectLayoutOperation;
typedef struct { ObjectLayoutOperation operation; ObjectKind kind; ChartMark mark; uint64_t rows; size_t columns; } ObjectLayoutCommand;
typedef struct { size_t owned,validation_scratch; } ObjectLayoutPlanInfo;
typedef enum { OBJECT_LAYOUT_APPLIED,OBJECT_LAYOUT_NOOP,OBJECT_LAYOUT_READY,OBJECT_LAYOUT_STALE,
    OBJECT_LAYOUT_INVALID,OBJECT_LAYOUT_PROTECTED,OBJECT_LAYOUT_LIMIT } ObjectLayoutStatus;
typedef struct { ObjectLayoutStatus status; uint64_t object,before_revision,after_revision; } ObjectLayoutResult;
/* Borrow current source extent/count scalars, never Page ownership. Pending
   counts protect the entire source column lane. Target also freezes the complete
   object fence; output unchanged on failure. App owns schema/encoding readiness. */
bool workbook_prepare_object_layout(ObjectLayoutContext *context,size_t sheet,uint64_t object,ObjectLayoutTarget *out,char **error);
/* Allocation-free complete definition/placement projection; info unchanged on
   no-op/failure. Matrix extents are supplied by the App's accepted result. */
ObjectLayoutStatus objects_layout_measure(const ObjectStore *store,uint64_t object,ObjectLayoutCommand command,const CellStore *cells,
    uint64_t source_rows,size_t source_columns,ObjectLayoutPlanInfo *info,char **error);
/* One graph/action/event; preserve query/Marks/title/allocator/source identity.
   Reserve before eviction and recheck source extent + full owner after reserve.
   plan_reserved covers graph + max parser scratch. No-op preserves Redo. App
   retains cache/focus/grid/job ownership; no retained App writer or IO here. */
ObjectLayoutResult workbook_apply_object_layout_recorded(ObjectLayoutContext *context,const ObjectLayoutTarget *target,
    ObjectLayoutCommand command,const WorkbookHistoryDefinitionRecorder *recorder,char **error);
#endif
