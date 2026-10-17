#ifndef DUXCEL_SELECTION_LADDER_H
#define DUXCEL_SELECTION_LADDER_H
#include "grid.h"

#define GRID_SELECTION_CANDIDATES_MAX 8

/* The caller supplies only resolved, current owner bounds as active regions.
   Choose the smallest strict superset of the entire finite selection. Equal
   areas use coordinate order, independent of candidate order. Invalid/inactive
   regions are ignored; malformed input/count or current state is a no-op.
   Without a finite superset, select the whole sheet. Row/column selections
   also become whole-sheet selections, which remain terminal. Only mode and
   boundary corners may change; retain the active cell and corner directions.
   No allocation, cell traversal, rendering, viewport change or query occurs. */
void grid_select_next_region(Grid *grid,const GridRegion *candidates,size_t count);
#endif
