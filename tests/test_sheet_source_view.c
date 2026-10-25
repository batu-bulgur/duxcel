#include "workbook/sheet_source_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source view:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SheetSourceViewSpec spec(Column *columns,size_t count) {
    return (SheetSourceViewSpec){columns,count,{.row=900,.col=9000,.anchor_row=901,.anchor_col=9001,.end_row=902,.end_col=9002,.top=890,
        .horizontal=12.5f,.fraction=.25,.canvas_zoom=1.5f,.selection=2}};
}
static void build_and_take(void) {
    Column columns[]={{.name=dx_strdup("City"),.type=DUCKDB_TYPE_VARCHAR},{.name=dx_strdup("When"),.type=DUCKDB_TYPE_TIMESTAMP},{.name=dx_strdup("Amount"),.type=DUCKDB_TYPE_DECIMAL}};
    SheetSourceViewSpec input=spec(columns,3); SheetSourceViewInfo info={0}; SheetSourceView *owned=NULL;
    CHECK(sheet_source_view_measure(input,&info)==SHEET_SOURCE_VIEW_OK && info.owned>sizeof(SheetView) && info.columns==52);
    CHECK(sheet_source_view_build(input,info.owned-1,&owned)==SHEET_SOURCE_VIEW_LIMIT && !owned);
    CHECK(sheet_source_view_build(input,info.owned,&owned)==SHEET_SOURCE_VIEW_OK && sheet_source_view_owned(owned)==info.owned);
    const SheetView *view=sheet_source_view_get(owned);
    CHECK(view && view->column_count==52 && view->widths[0]==128 && view->widths[1]==190 && view->widths[2]==128 && view->widths[51]==96);
    CHECK(view->columns[0]!=columns[0].name && !strcmp(view->columns[0],"City") && !*view->columns[51]);
    CHECK(view->row==900 && view->col==9000 && view->top==890 && view->canvas_zoom==1.5f && view->fraction==.25 && view->horizontal==12.5f && view->selection==2);
    bool matches=false; CHECK(sheet_source_view_matches(owned,input,&matches)==SHEET_SOURCE_VIEW_OK && matches);
    columns[1].type=DUCKDB_TYPE_TIMESTAMP_TZ; CHECK(sheet_source_view_matches(owned,input,&matches)==SHEET_SOURCE_VIEW_OK && !matches); columns[1].type=DUCKDB_TYPE_TIMESTAMP;
    input.navigation.canvas_zoom=1; CHECK(sheet_source_view_matches(owned,input,&matches)==SHEET_SOURCE_VIEW_OK && !matches); input.navigation.canvas_zoom=1.5f;
    SheetView occupied={.canvas_zoom=1}; SheetView before=occupied; CHECK(sheet_source_view_take(&owned,&occupied)==SHEET_SOURCE_VIEW_INVALID && owned && !memcmp(&occupied,&before,sizeof(before)));
    for (size_t i=0;i<3;i++) free(columns[i].name);
    CHECK(!strcmp(sheet_source_view_get(owned)->columns[1],"When"));
    SheetView after={0}; CHECK(sheet_source_view_take(&owned,&after)==SHEET_SOURCE_VIEW_OK && !owned && after.widths[1]==190 && after.col==9000);
    view_free(&after); CHECK(!after.columns && !after.widths && !after.column_count);
}
static void invalid_and_bounds(void) {
    Column column={.name="Field",.type=DUCKDB_TYPE_BIGINT}; SheetSourceViewSpec valid=spec(&column,1);
    for (unsigned mode=0;mode<13;mode++) {
        SheetSourceViewSpec input=valid; SheetSourceViewInfo info={1,2},before=info; SheetSourceView *owned=NULL;
        if (mode==0) input.count=0;
        if (mode==1) input.count=16385;
        if (mode==2) input.columns=NULL;
        if (mode==3) input.navigation.canvas_zoom=0;
        if (mode==4) input.navigation.canvas_zoom=NAN;
        if (mode==5) input.navigation.row=DX_SHEET_ROWS;
        if (mode==6) input.navigation.col=16384;
        if (mode==7) input.navigation.selection=4;
        if (mode==8) input.navigation.horizontal=-1;
        if (mode==9) input.navigation.fraction=1;
        if (mode==10) input.navigation.column_count=1;
        if (mode==11) input.navigation.columns=(char **)&input.columns;
        if (mode==12) input.navigation.widths=(float *)&input.count;
        CHECK(sheet_source_view_measure(input,&info)==SHEET_SOURCE_VIEW_INVALID && !memcmp(&info,&before,sizeof(info)));
        CHECK(sheet_source_view_build(input,SIZE_MAX,&owned)==SHEET_SOURCE_VIEW_INVALID && !owned);
    }
    column.name=NULL; SheetSourceViewInfo info; CHECK(sheet_source_view_measure(valid,&info)==SHEET_SOURCE_VIEW_INVALID); column.name="";
    CHECK(sheet_source_view_measure(valid,&info)==SHEET_SOURCE_VIEW_OK);
    Column *many=calloc(16384,sizeof(*many)); CHECK(many);
    for (size_t i=0;i<16384;i++) many[i]=(Column){.name="Field",.type=DUCKDB_TYPE_VARCHAR};
    CHECK(sheet_source_view_measure(spec(many,16384),&info)==SHEET_SOURCE_VIEW_OK && info.columns==16384);
    CHECK(sheet_source_view_measure(spec(many,100),&info)==SHEET_SOURCE_VIEW_OK && info.columns==116);
    SheetSourceView *owned=NULL; CHECK(sheet_source_view_build(spec(many,100),info.owned,&owned)==SHEET_SOURCE_VIEW_OK);
    CHECK(sheet_source_view_get(owned)->widths[99]==128 && sheet_source_view_get(owned)->widths[100]==96);
    sheet_source_view_free(&owned); sheet_source_view_free(&owned); free(many);
    CHECK(!sheet_source_view_get(NULL) && !sheet_source_view_owned(NULL));
}
int main(void) { build_and_take(); invalid_and_bounds(); printf("PASS: %u owned initial source view checks\n",checks); return 0; }
