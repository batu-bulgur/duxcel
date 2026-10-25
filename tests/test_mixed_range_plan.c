#include "workbook/mixed_range_plan.h"
#include "workbook/format_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL mixed range:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static Workbook fixture(bool cells,bool formats) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Mixed",&(Analysis){0},NULL)); Worksheet *s=b.sheets;
    CHECK(objects_add(&s->objects,2,2,"Template",NULL)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; s->objects.items[0].revision=77; worksheet_record(s,EDIT_OBJECTS);
    CHECK(objects_add_chart(&s->objects,40,20,"Outside",&(PivotDefinition){0},NULL)==1); worksheet_record(s,EDIT_OBJECTS);
    if (cells) { CHECK(cells_set(&s->cells,1,1,"9007199254740993.0123456789",NULL)); worksheet_record(s,EDIT_CELLS); CHECK(cells_set(&s->cells,20,0,"NULL",NULL)); worksheet_record(s,EDIT_CELLS); }
    if (formats) { CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&s->formats,(FormatRange){1,4,1,3},FORMAT_BOLD,bold,NULL)); worksheet_record(s,EDIT_FORMAT); }
    b.dirty=false; return b;
}
static void rows(void) {
    for (unsigned mode=0;mode<4;mode++) for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b=fixture(mode&1,mode&2); Worksheet *s=b.sheets; Workbook *reference=workbook_snapshot(&b); CHECK(reference);
        uint64_t row=0,count=2; bool insert=!deletion;
        if (deletion) { s->objects.items[0].row=5; reference->sheets[0].objects.items[0].row=5; }
        Worksheet before=*s; MixedRangePlanInfo info={0}; MixedRangePlan plan={0}; char *error=NULL;
        CHECK(mixed_rows_measure(s,&s->analysis,row,count,insert,&info,&error)==MIXED_RANGE_READY && !error);
        CHECK(mixed_rows_plan(s,&s->analysis,row,count,insert,info.owned-1,info.validation_scratch,&plan,&error)==MIXED_RANGE_LIMIT && !plan.objects.items); free(error); error=NULL;
        CHECK(mixed_rows_plan(s,&s->analysis,row,count,insert,info.owned,info.validation_scratch,&plan,&error)==MIXED_RANGE_READY && !error && plan.object_high_water==s->objects.next_id && !memcmp(&before,s,sizeof(before)));
        CHECK(worksheet_rows(reference->sheets,row,count,insert,0,&error) && !error);
        Worksheet *expected=reference->sheets; CHECK(plan.cells.count==expected->cells.count && plan.objects.count==expected->objects.count);
        for (size_t i=0;i<plan.cells.count;i++) CHECK(plan.cells.cells[i].row==expected->cells.cells[i].row && plan.cells.cells[i].col==expected->cells.cells[i].col && !strcmp(plan.cells.cells[i].text,expected->cells.cells[i].text));
        for (size_t i=0;i<plan.objects.count;i++) CHECK(plan.objects.items[i].row==expected->objects.items[i].row && plan.objects.items[i].col==expected->objects.items[i].col && plan.objects.items[i].id==expected->objects.items[i].id && plan.objects.items[i].name!=s->objects.items[i].name);
        CHECK(format_snapshot_matches(&expected->formats,&plan.formats));
        MixedRangePlan held=plan;
        CHECK(mixed_rows_plan(s,&s->analysis,s->objects.items[0].row+1,1,insert,SIZE_MAX,SIZE_MAX,&plan,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&plan,sizeof(plan)));
        CHECK(mixed_rows_plan(s,&s->analysis,DX_SHEET_ROWS-1,2,insert,SIZE_MAX,SIZE_MAX,&plan,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&plan,sizeof(plan)));
        CHECK(mixed_rows_plan(s,&s->analysis,1000,1,insert,0,0,&plan,NULL)==MIXED_RANGE_NOOP && !memcmp(&held,&plan,sizeof(plan)));
        workbook_free(reference); free(reference); workbook_free(&b); CHECK(plan.objects.items[0].revision==77); mixed_range_plan_free(&plan);
    }
    Workbook b=fixture(false,false); Worksheet *s=b.sheets; MixedRangePlanInfo info={.owned=123};
    s->objects.items[1].row=DX_SHEET_ROWS-s->objects.items[1].rows;
    CHECK(mixed_rows_measure(s,&s->analysis,0,1,true,&info,NULL)==MIXED_RANGE_INVALID && info.owned==123);
    s->objects.items[1].row=40; CHECK(cells_set(&s->cells,DX_SHEET_ROWS-1,0,"Edge",NULL));
    CHECK(mixed_rows_measure(s,&s->analysis,0,1,true,&info,NULL)==MIXED_RANGE_INVALID && info.owned==123);
    workbook_free(&b);
}
static void column_projection(void) {
    for (unsigned mode=0;mode<8;mode++) for (unsigned deletion=0;deletion<2;deletion++) {
        Workbook b=fixture(mode&1,mode&2); Worksheet *s=b.sheets;
        s->view.column_count=16384; s->view.widths=dx_alloc(16384*sizeof(float)); s->view.columns=dx_alloc(16384*sizeof(char *));
        for (size_t i=0;i<16384;i++) { s->view.widths[i]=96; s->view.columns[i]=dx_strdup(i==1 ? "Fixed name" : ""); }
        if (mode&4) { s->view.widths[1]=123; s->view.widths[16383]=333; }
        Workbook *reference=workbook_snapshot(&b); CHECK(reference); Worksheet before=*s; MixedRangePlan out={0}; MixedRangePlanInfo info={0}; char *error=NULL;
        CHECK(mixed_column_measure(s,&s->analysis,0,!deletion,&info,&error)==MIXED_RANGE_READY && !error);
        CHECK(mixed_column_plan(s,&s->analysis,0,!deletion,info.owned-1,info.validation_scratch,&out,&error)==MIXED_RANGE_LIMIT && !out.objects.items && !out.widths.entries); free(error); error=NULL;
        CHECK(mixed_column_plan(s,&s->analysis,0,!deletion,info.owned,info.validation_scratch,&out,&error)==MIXED_RANGE_READY && !error && !memcmp(&before,s,sizeof(before)));
        CHECK(out.effects==(4u|((mode&1)?2u:0u)|((mode&2)?8u:0u)|((mode&4)?16u:0u)) && out.object_high_water==s->objects.next_id);
        CHECK(worksheet_column(reference->sheets,0,!deletion,0,&error) && !error); Worksheet *expected=reference->sheets;
        CHECK(out.cells.count==expected->cells.count && out.objects.count==expected->objects.count && format_snapshot_matches(&expected->formats,&out.formats));
        for (size_t i=0;i<out.cells.count;i++) CHECK(out.cells.cells[i].row==expected->cells.cells[i].row && out.cells.cells[i].col==expected->cells.cells[i].col && !strcmp(out.cells.cells[i].text,expected->cells.cells[i].text));
        for (size_t i=0;i<out.objects.count;i++) CHECK(out.objects.items[i].row==expected->objects.items[i].row && out.objects.items[i].col==expected->objects.items[i].col && out.objects.items[i].id==expected->objects.items[i].id);
        ColumnWidthState widths={0}; CHECK(column_width_capture(expected->view.widths,expected->view.column_count,SIZE_MAX,&widths)==COLUMN_WIDTH_OK && column_width_equal(column_width_view(&widths),column_width_view(&out.widths)));
        if (mode&4) CHECK(column_width_at(column_width_view(&out.widths),deletion ? 0 : 2)==column_width_bits(123) && column_width_at(column_width_view(&out.widths),16383)==DX_COLUMN_WIDTH_DEFAULT_BITS);
        CHECK(!strcmp(s->view.columns[1],"Fixed name") && !strcmp(expected->view.columns[1],"Fixed name")); column_width_free(&widths);
        MixedRangePlan held=out;
        CHECK(mixed_column_plan(s,&s->analysis,deletion ? 2 : 3,!deletion,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        CHECK(mixed_column_plan(s,&s->analysis,16384,!deletion,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        s->view.widths[0]=column_width_value(UINT32_C(0x7fc00000)); CHECK(mixed_column_plan(s,&s->analysis,0,!deletion,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&out,sizeof(out))); s->view.widths[0]=96;
        workbook_free(reference); free(reference); workbook_free(&b); CHECK(!strcmp(out.objects.items[0].name,"Template")); mixed_range_plan_free(&out);
    }
    Workbook b=fixture(false,false); Worksheet *s=b.sheets; MixedRangePlanInfo info={.owned=123};
    CHECK(mixed_column_measure(s,&s->analysis,16383,true,&info,NULL)==MIXED_RANGE_NOOP && info.owned==123);
    s->objects.items[1].col=16384-s->objects.items[1].columns;
    CHECK(mixed_column_measure(s,&s->analysis,0,true,&info,NULL)==MIXED_RANGE_INVALID && info.owned==123);
    s->objects.items[1].col=20; CHECK(cells_set(&s->cells,100,16383,"Edge",NULL));
    CHECK(mixed_column_measure(s,&s->analysis,0,true,&info,NULL)==MIXED_RANGE_INVALID && info.owned==123); workbook_free(&b);
}
int main(void) {
    column_projection(); rows();
    for (unsigned mode=0;mode<4;mode++) {
        Workbook b=fixture(mode&1,mode&2); Worksheet *s=b.sheets; Worksheet before=*s; MixedRangePlanInfo info={0}; MixedRangePlan out={0}; SheetRange r={1,5,1,4}; char *error=NULL;
        CHECK(mixed_range_measure(s,&s->analysis,r,10,10,false,&info,&error)==MIXED_RANGE_READY && !error && info.owned);
        CHECK(mixed_range_plan(s,&s->analysis,r,10,10,false,info.owned-1,info.validation_scratch,&out,&error)==MIXED_RANGE_LIMIT && !out.objects.items); free(error); error=NULL;
        CHECK(mixed_range_plan(s,&s->analysis,r,10,10,false,info.owned,info.validation_scratch-1,&out,&error)==MIXED_RANGE_LIMIT && !out.objects.items); free(error); error=NULL;
        CHECK(mixed_range_plan(s,&s->analysis,r,10,10,false,info.owned,info.validation_scratch,&out,&error)==MIXED_RANGE_READY && !error);
        CHECK(out.effects==(4u|((mode&1)?2u:0u)|((mode&2)?8u:0u)) && out.object_high_water==s->objects.next_id && out.objects.count==2);
        CHECK(out.objects.items!=s->objects.items && out.objects.items[0].name!=s->objects.items[0].name && out.objects.items[0].row==12 && out.objects.items[0].col==12 && out.objects.items[0].revision==77);
        CHECK(out.objects.items[1].row==40 && out.objects.items[1].col==20 && out.objects.items[1].name!=s->objects.items[1].name);
        if (mode&1) CHECK(out.cells.count==2 && out.cells.cells[0].row==11 && out.cells.cells[0].col==11 && !strcmp(out.cells.cells[0].text,"9007199254740993.0123456789") && out.cells.cells[0].text!=s->cells.cells[0].text && !strcmp(out.cells.cells[1].text,"NULL"));
        if (mode&2) { FormatStore moved={.items=out.formats.items,.count=out.formats.count}; CHECK(out.formats.count && out.formats.items!=s->formats.items && (format_at(&moved,11,11).flags&FORMAT_BOLD) && !(format_at(&moved,1,1).flags&FORMAT_BOLD)); }
        CHECK(!memcmp(&before,s,sizeof(before)) && !b.command_revision && !b.dirty);
        MixedRangePlan held=out; info.owned=123;
        CHECK(mixed_range_measure(s,&s->analysis,r,0,0,false,&info,NULL)==MIXED_RANGE_NOOP && info.owned==123);
        CHECK(mixed_range_plan(s,&s->analysis,r,0,0,false,0,0,&out,NULL)==MIXED_RANGE_NOOP && !memcmp(&held,&out,sizeof(out)));
        CHECK(mixed_range_plan(s,&s->analysis,(SheetRange){2,2,2,2},1,1,false,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        CHECK(mixed_range_plan(s,&s->analysis,r,10,10,true,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_PROTECTED && !memcmp(&held,&out,sizeof(out)));
        CHECK(mixed_range_plan(s,&s->analysis,r,38,18,false,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_INVALID && !memcmp(&held,&out,sizeof(out)));
        s->analysis.kind=SOURCE_DEMO; CHECK(mixed_range_plan(s,&s->analysis,r,10,10,false,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_PROTECTED); s->analysis.kind=SOURCE_NONE;
        workbook_free(&b); CHECK(!strcmp(out.objects.items[0].name,"Template") && out.objects.items[0].row==12); mixed_range_plan_free(&out);
    }
    Workbook b=fixture(true,true); Worksheet *s=b.sheets; MixedRangePlan out={0}; MixedRangePlanInfo info={0};
    CHECK(cells_set(&s->cells,11,11,"Occupied",NULL));
    CHECK(mixed_range_measure(s,&s->analysis,(SheetRange){1,5,1,4},10,10,false,&info,NULL)==MIXED_RANGE_OCCUPIED);
    s->objects.items[0].source_id=1; CHECK(mixed_range_measure(s,&s->analysis,(SheetRange){1,5,1,4},10,10,false,&info,NULL)==MIXED_RANGE_PROTECTED); s->objects.items[0].source_id=0;
    CHECK(mixed_range_plan(s,&s->analysis,(SheetRange){20,20,0,0},0,1,true,SIZE_MAX,SIZE_MAX,&out,NULL)==MIXED_RANGE_READY && out.effects==2 && out.objects.items[0].row==2);
    mixed_range_plan_free(&out); workbook_free(&b); printf("PASS: %u complete source-free mixed range plan checks\n",checks); return 0;
}
