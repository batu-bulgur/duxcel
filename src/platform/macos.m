#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "platform.h"
#include "../common.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
/* raylib's macOS backend uses NSOpenGLContext. AppKit menu/panel work can change
   the thread's current context; return ownership to the renderer afterwards. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static float magnification;
static id gesture_monitor;
static id shortcut_monitor;
static id pointer_monitor;
static DxPointerGesture pointer_gestures[16],pointer_start[2];
static unsigned pointer_count,pointer_head;
static bool pointer_recording[2];
static DxCommand commands[64];
static unsigned command_head,command_count;
static DxKeyModifiers modifier_events[128];
static unsigned modifier_head,modifier_count;
static void record_key_modifiers(NSEvent *event) {
    int key=0;
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
    if (!key || modifier_count==128) return;
    NSEventModifierFlags flags=event.modifierFlags;
    unsigned modifiers=(flags&NSEventModifierFlagShift ? DX_MOD_SHIFT : 0) | (flags&NSEventModifierFlagControl ? DX_MOD_CONTROL : 0) | (flags&NSEventModifierFlagOption ? DX_MOD_ALT : 0) | (flags&NSEventModifierFlagCommand ? DX_MOD_SUPER : 0);
    modifier_events[(modifier_head+modifier_count)%128]=(DxKeyModifiers){key,modifiers}; modifier_count++;
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
static bool native_panel_active;
static NSSavePanel *active_native_panel;
static bool quit_requested;
static NSMenu *native_edit_menu;
static bool app_modal,text_input,pivot_context,key_tips;
static bool trace_input;
static NSWindow *canvas_window; /* borrowed from GLFW */
/* Preserve GLFW's delegate for display/input lifecycle messages. Termination
   must also unwind AppKit's nested file-panel loop before C cleanup can run. */
@interface DuxcelApplicationDelegate : NSProxy <NSApplicationDelegate> {
    id previous;
}
- (instancetype)initWithDelegate:(id)delegate;
@end
@implementation DuxcelApplicationDelegate
- (instancetype)initWithDelegate:(id)delegate { previous=[delegate retain]; return self; }
- (void)dealloc { [previous release]; [super dealloc]; }
- (BOOL)respondsToSelector:(SEL)selector {
    return selector==@selector(applicationShouldTerminate:) || [previous respondsToSelector:selector];
}
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector { return [previous methodSignatureForSelector:selector]; }
- (void)forwardInvocation:(NSInvocation *)invocation { [invocation invokeWithTarget:previous]; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender; quit_requested=true;
    if (native_panel_active) { [active_native_panel cancel:nil]; [NSApp abortModal]; }
    return NSTerminateCancel;
}
@end
static DuxcelApplicationDelegate *application_delegate;
bool dx_presentation_load(DxPresentation *out) {
    @autoreleasepool {
        id stored=[[NSUserDefaults standardUserDefaults] objectForKey:@"DuxcelPresentationV1"];
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
        [[NSUserDefaults standardUserDefaults] setObject:stored forKey:@"DuxcelPresentationV1"]; [stored release];
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
    enqueue_command(value);
}
- (void)about:(id)sender {
    (void)sender; if (native_panel_active) return;
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    NSString *version=[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    if (!version) version=@"Development";
    [NSApp orderFrontStandardAboutPanelWithOptions:@{NSAboutPanelOptionApplicationName:@"Duxcel",NSAboutPanelOptionApplicationVersion:version,NSAboutPanelOptionApplicationIcon:NSApp.applicationIconImage}];
    [context makeCurrentContext];
}
@end
static DuxcelMenuActions *actions;
bool dx_native_quit_requested(void) { return quit_requested; }
void dx_native_acknowledge_quit(void) { quit_requested=false; }
DxCommand dx_take_command(void) {
    if (!command_count) return DX_COMMAND_NONE;
    DxCommand value=commands[command_head]; command_head=(command_head+1)%64; command_count--; return value;
}
void dx_native_setup(void *window) {
    NSOpenGLContext *context=[NSOpenGLContext currentContext];
    (void)window;
    trace_input=getenv("DUXCEL_TRACE_INPUT")!=NULL;
    NSView *canvas=context.view;
    canvas_window=canvas.window;
    if (!pointer_monitor) pointer_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown|NSEventMaskLeftMouseUp|NSEventMaskRightMouseDown|NSEventMaskRightMouseUp handler:^NSEvent *(NSEvent *event) {
        int button=event.type==NSEventTypeRightMouseDown || event.type==NSEventTypeRightMouseUp;
        if (native_panel_active || event.window!=canvas.window) { pointer_recording[button]=false; return event; }
        float x=event.locationInWindow.x,y=canvas.bounds.size.height-event.locationInWindow.y;
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
    if (!shortcut_monitor) shortcut_monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event) {
        if (event.window && event.window!=canvas.window) return event;
        if (trace_input) fprintf(stderr,"Native key code=%u characters=%s flags=%lu panel=%d modal=%d\n",event.keyCode,[event.charactersIgnoringModifiers UTF8String],(unsigned long)event.modifierFlags,native_panel_active,app_modal);
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
        if (key_tips && !text_input && !(flags&(NSEventModifierFlagCommand|NSEventModifierFlagControl|NSEventModifierFlagOption)) && key>='0' && key<='9') { enqueue_command(key=='0' ? DX_COMMAND_TIP_0 : (DxCommand)(DX_COMMAND_TIP_1+key-'1')); return nil; }
        if (text_input || !(flags&NSEventModifierFlagOption) || (flags&(NSEventModifierFlagCommand|NSEventModifierFlagControl))) return event;
        key=(unichar)tolower(key<128 ? key : 0);
        DxCommand shortcut=key=='h' ? DX_COMMAND_ALT_H : key=='d' ? DX_COMMAND_ALT_D : key=='v' ? DX_COMMAND_ALT_V :
            pivot_context && key=='r' ? DX_COMMAND_ALT_R : pivot_context && key=='c' ? DX_COMMAND_ALT_C : pivot_context && key=='f' ? DX_COMMAND_ALT_F : DX_COMMAND_NONE;
        if (shortcut!=DX_COMMAND_NONE) { enqueue_command(shortcut); return nil; }
        return event;
    }];
    if (!actions) {
        actions=[[DuxcelMenuActions alloc] init];
        NSMenu *applicationMenu=[NSApp.mainMenu itemAtIndex:0].submenu;
        [NSApp.mainMenu itemAtIndex:0].title=@"Duxcel"; applicationMenu.title=@"Duxcel";
        NSMenuItem *about=nil;
        for (NSMenuItem *item in applicationMenu.itemArray) {
            if (item.action==@selector(orderFrontStandardAboutPanel:)) about=item;
            if (item.action==@selector(hide:)) item.title=@"Hide Duxcel";
            if (item.action==@selector(terminate:)) item.title=@"Quit Duxcel";
        }
        if (!about) { about=[[[NSMenuItem alloc] initWithTitle:@"About Duxcel" action:@selector(about:) keyEquivalent:@""] autorelease]; [applicationMenu insertItem:about atIndex:0]; }
        about.title=@"About Duxcel"; about.action=@selector(about:); about.target=actions;
        application_delegate=[[DuxcelApplicationDelegate alloc] initWithDelegate:NSApp.delegate];
        NSApp.delegate=(id<NSApplicationDelegate>)application_delegate;
        NSMenuItem *root=[[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
        NSMenu *file=[[NSMenu alloc] initWithTitle:@"File"];
        NSArray *titles=@[@"Open…",@"Save",@"Save As…",@"New Sheet",@"Settings…",@"Export Parquet…",@"Import Table…"];
        NSArray *keys=@[@"o",@"s",@"s",@"n",@",",@"e",@""];
        const DxCommand commands[]={DX_COMMAND_OPEN,DX_COMMAND_SAVE,DX_COMMAND_SAVE_AS,DX_COMMAND_NEW_SHEET,DX_COMMAND_SETTINGS,DX_COMMAND_EXPORT,DX_COMMAND_IMPORT};
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
    }
    [context makeCurrentContext];
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
