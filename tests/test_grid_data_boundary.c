#include "grid/grid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL grid boundary:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { GridBoundaryCell *cells; size_t count; bool fail; } Cells;
static bool read_cell(const void *ctx,size_t index,GridBoundaryCell *out) { const Cells *c=ctx; if (c->fail || index>=c->count) return false; *out=c->cells[index]; return true; }
static void key(Grid *g,UIContext *u,Page *p,int key,bool shift) { grid_handle_input(g,u,p,&(GridInput){.mouse={-10000,-10000},.key=key,.separate_keyboard=true,.key_shortcut=true,.key_shift=shift,.dt=.016f}); }
static void prepared(Grid *g,UIContext *u,Page *p,Cells *cells) {
    grid_init(g); grid_ensure_columns(g,52); g->chrome_scale=1; g->digit_width=7; u->scale=1; grid_layout(g,1,p,(Rectangle){0,0,640,400});
    g->manual_occupancy=(GridBoundaryOccupancy){read_cell,cells,cells->count,GRID_BOUNDARY_CELLS_MAX}; g->manual_occupancy_active=true;
}
int main(void) {
    GridBoundaryCell entries[]={{5,2,true},{6,2,true},{7,2,true},{20,2,true},{21,2,true},{30,0,true},{30,1,true},{30,3,false},{30,4,true}}; Cells cells={entries,9,false};
    UIContext ui={0}; Page page={0}; Grid grid; prepared(&grid,&ui,&page,&cells); grid_select_cell(&grid,5,2,false);
    key(&grid,&ui,&page,KEY_DOWN,false); CHECK(grid.row==7 && grid.anchor_row==7 && grid.end_row==7);
    key(&grid,&ui,&page,KEY_DOWN,false); CHECK(grid.row==20); key(&grid,&ui,&page,KEY_DOWN,true); CHECK(grid.row==20 && grid.anchor_row==20 && grid.end_row==21);
    key(&grid,&ui,&page,KEY_UP,true); CHECK(grid.anchor_row==20 && grid.end_row==20); key(&grid,&ui,&page,KEY_UP,true); CHECK(grid.anchor_row==20 && grid.end_row==7 && grid.row==20);
    grid_select_cell(&grid,30,0,false); key(&grid,&ui,&page,KEY_RIGHT,false); CHECK(grid.col==1); key(&grid,&ui,&page,KEY_RIGHT,false); CHECK(grid.col==4);
    grid_set_width(&grid,0,171); free(grid.column_names[0]); grid.column_names[0]=dx_strdup("Authored name");
    uint64_t width_revision=grid.width_revision; float first=grid.widths[0]; char *name=grid.column_names[0];
    for (unsigned failure=1;failure<=3;failure++) {
        grid_select_cell(&grid,30,4,false); GridLocation before=grid_location(&grid); float *widths=grid.widths,*offsets=grid.offsets; char **names=grid.column_names; size_t count=grid.width_count;
        grid_test_fail_column_allocation(failure); key(&grid,&ui,&page,KEY_RIGHT,false); grid_test_fail_column_allocation(0);
        CHECK(grid.row==before.row && grid.col==before.col && grid.anchor_col==before.anchor_col && grid.end_col==before.end_col && grid.top==before.top);
        CHECK(grid.widths==widths && grid.offsets==offsets && grid.column_names==names && grid.width_count==count && grid.widths[0]==first && grid.column_names[0]==name && grid.width_revision==width_revision);
    }
    key(&grid,&ui,&page,KEY_RIGHT,false); CHECK(grid.col==GRID_BOUNDARY_COLUMNS-1 && grid.width_count==GRID_BOUNDARY_COLUMNS);
    CHECK(grid.widths[0]==171 && grid.column_names[0]==name && grid.width_revision==width_revision);
    for (size_t i=52;i<grid.width_count;i++) CHECK(grid.widths[i]==96 && !grid.column_names[i]);
    GridLocation edge=grid_location(&grid); key(&grid,&ui,&page,KEY_RIGHT,true); CHECK(grid.col==edge.col && grid.end_col==edge.end_col);
    grid_select_cell(&grid,5,2,false); cells.fail=true; GridLocation old=grid_location(&grid); key(&grid,&ui,&page,KEY_DOWN,false); CHECK(grid.row==old.row && grid.end_row==old.end_row); cells.fail=false;
    grid.manual_occupancy_active=false; grid.navigation=(GridRegion){4,25,1,8,true}; key(&grid,&ui,&page,KEY_DOWN,false); CHECK(grid.row==25);
    grid.manual_occupancy_active=true; grid.selection=SELECT_ROWS; grid.anchor_row=grid.row=5; grid.end_row=6; key(&grid,&ui,&page,KEY_DOWN,true); CHECK(grid.end_row==25 && grid.selection==SELECT_ROWS);
    grid.selection=SELECT_CELLS; grid.navigation=(GridRegion){0}; grid_select_cell(&grid,0,9,false); key(&grid,&ui,&page,KEY_DOWN,false); CHECK(grid.row==DX_SHEET_ROWS-1);
    CHECK(!grid_try_ensure_columns(&grid,GRID_BOUNDARY_COLUMNS+1) && grid.widths[0]==171 && grid.column_names[0]==name);
    grid_destroy(&grid); printf("PASS: %u checked grid data-boundary input/capacity checks\n",checks); return 0;
}
