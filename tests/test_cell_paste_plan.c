#include "workbook/cell_paste_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL paste plan:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void parity(const char *text,uint64_t row,size_t col) {
    CellStore s={0},actual={0}; CellSnapshot out={0}; CellPastePlanInfo info={0}; char *error=NULL;
    CHECK(cells_set(&s,0,0,"Keep",&error) && cells_set(&s,2,3,"Clear",&error) && cells_set(&s,3,4,"Ragged",&error));
    cells_copy(&actual,&s); CellStore before=s; size_t rows=0,columns=0;
    bool accepted=cells_paste(&actual,row,col,text,&rows,&columns,&error); free(error); error=NULL;
    CellRangeStatus status=cells_paste_measure(&s,row,col,text,&info,&error);
    if (!accepted) CHECK(status==CELL_RANGE_INVALID || status==CELL_RANGE_LIMIT);
    else if (status==CELL_RANGE_NOOP) CHECK(actual.count==s.count && actual.bytes==s.bytes);
    else {
        CHECK(status==CELL_RANGE_READY && info.rows==rows && info.columns==columns && info.count==actual.count && info.bytes==actual.bytes);
        CHECK(cells_paste_plan(&s,row,col,text,info.owned-1,&out,NULL)==CELL_RANGE_LIMIT && !out.cells);
        CHECK(cells_paste_plan(&s,row,col,text,info.owned,&out,NULL)==CELL_RANGE_READY && out.count==actual.count);
        for (size_t i=0;i<out.count;i++) CHECK(out.cells[i].row==actual.cells[i].row && out.cells[i].col==actual.cells[i].col && !strcmp(out.cells[i].text,actual.cells[i].text) && out.cells[i].text!=actual.cells[i].text);
    }
    CHECK(!memcmp(&before,&s,sizeof(s))); cells_snapshot_free(&out); cells_free(&s); cells_free(&actual); free(error);
}
int main(void) {
    const char *inputs[]={"", "NULL\t\"\"\t9007199254740993.0123456789\r\nİstanbul", "\"a\t\"\"b\"\n\"line\ntwo\"\n", "a\t\n\n", "\"unterminated", "\"closed\"bad"};
    for (size_t i=0;i<sizeof(inputs)/sizeof(*inputs);i++) parity(inputs[i],2,3);
    parity("A\tB",DX_SHEET_ROWS-1,16383);
    CellStore s={0}; CellSnapshot out={0}; CellPastePlanInfo info={.owned=999};
    CHECK(cells_paste_measure(&s,0,0,"",&info,NULL)==CELL_RANGE_NOOP && info.owned==999);
    CHECK(cells_paste_plan(&s,0,0,"",0,&out,NULL)==CELL_RANGE_NOOP && !out.cells);
    CHECK(cells_set(&s,0,0,"NULL",NULL)); uint64_t revision=s.revision; size_t undo=s.undo_count;
    CHECK(cells_paste_measure(&s,0,0,"NULL",&info,NULL)==CELL_RANGE_NOOP && info.owned==999 && s.revision==revision && s.undo_count==undo);
    CHECK(cells_paste_plan(&s,0,0,"",0,&out,NULL)==CELL_RANGE_READY && !out.count && !out.cells && !strcmp(cells_get(&s,0,0),"NULL"));
    CHECK(cells_paste_plan(&s,0,0,"X",SIZE_MAX,&out,NULL)==CELL_RANGE_READY); CellSnapshot before=out;
    CHECK(cells_paste_plan(&s,0,0,"\"bad",SIZE_MAX,&out,NULL)==CELL_RANGE_INVALID && !memcmp(&out,&before,sizeof(out)));
    CHECK(cells_paste_plan(&s,0,0,"Y",0,&out,NULL)==CELL_RANGE_LIMIT && !memcmp(&out,&before,sizeof(out)));
    s.bytes++; CHECK(cells_paste_measure(&s,0,0,"X",&info,NULL)==CELL_RANGE_INVALID); s.bytes--;
    cells_snapshot_free(&out); cells_free(&s);
    s.count=s.capacity=DX_MAX_EDIT_CELLS; s.cells=dx_alloc(s.count*sizeof(*s.cells));
    for (size_t i=0;i<s.count;i++) { s.cells[i]=(SheetCell){i+1,0,dx_strdup("X")}; s.bytes+=2; }
    CellStore full=s; info.owned=777;
    CHECK(cells_paste_measure(&s,0,0,"Extra",&info,NULL)==CELL_RANGE_LIMIT && info.owned==777 && !memcmp(&full,&s,sizeof(s)));
    CHECK(cells_paste_measure(&s,1,0,"X",&info,NULL)==CELL_RANGE_NOOP && info.owned==777);
    cells_free(&s);
    char *large=dx_alloc(65537); memset(large,'A',65536); large[65536]=0;
    s.count=s.capacity=63; s.cells=dx_alloc(s.count*sizeof(*s.cells));
    for (size_t i=0;i<s.count;i++) { s.cells[i]=(SheetCell){i+1,0,dx_strdup(large)}; s.bytes+=65537; }
    full=s;
    CHECK(cells_paste_measure(&s,0,0,large,&info,NULL)==CELL_RANGE_LIMIT && info.owned==777 && !memcmp(&full,&s,sizeof(s)));
    cells_free(&s); free(large); printf("PASS: %u complete prospective paste checks\n",checks); return 0;
}
