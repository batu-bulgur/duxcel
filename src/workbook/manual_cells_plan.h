#ifndef DUXCEL_MANUAL_CELLS_PLAN_H
#define DUXCEL_MANUAL_CELLS_PLAN_H
#include "cells.h"
#include "manual_range_wire.h"
typedef struct ManualCells ManualCells;
typedef struct { size_t count,raw_bytes,owned,coexisting; } ManualCellsInfo;
typedef enum { MANUAL_CELLS_READY,MANUAL_CELLS_NOOP,MANUAL_CELLS_INVALID,MANUAL_CELLS_LIMIT } ManualCellsStatus;
/* Unpublished command candidate, NOT a second live CellStore. Sorted unique
   coordinates, at most20,000 present values, authored raw total<=4MiB; value
   headers/canonical text/array are additional explicitly measured ownership.
   Missing means no entry; SQL NULL and empty TEXT are present entries.
   Import requires the current canonical CellSnapshot shape/accounting (nonempty
   text<=64KiB). Every entry becomes explicit UTF-8 TEXT, without inference.
   A malformed legacy in-memory byte span rejects; this is not a file migration.
   Measure uses only the scalar value parser's fixed stack, no heap scratch.
   info stays unchanged on failure/NOOP. coexisting includes input + new owner. */
ManualCellsStatus manual_cells_import_measure(const CellSnapshot *source,ManualCellsInfo *info);
ManualCellsStatus manual_cells_import(const CellSnapshot *source,size_t retained,size_t reserved,ManualCells **out);
/* Complete sparse after-state for one explicit set/clear. Exact raw/type/presence
   equality decides NOOP. Before remains caller-owned and immutable. No revision,
   worksheet action, Undo/Redo, history eviction or source eligibility is issued.
   reserved covers input residency + distinct prior output + complete new owner
   + caller-supplied retained ownership, before any clone. If source==*out it is
   counted once. Borrowed input may refer to source/prior; both live until copied.
   Failure/NOOP preserves every owner/output. Disposal reads no borrowed input. */
ManualCellsStatus manual_cells_edit_measure(const ManualCells *source,uint64_t row,size_t column,ManualValueInput value,ManualCellsInfo *info);
ManualCellsStatus manual_cells_edit(const ManualCells *source,uint64_t row,size_t column,ManualValueInput value,size_t retained,size_t reserved,ManualCells **out);
/* Private DXMR values only: every coordinate replaces the destination rectangle,
   including missing holes which remove entries. Outside cells remain exact.
   Explicit offsets are checked before walking values; projection is a sorted
   O(source count + bounded rectangle area) merge, no dense worksheet allocation.
   coexisting includes source + decoded range + complete new candidate. A prior
   output distinct from source and retained are additionally admitted by plan.
   Policy/object/source protection and all live adoption remain external. */
ManualCellsStatus manual_cells_paste_measure(const ManualCells *source,uint64_t row,size_t column,const ManualRange *range,ManualCellsInfo *info);
ManualCellsStatus manual_cells_paste(const ManualCells *source,uint64_t row,size_t column,const ManualRange *range,size_t retained,size_t reserved,ManualCells **out);
size_t manual_cells_owned(const ManualCells *cells);
size_t manual_cells_count(const ManualCells *cells);
const ManualValue *manual_cells_at(const ManualCells *cells,uint64_t row,size_t column);
void manual_cells_free(ManualCells **cells);
/* Only the explicit bounded rectangle is materialized, into caller-owned slots
   (count pointers, separate caller charge), with exact missing holes. No heap
   allocation/bounding-box traversal, no lossy typed-to-text installation. Slots
   and output remain unchanged on refusal. Slots must be disjoint from source.
   View borrows slots and source through the downstream DXMR capture. */
ManualCellsStatus manual_cells_range(const ManualCells *source,uint64_t row,size_t column,size_t rows,size_t columns,const ManualValue **slots,size_t capacity,ManualRangeView *out);
/* Future atomic cutover MUST close live store and all snapshots/range plans,
   typed command/Undo and DXCD/journal semantics, clipboard/protected paste,
   strict presence assets/current core persistence, and render/editor intents.
   Existing text-only CellStore/DXCD1 remain unchanged. Never install this owner
   by converting NULL/empty/typed numbers to legacy text. No App/wire activation. */
#endif
