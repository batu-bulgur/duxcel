#include "grid/go_to.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Go To:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool same(GridLocation a,GridLocation b) { return a.row==b.row && a.anchor_row==b.anchor_row && a.end_row==b.end_row && a.top==b.top && a.col==b.col && a.anchor_col==b.anchor_col && a.end_col==b.end_col && a.horizontal==b.horizontal && a.fraction==b.fraction && a.selection==b.selection; }
static void parsing(void) {
    struct { const char *text; bool header; uint64_t row; size_t column; } good[]={ {"A1",false,0,0},{" $h$12 \t",false,11,7},{"A2",true,0,0},{"XFD1099511627776",false,DX_SHEET_ROWS-1,16383},{"XFD1099511627777",true,DX_SHEET_ROWS-1,16383},{"a00001",false,0,0} };
    for (size_t i=0;i<sizeof(good)/sizeof(*good);i++) { GridGoToTarget t={77,77}; CHECK(grid_go_to_parse(good[i].text,strlen(good[i].text),good[i].header,&t)==GRID_GO_TO_READY && t.row==good[i].row && t.column==good[i].column); char formatted[48]; grid_format_address(t.row,t.column,good[i].header,formatted); CHECK(grid_go_to_parse(formatted,strlen(formatted),good[i].header,&t)==GRID_GO_TO_READY && t.row==good[i].row && t.column==good[i].column); }
    const char *bad[]={"","A0","XFE1","A1099511627777","A1:B2","Sheet1!A1","=A1","A","1A","[book]A1","Ａ1","A-1","A1 tail"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) { GridGoToTarget t={77,77}; CHECK(grid_go_to_parse(bad[i],strlen(bad[i]),false,&t)==GRID_GO_TO_INVALID && t.row==77 && t.column==77); }
    GridGoToTarget t={77,77}; CHECK(grid_go_to_parse("A1",2,true,&t)==GRID_GO_TO_INVALID && t.row==77);
    char embedded[]={'A','1',0,'B'}; CHECK(grid_go_to_parse(embedded,sizeof(embedded),false,&t)==GRID_GO_TO_INVALID && t.row==77);
    char long_text[GRID_GO_TO_ADDRESS_BYTES+1]; memset(long_text,' ',sizeof(long_text)); long_text[0]='A'; long_text[1]='1'; CHECK(grid_go_to_parse(long_text,sizeof(long_text),false,&t)==GRID_GO_TO_LIMIT && t.row==77);
    CHECK(grid_go_to_parse(NULL,0,false,&t)==GRID_GO_TO_INVALID && t.row==77);
}
int main(void) {
    parsing(); Grid g; grid_init(&g); Page page={0}; grid_ensure_columns(&g,52); grid_layout(&g,1,&page,(Rectangle){0,0,640,400});
    grid_set_width(&g,0,173); g.column_names[0]=dx_strdup("Preserved"); char *name=g.column_names[0]; uint64_t revision=g.width_revision;
    GridGoToTarget target={1000000,16383};
    for (unsigned failure=1;failure<=3;failure++) {
        GridLocation before=grid_location(&g); float *widths=g.widths,*offsets=g.offsets; char **names=g.column_names;
        grid_test_fail_column_allocation(failure); CHECK(grid_go_to_apply(&g,&page,1,target)==GRID_GO_TO_LIMIT); grid_test_fail_column_allocation(0);
        CHECK(same(before,grid_location(&g)) && g.widths==widths && g.offsets==offsets && g.column_names==names && g.column_names[0]==name && g.widths[0]==173 && g.width_revision==revision);
    }
    CHECK(grid_go_to_apply(&g,&page,1,target)==GRID_GO_TO_APPLIED && g.row==target.row && g.col==target.column && g.anchor_row==g.end_row && g.anchor_col==g.end_col && g.width_count==16384);
    CHECK(g.column_names[0]==name && g.widths[0]==173 && g.width_revision==revision && g.top<=g.row && g.row<g.top+g.visible_rows);
    CHECK(g.offsets[g.col]>=g.horizontal && g.offsets[g.col+1]-g.horizontal<=g.body.width+.001f);
    for (size_t i=52;i<g.width_count;i++) CHECK(g.widths[i]==96 && !g.column_names[i]);
    CHECK(grid_go_to_apply(&g,&page,1,target)==GRID_GO_TO_NOOP);
    g.top=0; g.horizontal=0; CHECK(grid_go_to_apply(&g,&page,1,target)==GRID_GO_TO_APPLIED && g.top>0 && g.horizontal>0);
    GridLocation before=grid_location(&g); CHECK(grid_go_to_apply(&g,&page,NAN,(GridGoToTarget){0,0})==GRID_GO_TO_INVALID && same(before,grid_location(&g)));
    CHECK(grid_go_to_apply(&g,&page,1,(GridGoToTarget){DX_SHEET_ROWS,0})==GRID_GO_TO_INVALID && same(before,grid_location(&g)));
    g.dragging=true; CHECK(grid_go_to_apply(&g,&page,1,(GridGoToTarget){0,0})==GRID_GO_TO_PROTECTED && same(before,grid_location(&g))); g.dragging=false;
    g.bounded=true; CHECK(grid_go_to_apply(&g,&page,1,(GridGoToTarget){0,0})==GRID_GO_TO_PROTECTED && same(before,grid_location(&g))); g.bounded=false;
    g.selection=SELECT_ROWS; CHECK(grid_go_to_apply(&g,&page,1,(GridGoToTarget){0,0})==GRID_GO_TO_PROTECTED); g.selection=SELECT_CELLS;
    CHECK(grid_go_to_apply(&g,&page,1,(GridGoToTarget){0,0})==GRID_GO_TO_APPLIED && !g.row && !g.col && !g.top && !g.horizontal);
    grid_destroy(&g); printf("PASS: %u bounded same-sheet Go To controller checks\n",checks); return 0;
}
