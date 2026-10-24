#include "versionview.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
UIVersionLayout ui_version_layout(Vector2 viewport,float scale,size_t count) {
    UIVersionLayout l={0}; if (!isfinite(scale) || scale<=0 || viewport.x<300*scale || viewport.y<300*scale) return l;
    float desired=fmaxf(300,260+40*fminf(count,UI_VERSION_VISIBLE));
    float w=fminf(480*scale,viewport.x-24*scale),h=fminf(desired*scale,viewport.y-24*scale);
    l.panel=(Rectangle){(viewport.x-w)/2,(viewport.y-h)/2,w,h}; float x=l.panel.x+16*scale,y=l.panel.y;
    l.heading=(Rectangle){x,y+14*scale,w-32*scale,22*scale}; l.summary=(Rectangle){x,y+40*scale,w-32*scale,18*scale};
    l.latest=(Rectangle){x,y+66*scale,w-32*scale,34*scale};
    size_t visible=(size_t)fmaxf(1,fminf(UI_VERSION_VISIBLE,floorf((h/scale-244)/40)));
    l.visible=count<visible ? count : visible;
    for (size_t i=0;i<l.visible;i++) l.rows[i]=(Rectangle){x,y+(106+40*i)*scale,w-32*scale,38*scale};
    if (count>l.visible && l.visible) {
        l.scrollbar=(Rectangle){x+w-40*scale,l.rows[0].y,8*scale,l.visible*40*scale};
        for (size_t i=0;i<l.visible;i++) l.rows[i].width-=14*scale;
    }
    l.label=(Rectangle){x,y+h-132*scale,w-32*scale,18*scale}; l.input=(Rectangle){x,y+h-110*scale,w-32*scale,30*scale};
    l.message=(Rectangle){x,y+h-76*scale,w-32*scale,24*scale};
    l.create=(Rectangle){x+w-248*scale,y+h-44*scale,124*scale,28*scale};
    l.close=(Rectangle){x+w-116*scale,y+h-44*scale,84*scale,28*scale}; l.valid=true; return l;
}
UIVersionAction ui_version_draw(UIContext *u,UIVersionView *v,TextInput *name,UIVersionContext c,bool pointer) {
    size_t count=c.index ? c.index->tag_count : 0;
    v->layout=ui_version_layout((Vector2){GetScreenWidth(),GetScreenHeight()},u->scale,count); UIVersionLayout *l=&v->layout;
    if (ui_pressed(KEY_ESCAPE)) return UI_VERSION_CLOSE;
    if (!l->valid) return UI_VERSION_NONE;
    if (v->focus<0 || v->focus>3) v->focus=0;
    if (ui_pressed(KEY_TAB)) v->focus=(v->focus+(ui_key_shift(KEY_TAB) ? 3 : 1))%4;
    if (v->selected>=count) v->selected=count ? count-1 : 0;
    if (v->focus==3 && count) {
        if (ui_pressed(KEY_UP) && v->selected) v->selected--;
        if (ui_pressed(KEY_DOWN) && v->selected+1<count) v->selected++;
        if (ui_pressed(KEY_HOME)) v->selected=0;
        if (ui_pressed(KEY_END)) v->selected=count-1;
    }
    bool blocked=u->pointer_blocked; u->pointer_blocked=blocked || !pointer;
    if (pointer && !blocked && l->visible) {
        Rectangle list=l->rows[0]; list.height=l->visible*40*u->scale;
        if (CheckCollisionPointRec(ui_mouse_position(),list)) {
            int steps=ui_scroll_steps(&v->wheel,GetMouseWheelMoveV().y);
            if (steps>0) v->selected=(size_t)steps>v->selected ? 0 : v->selected-(size_t)steps;
            if (steps<0) { size_t next=v->selected+(size_t)(-(int64_t)steps); v->selected=next<count ? next : count-1; }
        }
    }
    if (v->scroll>v->selected) v->scroll=v->selected;
    if (count && v->selected>=v->scroll+l->visible) v->scroll=v->selected-l->visible+1;
    if (l->scrollbar.width) {
        float row=40*u->scale,offset=v->scroll*row;
        if (ui_scrollbar_update(u,l->scrollbar,count*row,l->visible*row,&offset,false,&v->scrollbar,true)) {
            v->scroll=(size_t)floorf(offset/row+.5f);
            if (v->scroll>count-l->visible) v->scroll=count-l->visible;
            if (v->selected<v->scroll) v->selected=v->scroll;
            if (v->selected>=v->scroll+l->visible) v->selected=v->scroll+l->visible-1;
            v->focus=3;
        }
    }
    DrawRectangleRounded(l->panel,.04f,4,UI_SURFACE); DrawRectangleRoundedLinesEx(l->panel,.04f,4,u->scale,UI_BORDER);
    ui_label(u,"Versions",l->heading,15,UI_TEXT,false,UI_SEMIBOLD);
    char summary[96]; snprintf(summary,sizeof(summary),"%zu named version%s · Save updates Latest Saved",count,count==1 ? "" : "s");
    ui_label(u,summary,l->summary,11,UI_MUTED,false,UI_REGULAR);
    DrawRectangleRec(l->latest,UI_TINT);
    Rectangle latest=l->latest; latest.x+=8*u->scale; latest.width-=16*u->scale;
    ui_label(u,c.index && c.index->saved.present ? "Latest Saved" : "No saved version yet",latest,12,UI_TEXT,false,UI_MEDIUM);
    for (size_t i=0;i<l->visible;i++) {
        size_t at=v->scroll+i; const WorkbookVersionTag *tag=c.index->tags+at; Rectangle r=l->rows[i];
        bool hover=false; if (ui_control_input(u,r,true,&hover)) { v->selected=at; v->focus=3; }
        if (at==v->selected) DrawRectangleRec(r,UI_TINT);
        char label[DX_VERSION_LABEL_BYTES+1]; size_t n=tag->label.bytes<DX_VERSION_LABEL_BYTES ? tag->label.bytes : DX_VERSION_LABEL_BYTES;
        memcpy(label,tag->label.data,n); label[n]=0;
        Rectangle title={r.x+8*u->scale,r.y+2*u->scale,r.width-16*u->scale,18*u->scale},detail=title; detail.y+=17*u->scale;
        ui_label(u,label,title,12,UI_TEXT,false,UI_MEDIUM);
        bool saved=c.index->saved.present && !memcmp(tag->root.bytes,c.index->saved.key.bytes,DX_VERSION_KEY_BYTES);
        ui_label(u,saved ? "Same state as Latest Saved" : "Retained named version",detail,10,UI_MUTED,false,UI_REGULAR);
        if (v->focus==3 && at==v->selected) ui_focus_border(r,l->panel,u->scale);
    }
    if (l->scrollbar.width) ui_scrollbar_paint(u,l->scrollbar,count*40*u->scale,l->visible*40*u->scale,v->scroll*40*u->scale,false);
    if (!count) { Rectangle empty={l->heading.x,l->latest.y+48*u->scale,l->heading.width,28*u->scale}; ui_label(u,"Name a version to keep it after later Saves.",empty,12,UI_MUTED,false,UI_REGULAR); }
    ui_label(u,"New version name",l->label,11,UI_MUTED,false,UI_REGULAR);
    bool hover=false; if (ui_control_input(u,l->input,c.writable,&hover)) v->focus=0;
    if (v->focus==0 && c.writable) input_draw_sized(u,name,l->input,12,true);
    else { DrawRectangleRec(l->input,UI_BG); ui_border(l->input,l->panel,u->scale,UI_BORDER); Rectangle r=l->input; r.x+=8*u->scale; r.width-=16*u->scale; ui_label(u,name->text ? name->text : "",r,12,UI_TEXT,false,UI_REGULAR); }
    ui_label(u,c.message && *c.message ? c.message : "Create version also saves the current workbook.",l->message,10,UI_MUTED,false,UI_REGULAR);
    bool enter=ui_pressed(KEY_ENTER) || ui_pressed(KEY_KP_ENTER),space=ui_pressed(KEY_SPACE);
    bool create=ui_button(u,l->create,"Create version",true,c.writable) || (c.writable && ((enter && v->focus<2) || (space && v->focus==1)));
    bool close=ui_button(u,l->close,"Close",false,true) || ((enter || space) && v->focus==2);
    if (v->focus==0) ui_focus_border(l->input,l->panel,u->scale);
    else if (v->focus<3) ui_focus_border(v->focus==1 ? l->create : l->close,l->panel,u->scale);
    u->pointer_blocked=blocked;
    return close ? UI_VERSION_CLOSE : create ? UI_VERSION_CREATE : UI_VERSION_NONE;
}
