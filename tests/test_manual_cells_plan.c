#include "workbook/manual_cells_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ManualValueInput text(const char *raw) { return (ManualValueInput){true,MANUAL_VALUE_TEXT,{0},raw,strlen(raw)}; }
static ManualValueInput number(const char *raw) { return (ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_DECIMAL,8,2},raw,strlen(raw)}; }
static ManualCells *empty(void) { CellSnapshot source={0}; ManualCells *out=NULL; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_READY); return out; }
static void set(ManualCells **cells,uint64_t row,size_t column,ManualValueInput value) { CHECK(manual_cells_edit(*cells,row,column,value,0,SIZE_MAX,cells)==MANUAL_CELLS_READY); }
static void current_snapshot_import(void) {
    CellStore live={0},copy={0}; char *error=NULL;
    CHECK(cells_set(&live,2,4,"NULL",&error) && cells_set(&live,0,1,"18446744073709551615",&error) && cells_set(&live,DX_SHEET_ROWS-1,16383,"=A1+1",&error));
    CHECK(cells_set(&live,3,1,"",&error) && live.count==3 && live.undo_count==3);
    cells_copy(&copy,&live); CHECK(copy.count==live.count && !copy.undo_count && copy.capacity==copy.count);
    CellSnapshot source={copy.cells,copy.count,copy.bytes}; ManualCellsInfo info;
    CHECK(manual_cells_import_measure(&source,&info)==MANUAL_CELLS_READY && info.count==3 && info.raw_bytes==copy.bytes-copy.count && info.coexisting==info.owned+copy.count*sizeof(SheetCell)+copy.bytes);
    /* Live current capacity + genuine retained Undo snapshots coexist with the
       independent snapshot supplied to import; no model eviction is permitted. */
    size_t retained=live.capacity*sizeof(SheetCell)+live.bytes;
    for (size_t i=0;i<live.undo_count;i++) retained+=live.undo[i].count*sizeof(SheetCell)+live.undo[i].bytes;
    ManualCells *out=NULL; CHECK(manual_cells_import(&source,retained,info.coexisting+retained-1,&out)==MANUAL_CELLS_LIMIT && !out);
    CHECK(manual_cells_import(&source,retained,info.coexisting+retained,&out)==MANUAL_CELLS_READY && manual_cells_owned(out)==info.owned);
    ManualCells *prior=out; size_t prior_owned=manual_cells_owned(out);
    CHECK(manual_cells_import(&source,retained,info.coexisting+retained+prior_owned-1,&out)==MANUAL_CELLS_LIMIT && out==prior);
    CHECK(manual_cells_import(&source,retained,info.coexisting+retained+prior_owned,&out)==MANUAL_CELLS_READY);
    CHECK(manual_cells_import(&source,SIZE_MAX,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT);
    CHECK(manual_value_view(manual_cells_at(out,0,1)).kind==MANUAL_VALUE_TEXT && !strcmp(manual_value_view(manual_cells_at(out,0,1)).raw,"18446744073709551615"));
    CHECK(manual_value_view(manual_cells_at(out,2,4)).kind==MANUAL_VALUE_TEXT && !strcmp(manual_value_view(manual_cells_at(out,2,4)).raw,"NULL"));
    CHECK(!strcmp(manual_value_view(manual_cells_at(out,DX_SHEET_ROWS-1,16383)).raw,"=A1+1") && !manual_cells_at(out,3,1));
    CHECK(live.undo_count==3 && !live.redo_count && !error);
    cells_free(&copy); cells_free(&live);
    CHECK(!strcmp(manual_value_view(manual_cells_at(out,2,4)).raw,"NULL")); manual_cells_free(&out); free(error);
}
static void presence_and_after(void) {
    ManualCells *before=empty(),*after=NULL; ManualCellsInfo info={11,12,13,14},prior=info;
    CHECK(manual_cells_edit_measure(before,1,1,(ManualValueInput){0},&info)==MANUAL_CELLS_NOOP && !memcmp(&info,&prior,sizeof(info)));
    CHECK(manual_cells_edit(before,1,1,(ManualValueInput){0},SIZE_MAX,0,&after)==MANUAL_CELLS_NOOP && !after);
    CHECK(manual_cells_edit(before,1,1,(ManualValueInput){.present=true},0,SIZE_MAX,&after)==MANUAL_CELLS_READY);
    CHECK(!manual_cells_at(before,1,1) && manual_cells_count(after)==1 && manual_value_view(manual_cells_at(after,1,1)).kind==MANUAL_VALUE_SQL_NULL);
    set(&after,1,2,text("")); set(&after,1,3,text("NULL")); set(&after,1,4,number("+0012.3"));
    CHECK(manual_cells_count(after)==4 && manual_value_view(manual_cells_at(after,1,2)).present && !manual_value_view(manual_cells_at(after,1,2)).raw_bytes);
    CHECK(!strcmp(manual_value_view(manual_cells_at(after,1,4)).canonical,"12.30"));
    ManualCells *old=after; CHECK(manual_cells_edit(after,1,4,number("+0012.3"),0,0,&after)==MANUAL_CELLS_NOOP && after==old);
    set(&after,1,4,number("12.30")); CHECK(!strcmp(manual_value_view(manual_cells_at(after,1,4)).raw,"12.30"));
    set(&after,1,1,(ManualValueInput){0}); CHECK(!manual_cells_at(after,1,1) && manual_cells_count(after)==3);
    set(&after,0,8,text("first")); set(&after,DX_SHEET_ROWS-1,0,text("last"));
    const ManualValue *slots[6]; for (size_t i=0;i<6;i++) slots[i]=(const ManualValue *)1;
    ManualRangeView view={1,2,3,NULL},oldview=view;
    CHECK(manual_cells_range(after,1,0,1,6,slots,5,&view)==MANUAL_CELLS_LIMIT && !memcmp(&view,&oldview,sizeof(view)) && slots[0]==(const ManualValue *)1);
    CHECK(manual_cells_range(after,1,0,1,6,slots,6,&view)==MANUAL_CELLS_READY && view.count==6 && !slots[0] && !slots[1] && slots[2] && slots[3] && slots[4] && !slots[5]);
    ManualRangePacket packet={0}; CHECK(manual_range_capture(view,SIZE_MAX,&packet)==MANUAL_RANGE_OK); ManualRange *decoded=NULL;
    CHECK(manual_range_decode(packet.data,packet.bytes,SIZE_MAX,&decoded)==MANUAL_RANGE_OK);
    CHECK(manual_value_equal(manual_range_view(decoded).values[4],manual_cells_at(after,1,4)) && !manual_range_view(decoded).values[1]);
    manual_cells_free(&before); manual_cells_free(&after); CHECK(!strcmp(manual_value_view(manual_range_view(decoded).values[3]).raw,"NULL"));
    manual_range_free(&decoded); manual_range_packet_free(&packet);
}
static void budgets_and_aliases(void) {
    ManualCells *source=empty(),*out=empty(); set(&source,4,5,text("Owned")); set(&out,0,1,text("Prior"));
    ManualCellsInfo info; CHECK(manual_cells_edit_measure(source,8,3,number("+0012.3"),&info)==MANUAL_CELLS_READY);
    size_t needed=info.coexisting+manual_cells_owned(out)+103; ManualCells *prior=out;
    CHECK(manual_cells_edit(source,8,3,number("+0012.3"),103,needed-1,&out)==MANUAL_CELLS_LIMIT && out==prior && !manual_cells_at(source,8,3));
    CHECK(manual_cells_edit(source,8,3,number("+0012.3"),103,needed,&out)==MANUAL_CELLS_READY && manual_cells_count(out)==2 && manual_cells_count(source)==1);
    CHECK(manual_cells_edit(source,9,1,text("X"),SIZE_MAX,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT);
    CHECK(manual_cells_edit_measure(source,4,5,text("Changed"),&info)==MANUAL_CELLS_READY);
    prior=source; CHECK(manual_cells_edit(source,4,5,text("Changed"),23,info.coexisting+22,&source)==MANUAL_CELLS_LIMIT && source==prior);
    CHECK(manual_cells_edit(source,4,5,text("Changed"),23,info.coexisting+23,&source)==MANUAL_CELLS_READY && !strcmp(manual_value_view(manual_cells_at(source,4,5)).raw,"Changed"));
    ManualValueView borrowed=manual_value_view(manual_cells_at(out,4,5));
    CHECK(manual_cells_edit(source,1,1,text(borrowed.raw),0,SIZE_MAX,&out)==MANUAL_CELLS_READY && !strcmp(manual_value_view(manual_cells_at(out,1,1)).raw,"Owned"));
    manual_cells_free(&source); CHECK(!strcmp(manual_value_view(manual_cells_at(out,4,5)).raw,"Changed"));
    manual_cells_free(&out); manual_cells_free(&out); manual_cells_free(NULL);
    CHECK(!manual_cells_count(NULL) && !manual_cells_owned(NULL) && !manual_cells_at(NULL,0,0));
}
static void invalid_inputs(void) {
    ManualCells *out=empty(),*prior=out; SheetCell cells[]={{1,2,"A"},{1,3,"B"}}; CellSnapshot source={cells,2,4};
    ManualCellsInfo info={11,12,13,14},old=info;
    const size_t wrong[]={0,3,5};
    for (size_t i=0;i<3;i++) { source.bytes=wrong[i]; CHECK(manual_cells_import_measure(&source,&info)==MANUAL_CELLS_INVALID && !memcmp(&info,&old,sizeof(info))); CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && out==prior); }
    source.bytes=4; cells[1].col=2; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].col=3;
    cells[1].row=0; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].row=1;
    cells[1].text=""; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].text=NULL; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    cells[1].text="\xff"; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].text="B";
    cells[1].row=DX_SHEET_ROWS; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].row=1;
    cells[1].col=16384; CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID); cells[1].col=3;
    CHECK(manual_cells_edit(out,DX_SHEET_ROWS,0,text("A"),0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && out==prior);
    CHECK(manual_cells_edit(out,0,16384,text("A"),0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_edit(out,0,0,number("1.234"),0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_edit(out,0,0,(ManualValueInput){.kind=MANUAL_VALUE_TEXT},0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_import(NULL,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && manual_cells_import(&source,0,SIZE_MAX,NULL)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_edit(NULL,0,0,text("A"),0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && manual_cells_edit(out,0,0,text("A"),0,SIZE_MAX,NULL)==MANUAL_CELLS_INVALID);
    const ManualValue *slots[1]; ManualRangeView range;
    CHECK(manual_cells_range(out,0,0,DX_SHEET_ROWS,16384,slots,1,&range)==MANUAL_CELLS_LIMIT);
    CHECK(manual_cells_range(out,DX_SHEET_ROWS-1,16383,2,1,slots,1,&range)==MANUAL_CELLS_LIMIT);
    CHECK(manual_cells_range(out,0,0,0,1,slots,1,&range)==MANUAL_CELLS_INVALID);
    CHECK(out==prior); manual_cells_free(&out);
}
static ManualRange *range(ManualRangeView view) {
    ManualRangePacket packet={0}; ManualRange *out=NULL;
    CHECK(manual_range_capture(view,SIZE_MAX,&packet)==MANUAL_RANGE_OK);
    CHECK(manual_range_decode(packet.data,packet.bytes,SIZE_MAX,&out)==MANUAL_RANGE_OK);
    manual_range_packet_free(&packet); return out;
}
static ManualValue *value(ManualValueInput input) { ManualValue *out=NULL; CHECK(manual_value_build(input,SIZE_MAX,&out)==MANUAL_VALUE_OK); return out; }
static void private_range_after(void) {
    ManualCells *before=empty(),*after=empty();
    set(&before,0,0,text("before")); set(&before,1,1,text("left")); set(&before,1,5,text("right"));
    set(&before,2,2,text("removed hole")); set(&before,1,2,text("replaced")); set(&before,DX_SHEET_ROWS-1,16383,text("far"));
    ManualValue *null=value((ManualValueInput){.present=true}),*empty_text=value(text("")),*numeric=value(number("+0012.3")),*literal=value(text("NULL"));
    const ManualValue *values[]={null,empty_text,NULL,NULL,numeric,literal}; ManualRange *input=range((ManualRangeView){2,3,6,values});
    ManualCellsInfo info; CHECK(manual_cells_paste_measure(before,1,2,input,&info)==MANUAL_CELLS_READY && info.count==8);
    CHECK(info.coexisting==info.owned+manual_cells_owned(before)+manual_range_owned(input));
    size_t required=info.coexisting+manual_cells_owned(after)+59; ManualCells *prior=after;
    CHECK(manual_cells_paste(before,1,2,input,59,required-1,&after)==MANUAL_CELLS_LIMIT && after==prior && !strcmp(manual_value_view(manual_cells_at(before,2,2)).raw,"removed hole"));
    CHECK(manual_cells_paste(before,1,2,input,59,required,&after)==MANUAL_CELLS_READY && manual_cells_count(after)==8);
    CHECK(!strcmp(manual_value_view(manual_cells_at(after,0,0)).raw,"before") && !strcmp(manual_value_view(manual_cells_at(after,1,1)).raw,"left") && !strcmp(manual_value_view(manual_cells_at(after,1,5)).raw,"right"));
    CHECK(!strcmp(manual_value_view(manual_cells_at(after,DX_SHEET_ROWS-1,16383)).raw,"far"));
    CHECK(manual_value_view(manual_cells_at(after,1,2)).kind==MANUAL_VALUE_SQL_NULL && manual_value_view(manual_cells_at(after,1,3)).present && !manual_value_view(manual_cells_at(after,1,3)).raw_bytes);
    CHECK(!manual_cells_at(after,1,4) && !manual_cells_at(after,2,2) && !strcmp(manual_value_view(manual_cells_at(after,2,3)).raw,"+0012.3") && !strcmp(manual_value_view(manual_cells_at(after,2,4)).raw,"NULL"));
    ManualCellsInfo old={1,2,3,4}; info=old; prior=after;
    CHECK(manual_cells_paste_measure(after,1,2,input,&info)==MANUAL_CELLS_NOOP && !memcmp(&info,&old,sizeof(info)));
    CHECK(manual_cells_paste(after,1,2,input,SIZE_MAX,0,&after)==MANUAL_CELLS_NOOP && after==prior);
    CHECK(manual_cells_paste(before,1,2,input,SIZE_MAX,SIZE_MAX,&after)==MANUAL_CELLS_LIMIT && after==prior);
    /* Same output/source alias counts that owner once; all range values and
       outside entries are independently copied before source release. */
    CHECK(manual_cells_paste_measure(before,1,2,input,&info)==MANUAL_CELLS_READY);
    prior=before; CHECK(manual_cells_paste(before,1,2,input,7,info.coexisting+6,&before)==MANUAL_CELLS_LIMIT && before==prior);
    CHECK(manual_cells_paste(before,1,2,input,7,info.coexisting+7,&before)==MANUAL_CELLS_READY);
    manual_range_free(&input); manual_value_free(&null); manual_value_free(&empty_text); manual_value_free(&numeric); manual_value_free(&literal); manual_cells_free(&after);
    CHECK(!strcmp(manual_value_view(manual_cells_at(before,2,3)).raw,"+0012.3"));
    const ManualValue *slots[6]; ManualRangeView exported;
    CHECK(manual_cells_range(before,1,2,2,3,slots,6,&exported)==MANUAL_CELLS_READY && !slots[2] && !slots[3]);
    input=range(exported); CHECK(manual_cells_paste(before,1,2,input,0,0,&before)==MANUAL_CELLS_NOOP);
    manual_range_free(&input); manual_cells_free(&before);
}
static void private_authored_equality(void) {
    ManualCells *source=empty(); set(&source,0,0,number("+0012.3"));
    ManualValue *v=value(number("12.30")); const ManualValue *one[]={v}; ManualRange *input=range((ManualRangeView){1,1,1,one});
    CHECK(manual_cells_paste(source,0,0,input,0,SIZE_MAX,&source)==MANUAL_CELLS_READY && !strcmp(manual_value_view(manual_cells_at(source,0,0)).raw,"12.30"));
    CHECK(manual_cells_paste(source,0,0,input,0,0,&source)==MANUAL_CELLS_NOOP);
    manual_range_free(&input); manual_value_free(&v); v=value(text("12.30")); one[0]=v; input=range((ManualRangeView){1,1,1,one});
    CHECK(manual_cells_paste(source,0,0,input,0,SIZE_MAX,&source)==MANUAL_CELLS_READY && manual_value_view(manual_cells_at(source,0,0)).kind==MANUAL_VALUE_TEXT);
    CHECK(manual_cells_paste(source,0,0,input,0,0,&source)==MANUAL_CELLS_NOOP);
    manual_range_free(&input); manual_value_free(&v); one[0]=NULL; input=range((ManualRangeView){1,1,1,one});
    CHECK(manual_cells_paste(source,0,0,input,0,SIZE_MAX,&source)==MANUAL_CELLS_READY && !manual_cells_count(source));
    CHECK(manual_cells_paste(source,0,0,input,0,0,&source)==MANUAL_CELLS_NOOP);
    manual_range_free(&input); manual_cells_free(&source);
}
static void private_range_bounds(void) {
    ManualCells *source=empty(),*out=empty(),*prior=out; ManualValue *v=value(text("X")); const ManualValue *one[]={v}; ManualRange *input=range((ManualRangeView){1,1,1,one});
    const uint64_t rows[]={DX_SHEET_ROWS,UINT64_MAX};
    for (size_t i=0;i<2;i++) CHECK(manual_cells_paste(source,rows[i],0,input,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && out==prior);
    CHECK(manual_cells_paste(source,0,16384,input,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_paste(NULL,0,0,input,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID && manual_cells_paste(source,0,0,NULL,0,SIZE_MAX,&out)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_paste(source,0,0,input,0,SIZE_MAX,NULL)==MANUAL_CELLS_INVALID);
    CHECK(manual_cells_paste(source,DX_SHEET_ROWS-1,16383,input,0,SIZE_MAX,&out)==MANUAL_CELLS_READY && manual_cells_count(out)==1);
    manual_range_free(&input); const ManualValue *two[]={v,NULL}; input=range((ManualRangeView){2,1,2,two}); prior=out;
    CHECK(manual_cells_paste(source,DX_SHEET_ROWS-1,16383,input,0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT && out==prior);
    manual_range_free(&input); input=range((ManualRangeView){1,2,2,two});
    CHECK(manual_cells_paste(source,DX_SHEET_ROWS-1,16383,input,0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT && out==prior);
    CHECK(manual_cells_paste(source,DX_SHEET_ROWS-1,16382,input,0,SIZE_MAX,&out)==MANUAL_CELLS_READY && manual_value_view(manual_cells_at(out,DX_SHEET_ROWS-1,16382)).present && !manual_cells_at(out,DX_SHEET_ROWS-1,16383));
    manual_cells_free(&source); manual_cells_free(&out); manual_range_free(&input); manual_value_free(&v);
}
static void limits(void) {
    SheetCell *rows=calloc(DX_MAX_EDIT_CELLS,sizeof(*rows)); CHECK(rows);
    for (size_t i=0;i<DX_MAX_EDIT_CELLS;i++) rows[i]=(SheetCell){i,0,"X"};
    CellSnapshot source={rows,DX_MAX_EDIT_CELLS,2*DX_MAX_EDIT_CELLS}; ManualCells *out=NULL;
    CHECK(manual_cells_import(&source,0,SIZE_MAX,&out)==MANUAL_CELLS_READY && manual_cells_count(out)==DX_MAX_EDIT_CELLS);
    CHECK(manual_cells_edit(out,DX_SHEET_ROWS-1,16383,(ManualValueInput){.present=true},0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT);
    ManualValue *present=value(text("Z")); const ManualValue *single[]={present}; ManualRange *input=range((ManualRangeView){1,1,1,single}); ManualCells *prior_count=out;
    CHECK(manual_cells_paste(out,DX_SHEET_ROWS-1,16383,input,0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT && out==prior_count);
    CHECK(manual_cells_paste(out,1,0,input,0,SIZE_MAX,&out)==MANUAL_CELLS_READY && manual_cells_count(out)==DX_MAX_EDIT_CELLS);
    CHECK(manual_cells_paste(out,1,0,input,0,0,&out)==MANUAL_CELLS_NOOP);
    manual_range_free(&input); manual_value_free(&present);
    set(&out,3,0,(ManualValueInput){0}); set(&out,DX_SHEET_ROWS-1,16383,text("")); CHECK(manual_cells_count(out)==DX_MAX_EDIT_CELLS);
    for (size_t i=0;i<DX_MAX_EDIT_CELLS;i++) if (i!=3 && i!=1) CHECK(!strcmp(manual_value_view(manual_cells_at(out,i,0)).raw,"X"));
    manual_cells_free(&out); free(rows);
    out=empty(); char *large=malloc(DX_CELL_BYTES+1); CHECK(large); memset(large,'x',DX_CELL_BYTES); large[DX_CELL_BYTES]=0;
    set(&out,0,0,text(large)); free(large); ManualCells *prior=out;
    CHECK(manual_cells_edit(out,0,1,text("x"),0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT && out==prior);
    present=value(text("Z")); single[0]=present; input=range((ManualRangeView){1,1,1,single});
    CHECK(manual_cells_paste(out,0,1,input,0,SIZE_MAX,&out)==MANUAL_CELLS_LIMIT && out==prior);
    CHECK(manual_cells_paste(out,0,0,input,0,SIZE_MAX,&out)==MANUAL_CELLS_READY && manual_cells_count(out)==1 && !strcmp(manual_value_view(manual_cells_at(out,0,0)).raw,"Z"));
    manual_range_free(&input); manual_value_free(&present);
    set(&out,0,1,text("")); set(&out,0,2,(ManualValueInput){.present=true}); CHECK(manual_cells_count(out)==3);
    manual_cells_free(&out);
}
int main(void) { current_snapshot_import(); presence_and_after(); budgets_and_aliases(); invalid_inputs(); private_range_after(); private_authored_equality(); private_range_bounds(); limits(); printf("PASS: %u unpublished typed sparse cell checks\n",checks); return 0; }
