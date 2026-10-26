#ifndef DUXCEL_GRID_DATA_BOUNDARY_H
#define DUXCEL_GRID_DATA_BOUNDARY_H
#include "../common.h"
#define GRID_BOUNDARY_CELLS_MAX 20000u
#define GRID_BOUNDARY_COLUMNS 16384u
typedef struct { uint64_t row; size_t column; bool occupied; } GridBoundaryCell;
typedef struct {
    /* Synchronous read-only/non-reentrant accessor. The complete sparse model,
       not a viewport Page, stays immutable/pinned throughout the call. Stored
       presence (including SQL NULL and empty TEXT) is occupied; format-only
       coordinates are not. Return false when this actual inventory is unknown.
       Entries must be strictly row/column ordered, unique and physically valid,
       including any explicit unoccupied entries. No borrowed content is kept. */
    bool (*read)(const void *context,size_t index,GridBoundaryCell *out);
    const void *context;
    size_t count,grant; /* Explicit bounded read grant; count<=grant<=20,000. */
} GridBoundaryOccupancy;
typedef struct { uint64_t row,anchor_row,end_row; size_t column,anchor_column,end_column; } GridBoundarySelection;
typedef enum { GRID_BOUNDARY_UP,GRID_BOUNDARY_DOWN,GRID_BOUNDARY_LEFT,GRID_BOUNDARY_RIGHT } GridBoundaryDirection;
typedef enum { GRID_BOUNDARY_APPLIED,GRID_BOUNDARY_NOOP,GRID_BOUNDARY_INVALID,GRID_BOUNDARY_UNAVAILABLE,GRID_BOUNDARY_LIMIT } GridBoundaryStatus;
/* Manual-only finite CELL selection controller. The adapter must exclude actual
   source/object/unknown analytical owners and whole-axis selections; this API
   neither resolves nor queries them. Full fixed physical edges, never Page or
   used-range edges, bound blank-gap traversal. Occupied + occupied neighbour
   follows that contiguous run to its end; otherwise jump to the first occupied
   coordinate in the direction or the grid edge. Work is O(inventory), with no
   allocation/coordinate-gap traversal, IO, query, dirty flag or history.
   Shift starts at moving end, preserves anchor, and keeps active inside the new
   rectangle (otherwise returns it to anchor, matching ordinary Grid extension).
   Non-Shift starts at active and collapses the selection. Only APPLIED writes
   out; all other statuses preserve it. Input/out alias is supported. */
GridBoundaryStatus grid_data_boundary(const GridBoundarySelection *before,const GridBoundaryOccupancy *occupancy,
    GridBoundaryDirection direction,bool shift,GridBoundarySelection *out);
#endif
