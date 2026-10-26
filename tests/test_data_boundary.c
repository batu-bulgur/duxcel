#include "grid/data_boundary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL data boundary:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { const GridBoundaryCell *cells; size_t count,reads,fail; } Inventory;
static bool read_cell(const void *context,size_t index,GridBoundaryCell *out) { Inventory *i=(Inventory *)context; i->reads++; if (index>=i->count || index==i->fail) return false; *out=i->cells[index]; return true; }
static GridBoundarySelection point(uint64_t row,size_t column) { return (GridBoundarySelection){row,row,row,column,column,column}; }
static GridBoundaryOccupancy inventory(Inventory *i) { return (GridBoundaryOccupancy){read_cell,i,i->count,GRID_BOUNDARY_CELLS_MAX}; }
static void target(const GridBoundaryCell *cells,size_t count,uint64_t row,size_t col,GridBoundaryDirection dir,uint64_t r,size_t c) {
    Inventory i={cells,count,0,SIZE_MAX}; GridBoundarySelection before=point(row,col),after=point(77,77); GridBoundaryOccupancy o=inventory(&i);
    GridBoundaryStatus status=grid_data_boundary(&before,&o,dir,false,&after);
    CHECK(i.reads==count); CHECK(status==(row==r && col==c ? GRID_BOUNDARY_NOOP : GRID_BOUNDARY_APPLIED));
    if (status==GRID_BOUNDARY_APPLIED) CHECK(after.row==r && after.column==c && after.anchor_row==r && after.anchor_column==c && after.end_row==r && after.end_column==c);
    else CHECK(after.row==77 && after.column==77);
}
static void islands(void) {
    /* Presence is supplied independently from text/type. Three adjacent present
       entries can therefore represent SQLNULL, empty TEXT and a number alike. */
    const GridBoundaryCell cells[]={{0,3,true},{1,3,true},{2,3,true},{4,3,false},{5,3,true},{6,3,true},{10,0,true},{10,1,true},{10,2,true},{10,4,false},{10,5,true},{10,6,true},{11,3,true},{DX_SHEET_ROWS-1,3,true}};
    target(cells,14,1,3,GRID_BOUNDARY_UP,0,3); target(cells,14,1,3,GRID_BOUNDARY_DOWN,2,3);
    target(cells,14,2,3,GRID_BOUNDARY_DOWN,5,3); target(cells,14,3,3,GRID_BOUNDARY_DOWN,5,3); target(cells,14,4,3,GRID_BOUNDARY_DOWN,5,3);
    target(cells,14,5,3,GRID_BOUNDARY_DOWN,6,3); target(cells,14,6,3,GRID_BOUNDARY_DOWN,11,3); target(cells,14,11,3,GRID_BOUNDARY_DOWN,DX_SHEET_ROWS-1,3);
    target(cells,14,4,3,GRID_BOUNDARY_UP,2,3); target(cells,14,5,3,GRID_BOUNDARY_UP,2,3); target(cells,14,6,3,GRID_BOUNDARY_UP,5,3);
    target(cells,14,10,1,GRID_BOUNDARY_LEFT,10,0); target(cells,14,10,1,GRID_BOUNDARY_RIGHT,10,2);
    target(cells,14,10,2,GRID_BOUNDARY_RIGHT,10,5); target(cells,14,10,3,GRID_BOUNDARY_RIGHT,10,5); target(cells,14,10,5,GRID_BOUNDARY_RIGHT,10,6);
    target(cells,14,10,6,GRID_BOUNDARY_RIGHT,10,GRID_BOUNDARY_COLUMNS-1); target(cells,14,10,4,GRID_BOUNDARY_LEFT,10,2);
    target(cells,14,10,6,GRID_BOUNDARY_LEFT,10,5); target(cells,14,7,9,GRID_BOUNDARY_DOWN,DX_SHEET_ROWS-1,9);
    target(NULL,0,7,9,GRID_BOUNDARY_UP,0,9); target(NULL,0,7,9,GRID_BOUNDARY_RIGHT,7,GRID_BOUNDARY_COLUMNS-1);
    target(NULL,0,0,0,GRID_BOUNDARY_UP,0,0); target(NULL,0,DX_SHEET_ROWS-1,GRID_BOUNDARY_COLUMNS-1,GRID_BOUNDARY_DOWN,DX_SHEET_ROWS-1,GRID_BOUNDARY_COLUMNS-1);
}
static void extending(void) {
    const GridBoundaryCell cells[]={{1,20,true},{12,20,true},{13,20,true},{14,20,true},{15,20,true},{16,20,true},{17,20,true},{25,20,true}};
    Inventory i={cells,8,0,SIZE_MAX}; GridBoundaryOccupancy o=inventory(&i); GridBoundarySelection s={15,10,20,15,10,20},next={0};
    CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,true,&next)==GRID_BOUNDARY_APPLIED && next.end_row==17 && next.anchor_row==10 && next.anchor_column==10 && next.row==15 && next.column==15);
    s=next; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,true,&s)==GRID_BOUNDARY_APPLIED && s.end_row==12 && s.row==10 && s.column==10 && s.anchor_row==10 && s.anchor_column==10);
    CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,true,&s)==GRID_BOUNDARY_APPLIED && s.end_row==1 && s.row==10 && s.anchor_row==10 && s.end_column==20);
    CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,true,&s)==GRID_BOUNDARY_APPLIED && s.end_row==12 && s.anchor_row==10 && s.row==10);
    s=(GridBoundarySelection){10,10,20,10,10,20}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,false,&s)==GRID_BOUNDARY_APPLIED && s.row==0 && !s.end_row && s.column==10 && s.anchor_column==10);
    s=point(0,0); next=point(88,88); CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,true,&next)==GRID_BOUNDARY_NOOP && next.row==88);
    s=(GridBoundarySelection){0,0,5,0,0,5}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_UP,false,&s)==GRID_BOUNDARY_APPLIED && !s.end_row && !s.end_column);
}
static void invalid(void) {
    GridBoundaryCell cells[]={{1,1,true},{2,2,true},{3,3,true}}; Inventory i={cells,3,0,2}; GridBoundaryOccupancy o=inventory(&i); GridBoundarySelection s=point(0,0),out=point(91,91);
    CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_UNAVAILABLE && out.row==91 && i.reads==3);
    i.fail=SIZE_MAX; cells[2]=(GridBoundaryCell){2,2,false}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    cells[2]=(GridBoundaryCell){0,0,true}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    cells[2]=(GridBoundaryCell){DX_SHEET_ROWS,0,false}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    cells[2]=(GridBoundaryCell){3,GRID_BOUNDARY_COLUMNS,false}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    o.grant=2; i.reads=0; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_LIMIT && !i.reads && out.row==91);
    o.grant=SIZE_MAX; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_LIMIT && !i.reads);
    o=inventory(&i); o.read=NULL; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    o=inventory(&i); s.row=DX_SHEET_ROWS; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    s=point(3,3); s.row=4; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_DOWN,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    s=point(0,0); CHECK(grid_data_boundary(&s,&o,(GridBoundaryDirection)99,false,&out)==GRID_BOUNDARY_INVALID && out.row==91);
    o=(GridBoundaryOccupancy){0}; CHECK(grid_data_boundary(&s,&o,GRID_BOUNDARY_RIGHT,false,&out)==GRID_BOUNDARY_APPLIED && out.column==GRID_BOUNDARY_COLUMNS-1);
}
static void maximum(void) {
    GridBoundaryCell *cells=malloc(GRID_BOUNDARY_CELLS_MAX*sizeof(*cells)); CHECK(cells);
    for (size_t n=0;n<GRID_BOUNDARY_CELLS_MAX;n++) cells[n]=(GridBoundaryCell){n,9,true};
    target(cells,GRID_BOUNDARY_CELLS_MAX,0,9,GRID_BOUNDARY_DOWN,GRID_BOUNDARY_CELLS_MAX-1,9);
    target(cells,GRID_BOUNDARY_CELLS_MAX,GRID_BOUNDARY_CELLS_MAX-1,9,GRID_BOUNDARY_DOWN,DX_SHEET_ROWS-1,9);
    free(cells);
}
static bool marked(const bool cells[8][8],int row,int column) { return row>=0 && row<8 && column>=0 && column<8 && cells[row][column]; }
static bool between(uint64_t value,uint64_t a,uint64_t b) { return value>=(a<b ? a : b) && value<=(a>b ? a : b); }
static bool same_selection(GridBoundarySelection a,GridBoundarySelection b) { return a.row==b.row && a.column==b.column && a.anchor_row==b.anchor_row && a.anchor_column==b.anchor_column && a.end_row==b.end_row && a.end_column==b.end_column; }
static GridBoundarySelection reference(const bool cells[8][8],GridBoundarySelection s,GridBoundaryDirection dir,bool shift) {
    int row=(int)(shift ? s.end_row : s.row),column=(int)(shift ? s.end_column : s.column);
    int dr=dir==GRID_BOUNDARY_UP ? -1 : dir==GRID_BOUNDARY_DOWN ? 1 : 0,dc=dir==GRID_BOUNDARY_LEFT ? -1 : dir==GRID_BOUNDARY_RIGHT ? 1 : 0;
    bool run=marked(cells,row,column) && marked(cells,row+dr,column+dc); int r=row+dr,c=column+dc;
    if (run) { while (marked(cells,r+dr,c+dc)) { r+=dr; c+=dc; } }
    else { while (r>=0 && r<8 && c>=0 && c<8 && !marked(cells,r,c)) { r+=dr; c+=dc; } }
    uint64_t target_row=dr ? (r<0 ? 0 : r>=8 ? DX_SHEET_ROWS-1 : (uint64_t)r) : (uint64_t)row;
    size_t target_column=dc ? (c<0 ? 0 : c>=8 ? GRID_BOUNDARY_COLUMNS-1 : (size_t)c) : (size_t)column;
    if (!shift) return point(target_row,target_column);
    s.end_row=target_row; s.end_column=target_column;
    if (!between(s.row,s.anchor_row,s.end_row) || !between(s.column,s.anchor_column,s.end_column)) { s.row=s.anchor_row; s.column=s.anchor_column; }
    return s;
}
static void matrix(void) {
    for (unsigned pattern=0;pattern<32;pattern++) {
        bool cells[8][8]; GridBoundaryCell entries[64];
        for (unsigned r=0;r<8;r++) for (unsigned c=0;c<8;c++) { cells[r][c]=((r*7+c*11+pattern*3)%(pattern%5+2))==0; entries[r*8+c]=(GridBoundaryCell){r,c,cells[r][c]}; }
        Inventory i={entries,64,0,SIZE_MAX}; GridBoundaryOccupancy o=inventory(&i);
        for (unsigned r=0;r<8;r++) for (unsigned c=0;c<8;c++) for (unsigned d=0;d<4;d++) for (unsigned shift=0;shift<2;shift++) {
            GridBoundarySelection s=point(r,c); if (shift) { s.anchor_row=s.row=(r+3)%8; s.anchor_column=s.column=(c+5)%8; }
            GridBoundarySelection expected=reference(cells,s,(GridBoundaryDirection)d,shift!=0),out=point(91,91); bool changed=!same_selection(s,expected);
            CHECK(grid_data_boundary(&s,&o,(GridBoundaryDirection)d,shift!=0,&out)==(changed ? GRID_BOUNDARY_APPLIED : GRID_BOUNDARY_NOOP));
            CHECK(same_selection(out,changed ? expected : point(91,91)));
        }
    }
}
int main(void) { islands(); extending(); invalid(); maximum(); matrix(); printf("PASS: %u sparse manual data-boundary checks\n",checks); return 0; }
