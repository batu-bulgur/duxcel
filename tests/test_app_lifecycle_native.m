#import <AppKit/AppKit.h>
#include "platform/platform.h"
#include <raylib.h>
/* A consumed, window-addressed key event exercises the actual local monitor
   without focusing or ordering the hidden test window. No global OS input. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool dx_test_session_native_input(void) {
    @autoreleasepool {
        NSWindow *window=[NSOpenGLContext currentContext].view.window;
        if (!window || window.visible || window.keyWindow || !IsWindowHidden()) return false;
        while (dx_take_command()!=DX_COMMAND_NONE) {}
        NSEvent *event=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
            modifierFlags:NSEventModifierFlagControl timestamp:0 windowNumber:window.windowNumber
            context:nil characters:@"\t" charactersIgnoringModifiers:@"\t" isARepeat:NO keyCode:48];
        [NSApp sendEvent:event];
        bool received=dx_take_command()==DX_COMMAND_TAB_NEXT && dx_take_command()==DX_COMMAND_NONE;
        return received && !window.visible && !window.keyWindow && IsWindowHidden() && !IsWindowFocused();
    }
}
#pragma clang diagnostic pop
