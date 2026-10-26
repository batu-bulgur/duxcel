#include "grid/manual_find.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL manual Find:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { const GridFindCell *cells; size_t count,fail,reads; } Inventory;
static bool read_cell(const void *context,size_t index,GridFindCell *out) { Inventory *i=(Inventory *)context; i->reads++; if (index>=i->count || index==i->fail) return false; *out=i->cells[index]; return true; }
static GridFindInventory owner(Inventory *i) { return (GridFindInventory){read_cell,i,i->count,GRID_BOUNDARY_CELLS_MAX,GRID_FIND_TEXT_BYTES}; }
static GridFindCell text(uint64_t row,size_t col,const char *s) { return (GridFindCell){row,col,true,true,s,strlen(s)}; }
static void found(GridFindInventory o,uint64_t row,size_t col,const char *query,GridFindDirection direction,GridFindMode mode,uint64_t expected_row,size_t expected_col,size_t matches,bool wrapped) {
    GridFindResult out={0}; CHECK(grid_manual_find(&o,row,col,query,strlen(query),direction,mode,&out)==GRID_FIND_FOUND);
    CHECK(out.row==expected_row && out.column==expected_col && out.matches==matches && out.wrapped==wrapped);
}
static void refusal(GridFindInventory o,const char *query,size_t bytes,GridFindStatus expected) {
    GridFindResult out={99,11,17,true},old=out; CHECK(grid_manual_find(&o,0,0,query,bytes,GRID_FIND_NEXT,GRID_FIND_CONTAINS,&out)==expected); CHECK(!memcmp(&old,&out,sizeof(out)));
}
int main(void) {
    GridFindCell cells[]={text(0,0,"alpha"),text(0,2,"Alpha"),text(4,1,"alphabet"),{5,0,false,false,NULL,0},{5,1,true,false,NULL,0},text(5,2,""),text(6,0,"NULL"),text(7,0,"+000123.4500"),text(8,0,"İstanbul 世界"),text(DX_SHEET_ROWS-1,16383,"alpha")};
    Inventory i={cells,10,SIZE_MAX,0}; GridFindInventory o=owner(&i);
    found(o,0,0,"alpha",GRID_FIND_NEXT,GRID_FIND_CONTAINS,4,1,3,false); CHECK(i.reads==i.count);
    found(o,4,1,"alpha",GRID_FIND_PREVIOUS,GRID_FIND_CONTAINS,0,0,3,false);
    found(o,DX_SHEET_ROWS-1,16383,"alpha",GRID_FIND_NEXT,GRID_FIND_CONTAINS,0,0,3,true);
    found(o,0,0,"alpha",GRID_FIND_PREVIOUS,GRID_FIND_CONTAINS,DX_SHEET_ROWS-1,16383,3,true);
    found(o,4,1,"alpha",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,DX_SHEET_ROWS-1,16383,2,false);
    found(o,8,0,"Alpha",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,0,2,1,true);
    found(o,0,2,"Alpha",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,0,2,1,true);
    found(o,0,2,"Alpha",GRID_FIND_PREVIOUS,GRID_FIND_WHOLE_CELL,0,2,1,true);
    found(o,0,0,"NULL",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,6,0,1,false);
    found(o,0,0,"+000123.4500",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,7,0,1,false);
    found(o,0,0,"世界",GRID_FIND_NEXT,GRID_FIND_CONTAINS,8,0,1,false);
    refusal(o,"+123.4500",9,GRID_FIND_NOT_FOUND); refusal(o,"",0,GRID_FIND_INVALID); refusal(o,"alpha",GRID_FIND_QUERY_BYTES+1,GRID_FIND_LIMIT);
    refusal(o,"a\0b",3,GRID_FIND_INVALID); refusal(o,"\xc0\x80",2,GRID_FIND_INVALID); refusal(o,"\xed\xa0\x80",3,GRID_FIND_INVALID);
    o.text_grant=4; refusal(o,"alpha",5,GRID_FIND_LIMIT); o=owner(&i); o.grant=9; refusal(o,"alpha",5,GRID_FIND_LIMIT);
    o=owner(&i); o.grant=GRID_BOUNDARY_CELLS_MAX+1; refusal(o,"alpha",5,GRID_FIND_LIMIT);
    o=owner(&i); o.text_grant=GRID_FIND_TEXT_BYTES+1; refusal(o,"alpha",5,GRID_FIND_LIMIT);
    o=owner(&i); i.fail=9; refusal(o,"alpha",5,GRID_FIND_UNAVAILABLE); i.fail=SIZE_MAX;
    GridFindCell saved=cells[9]; cells[9]=text(0,0,"alpha"); refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=text(DX_SHEET_ROWS,0,"alpha"); refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=text(9,GRID_BOUNDARY_COLUMNS,"alpha"); refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=text(9,0,"bad\xff"); refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=(GridFindCell){9,0,false,true,"alpha",5}; refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=(GridFindCell){9,0,true,false,"NULL",4}; refusal(o,"alpha",5,GRID_FIND_INVALID);
    cells[9]=saved;
    Inventory empty={NULL,0,SIZE_MAX,0}; refusal(owner(&empty),"alpha",5,GRID_FIND_NOT_FOUND);
    GridFindCell overlap=text(0,0,"aaaaaaaaaaaaaaaaaaaaaaaaab"),needle=text(1,0,"unchanged"); Inventory overlaps={&overlap,1,SIZE_MAX,0};
    found(owner(&overlaps),0,1,"aaaaab",GRID_FIND_NEXT,GRID_FIND_CONTAINS,0,0,1,true);
    GridFindResult out={0}; CHECK(grid_manual_find(&o,DX_SHEET_ROWS,0,"a",1,GRID_FIND_NEXT,GRID_FIND_CONTAINS,&out)==GRID_FIND_INVALID);
    CHECK(grid_manual_find(&o,0,0,"a",1,(GridFindDirection)2,GRID_FIND_CONTAINS,&out)==GRID_FIND_INVALID);
    CHECK(grid_manual_find(&o,0,0,"a",1,GRID_FIND_NEXT,(GridFindMode)2,&out)==GRID_FIND_INVALID);
    /* Both borrowed query and input survive; no clone or terminator required. */
    Inventory span={&needle,1,SIZE_MAX,0}; const char raw[3]={'a','n','g'}; GridFindInventory borrowed=owner(&span);
    CHECK(grid_manual_find(&borrowed,0,0,raw,sizeof(raw),GRID_FIND_NEXT,GRID_FIND_CONTAINS,&out)==GRID_FIND_FOUND && out.row==1);
    CHECK(!memcmp(raw,"ang",3) && !strcmp(needle.text,"unchanged"));
    GridFindCell *maximum=calloc(GRID_BOUNDARY_CELLS_MAX,sizeof(*maximum)); CHECK(maximum);
    for (size_t n=0;n<GRID_BOUNDARY_CELLS_MAX;n++) maximum[n]=text(n,0,"needle");
    Inventory max_inventory={maximum,GRID_BOUNDARY_CELLS_MAX,SIZE_MAX,0}; GridFindInventory max_owner=owner(&max_inventory);
    found(max_owner,GRID_BOUNDARY_CELLS_MAX-1,0,"needle",GRID_FIND_NEXT,GRID_FIND_WHOLE_CELL,0,0,GRID_BOUNDARY_CELLS_MAX,true);
    CHECK(max_inventory.reads==GRID_BOUNDARY_CELLS_MAX); max_inventory.fail=GRID_BOUNDARY_CELLS_MAX-1;
    refusal(max_owner,"needle",6,GRID_FIND_UNAVAILABLE); free(maximum);
    char *large=malloc(GRID_FIND_TEXT_BYTES),*pattern=malloc(GRID_FIND_QUERY_BYTES); CHECK(large && pattern);
    memset(large,'a',GRID_FIND_TEXT_BYTES); large[GRID_FIND_TEXT_BYTES-1]='b';
    memset(pattern,'a',GRID_FIND_QUERY_BYTES); pattern[GRID_FIND_QUERY_BYTES-1]='b';
    GridFindCell large_cells[]={{2,0,true,true,large,GRID_FIND_TEXT_BYTES},{3,0,true,true,"x",1}};
    Inventory large_inventory={large_cells,1,SIZE_MAX,0}; GridFindInventory large_owner=owner(&large_inventory);
    CHECK(grid_manual_find(&large_owner,0,0,pattern,GRID_FIND_QUERY_BYTES,GRID_FIND_NEXT,GRID_FIND_CONTAINS,&out)==GRID_FIND_FOUND && out.row==2 && out.matches==1);
    pattern[GRID_FIND_QUERY_BYTES-1]='c'; GridFindResult previous_result=out; large_inventory.reads=0;
    CHECK(grid_manual_find(&large_owner,0,0,pattern,GRID_FIND_QUERY_BYTES,GRID_FIND_NEXT,GRID_FIND_CONTAINS,&out)==GRID_FIND_NOT_FOUND);
    CHECK(!memcmp(&out,&previous_result,sizeof(out)) && large_inventory.reads==1);
    large_inventory.count=2; large_owner=owner(&large_inventory); refusal(large_owner,"b",1,GRID_FIND_LIMIT);
    free(pattern); free(large);
    printf("PASS: %u bounded manual Find checks\n",checks); return 0;
}
