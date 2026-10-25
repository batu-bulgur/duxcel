#ifndef DUXCEL_CELL_PASTE_PLAN_H
#define DUXCEL_CELL_PASTE_PLAN_H
#include "cell_range_plan.h"
typedef struct { size_t owned,count,bytes,rows,columns; } CellPastePlanInfo;
/* Pure manual-cell projection using the ordinary paste TSV scanner. Explicit
   empty fields clear one coordinate; ragged holes remain untouched. Literal
   NULL and exact numeric text stay text. Formats/source policy are separate.
   Measurement allocates nothing (scanner uses a bounded64KiB stack buffer).
   Input strings/current state remain immutable across measure/plan. info/out
   remain unchanged on NOOP/failure. Output must be independent owned storage.
   reserved covers the complete new sparse array and all its text before any
   cloning; existing current/output/history are charged separately. No mutation,
   history eviction, revision change or worksheet action is performed here. */
CellRangeStatus cells_paste_measure(const CellStore *store,uint64_t row,size_t column,const char *text,CellPastePlanInfo *info,char **error);
CellRangeStatus cells_paste_plan(const CellStore *store,uint64_t row,size_t column,const char *text,size_t reserved,CellSnapshot *out,char **error);
#endif
