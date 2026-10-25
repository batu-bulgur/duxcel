#ifndef DUXCEL_RANGE_COMMAND_H
#define DUXCEL_RANGE_COMMAND_H
#include "object_fence.h"
#include "cell_range_plan.h"
#include "cell_paste_plan.h"
#include "source_range_plan.h"
#include "history_range.h"
#include "workbook_width_command.h"
typedef bool (*RangeCommandAdmission)(void *context,const FormatStore *prospective,size_t adopted,size_t scratch,char **error);
/* Optional actual result policy for manual PASTE/CLEAR only. All pointers are
   borrowed live authorities and must remain readable through recorder callbacks.
   No default identity, source completion or movement policy is inferred. */
typedef struct {
    const uint64_t *rows; const size_t *columns;
    const bool *count_pending,*fetch_failed,*ready;
} RangeSourcePolicy;
/* Live result association for source MOVE/axes. The resolver borrows current
   owners afresh after callbacks. The synchronous resolver is read-only, performs
   no allocation/IO/event dispatch, and cannot reenter or mutate the command.
   Captured native pointers are never old owners
   to dereference. Primary object0, every Table and every Pivot with a nonempty Values shelf must
   be resolved, including obstacles outside the selected range. Page headers
   are fences only, never event payloads or dataset retention. */
typedef struct {
    uint64_t object,request,query_epoch,source_epoch; /* request is the document request epoch. */
    const void *owner; const Analysis *definition; const Page *page;
    bool ready,failed;
} RangeResultProof;
typedef struct { bool (*resolve)(void *context,uint64_t object,RangeResultProof *out); void *context; } RangeResultContext;
typedef struct { RangeResultProof proof; Analysis definition; Page page; } RangeResultSnapshot;
typedef struct {
    ObjectCommandContext command;
    const Analysis *primary; const uint64_t *primary_epoch;
    RangeCommandAdmission admit; void *admission_context;
    const RangeSourcePolicy *source;
    const RangeResultContext *results;
} RangeCommandContext;
typedef struct {
    ObjectCommandTarget owner;
    const Analysis *authority; Analysis primary,cached; uint64_t primary_epoch;
    CellStore cells; SheetCell entries[DX_MAX_EDIT_CELLS];
    FormatStore formats; FormatPatch patches[DX_FORMAT_PATCHES];
    FormatScope scopes[DX_FORMAT_PATCHES*DX_FORMAT_GUARDS];
    const RangeSourcePolicy *source_authority;
    RangeSourcePolicy source; uint64_t source_rows; size_t source_columns;
    bool source_present,count_pending,fetch_failed,result_ready;
    const RangeResultContext *results_authority; RangeResultContext results;
    size_t result_count; RangeResultSnapshot result[DX_MAX_OBJECTS+1];
    WorkbookWidthTarget widths;
} RangeCommandTarget;
typedef enum { RANGE_OPERATION_MOVE,RANGE_OPERATION_INSERT_ROWS,RANGE_OPERATION_DELETE_ROWS,RANGE_OPERATION_INSERT_COLUMNS,RANGE_OPERATION_DELETE_COLUMNS,RANGE_OPERATION_PASTE,RANGE_OPERATION_CLEAR } RangeCommandOperation;
typedef struct {
    SheetRange range; int64_t dr; int dc; bool replace;
    RangeCommandOperation operation;
    uint64_t row,row_count;
    size_t column,column_count; /* Current compound column owner admits count1. */
    const char *text; /* PASTE: borrowed immutable TSV until planning completes. */
} RangeCommand;
typedef struct { size_t owned,validation_scratch,rows,columns; } RangeCommandPlanInfo;
typedef enum { RANGE_COMMAND_APPLIED,RANGE_COMMAND_NOOP,RANGE_COMMAND_READY,RANGE_COMMAND_STALE,
    RANGE_COMMAND_INVALID,RANGE_COMMAND_PROTECTED,RANGE_COMMAND_OCCUPIED,RANGE_COMMAND_LIMIT } RangeCommandStatus;
typedef struct { RangeCommandStatus status; unsigned effects; uint64_t before_revision,after_revision; size_t rows,columns; } RangeCommandResult;
/* Immediate single-owner native descriptor fences; captured strings/packets
   remain immutable. Primary means actual App authority, separate from Save
   cache. Without an explicit source policy it admits source-free manual formats;
   an explicit source policy admits guarded manual paste/Clear. Source movement
   and axes additionally require the current actual-result resolver proofs. Fully contained
   source-free Pivot/Chart templates may move with manual cells/formats or
   translate as complete row/column-axis constituents under existing protections.
   Text-only paste outside templates preserves the existing full rectangle guard
   and changes cells only; it never modifies object definitions or formats.
   Finite Clear may span templates and removes only sparse manual CellStore data. */
bool workbook_prepare_range_command(RangeCommandContext *context,size_t sheet,RangeCommandTarget *out,char **error);
/* Conservative complete cell clone + changed-cell vector + bounded format-plan
   peak. No rectangle-area traversal; all constituent reservation precedes copy.
   Output preserved on failure. Zero displacement returns NOOP without allocation. */
RangeCommandStatus workbook_range_command_measure(const RangeCommandContext *context,size_t sheet,RangeCommand command,RangeCommandPlanInfo *info,char **error);
/* One changed-cell/Format compound event and one matching worksheet action.
   Exact Cells/Objects/Format/Width effects; no-op preserves Redo and all clocks. Validate projected
   Format history even with no global admission callback. Reserve before either
   model journal can evict, recheck all owners and transfer independent graphs.
   App owns budget/readiness, input/confirmation, selection and rendering. No IO,
   retained App writer, source activation or App source movement routing.
   Manual row and single-column insertion/deletion use this complete effect owner.
   Physical widths use committed Worksheet.view; App projects the admitted view
   into Grid. Source movement/axes remain protected without actual-result proofs. Text-only
   manual paste and finite CLEAR preserve existing styles and widths; rows/columns are returned on change.
   The shared scanner validates the whole text before allocation. Its complete
   independent cell plan supplies the event and adoption; callbacks never read
   the original clipboard text, which is no longer part of admitted ownership. */
RangeCommandResult workbook_apply_range_command_recorded(RangeCommandContext *context,const RangeCommandTarget *target,RangeCommand command,
    const WorkbookHistoryRangeRecorder *recorder,char **error);
#endif
