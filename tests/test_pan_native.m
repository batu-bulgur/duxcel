#import <AppKit/AppKit.h>
#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Test the actual private recorder without posting OS events or touching any
   other application's windows. Include its implementation in this one test
   translation unit; do not also compile macos.m as a separate source.

   Compile only (no window is created by compilation):
   clang -Isrc $(pkg-config --cflags raylib) -std=c99 -Wall -Wextra -Wpedantic \
     -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
     src/common.c tests/test_pan_native.m -o build/test-pan-native \
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

   The owned raylib/NSWindow remains hidden and unfocused throughout. No OS
   input posting, sleeps, window activation/order/move, preferences writes,
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
static void cleanup(void) {
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
int main(void) {
    @autoreleasepool {
        SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
        InitWindow(640,360,"Duxcel — hidden precise-pan regression");
        CHECK(IsWindowReady()); SetExitKey(KEY_NULL);
        NSWindow *window=(NSWindow *)GetWindowHandle(); CHECK(window!=nil); hidden(window);
        dx_native_setup(GetWindowHandle()); attached=true; CHECK(canvas_window==window && canvas_view.window==window); hidden(window);
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

        native_record_pan(packet(window,-4,-6,NSEventPhaseBegan,NSEventPhaseNone,0,true,120,90));
        dx_input_begin_frame(); snapshot(DX_PAN_BEGIN,4,6,120,90,true);
        native_record_pan(packet(window,-40,-60,NSEventPhaseChanged,NSEventPhaseNone,0,true,120,90));
        dx_native_shutdown(); attached=false; hidden(window);
        DxPanInput cleared; CHECK(!dx_input_pixel_pan(&cleared));
        CHECK(cleared.phase==DX_PAN_NONE && !cleared.event);
        close_float(cleared.x,0); close_float(cleared.y,0); close_float(cleared.pointer_x,0); close_float(cleared.pointer_y,0);
        idle(); CHECK(!canvas_view && !canvas_window); hidden(window);
        cleanup(); CHECK(!NSApp.isActive);
        printf("PASS: %u native precise-pan checks (real recorder,logical deltas,phase/coalescing/snapshot/ownership/shutdown;hidden,noOSinput)\n",checks);
        return 0;
    }
}
#pragma clang diagnostic pop
