#ifndef DUXCEL_MIXED_RANGE_PLAN_H
#define DUXCEL_MIXED_RANGE_PLAN_H
#include "workbook.h"
#include "cell_range_plan.h"
#include "objects_asset.h"
typedef struct { size_t owned,validation_scratch; } MixedRangePlanInfo;
typedef struct {
    CellSnapshot cells; ObjectSnapshot objects; FormatSnapshot formats;
    uint64_t object_high_water;
    unsigned effects; /* Truthful subset of Cells2/Objects4/Format8. */
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
void mixed_range_plan_free(MixedRangePlan *plan);
#endif
