/* Test-only filesystem errors, restricted to the driver's exact journal/WAL.
   Never linked or packaged into the product. */
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static ssize_t (*original_write)(int,const void *,size_t);
static ssize_t (*original_pwrite)(int,const void *,size_t,off_t);
static int (*original_fsync)(int);
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned hits;
static bool disabled;
#ifndef __APPLE__
static void symbol(void *out,const char *name,size_t bytes) { void *value=dlsym(RTLD_NEXT,name); if (bytes!=sizeof(value)) abort(); memcpy(out,&value,bytes); }
#endif
__attribute__((constructor)) static void install(void) {
#ifdef __APPLE__
    /* Interposition excludes bindings originating in the replacement image.
       dlsym can resolve an already-interposed symbol and recurse instead. */
    original_write=write; original_pwrite=pwrite; original_fsync=fsync;
#else
    symbol(&original_write,"write",sizeof(original_write)); symbol(&original_pwrite,"pwrite",sizeof(original_pwrite)); symbol(&original_fsync,"fsync",sizeof(original_fsync));
#endif
    disabled=getenv("DUXCEL_JOURNAL_FAULT_DISARMED")!=NULL;
}
unsigned duxcel_journal_fault_disable(void) { pthread_mutex_lock(&mutex); disabled=true; unsigned count=hits; pthread_mutex_unlock(&mutex); return count; }
void duxcel_journal_fault_arm(void) { pthread_mutex_lock(&mutex); disabled=false; hits=0; pthread_mutex_unlock(&mutex); }
static bool owned(int fd) {
    const char *target=getenv("DUXCEL_JOURNAL_FAULT_PATH"); if (!target || !*target) return false;
    char path[4096]={0};
#ifdef __APPLE__
    if (fcntl(fd,F_GETPATH,path)<0) return false;
#else
    char link[64]; snprintf(link,sizeof(link),"/proc/self/fd/%d",fd); ssize_t size=readlink(link,path,sizeof(path)-1); if (size<0) return false; path[size]=0;
#endif
    size_t n=strlen(target); return !strncmp(path,target,n) && (!path[n] || !strcmp(path+n,".wal"));
}
/* 0 forward, 1 error, 2 short write followed by persistent ENOSPC. */
static unsigned action(int fd,bool sync,size_t bytes,int *code) {
    const char *mode=getenv("DUXCEL_JOURNAL_FAULT"); if (!mode || !owned(fd)) return 0;
    bool sync_mode=!strcmp(mode,"fsync"); if (sync!=sync_mode) return 0;
    pthread_mutex_lock(&mutex);
    if (disabled) { pthread_mutex_unlock(&mutex); return 0; }
    unsigned count=++hits; pthread_mutex_unlock(&mutex);
    unsigned result=!strcmp(mode,"partial") && count==1 && bytes>1 ? 2 : 1;
    *code=!strcmp(mode,"eio") || sync_mode ? EIO : ENOSPC;
    const char *message=result==2 ? "JOURNAL_FAULT partial-write\n" : *code==EIO ? "JOURNAL_FAULT EIO\n" : "JOURNAL_FAULT ENOSPC\n";
    if (original_write) original_write(STDERR_FILENO,message,strlen(message));
    return result;
}
#ifdef __APPLE__
#define WRAP(name) fault_##name
#else
#define WRAP(name) name
#endif
ssize_t WRAP(write)(int fd,const void *buffer,size_t bytes) {
    int code=EIO; unsigned result=action(fd,false,bytes,&code);
    if (result==1 || !original_write) { errno=code; return -1; }
    return original_write(fd,buffer,result==2 ? bytes/2 : bytes);
}
ssize_t WRAP(pwrite)(int fd,const void *buffer,size_t bytes,off_t offset) {
    int code=EIO; unsigned result=action(fd,false,bytes,&code);
    if (result==1 || !original_pwrite) { errno=code; return -1; }
    return original_pwrite(fd,buffer,result==2 ? bytes/2 : bytes,offset);
}
int WRAP(fsync)(int fd) {
    int code=EIO; unsigned result=action(fd,true,0,&code);
    if (result || !original_fsync) { errno=code; return -1; }
    return original_fsync(fd);
}
#ifdef __APPLE__
#define INTERPOSE(replacement,replacee) __attribute__((used)) static const struct { const void *new_function,*old_function; } pair_##replacee __attribute__((section("__DATA,__interpose"))) = {(const void *)(uintptr_t)&replacement,(const void *)(uintptr_t)&replacee}
#include <stdint.h>
INTERPOSE(fault_write,write);
INTERPOSE(fault_pwrite,pwrite);
INTERPOSE(fault_fsync,fsync);
#endif
