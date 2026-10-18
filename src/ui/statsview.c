#include "statsview.h"
#include <math.h>
UIStatsLayout ui_stats_layout(Vector2 anchor,Vector2 viewport,float scale) {
    UIStatsLayout out={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(anchor.x) || !isfinite(anchor.y) || !isfinite(viewport.x) || !isfinite(viewport.y)) return out;
    float width=fminf(244,viewport.x/scale-12),height=fminf(312,viewport.y/scale-16);
    if (width<160 || height<200) return out;
    float x=fmaxf(5,fminf(anchor.x/scale,viewport.x/scale-width-6));
    float y=fmaxf(5,fminf(anchor.y/scale,viewport.y/scale-fminf(330,height+18)-10));
    out.panel=(Rectangle){x*scale,y*scale,width*scale,height*scale};
    out.title=(Rectangle){(x+13)*scale,(y+5)*scale,(width-26)*scale,30*scale};
    out.description=(Rectangle){(x+13)*scale,(y+35)*scale,(width-26)*scale,21*scale};
    float row=fminf(27,(height-96)/8);
    for (int i=0;i<8;i++) out.rows[i]=(Rectangle){(x+4)*scale,(y+56+i*row)*scale,(width-8)*scale,row*scale};
    out.separator=(Rectangle){(x+7)*scale,(y+height-36)*scale,(width-14)*scale,scale};
    out.done=(Rectangle){(x+4)*scale,(y+height-31)*scale,(width-8)*scale,26*scale}; out.valid=true; return out;
}
void ui_stats_surface(UIContext *ui,const UIStatsLayout *layout,const char *scope) {
    float s=ui->scale; Rectangle panel=layout->panel;
    for (int i=8;i>0;i--) DrawRectangleRounded((Rectangle){panel.x-i*s,panel.y+(5-i)*s,panel.width+2*i*s,panel.height+2*i*s},.025f,4,(Color){39,61,79,3});
    DrawRectangleRounded(panel,.025f,4,WHITE); DrawRectangleRoundedLinesEx(panel,.025f,4,s,(Color){189,203,215,255});
    ui_label(ui,"Selection statistics",layout->title,12,UI_TEXT,false,UI_MEDIUM);
    ui_label(ui,scope,layout->description,10.5f,UI_MUTED,false,UI_REGULAR); DrawRectangleRec(layout->separator,(Color){224,230,235,255});
}
bool ui_stats_row(UIContext *ui,Rectangle row,const char *name,const char *value,bool checked,bool focused,bool pointer) {
    Rectangle visible=ui->clipping ? GetCollisionRec(row,ui->clip) : row;
    if (visible.width<=0 || visible.height<=0) return false;
    Rectangle saved_clip=ui->clip; bool saved_clipping=ui->clipping;
    ui->clip=visible; ui->clipping=true;
    float s=ui->scale; bool hover=false,click=ui_control_input(ui,row,pointer,&hover);
    if (hover) DrawRectangleRec(visible,(Color){237,243,247,255});
    Rectangle check={row.x+10*s,row.y+(row.height-13*s)*.5f,13*s,13*s};
    Rectangle check_visible=GetCollisionRec(check,visible);
    if (check_visible.width>0 && check_visible.height>0) DrawRectangleRec(check_visible,checked ? (Color){65,110,138,255} : WHITE);
    ui_border(check,visible,s,(Color){122,135,145,255});
    if (checked) ui_carbon(ui,CARBON_CHECKMARK,(Rectangle){check.x+s,check.y+s,11*s,11*s},WHITE);
    float label_width=fminf(ui_measure(ui,name,11.5f),row.width-57*s);
    ui_label(ui,name,(Rectangle){check.x+21*s,row.y,label_width,row.height},11.5f,UI_TEXT,false,UI_REGULAR);
    Rectangle number={check.x+25*s+label_width,row.y,fmaxf(0,row.x+row.width-10*s-check.x-25*s-label_width),row.height};
    float number_width=fminf(number.width,ui_measure(ui,value,10)); number.x+=number.width-number_width; number.width=number_width;
    ui_label(ui,value,number,10,UI_MUTED,false,UI_REGULAR);
    if (focused) ui_border(row,visible,s,UI_FOCUS);
    ui->clip=saved_clip; ui->clipping=saved_clipping; return click;
}
