#include "selectview.h"
#include <math.h>

static float select_font_size(UISelectStyle style) {
    return style==UI_SELECT_PLOT ? 10.5f : style==UI_SELECT_FILTER_CONDITION ? 11 : 12;
}

bool ui_select_field(UIContext *u,Rectangle r,const char *label,UISelectStyle style,bool focused,bool enabled) {
    if (!u || !isfinite(u->scale) || u->scale<=0 ||
        !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.width) || !isfinite(r.height) ||
        r.width<=2*u->scale || r.height<=2*u->scale ||
        style<UI_SELECT_REPRESENTATION || style>UI_SELECT_FILTER_CONDITION) return false;
    float s=u->scale,padding=style==UI_SELECT_FONT_SIZE ? 3 : 7;
    float size=select_font_size(style);
    Rectangle visible=u->clipping ? GetCollisionRec(r,u->clip) : r;
    if (visible.width<=0 || visible.height<=0) return false;
    bool hover=false,clicked=ui_control_input(u,r,enabled,&hover);
    (void)hover; /* The reference select retains its white surface on hover. */

    bool partial=visible.x>r.x || visible.y>r.y || visible.width<r.width || visible.height<r.height;
    bool scissor=partial && ui_scissor_begin(u,visible);
    float corner=fminf(1,4*s/fminf(r.width,r.height));
    float edge=focused && enabled ? 2*s : s;
    Color border=focused && enabled ? UI_FOCUS : UI_BORDER;
    if (!partial || scissor) {
        DrawRectangleRounded(r,corner,4,border);
        Rectangle inside={r.x+edge,r.y+edge,r.width-2*edge,r.height-2*edge};
        if (inside.width>0 && inside.height>0) {
            if (edge==s) DrawRectangleRounded(inside,fminf(1,2*s/fminf(inside.width,inside.height)),4,WHITE);
            else DrawRectangleRec(inside,WHITE);
        }
    } else {
        /* Keep every pixel clipped if the parent's bounded scissor stack is
           already full; do not replace its clip or paint through another pane. */
        DrawRectangleRec(visible,WHITE); ui_border(r,visible,edge,border);
    }
    Color ink=enabled ? UI_TEXT : (Color){153,160,172,255};
    float arrow=11*s,right=(padding+1)*s;
    Rectangle text={r.x+(padding+1)*s,r.y,fmaxf(0,r.width-(padding+1)*s-right-arrow-3*s),r.height};
    ui_label(u,label ? label : "",text,size,ink,false,UI_REGULAR);
    ui_carbon(u,CARBON_CHEVRON_DOWN,(Rectangle){r.x+r.width-right-arrow,r.y+(r.height-arrow)*.5f,arrow,arrow},ink);
    if (scissor) ui_scissor_end(u);
    return clicked;
}

UISelectMenuLayout ui_select_menu_layout(Vector2 anchor,Vector2 window,float scale,float width,int count) {
    UISelectMenuLayout out={0};
    if (!isfinite(anchor.x) || !isfinite(anchor.y) || !isfinite(window.x) || !isfinite(window.y) ||
        !isfinite(scale) || scale<=0 || !isfinite(width) || width<=2 ||
        window.x<=0 || window.y<=0 || count<1 || count>64) return out;
    /* Divide in double precision so finite extreme inputs cannot overflow a
       float ratio or an integer capacity cast before the window bounds apply. */
    double available_width=(double)window.x/scale-10,available_height=(double)window.y/scale-10;
    double capacity=floor((available_height-2)/26);
    if (available_width<=2 || capacity<1) return out;
    int visible=capacity>=count ? count : (int)capacity;
    double w=fmin(width,available_width),h=2+26*visible;
    double x=fmax(5,fmin((double)anchor.x/scale,(double)window.x/scale-w-5));
    double y=fmax(5,fmin((double)anchor.y/scale,(double)window.y/scale-h-5));
    out.panel=(Rectangle){(float)(x*scale),(float)(y*scale),(float)(w*scale),(float)(h*scale)};
    out.row_height=26*scale; out.count=count; out.visible=visible;
    out.items=(Rectangle){out.panel.x+scale,out.panel.y+scale,out.panel.width-2*scale,visible*out.row_height};
    out.valid=true; return out;
}
Rectangle ui_select_menu_row(const UISelectMenuLayout *layout,int visible_row) {
    if (!layout || !layout->valid || visible_row<0 || visible_row>=layout->visible ||
        !isfinite(layout->row_height) || layout->row_height<=0) return (Rectangle){0};
    Rectangle row=layout->items; row.y+=visible_row*layout->row_height; row.height=layout->row_height;
    return row;
}
static Rectangle select_clip(const UIContext *ui,Rectangle rect) {
    return ui->clipping ? GetCollisionRec(rect,ui->clip) : rect;
}
static void select_fill(const UIContext *ui,Rectangle rect,Color color) {
    Rectangle visible=select_clip(ui,rect);
    if (visible.width>0 && visible.height>0) DrawRectangleRec(visible,color);
}
void ui_select_menu_frame(UIContext *ui,const UISelectMenuLayout *layout) {
    if (!ui || !layout || !layout->valid || !isfinite(ui->scale) || ui->scale<=0) return;
    float s=ui->scale; Rectangle panel=layout->panel;
    select_fill(ui,(Rectangle){panel.x+s,panel.y+2*s,panel.width,panel.height},(Color){25,42,70,24});
    select_fill(ui,panel,WHITE); ui_border(panel,select_clip(ui,panel),s,UI_BORDER);
}
bool ui_select_menu_option(UIContext *ui,const UISelectMenuLayout *layout,int visible_row,
    const char *label,UISelectStyle style,bool selected,bool focused,bool enabled,bool allow) {
    if (!ui || !isfinite(ui->scale) || ui->scale<=0 || style<UI_SELECT_REPRESENTATION || style>UI_SELECT_FILTER_CONDITION) return false;
    Rectangle row=ui_select_menu_row(layout,visible_row),visible=select_clip(ui,row);
    if (visible.width<=0 || visible.height<=0) return false;
    /* Text/icons have CPU clipping; scope it to this row without adding GPU
       scissor changes or allowing narrow/clipped menus to overpaint siblings. */
    Rectangle old_clip=ui->clip; bool old_clipping=ui->clipping;
    ui->clip=visible; ui->clipping=true;
    bool hover=false,clicked=ui_control_input(ui,row,enabled && allow,&hover);
    float s=ui->scale;
    if (enabled && (hover || focused)) {
        Color fill=hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? (Color){212,228,238,255} : UI_TINT;
        DrawRectangleRec(visible,fill);
    }
    Color ink=enabled ? UI_TEXT : (Color){153,160,172,255};
    if (selected) ui_carbon(ui,CARBON_CHECKMARK,(Rectangle){row.x+4*s,row.y+(row.height-12*s)*.5f,12*s,12*s},ink);
    ui_label(ui,label ? label : "",(Rectangle){row.x+20*s,row.y,fmaxf(0,row.width-24*s),row.height},select_font_size(style),ink,false,UI_REGULAR);
    if (focused && enabled) ui_focus_border(row,visible,s);
    ui->clip=old_clip; ui->clipping=old_clipping;
    return clicked;
}
