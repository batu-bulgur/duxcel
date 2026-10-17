#include "grid/selection_ladder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL selection ladder:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Grid fixture(void) {
    Grid g={0};
    g.selection=SELECT_CELLS; g.row=g.anchor_row=g.end_row=16; g.col=g.anchor_col=g.end_col=2;
    g.top=11; g.horizontal=17.25f; g.fraction=.375; g.zoom=1.25f; g.row_height=27;
    g.used_rows=20; g.used_columns=11; g.width_count=52; g.width_revision=19;
    g.rect=(Rectangle){228,165,772,633}; g.body=(Rectangle){268,213,720,573};
    g.elastic=(Vector2){-2,3}; g.spring_velocity=(Vector2){4,-5};
    g.navigation=(GridRegion){14,19,1,3,true}; g.visible_rows=21; g.render_rows=24;
    g.dragging=true; g.context_requested=true; g.clock=12.5; g.cursor=7;
    return g;
}
static void step(Grid *g,const GridRegion *candidates,size_t count,SelectionMode mode,
    uint64_t ar,uint64_t er,size_t ac,size_t ec) {
    Grid before; memcpy(&before,g,sizeof(before));
    grid_select_next_region(g,candidates,count);
    CHECK(g->selection==mode); CHECK(g->anchor_row==ar && g->end_row==er);
    CHECK(g->anchor_col==ac && g->end_col==ec); CHECK(g->row==before.row && g->col==before.col);
    Grid untouched; memcpy(&untouched,g,sizeof(untouched));
    untouched.selection=before.selection; untouched.anchor_row=before.anchor_row; untouched.end_row=before.end_row;
    untouched.anchor_col=before.anchor_col; untouched.end_col=before.end_col;
    CHECK(!memcmp(&untouched,&before,sizeof(before)));
}
static void progressive(void) {
    /* Deliberately shuffled; metadata defines the ladder, not input order. */
    const GridRegion regions[]={{0,19,0,10,true},{12,19,1,3,true},{14,19,1,3,true},{13,19,1,3,true}};
    Grid g=fixture();
    step(&g,regions,4,SELECT_CELLS,14,19,1,3);
    step(&g,regions,4,SELECT_CELLS,13,19,1,3);
    step(&g,regions,4,SELECT_CELLS,12,19,1,3);
    step(&g,regions,4,SELECT_CELLS,0,19,0,10);
    step(&g,regions,4,SELECT_ALL,0,19,0,10);
    step(&g,regions,4,SELECT_ALL,0,19,0,10);
    g=fixture(); g.anchor_row=17; g.end_row=16; g.row=17; g.anchor_col=3; g.end_col=2; g.col=3;
    step(&g,regions,4,SELECT_CELLS,19,14,3,1);
    step(&g,regions,4,SELECT_CELLS,19,13,3,1);
    step(&g,regions,4,SELECT_CELLS,19,12,3,1);
    step(&g,regions,4,SELECT_CELLS,19,0,10,0);
    step(&g,regions,4,SELECT_ALL,19,0,10,0);
    g=fixture(); g.anchor_row=17; g.end_row=16; g.row=17;
    step(&g,regions,4,SELECT_CELLS,19,14,1,3);
    g=fixture(); g.end_row=17; g.anchor_col=3; g.end_col=2; g.col=3;
    step(&g,regions,4,SELECT_CELLS,14,19,3,1);
    /* A cross-owner selection cannot shrink to the active owner's region. */
    g=fixture(); g.anchor_row=4; g.anchor_col=0;
    step(&g,regions,4,SELECT_CELLS,0,19,0,10);
}
static void candidates_and_guards(void) {
    Grid g=fixture(); g.anchor_row=14; g.end_row=19; g.anchor_col=1; g.end_col=3;
    const GridRegion regions[]={{13,19,1,3,false},{0,DX_SHEET_ROWS,0,3,true},
        {20,19,1,3,true},{13,19,4,3,true},{13,19,2,3,true},
        {14,19,1,3,true},{12,19,1,3,true},{13,19,1,3,true}};
    step(&g,regions,8,SELECT_CELLS,13,19,1,3);
    const GridRegion duplicates[]={{13,19,1,3,true},{12,19,1,3,true},{13,19,1,3,true},{12,19,1,3,true}};
    step(&g,duplicates,4,SELECT_CELLS,12,19,1,3);
    step(&g,duplicates,4,SELECT_ALL,12,19,1,3);
    const GridRegion invalid_columns[]={{0,19,0,16384,true}};
    g=fixture(); step(&g,invalid_columns,1,SELECT_ALL,16,16,2,2);
    g=fixture(); step(&g,NULL,0,SELECT_ALL,16,16,2,2);
    g=fixture(); step(&g,NULL,1,SELECT_CELLS,16,16,2,2);
    /* Reject a count above the bound without reading beyond this one item. */
    const GridRegion one=regions[0];
    step(&g,&one,GRID_SELECTION_CANDIDATES_MAX+1,SELECT_CELLS,16,16,2,2);
    g.row=DX_SHEET_ROWS; step(&g,regions,8,SELECT_CELLS,16,16,2,2);
    g=fixture(); g.col=16384; step(&g,regions,8,SELECT_CELLS,16,16,2,2);
    g=fixture(); g.anchor_row=DX_SHEET_ROWS; step(&g,regions,8,SELECT_CELLS,DX_SHEET_ROWS,16,2,2);
    g=fixture(); g.end_col=16384; step(&g,regions,8,SELECT_CELLS,16,16,2,16384);
    g=fixture(); g.row=15; step(&g,regions,8,SELECT_CELLS,16,16,2,2);
    g=fixture(); g.selection=(SelectionMode)99; step(&g,regions,8,(SelectionMode)99,16,16,2,2);
    grid_select_next_region(NULL,NULL,0);
}
static void terminal_and_ties(void) {
    const GridRegion used={0,19,0,10,true}; Grid g=fixture();
    g.row=g.anchor_row=g.end_row=99; g.col=g.anchor_col=g.end_col=25;
    step(&g,&used,1,SELECT_ALL,99,99,25,25);
    for (int mode=SELECT_ROWS;mode<=SELECT_ALL;mode++) {
        g=fixture(); g.selection=(SelectionMode)mode; g.anchor_row=17; g.end_row=16;
        step(&g,&used,1,SELECT_ALL,17,16,2,2);
    }
    const GridRegion entire={0,DX_SHEET_ROWS-1,0,16383,true};
    g=fixture(); g.row=g.anchor_row=g.end_row=DX_SHEET_ROWS-1; g.col=g.anchor_col=g.end_col=16383;
    step(&g,&entire,1,SELECT_ALL,DX_SHEET_ROWS-1,DX_SHEET_ROWS-1,16383,16383);
    /* Same-area overlapping candidates have a stable coordinate tie-break. */
    const GridRegion ties[]={{15,17,0,2,true},{14,16,1,3,true}};
    g=fixture(); step(&g,ties,2,SELECT_CELLS,14,16,1,3);
    const GridRegion reversed[]={ties[1],ties[0]};
    g=fixture(); step(&g,reversed,2,SELECT_CELLS,14,16,1,3);
}
int main(void) {
    progressive(); candidates_and_guards(); terminal_and_ties();
    printf("PASS selection ladder: %u checks\n",checks); return 0;
}
