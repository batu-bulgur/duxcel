#include "value_type.h"
static bool integer(NumberKind kind) { return (kind>=NUM_I8 && kind<=NUM_I128) || (kind>=NUM_U8 && kind<=NUM_U128); }
static unsigned bit_width(NumberKind kind) { static const unsigned widths[]={8,16,32,64,128}; return widths[kind>=NUM_U8 ? kind-NUM_U8 : kind-NUM_I8]; }
bool ui_value_type_valid(UIValueType v) {
    if (!v.present) return v.kind==MANUAL_VALUE_SQL_NULL && v.number.kind==NUM_NONE && !v.number.precision && !v.number.scale;
    if (v.kind==MANUAL_VALUE_SQL_NULL || v.kind==MANUAL_VALUE_TEXT) return v.number.kind==NUM_NONE && !v.number.precision && !v.number.scale;
    if (v.kind!=MANUAL_VALUE_NUMBER) return false;
    if (integer(v.number.kind)) return !v.number.precision && !v.number.scale;
    return v.number.kind==NUM_DECIMAL && v.number.precision>=1 && v.number.precision<=38 && v.number.scale<=v.number.precision;
}
bool ui_value_type_family(UIValueType v,UIValueTypeFamily *out) {
    if (!out || !ui_value_type_valid(v)) return false;
    *out=!v.present ? UI_VALUE_TYPE_CLEAR : v.kind==MANUAL_VALUE_SQL_NULL ? UI_VALUE_TYPE_NULL : v.kind==MANUAL_VALUE_TEXT ? UI_VALUE_TYPE_TEXT : v.number.kind==NUM_DECIMAL ? UI_VALUE_TYPE_DECIMAL : UI_VALUE_TYPE_INTEGER; return true;
}
bool ui_value_type_begin(ManualValueView view,UIValueTypeChooser *out) {
    UIValueType value={view.present,view.kind,view.number}; if (!out || !ui_value_type_valid(value)) return false;
    *out=(UIValueTypeChooser){.value=value,.focus=UI_VALUE_TYPE_FAMILY,.active=true}; return true;
}
bool ui_value_type_can_apply(const UIValueTypeChooser *c) { return c && c->active && ui_value_type_valid(c->value); }
bool ui_value_type_select(UIValueTypeChooser *c,UIValueTypeFamily family) {
    if (!c || !c->active) return false;
    UIValueType v={.present=true};
    switch (family) {
    case UI_VALUE_TYPE_TEXT: v.kind=MANUAL_VALUE_TEXT; break;
    case UI_VALUE_TYPE_NULL: v.kind=MANUAL_VALUE_SQL_NULL; break;
    case UI_VALUE_TYPE_CLEAR: v.present=false; break;
    case UI_VALUE_TYPE_INTEGER: v.kind=MANUAL_VALUE_NUMBER; v.number=integer(c->value.number.kind) && ui_value_type_valid(c->value) ? c->value.number : (NumberType){.kind=NUM_I64}; break;
    case UI_VALUE_TYPE_DECIMAL: v.kind=MANUAL_VALUE_NUMBER; v.number=c->value.number.kind==NUM_DECIMAL && ui_value_type_valid(c->value) ? c->value.number : (NumberType){NUM_DECIMAL,38,0}; break;
    default: return false;
    }
    c->value=v; if (!ui_value_type_focus_enabled(c,c->focus)) c->focus=UI_VALUE_TYPE_FAMILY; return true;
}
bool ui_value_type_integer(UIValueTypeChooser *c,bool unsigned_value,unsigned bits) {
    if (!ui_value_type_can_apply(c) || !integer(c->value.number.kind)) return false;
    static const unsigned widths[]={8,16,32,64,128}; unsigned i=0; while (i<5 && widths[i]!=bits) i++; if (i==5) return false;
    c->value.number=(NumberType){.kind=(NumberKind)((unsigned_value ? NUM_U8 : NUM_I8)+i)}; return true;
}
bool ui_value_type_decimal(UIValueTypeChooser *c,unsigned precision,unsigned scale) {
    if (!ui_value_type_can_apply(c) || c->value.number.kind!=NUM_DECIMAL || precision<1 || precision>38 || scale>precision) return false;
    c->value.number=(NumberType){NUM_DECIMAL,(unsigned char)precision,(unsigned char)scale}; return true;
}
bool ui_value_type_focus_enabled(const UIValueTypeChooser *c,UIValueTypeFocus focus) {
    if (!c || !c->active || focus<UI_VALUE_TYPE_FAMILY || focus>=UI_VALUE_TYPE_FOCUS_COUNT) return false;
    if (focus==UI_VALUE_TYPE_FAMILY || focus==UI_VALUE_TYPE_CANCEL) return true;
    if (focus==UI_VALUE_TYPE_APPLY) return ui_value_type_can_apply(c);
    if (!ui_value_type_valid(c->value)) return false;
    if (focus==UI_VALUE_TYPE_SIGN || focus==UI_VALUE_TYPE_BITS) return integer(c->value.number.kind);
    return c->value.number.kind==NUM_DECIMAL;
}
bool ui_value_type_tab(UIValueTypeChooser *c,bool reverse) {
    if (!c || !c->active) return false;
    int next=c->focus>=0 && c->focus<UI_VALUE_TYPE_FOCUS_COUNT ? c->focus : UI_VALUE_TYPE_FAMILY;
    do { next=(next+(reverse ? UI_VALUE_TYPE_FOCUS_COUNT-1 : 1))%UI_VALUE_TYPE_FOCUS_COUNT; } while (!ui_value_type_focus_enabled(c,(UIValueTypeFocus)next));
    c->focus=(UIValueTypeFocus)next; return true;
}
bool ui_value_type_adjust(UIValueTypeChooser *c,int direction) {
    if (!ui_value_type_can_apply(c) || !direction) return false;
    int step=direction>0 ? 1 : -1; NumberType number=c->value.number;
    if (c->focus==UI_VALUE_TYPE_FAMILY) { UIValueTypeFamily family; if (!ui_value_type_family(c->value,&family)) return false; return ui_value_type_select(c,(UIValueTypeFamily)((family+5+step)%5)); }
    if (c->focus==UI_VALUE_TYPE_SIGN && integer(number.kind)) return ui_value_type_integer(c,number.kind<NUM_U8,bit_width(number.kind));
    if (c->focus==UI_VALUE_TYPE_BITS && integer(number.kind)) {
        unsigned width=bit_width(number.kind); if ((width==8 && step<0) || (width==128 && step>0)) return false;
        return ui_value_type_integer(c,number.kind>=NUM_U8,step>0 ? width*2 : width/2);
    }
    if (c->focus==UI_VALUE_TYPE_PRECISION && number.kind==NUM_DECIMAL) {
        int precision=number.precision+step; if (precision<1 || precision>38 || precision<number.scale) return false;
        return ui_value_type_decimal(c,(unsigned)precision,number.scale);
    }
    if (c->focus==UI_VALUE_TYPE_SCALE && number.kind==NUM_DECIMAL) {
        int scale=number.scale+step; if (scale<0 || scale>number.precision) return false;
        return ui_value_type_decimal(c,number.precision,(unsigned)scale);
    }
    return false;
}
UIValueTypeOutcome ui_value_type_finish(UIValueTypeChooser *c,bool apply,UIValueType *out) {
    if (!c || !c->active) return UI_VALUE_TYPE_INVALID;
    if (!apply) { c->active=false; return UI_VALUE_TYPE_CANCELLED; }
    if (!out || !ui_value_type_can_apply(c)) return UI_VALUE_TYPE_INVALID;
    *out=c->value; c->active=false; return UI_VALUE_TYPE_APPLIED;
}
