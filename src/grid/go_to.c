#include "go_to.h"
#include <math.h>
#include <string.h>
GridGoToStatus grid_go_to_parse(const char *text,size_t bytes,bool source_header,GridGoToTarget *out) {
    if (!out || !text || !bytes) return GRID_GO_TO_INVALID;
    if (bytes>GRID_GO_TO_ADDRESS_BYTES) return GRID_GO_TO_LIMIT;
    if (memchr(text,0,bytes)) return GRID_GO_TO_INVALID;
    char draft[GRID_GO_TO_ADDRESS_BYTES+1]; memcpy(draft,text,bytes); draft[bytes]=0;
    GridGoToTarget next;
    if (!grid_parse_display_address(draft,source_header,&next.row,&next.column)) return GRID_GO_TO_INVALID;
    *out=next; return GRID_GO_TO_READY;
}
static bool same(GridLocation a,GridLocation b) {
    return a.row==b.row && a.anchor_row==b.anchor_row && a.end_row==b.end_row && a.top==b.top && a.col==b.col && a.anchor_col==b.anchor_col && a.end_col==b.end_col &&
        a.horizontal==b.horizontal && a.fraction==b.fraction && a.selection==b.selection;
}
GridGoToStatus grid_go_to_apply(Grid *g,const Page *page,float scale,GridGoToTarget target) {
    if (!g || !page || target.row>=DX_SHEET_ROWS || target.column>=GRID_BOUNDARY_COLUMNS || !isfinite(scale) || scale<=0 || !isfinite(g->zoom) || g->zoom<=0 ||
        !g->visible_rows || g->top>=DX_SHEET_ROWS || !isfinite(g->horizontal) || g->horizontal<0 || !isfinite(g->fraction) || g->fraction<0 || g->fraction>=1 ||
        !isfinite(g->rect.x) || !isfinite(g->rect.y) || !isfinite(g->rect.width) || !isfinite(g->rect.height) || g->rect.width<=0 || g->rect.height<=0 || !isfinite(g->body.width/scale)) return GRID_GO_TO_INVALID;
    if (g->bounded || g->selection!=SELECT_CELLS || g->dragging || g->moving || g->resizing>=0 || g->scrollbar) return GRID_GO_TO_PROTECTED;
    GridLocation before=grid_location(g); size_t capacity=g->width_count;
    if (!grid_try_ensure_columns(g,target.column+1)) return GRID_GO_TO_LIMIT;
    grid_select_cell(g,target.row,target.column,false); grid_reveal_navigation_target(g,page,scale,target.row,target.column);
    return capacity==g->width_count && same(before,grid_location(g)) ? GRID_GO_TO_NOOP : GRID_GO_TO_APPLIED;
}
