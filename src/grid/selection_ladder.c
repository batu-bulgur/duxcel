#include "selection_ladder.h"

static bool valid(const GridRegion *r) {
    return r->active && r->r0<=r->r1 && r->r1<DX_SHEET_ROWS &&
        r->c0<=r->c1 && r->c1<16384;
}
static bool before(const GridRegion *a,const GridRegion *b) {
    if (a->r0!=b->r0) return a->r0<b->r0;
    if (a->c0!=b->c0) return a->c0<b->c0;
    if (a->r1!=b->r1) return a->r1<b->r1;
    return a->c1<b->c1;
}
void grid_select_next_region(Grid *g,const GridRegion *candidates,size_t count) {
    if (!g || count>GRID_SELECTION_CANDIDATES_MAX || (count && !candidates) ||
        (unsigned)g->selection>SELECT_ALL || g->row>=DX_SHEET_ROWS || g->col>=16384) return;
    if (g->selection!=SELECT_CELLS) { g->selection=SELECT_ALL; return; }
    bool reverse_rows=g->anchor_row>g->end_row,reverse_columns=g->anchor_col>g->end_col;
    GridRegion current={reverse_rows ? g->end_row : g->anchor_row,
        reverse_rows ? g->anchor_row : g->end_row,
        reverse_columns ? g->end_col : g->anchor_col,
        reverse_columns ? g->anchor_col : g->end_col,true};
    if (!valid(&current) || g->row<current.r0 || g->row>current.r1 ||
        g->col<current.c0 || g->col>current.c1) return;
    const GridRegion *next=NULL; uint64_t area=0;
    for (size_t i=0;i<count;i++) {
        const GridRegion *r=&candidates[i];
        if (!valid(r) || r->r0>current.r0 || r->r1<current.r1 ||
            r->c0>current.c0 || r->c1<current.c1 ||
            (r->r0==current.r0 && r->r1==current.r1 && r->c0==current.c0 && r->c1==current.c1)) continue;
        /* Valid sheet bounds keep the product within 2^54. */
        uint64_t size=(r->r1-r->r0+1)*(uint64_t)(r->c1-r->c0+1);
        if (!next || size<area || (size==area && before(r,next))) { next=r; area=size; }
    }
    if (!next || (!next->r0 && next->r1==DX_SHEET_ROWS-1 && !next->c0 && next->c1==16383)) {
        g->selection=SELECT_ALL; return;
    }
    g->anchor_row=reverse_rows ? next->r1 : next->r0;
    g->end_row=reverse_rows ? next->r0 : next->r1;
    g->anchor_col=reverse_columns ? next->c1 : next->c0;
    g->end_col=reverse_columns ? next->c0 : next->c1;
}
