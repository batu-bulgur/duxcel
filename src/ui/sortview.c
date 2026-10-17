#include "sortview.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
static Rectangle rect(float x,float y,float w,float h) { return (Rectangle){x,y,w,h}; }
void sort_editor_free(SortEditor *e) {
    analysis_free(&e->draft); input_free(&e->search); free(e->matches); free(e->error); memset(e,0,sizeof(*e));
}
static bool contains(const char *text,const char *search) {
    if (!*search) return true;
    for (;*text;text++) {
        const unsigned char *a=(const unsigned char *)text,*b=(const unsigned char *)search;
        while (*a && *b && tolower(*a)==tolower(*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}
static void match_fields(SortEditor *e,const Page *page) {
    if (e->search_revision==e->search.revision) return;
    e->count=0; e->choice=0; e->field_scroll=0; e->search_revision=e->search.revision;
    for (size_t c=0;c<page->column_count;c++) if (contains(page->columns[c].name,e->search.text)) e->matches[e->count++]=c;
}
void sort_editor_open(SortEditor *e,const Analysis *a,const Page *page,size_t column) {
    sort_editor_free(e); input_set(&e->search,""); e->search_revision=UINT64_MAX; e->just_opened=true;
    e->matches=dx_alloc(page->column_count*sizeof(*e->matches));
    for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) { AnalysisSort sort=analysis_sort_at(a,i); analysis_sort_set(&e->draft,sort.column,sort.descending,true,NULL); }
    match_fields(e,page); if (column<e->count) { e->choice=column; e->field_scroll=(int)column; }
}
static size_t levels(SortEditor *e) { return e->draft.sort_count+(e->draft.sort_column ? 1 : 0); }
static void add_field(SortEditor *e,const char *name) {
    free(e->error); e->error=NULL;
    for (size_t i=0;i<levels(e);i++) if (!strcmp(analysis_sort_at(&e->draft,i).column,name)) { e->level=(int)i; return; }
    if (analysis_sort_set(&e->draft,name,false,true,&e->error)) e->level=(int)levels(e)-1;
}
static void reveal(int *scroll,int selected,int visible,int count) {
    if (selected<*scroll) *scroll=selected;
    if (selected>=*scroll+visible) *scroll=selected-visible+1;
    int max=count-visible; if (max<0) max=0; if (*scroll>max) *scroll=max; if (*scroll<0) *scroll=0;
}
static void wheel_scroll(int *scroll,double *remainder,Rectangle area,int visible,int count) {
    if (CheckCollisionPointRec(GetMousePosition(),area)) *scroll+=ui_scroll_steps(remainder,-GetMouseWheelMoveV().y*3);
    int max=count-visible; if (max<0) max=0; if (*scroll>max) *scroll=max; if (*scroll<0) *scroll=0;
}
static void scrollbar(SortEditor *e,UIContext *u,int id,int *scroll,Rectangle area,int visible,int count) {
    if (count<=visible) return;
    float s=u->scale,thumb=fmaxf(24*s,area.height*visible/count);
    Rectangle track=rect(area.x+area.width-11*s,area.y+2*s,9*s,area.height-4*s);
    thumb=fminf(thumb,track.height); float travel=track.height-thumb;
    Rectangle handle=rect(track.x,track.y+travel*(float)*scroll/(count-visible),track.width,thumb);
    DrawRectangleRec(track,UI_BG); DrawRectangleRounded(handle,.5f,4,(Color){161,174,191,255});
    Vector2 mouse=GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse,track)) { e->dragging=id; e->grab=CheckCollisionPointRec(mouse,handle) ? mouse.y-handle.y : thumb/2; }
    if (e->dragging==id) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) e->dragging=0;
        else if (travel>0) { float fraction=fminf(1,fmaxf(0,(mouse.y-e->grab-track.y)/travel)); *scroll=(int)roundf(fraction*(count-visible)); }
    }
}
SortResult sort_editor_draw(SortEditor *e,UIContext *u,const Page *page) {
    bool input=!e->just_opened; e->just_opened=false;
    if (input && ui_pressed(KEY_ESCAPE)) return SORT_CANCEL;
    float s=u->scale,w=fminf(880*s,GetScreenWidth()-32*s),h=fminf(620*s,GetScreenHeight()-40*s);
    float x=(GetScreenWidth()-w)/2,y=(GetScreenHeight()-h)/2;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){20,32,45,95});
    DrawRectangleRounded(rect(x+3*s,y+5*s,w,h),.025f,4,(Color){20,32,45,28});
    DrawRectangleRounded(rect(x,y,w,h),.025f,4,UI_BG); DrawRectangleLinesEx(rect(x,y,w,h),s,UI_BORDER);
    ui_text(u,"Sort data table",x+22*s,y+19*s,19,UI_TEXT);
    if (ui_button(u,rect(x+w-46*s,y+16*s,28*s,28*s),"×",false,true)) return SORT_CANCEL;
    bool wide=w>=720*s; float content=h-134*s;
    Rectangle fields=wide ? rect(x+18*s,y+60*s,w*.34f-24*s,content) : rect(x+18*s,y+60*s,w-36*s,148*s);
    Rectangle orders=wide ? rect(x+w*.34f+8*s,y+60*s,w*.66f-26*s,content) : rect(x+18*s,y+220*s,w-36*s,fmaxf(90*s,content-160*s));
    Rectangle search=rect(fields.x,fields.y,fields.width,30*s),list=rect(fields.x,fields.y+36*s,fields.width,fields.height-36*s);
    ui_text(u,"Columns · select to add",fields.x+3*s,fields.y-16*s,10,UI_MUTED);
    Rectangle priorities=rect(orders.x,orders.y+28*s,orders.width,orders.height-28*s);
    int field_visible=(int)fmaxf(1,floorf(list.height/(30*s))),order_visible=(int)fmaxf(1,floorf(priorities.height/(59*s)));
    if (input && ui_pressed(KEY_TAB)) e->focus=(e->focus+3+(ui_key_shift(KEY_TAB) ? -1 : 1))%3;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),search)) e->focus=0;
    if (input && e->focus==0) input_update(&e->search); else while (GetCharPressed()) { }
    match_fields(e,page);
    bool enter=input && (ui_pressed(KEY_ENTER)||ui_pressed(KEY_KP_ENTER)),apply=enter && (ui_key_shortcut(ui_pressed(KEY_KP_ENTER) ? KEY_KP_ENTER : KEY_ENTER) || e->focus==2);
    int movement=input ? ui_key(KEY_DOWN)-ui_key(KEY_UP) : 0;
    if (input && e->focus==1) {
        if (ui_key(KEY_PAGE_DOWN)) movement+=field_visible; if (ui_key(KEY_PAGE_UP)) movement-=field_visible;
        int choice=(int)e->choice+movement; if (ui_pressed(KEY_HOME)) choice=0; if (ui_pressed(KEY_END)) choice=(int)e->count-1;
        if (choice<0) choice=0; if (choice>=(int)e->count) choice=(int)e->count-1; e->choice=choice>0 ? (size_t)choice : 0;
        if (movement || ui_pressed(KEY_HOME) || ui_pressed(KEY_END)) reveal(&e->field_scroll,(int)e->choice,field_visible,(int)e->count);
    } else if (input && e->focus==2 && levels(e)) {
        int next=e->level+movement;
        if (next>=0 && next<(int)levels(e)) { if (ui_key_alt(movement<0 ? KEY_UP : KEY_DOWN)) analysis_sort_move(&e->draft,(size_t)e->level,(size_t)next); e->level=next; }
        if (ui_pressed(KEY_LEFT) || ui_pressed(KEY_RIGHT)) {
            AnalysisSort sort=analysis_sort_at(&e->draft,(size_t)e->level); analysis_sort_set(&e->draft,sort.column,ui_pressed(KEY_RIGHT),true,&e->error);
        }
        if (ui_pressed(KEY_DELETE)||ui_pressed(KEY_BACKSPACE)) analysis_sort_remove(&e->draft,(size_t)e->level);
    }
    if (enter && !apply && e->count) { add_field(e,page->columns[e->matches[e->choice]].name); e->focus=1; }
    if (e->level>=(int)levels(e)) e->level=levels(e) ? (int)levels(e)-1 : 0;
    if (e->focus==2 && movement) reveal(&e->order_scroll,e->level,order_visible,(int)levels(e));
    wheel_scroll(&e->field_scroll,&e->field_wheel,list,field_visible,(int)e->count); wheel_scroll(&e->order_scroll,&e->order_wheel,priorities,order_visible,(int)levels(e));
    if (e->focus==0) input_draw(u,&e->search,search);
    else { DrawRectangleRec(search,WHITE); DrawRectangleLinesEx(search,s,UI_BORDER); ui_text_clip(u,*e->search.text ? e->search.text : "Search columns",search,12,UI_MUTED,false); }
    DrawRectangleLinesEx(list,s,e->focus==1 ? UI_ACCENT : UI_BORDER);
    for (int row=e->field_scroll;row<(int)e->count && row<e->field_scroll+field_visible;row++) {
        size_t c=e->matches[row]; Rectangle item=rect(list.x+3*s,list.y+(row-e->field_scroll)*30*s+2*s,list.width-18*s,27*s);
        if (ui_button(u,item,page->columns[c].name,e->focus==1 && (int)e->choice==row,true)) { e->choice=(size_t)row; e->focus=1; add_field(e,page->columns[c].name); reveal(&e->order_scroll,e->level,order_visible,(int)levels(e)); }
    }
    if (!e->count) ui_text_clip(u,"No matching columns",list,12,UI_MUTED,false);
    ui_text(u,"Sort priority · NULLs last",orders.x+4*s,orders.y+4*s,12,UI_MUTED);
    DrawRectangleLinesEx(priorities,s,e->focus==2 ? UI_ACCENT : UI_BORDER);
    for (int i=e->order_scroll;i<(int)levels(e) && i<e->order_scroll+order_visible;i++) {
        AnalysisSort sort=analysis_sort_at(&e->draft,(size_t)i); float top=priorities.y+(i-e->order_scroll)*59*s+3*s;
        Rectangle row=rect(priorities.x+3*s,top,priorities.width-18*s,55*s);
        if (e->level==i) DrawRectangleRec(row,UI_TINT);
        char label[4200]; snprintf(label,sizeof(label),"%d. %s",i+1,sort.column);
        ui_text_clip(u,label,rect(row.x,top,row.width-84*s,23*s),12,UI_TEXT,false);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),row)) { e->level=i; e->focus=2; }
        if (ui_button(u,rect(row.x+row.width-81*s,top,24*s,23*s),"↑",false,i>0)) { analysis_sort_move(&e->draft,(size_t)i,(size_t)i-1); e->level=i-1; break; }
        if (ui_button(u,rect(row.x+row.width-54*s,top,24*s,23*s),"↓",false,i+1<(int)levels(e))) { analysis_sort_move(&e->draft,(size_t)i,(size_t)i+1); e->level=i+1; break; }
        if (ui_button(u,rect(row.x+row.width-27*s,top,24*s,23*s),"×",false,true)) { analysis_sort_remove(&e->draft,(size_t)i); break; }
        if (ui_button(u,rect(row.x+6*s,top+27*s,104*s,24*s),"Ascending",!sort.descending,true)) analysis_sort_set(&e->draft,sort.column,false,true,&e->error);
        if (ui_button(u,rect(row.x+116*s,top+27*s,108*s,24*s),"Descending",sort.descending,true)) analysis_sort_set(&e->draft,sort.column,true,true,&e->error);
    }
    scrollbar(e,u,1,&e->field_scroll,list,field_visible,(int)e->count);
    scrollbar(e,u,2,&e->order_scroll,priorities,order_visible,(int)levels(e));
    if (!levels(e)) ui_text_clip(u,"Choose a column to add a sort level.",priorities,12,UI_MUTED,false);
    ui_text_clip(u,e->error ? e->error : "Tab: search / columns / priorities · Enter: add or apply · Alt+arrows: reorder",rect(x+12*s,y+h-68*s,w-24*s,22*s),10,e->error ? UI_ACCENT : UI_MUTED,false);
    if (ui_button(u,rect(x+18*s,y+h-42*s,100*s,28*s),"Clear all",false,levels(e)>0)) analysis_sorts_clear(&e->draft);
    if (ui_button(u,rect(x+w-230*s,y+h-42*s,100*s,28*s),"Cancel",false,true)) return SORT_CANCEL;
    if (ui_button(u,rect(x+w-118*s,y+h-42*s,100*s,28*s),"Apply sort",true,true) || apply) return SORT_APPLY;
    return SORT_EDITING;
}
