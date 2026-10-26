#ifndef DUXCEL_GRID_GO_TO_H
#define DUXCEL_GRID_GO_TO_H
#include "grid.h"
#define GRID_GO_TO_ADDRESS_BYTES 64u
typedef struct { uint64_t row; size_t column; } GridGoToTarget;
typedef enum { GRID_GO_TO_READY,GRID_GO_TO_APPLIED,GRID_GO_TO_NOOP,GRID_GO_TO_INVALID,GRID_GO_TO_LIMIT,GRID_GO_TO_PROTECTED } GridGoToStatus;
/* Bounded stack copy, then the genuine displayed-address parser; no second
   address grammar, truncation, names/ranges/formulas or retained draft pointer.
   bytes excludes NUL; embedded NUL rejects. Failure preserves target/text. */
GridGoToStatus grid_go_to_parse(const char *text,size_t bytes,bool source_header,GridGoToTarget *out);
/* Actual closed manual Grid/view only, under the caller's current sheet/ready
   fence. No source/uncached/analytical permission is granted. Checked capacity
   precedes all selection/view mutation; existing names/widths remain exact.
   Only finite cell mode, idle/unbounded Grid and finite ready layout are
   supported. Success collapses to target and reveals it, without model/query/
   history access. NOOP means the logical selection/viewport stayed unchanged.
   Refusal changes nothing; no caller draft is accepted or mutated here. */
GridGoToStatus grid_go_to_apply(Grid *grid,const Page *actual_page,float scale,GridGoToTarget target);
#endif
