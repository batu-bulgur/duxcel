/* Actual ui_update input forwarding, with bounded raylib input/window stubs.
   No window, GPU, OS input or clipboard. Compile ui.c + fontmetrics.c + common.c
   and this file with -ffunction-sections -Wl,-dead_strip, raylib and libm.
   Browser regression: -DDUXCEL_BROWSER_HOST -U__APPLE__ (Wasm has no Apple
   predefine); native macOS omits both. -U__APPLE__ alone tests Control policy,
   not platform validation. This intentionally tests no browser DOM delivery. */
#include "ui/ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL ui update:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static UIContext ui;
static struct { DxKeyModifiers events[128]; size_t count,next; unsigned held; bool right; int density; } input;
int GetKeyPressed(void) {
    if (input.next==input.count) return 0;
    DxKeyModifiers event=input.events[input.next++]; input.held=event.modifiers; return event.key;
}
bool IsKeyDown(int key) {
    int shift=input.right ? KEY_RIGHT_SHIFT : KEY_LEFT_SHIFT,control=input.right ? KEY_RIGHT_CONTROL : KEY_LEFT_CONTROL;
    int alt=input.right ? KEY_RIGHT_ALT : KEY_LEFT_ALT,super=input.right ? KEY_RIGHT_SUPER : KEY_LEFT_SUPER;
    return (key==shift && (input.held&DX_MOD_SHIFT)) || (key==control && (input.held&DX_MOD_CONTROL)) ||
        (key==alt && (input.held&DX_MOD_ALT)) || (key==super && (input.held&DX_MOD_SUPER));
}
bool IsKeyPressed(int key) { (void)key; return false; }
bool IsMouseButtonPressed(int button) { (void)button; return false; }
int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }
int GetRenderWidth(void) { return 800*input.density; }
int GetRenderHeight(void) { return 600*input.density; }
Vector2 GetWindowScaleDPI(void) { return (Vector2){input.density,input.density}; }
double GetTime(void) { return 0; }
static bool shortcut(unsigned bits) {
#ifdef DUXCEL_BROWSER_HOST
    return (bits&(DX_MOD_CONTROL|DX_MOD_SUPER))!=0;
#elif defined(__APPLE__)
    return (bits&DX_MOD_SUPER)!=0;
#else
    return (bits&DX_MOD_CONTROL)!=0;
#endif
}
static void frame(float scale,int density,bool right) {
    memset(&ui,0,sizeof(ui)); memset(&input,0,sizeof(input)); input.density=density; input.right=right;
    ui.scale=.8f; ui.requested_scale=scale; ui.canvas_font_scale=1;
    ui.raster_size=(int)ceilf(16*scale*density); ui.small_raster=(int)ceilf(11*scale*density); ui.mono_raster=(int)ceilf(10*scale*density);
    ui.reload_after=1000; /* Do not create a font atlas in an input-only test. */
}
static void push(int key,unsigned modifiers) { CHECK(input.count<128); input.events[input.count++]=(DxKeyModifiers){key,modifiers,false}; }
static void update(float scale,int density) {
    ui.pointer_blocked=true; ui_update(&ui);
    CHECK(input.next==input.count && !ui.pointer_blocked && ui.scale==scale);
    CHECK(ui.render_scale.x==density && ui.render_scale.y==density && !ui.fonts_loaded);
}
static void event_matches(DxKeyModifiers actual,int key,unsigned modifiers) {
    if (actual.key!=key || actual.modifiers!=modifiers || actual.repeat)
        fprintf(stderr,"Expected key=%d modifiers=%u repeat=0; received key=%d modifiers=%u repeat=%d\n",key,modifiers,actual.key,actual.modifiers,actual.repeat);
    CHECK(actual.key==key && actual.modifiers==modifiers && !actual.repeat);
}
int main(void) {
    const float scales[]={.8f,1,1.5f}; const int navigation[]={KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_HOME,KEY_END,KEY_PAGE_UP,KEY_PAGE_DOWN,KEY_TAB,KEY_ENTER,KEY_KP_ENTER};
    /* Capture each real modifier independently, never infer Super from the
       shortcut policy: browser Control must remain exactly Control. */
    for (size_t s=0;s<3;s++) for (int density=1;density<=2;density++) for (int right=0;right<2;right++) for (unsigned bits=0;bits<16;bits++) {
        frame(scales[s],density,right!=0);
        for (size_t i=0;i<sizeof(navigation)/sizeof(*navigation);i++) push(navigation[i],bits);
        push(KEY_H,bits); push(KEY_ONE,bits); update(scales[s],density);
        DxKeyModifiers event;
        for (size_t i=0;i<sizeof(navigation)/sizeof(*navigation);i++) { CHECK(ui_take_navigation_key(&event)); event_matches(event,navigation[i],bits); }
        CHECK(!ui_take_navigation_key(&event));
        for (size_t i=0;i<input.count;i++) { CHECK(ui_take_keytip_press(&event)); event_matches(event,input.events[i].key,bits); CHECK(ui_pressed(event.key)); }
        CHECK(!ui_take_keytip_press(&event) && !ui_take_keytip_press(NULL));
        CHECK(ui_key_shift(KEY_DOWN)==!!(bits&DX_MOD_SHIFT) && ui_key_alt(KEY_DOWN)==!!(bits&DX_MOD_ALT));
        CHECK(ui_key_control(KEY_DOWN)==!!(bits&DX_MOD_CONTROL));
        CHECK(ui_key_shortcut(KEY_DOWN)==shortcut(bits) && ui_shortcut()==shortcut(bits));
        CHECK(ui_focus_visible());
    }
    /* The navigation and keytip queues retain their own ordered snapshots,
       even when subsequent queued keys see different physical modifiers. */
    frame(1,2,false); push(KEY_DOWN,DX_MOD_SUPER|DX_MOD_SHIFT); push(KEY_H,DX_MOD_ALT);
    push(KEY_HOME,DX_MOD_CONTROL); push(KEY_ONE,0); update(1,2);
    DxKeyModifiers event;
    CHECK(ui_take_navigation_key(&event)); event_matches(event,KEY_DOWN,DX_MOD_SUPER|DX_MOD_SHIFT);
    CHECK(ui_take_navigation_key(&event)); event_matches(event,KEY_HOME,DX_MOD_CONTROL); CHECK(!ui_take_navigation_key(&event));
    for (size_t i=0;i<input.count;i++) { CHECK(ui_take_keytip_press(&event)); event_matches(event,input.events[i].key,input.events[i].modifiers); }
    CHECK(!ui_take_keytip_press(&event));
    /* A subsequent empty frame clears presses and unconsumed events. Invalid
       raylib queue codes must never escape either bounded consumer. */
    frame(1,1,false); push(KEY_RIGHT,DX_MOD_SUPER); update(1,1);
    frame(1,1,false); push(-1,DX_MOD_SUPER); push(512,DX_MOD_CONTROL); update(1,1);
    CHECK(!ui_pressed(KEY_RIGHT) && !ui_take_navigation_key(&event) && !ui_take_keytip_press(&event));
    /* Native post-poll snapshots still take authority without duplicate
       raylib navigation, and do not turn browser Control into Command. */
    ui_note_key_modifiers(KEY_DOWN,DX_MOD_SUPER|DX_MOD_SHIFT);
    CHECK(ui_take_navigation_key(&event)); event_matches(event,KEY_DOWN,DX_MOD_SUPER|DX_MOD_SHIFT);
    CHECK(ui_key_shift(KEY_DOWN) && !ui_key_control(KEY_DOWN) && ui_key_shortcut(KEY_DOWN)==shortcut(DX_MOD_SUPER));
    frame(1,1,false); push(KEY_DOWN,DX_MOD_CONTROL); update(1,1); CHECK(!ui_take_navigation_key(&event));
    ui_note_key_modifiers(KEY_DOWN,DX_MOD_CONTROL); CHECK(ui_take_navigation_key(&event)); event_matches(event,KEY_DOWN,DX_MOD_CONTROL);
    CHECK(!ui_take_navigation_key(&event) && !ui_key_shift(KEY_DOWN) && ui_key_control(KEY_DOWN) && ui_key_shortcut(KEY_DOWN)==shortcut(DX_MOD_CONTROL));
    /* Keytip native authority and repeat filtering remain unchanged. */
    ui_note_keytip_press(KEY_H,DX_MOD_ALT,true,false); ui_note_keytip_press(KEY_ONE,0,false,false); ui_note_keytip_press(KEY_ONE,0,true,true);
    CHECK(ui_take_keytip_press(&event)); event_matches(event,KEY_H,DX_MOD_ALT); CHECK(!ui_take_keytip_press(&event));
    printf("PASS: %u actual ui_update modifier/navigation/keytip checks (%s; no window, GPU or OS input)\n",checks,
#ifdef DUXCEL_BROWSER_HOST
        "browser Command/Control without Apple predefine"
#elif defined(__APPLE__)
        "native macOS Command"
#else
        "portable Control policy"
#endif
    );
    return 0;
}
