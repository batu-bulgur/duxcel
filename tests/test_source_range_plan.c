#include "workbook/source_range_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source range:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Source range",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
    Analysis source={.kind=SOURCE_DEMO}; CHECK(objects_add_table(&s->objects,30,8,"Table",&source,NULL)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
    CHECK(cells_set(&s->cells,1,4,"NULL",NULL)); worksheet_record(s,EDIT_CELLS);
    CHECK(cells_set(&s->cells,20,4,"9007199254740993.0123456789",NULL)); worksheet_record(s,EDIT_CELLS);
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){0,40,0,10},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT);
    s->source_versions.high_water=7; b.dirty=false; return b;
}
static bool guard(void *context,uint64_t row,size_t column,size_t rows,size_t columns,char **error) {
    SourceRangeContext *c=context;
    if (c->primary->kind!=SOURCE_NONE && !c->source_columns && !c->fetch_failed) return false;
    return objects_fits(&c->sheet->objects,-1,row,column,rows,columns,NULL,c->count_pending ? UINT64_MAX : c->source_rows,c->source_columns,error);
}
static void paste(void) {
    Workbook b=fixture(); Worksheet *s=b.sheets; Analysis primary={.kind=SOURCE_DEMO}; SourceRangeContext c={s,&primary,10,3,false,false,true};
    Worksheet before=*s; SourceRangePlanInfo info={.owned=123}; CellSnapshot out={0}; const char *text="New\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789\n";
    CHECK(source_range_paste_measure(c,20,0,text,&info,NULL)==SOURCE_RANGE_READY && info.rows==2 && info.columns==2);
    CHECK(source_range_paste_plan(c,20,0,text,info.owned-1,info.validation_scratch,&out,NULL)==SOURCE_RANGE_LIMIT && !out.cells);
    CHECK(source_range_paste_plan(c,20,0,text,info.owned,info.validation_scratch-1,&out,NULL)==SOURCE_RANGE_LIMIT && !out.cells);
    CHECK(source_range_paste_plan(c,20,0,text,info.owned,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && !memcmp(&before,s,sizeof(before)));
    Workbook *reference=workbook_snapshot(&b); CHECK(reference); size_t rows=0,columns=0;
    CHECK(cells_paste_checked(&reference->sheets[0].cells,20,0,text,&rows,&columns,guard,&c,NULL) && rows==info.rows && columns==info.columns && out.count==reference->sheets[0].cells.count);
    for (size_t i=0;i<out.count;i++) CHECK(out.cells[i].row==reference->sheets[0].cells.cells[i].row && out.cells[i].col==reference->sheets[0].cells.cells[i].col && !strcmp(out.cells[i].text,reference->sheets[0].cells.cells[i].text) && out.cells[i].text!=reference->sheets[0].cells.cells[i].text);
    workbook_free(reference); free(reference); CellSnapshot held=out;
    c.count_pending=true; CHECK(source_range_paste_plan(c,20,0,text,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_paste_plan(c,20,4,text,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_READY); held=out;
    CHECK(source_range_paste_plan(c,30,8,"",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_paste_plan(c,28,7,"A\tB\tC\nD\nE",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
    c.count_pending=false; c.source_columns=0;
    CHECK(source_range_paste_plan(c,0,0,"New",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
    c.fetch_failed=true; CHECK(source_range_paste_plan(c,0,0,"New",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_READY); held=out;
    c.ready=false; CHECK(source_range_paste_plan(c,0,0,"New",0,0,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out))); c.ready=true;
    CHECK(source_range_paste_plan(c,1,4,"NULL",0,0,&out,NULL)==SOURCE_RANGE_NOOP && !memcmp(&held,&out,sizeof(out)));
    primary.source_version=99; CHECK(source_range_paste_plan(c,0,0,"New",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); primary.source_version=0;
    s->objects.items[0].source.source_version=99; CHECK(source_range_paste_plan(c,0,0,"New",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); s->objects.items[0].source.source_version=0;
    CHECK(!memcmp(&before,s,sizeof(before)) && s->source_versions.high_water==7 && (format_at(&s->formats,1,4).flags&FORMAT_BOLD));
    workbook_free(&b); CHECK(out.cells[0].text && *out.cells[0].text); cells_snapshot_free(&out);
}
static void clear(void) {
    Workbook b=fixture(); Worksheet *s=b.sheets; Worksheet before=*s; Analysis primary={.kind=SOURCE_DEMO}; SourceRangeContext c={s,&primary,10,3,true,false,true};
    SheetRange r={0,40,0,10}; SourceRangePlanInfo info={0}; CellSnapshot out={0};
    CHECK(source_range_clear_measure(c,r,&info,NULL)==SOURCE_RANGE_READY && !info.owned);
    CHECK(source_range_clear_plan(c,r,0,info.validation_scratch-1,&out,NULL)==SOURCE_RANGE_LIMIT && !out.cells);
    CHECK(source_range_clear_plan(c,r,0,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && !out.cells && !out.count && !memcmp(&before,s,sizeof(before)));
    CHECK(source_range_clear_plan(c,(SheetRange){30,32,8,9},0,0,&out,NULL)==SOURCE_RANGE_NOOP);
    CHECK(source_range_clear_plan(c,(SheetRange){0,UINT64_MAX,0,10},SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID);
    c.ready=false; CHECK(source_range_clear_plan(c,r,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.ready=true; c.source_columns=0;
    CHECK(source_range_clear_plan(c,r,0,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && !memcmp(&before,s,sizeof(before)));
    CHECK(cells_set(&s->cells,1,1,"Manual overlap to clear",NULL)); worksheet_record(s,EDIT_CELLS); before=*s; c.source_columns=3;
    CHECK(source_range_clear_plan(c,r,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_READY && !out.count && !memcmp(&before,s,sizeof(before)));
    CHECK(source_range_paste_plan(c,100,4,"Outside protected rectangle",SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_READY && !memcmp(&before,s,sizeof(before)));
    workbook_free(&b); cells_snapshot_free(&out);
}
static void move(void) {
    Workbook b=fixture(); Worksheet *s=b.sheets; Analysis primary={.kind=SOURCE_DEMO};
    SourceVersions ledger={0}; SourceVersion entry={.owner=s->objects.items[0].id,.version=8,.kind=SOURCE_CSV,.path="/owned.csv"};
    CHECK(source_versions_plan(&s->source_versions,&entry,true,&ledger,NULL)); source_versions_free(&s->source_versions); s->source_versions=ledger;
    s->objects.items[0].source.kind=SOURCE_CSV; s->objects.items[0].source.path=dx_strdup("/owned.csv"); s->objects.items[0].source.source_version=8; Worksheet before=*s;
    SourceTableExtent table={s->objects.items[0].id,3,2,true,false,false}; SourceMoveContext c={{s,&primary,10,3,false,false,true},&table,1};
    SheetRange r={20,32,4,9}; MixedRangePlanInfo info={0}; MixedRangePlan out={0};
    CHECK(source_range_move_measure(c,r,20,10,false,&info,NULL)==SOURCE_RANGE_READY);
    CHECK(source_range_move_plan(c,r,20,10,false,info.owned-1,info.validation_scratch,&out,NULL)==SOURCE_RANGE_LIMIT && !out.objects.items);
    CHECK(source_range_move_plan(c,r,20,10,false,info.owned,info.validation_scratch-1,&out,NULL)==SOURCE_RANGE_LIMIT && !out.objects.items);
    CHECK(source_range_move_plan(c,r,20,10,false,info.owned,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && out.effects==14 && !out.widths.count && out.object_high_water==s->objects.next_id && !memcmp(&before,s,sizeof(before)));
    Workbook *reference=workbook_snapshot(&b); CHECK(reference && worksheet_move_range(reference->sheets,r,20,10,10,3,NULL));
    Worksheet *expected=reference->sheets; CHECK(out.cells.count==expected->cells.count && out.objects.count==expected->objects.count && format_snapshot_matches(&expected->formats,&out.formats));
    for (size_t i=0;i<out.cells.count;i++) CHECK(out.cells.cells[i].row==expected->cells.cells[i].row && out.cells.cells[i].col==expected->cells.cells[i].col && !strcmp(out.cells.cells[i].text,expected->cells.cells[i].text));
    CHECK(out.objects.items[0].row==50 && out.objects.items[0].col==18 && out.objects.items[0].name!=s->objects.items[0].name && out.objects.items[0].source.kind==SOURCE_CSV && out.objects.items[0].source.source_version==8 && out.objects.items[0].source.path!=s->objects.items[0].source.path);
    MixedRangePlan held=out;
    CHECK(source_range_move_plan(c,(SheetRange){30,30,8,8},20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_move_plan(c,r,20,10,true,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_move_plan(c,(SheetRange){0,1,0,1},20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_move_plan(c,r,-20,-4,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_move_plan(c,r,0,0,false,0,0,&out,NULL)==SOURCE_RANGE_NOOP && !memcmp(&held,&out,sizeof(out)));
    CHECK(source_range_move_plan(c,(SheetRange){100,101,100,101},1,1,false,0,0,&out,NULL)==SOURCE_RANGE_NOOP && !memcmp(&held,&out,sizeof(out)));
    c.range.count_pending=true; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.range.count_pending=false;
    c.range.fetch_failed=true; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.range.fetch_failed=false;
    c.range.source_columns=0; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.range.source_columns=3;
    table.count_pending=true; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); table.count_pending=false;
    table.failed=true; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); table.failed=false;
    table.ready=false; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); table.ready=true;
    table.rows=2; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID); table.rows=3;
    c.table_count=0; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.table_count=1;
    SourceTableExtent proofs[2]={table,table}; c.tables=proofs; c.table_count=2;
    CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
    proofs[1].object++; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); c.tables=&table; c.table_count=1;
    s->source_versions.items[0].owner++; CHECK(source_range_move_plan(c,r,20,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); s->source_versions.items[0].owner--;
    CHECK(!memcmp(&before,s,sizeof(before)) && s->source_versions.high_water==8 && s->source_versions.count==1 && !memcmp(&held,&out,sizeof(out)));
    workbook_free(reference); free(reference); workbook_free(&b); CHECK(out.objects.items[0].name && out.cells.cells[0].text); mixed_range_plan_free(&out);
}
static void rows_projection(void) {
    for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b=fixture(); Worksheet *s=b.sheets; Worksheet before=*s; Analysis primary={.kind=SOURCE_DEMO};
        SourceTableExtent table={s->objects.items[0].id,3,2,true,false,false}; SourceMoveContext c={{s,&primary,10,3,false,false,true},&table,1};
        MixedRangePlanInfo info={0}; MixedRangePlan out={0}; bool insert=!deletion;
        CHECK(source_range_rows_measure(c,10,2,insert,&info,NULL)==SOURCE_RANGE_READY);
        CHECK(source_range_rows_plan(c,10,2,insert,info.owned-1,info.validation_scratch,&out,NULL)==SOURCE_RANGE_LIMIT && !out.objects.items);
        CHECK(source_range_rows_plan(c,10,2,insert,info.owned,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && out.effects==14 && !out.widths.count && out.object_high_water==s->objects.next_id && !memcmp(&before,s,sizeof(before)));
        Workbook *reference=workbook_snapshot(&b); CHECK(reference && worksheet_rows(reference->sheets,10,2,insert,10,NULL)); Worksheet *expected=reference->sheets;
        CHECK(out.cells.count==expected->cells.count && out.objects.items[0].row==expected->objects.items[0].row && out.objects.items[0].row==(deletion ? 28u : 32u) && format_snapshot_matches(&expected->formats,&out.formats));
        for (size_t i=0;i<out.cells.count;i++) CHECK(out.cells.cells[i].row==expected->cells.cells[i].row && out.cells.cells[i].col==expected->cells.cells[i].col && !strcmp(out.cells.cells[i].text,expected->cells.cells[i].text) && out.cells.cells[i].text!=s->cells.cells[i].text);
        MixedRangePlan held=out;
        CHECK(source_range_rows_plan(c,9,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
        CHECK(source_range_rows_plan(c,deletion ? 29 : 31,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        CHECK(source_range_rows_plan(c,1000,2,insert,0,0,&out,NULL)==SOURCE_RANGE_NOOP && !memcmp(&held,&out,sizeof(out)));
        CHECK(source_range_rows_plan(c,DX_SHEET_ROWS-1,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        table.count_pending=true; CHECK(source_range_rows_plan(c,10,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); table.count_pending=false;
        c.range.count_pending=true; CHECK(source_range_rows_plan(c,10,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); c.range.count_pending=false;
        CHECK(!memcmp(&before,s,sizeof(before)) && s->source_versions.high_water==7 && !memcmp(&held,&out,sizeof(out)));
        workbook_free(reference); free(reference); workbook_free(&b); CHECK(out.objects.items[0].name && out.cells.cells[0].text); mixed_range_plan_free(&out);
    }
}
static void column_projection(void) {
    for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b=fixture(); Worksheet *s=b.sheets; Analysis primary={.kind=SOURCE_DEMO};
        s->view.column_count=16384; s->view.columns=dx_alloc(16384*sizeof(char *)); s->view.widths=dx_alloc(16384*sizeof(float));
        for (size_t i=0;i<16384;i++) { s->view.columns[i]=dx_strdup(i==4 ? "Fixed name" : ""); s->view.widths[i]=96; }
        s->view.widths[4]=123; s->view.widths[16383]=333; Worksheet before=*s;
        SourceTableExtent table={s->objects.items[0].id,3,2,true,false,false}; SourceMoveContext c={{s,&primary,10,3,false,false,true},&table,1};
        MixedRangePlanInfo info={0}; MixedRangePlan out={0}; bool insert=!deletion;
        CHECK(source_range_column_measure(c,3,insert,&info,NULL)==SOURCE_RANGE_READY);
        CHECK(source_range_column_plan(c,3,insert,info.owned-1,info.validation_scratch,&out,NULL)==SOURCE_RANGE_LIMIT && !out.widths.entries);
        CHECK(source_range_column_plan(c,3,insert,info.owned,info.validation_scratch,&out,NULL)==SOURCE_RANGE_READY && out.effects==30 && out.object_high_water==s->objects.next_id && !memcmp(&before,s,sizeof(before)));
        Workbook *reference=workbook_snapshot(&b); CHECK(reference && worksheet_column(reference->sheets,3,insert,3,NULL)); Worksheet *expected=reference->sheets;
        CHECK(out.cells.count==expected->cells.count && out.objects.items[0].col==expected->objects.items[0].col && out.objects.items[0].col==(deletion ? 7u : 9u) && format_snapshot_matches(&expected->formats,&out.formats));
        for (size_t i=0;i<out.cells.count;i++) CHECK(out.cells.cells[i].row==expected->cells.cells[i].row && out.cells.cells[i].col==expected->cells.cells[i].col && !strcmp(out.cells.cells[i].text,expected->cells.cells[i].text));
        ColumnWidthState expected_widths={0}; CHECK(column_width_capture(expected->view.widths,expected->view.column_count,SIZE_MAX,&expected_widths)==COLUMN_WIDTH_OK && column_width_equal(column_width_view(&expected_widths),column_width_view(&out.widths)));
        CHECK(column_width_at(column_width_view(&out.widths),deletion ? 3 : 5)==column_width_bits(123) && column_width_at(column_width_view(&out.widths),16383)==DX_COLUMN_WIDTH_DEFAULT_BITS && s->view.widths[16383]==333 && !strcmp(s->view.columns[4],"Fixed name")); column_width_free(&expected_widths);
        MixedRangePlan held=out;
        CHECK(source_range_column_plan(c,2,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
        CHECK(source_range_column_plan(c,deletion ? 8 : 9,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        CHECK(source_range_column_plan(c,16384,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        s->view.widths[16383]=96; CHECK(source_range_column_plan(c,16000,insert,0,0,&out,NULL)==SOURCE_RANGE_NOOP && !memcmp(&held,&out,sizeof(out))); s->view.widths[16383]=333;
        s->view.widths[0]=column_width_value(UINT32_C(0x7fc00000)); CHECK(source_range_column_plan(c,3,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); s->view.widths[0]=96;
        table.count_pending=true; CHECK(source_range_column_plan(c,3,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_PROTECTED); table.count_pending=false;
        CHECK(source_range_column_plan(c,16383,insert,SIZE_MAX,SIZE_MAX,&out,NULL)==SOURCE_RANGE_READY && out.effects==16 && out.objects.items[0].col==8 && column_width_at(column_width_view(&out.widths),16383)==DX_COLUMN_WIDTH_DEFAULT_BITS);
        CHECK(!memcmp(&before,s,sizeof(before)) && s->view.widths[16383]==333 && s->source_versions.high_water==7);
        workbook_free(reference); free(reference); workbook_free(&b); CHECK(out.objects.items[0].name && out.cells.cells[0].text && out.widths.entries); mixed_range_plan_free(&out);
    }
}
int main(void) { paste(); clear(); move(); rows_projection(); column_projection(); printf("PASS: %u guarded source range projection checks\n",checks); return 0; }
