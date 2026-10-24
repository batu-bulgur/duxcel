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
    out.visible=count<UI_CHOICE_LIST_ROWS ? count : UI_CHOICE_LIST_ROWS;
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
    if (!ui || !isfinite(ui->scale) || ui->scale<=0 ||
        !isfinite(row.x) || !isfinite(row.y) || !isfinite(row.width) || !isfinite(row.height) || row.width<=0 || row.height<=0) return false;
    Rectangle visible=ui->clipping ? GetCollisionRec(row,ui->clip) : row;
    if (visible.width<=0 || visible.height<=0) return false;
    float s=ui->scale; bool hover=false,chosen=ui_control_input(ui,row,enabled,&hover);
    /* ActionMenu rows use their own borderless hover/press treatment, rather
       than the toolbar button's outline. Keep the shared 90ms feedback ease. */
    Color ink={39,63,81,255},icon_ink={65,110,138,255},key_ink={94,110,124,255};
    if (!enabled) { ink=Fade(ink,.4f); icon_ink=Fade(icon_ink,.4f); key_ink=Fade(key_ink,.4f); }
    Color base[]={{231,240,246,0},{0},ink};
    bool pressed=chosen || (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    Color colors[]={pressed ? (Color){212,228,238,255} : hover ? (Color){231,240,246,255} : base[0],base[1],ink};
    ui_feedback_colors(ui,row,8,base,colors);
    if (colors[0].a) {
        bool partial=visible.x>row.x || visible.y>row.y || visible.width<row.width || visible.height<row.height;
        bool scissor=partial && ui_scissor_begin(ui,visible);
        if (!partial || scissor) DrawRectangleRounded(row,fminf(1,4*s/fminf(row.width,row.height)),4,colors[0]);
        else DrawRectangleRec(visible,colors[0]);
        if (scissor) ui_scissor_end(ui);
    }
    /* A menu may scroll inside a clipped parent. Text, icons and the focus
       outline must stay inside this row even when its siblings are visible. */
    Rectangle old_clip=ui->clip; bool old_clipping=ui->clipping;
    ui->clip=visible; ui->clipping=true;
    if (checked||icon!=CARBON_COUNT) ui_carbon(ui,checked ? CARBON_CHECKMARK : icon,(Rectangle){row.x+8.5f*s,row.y+(row.height-16*s)*.5f,16*s,16*s},icon_ink);
    float key_width=shortcut ? ui_measure(ui,shortcut,11)+8*s : 0;
    ui_label(ui,label ? label : "",(Rectangle){row.x+32*s,row.y,fmaxf(0,row.width-39*s-key_width),row.height},11.5f,colors[2],false,UI_REGULAR);
    if (shortcut) ui_label(ui,shortcut,(Rectangle){row.x+row.width-7*s-key_width,row.y,key_width,row.height},11,key_ink,true,UI_REGULAR);
    if (focus && enabled) ui_focus_border(row,visible,s);
    ui->clip=old_clip; ui->clipping=old_clipping; return chosen;
}
bool ui_choice_row_input(UIContext *ui,Rectangle row,const char *label,const char *shortcut,UICarbonIcon icon,bool checked,bool focus,bool available,bool accepts_input) {
    if (!ui) return false;
    bool blocked=ui->pointer_blocked; ui->pointer_blocked|=!accepts_input;
    bool chosen=ui_choice_row(ui,row,label,shortcut,icon,checked,focus,available);
    ui->pointer_blocked=blocked; return chosen;
}
