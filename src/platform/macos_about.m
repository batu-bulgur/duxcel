#import <AppKit/AppKit.h>
/* raylib owns an OpenGL context; AppKit drawing must restore that context. */
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "platform.h"
#include "macos_about.h"
#include "../app/build_info.h"
#include <stdlib.h>
#include <string.h>

static NSPanel *about_panel;
static NSImage *normal_icon,*test_icon,*installed_icon;
static bool test_instance;

void dx_native_test_identity(bool automated) {
    if (!NSApp) return;
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    test_instance=automated;
    if (automated) {
        if (NSApp.applicationIconImage!=installed_icon) {
            [normal_icon release]; normal_icon=[NSApp.applicationIconImage retain];
        }
        [test_icon release]; test_icon=[[NSImage alloc] initWithSize:NSMakeSize(512,512)];
        [test_icon lockFocus];
        [normal_icon drawInRect:NSMakeRect(0,0,512,512)];
        [[NSColor colorWithCalibratedRed:.47 green:.19 blue:.83 alpha:1] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(40,20,432,144) xRadius:38 yRadius:38] fill];
        NSDictionary *style=@{NSFontAttributeName:[NSFont boldSystemFontOfSize:91],NSForegroundColorAttributeName:NSColor.whiteColor};
        NSString *label=@"TEST"; NSSize size=[label sizeWithAttributes:style];
        [label drawAtPoint:NSMakePoint((512-size.width)/2,32) withAttributes:style];
        [test_icon unlockFocus]; NSApp.applicationIconImage=test_icon;
        [installed_icon release]; installed_icon=[NSApp.applicationIconImage retain];
        NSApp.dockTile.badgeLabel=@"TEST";
    } else {
        if (normal_icon && NSApp.applicationIconImage==installed_icon) NSApp.applicationIconImage=normal_icon;
        NSApp.dockTile.badgeLabel=nil;
    }
    [context makeCurrentContext];
}

void dx_native_about_show(void) {
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    if (!about_panel) {
        const BuildInfo *info=build_info();
        about_panel=[[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,680,500)
            styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        about_panel.title=test_instance ? @"About Duxcel [TEST]" : @"About Duxcel";
        about_panel.releasedWhenClosed=NO; about_panel.minSize=NSMakeSize(480,360);
        NSView *content=about_panel.contentView;
        NSImageView *icon=[[[NSImageView alloc] initWithFrame:NSMakeRect(20,392,64,64)] autorelease];
        icon.image=NSApp.applicationIconImage; icon.autoresizingMask=NSViewMinYMargin;
        [content addSubview:icon];
        NSString *state=info->dirty ? @"Working tree contains uncommitted changes" : info->revision_known ? @"Committed source" : @"Source archive";
        NSString *version=[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
        if (!version) version=[NSString stringWithUTF8String:info->version];
        NSTextField *heading=[NSTextField wrappingLabelWithString:[NSString stringWithFormat:@"Duxcel %@%@\n%@\nBuilt %@",version,test_instance ? @" · TEST" : @"",state,[NSString stringWithUTF8String:info->built_at]]];
        heading.frame=NSMakeRect(100,392,560,82); heading.font=[NSFont systemFontOfSize:13];
        heading.autoresizingMask=NSViewWidthSizable|NSViewMinYMargin; [content addSubview:heading];
        NSScrollView *scroll=[[[NSScrollView alloc] initWithFrame:NSMakeRect(20,20,640,360)] autorelease];
        scroll.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
        scroll.hasVerticalScroller=YES; scroll.borderType=NSBezelBorder;
        NSTextView *text=[[[NSTextView alloc] initWithFrame:NSMakeRect(0,0,620,360)] autorelease];
        text.editable=NO; text.selectable=YES; text.richText=NO;
        text.verticallyResizable=YES; text.horizontallyResizable=NO;
        text.autoresizingMask=NSViewWidthSizable;
        text.textContainer.widthTracksTextView=YES;
        text.textContainerInset=NSMakeSize(12,12); text.font=[NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
        text.string=[NSString stringWithFormat:@"Revision\n%s\n\n%s\n%s · %s\n\nSource fingerprint\n%s\n\nRecent changes (up to 40 commits)\n\n%s",
            info->revision,info->subject,info->author,info->commit_date,info->source_id,info->changelog];
        scroll.documentView=text; [content addSubview:scroll];
        [text.layoutManager ensureLayoutForTextContainer:text.textContainer];
        [text sizeToFit];
        [about_panel center];
    }
    const char *visible=getenv("DUXCEL_TEST_VISIBLE");
    /* QA can inspect/render its own panel without stealing the user's focus. */
    if (!test_instance || (visible && !strcmp(visible,"1"))) [about_panel makeKeyAndOrderFront:nil];
    [context makeCurrentContext];
}

void dx_native_about_shutdown(void) {
    [about_panel orderOut:nil]; [about_panel close]; [about_panel release]; about_panel=nil;
    dx_native_test_identity(false);
    [normal_icon release]; [test_icon release]; [installed_icon release]; normal_icon=test_icon=installed_icon=nil;
}
