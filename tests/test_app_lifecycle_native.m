#import <AppKit/AppKit.h>
#include "platform/platform.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
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
        id<NSApplicationDelegate> delegate=NSApp.delegate;
        if (![delegate respondsToSelector:@selector(application:openFile:)] ||
            ![delegate respondsToSelector:@selector(application:openFiles:)]) return false;
        NSString *first=@"/tmp/Duxcel ünicode document.dxls",*second=@"/tmp/legacy.duxcel";
        if ([delegate application:NSApp openFile:@"relative.dxls"] || [delegate application:NSApp openFile:@"/tmp/source.csv"]) return false;
        if (![delegate application:NSApp openFile:first] || ![delegate application:NSApp openFile:second]) return false;
        char *path=dx_take_open_path(); bool ordered=path && !strcmp(path,first.fileSystemRepresentation); free(path);
        path=dx_take_open_path(); ordered=ordered && path && !strcmp(path,second.fileSystemRepresentation); free(path);
        if (!ordered || dx_take_open_path()) return false;
        [delegate application:NSApp openFiles:@[first,second]];
        path=dx_take_open_path(); ordered=path && !strcmp(path,first.fileSystemRepresentation); free(path);
        path=dx_take_open_path(); ordered=ordered && path && !strcmp(path,second.fileSystemRepresentation); free(path);
        if (!ordered || dx_take_open_path()) return false;
        for (unsigned i=0;i<16;i++) if (![delegate application:NSApp openFile:first]) return false;
        if ([delegate application:NSApp openFile:second]) return false;
        for (unsigned i=0;i<16;i++) { path=dx_take_open_path(); ordered=ordered && path && !strcmp(path,first.fileSystemRepresentation); free(path); }
        if (!ordered || dx_take_open_path()) return false;
        return received && !window.visible && !window.keyWindow && IsWindowHidden() && !IsWindowFocused();
    }
}
#pragma clang diagnostic pop
