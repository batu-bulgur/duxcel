/* Test-only injection into owned LaunchServices probes, never the product. */
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void reject_alert(NSAlert *alert,const char *entry) {
    const char *message=alert.messageText.UTF8String;
    fprintf(stderr,"FAIL: native alert intercepted at %s: %s\n",entry,message ? message : "");
    fflush(stderr); _exit(73);
}
static NSModalResponse reject_modal(NSAlert *alert,SEL selector) {
    (void)selector; reject_alert(alert,"runModal"); return NSModalResponseAbort;
}
static void reject_sheet(NSAlert *alert,SEL selector,NSWindow *window,void (^completion)(NSModalResponse)) {
    (void)selector; (void)window; (void)completion; reject_alert(alert,"beginSheetModal");
}
__attribute__((constructor)) static void install_alert_guard(void) {
    @autoreleasepool {
        Method modal=class_getInstanceMethod(NSAlert.class,@selector(runModal));
        Method sheet=class_getInstanceMethod(NSAlert.class,@selector(beginSheetModalForWindow:completionHandler:));
        if (!modal || !sheet) { fputs("FAIL: native alert guard cannot install\n",stderr); _exit(74); }
        method_setImplementation(modal,(IMP)reject_modal);
        method_setImplementation(sheet,(IMP)reject_sheet);
        fputs("Native alert guard installed\n",stderr); fflush(stderr);
        const char *probe=getenv("DUXCEL_TEST_ALERT_PROBE");
        if (probe && !strcmp(probe,"1")) {
            NSAlert *alert=[[NSAlert alloc] init]; alert.messageText=@"Owned alert negative control";
            [alert runModal]; _exit(74);
        }
    }
}
