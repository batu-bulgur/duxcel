#include "textview.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#ifdef DUXCEL_DEV_REVIEW
#include "../dev/review_input.h"
#endif
void text_lines_free(TextLines *l) { free(l->offsets); memset(l,0,sizeof(*l)); }
void text_lines_layout_sized(TextLines *l,UIContext *u,const char *text,float width,float size) {
    l->width=width; l->scale=u->scale; l->font_size=size; l->raster_size=u->raster_size; l->limited=false;
    size_t capacity=l->capacity;
    if (!l->offsets) { capacity=256; l->offsets=dx_alloc(capacity*sizeof(size_t)); }
    l->count=1; l->offsets[0]=0;
    size_t pos=0,start=0,chars=0,word_break=0; float used=0,space=ui_codepoint_width(u,' ',size);
    while (text[pos]) {
        int bytes=0,cp=GetCodepointNext(text+pos,&bytes); if (bytes<=0) break;
        float w=cp=='\t' ? 4*space : ui_codepoint_width(u,cp,size);
        bool newline=cp=='\n' || cp=='\r',wrap=!newline && pos>start && (used+w>fmaxf(1,width) || chars>=900);
        if (newline || wrap) {
            if (l->count==DX_TEXT_LINES) { l->limited=true; break; }
            if (newline) { pos+=(size_t)bytes; if (cp=='\r' && text[pos]=='\n') pos++; }
            else if (word_break>start) pos=word_break;
            if (l->count+1>=capacity) { capacity*=2; if (capacity>DX_TEXT_LINES+1) capacity=DX_TEXT_LINES+1; l->offsets=dx_realloc(l->offsets,capacity*sizeof(size_t)); }
            l->offsets[l->count++]=pos; start=pos; word_break=pos; used=0; chars=0;
            continue;
        }
        pos+=(size_t)bytes; used+=w; chars+=cp=='\t' ? 4 : 1;
        if (cp==' ' || cp=='\t') word_break=pos;
    }
    l->offsets[l->count]=pos; l->capacity=capacity;
}
void text_lines_layout(TextLines *l,UIContext *u,const char *text,float width) { text_lines_layout_sized(l,u,text,width,13); }
void text_lines_draw_sized(const TextLines *l,UIContext *u,const char *text,size_t first,Rectangle r,float size,float line_height,Color color) {
    float height=line_height*u->scale; size_t visible=(size_t)ceilf(r.height/height);
    Rectangle parent=u->clip,clip=u->clipping ? GetCollisionRec(r,parent) : r; bool clipped=u->clipping;
    if (clip.width<=0 || clip.height<=0) return;
    ui_set_clip(u,clip);
    for (size_t row=0;row<visible && first+row<l->count;row++) {
        Rectangle line_bounds={r.x,r.y+row*height,r.width,height};
        if (line_bounds.y+line_bounds.height<=clip.y || line_bounds.y>=clip.y+clip.height) continue;
        size_t at=first+row,start=l->offsets[at],end=l->offsets[at+1];
        while (end>start && (text[end-1]=='\r' || text[end-1]=='\n')) end--;
        char line[4096]; size_t n=0;
        for (size_t p=start;p<end && n<sizeof(line)-5;p++) {
            if (text[p]=='\t') { memcpy(line+n,"    ",4); n+=4; }
            else line[n++]=text[p];
        }
        line[n]=0; ui_label(u,line,line_bounds,size,color,false,UI_REGULAR);
    }
    u->clip=parent; u->clipping=clipped;
}
void text_lines_draw(const TextLines *l,UIContext *u,const char *text,size_t first,Rectangle r) { text_lines_draw_sized(l,u,text,first,r,13,21,UI_TEXT); }
void text_editor_free(TextEditor *e) { text_lines_free(&e->lines); memset(e,0,sizeof(*e)); }
static size_t editor_line(const TextLines *l,size_t cursor) {
    size_t lo=0,hi=l->count;
    while (lo+1<hi) { size_t mid=lo+(hi-lo)/2; if (l->offsets[mid]<=cursor) lo=mid; else hi=mid; }
    return lo;
}
static size_t editor_end(const TextLines *l,const char *text,size_t row) {
    size_t end=l->offsets[row+1]; while (end>l->offsets[row] && (text[end-1]=='\r'||text[end-1]=='\n')) end--; return end;
}
static float editor_width(UIContext *u,const char *text,size_t start,size_t end) {
    float width=0;
    while (start<end) { int bytes=0,cp=GetCodepointNext(text+start,&bytes); if (bytes<=0) break; width+=ui_codepoint_width(u,cp=='\t' ? ' ' : cp,13)*(cp=='\t' ? 4 : 1); start+=(size_t)bytes; }
    return width;
}
static size_t editor_hit_x(const TextLines *l,UIContext *u,const char *text,size_t row,float x) {
    size_t start=l->offsets[row],end=editor_end(l,text,row);
    while (start<end) { int bytes=0,cp=GetCodepointNext(text+start,&bytes); if (bytes<=0) break; float w=ui_codepoint_width(u,cp=='\t' ? ' ' : cp,13)*(cp=='\t' ? 4 : 1); if (x<w*.5f) break; x-=w; start+=(size_t)bytes; }
    return start;
}
static size_t editor_hit(TextEditor *e,TextInput *in,UIContext *u,Rectangle r,Vector2 point) {
    int row=e->scroll+(int)floorf((point.y-r.y)/(21*u->scale)); if (row<0) row=0; if ((size_t)row>=e->lines.count) row=(int)e->lines.count-1;
    return editor_hit_x(&e->lines,u,in->text,(size_t)row,point.x-r.x);
}
bool text_editor_draw(TextEditor *e,TextInput *in,UIContext *u,Rectangle r,bool active) {
    float s=u->scale,line_height=21*s; Rectangle content={r.x+10*s,r.y+8*s,fmaxf(1,r.width-30*s),fmaxf(1,r.height-16*s)};
    Rectangle parent=u->clip,area=u->clipping ? GetCollisionRec(r,parent) : r; bool clipped=u->clipping;
    Rectangle content_clip=GetCollisionRec(content,area);
    bool interactive=!u->pointer_blocked;
#ifdef DUXCEL_DEV_REVIEW
    interactive=interactive && !dx_review_input_blocked();
#endif
    if (interactive) { in->multiline=true; if (active) input_update(in); }
    if (!e->lines.offsets || e->revision!=in->revision || e->lines.width!=content.width || e->lines.scale!=s || e->lines.raster_size!=u->raster_size) {
        text_lines_layout(&e->lines,u,in->text ? in->text : "",content.width); e->revision=in->revision;
    }
    size_t cursor=in->select_all ? in->length : in->cursor;
    int visible=(int)(content.height/line_height); if (visible<1) visible=1;
    const int keys[]={KEY_UP,KEY_DOWN,KEY_PAGE_UP,KEY_PAGE_DOWN};
    for (size_t i=0;interactive && active && i<sizeof(keys)/sizeof(*keys);i++) if (ui_key(keys[i])) {
        size_t row=editor_line(&e->lines,cursor); int target=(int)row+(keys[i]==KEY_UP ? -1 : keys[i]==KEY_DOWN ? 1 : keys[i]==KEY_PAGE_UP ? -visible : visible);
        if (target<0) target=0; if ((size_t)target>=e->lines.count) target=(int)e->lines.count-1;
        size_t next=ui_key_shortcut(keys[i]) ? (keys[i]==KEY_UP || keys[i]==KEY_PAGE_UP ? 0 : in->length) : editor_hit_x(&e->lines,u,in->text,(size_t)target,editor_width(u,in->text,e->lines.offsets[row],cursor));
        if (!ui_key_shift(keys[i])) in->anchor=next; else if (in->select_all) in->anchor=0;
        in->cursor=cursor=next; in->select_all=false; in->edit_kind=0; break;
    }
    if (interactive && active && (e->revealed_cursor!=cursor || e->revealed_revision!=in->revision)) {
        size_t row=editor_line(&e->lines,cursor);
        if (row<(size_t)e->scroll) e->scroll=(int)row; else if (row>=(size_t)(e->scroll+visible)) e->scroll=(int)row-visible+1;
    }
    Vector2 mouse=ui_mouse_position();
    if (interactive && area.width>0 && area.height>0 && CheckCollisionPointRec(mouse,area)) { u->cursor=MOUSE_CURSOR_IBEAM; e->scroll+=ui_scroll_steps(&e->wheel,-GetMouseWheelMoveV().y*3); }
    int maximum=(int)e->lines.count-visible; if (maximum<0) maximum=0;
    int scroll=e->scroll<0 ? 0 : e->scroll>maximum ? maximum : e->scroll;
    if (interactive) e->scroll=scroll;
    DxPointerGesture gesture={0}; bool replay=interactive && content_clip.width>0 && content_clip.height>0 && ui_take_left_gesture_from(content_clip,&gesture);
    Vector2 pressed=replay ? (Vector2){gesture.x0,gesture.y0} : mouse;
    bool activated=interactive && content_clip.width>0 && content_clip.height>0 && (replay || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && CheckCollisionPointRec(pressed,content_clip);
    if (activated) {
        size_t at=editor_hit(e,in,u,content,pressed); if (!(replay ? gesture.modifiers&DX_MOD_SHIFT : ui_shift())) in->anchor=at; else if (in->select_all) in->anchor=0;
        in->cursor=at; in->select_all=false; in->dragging=true; in->edit_kind=0; active=true;
    }
    if (interactive && active && in->dragging && (replay || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        Vector2 dragged=replay ? (Vector2){gesture.x1,gesture.y1} : mouse;
        if (dragged.y<content.y && e->scroll) e->scroll--; else if (dragged.y>content.y+content.height && e->scroll<maximum) e->scroll++;
        in->cursor=editor_hit(e,in,u,content,dragged); in->select_all=false;
    }
    if (interactive && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) in->dragging=false;
    cursor=in->select_all ? in->length : in->cursor;
    if (interactive) { e->revealed_cursor=cursor; e->revealed_revision=in->revision; scroll=e->scroll; }
    if (area.width>0 && area.height>0) DrawRectangleRec(area,WHITE);
    ui_border(r,area,1.5f*s,active ? UI_ACCENT : UI_BORDER);
    size_t start=in->select_all ? 0 : in->anchor<cursor ? in->anchor : cursor,end=in->select_all ? in->length : in->anchor>cursor ? in->anchor : cursor;
    ui_set_clip(u,content_clip);
    if (active && start<end) for (int i=0;i<=visible && (size_t)(scroll+i)<e->lines.count;i++) {
        size_t row=(size_t)(scroll+i),lo=e->lines.offsets[row],hi=e->lines.offsets[row+1];
        if (start>=hi || end<=lo) continue;
        float left=start<=lo ? 0 : editor_width(u,in->text,lo,start),right=end>=hi ? content.width : editor_width(u,in->text,lo,end);
        Rectangle selection=GetCollisionRec((Rectangle){content.x+left,content.y+i*line_height,fmaxf(0,right-left),line_height},content_clip);
        if (selection.width>0 && selection.height>0) DrawRectangleRec(selection,UI_TINT);
    }
    text_lines_draw(&e->lines,u,in->text ? in->text : "",(size_t)scroll,content);
    if (active && start==end && ((int)(GetTime()*2)%2)==0) {
        size_t row=editor_line(&e->lines,cursor); float x=content.x+editor_width(u,in->text,e->lines.offsets[row],cursor),y=content.y+((int)row-scroll)*line_height;
        float top=fmaxf(y+2*s,content_clip.y),bottom=fminf(y+18*s,content_clip.y+content_clip.height);
        if (x>=content_clip.x && x<=content_clip.x+content_clip.width && bottom>top) DrawLineV((Vector2){x,top},(Vector2){x,bottom},UI_ACCENT);
    }
    if (maximum) {
        Rectangle track={r.x+r.width-12*s,content.y,7*s,content.height}; float height=fminf(track.height,fmaxf(24*s,track.height*visible/e->lines.count)),travel=track.height-height;
        Rectangle thumb={track.x,track.y+travel*scroll/maximum,track.width,height};
        Rectangle visible_thumb=GetCollisionRec(thumb,area),visible_track=GetCollisionRec(track,area);
        if (visible_thumb.width>0 && visible_thumb.height>0) DrawRectangleRounded(visible_thumb,.4f,3,UI_MUTED);
        if (interactive && visible_track.width>0 && visible_track.height>0 && CheckCollisionPointRec(mouse,visible_track)) u->cursor=MOUSE_CURSOR_DEFAULT;
        if (interactive && visible_track.width>0 && visible_track.height>0 && CheckCollisionPointRec(mouse,visible_track) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { e->dragging_scroll=true; e->scroll_grab=CheckCollisionPointRec(mouse,thumb) ? mouse.y-thumb.y : height/2; }
        if (interactive && e->dragging_scroll && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && travel>0) e->scroll=(int)lroundf(fmaxf(0,fminf(1,(mouse.y-track.y-e->scroll_grab)/travel))*maximum);
        if (interactive && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) e->dragging_scroll=false;
    }
    u->clip=parent; u->clipping=clipped;
    return activated;
}
