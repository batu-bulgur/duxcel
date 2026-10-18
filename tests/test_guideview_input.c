/* Actual Guide/shared UI/text layout against bounded raylib input/draw stubs.
   No window, graphics context, native events, clipboard or database. */
#include "ui/guideview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Guide input:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct { Vector2 mouse; int width,height,key; bool pressed,quarantined; unsigned paints; } state;
static GlyphInfo glyph={.value='?',.advanceX=5};
static Rectangle glyph_rect={0,0,5,10};
int GetScreenWidth(void) { return state.width; }
int GetScreenHeight(void) { return state.height; }
Vector2 GetMousePosition(void) { return state.mouse; }
Vector2 GetMouseWheelMoveV(void) { return (Vector2){0}; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && state.pressed; }
bool IsMouseButtonDown(int button) { (void)button; return false; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
bool IsKeyPressed(int key) { return key==state.key && key!=0; }
bool IsKeyPressedRepeat(int key) { (void)key; return false; }
bool IsKeyDown(int key) { (void)key; return false; }
void DrawRectangle(int x,int y,int width,int height,Color color) { (void)x; (void)y; (void)width; (void)height; (void)color; state.paints++; }
void DrawRectangleRec(Rectangle rect,Color color) { (void)rect; (void)color; state.paints++; }
void DrawRectangleLinesEx(Rectangle rect,float width,Color color) { (void)rect; (void)width; (void)color; state.paints++; }
void DrawRectangleRounded(Rectangle rect,float roundness,int segments,Color color) { (void)rect; (void)roundness; (void)segments; (void)color; state.paints++; }
void DrawRectangleRoundedLinesEx(Rectangle rect,float roundness,int segments,float width,Color color) { (void)rect; (void)roundness; (void)segments; (void)width; (void)color; state.paints++; }
void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle target,Vector2 origin,float rotation,Color color) {
    (void)texture; (void)source; (void)target; (void)origin; (void)rotation; (void)color; state.paints++;
}
#ifdef DUXCEL_DEV_REVIEW
bool dx_review_input_blocked(void) { return state.quarantined; }
#endif
static void reset(UIContext *ui) {
    state.mouse=(Vector2){-1000,-1000}; state.key=0; state.pressed=state.quarantined=false; state.paints=0;
    ui->cursor=MOUSE_CURSOR_CROSSHAIR; ui->pointer_blocked=false; ui->clipping=false; ui_pointer_offset((Vector2){0});
    DxPointerGesture gesture; while (ui_take_pointer_gesture(&gesture)) {}
}
static void setup(UIContext *ui,UIGuide *guide,float scale,int width,int height) {
    *ui=(UIContext){.scale=scale,.raster_size=20,.render_scale={2,2},.icons={.id=2},.reduced_motion=true}; *guide=(UIGuide){0};
    Font font={.baseSize=10,.glyphCount=1,.texture={.id=1},.recs=&glyph_rect,.glyphs=&glyph}; ui->body=ui->small=ui->medium=font;
    state.width=width; state.height=height; reset(ui);
    CHECK(!ui_guide_draw(ui,guide,false)); CHECK(guide->layout.valid && guide->layout.panel.x>0 && guide->layout.panel.y>0 && guide->intro.offsets && guide->note.offsets);
    reset(ui);
}
static Vector2 center(Rectangle rect) { return (Vector2){rect.x+rect.width*.5f,rect.y+rect.height*.5f}; }
static void queue(Vector2 from,Vector2 to,bool right) { ui_note_pointer_gesture((DxPointerGesture){from.x,from.y,to.x,to.y,0,right}); }
static void assert_guide(const UIGuide *guide,const UIGuide *before) {
    CHECK(!memcmp(guide,before,sizeof(*guide)));
    CHECK(guide->intro.offsets==before->intro.offsets && guide->note.offsets==before->note.offsets);
}
static void test_backdrop(float scale,int width,int height) {
    UIContext ui; UIGuide guide; setup(&ui,&guide,scale,width,height); Rectangle panel=guide.layout.panel;
    Vector2 points[]={{width*.5f,panel.y*.5f},{width*.5f,(panel.y+panel.height+height)*.5f},
        {panel.x*.5f,panel.y+panel.height*.5f},{(panel.x+panel.width+width)*.5f,panel.y+panel.height*.5f}};
    UIGuide before=guide;
    for (size_t i=0;i<sizeof(points)/sizeof(*points);i++) {
        reset(&ui); queue(points[i],points[i],false);
        CHECK(ui_guide_draw(&ui,&guide,true)); CHECK(!ui_has_pointer_gesture() && !state.paints && ui.cursor==MOUSE_CURSOR_CROSSHAIR); assert_guide(&guide,&before);
        reset(&ui); state.mouse=points[i]; state.pressed=true;
        CHECK(ui_guide_draw(&ui,&guide,true)); CHECK(!state.paints && ui.cursor==MOUSE_CURSOR_CROSSHAIR); assert_guide(&guide,&before);
        reset(&ui); queue(points[i],center(panel),false);
        CHECK(ui_guide_draw(&ui,&guide,true) && !ui_has_pointer_gesture()); assert_guide(&guide,&before);
    }
    reset(&ui); Vector2 offset={19*scale,11*scale}; ui_pointer_offset(offset);
    Vector2 point={points[0].x+offset.x,points[0].y+offset.y}; queue(point,point,false);
    CHECK(ui_guide_draw(&ui,&guide,true) && !ui_has_pointer_gesture()); assert_guide(&guide,&before);
    reset(&ui); ui_pointer_offset(offset); state.mouse=(Vector2){points[2].x+offset.x,points[2].y+offset.y}; state.pressed=true;
    CHECK(ui_guide_draw(&ui,&guide,true)); assert_guide(&guide,&before);
    ui_guide_free(&guide); CHECK(!guide.intro.offsets && !guide.note.offsets);
}
static void test_ownership(float scale,int width,int height) {
    UIContext ui; UIGuide guide; setup(&ui,&guide,scale,width,height); Rectangle panel=guide.layout.panel;
    Vector2 outside={panel.x*.5f,panel.y+panel.height*.5f}; UIGuide before=guide;
    for (int guard=0;guard<3;guard++) {
        reset(&ui); queue(outside,outside,false); ui.pointer_blocked=guard==0;
        if (guard==2) { ui.clip=panel; ui.clipping=true; }
        CHECK(!ui_guide_draw(&ui,&guide,guard!=1)); CHECK(ui_has_pointer_gesture() && ui.cursor==MOUSE_CURSOR_CROSSHAIR); assert_guide(&guide,&before);
    }
    reset(&ui); state.mouse=outside; state.pressed=true; ui.pointer_blocked=true;
    CHECK(!ui_guide_draw(&ui,&guide,true)); CHECK(ui.cursor==MOUSE_CURSOR_CROSSHAIR); assert_guide(&guide,&before);
    reset(&ui); queue(outside,outside,true); CHECK(!ui_guide_draw(&ui,&guide,true));
    DxPointerGesture retained; CHECK(ui_take_pointer_gesture(&retained) && retained.right); assert_guide(&guide,&before);
    reset(&ui); Vector2 body=center(guide.layout.intro); queue(body,body,false);
    CHECK(!ui_guide_draw(&ui,&guide,true) && ui_has_pointer_gesture()); assert_guide(&guide,&before);
#ifdef DUXCEL_DEV_REVIEW
    reset(&ui); queue(outside,outside,false); state.quarantined=true;
    CHECK(!ui_guide_draw(&ui,&guide,true)); state.quarantined=false; CHECK(ui_has_pointer_gesture()); assert_guide(&guide,&before);
#endif
    ui_guide_free(&guide);
}
static void test_existing_controls(float scale,int width,int height) {
    UIContext ui; UIGuide guide; setup(&ui,&guide,scale,width,height); UIGuide before=guide;
    Vector2 close=center(guide.layout.close); queue(close,close,false);
    CHECK(ui_guide_draw(&ui,&guide,true) && !ui_has_pointer_gesture()); assert_guide(&guide,&before);
    reset(&ui); state.key=KEY_END; CHECK(!ui_guide_draw(&ui,&guide,true));
    CHECK(guide.scroll==fmaxf(0,guide.layout.content_height-guide.layout.panel.height/scale));
    if (height<=640) CHECK(guide.scroll>0);
    reset(&ui); guide.focus=1; before=guide; Rectangle start=guide.layout.start; start.y-=guide.scroll*scale; Vector2 target=center(start);
    CHECK(CheckCollisionPointRec(target,guide.layout.clip)); queue(target,target,false);
    CHECK(ui_guide_draw(&ui,&guide,true) && !ui_has_pointer_gesture()); assert_guide(&guide,&before);
    reset(&ui); state.key=KEY_ESCAPE; CHECK(ui_guide_draw(&ui,&guide,true)); assert_guide(&guide,&before);
    reset(&ui); state.key=KEY_ENTER; CHECK(ui_guide_draw(&ui,&guide,true)); assert_guide(&guide,&before);
    ui_guide_free(&guide);
}
int main(void) {
    const struct { float scale; int width,height; } cases[]={{1,1280,820},{1.5f,640,640},{1.5f,640,480}};
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        test_backdrop(cases[i].scale,cases[i].width,cases[i].height);
        test_ownership(cases[i].scale,cases[i].width,cases[i].height);
        test_existing_controls(cases[i].scale,cases[i].width,cases[i].height);
    }
    printf("PASS: %u Guide input checks (actual Guide/shared control/layout modules; completed backdrop presses, translated pointers, blocked/clipped/right ownership and existing Close/Start/Escape/Enter; no GPU/OS)\n",checks);
    return 0;
}
