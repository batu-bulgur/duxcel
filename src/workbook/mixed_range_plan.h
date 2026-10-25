#ifndef DUXCEL_MIXED_RANGE_PLAN_H
#define DUXCEL_MIXED_RANGE_PLAN_H
#include "workbook.h"
#include "cell_range_plan.h"
#include "cell_paste_plan.h"
#include "objects_asset.h"
#include "column_widths.h"
typedef struct { size_t owned,validation_scratch; } MixedRangePlanInfo;
typedef struct {
    CellSnapshot cells; ObjectSnapshot objects; FormatSnapshot formats;
    uint64_t object_high_water;
    ColumnWidthState widths; /* Column axis: complete canonical target only. */
    unsigned effects; /* Truthful subset of Cells2/Objects4/Format8/Width16. */
} MixedRangePlan;
typedef enum { MIXED_RANGE_READY,MIXED_RANGE_NOOP,MIXED_RANGE_INVALID,MIXED_RANGE_PROTECTED,MIXED_RANGE_OCCUPIED,MIXED_RANGE_LIMIT } MixedRangeStatus;
/* Source-free manual range plus fully contained blank-primary Pivot/Chart
   templates only. Tables/source links/imported formats remain protected.
   Existing range geometry and Replace rules apply. Measurement allocates no
   models; complete semantic validation follows caller scratch admission.
   Plan owns complete independent after states, including unchanged lanes.
   It preserves runtime object metadata and allocator high-water. No histories,
   clocks, packets or live state are adopted; a later complete event/controller
   still must reserve all constituents and fence actual owners before adoption.
   Inputs are trusted immutable ownership; out must be independent. Failure and
   NOOP preserve out and every input. reserved covers NEW clones/planner peak;
   validation_reserved is separate bounded grammar scratch. */
MixedRangeStatus mixed_range_measure(const Worksheet *sheet,const Analysis *primary,SheetRange range,int64_t dr,int dc,bool replace,MixedRangePlanInfo *info,char **error);
MixedRangeStatus mixed_range_plan(const Worksheet *sheet,const Analysis *primary,SheetRange range,int64_t dr,int dc,bool replace,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);
/* Existing row-axis semantics: insertion/deletion may translate whole templates
   after the boundary, never split/delete an intersected object or lose data at
   the worksheet edge. Complete cloned cells/objects/formats; widths unchanged. */
MixedRangeStatus mixed_rows_measure(const Worksheet *sheet,const Analysis *primary,uint64_t row,uint64_t count,bool insert,MixedRangePlanInfo *info,char **error);
MixedRangeStatus mixed_rows_plan(const Worksheet *sheet,const Analysis *primary,uint64_t row,uint64_t count,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);
/* Single-column counterpart with the existing whole-object intersection/edge
   guards. Own canonical target widths, preserving virtual default96 semantics
   and a truncated nondefault tail through the later complete before/after pair.
   Names stay at fixed positions. This remains a pure plan, not width adoption. */
MixedRangeStatus mixed_column_measure(const Worksheet *sheet,const Analysis *primary,size_t column,bool insert,MixedRangePlanInfo *info,char **error);
MixedRangeStatus mixed_column_plan(const Worksheet *sheet,const Analysis *primary,size_t column,bool insert,size_t reserved,size_t validation_reserved,MixedRangePlan *out,char **error);
typedef struct { CellPastePlanInfo cells; size_t validation_scratch; } MixedPastePlanInfo;
/* Text-only paste on a source-free template sheet. Preserve the existing full
   parsed bounding-rectangle guard, including ragged holes and empty fields.
   Objects/formats/widths remain borrowed and unchanged; output owns cells only.
   Measure allocates no models. Plan validates the complete unchanged object
   graph after scratch admission, then owns the exact prospective sparse cells.
   NOOP/failure preserve output. Input text stays immutable through this pure
   call; the later controller must own its packet before external callbacks. */
MixedRangeStatus mixed_paste_measure(const Worksheet *sheet,const Analysis *primary,uint64_t row,size_t column,const char *text,MixedPastePlanInfo *info,char **error);
MixedRangeStatus mixed_paste_plan(const Worksheet *sheet,const Analysis *primary,uint64_t row,size_t column,const char *text,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error);
typedef struct { CellRangePlanInfo cells; size_t validation_scratch; } MixedClearPlanInfo;
/* Existing sparse-only Clear: a range may span a template's painted rectangle,
   but only CellStore entries are removed. Objects, styles and widths stay
   unchanged. Finite bounds, independent output and admitted graph validation
   follow the same pure ownership rules as template-sheet text paste. */
MixedRangeStatus mixed_clear_measure(const Worksheet *sheet,const Analysis *primary,SheetRange range,MixedClearPlanInfo *info,char **error);
MixedRangeStatus mixed_clear_plan(const Worksheet *sheet,const Analysis *primary,SheetRange range,size_t reserved,size_t validation_reserved,CellSnapshot *out,char **error);
void mixed_range_plan_free(MixedRangePlan *plan);
#endif
