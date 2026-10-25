#include "workbook/cell_range_plan.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell range:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool plan(CellStore *s,bool replace,int64_t dr,int dc,CellSnapshot *out) { return cells_range_plan(s,1,3,1,3,dr,dc,replace,SIZE_MAX,out,NULL)==CELL_RANGE_READY; }
static void columns(void) {
    CellStore s={0}; CellSnapshot out={0}; CellRangePlanInfo info={0};
    CHECK(cells_set(&s,0,0,"NULL",NULL) && cells_set(&s,0,2,"9007199254740993.0123456789",NULL) && cells_set(&s,DX_SHEET_ROWS-1,2,"İstanbul\tline\ntwo",NULL));
    CellStore before=s;
    CHECK(cells_columns_measure(&s,1,2,true,&info,NULL)==CELL_RANGE_READY && info.count==3);
    CHECK(cells_columns_plan(&s,1,2,true,info.owned-1,&out,NULL)==CELL_RANGE_LIMIT && !out.cells && !memcmp(&s,&before,sizeof(s)));
    CHECK(cells_columns_plan(&s,1,2,true,info.owned,&out,NULL)==CELL_RANGE_READY && out.cells[0].col==0 && out.cells[1].col==4 && out.cells[2].col==4);
    CHECK(out.cells[1].text!=s.cells[1].text && !strcmp(out.cells[1].text,s.cells[1].text) && !strcmp(out.cells[2].text,"İstanbul\tline\ntwo"));
    cells_adopt_range(&s,&out); CHECK(!*cells_get(&s,0,2) && !strcmp(cells_get(&s,0,4),"9007199254740993.0123456789"));
    CHECK(cells_history(&s,false) && !strcmp(cells_get(&s,0,2),"9007199254740993.0123456789"));
    CHECK(cells_columns_plan(&s,1,2,false,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && out.count==1 && !strcmp(out.cells[0].text,"NULL")); cells_snapshot_free(&out);
    CHECK(cells_columns_plan(&s,0,16384,false,0,&out,NULL)==CELL_RANGE_READY && !out.count && !out.cells);
    info.owned=999; CHECK(cells_columns_measure(&s,3,1,true,&info,NULL)==CELL_RANGE_NOOP && info.owned==999);
    CHECK(cells_columns_measure(&s,16384,1,true,&info,NULL)==CELL_RANGE_INVALID && cells_columns_measure(&s,0,0,true,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_columns_measure(&s,16383,SIZE_MAX,false,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_set(&s,1,16383,"Right edge",NULL));
    CHECK(cells_columns_measure(&s,0,1,true,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_columns_plan(&s,16383,1,false,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && out.count==3);
    cells_snapshot_free(&out); cells_free(&s);
}
int main(void) {
    columns(); CellStore s={0}; char *error=NULL; CHECK(cells_set(&s,1,1,"NULL",&error) && cells_set(&s,3,3,"12345678901234567890.123456789",&error) && cells_set(&s,10,10,"Occupied",&error));
    CellRangePlanInfo info={.owned=99}; CellStore before=s; CellSnapshot out={0};
    CHECK(cells_range_measure(&s,1,3,1,3,0,0,false,&info,NULL)==CELL_RANGE_NOOP && info.owned==99 && !memcmp(&before,&s,sizeof(s)));
    CHECK(cells_range_measure(&s,1,3,1,3,9,9,false,&info,NULL)==CELL_RANGE_OCCUPIED && info.owned==99);
    CHECK(cells_range_plan(&s,1,3,1,3,9,9,true,0,&out,NULL)==CELL_RANGE_LIMIT && !out.cells && !memcmp(&before,&s,sizeof(s)));
    CHECK(plan(&s,true,9,9,&out) && out.count==2 && out.cells[0].row==10 && out.cells[1].row==12 && !strcmp(out.cells[0].text,"NULL") && strcmp(out.cells[0].text,"Occupied") && out.cells[0].text!=s.cells[0].text);
    CHECK(!memcmp(&before,&s,sizeof(s))); cells_adopt_range(&s,&out); CHECK(s.count==2 && !*cells_get(&s,1,1) && !strcmp(cells_get(&s,12,12),"12345678901234567890.123456789"));
    CHECK(cells_history(&s,false) && !strcmp(cells_get(&s,10,10),"Occupied") && !strcmp(cells_get(&s,1,1),"NULL"));
    CHECK(cells_history(&s,true) && !strcmp(cells_get(&s,10,10),"NULL")); cells_free(&s);
    CHECK(cells_set(&s,1,1,"A",&error) && cells_set(&s,2,2,"B",&error) && cells_set(&s,3,3,"C",&error));
    CHECK(plan(&s,true,1,1,&out) && out.count==3 && out.cells[0].row==2 && !strcmp(out.cells[0].text,"A")); cells_adopt_range(&s,&out);
    CHECK(cells_history(&s,false) && plan(&s,true,-1,-1,&out) && !out.cells[0].row && !out.cells[0].col); cells_snapshot_free(&out);
    CHECK(cells_range_measure(&s,1,3,1,3,INT64_MIN,0,true,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_range_measure(&s,1,3,1,3,0,INT_MIN,true,&info,NULL)==CELL_RANGE_INVALID);
    s.bytes++; CHECK(cells_range_measure(&s,1,3,1,3,1,1,true,&info,NULL)==CELL_RANGE_INVALID); s.bytes--;
    cells_free(&s);
    CHECK(cells_set(&s,50,50,"Destination",&error));
    CHECK(cells_range_plan(&s,1,1,1,1,49,49,true,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && !out.count && !out.cells); cells_adopt_range(&s,&out); CHECK(!s.count);
    CHECK(cells_history(&s,false) && !strcmp(cells_get(&s,50,50),"Destination")); cells_free(&s);
    s.count=s.capacity=DX_MAX_EDIT_CELLS; s.cells=dx_alloc(s.count*sizeof(*s.cells));
    for (size_t i=0;i<s.count;i++) { s.cells[i]=(SheetCell){i,0,dx_strdup("X")}; s.bytes+=2; }
    CHECK(cells_range_measure(&s,0,DX_SHEET_ROWS-2,0,16382,1,1,false,&info,NULL)==CELL_RANGE_READY && info.count==DX_MAX_EDIT_CELLS);
    CHECK(cells_range_plan(&s,0,DX_SHEET_ROWS-2,0,16382,1,1,false,info.owned,&out,NULL)==CELL_RANGE_READY && out.count==DX_MAX_EDIT_CELLS && out.cells[out.count-1].row==DX_MAX_EDIT_CELLS);
    cells_snapshot_free(&out); cells_free(&s);
    CHECK(cells_set(&s,1,1,"NULL",&error) && cells_set(&s,3,3,"Exact",&error));
    CHECK(cells_rows_plan(&s,2,2,true,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && out.cells[0].row==1 && out.cells[1].row==5); cells_snapshot_free(&out);
    CHECK(cells_rows_plan(&s,1,3,false,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && !out.count); cells_snapshot_free(&out);
    CHECK(cells_rows_measure(&s,10,3,true,&info,NULL)==CELL_RANGE_NOOP && cells_rows_measure(&s,1,0,true,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_set(&s,DX_SHEET_ROWS-1,1,"Boundary",&error));
    CHECK(cells_rows_measure(&s,DX_SHEET_ROWS-1,1,true,&info,NULL)==CELL_RANGE_INVALID);
    CHECK(cells_rows_plan(&s,DX_SHEET_ROWS-1,1,false,SIZE_MAX,&out,NULL)==CELL_RANGE_READY && out.count==2); cells_snapshot_free(&out);
    cells_free(&s); free(error); printf("PASS: %u sparse cell range checks\n",checks); return 0;
}
