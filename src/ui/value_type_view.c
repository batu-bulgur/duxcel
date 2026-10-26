#include "value_type_view.h"
#include <stdio.h>
static UIValueTypeViewEvent finish(UIValueTypeChooser *c,bool apply,UIValueType *out) {
    UIValueTypeOutcome outcome=ui_value_type_finish(c,apply,out);
    return outcome==UI_VALUE_TYPE_APPLIED ? UI_VALUE_TYPE_VIEW_APPLY : outcome==UI_VALUE_TYPE_CANCELLED ? UI_VALUE_TYPE_VIEW_CANCEL : UI_VALUE_TYPE_VIEW_IDLE;
}
static void focus(UIContext *u,Rectangle r,Rectangle panel,bool selected,bool enabled) { if (selected && enabled) ui_focus_border(r,panel,u->scale); }
UIValueTypeViewEvent ui_value_type_view_draw(UIValueTypeChooser *c,UIContext *u,Rectangle viewport,bool input,UIValueType *out) {
    if (!c || !u || !c->active) return UI_VALUE_TYPE_VIEW_IDLE;
    UIValueTypeLayout l=ui_value_type_layout(viewport,u->scale); if (!l.valid) return UI_VALUE_TYPE_VIEW_IDLE;
    UIValueTypeViewEvent event=UI_VALUE_TYPE_VIEW_IDLE;
    if (input) {
        if (ui_pressed(KEY_ESCAPE)) event=finish(c,false,out);
        else {
            if (ui_pressed(KEY_TAB)) ui_value_type_tab(c,ui_key_shift(KEY_TAB));
            DxKeyModifiers key;
            while (ui_take_navigation_key(&key)) {
                if (key.key==KEY_UP || key.key==KEY_LEFT) ui_value_type_adjust(c,-1);
                else if (key.key==KEY_DOWN || key.key==KEY_RIGHT) ui_value_type_adjust(c,1);
            }
            if (ui_pressed(KEY_ENTER) || ui_pressed(KEY_KP_ENTER)) event=finish(c,c->focus!=UI_VALUE_TYPE_CANCEL,out);
        }
    }
    bool enabled=input && c->active; float s=u->scale;
    DrawRectangleRounded(l.panel,.04f,4,WHITE); DrawRectangleRoundedLinesEx(l.panel,.04f,4,s,UI_BORDER);
    ui_label(u,"Value type",l.title,14,UI_TEXT,false,UI_MEDIUM);
    const char *labels[]={"Text","SQL NULL","Integer","Decimal","Clear value"}; UIValueTypeFamily family=UI_VALUE_TYPE_TEXT; bool valid=ui_value_type_family(c->value,&family);
    for (unsigned i=0;i<5;i++) {
        if (ui_button(u,l.family[i],labels[i],valid && family==(UIValueTypeFamily)i,enabled)) { ui_value_type_select(c,(UIValueTypeFamily)i); c->focus=UI_VALUE_TYPE_FAMILY; }
        focus(u,l.family[i],l.panel,c->focus==UI_VALUE_TYPE_FAMILY && valid && family==(UIValueTypeFamily)i,enabled);
    }
    valid=ui_value_type_family(c->value,&family);
    if (valid && family==UI_VALUE_TYPE_INTEGER) {
        bool unsigned_value=c->value.number.kind>=NUM_U8;
        static const unsigned widths[]={8,16,32,64,128}; unsigned bits=widths[c->value.number.kind-(unsigned_value ? NUM_U8 : NUM_I8)];
        for (unsigned i=0;i<2;i++) {
            if (ui_button(u,l.sign[i],i ? "Unsigned" : "Signed",unsigned_value==(i!=0),enabled)) { ui_value_type_integer(c,i!=0,bits); c->focus=UI_VALUE_TYPE_SIGN; }
            focus(u,l.sign[i],l.panel,c->focus==UI_VALUE_TYPE_SIGN && unsigned_value==(i!=0),enabled);
        }
        unsigned_value=c->value.number.kind>=NUM_U8;
        for (unsigned i=0;i<5;i++) {
            char label[24]; snprintf(label,sizeof(label),"%u bit",widths[i]);
            if (ui_button(u,l.bits[i],label,bits==widths[i],enabled)) { ui_value_type_integer(c,unsigned_value,widths[i]); c->focus=UI_VALUE_TYPE_BITS; }
            focus(u,l.bits[i],l.panel,c->focus==UI_VALUE_TYPE_BITS && bits==widths[i],enabled);
        }
    } else if (valid && family==UI_VALUE_TYPE_DECIMAL) {
        for (unsigned i=0;i<2;i++) {
            UIValueTypeFocus target=i ? UI_VALUE_TYPE_SCALE : UI_VALUE_TYPE_PRECISION; NumberType n=c->value.number;
            unsigned value=i ? n.scale : n.precision; bool lower=i ? n.scale>0 : n.precision>1 && n.precision>n.scale,upper=i ? n.scale<n.precision : n.precision<38;
            Rectangle label=l.details[i]; label.width=92*s; ui_label(u,i ? "Scale" : "Precision",label,11,UI_TEXT,false,UI_REGULAR);
            if (ui_button(u,l.minus[i],"−",false,enabled && lower)) { c->focus=target; ui_value_type_adjust(c,-1); }
            char number[16]; snprintf(number,sizeof(number),"%u",value);
            if (ui_button(u,l.value[i],number,c->focus==target,enabled)) c->focus=target;
            if (ui_button(u,l.plus[i],"+",false,enabled && upper)) { c->focus=target; ui_value_type_adjust(c,1); }
            focus(u,l.value[i],l.panel,c->focus==target,enabled);
        }
    } else {
        const char *explain=!valid ? "Choose a supported value type." : family==UI_VALUE_TYPE_CLEAR ? "Remove the manual value." : family==UI_VALUE_TYPE_NULL ? "Store an explicit SQL NULL." : "Keep exact characters, including empty text.";
        ui_text_clip(u,explain,l.details[0],11,UI_MUTED,false);
    }
    ui_text_clip(u,"Enter applies this type. Escape cancels.",l.help,10,UI_MUTED,false);
    if (ui_button(u,l.cancel,"Cancel",false,enabled)) event=finish(c,false,out);
    bool apply_enabled=enabled && out && ui_value_type_can_apply(c);
    if (ui_button(u,l.apply,"Apply",true,apply_enabled)) event=finish(c,true,out);
    focus(u,l.cancel,l.panel,c->focus==UI_VALUE_TYPE_CANCEL,enabled); focus(u,l.apply,l.panel,c->focus==UI_VALUE_TYPE_APPLY,apply_enabled);
    return event;
}
