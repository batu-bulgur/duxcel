#ifndef DUXCEL_SOURCE_RANGE_PLAN_H
#define DUXCEL_SOURCE_RANGE_PLAN_H
#include "workbook.h"
#include "cell_paste_plan.h"
/* Actual borrowed result policy. ready means the worksheet/result association
   is usable, not that count completed. Pending counts protect all rows in the
   known source columns. Zero columns reject paste unless fetch_failed selects
   the existing failed-result policy. No source identity is inferred. */
typedef struct {
    const Worksheet *sheet; const Analysis *primary;
    uint64_t source_rows; size_t source_columns;
    bool count_pending,fetch_failed,ready;
} SourceRangeContext;
typedef struct { size_t owned,validation_scratch,rows,columns; } SourceRangePlanInfo;
typedef enum { SOURCE_RANGE_READY,SOURCE_RANGE_NOOP,SOURCE_RANGE_INVALID,SOURCE_RANGE_PROTECTED,SOURCE_RANGE_LIMIT } SourceRangeStatus;
/* Paste protects its WHOLE parsed rectangle, including ragged holes/empty clears.
   Clear only removes sparse manual cells, even across painted results. Both own
   complete prospective cells only; primary/objects/formats/ledger/widths remain
   unchanged. Measure allocates no models. Admitted scratch precedes unchanged
   graph/dependency validation and cloning. Inputs/text remain immutable through
   pure calls; out is independent. NOOP/failure preserve out/info and inputs.
   No history, clock, event, IO, activation, identity issuance or Origin fence. */
SourceRangeStatus source_range_paste_measure(SourceRangeContext c,uint64_t row,size_t column,const char *text,SourceRangePlanInfo *info,char **error);
SourceRangeStatus source_range_paste_plan(SourceRangeContext c,uint64_t row,size_t column,const char *text,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error);
SourceRangeStatus source_range_clear_measure(SourceRangeContext c,SheetRange range,SourceRangePlanInfo *info,char **error);
SourceRangeStatus source_range_clear_plan(SourceRangeContext c,SheetRange range,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error);
#endif
