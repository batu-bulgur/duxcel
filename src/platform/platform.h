#ifndef DUXCEL_PLATFORM_H
#define DUXCEL_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
typedef CRITICAL_SECTION DxMutex;
typedef CONDITION_VARIABLE DxCond;
typedef HANDLE DxThread;
#else
#include <pthread.h>
typedef pthread_mutex_t DxMutex;
typedef pthread_cond_t DxCond;
typedef pthread_t DxThread;
#endif
void dx_mutex_init(DxMutex *m);
void dx_mutex_destroy(DxMutex *m);
void dx_lock(DxMutex *m);
void dx_unlock(DxMutex *m);
void dx_cond_init(DxCond *c);
void dx_cond_destroy(DxCond *c);
void dx_signal(DxCond *c);
void dx_cond_wait(DxCond *c, DxMutex *m);
bool dx_thread_start(DxThread *t, void *(*fn)(void *), void *arg);
void dx_thread_join(DxThread t);
double dx_millis(void);
void dx_sleep_ms(unsigned ms);
/* Existing paths only. Caller owns result; NULL means missing/inaccessible. */
char *dx_absolute_path(const char *path);
/* Main thread only. Returned paths use the C allocator; NULL means cancelled. */
char *dx_open_file_panel(void);
char *dx_open_table_panel(bool relink);
char *dx_save_file_panel(const char *suggested_path);
char *dx_export_parquet_panel(const char *suggested_path);
/* Export staging uses a reserved sibling file and an atomic final replacement. */
char *dx_temp_sibling(const char *path);
char *dx_temp_directory(void); /* new private writable absolute directory; caller owns */
void dx_remove_temp_directory(const char *path); /* remove empty owned directory */
bool dx_replace_file(const char *temporary,const char *target);
bool dx_same_file(const char *first,const char *second);
uint64_t dx_file_stamp(const char *path);
void dx_native_setup(void *window);
/* Canvas-space traffic-light reservation; zero on other platforms/fullscreen. */
float dx_native_titlebar_inset(void);
void dx_native_titlebar_height(float height);
void dx_native_shutdown(void); /* detach input/menu hooks before destroying the window */
bool dx_native_set_icon(const unsigned char *icns,size_t length);
void dx_native_context(bool modal,bool text_input,bool pivot,bool key_tips);
bool dx_native_quit_requested(void);
void dx_native_acknowledge_quit(void);
typedef struct {
    int ribbon_mode,previous_ribbon_mode;
    bool workbook,fields,transformations,workbook_collapsed,fields_collapsed,reduced_motion,transformations_collapsed,freeze;
    float sidebar_width,pipeline_width,interface_scale;
    unsigned statistics_mask;
} DxPresentation;
/* App-level presentation, separate from data/workbook definitions. Main thread. */
bool dx_presentation_load(DxPresentation *presentation);
void dx_presentation_save(const DxPresentation *presentation);
float dx_take_magnification(void);
#ifdef DUXCEL_BROWSER_HOST
/* Non-consuming per-frame CSS-pixel pan; never monitor/backing pixels. */
bool dx_web_pixel_pan(float *x,float *y);
#endif
typedef enum { DX_COMMAND_NONE,DX_COMMAND_OPEN,DX_COMMAND_SAVE,DX_COMMAND_SAVE_AS,DX_COMMAND_NEW_SHEET,DX_COMMAND_SETTINGS,
    DX_COMMAND_COPY,DX_COMMAND_PASTE,DX_COMMAND_SELECT_ALL,DX_COMMAND_UNDO,DX_COMMAND_REDO,DX_COMMAND_PIVOT,
    DX_COMMAND_FOCUS_NEXT,DX_COMMAND_FOCUS_PREVIOUS,DX_COMMAND_ALT_H,DX_COMMAND_ALT_D,DX_COMMAND_ALT_V,DX_COMMAND_ALT_R,DX_COMMAND_ALT_C,DX_COMMAND_ALT_F,DX_COMMAND_INSPECT,DX_COMMAND_EXPORT,DX_COMMAND_STATISTICS,DX_COMMAND_CONTEXT,
    DX_COMMAND_TIP_1,DX_COMMAND_TIP_2,DX_COMMAND_TIP_3,DX_COMMAND_TIP_4,DX_COMMAND_TIP_5,DX_COMMAND_TIP_6,DX_COMMAND_TIP_7,DX_COMMAND_TIP_8,DX_COMMAND_TIP_9,DX_COMMAND_TIP_0,DX_COMMAND_TAB_NEXT,DX_COMMAND_TAB_PREVIOUS,DX_COMMAND_IMPORT,DX_COMMAND_QUERY,DX_COMMAND_UNIQUE,DX_COMMAND_SORT,
    DX_COMMAND_TRANSFORM_PANE,DX_COMMAND_FIELDS_PANE,DX_COMMAND_WORKBOOK_PANE,DX_COMMAND_SQL_PANE,DX_COMMAND_FOCUS_VIEW
#ifdef DUXCEL_DEV_REVIEW
    ,DX_COMMAND_REPORT_UI
#endif
} DxCommand;
DxCommand dx_take_command(void);
#ifdef DUXCEL_DEV_REVIEW
/* Review-only command channel precedes modal/worksheet dispatch. */
bool dx_review_take_native_request(void);
#endif
enum { DX_MOD_SHIFT=1,DX_MOD_CONTROL=2,DX_MOD_ALT=4,DX_MOD_SUPER=8 };
typedef struct { int key; unsigned modifiers; } DxKeyModifiers;
/* Captured at native key-down, before a fast chord can release its modifiers. */
bool dx_take_key_modifiers(DxKeyModifiers *event);
typedef struct { float x0,y0,x1,y1; unsigned modifiers; bool right; } DxPointerGesture;
/* Completed left-button gestures; preserve down/drag/up inside one event poll. */
bool dx_take_pointer_gesture(DxPointerGesture *gesture);
#endif
