#include "ui/value_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL value type:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool same(UIValueType a,UIValueType b) { return a.present==b.present && a.kind==b.kind && a.number.kind==b.number.kind && a.number.precision==b.number.precision && a.number.scale==b.number.scale; }
static void numeric(void) {
    const unsigned widths[]={8,16,32,64,128}; UIValueTypeChooser c;
    CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&c));
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_INTEGER));
    for (unsigned u=0;u<2;u++) for (unsigned i=0;i<5;i++) {
        CHECK(ui_value_type_integer(&c,u!=0,widths[i]) && c.value.number.kind==(NumberKind)((u ? NUM_U8 : NUM_I8)+i));
        ManualValueInfo measured; CHECK(manual_value_measure((ManualValueInput){.present=true,.kind=MANUAL_VALUE_NUMBER,.number=c.value.number,.raw="0",.raw_bytes=1},&measured)==MANUAL_VALUE_OK);
        UIValueTypeChooser previous=c; CHECK(!ui_value_type_integer(&c,u!=0,9) && same(c.value,previous.value));
        c.focus=UI_VALUE_TYPE_SIGN; CHECK(ui_value_type_adjust(&c,1) && c.value.number.kind==(NumberKind)((u ? NUM_I8 : NUM_U8)+i));
        CHECK(ui_value_type_adjust(&c,-1) && same(c.value,previous.value));
    }
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_DECIMAL) && c.value.number.precision==38 && !c.value.number.scale);
    for (unsigned p=1;p<=38;p++) for (unsigned s=0;s<=p;s++) {
        CHECK(ui_value_type_decimal(&c,p,s) && c.value.number.precision==p && c.value.number.scale==s);
        ManualValueInfo measured; CHECK(manual_value_measure((ManualValueInput){.present=true,.kind=MANUAL_VALUE_NUMBER,.number=c.value.number,.raw="0",.raw_bytes=1},&measured)==MANUAL_VALUE_OK);
        UIValueType before=c.value; CHECK(!ui_value_type_decimal(&c,p,p+1) && same(c.value,before));
    }
    UIValueType before=c.value; CHECK(!ui_value_type_decimal(&c,0,0) && !ui_value_type_decimal(&c,39,0) && !ui_value_type_decimal(&c,UINT32_MAX,0) && same(c.value,before));
    CHECK(ui_value_type_decimal(&c,3,3)); c.focus=UI_VALUE_TYPE_PRECISION; CHECK(!ui_value_type_adjust(&c,-1) && c.value.number.precision==3 && c.value.number.scale==3);
    c.focus=UI_VALUE_TYPE_SCALE; CHECK(!ui_value_type_adjust(&c,1)); CHECK(ui_value_type_adjust(&c,-1) && c.value.number.scale==2);
    c.focus=UI_VALUE_TYPE_PRECISION; CHECK(ui_value_type_adjust(&c,-1) && c.value.number.precision==2);
}
int main(void) {
    numeric(); UIValueTypeChooser c={0}; UIValueType result={.present=true,.kind=MANUAL_VALUE_TEXT};
    /* Metadata-only initialization never reads, copies or retains value text. */
    CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT,.raw=(const char *)(uintptr_t)1,.canonical=(const char *)(uintptr_t)2,.raw_bytes=SIZE_MAX},&c));
    CHECK(ui_value_type_finish(&c,true,&result)==UI_VALUE_TYPE_APPLIED && result.present && result.kind==MANUAL_VALUE_TEXT);
    CHECK(ui_value_type_begin((ManualValueView){0},&c)); UIValueTypeFamily family; CHECK(ui_value_type_family(c.value,&family) && family==UI_VALUE_TYPE_CLEAR);
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_NULL) && c.value.present && c.value.kind==MANUAL_VALUE_SQL_NULL);
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_TEXT) && c.value.present && c.value.kind==MANUAL_VALUE_TEXT);
    UIValueType prior=result; CHECK(ui_value_type_finish(&c,false,&result)==UI_VALUE_TYPE_CANCELLED && same(result,prior));
    CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&c));
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_CLEAR) && !c.value.present && c.value.kind==MANUAL_VALUE_SQL_NULL);
    CHECK(ui_value_type_finish(&c,true,&result)==UI_VALUE_TYPE_APPLIED && !result.present);
    CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_TEXT},&c)); UIValueType unchanged=c.value;
    CHECK(!ui_value_type_select(&c,(UIValueTypeFamily)99) && same(c.value,unchanged));
    CHECK(ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_NUMBER,.number={NUM_U128,0,0}},&c));
    UIValueTypeChooser before=c; CHECK(!ui_value_type_begin((ManualValueView){.present=true,.kind=MANUAL_VALUE_NUMBER,.number={NUM_FLOAT,0,0}},&c) && same(c.value,before.value) && c.focus==before.focus && c.active);
    for (unsigned i=0;i<40;i++) { CHECK(ui_value_type_tab(&c,false) && ui_value_type_focus_enabled(&c,c.focus)); CHECK(c.focus!=UI_VALUE_TYPE_PRECISION && c.focus!=UI_VALUE_TYPE_SCALE); }
    for (unsigned i=0;i<40;i++) CHECK(ui_value_type_tab(&c,true) && ui_value_type_focus_enabled(&c,c.focus));
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_DECIMAL));
    for (unsigned i=0;i<40;i++) { CHECK(ui_value_type_tab(&c,false) && ui_value_type_focus_enabled(&c,c.focus)); CHECK(c.focus!=UI_VALUE_TYPE_SIGN && c.focus!=UI_VALUE_TYPE_BITS); }
    c.value.number.kind=NUM_FLOAT; CHECK(!ui_value_type_can_apply(&c)); prior=result;
    CHECK(ui_value_type_finish(&c,true,&result)==UI_VALUE_TYPE_INVALID && c.active && same(result,prior));
    for (unsigned i=0;i<8;i++) { CHECK(ui_value_type_tab(&c,false)); CHECK(c.focus==UI_VALUE_TYPE_FAMILY || c.focus==UI_VALUE_TYPE_CANCEL); }
    CHECK(ui_value_type_select(&c,UI_VALUE_TYPE_TEXT) && ui_value_type_can_apply(&c));
    CHECK(ui_value_type_finish(&c,true,&result)==UI_VALUE_TYPE_APPLIED && result.present && result.kind==MANUAL_VALUE_TEXT);
    CHECK(ui_value_type_finish(&c,true,&result)==UI_VALUE_TYPE_INVALID);
    const UIValueType bad[]={ {false,MANUAL_VALUE_TEXT,{0}}, {true,MANUAL_VALUE_SQL_NULL,{NUM_I8,0,0}}, {true,MANUAL_VALUE_NUMBER,{NUM_DECIMAL,0,0}}, {true,MANUAL_VALUE_NUMBER,{NUM_I64,2,0}}, {true,(ManualValueKind)99,{0}} };
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) CHECK(!ui_value_type_valid(bad[i]));
    printf("PASS: %u explicit value-type metadata transition checks\n",checks); return 0;
}
