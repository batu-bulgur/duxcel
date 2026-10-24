#ifndef DUXCEL_RANGE_COMMAND_H
#define DUXCEL_RANGE_COMMAND_H
#include "object_fence.h"
#include "cell_range_plan.h"
#include "history_range.h"
typedef bool (*RangeCommandAdmission)(void *context,const FormatStore *prospective,size_t adopted,size_t scratch,char **error);
typedef struct {
    ObjectCommandContext command;
    const Analysis *primary; const uint64_t *primary_epoch;
    RangeCommandAdmission admit; void *admission_context;
} RangeCommandContext;
typedef struct {
    ObjectCommandTarget owner;
    const Analysis *authority; Analysis primary; uint64_t primary_epoch;
    CellStore cells; SheetCell entries[DX_MAX_EDIT_CELLS];
    FormatStore formats; FormatPatch patches[DX_FORMAT_PATCHES];
} RangeCommandTarget;
typedef enum { RANGE_OPERATION_MOVE,RANGE_OPERATION_INSERT_ROWS,RANGE_OPERATION_DELETE_ROWS } RangeCommandOperation;
typedef struct {
    SheetRange range; int64_t dr; int dc; bool replace;
    RangeCommandOperation operation;
    uint64_t row,row_count;
} RangeCommand;
typedef struct { size_t owned,validation_scratch; } RangeCommandPlanInfo;
typedef enum { RANGE_COMMAND_APPLIED,RANGE_COMMAND_NOOP,RANGE_COMMAND_READY,RANGE_COMMAND_STALE,
    RANGE_COMMAND_INVALID,RANGE_COMMAND_PROTECTED,RANGE_COMMAND_OCCUPIED,RANGE_COMMAND_LIMIT } RangeCommandStatus;
typedef struct { RangeCommandStatus status; unsigned effects; uint64_t before_revision,after_revision; } RangeCommandResult;
/* Immediate single-owner native descriptor fences; captured strings/packets
   remain immutable. Primary means actual App authority, separate from Save
   cache. This first owner admits source/object-free spatial manual formats only;
   mixed/source/object ranges retain their existing complete producer. */
bool workbook_prepare_range_command(RangeCommandContext *context,size_t sheet,RangeCommandTarget *out,char **error);
/* Conservative complete cell clone + changed-cell vector + bounded format-plan
   peak. No rectangle-area traversal; all constituent reservation precedes copy.
   Output preserved on failure. Zero displacement returns NOOP without allocation. */
RangeCommandStatus workbook_range_command_measure(const RangeCommandContext *context,size_t sheet,RangeCommand command,RangeCommandPlanInfo *info,char **error);
/* One changed-cell/Format compound event and one matching worksheet action.
   Exact effects2/8/10; no-op preserves Redo and all clocks. Validate projected
   Format history even with no global admission callback. Reserve before either
   model journal can evict, recheck all owners and transfer independent graphs.
   App owns budget/readiness, input/confirmation, selection and rendering. No IO,
   retained App writer, source activation or column/paste/mixed-range admission.
   Manual row insertion/deletion uses the same complete effect owner; columns
   need their additional width/history constituent before admission. */
RangeCommandResult workbook_apply_range_command_recorded(RangeCommandContext *context,const RangeCommandTarget *target,RangeCommand command,
    const WorkbookHistoryRangeRecorder *recorder,char **error);
#endif
