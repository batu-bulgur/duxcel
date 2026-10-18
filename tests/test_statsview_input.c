/* Links the actual statsview.c and ui.c. Bounded raylib draw/input recorders
   exercise shared control handling and CPU clipping without a GPU, window,
   font atlas, engine, native event delivery or clipboard. Use -Wl,-dead_strip
   on macOS so unused UI/platform functions need no production dependencies. */
#include "ui/statsview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL statistics input:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef enum { PAINT_RECT,PAINT_BORDER,PAINT_TEXTURE } PaintKind;
typedef struct { PaintKind kind; Rectangle rect; Color color; unsigned texture; } Paint;
static struct {
    Vector2 mouse; bool pressed,down,quarantined;
    Paint paints[256]; size_t count;
} state;
static GlyphInfo glyph={.value='?',.advanceX=5};
static Rectangle glyph_rect={0,0,5,10};

static bool same_rect(Rectangle a,Rectangle b) {
    return fabsf(a.x-b.x)<.001f && fabsf(a.y-b.y)<.001f && fabsf(a.width-b.width)<.001f && fabsf(a.height-b.height)<.001f;
}
static bool inside(Rectangle a,Rectangle b) {
    return a.width>0 && a.height>0 && a.x>=b.x-.001f && a.y>=b.y-.001f && a.x+a.width<=b.x+b.width+.001f && a.y+a.height<=b.y+b.height+.001f;
}
static void record(PaintKind kind,Rectangle rect,Color color,unsigned texture) {
    CHECK(state.count<sizeof(state.paints)/sizeof(*state.paints));
    state.paints[state.count++]=(Paint){kind,rect,color,texture};
}
Vector2 GetMousePosition(void) { return state.mouse; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && state.pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT && state.down; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
void DrawRectangleRec(Rectangle rect,Color color) { record(PAINT_RECT,rect,color,0); }
void DrawRectangleLinesEx(Rectangle rect,float width,Color color) { (void)width; record(PAINT_BORDER,rect,color,0); }
void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle target,Vector2 origin,float rotation,Color color) {
    (void)source; (void)origin; (void)rotation; record(PAINT_TEXTURE,target,color,texture.id);
}
#ifdef DUXCEL_DEV_REVIEW
bool dx_review_input_blocked(void) { return state.quarantined; }
#endif
static void reset(UIContext *ui) {
    memset(&state,0,sizeof(state)); state.mouse=(Vector2){-1000,-1000};
    ui->cursor=MOUSE_CURSOR_CROSSHAIR; ui_pointer_offset((Vector2){0});
    DxPointerGesture gesture; while (ui_take_pointer_gesture(&gesture)) {}
}
static void setup(UIContext *ui,float scale,float density) {
    *ui=(UIContext){.scale=scale,.render_scale={density,density},.icons={.id=2}};
    Font font={.baseSize=10,.glyphCount=1,.texture={.id=1},.recs=&glyph_rect,.glyphs=&glyph};
    ui->body=ui->small=ui->medium=font; reset(ui);
}
static void queue(Vector2 from,Vector2 to,bool right) {
    ui_note_pointer_gesture((DxPointerGesture){from.x,from.y,to.x,to.y,0,right});
}
static bool row(UIContext *ui,Rectangle bounds,bool checked,bool focused,bool enabled) {
    return ui_stats_row(ui,bounds,"Numeric count","1,234.50",checked,focused,enabled);
}
static void expect_paints(Rectangle clip,bool checked,bool focused) {
    unsigned text=0,icons=0;
    CHECK(state.count>0);
    for (size_t i=0;i<state.count;i++) {
        Paint *p=&state.paints[i]; CHECK(inside(p->rect,clip));
        if (p->kind==PAINT_TEXTURE) { text+=p->texture==1; icons+=p->texture==2; }
    }
    CHECK(text>0 && icons==(checked ? 1u : 0u));
    if (focused) {
        unsigned focus=0; for (size_t i=0;i<state.count;i++) focus+=!memcmp(&state.paints[i].color,&UI_FOCUS,sizeof(Color));
        CHECK(focus>0);
    }
}
static void test_translated_pointer(float scale,float density) {
    UIContext ui; setup(&ui,scale,density); Rectangle bounds={40,100,236*scale,27*scale};
    Vector2 offset={17*scale,9*scale},logical={bounds.x+bounds.width*.5f,bounds.y+bounds.height-2*scale};
    ui_pointer_offset(offset); state.mouse=(Vector2){logical.x+offset.x,logical.y+offset.y}; state.pressed=true;
    CHECK(row(&ui,bounds,true,false,true)); CHECK(ui.cursor==MOUSE_CURSOR_POINTING_HAND); expect_paints(bounds,true,false);
    reset(&ui); ui_pointer_offset(offset); state.mouse=(Vector2){logical.x,bounds.y+2*scale}; state.pressed=true;
    CHECK(!row(&ui,bounds,false,false,true)); CHECK(ui.cursor==MOUSE_CURSOR_CROSSHAIR);
    reset(&ui); ui_pointer_offset(offset); queue((Vector2){logical.x+offset.x,logical.y+offset.y},(Vector2){logical.x+offset.x,logical.y+offset.y},false);
    CHECK(row(&ui,bounds,false,true,true) && !ui_has_pointer_gesture()); expect_paints(bounds,false,true);
}
static void test_input_ownership(float scale,float density) {
    UIContext ui; setup(&ui,scale,density); Rectangle bounds={40,100,236*scale,27*scale};
    Vector2 point={bounds.x+30*scale,bounds.y+12*scale};
    for (int guard=0;guard<2;guard++) {
        reset(&ui); ui.pointer_blocked=guard==0; state.mouse=point; state.pressed=true;
        CHECK(!row(&ui,bounds,true,true,guard==0)); CHECK(ui.cursor==MOUSE_CURSOR_CROSSHAIR);
        reset(&ui); queue(point,point,false);
        CHECK(!row(&ui,bounds,false,false,guard==0)); CHECK(ui_has_pointer_gesture());
        ui.pointer_blocked=false; CHECK(row(&ui,bounds,false,false,true)); CHECK(!ui_has_pointer_gesture());
    }
    reset(&ui); queue(point,point,true); CHECK(!row(&ui,bounds,true,false,true)); CHECK(ui_has_pointer_gesture());
    DxPointerGesture retained; CHECK(ui_take_pointer_gesture(&retained) && retained.right);
    reset(&ui); queue(point,(Vector2){point.x,bounds.y+bounds.height+scale},false);
    CHECK(!row(&ui,bounds,false,false,true)); CHECK(!ui_has_pointer_gesture());
    reset(&ui); state.mouse=point; CHECK(!row(&ui,bounds,false,false,true)); CHECK(ui.cursor==MOUSE_CURSOR_POINTING_HAND);
    reset(&ui); state.mouse=point; state.pressed=true; CHECK(row(&ui,bounds,false,false,true));
#ifdef DUXCEL_DEV_REVIEW
    reset(&ui); state.quarantined=true; state.mouse=point; state.pressed=true;
    CHECK(!row(&ui,bounds,true,true,true)); CHECK(ui.cursor==MOUSE_CURSOR_CROSSHAIR);
    reset(&ui); queue(point,point,false); state.quarantined=true;
    CHECK(!row(&ui,bounds,false,false,true)); state.quarantined=false; CHECK(ui_has_pointer_gesture());
    CHECK(row(&ui,bounds,false,false,true) && !ui_has_pointer_gesture());
#endif
}
static void test_parent_clip(float scale,float density) {
    UIContext ui; setup(&ui,scale,density); Rectangle bounds={40,100,236*scale,27*scale};
    Rectangle parent={bounds.x+15*scale,bounds.y+8*scale,bounds.width-25*scale,19*scale};
    ui.clip=parent; ui.clipping=true; ui.scissor_depth=2; ui.paint_depth=1;
    ui.scissor_rects[0]=(Rectangle){0,0,1000,1000}; ui.scissor_rects[1]=parent;
    UIContext before=ui;
    state.mouse=(Vector2){bounds.x+4*scale,bounds.y+10*scale}; state.pressed=true;
    CHECK(!row(&ui,bounds,true,true,true)); CHECK(ui.cursor==MOUSE_CURSOR_CROSSHAIR); expect_paints(parent,true,true);
    CHECK(!memcmp(&ui,&before,sizeof(ui)));
    reset(&ui); Vector2 outside={bounds.x+4*scale,bounds.y+10*scale}; queue(outside,outside,false);
    CHECK(!row(&ui,bounds,false,false,true)); CHECK(ui_has_pointer_gesture()); CHECK(same_rect(ui.clip,parent) && ui.clipping);
    reset(&ui); Vector2 visible={parent.x+35*scale,parent.y+5*scale}; queue(visible,visible,false);
    CHECK(row(&ui,bounds,true,false,true) && !ui_has_pointer_gesture()); expect_paints(parent,true,false);
    CHECK(same_rect(ui.clip,parent) && ui.clipping && ui.scissor_depth==2 && ui.paint_depth==1);
    reset(&ui); queue(visible,outside,false); CHECK(!row(&ui,bounds,false,false,true)); CHECK(!ui_has_pointer_gesture());
    reset(&ui); ui.clip=(Rectangle){-100,-100,10,10}; before=ui; queue(visible,visible,false);
    CHECK(!row(&ui,bounds,true,true,true)); CHECK(!state.count && ui_has_pointer_gesture()); CHECK(!memcmp(&ui,&before,sizeof(ui)));
}
int main(void) {
    const float scales[]={1,1.5f},densities[]={1,2};
    for (size_t s=0;s<sizeof(scales)/sizeof(*scales);s++) for (size_t d=0;d<sizeof(densities)/sizeof(*densities);d++) {
        test_translated_pointer(scales[s],densities[d]); test_input_ownership(scales[s],densities[d]); test_parent_clip(scales[s],densities[d]);
    }
    printf("PASS: %u statistics row checks (actual widget/shared input and glyph clipping; translated pointer, blocked/disabled/quarantined input, queued ownership and nested clips; no GPU/OS)\n",checks);
    return 0;
}
