#import <AppKit/AppKit.h>
#include <raylib.h>
#include <rlgl.h>
#include "grid/grid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Test the actual private recorder without posting OS events or touching any
   other application's windows. Include its implementation in this one test
   translation unit; do not also compile macos.m as a separate source.

   Compile only (no window is created by compilation):
   clang -Isrc -I/opt/homebrew/include $(pkg-config --cflags raylib) \
     -std=c99 -Wall -Wextra -Wpedantic \
     -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
     src/common.c src/workbook/format.c src/transforms/number.c \
     src/ui/motion.c src/ui/fontmetrics.c src/ui/paintlayer.c \
     src/ui/paintlayer_backend.c src/ui/ui.c src/grid/grid.c \
     tests/test_pan_native.m -o build/test-pan-native \
     $(pkg-config --libs raylib) -framework AppKit \
     -framework UniformTypeIdentifiers -fsanitize=address,undefined

   Desktop owner execution, from the repo root, only under the shared lock:
   python3 - <<'PY'
   import os, subprocess, sys
   sys.path.insert(0, 'tools')
   from test import FileLock, native_lock_path
   with FileLock(native_lock_path(), 'Native precise-pan regression'):
       subprocess.run(['build/test-pan-native'], check=True,
                      env={**os.environ, 'ASAN_OPTIONS': 'halt_on_error=1'})
   PY

   The owned raylib/NSWindow remains hidden and unfocused throughout. The
   integrated return uses normal120Hz raylib pacing and current GetTime deltas.
   No OS input posting, explicit sleeps, window activation/order/move, preferences writes,
   physical gesture, or cross-Space behavior is performed or claimed. */
#include "../src/platform/macos.m"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface DxPanTestEvent : NSEvent {
@public
    NSWindow *test_window;
    NSPoint test_location;
    CGFloat test_x,test_y;
    NSEventPhase test_phase,test_momentum;
    NSEventModifierFlags test_flags;
    BOOL test_precise;
}
@end
@implementation DxPanTestEvent
- (NSEventType)type { return NSEventTypeScrollWheel; }
- (NSWindow *)window { return test_window; }
- (NSPoint)locationInWindow { return test_location; }
- (CGFloat)scrollingDeltaX { return test_x; }
- (CGFloat)scrollingDeltaY { return test_y; }
- (NSEventPhase)phase { return test_phase; }
- (NSEventPhase)momentumPhase { return test_momentum; }
- (NSEventModifierFlags)modifierFlags { return test_flags; }
- (BOOL)hasPreciseScrollingDeltas { return test_precise; }
@end

static unsigned checks;
static bool attached;
static NSWindow *other_window;
static UIContext rendered_ui;
static Grid rendered_grid;
static bool ui_owned,grid_owned;
static void cleanup(void) {
    if (grid_owned) { grid_destroy(&rendered_grid); grid_owned=false; }
    if (ui_owned) { ui_destroy(&rendered_ui); ui_owned=false; }
    if (attached) { dx_native_shutdown(); attached=false; }
    if (other_window) { [other_window close]; other_window=nil; }
    if (IsWindowReady()) CloseWindow();
}
#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr,"FAIL: native pan at %d: %s\n",__LINE__,#condition); \
        cleanup(); exit(1); \
    } \
} while (0)
static void close_float(float actual,float expected) {
    CHECK(isfinite(actual) && fabsf(actual-expected)<.0001f);
}
static void hidden(NSWindow *window) {
    CHECK(IsWindowHidden() && !IsWindowFocused());
    CHECK(!window.isVisible && !window.isKeyWindow && !window.isMainWindow);
    CHECK(!NSApp.isActive);
    if (other_window) CHECK(!other_window.isVisible && !other_window.isKeyWindow);
}
static DxPanTestEvent *packet(NSWindow *window,float x,float y,NSEventPhase phase,NSEventPhase momentum,NSEventModifierFlags flags,bool precise,float pointer_x,float pointer_y) {
    DxPanTestEvent *event=[[[DxPanTestEvent alloc] init] autorelease];
    event->test_window=window; event->test_x=x; event->test_y=y;
    event->test_phase=phase; event->test_momentum=momentum;
    event->test_flags=flags; event->test_precise=precise;
    NSRect bounds=canvas_view.bounds;
    NSPoint point=NSMakePoint(NSMinX(bounds)+pointer_x,canvas_view.isFlipped ? NSMinY(bounds)+pointer_y : NSMaxY(bounds)-pointer_y);
    event->test_location=[canvas_view convertPoint:point toView:nil];
    return event;
}
static DxPanInput snapshot(DxPanPhase phase,float x,float y,float pointer_x,float pointer_y,bool event) {
    DxPanInput first,second;
    CHECK(dx_input_pixel_pan(&first)); CHECK(dx_input_pixel_pan(NULL)); CHECK(dx_input_pixel_pan(&second));
    CHECK(first.phase==phase && first.event==event);
    close_float(first.x,x); close_float(first.y,y);
    close_float(first.pointer_x,pointer_x); close_float(first.pointer_y,pointer_y);
    CHECK(second.phase==first.phase && second.event==first.event);
    close_float(second.x,first.x); close_float(second.y,first.y);
    close_float(second.pointer_x,first.pointer_x); close_float(second.pointer_y,first.pointer_y);
    hidden(canvas_window); return first;
}
static void idle(void) {
    DxPanInput input={.x=99,.y=99,.phase=DX_PAN_ACTIVE,.event=true};
    dx_input_begin_frame(); CHECK(!dx_input_pixel_pan(&input)); CHECK(!dx_input_pixel_pan(NULL));
    CHECK(input.phase==DX_PAN_NONE && !input.event); close_float(input.x,0); close_float(input.y,0);
}
static void end_direct(NSWindow *window,float pointer_x,float pointer_y) {
    native_record_pan(packet(window,0,0,NSEventPhaseEnded,NSEventPhaseNone,0,true,pointer_x,pointer_y));
    dx_input_begin_frame(); snapshot(DX_PAN_RELEASE,0,0,pointer_x,pointer_y,true); idle();
}
static void image_pixel(Image image,float x,float y,Color expected) {
    CHECK(IsImageValid(image));
    int pixel_x=(int)floorf(x*image.width/GetScreenWidth()),pixel_y=(int)floorf(y*image.height/GetScreenHeight());
    CHECK(pixel_x>=0 && pixel_x<image.width && pixel_y>=0 && pixel_y<image.height);
    Color actual=GetImageColor(image,pixel_x,pixel_y);
    CHECK(abs((int)actual.r-expected.r)<=2 && abs((int)actual.g-expected.g)<=2 && abs((int)actual.b-expected.b)<=2);
}
static void rendered_frame(NSWindow *window,const Page *page,Rectangle bounds,double *last_tick,bool check_pixels) {
    UIContext *ui=&rendered_ui; Grid *grid=&rendered_grid;
    /* Match App's current-frame monotonic input seam. GetFrameTime reports a
       preceding frame duration and must not drive this acceptance check. */
    double now=GetTime(); ui->frame_time=(float)fmax(0,fmin(1,now-*last_tick)); *last_tick=now;
    ui->paint_time=now;
    dx_input_begin_frame(); ui->native_pan_valid=dx_input_pixel_pan(&ui->native_pan); ui_update(ui);
    hidden(window); BeginDrawing(); ClearBackground(WHITE);
    grid_update(grid,ui,page,bounds,true); grid_draw(grid,ui,page,false);
    rlDrawRenderBatchActive();
    if (check_pixels) {
        Image image=LoadImageFromScreen();
        CHECK(image.width==GetRenderWidth() && image.height==GetRenderHeight());
        Vector2 origin=grid_origin(grid,ui->scale);
        image_pixel(image,grid->body.x+8,grid->body.y+8,(Color){201,211,220,255});
        image_pixel(image,origin.x+.25f,origin.y+80,(Color){143,162,179,255});
        image_pixel(image,origin.x+200,origin.y+.25f,(Color){143,162,179,255});
        image_pixel(image,origin.x+120,origin.y+40,WHITE);
        UnloadImage(image);
    }
    EndDrawing(); hidden(window);
}
static void rendered_pan(NSWindow *window) {
    CHECK(ui_init(&rendered_ui,"assets/fonts/IBMPlexSans-Regular.otf","assets/fonts/IBMPlexSans-Medium.otf","assets/fonts/IBMPlexSans-SemiBold.otf","assets/fonts/JetBrainsMono-Regular.ttf"));
    ui_owned=true; CHECK(rendered_ui.fonts_loaded && rendered_ui.body.texture.id && rendered_ui.mono.texture.id);
    CHECK(ui_measure(&rendered_ui,"A1 NULL",13)>20);
    grid_init(&rendered_grid); grid_owned=true;
    Page page={0}; grid_schema(&rendered_grid,&page,true); rendered_grid.row_height=24;
    Rectangle bounds={0,0,GetScreenWidth(),GetScreenHeight()};
    grid_layout(&rendered_grid,1,&page,bounds); grid_select_cell(&rendered_grid,0,0,false);
    double last_tick=GetTime(); SetTargetFPS(120);
    for (int frame=0;frame<4;frame++) rendered_frame(window,&page,bounds,&last_tick,false);
    float pointer_x=rendered_grid.body.x+200,pointer_y=rendered_grid.body.y+120;
    native_record_pan(packet(window,160,160,NSEventPhaseBegan,NSEventPhaseNone,0,true,pointer_x,pointer_y));
    rendered_frame(window,&page,bounds,&last_tick,true);
    CHECK(rendered_ui.native_pan_valid && rendered_ui.native_pan.phase==DX_PAN_BEGIN);
    close_float(rendered_grid.elastic.x,32); close_float(rendered_grid.elastic.y,32);
    CHECK(!rendered_grid.top && !rendered_grid.fraction && !rendered_grid.horizontal);
    CHECK(!rendered_grid.row && !rendered_grid.col && rendered_grid.selection==SELECT_CELLS);
    double released=GetTime();
    native_record_pan(packet(window,0,0,NSEventPhaseEnded,NSEventPhaseNone,0,true,pointer_x,pointer_y));
    rendered_frame(window,&page,bounds,&last_tick,false);
    CHECK(rendered_ui.native_pan.phase==DX_PAN_RELEASE);
    float previous_x=rendered_grid.elastic.x,previous_y=rendered_grid.elastic.y;
    double settled=-1; unsigned frames=0,tiny=0;
    while (GetTime()-released<.38 && frames<120) {
        float x,y;
        if (frames%13==0) { x=48; y=64; }
        else { x=tiny++%2 ? .6f : -.6f; y=-x; }
        native_record_pan(packet(window,x,y,NSEventPhaseNone,frames ? NSEventPhaseChanged : NSEventPhaseBegan,0,true,pointer_x,pointer_y));
        if (frames%13==0) for (int packet_index=0;packet_index<3;packet_index++) native_record_pan(packet(window,24,32,NSEventPhaseNone,NSEventPhaseChanged,0,true,pointer_x,pointer_y));
        rendered_frame(window,&page,bounds,&last_tick,false); frames++;
        CHECK(rendered_ui.native_pan_valid && rendered_ui.native_pan.phase==DX_PAN_MOMENTUM);
        CHECK(rendered_grid.elastic.x>=0 && rendered_grid.elastic.y>=0);
        CHECK(rendered_grid.elastic.x<=previous_x+.0001f && rendered_grid.elastic.y<=previous_y+.0001f);
        CHECK(!rendered_grid.top && !rendered_grid.fraction && !rendered_grid.horizontal);
        CHECK(!rendered_grid.row && !rendered_grid.col && !rendered_grid.anchor_row && !rendered_grid.anchor_col && !rendered_grid.end_row && !rendered_grid.end_col && rendered_grid.selection==SELECT_CELLS);
        if (settled<0 && rendered_grid.elastic.x<.25f && rendered_grid.elastic.y<.25f) settled=GetTime()-released;
        previous_x=rendered_grid.elastic.x; previous_y=rendered_grid.elastic.y;
    }
    double elapsed=GetTime()-released;
    CHECK(frames>=8 && elapsed<1 && settled>=0 && settled<.25);
    CHECK(!rendered_grid.elastic.x && !rendered_grid.elastic.y && !rendered_grid.spring_velocity.x && !rendered_grid.spring_velocity.y);
    native_record_pan(packet(window,0,0,NSEventPhaseNone,NSEventPhaseEnded,0,true,pointer_x,pointer_y));
    rendered_frame(window,&page,bounds,&last_tick,false); idle();
    printf("Native rendered pan: %u actual frames,%.3fs,below0.25pxat%.3fs,32pointcue,A1fixed,realfonts/backing/edges\n",frames,elapsed,settled);
    grid_destroy(&rendered_grid); grid_owned=false; ui_destroy(&rendered_ui); ui_owned=false; SetTargetFPS(0);
}
int main(void) {
    @autoreleasepool {
        SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
        InitWindow(640,360,"Duxcel — hidden precise-pan regression");
        CHECK(IsWindowReady()); SetExitKey(KEY_NULL);
        NSWindow *window=(NSWindow *)GetWindowHandle(); CHECK(window!=nil); hidden(window);
        dx_native_setup(GetWindowHandle()); attached=true; CHECK(canvas_window==window && canvas_view.window==window); hidden(window);
        /* Released Option followed by H inside one poll retains its opener;
           no native event is posted, and release must not toggle tips twice. */
        record_key_modifiers([NSEvent keyEventWithType:NSEventTypeFlagsChanged location:NSZeroPoint modifierFlags:NSEventModifierFlagOption|NX_DEVICELALTKEYMASK timestamp:0 windowNumber:window.windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:58]);
        record_key_modifiers([NSEvent keyEventWithType:NSEventTypeFlagsChanged location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:58]);
        record_key_modifiers([NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:window.windowNumber context:nil characters:@"h" charactersIgnoringModifiers:@"h" isARepeat:NO keyCode:4]);
        DxKeyModifiers key;
        CHECK(dx_take_key_modifiers(&key) && key.key==KEY_LEFT_ALT && key.modifiers==DX_MOD_ALT && !key.repeat);
        CHECK(dx_take_key_modifiers(&key) && key.key==KEY_H && !key.modifiers && !key.repeat);
        CHECK(!dx_take_key_modifiers(&key));
        CHECK(![(id<NSApplicationDelegate>)NSApp.delegate applicationShouldTerminateAfterLastWindowClosed:NSApp]);
        CHECK(![(id<NSApplicationDelegate>)NSApp.delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:NO]);
        CHECK(dx_take_command()==DX_COMMAND_NEW_WORKBOOK && dx_take_command()==DX_COMMAND_NONE); hidden(window);
        CHECK([(id<NSApplicationDelegate>)NSApp.delegate applicationShouldTerminate:NSApp]==NSTerminateCancel);
        CHECK(dx_native_quit_requested()); dx_native_acknowledge_quit(); CHECK(!dx_native_quit_requested()); hidden(window);
        NSMenuItem *close=nil;
        for (NSMenuItem *root in NSApp.mainMenu.itemArray) for (NSMenuItem *item in root.submenu.itemArray)
            if (item.target==actions && item.tag==DX_COMMAND_CLOSE) close=item;
        CHECK(close && [close.keyEquivalent isEqualToString:@"w"] && close.keyEquivalentModifierMask==NSEventModifierFlagCommand);
        [actions invoke:close]; CHECK(dx_take_command()==DX_COMMAND_CLOSE && !dx_native_quit_requested()); hidden(window);
        other_window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,20,20) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES];
        other_window.releasedWhenClosed=YES; hidden(window); idle();

        /* Exact Cocoa logical points, including simultaneous fractional axes;
           no GLFW detent multiplier or Retina backing factor belongs here. */
        native_record_pan(packet(window,.125f,-.375f,NSEventPhaseBegan,NSEventPhaseNone,0,true,123.25f,87.5f));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,-.125f,.375f,123.25f,87.5f,true);
        for (int frame=0;frame<3;frame++) { dx_input_begin_frame(); snapshot(DX_PAN_ACTIVE,0,0,123.25f,87.5f,false); }
        native_record_pan(packet(window,-1.25f,2.5f,NSEventPhaseChanged,NSEventPhaseNone,0,true,211.5f,96.75f));
        native_record_pan(packet(window,.75f,-1.f,NSEventPhaseChanged,NSEventPhaseNone,0,true,212.25f,97.25f));
        dx_input_begin_frame(); snapshot(DX_PAN_ACTIVE,.5f,-1.5f,212.25f,97.25f,true);
        end_direct(window,212.25f,97.25f);

        /* A whole begin/change/end can arrive in one poll. Preserve the pull
           once, then release next frame instead of silently losing either. */
        native_record_pan(packet(window,-8,-12,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90));
        native_record_pan(packet(window,-4,-2,NSEventPhaseChanged,NSEventPhaseNone,0,true,120,90));
        native_record_pan(packet(window,0,0,NSEventPhaseEnded,NSEventPhaseNone,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,12,14,120,90,true);
        dx_input_begin_frame(); snapshot(DX_PAN_RELEASE,0,0,120,90,false); idle();

        native_record_pan(packet(window,-6,-9,NSEventPhaseNone,NSEventPhaseBegan,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_MOMENTUM,6,9,120,90,true);
        dx_input_begin_frame(); snapshot(DX_PAN_MOMENTUM,0,0,120,90,false);
        native_record_pan(packet(window,-.25f,.5f,NSEventPhaseNone,NSEventPhaseChanged,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_MOMENTUM,.25f,-.5f,120,90,true);
        native_record_pan(packet(window,0,0,NSEventPhaseNone,NSEventPhaseEnded,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_RELEASE,0,0,120,90,true); idle();

        /* Shift remaps a sole vertical axis; a true two-axis gesture retains
           both components and does not apply Shift a second time. */
        native_record_pan(packet(window,0,2.75f,NSEventPhaseBegan,NSEventPhaseNone,NSEventModifierFlagShift,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,-2.75f,0,120,90,true); end_direct(window,120,90);
        native_record_pan(packet(window,.125f,.25f,NSEventPhaseBegan,NSEventPhaseNone,NSEventModifierFlagShift,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,-.125f,-.25f,120,90,true); end_direct(window,120,90);

        /* Other windows, file panels, legacy detent wheels and invalid input
           cannot start/modify this precise-pan channel. No event is posted. */
        native_record_pan(packet(other_window,-50,-50,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90)); idle();
        native_record_pan(packet(window,-3,-3,NSEventPhaseNone,NSEventPhaseNone,0,false,120,90)); idle();
        native_record_pan(packet(window,NAN,1,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90)); idle();
        native_panel_active=true;
        native_record_pan(packet(window,-20,-20,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90));
        native_panel_active=false; idle();
        native_record_pan(packet(window,-.5f,-.25f,NSEventPhaseBegan,NSEventPhaseNone,0,false,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,.5f,.25f,120,90,true);
        pan_seen-=1; /* Simulate a lost End without sleeping or OS interaction. */
        dx_input_begin_frame(); snapshot(DX_PAN_RELEASE,0,0,120,90,false); idle();

        rendered_pan(window);
        native_record_pan(packet(window,-4,-6,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,4,6,120,90,true);
        native_record_pan(packet(window,-40,-60,NSEventPhaseChanged,NSEventPhaseNone,0,true,120,90));
        dx_native_shutdown(); attached=false; hidden(window);
        DxPanInput cleared; CHECK(!dx_input_pixel_pan(&cleared));
        CHECK(cleared.phase==DX_PAN_NONE && !cleared.event);
        close_float(cleared.x,0); close_float(cleared.y,0); close_float(cleared.pointer_x,0); close_float(cleared.pointer_y,0);
        idle(); CHECK(!canvas_view && !canvas_window); hidden(window);
        cleanup(); CHECK(!NSApp.isActive);
        printf("PASS: %u native precise-pan checks (real recorder,logical deltas,phase/coalescing/snapshot/ownership/shutdown,renderedinput/font/GPU/A1return;hidden,noOSinput)\n",checks);
        return 0;
    }
}
#pragma clang diagnostic pop
