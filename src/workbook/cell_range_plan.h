#ifndef DUXCEL_CELL_RANGE_PLAN_H
#define DUXCEL_CELL_RANGE_PLAN_H
#include "cells.h"
typedef struct { size_t owned,count,bytes; } CellRangePlanInfo;
typedef enum { CELL_RANGE_READY,CELL_RANGE_NOOP,CELL_RANGE_INVALID,CELL_RANGE_OCCUPIED,CELL_RANGE_LIMIT } CellRangeStatus;
/* Sorted sparse manual cells only. Projection never visits rectangle area;
   holes clear destination on explicit replace. Full canonical current shape and
   exact text ownership are measured before cloning. info/output unchanged on
   failure/no-op. No live model/history mutation or eviction. */
CellRangeStatus cells_range_measure(const CellStore *store,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,bool replace,CellRangePlanInfo *info,char **error);
CellRangeStatus cells_range_plan(const CellStore *store,uint64_t r0,uint64_t r1,size_t c0,size_t c1,int64_t dr,int dc,bool replace,size_t reserved,CellSnapshot *out,char **error);
/* Independent sparse row insertion/deletion; no object/source or column-width
   constituent. Delete retains exact removed text in the outer before-event. */
CellRangeStatus cells_rows_measure(const CellStore *store,uint64_t row,uint64_t count,bool insert,CellRangePlanInfo *info,char **error);
CellRangeStatus cells_rows_plan(const CellStore *store,uint64_t row,uint64_t count,bool insert,size_t reserved,CellSnapshot *out,char **error);
/* Nonfallible transfer AFTER complete command/constituent reservation and
   owner recheck. Independent changed canonical snapshot replaces current; prior
   current ownership moves to bounded Undo, Redo clears, cell revision advances.
   Caller records/publishes the complete worksheet action exactly once. */
void cells_adopt_range(CellStore *store,CellSnapshot *owned);
void cells_snapshot_free(CellSnapshot *snapshot);
#endif
