#ifndef DUXCEL_SOURCE_RANGE_PLAN_H
#define DUXCEL_SOURCE_RANGE_PLAN_H
#include "workbook.h"
#include "cell_paste_plan.h"
#include "mixed_range_plan.h"
/* Actual borrowed result policy. ready means the worksheet/result association
   is usable, not that count completed. Pending counts protect all rows in the
   known source columns. Zero columns reject paste unless fetch_failed selects
   the existing failed-result policy. No source identity is inferred. */
typedef struct {
    const Worksheet *sheet; const Analysis *primary;
    uint64_t source_rows; size_t source_columns;
    bool count_pending,fetch_failed,ready;
} SourceRangeContext;
typedef struct { size_t owned,validation_scratch,rows,columns,count; } SourceRangePlanInfo;
typedef enum { SOURCE_RANGE_READY,SOURCE_RANGE_NOOP,SOURCE_RANGE_INVALID,SOURCE_RANGE_PROTECTED,SOURCE_RANGE_LIMIT,SOURCE_RANGE_OCCUPIED } SourceRangeStatus;
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
/* Explicit full protected Table footprint, including its header if applicable.
   The integration owner proves this came from complete actual result metadata,
   never viewport rows. Exactly one proof per live Table; no unknown/pending/
   failed primary or Table result can enter this pure movement producer. */
typedef struct { uint64_t object,rows; size_t columns; bool ready,count_pending,failed; } SourceTableExtent;
typedef struct { SourceRangeContext range; const SourceTableExtent *tables; size_t table_count; } SourceMoveContext;
/* Existing whole-object/primary-rectangle/Replace protections; complete owned
   cells/objects/formats, unchanged widths, primary and immutable source ledger.
   No source issuance/rebinding, event/controller/App activation or history edits. */
SourceRangeStatus source_range_move_measure(SourceMoveContext c,SheetRange range,int64_t dr,int dc,bool replace,MixedRangePlanInfo *info,char **error);
SourceRangeStatus source_range_move_plan(SourceMoveContext c,SheetRange range,int64_t dr,int dc,bool replace,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);

/* Complete source-bearing row-axis projection. Imported primary rows and
   intersected Tables/pivots/charts remain protected; no source/width changes. */
SourceRangeStatus source_range_rows_measure(SourceMoveContext c,uint64_t row,uint64_t count,bool insert,MixedRangePlanInfo *info,char **error);
SourceRangeStatus source_range_rows_plan(SourceMoveContext c,uint64_t row,uint64_t count,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);

/* Single-column counterpart, with an independently owned canonical target
   width state. The later owner still must pair the exact current/after widths;
   no scalar inverse, name shift or duplicate committed width owner is created. */
SourceRangeStatus source_range_column_measure(SourceMoveContext c,size_t column,bool insert,MixedRangePlanInfo *info,char **error);
SourceRangeStatus source_range_column_plan(SourceMoveContext c,size_t column,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);

#endif
