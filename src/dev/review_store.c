#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#elif !defined(_WIN32)
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#endif
#include "review_store.h"
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

#define STORE_PATH 4096
static bool fail(char *error,size_t capacity,const char *format,...) {
    if (error && capacity) { va_list args; va_start(args,format); vsnprintf(error,capacity,format,args); va_end(args); }
    return false;
}
static bool identifier(const char *id) {
    if (!id || !*id) return false;
    for (size_t n=0;;n++) {
        unsigned char c=(unsigned char)id[n]; if (!c) return true;
        if (n>=DX_REVIEW_STORE_ID_MAX || !((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c=='-')) return false;
    }
}
static bool absolute(const char *path) {
    if (!path || !*path) return false;
#ifdef _WIN32
    return ((((path[0]>='A'&&path[0]<='Z') || (path[0]>='a'&&path[0]<='z')) && path[1]==':' && (path[2]=='/' || path[2]=='\\')) || (path[0]=='\\' && path[1]=='\\'));
#else
    return path[0]=='/';
#endif
}
static bool path_copy(char *output,size_t capacity,const char *path,char *error,size_t error_capacity) {
    size_t n=0; while (n<STORE_PATH && path[n]) n++;
    if (n>=STORE_PATH || n>=capacity) return fail(error,error_capacity,"Review outbox path is too long");
    memcpy(output,path,n+1); return true;
}
bool dx_review_store_default_root(const char *project_id,char *output,size_t capacity,char *error,size_t error_capacity) {
    if (error && error_capacity) error[0]=0;
    if (output && capacity) output[0]=0;
    if (!output || !capacity || !identifier(project_id)) return fail(error,error_capacity,"Invalid review project ID or output buffer");
    const char *override=getenv("DUXCEL_REVIEW_OUTBOX");
    if (override && *override) {
        if (!absolute(override)) return fail(error,error_capacity,"DUXCEL_REVIEW_OUTBOX must be an absolute project outbox path");
        return path_copy(output,capacity,override,error,error_capacity);
    }
    char path[STORE_PATH]; const char *base=NULL; int length;
#ifdef _WIN32
    base=getenv("LOCALAPPDATA");
    if (!absolute(base)) return fail(error,error_capacity,"LOCALAPPDATA is unavailable for the review outbox");
    length=snprintf(path,sizeof(path),"%s/Duxcel/Review/%s",base,project_id);
#elif defined(__APPLE__)
    base=getenv("HOME");
    if (!absolute(base)) return fail(error,error_capacity,"HOME is unavailable for the review outbox");
    length=snprintf(path,sizeof(path),"%s/Library/Application Support/Duxcel/Review/%s",base,project_id);
#else
    base=getenv("XDG_DATA_HOME");
    if (base && *base && absolute(base)) length=snprintf(path,sizeof(path),"%s/Duxcel/Review/%s",base,project_id);
    else {
        base=getenv("HOME");
        if (!absolute(base)) return fail(error,error_capacity,"HOME is unavailable for the review outbox");
        length=snprintf(path,sizeof(path),"%s/.local/share/Duxcel/Review/%s",base,project_id);
    }
#endif
    if (length<0 || (size_t)length>=sizeof(path)) return fail(error,error_capacity,"Review outbox path is too long");
    return path_copy(output,capacity,path,error,error_capacity);
}

#ifndef _WIN32
static int make_root(const char *root) {
    char path[STORE_PATH]; memcpy(path,root,strlen(root)+1);
    int directory=open("/",O_RDONLY|O_DIRECTORY); if (directory<0) return -1;
    char *part=path+1;
    while (*part) {
        while (*part=='/') part++;
        if (!*part) break;
        char *end=part; while (*end && *end!='/') end++;
        char saved=*end; *end=0; char *next=end+(saved ? 1 : 0);
        while (*next=='/') next++;
        bool final=!*next;
        bool created=mkdirat(directory,part,0700)==0;
        if ((!created && errno!=EEXIST) || (created && fsync(directory))) { int code=errno; close(directory); errno=code; return -1; }
        int child=openat(directory,part,O_RDONLY|O_DIRECTORY|(final ? O_NOFOLLOW : 0));
        if (child<0) { int code=errno; close(directory); errno=code; return -1; }
        close(directory); directory=child; part=next;
    }
    return directory;
}
static bool write_file(int directory,const char *name,const void *bytes,size_t length) {
    int fd=openat(directory,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600); if (fd<0) return false;
    const unsigned char *at=bytes; bool ok=true;
    while (length) {
        ssize_t amount=write(fd,at,length);
        if (amount<0 && errno==EINTR) continue;
        if (amount<=0) { if (!amount) errno=EIO; ok=false; break; }
        at+=amount; length-=(size_t)amount;
    }
    if (ok && fsync(fd)) ok=false;
    int saved=errno; if (close(fd) && ok) { ok=false; saved=errno; } errno=saved; return ok;
}
static int publish(int directory,const char *temporary,const char *report) {
#if defined(__APPLE__)
    return renameatx_np(directory,temporary,directory,report,RENAME_EXCL);
#elif defined(__linux__) && defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2,directory,temporary,directory,report,1u /* RENAME_NOREPLACE */);
#else
    (void)directory; (void)temporary; (void)report; errno=ENOTSUP; return -1;
#endif
}
static bool store_native(const char *root,const char *id,const char *json,size_t json_length,const unsigned char *png,size_t png_length,char *error,size_t error_capacity) {
    int parent=make_root(root);
    if (parent<0) return fail(error,error_capacity,"Cannot open review outbox: %s",strerror(errno));
    struct stat existing;
    if (!fstatat(parent,id,&existing,AT_SYMLINK_NOFOLLOW) || errno!=ENOENT) {
        close(parent); return fail(error,error_capacity,"Review report already exists or cannot be inspected");
    }
    char temporary[192]; int directory=-1; bool owned=false,published=false,ok=false; int saved=0;
    for (unsigned attempt=0;attempt<256;attempt++) {
        snprintf(temporary,sizeof(temporary),".tmp-%s-%lu-%llx-%u",id,(unsigned long)getpid(),(unsigned long long)time(NULL),attempt);
        if (!mkdirat(parent,temporary,0700)) { owned=true; break; }
        if (errno!=EEXIST) break;
    }
    if (!owned) { saved=errno; goto done; }
    directory=openat(parent,temporary,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
    if (directory<0) { saved=errno; goto done; }
    if (!write_file(directory,"manifest.json",json,json_length) || !write_file(directory,"frame.png",png,png_length) || fsync(directory)) { saved=errno; goto done; }
    if (publish(parent,temporary,id)) { saved=errno; goto done; }
    published=true;
    if (fsync(parent)) { saved=errno; goto done; }
    ok=true;
done:
    if (owned && !published) {
        if (directory>=0) { unlinkat(directory,"manifest.json",0); unlinkat(directory,"frame.png",0); }
        unlinkat(parent,temporary,AT_REMOVEDIR);
    }
    if (directory>=0) close(directory);
    close(parent);
    if (!ok) return fail(error,error_capacity,published ? "Report published; parent durability flush failed: %s" : "Cannot save review report: %s",strerror(saved ? saved : EIO));
    return true;
}
#else
static bool wide_path(const char *path,wchar_t out[STORE_PATH]) {
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,out,STORE_PATH)) { SetLastError(ERROR_INVALID_NAME); return false; }
    return true;
}
static bool directory_path(const char *path,bool create) {
    wchar_t wide[STORE_PATH]; if (!wide_path(path,wide)) return false;
    if (create) {
        for (size_t i=3;;i++) if (!wide[i] || wide[i]=='/' || wide[i]=='\\') {
            wchar_t saved=wide[i]; wide[i]=0;
            if (!CreateDirectoryW(wide,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
            wide[i]=saved; if (!saved) break;
        }
    }
    DWORD kind=GetFileAttributesW(wide);
    return kind!=INVALID_FILE_ATTRIBUTES && (kind&FILE_ATTRIBUTE_DIRECTORY) && !(kind&FILE_ATTRIBUTE_REPARSE_POINT);
}
static bool win_file(const char *path,const void *bytes,size_t length) {
    wchar_t wide[STORE_PATH]; if (!wide_path(path,wide)) return false;
    HANDLE file=CreateFileW(wide,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return false;
    const unsigned char *at=bytes; bool ok=true;
    while (length) {
        DWORD count=0,chunk=(DWORD)(length>1048576 ? 1048576 : length);
        if (!WriteFile(file,at,chunk,&count,NULL) || !count) { ok=false; break; }
        at+=count; length-=count;
    }
    if (ok && !FlushFileBuffers(file)) ok=false;
    DWORD saved=GetLastError(); if (!CloseHandle(file) && ok) { ok=false; saved=GetLastError(); } SetLastError(saved); return ok;
}
static bool store_native(const char *root,const char *id,const char *json,size_t json_length,const unsigned char *png,size_t png_length,char *error,size_t error_capacity) {
    if (!directory_path(root,true)) return fail(error,error_capacity,"Cannot create/open review outbox (Windows error %lu)",(unsigned long)GetLastError());
    char temporary[STORE_PATH],target[STORE_PATH],manifest[STORE_PATH],frame[STORE_PATH];
    wchar_t temp_w[STORE_PATH]={0},target_w[STORE_PATH]={0},manifest_w[STORE_PATH]={0},frame_w[STORE_PATH]={0};
    int length=snprintf(target,sizeof(target),"%s/%s",root,id);
    if (length<0 || (size_t)length>=sizeof(target) || !wide_path(target,target_w)) return fail(error,error_capacity,"Review report path is too long");
    if (GetFileAttributesW(target_w)!=INVALID_FILE_ATTRIBUTES) return fail(error,error_capacity,"Review report already exists");
    bool owned=false,ok=false;
    for (unsigned attempt=0;attempt<256;attempt++) {
        length=snprintf(temporary,sizeof(temporary),"%s/.tmp-%s-%lu-%llu-%u",root,id,(unsigned long)GetCurrentProcessId(),(unsigned long long)GetTickCount64(),attempt);
        if (length<0 || (size_t)length>=sizeof(temporary) || !wide_path(temporary,temp_w)) break;
        if (CreateDirectoryW(temp_w,NULL)) { owned=true; break; }
        if (GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    if (!owned) return fail(error,error_capacity,"Cannot stage review report (Windows error %lu)",(unsigned long)GetLastError());
    length=snprintf(manifest,sizeof(manifest),"%s/manifest.json",temporary);
    if (length<0 || (size_t)length>=sizeof(manifest) || !wide_path(manifest,manifest_w)) goto done;
    length=snprintf(frame,sizeof(frame),"%s/frame.png",temporary);
    if (length<0 || (size_t)length>=sizeof(frame) || !wide_path(frame,frame_w)) goto done;
    if (!win_file(manifest,json,json_length) || !win_file(frame,png,png_length)) goto done;
    /* No REPLACE_EXISTING: even an empty existing directory is protected. */
    if (!MoveFileExW(temp_w,target_w,MOVEFILE_WRITE_THROUGH)) goto done;
    ok=true;
done:
    if (!ok) { DWORD saved=GetLastError(); if (*manifest_w) DeleteFileW(manifest_w); if (*frame_w) DeleteFileW(frame_w); RemoveDirectoryW(temp_w); SetLastError(saved); }
    return ok ? true : fail(error,error_capacity,"Cannot save review report (Windows error %lu)",(unsigned long)GetLastError());
}
#endif

bool dx_review_store_write(const char *root,const char *id,const char *json,const unsigned char *png,size_t png_length,char *error,size_t error_capacity) {
    static const unsigned char signature[8]={137,80,78,71,13,10,26,10};
    if (error && error_capacity) error[0]=0;
    if (!absolute(root) || !identifier(id) || !json || !png) return fail(error,error_capacity,"Invalid review outbox, report ID or owned payload");
    size_t path_length=0; while (path_length<STORE_PATH && root[path_length]) path_length++;
    if (path_length>=STORE_PATH || path_length+DX_REVIEW_STORE_ID_MAX+100>=STORE_PATH) return fail(error,error_capacity,"Review outbox path is too long");
    size_t length=0; while (length<DX_REVIEW_STORE_JSON_MAX && json[length]) length++;
    if (!length || length>=DX_REVIEW_STORE_JSON_MAX) return fail(error,error_capacity,"Review manifest exceeds its 4 MiB limit");
    if (png_length<sizeof(signature) || png_length>DX_REVIEW_STORE_PNG_MAX || memcmp(png,signature,sizeof(signature))) return fail(error,error_capacity,"Invalid PNG signature or review image exceeds its 128 MiB limit");
    return store_native(root,id,json,length,png,png_length,error,error_capacity);
}
