#include "workbook/format.h"
#include "transforms/number.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL format:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void display(const char *raw,unsigned decimals,bool accounting,const char *expected) {
    char text[512]; CHECK(number_display(raw,decimals,accounting,text)); CHECK(!strcmp(text,expected));
}
int main(void) {
    display("128450.00",2,true,"128,450.00"); display("-24860",2,true,"(24,860.00)"); display("-24860",0,false,"−24,860");
    display("9.995",2,true,"10.00"); display("-0.00005",4,true,"(0.0001)"); display("0.00000000001",4,false,"0.0000");
    display("99999999999999999999999999999999999999.99",0,true,"100,000,000,000,000,000,000,000,000,000,000,000,000");
    display("340282366920938463463374607431768211455",2,true,"340,282,366,920,938,463,463,374,607,431,768,211,455.00");
    display("-170141183460469231731687303715884105728",0,true,"(170,141,183,460,469,231,731,687,303,715,884,105,728)");
    display("1.25e-2",4,false,"0.0125"); display("2.675",2,true,"2.68"); display("1e3",1,true,"1,000.0");
    char text[512]; CHECK(!number_display("nan",2,true,text)); CHECK(!number_display("1e400",2,true,text)); CHECK(!number_display("1",5,true,text));
    FormatStore s={0},copy={0}; CellFormat value=format_default(); char *error=NULL;
    CHECK(format_at(&s,0,0).size==13 && !s.count); value.flags=FORMAT_BOLD;
    CHECK(format_apply(&s,(FormatRange){1,9,2,5},FORMAT_BOLD,value,&error)); CHECK(s.count==1 && s.undo_count==1 && format_at(&s,4,3).flags==FORMAT_BOLD && !format_at(&s,0,3).flags);
    uint64_t revision=s.revision; CHECK(format_apply(&s,(FormatRange){1,9,2,5},FORMAT_BOLD,value,&error) && s.revision==revision && s.undo_count==1);
    value.color=3; value.size=16; CHECK(format_apply(&s,(FormatRange){3,5,3,4},FORMAT_COLOR|FORMAT_SIZE,value,&error));
    CHECK(format_at(&s,4,3).flags==FORMAT_BOLD && format_at(&s,4,3).color==3 && format_at(&s,4,3).size==16 && !format_at(&s,1,3).color);
    value.flags=0; CHECK(format_apply(&s,(FormatRange){4,4,3,3},FORMAT_BOLD,value,&error)); CHECK(!format_at(&s,4,3).flags && format_at(&s,4,3).color==3);
    CHECK(format_history(&s,false) && format_at(&s,4,3).flags==FORMAT_BOLD); CHECK(format_history(&s,true) && !format_at(&s,4,3).flags);
    format_copy(&copy,&s); CHECK(copy.count==s.count && copy.items!=s.items && !copy.undo_count); format_copy(&copy,&copy); CHECK(copy.count==s.count);
    FormatSnapshot plan={0}; CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},10,5,&plan,&error)); CHECK(format_commit(&s,&plan) && !plan.items);
    CHECK(format_at(&s,14,8).size==16 && format_at(&s,14,8).color==3 && !format_at(&s,14,8).flags);
    CHECK(format_at(&s,3,3).size==13 && !format_at(&s,3,3).flags && !format_at(&s,3,3).color && format_at(&s,3,2).flags==FORMAT_BOLD);
    CHECK(format_history(&s,false) && format_at(&s,4,3).color==3 && !format_at(&s,14,8).color);
    CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},1,1,&plan,&error) && format_commit(&s,&plan));
    CHECK(format_at(&s,5,4).size==16 && !format_at(&s,5,4).flags && !format_at(&s,3,3).color);
    CHECK(format_plan_move(&s,(FormatRange){3,5,3,4},0,0,&plan,&error) && !format_commit(&s,&plan));
    CHECK(!format_plan_move(&s,(FormatRange){0,0,0,0},-1,0,&plan,&error)); free(error); error=NULL;
    CHECK(format_plan_axis(&s,true,4,2,true,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,7,4).size==16 && format_at(&s,7,4).color==3);
    CHECK(format_plan_axis(&s,true,4,2,false,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,4).size==16 && !format_at(&s,5,4).flags);
    CHECK(format_plan_axis(&s,false,4,1,true,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,5).size==16);
    CHECK(format_plan_axis(&s,false,4,1,false,&plan,&error) && format_commit(&s,&plan)); CHECK(format_at(&s,5,4).size==16);
    CHECK(format_valid(&s,&error)); format_free(&s); format_free(&copy);
    for (size_t i=0;i<DX_FORMAT_PATCHES;i++) { value.flags=(unsigned char)(i%2 ? FORMAT_ITALIC : FORMAT_BOLD); CHECK(format_apply(&s,(FormatRange){i,i,0,0},FORMAT_BOLD|FORMAT_ITALIC,value,&error)); }
    CHECK(s.count==DX_FORMAT_PATCHES && s.undo_count==DX_FORMAT_HISTORY);
    revision=s.revision; CHECK(!format_apply(&s,(FormatRange){DX_FORMAT_PATCHES,DX_FORMAT_PATCHES,0,0},FORMAT_BOLD,value,&error) && s.revision==revision && s.count==DX_FORMAT_PATCHES); free(error); error=NULL;
    value.flags=0; value.number=FORMAT_ACCOUNTING; CHECK(format_apply(&s,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_ALL,value,&error)); CHECK(s.count==1 && format_at(&s,DX_SHEET_ROWS-1,16383).number==FORMAT_ACCOUNTING);
    CHECK(format_plan_axis(&s,true,DX_SHEET_ROWS-1,1,true,&plan,&error) && !format_commit(&s,&plan));
    format_trim_history(&s,false,3); CHECK(s.undo_count==3); format_free(&s);
    printf("PASS: %u formatting checks (exact numeric display, bounded ranges, precedence, ownership, moves, structural edits, history and whole-sheet selection)\n",checks); return 0;
}
