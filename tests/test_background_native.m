#import <AppKit/AppKit.h>
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        fprintf(stderr,"FAIL: background native at %d: %s\n",__LINE__,#condition); \
        if (IsWindowReady()) CloseWindow(); exit(1); \
    } \
} while (0)

/* Observe only this process. Another application may become frontmost while
   the test runs, so never require a stable external PID or activate it again. */
static void check_background(NSWindow *window) {
    CHECK(IsWindowHidden());
    CHECK(!IsWindowFocused());
    CHECK(!window.isVisible);
    CHECK(!window.isKeyWindow);
    CHECK(!NSApp.isActive);
    NSRunningApplication *frontmost=NSWorkspace.sharedWorkspace.frontmostApplication;
    CHECK(!frontmost || frontmost.processIdentifier!=getpid());
}

static void check_pixel(Image image,int x,int y,Color expected) {
    float sx=(float)image.width/GetScreenWidth(),sy=(float)image.height/GetScreenHeight();
    Color actual=GetImageColor(image,(int)(x*sx),(int)(y*sy));
    CHECK(abs((int)actual.r-expected.r)<=2);
    CHECK(abs((int)actual.g-expected.g)<=2);
    CHECK(abs((int)actual.b-expected.b)<=2);
}

static void check_framebuffer(void) {
    Image image=LoadImageFromScreen();
    CHECK(IsImageValid(image));
    CHECK(image.width==GetRenderWidth() && image.height==GetRenderHeight());
    check_pixel(image,4,4,WHITE);
    check_pixel(image,32,32,BLUE);
    check_pixel(image,112,72,RED);
    UnloadImage(image);
}

static void paint_frame(void) {
    BeginDrawing(); ClearBackground(WHITE);
    DrawRectangle(16,16,64,48,BLUE);
    DrawRectangle(96,56,48,40,RED);
}

static void check_phase(NSWindow *window,int width,int height) {
    /* Warm both buffers after drawable creation/resize before testing the
       app's post-swap capture path. No animation waits are shortened here. */
    for (int frame=0;frame<8;frame++) {
        check_background(window); paint_frame(); EndDrawing(); check_background(window);
    }
    CHECK(GetScreenWidth()==width && GetScreenHeight()==height);
    CHECK(GetRenderWidth()>=width && GetRenderHeight()>=height);
    for (int frame=0;frame<4;frame++) {
        check_background(window); paint_frame();
        rlDrawRenderBatchActive(); check_framebuffer();
        EndDrawing(); check_background(window);
        check_framebuffer();
    }
    printf("Background framebuffer: logical=%dx%d render=%dx%d hidden=%d focused=%d\n",
        GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight(),IsWindowHidden(),IsWindowFocused());
}

int main(void) {
    @autoreleasepool {
        SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|
            FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
        InitWindow(320,240,"Duxcel — background window regression");
        if (!IsWindowReady()) { fprintf(stderr,"FAIL: background native cannot create a graphics context\n"); return 1; }
        SetExitKey(KEY_NULL); SetTargetFPS(60);
        NSWindow *window=(NSWindow *)GetWindowHandle();
        CHECK(window!=nil); check_background(window);
        check_phase(window,320,240);
        SetWindowSize(480,320); check_phase(window,480,320);
        CloseWindow();
        CHECK(!NSApp.isActive);
        NSRunningApplication *frontmost=NSWorkspace.sharedWorkspace.frontmostApplication;
        CHECK(!frontmost || frontmost.processIdentifier!=getpid());
        printf("PASS: %u background native checks (hidden non-key Cocoa window, inactive app, external frontmost PID, real pixels before/after swap and resize)\n",checks);
        return 0;
    }
}
