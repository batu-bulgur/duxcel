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
int main(void) {
    rows();
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
