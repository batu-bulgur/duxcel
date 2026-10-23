#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#include "platform.h"
#include "../common.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef DUXCEL_BROWSER_HOST
#include <emscripten/emscripten.h>
EM_JS(unsigned, dx_web_clipboard_begin_js, (const char *text), {
    return Module['dxClipboard'] ? Module['dxClipboard'].begin(UTF8ToString(text)) : 0;
})
EM_JS(int, dx_web_clipboard_take_js, (unsigned *id,int *success,char *error,unsigned capacity), {
    const result=Module['dxClipboard']?.take();
    if (!result) return 0;
    HEAPU32[id>>2]=result.id;
    HEAP32[success>>2]=result.ok ? 1 : 0;
    stringToUTF8(result.error || '',error,capacity);
    return 1;
})
unsigned dx_web_clipboard_begin(const char *text) {
    return text ? dx_web_clipboard_begin_js(text) : 0;
}
bool dx_web_clipboard_take(DxWebClipboardResult *result) {
    if (!result) return false;
    int success=0;
    *result=(DxWebClipboardResult){0};
    if (!dx_web_clipboard_take_js(&result->id,&success,result->error,sizeof(result->error))) return false;
    result->success=success!=0;
    return true;
}
EM_JS(int, dx_web_download_ready_js, (void), {
    try { return Module['dxDownload']?.ready()===true ? 1 : 0; }
    catch { return 0; }
})
EM_JS(int, dx_web_download_parquet_js, (const unsigned char *data,unsigned size,const char *name,char *error,unsigned capacity), {
    try {
        const result=Module['dxDownload']?.request(HEAPU8.subarray(data,data+size),UTF8ToString(name));
        if (result?.ok===true) return 1;
        stringToUTF8(result?.error || 'Browser downloads are unavailable.',error,capacity);
    } catch { stringToUTF8('Browser download dispatch failed.',error,capacity); }
    return 0;
})
bool dx_web_download_ready(void) { return dx_web_download_ready_js()!=0; }
bool dx_web_download_parquet(const unsigned char *data,size_t size,const char *name,char **error) {
    if (error) *error=NULL;
    char failure[513]={0};
    if (!data || size<12 || size>64u*1024u*1024u || !name || !*name) snprintf(failure,sizeof(failure),"Invalid bounded Parquet download.");
    else if (dx_web_download_parquet_js(data,(unsigned)size,name,failure,sizeof(failure))) return true;
    if (error) *error=dx_strdup(*failure ? failure : "Browser download dispatch failed.");
    return false;
}
#endif
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
void dx_mutex_init(DxMutex *m) { InitializeCriticalSection(m); }
void dx_mutex_destroy(DxMutex *m) { DeleteCriticalSection(m); }
void dx_lock(DxMutex *m) { EnterCriticalSection(m); }
void dx_unlock(DxMutex *m) { LeaveCriticalSection(m); }
void dx_cond_init(DxCond *c) { InitializeConditionVariable(c); }
void dx_cond_destroy(DxCond *c) { (void)c; }
void dx_signal(DxCond *c) { WakeConditionVariable(c); }
void dx_cond_wait(DxCond *c, DxMutex *m) { SleepConditionVariableCS(c, m, INFINITE); }
typedef struct { void *(*fn)(void *); void *arg; } Start;
static DWORD WINAPI start_thread(LPVOID ptr) {
    Start s = *(Start *)ptr; free(ptr); s.fn(s.arg); return 0;
}
bool dx_thread_start(DxThread *t, void *(*fn)(void *), void *arg) {
    Start *s = dx_alloc(sizeof(*s)); *s = (Start){fn, arg};
    *t = CreateThread(NULL, 0, start_thread, s, 0, NULL);
    if (!*t) free(s);
    return *t != NULL;
}
void dx_thread_join(DxThread t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
double dx_millis(void) {
    LARGE_INTEGER n, f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f);
    return (double)n.QuadPart * 1000.0 / (double)f.QuadPart;
}
void dx_sleep_ms(unsigned ms) { Sleep(ms); }
char *dx_absolute_path(const char *path) { return _fullpath(NULL, path, 0); }
#else
void dx_mutex_init(DxMutex *m) { pthread_mutex_init(m, NULL); }
void dx_mutex_destroy(DxMutex *m) { pthread_mutex_destroy(m); }
void dx_lock(DxMutex *m) { pthread_mutex_lock(m); }
void dx_unlock(DxMutex *m) { pthread_mutex_unlock(m); }
void dx_cond_init(DxCond *c) { pthread_cond_init(c, NULL); }
void dx_cond_destroy(DxCond *c) { pthread_cond_destroy(c); }
void dx_signal(DxCond *c) { pthread_cond_signal(c); }
void dx_cond_wait(DxCond *c, DxMutex *m) { pthread_cond_wait(c, m); }
bool dx_thread_start(DxThread *t, void *(*fn)(void *), void *arg) {
    return pthread_create(t, NULL, fn, arg) == 0;
}
void dx_thread_join(DxThread t) { pthread_join(t, NULL); }
double dx_millis(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
void dx_sleep_ms(unsigned ms) {
    struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}
char *dx_absolute_path(const char *path) { return realpath(path, NULL); }
#endif
char *dx_temp_directory(void) {
#ifdef _WIN32
    char root[MAX_PATH],path[MAX_PATH];
    DWORD length=GetTempPathA(MAX_PATH,root);
    if (!length || length>=MAX_PATH || !GetTempFileNameA(root,"DXL",0,path)) return NULL;
    if (!DeleteFileA(path) || !CreateDirectoryA(path,NULL)) return NULL;
    return dx_strdup(path);
#else
    char system_temp[4096]={0};
#if defined(__APPLE__) && defined(_CS_DARWIN_USER_TEMP_DIR)
    size_t length=confstr(_CS_DARWIN_USER_TEMP_DIR,system_temp,sizeof(system_temp));
    if (!length || length>sizeof(system_temp)) system_temp[0]=0;
#endif
    const char *roots[]={getenv("TMPDIR"),system_temp,"/tmp"};
    for (size_t i=0;i<sizeof(roots)/sizeof(*roots);i++) {
        const char *root=roots[i]; if (!root || *root!='/') continue;
        char *path=dx_printf("%s%sDuxcel-XXXXXX",root,root[strlen(root)-1]=='/' ? "" : "/");
        if (mkdtemp(path)) return path;
        free(path);
    }
    return NULL;
#endif
}
void dx_remove_temp_directory(const char *path) {
    if (!path) return;
#ifdef _WIN32
    RemoveDirectoryA(path);
#else
    rmdir(path);
#endif
}
#ifndef __APPLE__
void dx_input_begin_frame(void) { }
bool dx_input_pixel_pan(DxPanInput *input) { if (input) *input=(DxPanInput){0}; return false; }
#ifdef DUXCEL_DEV_REVIEW
bool dx_review_take_native_request(void) { return false; }
#endif
bool dx_native_quit_requested(void) { return false; }
void dx_native_acknowledge_quit(void) { }
void dx_native_window_visible(bool visible) { (void)visible; }
bool dx_presentation_load(DxPresentation *presentation) { (void)presentation; return false; }
void dx_presentation_save(const DxPresentation *presentation) { (void)presentation; }
char *dx_open_file_panel(void) { return NULL; }
char *dx_open_table_panel(bool relink) { (void)relink; return NULL; }
char *dx_save_file_panel(const char *suggested) { (void)suggested; return NULL; }
char *dx_export_parquet_panel(const char *suggested) { (void)suggested; return NULL; }
void dx_native_setup(void *window) { (void)window; }
void dx_native_prepare(void) {}
char *dx_take_open_path(void) { return NULL; }
float dx_native_titlebar_inset(void) { return 0; }
void dx_native_titlebar_height(float height) { (void)height; }
void dx_native_shutdown(void) {}
void dx_native_test_identity(bool automated) { (void)automated; }
bool dx_native_set_icon(const unsigned char *icns,size_t length) { (void)icns; (void)length; return false; }
void dx_native_context(bool modal,bool text_input,bool pivot,bool key_tips) { (void)modal; (void)text_input; (void)pivot; (void)key_tips; }
#ifdef DUXCEL_BROWSER_HOST
float dx_web_take_magnification(void);
float dx_take_magnification(void) { return dx_web_take_magnification(); }
#else
float dx_take_magnification(void) { return 0; }
#endif
DxCommand dx_take_command(void) { return DX_COMMAND_NONE; }
bool dx_take_key_modifiers(DxKeyModifiers *event) { (void)event; return false; }
bool dx_take_pointer_gesture(DxPointerGesture *gesture) { (void)gesture; return false; }
#endif
char *dx_temp_sibling(const char *path) {
#ifdef _WIN32
    char *parent=dx_strdup(path),*slash=strrchr(parent,'\\');
    if (!slash) slash=strrchr(parent,'/'); if (slash) *slash=0; else { free(parent); parent=dx_strdup("."); }
    char temporary[MAX_PATH]; bool ok=GetTempFileNameA(parent,"DXL",0,temporary)!=0; free(parent);
    return ok ? dx_strdup(temporary) : NULL;
#else
    char *temporary=dx_printf("%s.duxcel-XXXXXX",path); int fd=mkstemp(temporary);
    if (fd<0) { free(temporary); return NULL; }
    close(fd); return temporary;
#endif
}
bool dx_replace_file(const char *temporary,const char *target) {
#ifdef _WIN32
    return MoveFileExA(temporary,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(temporary,target)==0;
#endif
}
bool dx_same_file(const char *first,const char *second) {
    if (!first || !second) return false;
#ifdef _WIN32
    HANDLE a=CreateFileA(first,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    HANDLE b=CreateFileA(second,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    BY_HANDLE_FILE_INFORMATION x,y; bool same=a!=INVALID_HANDLE_VALUE && b!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(a,&x) && GetFileInformationByHandle(b,&y) &&
        x.dwVolumeSerialNumber==y.dwVolumeSerialNumber && x.nFileIndexHigh==y.nFileIndexHigh && x.nFileIndexLow==y.nFileIndexLow;
    if (a!=INVALID_HANDLE_VALUE) CloseHandle(a); if (b!=INVALID_HANDLE_VALUE) CloseHandle(b); return same;
#else
    struct stat a,b;
    return stat(first,&a)==0 && stat(second,&b)==0 && a.st_dev==b.st_dev && a.st_ino==b.st_ino;
#endif
}
uint64_t dx_file_stamp(const char *path) {
    if (!path || !*path) return 0;
    struct stat info; if (stat(path,&info)!=0) return 0;
    uint64_t stamp=(uint64_t)info.st_size ^ (uint64_t)info.st_mtime*UINT64_C(1099511628211) ^ (uint64_t)info.st_ino;
#ifdef __APPLE__
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
    stamp^=(uint64_t)info.st_mtimespec.tv_nsec*UINT64_C(1469598103934665603);
#else
    stamp^=(uint64_t)info.st_mtimensec*UINT64_C(1469598103934665603);
#endif
#elif !defined(_WIN32)
    stamp^=(uint64_t)info.st_mtim.tv_nsec*UINT64_C(1469598103934665603);
#endif
    return stamp;
}
