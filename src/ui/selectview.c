#include "selectview.h"
#include <math.h>

bool ui_select_field(UIContext *u,Rectangle r,const char *label,UISelectStyle style,bool focused,bool enabled) {
    if (!u || !isfinite(u->scale) || u->scale<=0 ||
        !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.width) || !isfinite(r.height) ||
        r.width<=2*u->scale || r.height<=2*u->scale ||
        style<UI_SELECT_REPRESENTATION || style>UI_SELECT_NUMBER_FORMAT) return false;
    float s=u->scale,padding=style==UI_SELECT_FONT_SIZE ? 3 : 7;
    float size=style==UI_SELECT_PLOT ? 10.5f : 12;
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
