#import <AppKit/AppKit.h>
#include "platform/platform.h"
#include "platform/macos_about.h"
#include "app/build_info.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL About:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        setenv("DUXCEL_TEST_VISIBLE","0",1);
        NSImage *original=[[NSImage alloc] initWithContentsOfFile:@"assets/icons/duxcel.icns"];
        CHECK(original!=nil);
        NSApp.applicationIconImage=original;
        NSImage *baseline=[NSApp.applicationIconImage retain];
        for (unsigned cycle=0;cycle<2;cycle++) {
            dx_native_test_identity(true);
            const char *lane=getenv("DUXCEL_LANE");
            if (!lane || !*lane) lane=build_info()->lane;
            NSString *badge=*lane ? [NSString stringWithFormat:@"TEST %s",lane] : @"TEST";
            CHECK([NSApp.dockTile.badgeLabel isEqualToString:badge]);
            CHECK(NSApp.applicationIconImage!=original);
            CHECK(![NSApp.applicationIconImage.TIFFRepresentation isEqualToData:original.TIFFRepresentation]);
            dx_native_about_show(); dx_native_about_show();
            NSPanel *panel=nil;
            for (NSWindow *window in NSApp.windows) if ([window.title isEqualToString:@"About Duxcel [TEST]"]) panel=(NSPanel *)window;
            CHECK(panel && !panel.visible); CHECK(!NSApp.keyWindow);
            NSScrollView *scroll=nil;
            for (NSView *view in panel.contentView.subviews) if ([view isKindOfClass:NSScrollView.class]) scroll=(NSScrollView *)view;
            CHECK(scroll && scroll.hasVerticalScroller);
            NSTextView *text=(NSTextView *)scroll.documentView;
            CHECK([text isKindOfClass:NSTextView.class] && !text.editable && text.selectable);
            CHECK([text.string containsString:[NSString stringWithUTF8String:build_info()->revision]]);
            CHECK([text.string containsString:[NSString stringWithUTF8String:build_info()->subject]]);
            CHECK([text.string containsString:[NSString stringWithUTF8String:build_info()->source_id]]);
            CHECK([text.string containsString:@"Recent changes"]);
            CHECK([text.string containsString:@"Build lane"]);
            if (*build_info()->lane) {
                CHECK([text.string containsString:[NSString stringWithUTF8String:build_info()->lane]]);
                CHECK([text.string containsString:[NSString stringWithUTF8String:build_info()->checkout]]);
            }
            if (build_info()->revision_known) {
                CHECK(text.frame.size.height>scroll.contentSize.height);
                [text scrollRangeToVisible:NSMakeRange(text.string.length,0)];
                CHECK(scroll.contentView.bounds.origin.y>0);
            }
            if (!cycle) {
                [text scrollRangeToVisible:NSMakeRange(0,0)];
                NSString *folder=@"build/tests/results/about";
                CHECK([[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil]);
                NSBitmapImageRep *capture=[panel.contentView bitmapImageRepForCachingDisplayInRect:panel.contentView.bounds];
                CHECK(capture!=nil);
                [panel.contentView cacheDisplayInRect:panel.contentView.bounds toBitmapImageRep:capture];
                CHECK([[capture representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[folder stringByAppendingPathComponent:@"panel.png"] atomically:YES]);
                NSBitmapImageRep *icon=[NSBitmapImageRep imageRepWithData:NSApp.applicationIconImage.TIFFRepresentation];
                CHECK([[icon representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[folder stringByAppendingPathComponent:@"test-icon.png"] atomically:YES]);
                CHECK(!panel.visible && !NSApp.keyWindow);
            }
            dx_native_about_shutdown();
            CHECK(!NSApp.dockTile.badgeLabel); CHECK([NSApp.applicationIconImage.TIFFRepresentation isEqualToData:baseline.TIFFRepresentation]);
        }
        [baseline release]; [original release];
        printf("PASS: native About and test identity (%u checks)\n",checks);
    }
    return 0;
}
