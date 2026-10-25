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
int main(void) { paste(); clear(); printf("PASS: %u guarded source range projection checks\n",checks); return 0; }
