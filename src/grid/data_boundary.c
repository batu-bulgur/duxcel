#include "data_boundary.h"
static bool inside(uint64_t value,uint64_t first,uint64_t last) { return value>=(first<last ? first : last) && value<=(first>last ? first : last); }
static bool valid(GridBoundarySelection s) {
    return s.row<DX_SHEET_ROWS && s.anchor_row<DX_SHEET_ROWS && s.end_row<DX_SHEET_ROWS && s.column<GRID_BOUNDARY_COLUMNS && s.anchor_column<GRID_BOUNDARY_COLUMNS && s.end_column<GRID_BOUNDARY_COLUMNS &&
        inside(s.row,s.anchor_row,s.end_row) && inside(s.column,s.anchor_column,s.end_column);
}
static bool equal(GridBoundarySelection a,GridBoundarySelection b) {
    return a.row==b.row && a.anchor_row==b.anchor_row && a.end_row==b.end_row && a.column==b.column && a.anchor_column==b.anchor_column && a.end_column==b.end_column;
}
GridBoundaryStatus grid_data_boundary(const GridBoundarySelection *before,const GridBoundaryOccupancy *o,GridBoundaryDirection direction,bool shift,GridBoundarySelection *out) {
    if (!before || !o || !out || (unsigned)direction>GRID_BOUNDARY_RIGHT || !valid(*before) || (o->count && !o->read)) return GRID_BOUNDARY_INVALID;
    if (o->count>o->grant || o->grant>GRID_BOUNDARY_CELLS_MAX) return GRID_BOUNDARY_LIMIT;
    GridBoundarySelection state=*before;
    bool vertical=direction==GRID_BOUNDARY_UP || direction==GRID_BOUNDARY_DOWN,forward=direction==GRID_BOUNDARY_DOWN || direction==GRID_BOUNDARY_RIGHT;
    uint64_t row=shift ? state.end_row : state.row; size_t column=shift ? state.end_column : state.column;
    uint64_t start=vertical ? row : column,edge=forward ? (vertical ? DX_SHEET_ROWS-1 : GRID_BOUNDARY_COLUMNS-1) : 0;
    uint64_t nearest=edge,run_first=0,run_last=0,lane_first=0,lane_previous=0; bool found=false,occupied=false,have_lane=false;
    GridBoundaryCell previous={0};
    for (size_t i=0;i<o->count;i++) {
        GridBoundaryCell cell={0}; if (!o->read(o->context,i,&cell)) return GRID_BOUNDARY_UNAVAILABLE;
        if (cell.row>=DX_SHEET_ROWS || cell.column>=GRID_BOUNDARY_COLUMNS || (i && (cell.row<previous.row || (cell.row==previous.row && cell.column<=previous.column)))) return GRID_BOUNDARY_INVALID;
        previous=cell;
        if (!cell.occupied || (vertical ? cell.column!=column : cell.row!=row)) continue;
        uint64_t position=vertical ? cell.row : cell.column;
        if (!have_lane || position!=lane_previous+1) lane_first=position;
        lane_previous=position; have_lane=true;
        if (position==start) { occupied=true; run_first=lane_first; run_last=start; }
        else if (occupied && position==run_last+1) run_last=position;
        if (forward ? position>start : position<start) {
            if (!found || (forward ? position<nearest : position>nearest)) nearest=position;
            found=true;
        }
    }
    uint64_t target=occupied && (forward ? run_last>start : run_first<start) ? (forward ? run_last : run_first) : nearest;
    if (vertical) row=target; else column=(size_t)target;
    GridBoundarySelection next=state;
    if (!shift) next=(GridBoundarySelection){row,row,row,column,column,column};
    else {
        next.end_row=row; next.end_column=column;
        if (!inside(next.row,next.anchor_row,next.end_row) || !inside(next.column,next.anchor_column,next.end_column)) { next.row=next.anchor_row; next.column=next.anchor_column; }
    }
    if (equal(state,next)) return GRID_BOUNDARY_NOOP;
    *out=next; return GRID_BOUNDARY_APPLIED;
}
