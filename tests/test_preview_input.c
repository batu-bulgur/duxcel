/* Headless forwarding test for the actual preview module. Link preview.c with
   this file only, raylib and libm; add -DDUXCEL_BROWSER_HOST for browser rules.
   The bounded event, painter and grid-call recorders below never enter an App
   executable. This tests no GPU pixels, OS input or spreadsheet calculation. */
#include "ui/preview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL preview input:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static UIContext ui;
static struct {
    DxKeyModifiers events[32]; size_t count,next;
    GridInput delivered[32]; size_t deliveries;
    Grid *grid; const Page *page;
    float scale; bool enabled,loading; unsigned updates,draws;
} calls;
const Color UI_MUTED={99,113,125,255};

void DrawRectangleRec(Rectangle rect,Color color) { (void)rect; (void)color; }
void DrawLineV(Vector2 start,Vector2 end,Color color) { (void)start; (void)end; (void)color; }
Vector2 GetMousePosition(void) { return (Vector2){-1000,-1000}; }
bool IsMouseButtonPressed(int button) { (void)button; return false; }
float ui_measure(UIContext *u,const char *text,float size) { return (float)strlen(text)*size*u->scale*.5f; }
float ui_measure_weight(UIContext *u,const char *text,float size,UIFontWeight weight) { (void)weight; return ui_measure(u,text,size); }
void ui_label(UIContext *u,const char *text,Rectangle rect,float size,Color color,bool right,UIFontWeight weight) {
    (void)u; (void)text; (void)rect; (void)size; (void)color; (void)right; (void)weight;
}
void ui_carbon(UIContext *u,UICarbonIcon icon,Rectangle rect,Color color) { (void)u; (void)icon; (void)rect; (void)color; }
void ui_text_clip(UIContext *u,const char *text,Rectangle rect,float size,Color color,bool right) {
    (void)u; (void)text; (void)rect; (void)size; (void)color; (void)right;
}
bool ui_take_gesture_from(Rectangle area,DxPointerGesture *gesture) { (void)area; (void)gesture; return false; }
bool ui_take_navigation_key(DxKeyModifiers *event) {
    if (calls.next==calls.count) return false;
    *event=calls.events[calls.next++]; return true;
}
void grid_update(Grid *grid,UIContext *u,const Page *page,Rectangle rect,bool enabled) {
    CHECK(grid==calls.grid && u==&ui && page==calls.page);
    CHECK(rect.width>0 && rect.height>0 && enabled==calls.enabled && u->scale==calls.scale);
    calls.updates++;
}
void grid_handle_input(Grid *grid,UIContext *u,const Page *page,const GridInput *input) {
    CHECK(grid==calls.grid && u==&ui && page==calls.page && u->scale==calls.scale);
    CHECK(calls.deliveries<sizeof(calls.delivered)/sizeof(*calls.delivered));
    calls.delivered[calls.deliveries++]=*input;
}
void grid_draw(Grid *grid,UIContext *u,const Page *page,bool loading) {
    CHECK(grid==calls.grid && u==&ui && page==calls.page);
    CHECK(u->scale==calls.scale && loading==calls.loading); calls.draws++;
}
typedef struct { unsigned bits; bool mac,browser,control,shift,alt; } Modifiers;
static const Modifiers modifiers[]={
    {0,false,false,false,false,false},
    {DX_MOD_SHIFT,false,false,false,true,false},
    {DX_MOD_CONTROL,false,true,true,false,false},
    {DX_MOD_ALT,false,false,false,false,true},
    {DX_MOD_SUPER,true,true,false,false,false},
    {DX_MOD_CONTROL|DX_MOD_SHIFT,false,true,true,true,false},
    {DX_MOD_CONTROL|DX_MOD_ALT,false,true,true,false,true},
    {DX_MOD_SUPER|DX_MOD_SHIFT,true,true,false,true,false},
    {DX_MOD_SUPER|DX_MOD_ALT,true,true,false,false,true},
    {DX_MOD_SHIFT|DX_MOD_ALT,false,false,false,true,true},
    {DX_MOD_CONTROL|DX_MOD_SUPER,true,true,true,false,false},
    {DX_MOD_CONTROL|DX_MOD_SUPER|DX_MOD_SHIFT,true,true,true,true,false},
    {DX_MOD_CONTROL|DX_MOD_SUPER|DX_MOD_ALT,true,true,true,false,true},
    {DX_MOD_CONTROL|DX_MOD_SHIFT|DX_MOD_ALT,false,true,true,true,true},
    {DX_MOD_SUPER|DX_MOD_SHIFT|DX_MOD_ALT,true,true,false,true,true},
    {DX_MOD_CONTROL|DX_MOD_SUPER|DX_MOD_SHIFT|DX_MOD_ALT,true,true,true,true,true}
};
static bool primary(Modifiers value) {
#ifdef DUXCEL_BROWSER_HOST
    return value.browser;
#elif defined(__APPLE__)
    return value.mac;
#else
    return value.control;
#endif
}
static void begin(Grid *grid,const Page *page,float scale,bool enabled,bool loading) {
    memset(&calls,0,sizeof(calls)); memset(&ui,0,sizeof(ui)); ui.scale=scale; ui.digit_width=6;
    *grid=(Grid){.zoom=1.25f,.resizing=-1,.row=17,.col=2,.anchor_row=19,.end_row=11,.top=7,.horizontal=123,.fraction=.375};
    calls.grid=grid; calls.page=page; calls.scale=scale*grid->zoom; calls.enabled=enabled; calls.loading=loading;
}
static void draw(Grid *grid,const Page *page,float scale) {
    Page before=*page;
    CHECK(!ui_preview_draw(&ui,grid,page,(Rectangle){10,20,800*scale,600*scale},2,"Filter City",true,calls.enabled,calls.loading,NULL));
    CHECK(calls.next==calls.count && calls.updates==1 && calls.draws==1);
    CHECK(ui.scale==scale && grid->chrome_scale==scale && grid->digit_width==6 && grid->source_header);
    CHECK(!grid->allow_move && grid->lock_structure==calls.loading);
    CHECK(grid->row==17 && grid->col==2 && grid->anchor_row==19 && grid->end_row==11 && grid->top==7);
    CHECK(grid->horizontal==123 && grid->fraction==.375 && !memcmp(page,&before,sizeof(before)));
}
int main(void) {
    Column column={.name="City"}; Cell cell={.text="NULL"};
    Page page={.columns=&column,.column_count=1,.cells=&cell,.row_count=1,.total=1000};
    const float scales[]={1,1.5f}; const int keys[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN,KEY_HOME,KEY_END};
    for (size_t s=0;s<sizeof(scales)/sizeof(*scales);s++) for (size_t m=0;m<sizeof(modifiers)/sizeof(*modifiers);m++) {
        Grid grid; begin(&grid,&page,scales[s],true,false);
        for (size_t k=0;k<sizeof(keys)/sizeof(*keys);k++) calls.events[calls.count++]=(DxKeyModifiers){keys[k],modifiers[m].bits,k%2!=0};
        draw(&grid,&page,scales[s]); CHECK(calls.deliveries==sizeof(keys)/sizeof(*keys));
        for (size_t k=0;k<calls.deliveries;k++) {
            const GridInput *input=&calls.delivered[k]; CHECK(input->key==keys[k] && input->separate_keyboard);
            CHECK(input->key_shortcut==primary(modifiers[m]) && input->key_shift==modifiers[m].shift && input->key_alt==modifiers[m].alt);
            CHECK(input->mouse.x<0 && input->mouse.y<0 && !input->pressed && !input->down && !input->right);
        }
    }
    /* Do not let a disabled or pointer-owned preview replay keys later. Each
       queued batch is drained but never forwarded through these existing guards. */
    for (int guard=0;guard<7;guard++) {
        Grid grid; begin(&grid,&page,1.5f,guard!=0,false);
        if (guard==1) grid.dragging=true; if (guard==2) grid.resizing=0;
        if (guard==3) grid.resizing=3; if (guard==4) grid.context_requested=true;
        if (guard==5) grid.filter_requested=true; if (guard==6) grid.fit_requested=true;
        calls.events[calls.count++]=(DxKeyModifiers){KEY_END,DX_MOD_CONTROL|DX_MOD_SUPER|DX_MOD_SHIFT,false};
        draw(&grid,&page,1.5f); CHECK(!calls.deliveries);
    }
    Grid grid; begin(&grid,&page,1,true,true);
    calls.events[calls.count++]=(DxKeyModifiers){KEY_HOME,DX_MOD_SUPER,false}; draw(&grid,&page,1);
    CHECK(calls.deliveries==1 && grid.lock_structure);
    printf("PASS: %u preview event-forwarding checks (%s rules; no GPU, OS input or engine)\n",checks,
#ifdef DUXCEL_BROWSER_HOST
        "browser Command/Control"
#elif defined(__APPLE__)
        "native macOS Command"
#else
        "native Control"
#endif
    );
    return 0;
}
