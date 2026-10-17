#include "choiceview.h"
#include <math.h>
UIChoiceLayout ui_choice_layout(Vector2 origin,Vector2 viewport,float scale,int count,int separator_before) {
    UIChoiceLayout out={0};
    if (!isfinite(scale)||scale<=0||!isfinite(origin.x)||!isfinite(origin.y)||!isfinite(viewport.x)||!isfinite(viewport.y)||count<1||count>UI_CHOICE_MAX||separator_before< -1||separator_before>=count) return out;
    float width=fminf(240,viewport.x/scale-12),height=40+count*28+(separator_before>=0 ? 9 : 0);
    if (width<160||height>viewport.y/scale-16) return out;
    float reserve=fminf(560,(count+(separator_before>=0))*30+46);
    float x=fmaxf(5,fminf(origin.x/scale,viewport.x/scale-width-6));
    float y=fmaxf(5,fminf(origin.y/scale,viewport.y/scale-fmaxf(height,reserve)-10));
    out.panel=(Rectangle){x*scale,y*scale,width*scale,height*scale};
    out.title=(Rectangle){(x+14)*scale,(y+5)*scale,(width-28)*scale,30*scale};
    float row_y=y+35;
    for (int i=0;i<count;i++) {
        if (i==separator_before) { out.separator=(Rectangle){(x+8)*scale,(row_y+4)*scale,(width-16)*scale,scale}; row_y+=9; }
        out.rows[i]=(Rectangle){(x+5)*scale,row_y*scale,(width-10)*scale,28*scale}; row_y+=28;
    }
    out.count=count; out.valid=true; return out;
}
UIChoiceListLayout ui_choice_list_layout(Vector2 origin,Vector2 viewport,float scale,int count) {
    UIChoiceListLayout out={0};
    if (!isfinite(scale)||scale<=0 || !isfinite(viewport.y) || count<1) return out;
    float capacity=floorf((viewport.y/scale-56)/28);
    if (capacity<1) return out;
    out.visible=count<UI_CHOICE_MAX ? count : UI_CHOICE_MAX;
    if (capacity<out.visible) out.visible=(int)capacity;
    out.frame=ui_choice_layout(origin,viewport,scale,out.visible,-1);
    if (!out.frame.valid) return (UIChoiceListLayout){0};
    out.items=out.frame.rows[0]; out.items.height=out.visible*28*scale;
    return out;
}
void ui_choice_frame(UIContext *ui,const UIChoiceLayout *layout,const char *title) {
    if (!layout->valid) return; float s=ui->scale; Rectangle r=layout->panel;
    for (int i=10;i>0;i--) DrawRectangleRounded((Rectangle){r.x-i*s,r.y+(8-i)*s,r.width+2*i*s,r.height+2*i*s},.02f,4,(Color){39,61,79,3});
    float corner=fminf(1,4*s/fminf(r.width,r.height));
    DrawRectangleRounded(r,corner,4,WHITE); DrawRectangleRoundedLinesEx(r,corner,4,s,(Color){183,201,215,255});
    ui_label(ui,title,layout->title,12,UI_TEXT,false,UI_MEDIUM);
    if (layout->separator.height) DrawRectangleRec(layout->separator,(Color){222,229,235,255});
}
bool ui_choice_row(UIContext *ui,Rectangle row,const char *label,const char *shortcut,UICarbonIcon icon,bool checked,bool focus,bool enabled) {
    float s=ui->scale; bool chosen=ui_tool_button(ui,row,"",false,enabled);
    Color ink=enabled ? UI_TEXT : Fade(UI_TEXT,.4f);
    if (checked||icon!=CARBON_COUNT) ui_carbon(ui,checked ? CARBON_CHECKMARK : icon,(Rectangle){row.x+7*s,row.y+6*s,16*s,16*s},ink);
    float key_width=shortcut ? ui_measure(ui,shortcut,10.5f)+8*s : 0;
    ui_label(ui,label,(Rectangle){row.x+32*s,row.y,fmaxf(0,row.width-39*s-key_width),row.height},11.5f,ink,false,UI_REGULAR);
    if (shortcut) ui_label(ui,shortcut,(Rectangle){row.x+row.width-7*s-key_width,row.y,key_width,row.height},10.5f,UI_MUTED,true,UI_REGULAR);
    if (focus) ui_focus_border(row,row,s); return chosen;
}
bool ui_choice_row_input(UIContext *ui,Rectangle row,const char *label,const char *shortcut,UICarbonIcon icon,bool checked,bool focus,bool available,bool accepts_input) {
    bool blocked=ui->pointer_blocked; ui->pointer_blocked|=!accepts_input;
    bool chosen=ui_choice_row(ui,row,label,shortcut,icon,checked,focus,available);
    ui->pointer_blocked=blocked; return chosen;
}
