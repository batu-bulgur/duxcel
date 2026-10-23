#include "workbook/cells.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell regions:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void add(CellStore *s,uint64_t row,size_t col,const char *value) {
    char *error=NULL; CHECK(cells_set(s,row,col,value,&error)); free(error);
}
static void region(CellStore *s,uint64_t r0,uint64_t r1,size_t c0,size_t c1,bool expanded,
    uint64_t er0,uint64_t er1,size_t ec0,size_t ec1) {
    CellStore before=*s;
    CHECK(cells_containing_region(s,&r0,&r1,&c0,&c1)==expanded);
    CHECK(r0==er0 && r1==er1 && c0==ec0 && c1==ec1);
    CHECK(!memcmp(&before,s,sizeof(before))); /* Borrowed model/history never changes. */
}
static void shapes(void) {
    CellStore s={0};
    /* L-shaped component; another island sits inside its bounding rectangle
       and extends below it. Bounding growth used to absorb both in one step. */
    for (size_t c=1;c<=4;c++) add(&s,1,c,"Header");
    for (uint64_t r=2;r<=4;r++) add(&s,r,4,r==2 ? "NULL" : "12345678901234567890.123456789");
    for (uint64_t r=4;r<=6;r++) add(&s,r,1,"Separate island");
    add(&s,5,5,"Diagonal"); add(&s,6,6,"Another diagonal"); add(&s,20000,25,"Offscreen");
    region(&s,1,1,1,1,true,1,4,1,4);
    region(&s,1,4,1,4,true,1,6,1,4);
    region(&s,1,6,1,4,false,1,6,1,4);
    region(&s,2,4,4,4,true,1,4,1,4); /* Entire finite selection is the seed. */
    region(&s,3,5,1,5,true,1,6,1,5); /* Multiple directly selected components. */
    region(&s,2,2,2,2,true,1,4,1,4); /* Blank interior uses enclosing bounds. */
    region(&s,5,5,5,5,false,5,5,5,5); /* Diagonals never connect. */
    region(&s,20,22,20,23,false,20,22,20,23);
    region(&s,19999,20000,25,25,false,19999,20000,25,25); /* Never shrink a seed. */
    region(&s,20000,20000,24,25,false,20000,20000,24,25);
    uint64_t r0=0,r1=0; size_t c0=0,c1=0;
    cells_region(&s,100000,90,&r0,&r1,&c0,&c1,true);
    CHECK(r0==1 && r1==20000 && c0==1 && c1==25);
    cells_free(&s);
    /* A ring encloses a real hole; a literal NULL connects like any text. */
    for (uint64_t r=0;r<3;r++) for (size_t c=0;c<3;c++) if (r!=1 || c!=1) add(&s,r,c,"NULL");
    region(&s,1,1,1,1,true,0,2,0,2);
    add(&s,1,1,""); region(&s,1,1,1,1,true,0,2,0,2);
    region(&s,0,2,0,2,false,0,2,0,2); cells_free(&s);
    /* Removing a bridge splits the component immediately, including after Undo. */
    add(&s,0,0,"A"); add(&s,0,1,"Bridge"); add(&s,0,2,"B");
    region(&s,0,0,0,0,true,0,0,0,2); add(&s,0,1,"");
    region(&s,0,0,0,0,false,0,0,0,0); CHECK(cells_history(&s,false));
    region(&s,0,0,0,0,true,0,0,0,2); cells_free(&s);
    add(&s,DX_SHEET_ROWS-1,16383,"Edge"); add(&s,DX_SHEET_ROWS-2,16383,"Neighbor");
    region(&s,DX_SHEET_ROWS-1,DX_SHEET_ROWS-1,16383,16383,true,DX_SHEET_ROWS-2,DX_SHEET_ROWS-1,16383,16383);
    region(&s,0,DX_SHEET_ROWS,0,0,false,0,DX_SHEET_ROWS,0,0); cells_free(&s);
}
static void blank_ties(void) {
    /* Two disconnected components have overlapping 18-cell bounding boxes.
       The blank seed belongs to both boxes: coordinate order resolves the tie,
       independently of the order in which the owned cells were inserted. */
    const size_t coordinates[][2]={{0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{1,5},{2,5},
        {2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{7,2},{7,3}};
    for (unsigned reverse=0;reverse<2;reverse++) {
        CellStore s={0}; size_t count=sizeof(coordinates)/sizeof(*coordinates);
        for (size_t i=0;i<count;i++) { size_t j=reverse ? count-1-i : i; add(&s,coordinates[j][0],coordinates[j][1],"v"); }
        region(&s,2,2,2,2,true,0,2,0,5); cells_free(&s);
    }
}
static void scale_and_guards(void) {
    CellStore s={0}; region(&s,0,0,0,0,false,0,0,0,0);
    CHECK(!cells_containing_region(NULL,NULL,NULL,NULL,NULL));
    /* The maximum admitted sparse store, built as owned finite test state,
       avoids manufacturing 20,000 unrelated Undo actions just to stress reads. */
    s.count=s.capacity=DX_MAX_EDIT_CELLS; s.bytes=2*s.count; s.cells=dx_alloc(s.count*sizeof(*s.cells));
    for (size_t i=0;i<s.count;i++) s.cells[i]=(SheetCell){i,5,dx_strdup("v")};
    region(&s,100,101,5,5,true,0,DX_MAX_EDIT_CELLS-1,5,5);
    region(&s,0,DX_MAX_EDIT_CELLS-1,5,5,false,0,DX_MAX_EDIT_CELLS-1,5,5);
    CHECK(!strcmp(s.cells[0].text,"v") && !strcmp(s.cells[s.count-1].text,"v"));
    cells_free(&s);
}
int main(void) {
    shapes(); blank_ties(); scale_and_guards();
    printf("PASS: %u sparse containing-region checks\n",checks); return 0;
}
