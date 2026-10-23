#ifndef DUXCEL_APP_SELECTION_H
#define DUXCEL_APP_SELECTION_H
#include "../grid/selection_ladder.h"
#include "../workbook/cells.h"

/* Borrowed, resolved frame snapshot. The App adapter verifies source/result
   revisions; this controller owns no pages, workbook, worker or UI focus. */
typedef struct {
    const CellStore *cells;
    GridRegion owner,body,headers,primary,used;
} SelectionContext;
typedef struct { bool within_owner,whole_owner; } SelectionOutcome;

/* Only changes finite selection boundaries/mode. No I/O, query, history,
   viewport changes or borrowed-state mutation. */
SelectionOutcome selection_expand(Grid *grid,const SelectionContext *context);
#endif
