#include "ui/textview.h"
#ifdef DUXCEL_DEV_REVIEW
#define DUXCEL_REVIEW_INPUT_IMPLEMENTATION
#include "dev/review_input.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The real textview module runs against bounded paint/input recorders. No
   window, graphics context, clipboard, SQL or native event adapter is used. */
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL text view:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
const Color UI_BG={244,246,248,255},UI_TEXT={32,39,46,255},UI_MUTED={99,113,125,255};
const Color UI_BORDER={202,210,218,255},UI_ACCENT={40,95,134,255},UI_TINT={228,238,246,255};
typedef enum { RECT,LABEL,LINE,THUMB } PaintKind;
typedef struct { PaintKind kind; Rectangle rect,clip; Color color; bool clipping; char text[128]; } Paint;
static struct {
    UIContext *ui; Paint paints[256]; size_t count;
    Vector2 mouse,offset,wheel; bool pressed,down,shift,shortcut,gesture,quarantine;
    DxPointerGesture queued; int key; unsigned updates,wheels,gestures;
} state;
static bool same_rect(Rectangle a,Rectangle b) { return fabsf(a.x-b.x)<.001f && fabsf(a.y-b.y)<.001f && fabsf(a.width-b.width)<.001f && fabsf(a.height-b.height)<.001f; }
static bool inside(Rectangle a,Rectangle b) { return a.width>=0 && a.height>=0 && a.x>=b.x-.001f && a.y>=b.y-.001f && a.x+a.width<=b.x+b.width+.001f && a.y+a.height<=b.y+b.height+.001f; }
static bool same_color(Color a,Color b) { return !memcmp(&a,&b,sizeof(a)); }
static void reset(UIContext *u) { memset(&state,0,sizeof(state)); state.ui=u; state.mouse=(Vector2){-100,-100}; u->cursor=MOUSE_CURSOR_CROSSHAIR; }
static Paint *paint(PaintKind kind,Rectangle r,Color color) {
    CHECK(state.count<sizeof(state.paints)/sizeof(*state.paints));
    Paint *p=state.paints+state.count++; *p=(Paint){.kind=kind,.rect=r,.clip=state.ui->clip,.clipping=state.ui->clipping,.color=color}; return p;
}
void DrawRectangleRec(Rectangle r,Color color) { paint(RECT,r,color); }
void DrawRectangleLinesEx(Rectangle r,float width,Color color) { ui_border(r,(Rectangle){-100000,-100000,200000,200000},width,color); }
void DrawRectangleRounded(Rectangle r,float roundness,int segments,Color color) { (void)roundness; (void)segments; paint(THUMB,r,color); }
void DrawLineV(Vector2 from,Vector2 to,Color color) { paint(LINE,(Rectangle){from.x,from.y,to.x-from.x,to.y-from.y},color); }
/* A border uses the same clipping contract as ui.c, rather than a GPU scissor. */
void ui_border(Rectangle r,Rectangle clip,float width,Color color) {
    Rectangle edges[]={{r.x,r.y,r.width,width},{r.x,r.y+r.height-width,r.width,width},{r.x,r.y,width,r.height},{r.x+r.width-width,r.y,width,r.height}};
    for (size_t i=0;i<4;i++) { Rectangle cut=GetCollisionRec(edges[i],clip); if (cut.width>0 && cut.height>0) DrawRectangleRec(cut,color); }
}
void ui_label(UIContext *u,const char *text,Rectangle r,float size,Color color,bool right,UIFontWeight weight) {
    (void)u; (void)size; (void)right; (void)weight; snprintf(paint(LABEL,r,color)->text,128,"%s",text);
}
void ui_set_clip(UIContext *u,Rectangle clip) { u->clip=u->scissor_depth ? GetCollisionRec(clip,u->scissor_rects[u->scissor_depth-1]) : clip; u->clipping=true; }
void ui_clear_clip(UIContext *u) { u->clipping=u->scissor_depth!=0; if (u->scissor_depth) u->clip=u->scissor_rects[u->scissor_depth-1]; }
float ui_codepoint_width(UIContext *u,int cp,float size) { (void)cp; return size*u->scale*.5f; }
void input_update(TextInput *input) { (void)input; state.updates++; }
Vector2 GetMousePosition(void) { return state.mouse; }
Vector2 ui_mouse_position(void) { return (Vector2){state.mouse.x-state.offset.x,state.mouse.y-state.offset.y}; }
Vector2 GetMouseWheelMoveV(void) { state.wheels++; return state.wheel; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && state.pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT && state.down; }
bool ui_shift(void) { return state.shift; }
bool ui_key(int key) { return state.key==key; }
bool ui_key_shortcut(int key) { (void)key; return state.shortcut; }
bool ui_key_shift(int key) { (void)key; return state.shift; }
bool ui_take_left_gesture_from(Rectangle r,DxPointerGesture *gesture) {
    state.gestures++;
    if (!state.gesture || !CheckCollisionPointRec((Vector2){state.queued.x0,state.queued.y0},r)) return false;
    *gesture=state.queued; state.gesture=false; return true;
}
int ui_scroll_steps(double *remainder,double delta) { double total=*remainder+delta; int steps=(int)total; *remainder=total-steps; return steps; }
double GetTime(void) { return 0; }
#ifdef DUXCEL_DEV_REVIEW
bool dx_review_input_blocked(void) { return state.quarantine; }
#endif
static Rectangle content(Rectangle r,float scale) { return (Rectangle){r.x+10*scale,r.y+8*scale,r.width-30*scale,r.height-16*scale}; }
static void setup(TextEditor *e,TextInput *in,UIContext *u,Rectangle r) {
    char text[2048]=""; for (int i=0;i<50;i++) strcat(text,"alpha βeta\t0123456789\n");
    *in=(TextInput){.text=dx_strdup(text),.length=strlen(text),.capacity=strlen(text)+1,.multiline=true,.revision=7,.active_frame=19,.edit_kind=3};
    *e=(TextEditor){0}; text_lines_layout(&e->lines,u,in->text,content(r,u->scale).width);
    e->revision=in->revision; e->scroll=2; e->wheel=.35; in->cursor=e->lines.offsets[2]+2; in->anchor=in->cursor;
    e->revealed_cursor=in->cursor; e->revealed_revision=in->revision;
    in->undo[0]=(TextInputState){.text=dx_strdup("retained undo"),.length=13,.capacity=14}; in->undo_count=1;
}
static void cleanup(TextEditor *e,TextInput *in) { text_editor_free(e); free(in->text); free(in->undo[0].text); }
static void assert_retained(TextEditor *e,TextInput *in,const TextEditor *before,const TextInput *input,const size_t *offsets) {
    CHECK(!memcmp(e,before,sizeof(*e)) && !memcmp(in,input,sizeof(*in)));
    CHECK(!memcmp(e->lines.offsets,offsets,(e->lines.count+1)*sizeof(*offsets)));
    CHECK(!strcmp(in->undo[0].text,"retained undo"));
    CHECK(!state.updates && !state.wheels && !state.gestures && state.ui->cursor==MOUSE_CURSOR_CROSSHAIR);
}
static void test_blocked(float scale,bool quarantine) {
    UIContext u={.scale=scale,.raster_size=42,.clip={2,3,4,5}}; Rectangle r={20,40,400*scale,120*scale};
    TextEditor e; TextInput in; setup(&e,&in,&u,r); Rectangle c=content(r,scale);
    size_t *offsets=dx_alloc((e.lines.count+1)*sizeof(*offsets)); memcpy(offsets,e.lines.offsets,(e.lines.count+1)*sizeof(*offsets));
    for (int scenario=0;scenario<5;scenario++) {
        reset(&u); u.pointer_blocked=!quarantine; state.quarantine=quarantine;
        if (scenario==4) e.scroll=(int)e.lines.count+100;
        in.dragging=true; e.dragging_scroll=true; e.scroll_grab=3.25f; e.revealed_cursor=0; e.revealed_revision=0;
        state.mouse=scenario==1 ? (Vector2){r.x+r.width-8*scale,c.y+20*scale} : scenario==2 ? (Vector2){c.x+20*scale,c.y+c.height+40*scale} : (Vector2){c.x+20*scale,c.y+10*scale};
        state.pressed=scenario<2; state.down=scenario!=3; state.wheel=(Vector2){0,-.4f}; state.key=KEY_DOWN;
        state.gesture=true; state.queued=(DxPointerGesture){c.x+5*scale,c.y+5*scale,c.x+30*scale,c.y+30*scale,0,false};
        TextEditor before=e; TextInput input=in;
        CHECK(!text_editor_draw(&e,&in,&u,r,true)); assert_retained(&e,&in,&before,&input,offsets);
        CHECK(!u.clipping && same_rect(u.clip,(Rectangle){2,3,4,5}) && state.gesture);
    }
    free(offsets); cleanup(&e,&in);
}
static void test_enabled(float scale) {
    UIContext u={.scale=scale,.raster_size=42}; Rectangle r={20,40,400*scale,120*scale}; TextEditor e; TextInput in; setup(&e,&in,&u,r); Rectangle c=content(r,scale);
    reset(&u); state.mouse=(Vector2){c.x+13*scale,c.y+5*scale}; state.pressed=state.down=true;
    CHECK(text_editor_draw(&e,&in,&u,r,false)); CHECK(in.cursor==e.lines.offsets[2]+2 && in.anchor==in.cursor && in.dragging && !in.select_all);
    CHECK(!state.updates && u.cursor==MOUSE_CURSOR_IBEAM);
    reset(&u); state.mouse=(Vector2){c.x+20*scale,c.y+5*scale}; state.wheel=(Vector2){0,-.4f};
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); CHECK(e.scroll==3 && fabs(e.wheel-.55)<.0001 && !in.dragging && state.updates==1);
    reset(&u); in.dragging=true; size_t old=in.cursor; state.down=true; state.mouse=(Vector2){c.x+26*scale,c.y+c.height+15*scale};
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); CHECK(e.scroll==4 && in.cursor>old && in.dragging);
    reset(&u); CHECK(!text_editor_draw(&e,&in,&u,r,true) && !in.dragging);
    reset(&u); state.pressed=state.down=true; state.mouse=(Vector2){r.x+r.width-8*scale,c.y+c.height-5*scale};
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); CHECK(e.dragging_scroll && e.scroll>4 && u.cursor==MOUSE_CURSOR_DEFAULT);
    reset(&u); state.down=true; state.mouse=(Vector2){r.x+r.width-8*scale,c.y-100*scale};
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); CHECK(!e.scroll && e.dragging_scroll);
    reset(&u); CHECK(!text_editor_draw(&e,&in,&u,r,true) && !e.dragging_scroll);
    cleanup(&e,&in);
}
static void test_clipped_input(float scale) {
    UIContext u={.scale=scale,.raster_size=42,.clipping=true}; Rectangle r={20,40,400*scale,120*scale}; TextEditor e; TextInput in; setup(&e,&in,&u,r); Rectangle c=content(r,scale);
    Rectangle parent={c.x+100*scale,c.y+10*scale,100*scale,40*scale}; u.clip=parent;
    reset(&u); state.pressed=state.down=true; state.mouse=(Vector2){c.x+10*scale,c.y+15*scale}; state.wheel=(Vector2){0,-2};
    TextEditor before=e; TextInput input=in;
    CHECK(!text_editor_draw(&e,&in,&u,r,false)); CHECK(!memcmp(&e,&before,sizeof(e)) && !memcmp(&in,&input,sizeof(in)));
    CHECK(!state.wheels && u.cursor==MOUSE_CURSOR_CROSSHAIR && same_rect(u.clip,parent) && u.clipping);
    reset(&u); u.clip=(Rectangle){c.x,c.y,c.width,c.height}; state.offset=(Vector2){17*scale,9*scale}; state.pressed=state.down=true;
    state.mouse=(Vector2){c.x+13*scale+state.offset.x,c.y+5*scale+state.offset.y};
    CHECK(text_editor_draw(&e,&in,&u,r,false)); CHECK(in.cursor==e.lines.offsets[2]+2);
    reset(&u); state.gesture=true; state.queued=(DxPointerGesture){c.x+13*scale,c.y+5*scale,c.x+45.5f*scale,c.y+5*scale,DX_MOD_SHIFT,false};
    size_t anchor=in.anchor;
    CHECK(text_editor_draw(&e,&in,&u,r,false)); CHECK(in.anchor==anchor && in.cursor==e.lines.offsets[2]+8 && !state.gesture);
    cleanup(&e,&in);
}
static void test_navigation(float scale) {
    UIContext u={.scale=scale,.raster_size=42}; Rectangle r={20,40,400*scale,120*scale}; TextEditor e; TextInput in; setup(&e,&in,&u,r);
    size_t anchor=in.anchor; reset(&u); state.key=KEY_DOWN; state.shift=true;
    CHECK(!text_editor_draw(&e,&in,&u,r,true));
    CHECK(state.updates==1 && in.cursor==e.lines.offsets[3]+2 && in.anchor==anchor && !in.select_all && !in.edit_kind);
    CHECK(in.undo_count==1 && !strcmp(in.undo[0].text,"retained undo"));
    reset(&u); state.key=KEY_PAGE_DOWN; state.shortcut=true; state.shift=true;
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); CHECK(in.cursor==in.length && in.anchor==anchor);
    reset(&u); state.key=KEY_UP; TextInput before=in;
    CHECK(!text_editor_draw(&e,&in,&u,r,false)); CHECK(!state.updates && !memcmp(&in,&before,sizeof(in)));
    cleanup(&e,&in);
}
static void test_paint_clipping(float scale) {
    UIContext u={.scale=scale,.raster_size=42,.clipping=true,.pointer_blocked=true}; Rectangle r={20,40,400*scale,422*scale}; TextEditor e; TextInput in; setup(&e,&in,&u,r); Rectangle c=content(r,scale);
    Rectangle parent={c.x+5*scale,c.y+5*scale,c.width-10*scale,c.height-8*scale}; u.clip=parent;
    u.scissor_depth=1; u.scissor_rects[0]=parent;
    in.select_all=true; e.scroll=0; reset(&u);
    CHECK(!text_editor_draw(&e,&in,&u,r,true));
    unsigned selections=0,labels=0; bool partial=false;
    for (size_t i=0;i<state.count;i++) {
        Paint *p=state.paints+i;
        if (p->kind==RECT || p->kind==THUMB) CHECK(inside(p->rect,parent));
        if (p->kind==RECT && same_color(p->color,UI_TINT)) { selections++; CHECK(inside(p->rect,GetCollisionRec(c,parent))); partial|=p->rect.height<21*scale-.001f; }
        if (p->kind==LABEL) { labels++; CHECK(p->clipping && inside(p->clip,parent) && inside(p->clip,c)); }
    }
    CHECK(selections>10 && labels>10 && partial && same_rect(u.clip,parent) && u.clipping && u.scissor_depth==1);
    in.select_all=false; in.cursor=in.anchor=0; e.scroll=0;
    parent=(Rectangle){c.x,c.y+5*scale,c.width,c.height-5*scale}; u.clip=u.scissor_rects[0]=parent; reset(&u);
    CHECK(!text_editor_draw(&e,&in,&u,r,true)); unsigned carets=0;
    for (size_t i=0;i<state.count;i++) if (state.paints[i].kind==LINE) { CHECK(inside(state.paints[i].rect,parent)); CHECK(fabsf(state.paints[i].rect.y-parent.y)<.001f); carets++; }
    CHECK(carets==1 && same_rect(u.clip,parent));
    parent.x+=20*scale; parent.width-=20*scale; u.clip=u.scissor_rects[0]=parent; reset(&u);
    CHECK(!text_editor_draw(&e,&in,&u,r,true));
    for (size_t i=0;i<state.count;i++) CHECK(state.paints[i].kind!=LINE);
    cleanup(&e,&in);
}
static void test_lines_parent(float scale) {
    UIContext u={.scale=scale,.clipping=true}; TextLines lines={0}; const char *text="alpha\nβeta\nthird\nfourth\n";
    Rectangle r={20,40,200*scale,84*scale}; text_lines_layout(&lines,&u,text,r.width);
    Rectangle parent={r.x+10*scale,r.y+22*scale,100*scale,18*scale}; u.clip=parent; size_t *retained=lines.offsets;
    reset(&u); text_lines_draw(&lines,&u,text,0,r); CHECK(same_rect(u.clip,parent) && u.clipping && lines.offsets==retained);
    unsigned labels=0;
    for (size_t i=0;i<state.count;i++) if (state.paints[i].kind==LABEL) { CHECK(!strcmp(state.paints[i].text,"βeta") && same_rect(state.paints[i].clip,parent)); labels++; }
    CHECK(labels==1);
    u.clip=(Rectangle){10000,10000,5,5}; parent=u.clip; reset(&u); text_lines_draw(&lines,&u,text,0,r);
    CHECK(!state.count && same_rect(u.clip,parent) && u.clipping);
    u.clipping=false; reset(&u); text_lines_draw(&lines,&u,text,0,r); CHECK(!u.clipping && same_rect(u.clip,parent));
    text_lines_free(&lines);
}
int main(void) {
    const float scales[]={.8f,1,1.5f,2.5f};
    for (size_t i=0;i<sizeof(scales)/sizeof(*scales);i++) {
        test_blocked(scales[i],false); test_enabled(scales[i]); test_clipped_input(scales[i]); test_navigation(scales[i]); test_paint_clipping(scales[i]); test_lines_parent(scales[i]);
#ifdef DUXCEL_DEV_REVIEW
        test_blocked(scales[i],true);
#endif
    }
    printf("PASS: %u text-editor checks (actual module; blocked press/wheel/drag/thumb, enabled UTF8 gestures, clipped selection/caret and parent restoration; no GPU/OS)\n",checks);
    return 0;
}
