#include "macos_about.h"
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include "platform.h"
#include "../common.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
/* raylib's macOS backend uses NSOpenGLContext. AppKit menu/panel work can change
   the thread's current context; return ownership to the renderer afterwards. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#ifdef DUXCEL_DEV_REVIEW
#define DX_PRESENTATION_KEY @"DuxcelReviewPresentationV1"
#else
#define DX_PRESENTATION_KEY @"DuxcelPresentationV1"
#endif

static float magnification;
static id gesture_monitor;
static id shortcut_monitor;
static id pointer_monitor;
static id scroll_monitor;
static DxPanInput pending_pan,frame_pan;
static DxPanPhase pan_phase;
static bool pan_begin,pan_direct,frame_pan_valid;
static double pan_seen;
static DxPointerGesture pointer_gestures[16],pointer_start[2];
static unsigned pointer_count,pointer_head;
static bool pointer_recording[2];
static DxCommand commands[64];
static unsigned command_head,command_count;
static char *open_paths[16];
static unsigned open_head,open_count;
static id launch_observer;
static bool enqueue_open_paths(NSArray<NSString *> *paths) {
    /* Admit a whole event or none; never silently drop the end of a batch. */
    if (!paths.count || paths.count>16-open_count) return false;
    for (NSString *path in paths) {
        NSString *extension=path.pathExtension.lowercaseString;
        if (!path.isAbsolutePath || (![extension isEqualToString:@"dxls"] && ![extension isEqualToString:@"duxcel"]) ||
            !path.fileSystemRepresentation || strlen(path.fileSystemRepresentation)>32768) return false;
    }
    for (NSString *path in paths) { open_paths[(open_head+open_count)%16]=dx_strdup(path.fileSystemRepresentation); open_count++; }
    return true;
}
char *dx_take_open_path(void) {
    if (!open_count) return NULL;
    char *path=open_paths[open_head]; open_paths[open_head]=NULL; open_head=(open_head+1)%16; open_count--; return path;
}
static DxKeyModifiers modifier_events[128];
static unsigned modifier_head,modifier_count;
static void record_key_modifiers(NSEvent *event) {
    int key=0;
    if (event.type==NSEventTypeFlagsChanged) {
        /* Capture the opener in the same ordered native stream as letters.
           A quick Option-down/up/H can all precede one rendered frame. */
        NSUInteger mask=event.keyCode==58 ? NX_DEVICELALTKEYMASK : event.keyCode==61 ? NX_DEVICERALTKEYMASK : 0;
        if (!mask || !(event.modifierFlags&mask)) return;
        key=event.keyCode==58 ? KEY_LEFT_ALT : KEY_RIGHT_ALT;
    } else {
    switch (event.keyCode) {
    case 126: key=KEY_UP; break; case 125: key=KEY_DOWN; break;
    case 123: key=KEY_LEFT; break; case 124: key=KEY_RIGHT; break;
    case 115: key=KEY_HOME; break; case 119: key=KEY_END; break;
    case 116: key=KEY_PAGE_UP; break; case 121: key=KEY_PAGE_DOWN; break;
    case 48: key=KEY_TAB; break; case 36: key=KEY_ENTER; break; case 76: key=KEY_KP_ENTER; break;
    case 24: key=KEY_EQUAL; break; case 27: key=KEY_MINUS; break; case 44: key=KEY_SLASH; break;
    case 53: key=KEY_ESCAPE; break; case 51: key=KEY_BACKSPACE; break; case 117: key=KEY_DELETE; break;
    default: {
        NSString *text=[event charactersByApplyingModifiers:0]; if (!text.length) text=event.charactersIgnoringModifiers;
        if (text.length) { unichar c=[text characterAtIndex:0]; if (c>='a' && c<='z') c-=32; if ((c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ') key=c; else if (c>=NSF1FunctionKey && c<=NSF12FunctionKey) key=KEY_F1+c-NSF1FunctionKey; }
        break;
    }
    }
    }
    if (!key || modifier_count==128) return;
    NSEventModifierFlags flags=event.modifierFlags;
    unsigned modifiers=(flags&NSEventModifierFlagShift ? DX_MOD_SHIFT : 0) | (flags&NSEventModifierFlagControl ? DX_MOD_CONTROL : 0) | (flags&NSEventModifierFlagOption ? DX_MOD_ALT : 0) | (flags&NSEventModifierFlagCommand ? DX_MOD_SUPER : 0);
    modifier_events[(modifier_head+modifier_count)%128]=(DxKeyModifiers){key,modifiers,event.type==NSEventTypeKeyDown && event.isARepeat}; modifier_count++;
}
bool dx_take_key_modifiers(DxKeyModifiers *event) {
    if (!modifier_count) return false;
    *event=modifier_events[modifier_head]; modifier_head=(modifier_head+1)%128; modifier_count--; return true;
}
bool dx_take_pointer_gesture(DxPointerGesture *gesture) {
    if (!pointer_count) return false;
    *gesture=pointer_gestures[pointer_head]; pointer_head=(pointer_head+1)%16; pointer_count--; return true;
}
static void enqueue_command(DxCommand value) {
    if (command_count<64) { commands[(command_head+command_count)%64]=value; command_count++; }
}
#ifdef DUXCEL_DEV_REVIEW
static bool review_request;
bool dx_review_take_native_request(void) { bool requested=review_request; review_request=false; return requested; }
#endif
static bool native_panel_active;
static NSSavePanel *active_native_panel;
static bool quit_requested;
static NSMenu *native_edit_menu;
static NSMenu *native_app_menu,*previous_app_menu,*previous_services_menu;
static NSMenuItem *native_app_root;
static bool native_app_root_added;
static bool app_modal,text_input,pivot_context,key_tips;
static bool trace_input;
static NSWindow *canvas_window; /* borrowed from GLFW */
static NSView *canvas_view; /* same borrowed renderer view, in window coordinates */
static float native_titlebar_height;
static const NSWindowButton titlebar_button_kinds[]={NSWindowCloseButton,NSWindowMiniaturizeButton,NSWindowZoomButton};
/* The standard controls remain AppKit-owned. The C bar reserves their actual
   canvas-space extent instead of assuming a fixed Retina/OS titlebar width. */
static bool native_fullscreen(void) {
    return canvas_window && (canvas_window.styleMask&NSWindowStyleMaskFullScreen)!=0;
}
static bool native_canvas_point(NSEvent *event,float *x,float *y) {
    if (!canvas_view || canvas_view.window!=canvas_window || event.window!=canvas_window) return false;
    NSPoint point=[canvas_view convertPoint:event.locationInWindow fromView:nil];
    NSRect bounds=canvas_view.bounds;
    *x=(float)(point.x-NSMinX(bounds));
    *y=(float)(canvas_view.isFlipped ? point.y-NSMinY(bounds) : NSMaxY(bounds)-point.y);
    return isfinite(*x) && isfinite(*y);
}
static void native_record_pan(NSEvent *event) {
    if (native_panel_active || event.window!=canvas_window ||
        (!event.hasPreciseScrollingDeltas && !event.phase && !event.momentumPhase)) return;
    float x,y; if (!native_canvas_point(event,&x,&y)) return;
    float dx=-(float)event.scrollingDeltaX,dy=-(float)event.scrollingDeltaY;
    if (!isfinite(dx) || !isfinite(dy)) return;
    if ((event.modifierFlags&NSEventModifierFlagShift) && !dx) { dx=dy; dy=0; }
    pending_pan.x=fmaxf(-1000000,fminf(1000000,pending_pan.x+dx));
    pending_pan.y=fmaxf(-1000000,fminf(1000000,pending_pan.y+dy));
    pending_pan.pointer_x=x; pending_pan.pointer_y=y; pending_pan.event=true;
    pan_seen=[NSProcessInfo processInfo].systemUptime;
    if (event.momentumPhase) pan_phase=(event.momentumPhase&(NSEventPhaseEnded|NSEventPhaseCancelled)) ? DX_PAN_RELEASE : DX_PAN_MOMENTUM;
    else if (event.phase&(NSEventPhaseEnded|NSEventPhaseCancelled)) pan_phase=DX_PAN_RELEASE;
    else if (event.phase&(NSEventPhaseBegan|NSEventPhaseChanged|NSEventPhaseStationary)) {
        pan_phase=DX_PAN_ACTIVE; pan_begin|=(event.phase&NSEventPhaseBegan)!=0;
        pan_direct|=dx!=0 || dy!=0;
    } else pan_phase=DX_PAN_NONE;
}
void dx_input_begin_frame(void) {
    /* A lost End (window/modal switch) must not leave the canvas held forever. */
    if (pan_phase==DX_PAN_ACTIVE && [NSProcessInfo processInfo].systemUptime-pan_seen>.25) pan_phase=DX_PAN_RELEASE;
    frame_pan=pending_pan;
    frame_pan.phase=pan_begin ? DX_PAN_BEGIN : pan_direct ? DX_PAN_ACTIVE : pan_phase;
    frame_pan_valid=frame_pan.event || frame_pan.phase!=DX_PAN_NONE;
    pending_pan.x=pending_pan.y=0; pending_pan.event=false; pan_begin=pan_direct=false;
    if (frame_pan.phase==DX_PAN_RELEASE) pan_phase=DX_PAN_NONE;
}
bool dx_input_pixel_pan(DxPanInput *input) {
    if (input) *input=frame_pan;
    return frame_pan_valid;
}
float dx_native_titlebar_inset(void) {
    if (!canvas_window || canvas_view.window!=canvas_window || native_fullscreen() ||
        !(canvas_window.styleMask&NSWindowStyleMaskTitled)) return 0;
    CGFloat trailing=0;
    for (unsigned i=0;i<sizeof(titlebar_button_kinds)/sizeof(*titlebar_button_kinds);i++) {
        NSButton *button=[canvas_window standardWindowButton:titlebar_button_kinds[i]];
        if (!button || button.isHiddenOrHasHiddenAncestor || button.window!=canvas_window) continue;
        NSRect frame=[canvas_view convertRect:button.bounds fromView:button];
        trailing=fmax(trailing,NSMaxX(frame)-NSMinX(canvas_view.bounds));
    }
    return trailing>0 ? (float)ceil(trailing+12) : 0;
}
void dx_native_titlebar_height(float height) {
    native_titlebar_height=isfinite(height) ? fminf(512,fmaxf(0,height)) : 0;
    /* AppKit owns control geometry and relayout. Repositioning those views here
       is undone during its next layout and would make traffic lights jitter. */
}
static bool native_titlebar_button_hit(float x,float y) {
    NSRect bounds=canvas_view.bounds;
    NSPoint point=NSMakePoint(x+NSMinX(bounds),canvas_view.isFlipped ? y+NSMinY(bounds) : NSMaxY(bounds)-y);
    for (unsigned i=0;i<sizeof(titlebar_button_kinds)/sizeof(*titlebar_button_kinds);i++) {
        NSButton *button=[canvas_window standardWindowButton:titlebar_button_kinds[i]];
        if (button && !button.isHiddenOrHasHiddenAncestor && button.window==canvas_window &&
            NSPointInRect(point,[canvas_view convertRect:button.bounds fromView:button])) return true;
    }
    return false;
}
static bool native_titlebar_drag(NSEvent *event,float x,float y) {
    if (event.type!=NSEventTypeLeftMouseDown || native_titlebar_height<=0 ||
        native_panel_active || app_modal || NSApp.modalWindow || canvas_window.attachedSheet ||
        !canvas_window.isVisible || NSApp.isHidden || canvas_window.isMiniaturized || native_fullscreen()) return false;
    float leading=dx_native_titlebar_inset();
    float trailing=fmaxf(40,native_titlebar_height*1.3f);
    if (x<leading || y<0 || y>=native_titlebar_height ||
        x>=canvas_view.bounds.size.width-trailing) return false;
    /* AppKit drives the tracking loop and screen transitions. A consumed down
       must not create a second C selection/gesture when its up is delivered. */
    pointer_recording[0]=false;
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    if (event.clickCount==2) {
        NSString *action=[NSUserDefaults.standardUserDefaults stringForKey:@"AppleActionOnDoubleClick"];
        if ([action isEqualToString:@"Minimize"]) [canvas_window performMiniaturize:nil];
        else if (![action isEqualToString:@"None"]) [canvas_window performZoom:nil];
        [context makeCurrentContext];
        return true;
    }
    [canvas_window performWindowDragWithEvent:event];
    [context makeCurrentContext];
    return true;
}
/* Preserve GLFW's delegate for display/input lifecycle messages. Termination
   must also unwind AppKit's nested file-panel loop before C cleanup can run. */
@interface DuxcelApplicationDelegate : NSProxy <NSApplicationDelegate> {
    id previous;
}
- (instancetype)initWithDelegate:(id)delegate;
- (id)previousDelegate;
@end
@implementation DuxcelApplicationDelegate
- (instancetype)initWithDelegate:(id)delegate { previous=[delegate retain]; return self; }
- (void)dealloc { [previous release]; [super dealloc]; }
- (id)previousDelegate { return previous; }
- (BOOL)respondsToSelector:(SEL)selector {
    return selector==@selector(application:openFile:) || selector==@selector(application:openFiles:) || selector==@selector(applicationShouldTerminate:) || selector==@selector(applicationShouldHandleReopen:hasVisibleWindows:) || selector==@selector(applicationShouldTerminateAfterLastWindowClosed:) || [previous respondsToSelector:selector];
}
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector { return [previous methodSignatureForSelector:selector]; }
- (void)forwardInvocation:(NSInvocation *)invocation { [invocation invokeWithTarget:previous]; }
- (BOOL)application:(NSApplication *)sender openFile:(NSString *)path { (void)sender; return enqueue_open_paths(@[path]); }
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)paths {
    [sender replyToOpenOrPrint:enqueue_open_paths(paths) ? NSApplicationDelegateReplySuccess : NSApplicationDelegateReplyFailure];
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender; quit_requested=true;
    if (native_panel_active) { [active_native_panel cancel:nil]; [NSApp abortModal]; }
    return NSTerminateCancel;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { (void)sender; return NO; }
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)visible {
    (void)sender; if (!visible) enqueue_command(DX_COMMAND_NEW_WORKBOOK); return NO;
}
@end
static DuxcelApplicationDelegate *application_delegate;
static void install_application_delegate(void) {
    if (NSApp.delegate==(id<NSApplicationDelegate>)application_delegate) return;
    [application_delegate release];
    application_delegate=[[DuxcelApplicationDelegate alloc] initWithDelegate:NSApp.delegate];
    NSApp.delegate=(id<NSApplicationDelegate>)application_delegate;
}
void dx_native_prepare(void) {
    /* GLFW creates its delegate and calls finishLaunching inside InitWindow.
       Cold-launch open-document events precede our window setup. Observe the
       will-finish boundary without creating/replacing GLFW's NSApplication. */
    if (!launch_observer) launch_observer=[[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationWillFinishLaunchingNotification object:nil queue:nil
        usingBlock:^(NSNotification *notification) { (void)notification; install_application_delegate(); }];
}
bool dx_presentation_load(DxPresentation *out) {
    @autoreleasepool {
        id stored=[[NSUserDefaults standardUserDefaults] objectForKey:DX_PRESENTATION_KEY];
        if (![stored isKindOfClass:[NSDictionary class]]) return false;
        NSDictionary *d=stored;
        NSArray *keys=@[@"ribbon",@"previousRibbon",@"workbook",@"fields",@"transformations",@"workbookCollapsed",@"fieldsCollapsed",@"reduceMotion",@"leftWidth",@"rightWidth",@"interfaceScale"];
        for (NSString *key in keys) if (![[d objectForKey:key] isKindOfClass:[NSNumber class]]) return false;
        double ribbon=[d[@"ribbon"] doubleValue],previous=[d[@"previousRibbon"] doubleValue],left=[d[@"leftWidth"] doubleValue],right=[d[@"rightWidth"] doubleValue],scale=[d[@"interfaceScale"] doubleValue];
        id statistics=d[@"statisticsMask"]; double mask=statistics ? [statistics isKindOfClass:[NSNumber class]] ? [statistics doubleValue] : -1 : 49;
        if (!isfinite(mask) || mask<0 || mask>255 || floor(mask)!=mask) return false;
        if (!isfinite(ribbon) || ribbon<0 || ribbon>3 || floor(ribbon)!=ribbon || !isfinite(previous) || previous<0 || previous>2 || floor(previous)!=previous || !isfinite(left) || left<188 || left>360 || !isfinite(right) || right<244 || right>420 || !isfinite(scale) || scale<.8 || scale>1.5) return false;
        *out=(DxPresentation){.ribbon_mode=(int)ribbon,.previous_ribbon_mode=(int)previous,.workbook=[d[@"workbook"] boolValue],.fields=[d[@"fields"] boolValue],.transformations=[d[@"transformations"] boolValue],.workbook_collapsed=[d[@"workbookCollapsed"] boolValue],.fields_collapsed=[d[@"fieldsCollapsed"] boolValue],.reduced_motion=[d[@"reduceMotion"] boolValue],.sidebar_width=(float)left,.pipeline_width=(float)right,.interface_scale=(float)scale};
        out->statistics_mask=(unsigned)mask;
        id collapsed=d[@"transformationsCollapsed"];
        if (collapsed && ![collapsed isKindOfClass:[NSNumber class]]) return false;
        out->transformations_collapsed=collapsed ? [collapsed boolValue] : false;
        id freeze=d[@"freeze"];
        if (freeze && ![freeze isKindOfClass:[NSNumber class]]) return false;
        out->freeze=freeze ? [freeze boolValue] : true;
        return true;
    }
}
void dx_presentation_save(const DxPresentation *p) {
    @autoreleasepool {
        NSDictionary *d=@{@"ribbon":@(p->ribbon_mode),@"previousRibbon":@(p->previous_ribbon_mode),@"workbook":@(p->workbook),@"fields":@(p->fields),@"transformations":@(p->transformations),@"workbookCollapsed":@(p->workbook_collapsed),@"fieldsCollapsed":@(p->fields_collapsed),@"reduceMotion":@(p->reduced_motion),@"leftWidth":@(p->sidebar_width),@"rightWidth":@(p->pipeline_width),@"interfaceScale":@(p->interface_scale),@"statisticsMask":@(p->statistics_mask)};
        NSMutableDictionary *stored=[d mutableCopy]; stored[@"transformationsCollapsed"]=@(p->transformations_collapsed);
        stored[@"freeze"]=@(p->freeze);
        [[NSUserDefaults standardUserDefaults] setObject:stored forKey:DX_PRESENTATION_KEY]; [stored release];
    }
}
void dx_native_context(bool modal,bool editing,bool pivot,bool tips) { app_modal=modal; text_input=editing; pivot_context=pivot; key_tips=tips; }
@interface DuxcelMenuActions : NSObject
- (void)invoke:(id)sender;
- (void)about:(id)sender;
@end
@implementation DuxcelMenuActions
- (void)invoke:(id)sender {
    DxCommand value=(DxCommand)[sender tag];
    if (native_panel_active || (NSApp.keyWindow && NSApp.keyWindow!=canvas_window)) return;
#ifdef DUXCEL_DEV_REVIEW
    if (value==DX_COMMAND_REPORT_UI) { review_request=true; return; }
#endif
    enqueue_command(value);
}
- (void)about:(id)sender {
    (void)sender; if (native_panel_active) return;
    dx_native_about_show();
}
@end
static DuxcelMenuActions *actions;
static void install_application_menu(void) {
    /* GLFW's menu depends on its host/configuration. Build the application
       commands explicitly, including Quit's standard Command-Q equivalent. */
    if (!NSApp.mainMenu) {
        NSMenu *bar=[[NSMenu alloc] initWithTitle:@"Duxcel"];
        NSApp.mainMenu=bar; [bar release];
    }
    native_app_root_added=NSApp.mainMenu.numberOfItems==0;
    if (native_app_root_added) {
        NSMenuItem *root=[[NSMenuItem alloc] initWithTitle:@"Duxcel" action:nil keyEquivalent:@""];
        [NSApp.mainMenu addItem:root]; [root release];
    }
    native_app_root=[NSApp.mainMenu itemAtIndex:0];
    previous_app_menu=[native_app_root.submenu retain];
    previous_services_menu=[NSApp.servicesMenu retain];
    NSMenu *menu=[[NSMenu alloc] initWithTitle:@"Duxcel"];
    native_app_root.title=@"Duxcel"; native_app_root.submenu=menu;
    native_app_menu=menu; [menu release]; /* Retained by the root. */
    NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:@"About Duxcel" action:@selector(about:) keyEquivalent:@""];
    item.target=actions; [menu addItem:item]; [item release];
    [menu addItem:NSMenuItem.separatorItem];
    item=[[NSMenuItem alloc] initWithTitle:@"Settings…" action:@selector(invoke:) keyEquivalent:@","];
    item.target=actions; item.tag=DX_COMMAND_SETTINGS; item.keyEquivalentModifierMask=NSEventModifierFlagCommand;
    [menu addItem:item]; [item release]; [menu addItem:NSMenuItem.separatorItem];
    item=[[NSMenuItem alloc] initWithTitle:@"Services" action:nil keyEquivalent:@""];
    NSMenu *services=[[NSMenu alloc] initWithTitle:@"Services"];
    item.submenu=services; NSApp.servicesMenu=services; [menu addItem:item]; [services release]; [item release];
    [menu addItem:NSMenuItem.separatorItem];
    const SEL selectors[]={@selector(hide:),@selector(hideOtherApplications:),@selector(unhideAllApplications:),@selector(terminate:)};
    NSArray *titles=@[@"Hide Duxcel",@"Hide Others",@"Show All",@"Quit Duxcel"];
    NSArray *keys=@[@"h",@"h",@"",@"q"];
    for (NSUInteger i=0;i<4;i++) {
        if (i==3) [menu addItem:NSMenuItem.separatorItem];
        item=[[NSMenuItem alloc] initWithTitle:titles[i] action:selectors[i] keyEquivalent:keys[i]];
        item.target=NSApp; item.keyEquivalentModifierMask=NSEventModifierFlagCommand | (i==1 ? NSEventModifierFlagOption : 0);
        [menu addItem:item]; [item release];
    }
}
bool dx_native_quit_requested(void) { return quit_requested; }
void dx_native_acknowledge_quit(void) { quit_requested=false; }
void dx_native_window_visible(bool visible) {
    if (!canvas_window) return;
    if (visible) { ClearWindowState(FLAG_WINDOW_HIDDEN); [canvas_window makeKeyAndOrderFront:nil]; }
    else { SetWindowState(FLAG_WINDOW_HIDDEN); [canvas_window orderOut:nil]; }
}
DxCommand dx_take_command(void) {
    if (!command_count) return DX_COMMAND_NONE;
    DxCommand value=commands[command_head]; command_head=(command_head+1)%64; command_count--; return value;
}
void dx_native_setup(void *window) {
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    (void)window;
    trace_input=getenv("DUXCEL_TRACE_INPUT")!=NULL;
    const char *test_role=getenv("DUXCEL_TEST_INSTANCE");
    if (test_role && !strcmp(test_role,"1")) dx_native_test_identity(true);
    NSView *canvas=context.view;
    canvas_window=canvas.window; canvas_view=canvas;
    native_titlebar_height=0;
    if (canvas_window && (canvas_window.styleMask&NSWindowStyleMaskTitled)) {
        /* Full-size content removes the duplicate OS header while retaining
           native traffic lights, resizing, shadow and fullscreen machinery.
           Never order/activate/move this or any other application window. */
        canvas_window.styleMask|=NSWindowStyleMaskFullSizeContentView;
        canvas_window.titlebarAppearsTransparent=YES;
        canvas_window.titleVisibility=NSWindowTitleHidden;
        canvas_window.movableByWindowBackground=NO;
        if (@available(macOS 11.0,*)) canvas_window.titlebarSeparatorStyle=NSTitlebarSeparatorStyleNone;
        [canvas_window.contentView layoutSubtreeIfNeeded];
        [context update];
    }
    if (!pointer_monitor) pointer_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown|NSEventMaskLeftMouseUp|NSEventMaskRightMouseDown|NSEventMaskRightMouseUp handler:^NSEvent *(NSEvent *event) {
        int button=event.type==NSEventTypeRightMouseDown || event.type==NSEventTypeRightMouseUp;
        if (native_panel_active || event.window!=canvas.window) { pointer_recording[button]=false; return event; }
        float x,y;
        if (!native_canvas_point(event,&x,&y)) { pointer_recording[button]=false; return event; }
        /* Buttons live above the GL content in the unified bar, including
           Focus mode with no C bar. Never synthesize worksheet gestures for
           their native responder chain, even when the button is disabled. */
        if (native_titlebar_button_hit(x,y)) {
            pointer_recording[button]=false; return event;
        }
        if (native_titlebar_drag(event,x,y)) return nil;
        if (y>=0 && y<native_titlebar_height && x<dx_native_titlebar_inset()) {
            pointer_recording[button]=false; return event;
        }
        if (event.type==NSEventTypeLeftMouseDown || event.type==NSEventTypeRightMouseDown) {
            NSEventModifierFlags flags=event.modifierFlags;
            unsigned modifiers=(flags&NSEventModifierFlagShift ? DX_MOD_SHIFT : 0) | (flags&NSEventModifierFlagControl ? DX_MOD_CONTROL : 0) | (flags&NSEventModifierFlagOption ? DX_MOD_ALT : 0) | (flags&NSEventModifierFlagCommand ? DX_MOD_SUPER : 0);
            pointer_start[button]=(DxPointerGesture){x,y,x,y,modifiers,button!=0}; pointer_recording[button]=true;
        } else if (pointer_recording[button]) {
            pointer_start[button].x1=x; pointer_start[button].y1=y;
            if (pointer_count<16) { pointer_gestures[(pointer_head+pointer_count)%16]=pointer_start[button]; pointer_count++; }
            pointer_recording[button]=false;
        }
        if (trace_input) fprintf(stderr,"Native pointer type=%ld x=%.1f y=%.1f\n",(long)event.type,x,y);
        return event;
    }];
    if (!gesture_monitor) gesture_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskMagnify handler:^NSEvent *(NSEvent *event) {
        if (event.window==canvas.window) magnification+=(float)event.magnification;
        return event;
    }];
    if (!scroll_monitor) scroll_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel handler:^NSEvent *(NSEvent *event) {
        native_record_pan(event); return event; /* other widgets retain GLFW wheel input */
    }];
    if (!shortcut_monitor) shortcut_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown|NSEventMaskFlagsChanged handler:^NSEvent *(NSEvent *event) {
        if (event.window && event.window!=canvas.window) return event;
        if (event.type==NSEventTypeFlagsChanged) { if (!native_panel_active) record_key_modifiers(event); return event; }
        if (trace_input) fprintf(stderr,"Native key code=%u characters=%s flags=%lu panel=%d modal=%d\n",event.keyCode,[event.charactersIgnoringModifiers UTF8String],(unsigned long)event.modifierFlags,native_panel_active,app_modal);
#ifdef DUXCEL_DEV_REVIEW
        NSEventModifierFlags review_flags=event.modifierFlags;
        if (!native_panel_active && event.keyCode==47 && (review_flags&NSEventModifierFlagShift) &&
            (review_flags&(NSEventModifierFlagCommand|NSEventModifierFlagControl)) && !(review_flags&NSEventModifierFlagOption)) {
            if (!event.isARepeat) review_request=true;
            return nil;
        }
#endif
        if (!native_panel_active) record_key_modifiers(event);
        /* AppKit can consume Control+Tab before GLFW sees a press. Keep native
           file panels on their responder chain and route canvas tabs once. */
        if (!native_panel_active && event.keyCode==48 && (event.modifierFlags&NSEventModifierFlagControl) && !(event.modifierFlags&(NSEventModifierFlagCommand|NSEventModifierFlagOption))) {
            enqueue_command(event.modifierFlags&NSEventModifierFlagShift ? DX_COMMAND_TAB_PREVIOUS : DX_COMMAND_TAB_NEXT); return nil;
        }
        if (native_panel_active || app_modal) return event;
        if (event.keyCode==109 && (event.modifierFlags&NSEventModifierFlagShift)) { enqueue_command(DX_COMMAND_CONTEXT); return nil; }
        if (event.keyCode==97) { enqueue_command(event.modifierFlags&NSEventModifierFlagShift ? DX_COMMAND_FOCUS_PREVIOUS : DX_COMMAND_FOCUS_NEXT); return nil; }
        NSString *characters=[event charactersByApplyingModifiers:0]; if (!characters.length) characters=event.charactersIgnoringModifiers; if (!characters.length) return event;
        unichar key=[characters characterAtIndex:0]; NSEventModifierFlags flags=event.modifierFlags;
        unichar letter=(unichar)tolower(key<128 ? key : 0);
        if ((flags&NSEventModifierFlagCommand) && (flags&NSEventModifierFlagOption) && !(flags&NSEventModifierFlagControl)) {
            DxCommand pane=letter=='t' ? DX_COMMAND_TRANSFORM_PANE : letter=='f' ? DX_COMMAND_FIELDS_PANE : letter=='w' ? DX_COMMAND_WORKBOOK_PANE : letter=='i' ? DX_COMMAND_SQL_PANE : DX_COMMAND_NONE;
            if (pane!=DX_COMMAND_NONE) { enqueue_command(pane); return nil; }
        }
        if (letter=='f' && (flags&NSEventModifierFlagCommand) && (flags&NSEventModifierFlagShift) && !(flags&(NSEventModifierFlagOption|NSEventModifierFlagControl))) { enqueue_command(DX_COMMAND_FOCUS_VIEW); return nil; }
        if (key==NSF6FunctionKey) { enqueue_command(flags&NSEventModifierFlagShift ? DX_COMMAND_FOCUS_PREVIOUS : DX_COMMAND_FOCUS_NEXT); return nil; }
        if (!text_input && !(flags&(NSEventModifierFlagCommand|NSEventModifierFlagControl)) &&
            (key_tips || (flags&NSEventModifierFlagOption)) &&
            ((letter>='a' && letter<='z') || (key>='0' && key<='9'))) {
            key_tips=true; /* remaining presses in this poll also belong to tips */
            return nil;
        }
        if (text_input || !(flags&NSEventModifierFlagOption) || (flags&(NSEventModifierFlagCommand|NSEventModifierFlagControl))) return event;
        key=(unichar)tolower(key<128 ? key : 0);
        DxCommand shortcut=key=='h' ? DX_COMMAND_ALT_H : key=='d' ? DX_COMMAND_ALT_D : key=='v' ? DX_COMMAND_ALT_V :
            pivot_context && key=='r' ? DX_COMMAND_ALT_R : pivot_context && key=='c' ? DX_COMMAND_ALT_C : pivot_context && key=='f' ? DX_COMMAND_ALT_F : DX_COMMAND_NONE;
        if (shortcut!=DX_COMMAND_NONE) { enqueue_command(shortcut); return nil; }
        return event;
    }];
    if (!actions) {
        actions=[[DuxcelMenuActions alloc] init];
        install_application_menu();
        install_application_delegate();
        NSMenuItem *root=[[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
        NSMenu *file=[[NSMenu alloc] initWithTitle:@"File"];
        NSArray *titles=@[@"Open…",@"Save",@"Save As…",@"New Workbook",@"Settings…",@"Export Parquet…",@"Import Table…",@"Close Window",@"Versions…"];
        NSArray *keys=@[@"o",@"s",@"s",@"n",@",",@"e",@"",@"w",@""];
        const DxCommand commands[]={DX_COMMAND_OPEN,DX_COMMAND_SAVE,DX_COMMAND_SAVE_AS,DX_COMMAND_NEW_WORKBOOK,DX_COMMAND_SETTINGS,DX_COMMAND_EXPORT,DX_COMMAND_IMPORT,DX_COMMAND_CLOSE,DX_COMMAND_VERSIONS};
        for (NSUInteger i=0;i<titles.count;i++) {
            NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:titles[i] action:@selector(invoke:) keyEquivalent:keys[i]];
            item.target=actions; item.tag=commands[i]; item.keyEquivalentModifierMask=NSEventModifierFlagCommand | (i==2 || i==5 ? NSEventModifierFlagShift : 0);
            [file addItem:item]; [item release];
        }
        root.submenu=file; [NSApp.mainMenu insertItem:root atIndex:1]; [file release]; [root release];
        root=[[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
        NSMenu *edit=[[NSMenu alloc] initWithTitle:@"Edit"];
        native_edit_menu=edit; /* Retained by the application menu. */
        NSArray *edit_titles=@[@"Undo",@"Redo",@"Copy",@"Paste",@"Select All"];
        NSArray *edit_keys=@[@"z",@"z",@"c",@"v",@"a"];
        const DxCommand edit_commands[]={DX_COMMAND_UNDO,DX_COMMAND_REDO,DX_COMMAND_COPY,DX_COMMAND_PASTE,DX_COMMAND_SELECT_ALL};
        for (NSUInteger i=0;i<edit_titles.count;i++) {
            NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:edit_titles[i] action:@selector(invoke:) keyEquivalent:edit_keys[i]];
            item.target=actions; item.tag=edit_commands[i]; item.keyEquivalentModifierMask=NSEventModifierFlagCommand | (i==1 ? NSEventModifierFlagShift : 0);
            [edit addItem:item]; [item release];
        }
        NSMenuItem *inspect=[[NSMenuItem alloc] initWithTitle:@"Inspect Cell Value" action:@selector(invoke:) keyEquivalent:@"i"];
        inspect.target=actions; inspect.tag=DX_COMMAND_INSPECT; inspect.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagShift;
        [edit addItem:inspect]; [inspect release];
        NSMenuItem *statistics=[[NSMenuItem alloc] initWithTitle:@"Selection Statistics…" action:@selector(invoke:) keyEquivalent:@"m"];
        statistics.target=actions; statistics.tag=DX_COMMAND_STATISTICS; statistics.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagShift;
        [edit addItem:statistics]; [statistics release];
        root.submenu=edit; [NSApp.mainMenu insertItem:root atIndex:2]; [edit release]; [root release];
        root=[[NSMenuItem alloc] initWithTitle:@"Insert" action:nil keyEquivalent:@""];
        NSMenu *insert=[[NSMenu alloc] initWithTitle:@"Insert"];
        NSMenuItem *pivot=[[NSMenuItem alloc] initWithTitle:@"Pivot Table" action:@selector(invoke:) keyEquivalent:@"p"];
        pivot.target=actions; pivot.tag=DX_COMMAND_PIVOT; pivot.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagOption;
        [insert addItem:pivot]; [pivot release]; root.submenu=insert;
        [NSApp.mainMenu insertItem:root atIndex:3]; [insert release]; [root release];
        root=[[NSMenuItem alloc] initWithTitle:@"Data" action:nil keyEquivalent:@""];
        NSMenu *data=[[NSMenu alloc] initWithTitle:@"Data"];
        NSArray *data_titles=@[@"Edit Source Query…",@"Remove / Restore Duplicate Rows",@"Sort by Multiple Columns…"];
        const DxCommand data_commands[]={DX_COMMAND_QUERY,DX_COMMAND_UNIQUE,DX_COMMAND_SORT};
        for (NSUInteger i=0;i<data_titles.count;i++) {
            NSMenuItem *item=[[NSMenuItem alloc] initWithTitle:data_titles[i] action:@selector(invoke:) keyEquivalent:@""];
            item.target=actions; item.tag=data_commands[i]; [data addItem:item]; [item release];
        }
        root.submenu=data; [NSApp.mainMenu insertItem:root atIndex:4]; [data release]; [root release];
#ifdef DUXCEL_DEV_REVIEW
        NSMenu *review=[[[NSMenu alloc] initWithTitle:@"Review"] autorelease];
        NSMenuItem *review_root=[[[NSMenuItem alloc] initWithTitle:@"Review" action:nil keyEquivalent:@""] autorelease];
        NSMenuItem *report=[[[NSMenuItem alloc] initWithTitle:@"Report UI issue…" action:@selector(invoke:) keyEquivalent:@"."] autorelease];
        report.target=actions; report.tag=DX_COMMAND_REPORT_UI;
        report.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagShift;
        [review addItem:report]; review_root.submenu=review; [NSApp.mainMenu addItem:review_root];
#endif
    }
    [context makeCurrentContext];
}
void dx_native_shutdown(void) {
    @autoreleasepool {
        dx_native_about_shutdown();
        if (launch_observer) [[NSNotificationCenter defaultCenter] removeObserver:launch_observer];
        launch_observer=nil;
        char *path; while ((path=dx_take_open_path())) free(path);
        open_head=0;
        if (pointer_monitor) [NSEvent removeMonitor:pointer_monitor];
        if (gesture_monitor) [NSEvent removeMonitor:gesture_monitor];
        if (shortcut_monitor) [NSEvent removeMonitor:shortcut_monitor];
        if (scroll_monitor) [NSEvent removeMonitor:scroll_monitor];
        pointer_monitor=gesture_monitor=shortcut_monitor=nil;
        scroll_monitor=nil; pending_pan=frame_pan=(DxPanInput){0}; pan_phase=DX_PAN_NONE;
        pan_begin=pan_direct=frame_pan_valid=false; pan_seen=0;
        canvas_window=nil; canvas_view=nil; native_titlebar_height=0;
        pointer_count=pointer_head=command_count=command_head=modifier_count=modifier_head=0;
        pointer_recording[0]=pointer_recording[1]=false; magnification=0;
        app_modal=text_input=pivot_context=key_tips=quit_requested=native_panel_active=false;
        active_native_panel=nil; trace_input=false;
#ifdef DUXCEL_DEV_REVIEW
        review_request=false;
#endif
        /* Remove only menus installed by this session. Restore GLFW's delegate
           before its shutdown, so the next session wraps the new delegate. */
        NSArray *roots=[[NSApp.mainMenu.itemArray copy] autorelease];
        if (actions) for (NSMenuItem *root in roots) {
            NSMenu *menu=root.submenu;
            if (menu.numberOfItems && [menu itemAtIndex:0].target==actions && root!=[NSApp.mainMenu itemAtIndex:0])
                [NSApp.mainMenu removeItem:root];
            else for (NSMenuItem *item in menu.itemArray) if (item.target==actions) {
                item.target=NSApp; item.action=@selector(orderFrontStandardAboutPanel:);
            }
        }
        native_edit_menu=nil;
        if (native_app_root.submenu==native_app_menu) {
            if (native_app_root_added) [NSApp.mainMenu removeItem:native_app_root];
            else native_app_root.submenu=previous_app_menu;
        }
        NSApp.servicesMenu=previous_services_menu;
        [previous_app_menu release]; [previous_services_menu release];
        previous_app_menu=previous_services_menu=native_app_menu=nil; native_app_root=nil; native_app_root_added=false;
        if (NSApp.delegate==(id<NSApplicationDelegate>)application_delegate)
            NSApp.delegate=(id<NSApplicationDelegate>)[application_delegate previousDelegate];
        [application_delegate release]; application_delegate=nil;
        [actions release]; actions=nil;
    }
}
bool dx_native_set_icon(const unsigned char *icns,size_t length) {
    if (!icns || !length || !NSApp) return false;
    @autoreleasepool {
        NSOpenGLContext *context=[NSOpenGLContext currentContext];
        NSData *data=[NSData dataWithBytes:icns length:length];
        NSImage *icon=[[NSImage alloc] initWithData:data];
        bool valid=icon && icon.isValid;
        if (valid) NSApp.applicationIconImage=icon;
        [icon release]; [context makeCurrentContext]; return valid;
    }
}
static NSModalResponse run_panel(NSSavePanel *panel) {
    /* Background automation must never surface an OS modal or restore its
       canvas as a key window. Real panel/responder QA is explicitly visible. */
    if (IsWindowHidden()) { fprintf(stderr,"Native file panel requires visible QA (DUXCEL_TEST_VISIBLE=1).\n"); return NSModalResponseCancel; }
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    NSWindow *canvas=[context.view.window retain];
    NSResponder *responder=[canvas.firstResponder retain];
    /* Let AppKit own the modal session, including nested Go to Folder panels.
       A manually nested session on the canvas window intercepts their keys. */
    /* Standard responder actions give native field editors their normal menu
       validation and key equivalents instead of queueing canvas commands. */
    SEL selectors[]={@selector(undo:),@selector(redo:),@selector(copy:),@selector(paste:),@selector(selectAll:)};
    for (NSInteger i=0;i<5 && i<native_edit_menu.numberOfItems;i++) { NSMenuItem *item=[native_edit_menu itemAtIndex:i]; item.target=nil; item.action=selectors[i]; }
    active_native_panel=panel; native_panel_active=true;
    /* abortModal unwinds the innermost session. A second modal session (Go to
       Folder) can be on top, so continue unwinding until runModal returns.
       raylib's close flag only updates after event polling; do not poll it
       reentrantly from AppKit. This timer exists only while the panel is open. */
    NSTimer *quit_timer=[NSTimer timerWithTimeInterval:.05 repeats:YES block:^(NSTimer *timer) {
        (void)timer;
        if (quit_requested) { [panel cancel:nil]; [NSApp abortModal]; }
    }];
    [[NSRunLoop currentRunLoop] addTimer:quit_timer forMode:NSModalPanelRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:quit_timer forMode:NSRunLoopCommonModes];
    NSModalResponse result=[panel runModal]; native_panel_active=false; active_native_panel=nil;
    [quit_timer invalidate];
    [panel orderOut:nil];
    for (NSMenuItem *item in native_edit_menu.itemArray) { item.target=actions; item.action=@selector(invoke:); }
    if (!quit_requested) {
        [canvas makeKeyAndOrderFront:nil];
        [canvas makeFirstResponder:responder ? responder : canvas.contentView];
    }
    [responder release]; [canvas release];
    [context makeCurrentContext];
    return result;
}
float dx_take_magnification(void) { float value=magnification; magnification=0; return value; }
char *dx_open_file_panel(void) {
    @autoreleasepool {
        NSOpenPanel *panel=[NSOpenPanel openPanel];
        panel.title=@"Open data or workbook";
        panel.canChooseDirectories=NO; panel.canChooseFiles=YES; panel.allowsMultipleSelection=NO;
        panel.resolvesAliases=YES;
        if (run_panel(panel)!=NSModalResponseOK) return NULL;
        return dx_strdup(panel.URL.fileSystemRepresentation);
    }
}
char *dx_open_table_panel(bool relink) {
    @autoreleasepool {
        NSOpenPanel *panel=[NSOpenPanel openPanel];
        panel.title=relink ? @"Relink table source" : @"Import table"; panel.prompt=relink ? @"Relink" : @"Import";
        panel.canChooseDirectories=NO; panel.canChooseFiles=YES; panel.allowsMultipleSelection=NO; panel.resolvesAliases=YES;
        NSMutableArray *types=[NSMutableArray array];
        for (NSString *extension in @[@"csv",@"tsv",@"gz",@"parquet",@"duckdb",@"db"]) {
            UTType *type=[UTType typeWithFilenameExtension:extension conformingToType:UTTypeData];
            if (type) [types addObject:type];
        }
        panel.allowedContentTypes=types;
        if (run_panel(panel)!=NSModalResponseOK) return NULL;
        return dx_strdup(panel.URL.fileSystemRepresentation);
    }
}
char *dx_save_file_panel(const char *suggested) {
    @autoreleasepool {
        NSSavePanel *panel=[NSSavePanel savePanel];
        panel.title=@"Save workbook"; panel.canCreateDirectories=YES;
        panel.allowedContentTypes=@[[UTType typeWithFilenameExtension:@"dxls" conformingToType:UTTypeData]];
        panel.extensionHidden=NO;
        NSString *path=suggested ? [NSString stringWithUTF8String:suggested] : @"Workbook.dxls";
        panel.nameFieldStringValue=path.lastPathComponent;
        if (path.isAbsolutePath) panel.directoryURL=[NSURL fileURLWithPath:path.stringByDeletingLastPathComponent isDirectory:YES];
        if (run_panel(panel)!=NSModalResponseOK) return NULL;
        return dx_strdup(panel.URL.fileSystemRepresentation);
    }
}
#pragma clang diagnostic pop
char *dx_export_parquet_panel(const char *suggested) {
    @autoreleasepool {
        NSSavePanel *panel=[NSSavePanel savePanel];
        panel.title=@"Export to Parquet"; panel.message=@"Export the selected table, pivot or chart's complete data result.";
        panel.canCreateDirectories=YES; panel.extensionHidden=NO;
        panel.allowedContentTypes=@[[UTType typeWithFilenameExtension:@"parquet" conformingToType:UTTypeData]];
        NSString *path=suggested ? [NSString stringWithUTF8String:suggested] : @"Data.parquet";
        panel.nameFieldStringValue=path.lastPathComponent;
        if (path.isAbsolutePath) panel.directoryURL=[NSURL fileURLWithPath:path.stringByDeletingLastPathComponent isDirectory:YES];
        if (run_panel(panel)!=NSModalResponseOK) return NULL;
        return dx_strdup(panel.URL.fileSystemRepresentation);
    }
}

char *dx_recovery_location(bool create) {
    @autoreleasepool {
        NSURL *support=[[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] firstObject];
        if (!support) return NULL;
        NSURL *folder=[support URLByAppendingPathComponent:@"Duxcel/Recovery" isDirectory:YES];
        if (create && ![[NSFileManager defaultManager] createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil]) return NULL;
        return dx_strdup(folder.fileSystemRepresentation);
    }
}

char *dx_recovery_directory(void) { return dx_recovery_location(true); }
