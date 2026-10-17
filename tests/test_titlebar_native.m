#import <AppKit/AppKit.h>
#include "platform/platform.h"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Run only under tools/test.py's desktop FileLock. This exercises the actual
   platform and GL surface in an owned hidden window. No OS mouse/key injection,
   window ordering, dragging, fullscreen/Space transition or preferences write.
   Optional argv[1] is an existing private capture directory. The exported PNGs
   contain test paint in the real GL surface; AppKit controls are verified by
   actual native-view geometry, not falsely claimed to be inside the GL image.

   Direct build (root supplies the regular platform header/prototypes):
   clang -Isrc $(pkg-config --cflags raylib) -std=c99 -Wall -Wextra -Wpedantic \
     -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
     src/common.c src/platform/macos.m tests/test_titlebar_native.m \
     -o build/test-titlebar-native $(pkg-config --libs raylib) \
     -framework AppKit -framework UniformTypeIdentifiers \
     -fsanitize=address,undefined
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static unsigned checks;
static bool native_attached;
static const Color title_color={233,238,242,255};
static const char *capture_directory;

static void cleanup(void) {
    if (native_attached) { dx_native_shutdown(); native_attached=false; }
    if (IsWindowReady()) CloseWindow();
}
#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr,"FAIL: native titlebar at %d: %s\n",__LINE__,#condition); \
        cleanup(); exit(1); \
    } \
} while (0)

static void check_background(NSWindow *window) {
    CHECK(IsWindowHidden()); CHECK(!IsWindowFocused());
    CHECK(!window.isVisible); CHECK(!window.isKeyWindow); CHECK(!NSApp.isActive);
    NSRunningApplication *frontmost=NSWorkspace.sharedWorkspace.frontmostApplication;
    /* Another application may legitimately become frontmost during the test.
       Never assert its identity remains fixed or try to reactivate it. */
    CHECK(!frontmost || frontmost.processIdentifier!=getpid());
}
static void check_close(double actual,double expected,double tolerance) {
    CHECK(isfinite(actual) && fabs(actual-expected)<=tolerance);
}
static NSRect canvas_rect(NSView *canvas,NSRect rect,NSView *source) {
    NSRect result=[canvas convertRect:rect fromView:source],bounds=canvas.bounds;
    result.origin.x-=NSMinX(bounds);
    result.origin.y=canvas.isFlipped ? NSMinY(result)-NSMinY(bounds) : NSMaxY(bounds)-NSMaxY(result);
    return result;
}
static void check_geometry(NSWindow *window,float scale,float title_height) {
    NSView *canvas=[NSOpenGLContext currentContext].view;
    CHECK(canvas && canvas.window==window); CHECK(window.contentView==canvas);
    CHECK((window.styleMask&NSWindowStyleMaskTitled)!=0);
    CHECK((window.styleMask&NSWindowStyleMaskResizable)!=0);
    CHECK((window.styleMask&NSWindowStyleMaskFullSizeContentView)!=0);
    CHECK((window.styleMask&NSWindowStyleMaskFullScreen)==0);
    CHECK(window.titlebarAppearsTransparent); CHECK(window.titleVisibility==NSWindowTitleHidden);
    CHECK(!window.movableByWindowBackground);
    NSRect bounds=canvas.bounds,in_window=[canvas convertRect:bounds toView:nil];
    CHECK(bounds.size.width>0 && bounds.size.height>0);
    check_close(NSMaxY(in_window),window.frame.size.height,1);
    check_close(bounds.size.width,GetScreenWidth(),1);
    check_close(bounds.size.height,GetScreenHeight(),1);
    NSRect backing=[canvas convertRectToBacking:bounds];
    check_close(backing.size.width,GetRenderWidth(),1);
    check_close(backing.size.height,GetRenderHeight(),1);
    Vector2 dpi=GetWindowScaleDPI();
    check_close((double)GetRenderWidth()/GetScreenWidth(),dpi.x,.01);
    check_close((double)GetRenderHeight()/GetScreenHeight(),dpi.y,.01);
    const NSWindowButton kinds[]={NSWindowCloseButton,NSWindowMiniaturizeButton,NSWindowZoomButton};
    float right=0,previous_right=-1,center=0;
    for (unsigned i=0;i<sizeof(kinds)/sizeof(*kinds);i++) {
        NSButton *button=[window standardWindowButton:kinds[i]];
        CHECK(button && button.window==window); CHECK(!button.isHiddenOrHasHiddenAncestor);
        NSRect rect=canvas_rect(canvas,button.bounds,button);
        CHECK(isfinite(rect.origin.x) && isfinite(rect.origin.y));
        CHECK(rect.size.width>0 && rect.size.height>0);
        CHECK(rect.origin.x>=0 && rect.origin.y>=-1);
        CHECK(NSMaxX(rect)<=bounds.size.width && NSMaxY(rect)<=bounds.size.height);
        CHECK(rect.origin.x>previous_right); previous_right=(float)NSMaxX(rect);
        if (i) check_close(NSMidY(rect),center,1); else center=(float)NSMidY(rect);
        if (title_height>0) CHECK(NSMaxY(rect)<=title_height+1);
        right=fmaxf(right,(float)NSMaxX(rect));
        printf("Native titlebar control: scale=%.2f kind=%lu x=%.2f y=%.2f width=%.2f height=%.2f center_delta=%.2f\n",
            scale,(unsigned long)kinds[i],rect.origin.x,rect.origin.y,rect.size.width,rect.size.height,
            title_height>0 ? NSMidY(rect)-title_height*.5 : 0);
    }
    float inset=dx_native_titlebar_inset();
    CHECK(isfinite(inset) && inset>right && inset<GetScreenWidth()-100);
    check_close(inset,ceil(right+12),.01);
    printf("Native titlebar surface: scale=%.2f title=%.2f inset=%.2f logical=%dx%d render=%dx%d view_top=%.2f window_top=%.2f\n",
        scale,title_height,inset,GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight(),
        NSMaxY(in_window),window.frame.size.height);
}
static void check_pixel(Image image,float x,float y,Color expected) {
    float sx=(float)image.width/GetScreenWidth(),sy=(float)image.height/GetScreenHeight();
    int px=(int)floorf(x*sx),py=(int)floorf(y*sy);
    CHECK(px>=0 && py>=0 && px<image.width && py<image.height);
    Color actual=GetImageColor(image,px,py);
    CHECK(abs((int)actual.r-expected.r)<=2); CHECK(abs((int)actual.g-expected.g)<=2);
    CHECK(abs((int)actual.b-expected.b)<=2);
}
static void paint(float scale,float height) {
    BeginDrawing(); ClearBackground(WHITE);
    if (height>0) {
        float inset=dx_native_titlebar_inset();
        DrawRectangleRec((Rectangle){0,0,GetScreenWidth(),height},title_color);
        DrawRectangleRec((Rectangle){inset+12*scale,5*scale,54*scale,18*scale},BLUE);
        DrawRectangleRec((Rectangle){inset+82*scale,9*scale,80*scale,10*scale},DARKBLUE);
        DrawRectangleRec((Rectangle){GetScreenWidth()-34*scale,2*scale,24*scale,24*scale},GREEN);
    }
    DrawRectangleRec((Rectangle){96,height+32,48,40},RED);
}
static void check_framebuffer(float scale,float height,const char *phase,bool save) {
    Image image=LoadImageFromScreen();
    CHECK(IsImageValid(image));
    CHECK(image.width==GetRenderWidth() && image.height==GetRenderHeight());
    check_pixel(image,4,4,height>0 ? title_color : WHITE);
    if (height>0) {
        float inset=dx_native_titlebar_inset();
        check_pixel(image,inset+20*scale,14*scale,BLUE);
        check_pixel(image,inset+100*scale,14*scale,DARKBLUE);
        check_pixel(image,GetScreenWidth()-22*scale,14*scale,GREEN);
    }
    check_pixel(image,112,height+48,RED);
    check_pixel(image,GetScreenWidth()-20,height+48,WHITE);
    if (save && capture_directory) {
        char path[4096];
        int length=snprintf(path,sizeof(path),"%s/%s.png",capture_directory,phase);
        CHECK(length>0 && (size_t)length<sizeof(path)); CHECK(ExportImage(image,path));
    }
    UnloadImage(image);
}
static void phase(NSWindow *window,float scale,const char *name) {
    float height=scale>0 ? 28*scale : 0;
    dx_native_titlebar_height(height);
    /* Keep the established double-buffer warmup; this is not an animation wait. */
    for (int frame=0;frame<8;frame++) {
        check_background(window); paint(scale,height); EndDrawing(); check_background(window);
    }
    check_geometry(window,scale,height);
    for (int frame=0;frame<4;frame++) {
        check_background(window); paint(scale,height); rlDrawRenderBatchActive();
        check_framebuffer(scale,height,name,frame==0);
        EndDrawing(); check_background(window); check_framebuffer(scale,height,name,false);
    }
}
int main(int argc,char **argv) {
    @autoreleasepool {
        CHECK(argc<=2); capture_directory=argc==2 ? argv[1] : NULL;
        SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|
            FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
        InitWindow(640,360,"Duxcel — hidden unified titlebar regression");
        if (!IsWindowReady()) { fprintf(stderr,"FAIL: native titlebar cannot create a graphics context\n"); return 1; }
        SetExitKey(KEY_NULL); SetTargetFPS(60);
        NSWindow *window=(NSWindow *)GetWindowHandle();
        CHECK(window!=nil); check_background(window);
        NSRect original_frame=window.frame;
        CHECK(dx_native_titlebar_inset()==0);
        dx_native_setup(GetWindowHandle()); native_attached=true;
        check_background(window);
        check_close(window.frame.origin.x,original_frame.origin.x,.5);
        check_close(window.frame.origin.y,original_frame.origin.y,.5);
        check_close(window.frame.size.width,original_frame.size.width,.5);
        check_close(window.frame.size.height,original_frame.size.height,.5);
        phase(window,1,"native-titlebar-100");
        phase(window,1.5f,"native-titlebar-150");
        SetWindowSize(800,480); phase(window,1.5f,"native-titlebar-resized-150");
        CHECK(GetScreenWidth()==800 && GetScreenHeight()==480);
        phase(window,0,"native-titlebar-focus");
        dx_native_shutdown(); native_attached=false;
        CHECK(dx_native_titlebar_inset()==0); check_background(window);
        CloseWindow(); CHECK(!NSApp.isActive);
        NSRunningApplication *frontmost=NSWorkspace.sharedWorkspace.frontmostApplication;
        CHECK(!frontmost || frontmost.processIdentifier!=getpid());
        printf("PASS: %u native titlebar checks (actual unified content/control geometry,100/150%% andFocus,hidden resize,real before/after-swap pixels,noactivation;nophysical drag/fullscreen claim)\n",checks);
        return 0;
    }
}
#pragma clang diagnostic pop
