#import <AppKit/AppKit.h>
#include "platform/platform.h"
#include "app/build_info.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
/* A consumed, window-addressed key event exercises the actual local monitor
   without focusing or ordering the hidden test window. No global OS input. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool dx_test_session_native_quit(bool keyboard) {
    @autoreleasepool {
        NSWindow *window=[NSOpenGLContext currentContext].view.window;
        if (!window || window.visible || window.keyWindow || !IsWindowHidden() || dx_native_quit_requested()) return false;
        NSMenu *menu=[NSApp.mainMenu itemAtIndex:0].submenu; NSMenuItem *quit=nil;
        unsigned count=0;
        for (NSMenuItem *item in menu.itemArray) if (item.action==@selector(terminate:)) { quit=item; count++; }
        if (count!=1 || ![quit.title isEqualToString:@"Quit Duxcel"] || ![quit.keyEquivalent isEqualToString:@"q"] ||
            quit.target!=NSApp || quit.keyEquivalentModifierMask!=NSEventModifierFlagCommand) return false;
        /* Text input/popups must not disable the application-level Quit. */
        dx_native_context(true,true,false,false);
        bool invoked;
        if (keyboard) {
            NSEvent *event=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
                modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:window.windowNumber
                context:nil characters:@"q" charactersIgnoringModifiers:@"q" isARepeat:NO keyCode:12];
            invoked=[NSApp.mainMenu performKeyEquivalent:event];
        } else invoked=[NSApp sendAction:quit.action to:quit.target from:quit];
        dx_native_context(false,false,false,false);
        return invoked && dx_native_quit_requested() && !window.visible && !window.keyWindow && !IsWindowFocused();
    }
}
bool dx_test_session_native_input(void) {
    @autoreleasepool {
        NSWindow *window=[NSOpenGLContext currentContext].view.window;
        if (!window || window.visible || window.keyWindow || !IsWindowHidden()) return false;
        while (dx_take_command()!=DX_COMMAND_NONE) {}
        /* Deferred cross-feature coverage: real menu -> About adapter -> same
           GL context, followed by normal input/Open/Quit in this session. */
        NSOpenGLContext *context=[NSOpenGLContext currentContext];
        NSMenuItem *about=nil;
        for (NSMenuItem *item in [NSApp.mainMenu itemAtIndex:0].submenu.itemArray)
            if ([item.title isEqualToString:@"About Duxcel"]) about=item;
        if (!about || ![NSApp.dockTile.badgeLabel isEqualToString:@"TEST"] ||
            ![NSApp sendAction:about.action to:about.target from:about]) return false;
        NSPanel *panel=nil;
        for (NSWindow *candidate in NSApp.windows)
            if ([candidate.title isEqualToString:@"About Duxcel [TEST]"]) panel=(NSPanel *)candidate;
        if (!panel || panel.visible || [NSOpenGLContext currentContext]!=context) return false;
        bool metadata=false;
        for (NSView *view in panel.contentView.subviews) if ([view isKindOfClass:NSScrollView.class]) {
            NSTextView *text=(NSTextView *)[(NSScrollView *)view documentView];
            metadata=[text.string containsString:[NSString stringWithUTF8String:build_info()->revision]];
        }
        if (!metadata) return false;
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
        if (!dx_test_session_native_quit(false)) return false;
        dx_native_acknowledge_quit();
        if (!dx_test_session_native_quit(true)) return false;
        dx_native_acknowledge_quit();
        return received && !window.visible && !window.keyWindow && IsWindowHidden() && !IsWindowFocused();
    }
}
#pragma clang diagnostic pop
