#include "selection.h"

static void bounds(const Grid *g,uint64_t *r0,uint64_t *r1,size_t *c0,size_t *c1) {
    *r0=g->anchor_row<g->end_row ? g->anchor_row : g->end_row;
    *r1=g->anchor_row>g->end_row ? g->anchor_row : g->end_row;
    *c0=g->anchor_col<g->end_col ? g->anchor_col : g->end_col;
    *c1=g->anchor_col>g->end_col ? g->anchor_col : g->end_col;
}

SelectionOutcome selection_expand(Grid *g,const SelectionContext *context) {
    SelectionOutcome result={0};
    if (!g || !context) return result;
    GridRegion candidates[5]; size_t count=0;
    if (context->owner.active) {
        candidates[count++]=context->body;
        candidates[count++]=context->headers;
        candidates[count++]=context->owner;
    } else if (context->primary.active && g->row>=context->primary.r0 && g->row<=context->primary.r1 &&
               g->col>=context->primary.c0 && g->col<=context->primary.c1) {
        candidates[count++]=context->primary;
    } else if (g->selection==SELECT_CELLS && context->cells) {
        GridRegion region={0}; bounds(g,&region.r0,&region.r1,&region.c0,&region.c1);
        if (cells_containing_region(context->cells,&region.r0,&region.r1,&region.c0,&region.c1)) {
            region.active=true; candidates[count++]=region;
        }
    }
    candidates[count++]=context->used;
    grid_select_next_region(g,candidates,count);
    if (context->owner.active && g->selection==SELECT_CELLS) {
        uint64_t r0,r1; size_t c0,c1; bounds(g,&r0,&r1,&c0,&c1);
        GridRegion owner=context->owner;
        result.within_owner=r0>=owner.r0 && r1<=owner.r1 && c0>=owner.c0 && c1<=owner.c1;
        result.whole_owner=r0==owner.r0 && r1==owner.r1 && c0==owner.c0 && c1==owner.c1;
    }
    return result;
}
