#include "recoveryview.h"
#include <math.h>
UIRecoveryLayout ui_recovery_layout(Vector2 viewport,float scale,size_t count) {
    UIRecoveryLayout l={0}; if (!isfinite(scale) || scale<=0 || viewport.x<300*scale || viewport.y<300*scale) return l;
    float w=fminf(520*scale,viewport.x-24*scale),h=fminf(390*scale,viewport.y-24*scale);
    l.panel=(Rectangle){(viewport.x-w)/2,(viewport.y-h)/2,w,h}; float x=l.panel.x+16*scale,y=l.panel.y;
    l.heading=(Rectangle){x,y+14*scale,w-32*scale,24*scale}; l.message=(Rectangle){x,y+42*scale,w-32*scale,22*scale};
    size_t visible=(size_t)fmaxf(1,fminf(UI_RECOVERY_VISIBLE,floorf((h/scale-204)/42)));
    l.visible=count<visible ? count : visible;
    for (size_t i=0;i<l.visible;i++) l.rows[i]=(Rectangle){x,y+(70+42*i)*scale,w-32*scale,40*scale};
    l.detail=(Rectangle){x,y+h-122*scale,w-32*scale,24*scale};
    float third=(w-48*scale)/3;
    for (int i=0;i<3;i++) l.buttons[i]=(Rectangle){x+i*(third+8*scale),y+h-46*scale,third,30*scale};
    /* Lower row: recover, checkpoint copy, later. Upper row: refresh, next. */
    l.buttons[4]=l.buttons[2]; l.buttons[2]=(Rectangle){x,y+h-84*scale,100*scale,28*scale};
    l.buttons[3]=(Rectangle){x+108*scale,y+h-84*scale,100*scale,28*scale}; l.valid=true; return l;
}
UIRecoveryAction ui_recovery_draw(UIContext *u,UIRecoveryView *v,UIRecoveryContext c,bool pointer) {
    if (c.count>UI_RECOVERY_ROWS) c.count=UI_RECOVERY_ROWS;
    v->layout=ui_recovery_layout((Vector2){GetScreenWidth(),GetScreenHeight()},u->scale,c.count); UIRecoveryLayout *l=&v->layout;
    if (ui_pressed(KEY_ESCAPE)) return UI_RECOVERY_LATER;
    if (!l->valid) return UI_RECOVERY_NONE;
    if (v->selected>=c.count) v->selected=c.count ? c.count-1 : 0;
    if (v->focus<0 || v->focus>5) v->focus=0;
    if (ui_pressed(KEY_TAB)) v->focus=(v->focus+(ui_key_shift(KEY_TAB) ? 5 : 1))%6;
    if (!v->focus && c.count) {
        if (ui_pressed(KEY_UP) && v->selected) v->selected--;
        if (ui_pressed(KEY_DOWN) && v->selected+1<c.count) v->selected++;
        if (ui_pressed(KEY_HOME)) v->selected=0;
        if (ui_pressed(KEY_END)) v->selected=c.count-1;
    }
    if (pointer && !u->pointer_blocked && !c.busy && l->visible && CheckCollisionPointRec(ui_mouse_position(),(Rectangle){l->rows[0].x,l->rows[0].y,l->rows[0].width,l->visible*42*u->scale})) {
        float wheel=GetMouseWheelMoveV().y;
        if (wheel>0 && v->selected) v->selected--;
        if (wheel<0 && v->selected+1<c.count) v->selected++;
    }
    if (v->scroll>v->selected) v->scroll=v->selected;
    if (c.count && v->selected>=v->scroll+l->visible) v->scroll=v->selected-l->visible+1;
    DrawRectangleRec(l->panel,UI_SURFACE); ui_border(l->panel,l->panel,1,UI_BORDER);
    ui_label(u,"Recover a session",l->heading,15,UI_TEXT,false,UI_SEMIBOLD);
    ui_label(u,c.message ? c.message : "Original files stay unchanged until you choose a session.",l->message,11,UI_MUTED,false,UI_REGULAR);
    bool blocked=u->pointer_blocked; u->pointer_blocked=blocked || !pointer;
    for (size_t i=0;i<l->visible;i++) {
        size_t at=v->scroll+i; if (at>=c.count) break;
        Rectangle r=l->rows[i]; const UIRecoveryRow *row=c.rows+at;
        if (at==v->selected) DrawRectangleRec(r,UI_TINT);
        bool hover=false; if (ui_control_input(u,r,!c.busy,&hover)) { v->selected=at; v->focus=0; }
        Rectangle title={r.x+8*u->scale,r.y+3*u->scale,r.width-16*u->scale,18*u->scale},id=title; id.y+=17*u->scale;
        ui_label(u,row->title,title,12,UI_TEXT,false,UI_MEDIUM); ui_label(u,row->identity,id,10,UI_MUTED,false,UI_REGULAR);
        if (!v->focus && at==v->selected) ui_focus_border(r,l->panel,u->scale);
    }
    if (!c.count) ui_label(u,c.busy ? "Looking for inactive sessions…" : "No recoverable sessions on this page.",(Rectangle){l->heading.x,l->heading.y+62*u->scale,l->heading.width,30*u->scale},12,UI_MUTED,false,UI_REGULAR);
    const UIRecoveryRow *selected=c.count ? c.rows+v->selected : NULL;
    ui_label(u,selected ? selected->detail : "Active sessions are excluded. Refresh checks again.",l->detail,10,UI_MUTED,false,UI_REGULAR);
    const char *labels[]={"Recover changes","Checkpoint copy","Refresh","Next page",c.busy ? "Cancel" : "Later"};
    bool enabled[]={!c.busy && selected && selected->replay,!c.busy && selected && selected->checkpoint,!c.busy,!c.busy && c.more,true};
    UIRecoveryAction result=UI_RECOVERY_NONE;
    for (int i=0;i<5;i++) {
        bool keyboard=v->focus==i+1 && (ui_pressed(KEY_ENTER) || ui_pressed(KEY_KP_ENTER) || ui_pressed(KEY_SPACE));
        if ((ui_popup_button(u,l->buttons[i],labels[i],i==0,false,enabled[i]) || (keyboard && enabled[i]))) result=(UIRecoveryAction)(i+1);
        if (v->focus==i+1) ui_focus_border(l->buttons[i],l->panel,u->scale);
    }
    u->pointer_blocked=blocked; return result;
}
