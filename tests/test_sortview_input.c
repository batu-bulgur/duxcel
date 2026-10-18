/* Actual Sort/UI/Analysis modules, with bounded raylib event/draw recorders.
   Layout comes from painted rectangles, not a second wizard implementation.
   No window, GPU, database, clipboard, OS event delivery or prototype claim. */
#include "ui/sortview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Sort input:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct {
    UIContext *ui; Vector2 mouse,wheel; bool pressed,down,quarantined; int key,width,height;
    Rectangle lines[16],tracks[2],selected; size_t line_count,track_count; unsigned wheels;
} state;
static GlyphInfo glyph={.value='?',.advanceX=5}; static Rectangle glyph_rect={0,0,5,10};
typedef struct { Rectangle search,fields,orders,field_track,order_track,level; } Layout;
typedef struct { UIContext ui; SortEditor editor; Column columns[24]; char names[24][16]; Page page; Layout layout; } Fixture;
int GetScreenWidth(void) { return state.width; } int GetScreenHeight(void) { return state.height; }
Vector2 GetMousePosition(void) { return state.mouse; }
Vector2 GetMouseWheelMoveV(void) { state.wheels++; return state.wheel; }
bool IsMouseButtonPressed(int button) { return button==MOUSE_BUTTON_LEFT && state.pressed; }
bool IsMouseButtonDown(int button) { return button==MOUSE_BUTTON_LEFT && state.down; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
bool IsKeyPressed(int key) { return key==state.key && key!=0; }
bool IsKeyPressedRepeat(int key) { (void)key; return false; }
bool IsKeyDown(int key) { (void)key; return false; }
int GetCharPressed(void) { return 0; }
double GetTime(void) { return 1; }
void DrawRectangle(int x,int y,int width,int height,Color color) { (void)x; (void)y; (void)width; (void)height; (void)color; }
void DrawRectangleRec(Rectangle r,Color color) {
    if (!memcmp(&color,&UI_BG,sizeof(color)) && fabsf(r.width-9*state.ui->scale)<.001f && state.track_count<2) state.tracks[state.track_count++]=r;
    if (!memcmp(&color,&UI_TINT,sizeof(color)) && fabsf(r.height-55*state.ui->scale)<.001f) state.selected=r;
}
void DrawRectangleLinesEx(Rectangle r,float width,Color color) {
    (void)width; (void)color; if (state.line_count<sizeof(state.lines)/sizeof(*state.lines)) state.lines[state.line_count++]=r;
}
void DrawRectangleRounded(Rectangle r,float roundness,int segments,Color color) { (void)r; (void)roundness; (void)segments; (void)color; }
void DrawRectangleRoundedLinesEx(Rectangle r,float roundness,int segments,float width,Color color) { (void)r; (void)roundness; (void)segments; (void)width; (void)color; }
void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle target,Vector2 origin,float rotation,Color color) { (void)texture; (void)source; (void)target; (void)origin; (void)rotation; (void)color; }
void DrawLineV(Vector2 from,Vector2 to,Color color) { (void)from; (void)to; (void)color; }
#ifdef DUXCEL_DEV_REVIEW
bool dx_review_input_blocked(void) { return state.quarantined; }
#endif
static void reset(Fixture *f) {
    state.ui=&f->ui; state.mouse=(Vector2){-1000,-1000}; state.wheel=(Vector2){0};
    state.key=0; state.pressed=state.down=state.quarantined=false; state.line_count=state.track_count=state.wheels=0;
    state.selected=(Rectangle){0}; f->ui.cursor=MOUSE_CURSOR_CROSSHAIR; f->ui.pointer_blocked=false; f->ui.clipping=false; ui_pointer_offset((Vector2){0});
    DxPointerGesture gesture; while (ui_take_pointer_gesture(&gesture)) {}
}
static void setup(Fixture *f,float scale,int width) {
    *f=(Fixture){0}; f->ui.scale=scale; f->ui.render_scale=(Vector2){2,2}; f->ui.reduced_motion=true; f->ui.icons.id=2;
    Font font={.baseSize=10,.glyphCount=1,.texture={.id=1},.recs=&glyph_rect,.glyphs=&glyph}; f->ui.body=f->ui.small=f->ui.medium=font;
    for (size_t i=0;i<24;i++) { snprintf(f->names[i],sizeof(f->names[i]),"Field%02zu",i); f->columns[i].name=f->names[i]; }
    f->page=(Page){.columns=f->columns,.column_count=24}; Analysis source={0};
    for (size_t i=0;i<8;i++) CHECK(analysis_sort_set(&source,f->names[i],i%2!=0,true,NULL));
    sort_editor_open(&f->editor,&source,&f->page,0); analysis_free(&source);
    input_set(&f->editor.search,"Field"); f->editor.search.select_all=false; f->editor.search.cursor=2; f->editor.search.anchor=1;
    f->editor.focus=1; f->editor.field_scroll=2; f->editor.field_wheel=.25; f->editor.order_wheel=.125;
    state.width=width; state.height=820; reset(f); CHECK(sort_editor_draw(&f->editor,&f->ui,&f->page)==SORT_EDITING);
    CHECK(state.line_count==4 && state.track_count==2 && state.selected.width>0 && !f->editor.just_opened && f->editor.count==24);
    f->layout=(Layout){state.lines[1],state.lines[2],state.lines[3],state.tracks[0],state.tracks[1],state.selected}; reset(f);
}
static Vector2 center(Rectangle r) { return (Vector2){r.x+r.width*.5f,r.y+r.height*.5f}; }
static void retained(Fixture *f,const SortEditor *before) {
    CHECK(!memcmp(&f->editor,before,sizeof(*before)) && !state.wheels && f->ui.cursor==MOUSE_CURSOR_CROSSHAIR);
    CHECK(!strcmp(f->editor.search.text,"Field") && f->editor.search.cursor==2 && f->editor.search.anchor==1);
    for (size_t i=0;i<8;i++) { AnalysisSort sort=analysis_sort_at(&f->editor.draft,i); CHECK(!strcmp(sort.column,f->names[i]) && sort.descending==(i%2!=0)); }
}
static void test_blocked(float scale,int width,bool quarantine) {
    for (int scenario=0;scenario<7;scenario++) {
        Fixture f; setup(&f,scale,width); SortEditor *e=&f.editor;
        f.ui.pointer_blocked=!quarantine; state.quarantined=quarantine;
        state.mouse=scenario==0 ? center(f.layout.search) : scenario==1 ? (Vector2){f.layout.level.x+4*scale,f.layout.level.y+8*scale} :
            scenario==2 ? center(f.layout.fields) : scenario==3 ? center(f.layout.orders) : center(f.layout.field_track);
        state.pressed=scenario==0 || scenario==1 || scenario==4; state.wheel=(Vector2){0,-1};
        if (scenario>=5) { e->dragging=scenario==5 ? 1 : 2; e->grab=3.25f; state.down=scenario==6; state.mouse.y+=200*scale; }
        SortEditor before=*e; CHECK(sort_editor_draw(e,&f.ui,&f.page)==SORT_EDITING); retained(&f,&before); sort_editor_free(e);
    }
}
static void test_clipped(float scale,int width) {
    for (int scenario=0;scenario<5;scenario++) {
        Fixture f; setup(&f,scale,width); f.ui.clipping=true; f.ui.clip=(Rectangle){0,0,1,1};
        state.mouse=scenario==0 ? center(f.layout.search) : scenario==1 ? (Vector2){f.layout.level.x+4*scale,f.layout.level.y+8*scale} :
            scenario==2 ? center(f.layout.fields) : scenario==3 ? center(f.layout.orders) : center(f.layout.field_track);
        state.pressed=scenario==0 || scenario==1 || scenario==4; state.wheel=(Vector2){0,-1}; SortEditor before=f.editor;
        CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING); retained(&f,&before);
        CHECK(f.ui.clipping && f.ui.clip.x==0 && f.ui.clip.y==0 && f.ui.clip.width==1 && f.ui.clip.height==1); sort_editor_free(&f.editor);
    }
}
static void test_enabled(float scale,int width) {
    Fixture f; setup(&f,scale,width); state.mouse=center(f.layout.fields); state.wheel=(Vector2){0,-1};
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.field_scroll==3 && fabs(f.editor.field_wheel-.25)<.0001);
    reset(&f); state.mouse=center(f.layout.orders); state.wheel=(Vector2){0,-1};
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.order_scroll>0 && f.editor.order_scroll<=3);
    reset(&f); state.mouse=(Vector2){f.layout.field_track.x+f.layout.field_track.width*.5f,f.layout.field_track.y+f.layout.field_track.height-2*scale}; state.pressed=state.down=true;
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.dragging==1 && f.editor.field_scroll>3);
    reset(&f); CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && !f.editor.dragging); sort_editor_free(&f.editor);
    setup(&f,scale,width); Vector2 offset={17*scale,65*scale}; ui_pointer_offset(offset); Vector2 logical=center(f.layout.search);
    state.mouse=(Vector2){logical.x+offset.x,logical.y+offset.y}; state.pressed=true;
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.focus==0 && !strcmp(f.editor.search.text,"Field")); sort_editor_free(&f.editor);
    setup(&f,scale,width); ui_pointer_offset(offset); logical=(Vector2){f.layout.fields.x+f.layout.fields.width*.5f,f.layout.fields.y+f.layout.fields.height-2*scale};
    state.mouse=(Vector2){logical.x+offset.x,logical.y+offset.y}; state.wheel=(Vector2){0,-1};
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.field_scroll==3); sort_editor_free(&f.editor);
    setup(&f,scale,width); ui_pointer_offset(offset); logical=(Vector2){f.layout.level.x+4*scale,f.layout.level.y+8*scale};
    state.mouse=(Vector2){logical.x+offset.x,logical.y+offset.y}; state.pressed=true;
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.focus==2 && f.editor.level==0); sort_editor_free(&f.editor);
}
static void test_keyboard(float scale,int width) {
    Fixture f; setup(&f,scale,width); state.key=KEY_TAB;
    CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.focus==2);
    reset(&f); state.key=KEY_RIGHT; CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && analysis_sort_at(&f.editor.draft,0).descending);
    reset(&f); state.key=KEY_DELETE; CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_EDITING && f.editor.draft.sort_count==6 && !strcmp(f.editor.draft.sort_column,"Field01"));
    reset(&f); state.key=KEY_ENTER; CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_APPLY);
    reset(&f); state.key=KEY_ESCAPE; CHECK(sort_editor_draw(&f.editor,&f.ui,&f.page)==SORT_CANCEL); sort_editor_free(&f.editor);
}
int main(void) {
    const struct { float scale; int width; } cases[]={{1,1280},{1.5f,760}};
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        test_blocked(cases[i].scale,cases[i].width,false); test_clipped(cases[i].scale,cases[i].width); test_enabled(cases[i].scale,cases[i].width); test_keyboard(cases[i].scale,cases[i].width);
#ifdef DUXCEL_DEV_REVIEW
        test_blocked(cases[i].scale,cases[i].width,true);
#endif
    }
    printf("PASS: %u Sort pointer checks (actual wizard/shared UI/Analysis; blocked focus/wheel/thumb, clipped and translated pointer, retained drag and normal keyboard; no GPU/OS/engine)\n",checks); return 0;
}
