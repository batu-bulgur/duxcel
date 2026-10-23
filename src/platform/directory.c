#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "directory.h"
#include "../common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
struct DxDirectory { DIR *handle; bool ended; };
static DxPathKind kind(const struct stat *s) {
    return S_ISLNK(s->st_mode) ? DX_PATH_SYMLINK : S_ISDIR(s->st_mode) ? DX_PATH_DIRECTORY : S_ISREG(s->st_mode) ? DX_PATH_FILE : DX_PATH_OTHER;
}
DxPathKind dx_path_kind(const char *path) {
    struct stat s; if (!path || !*path) return DX_PATH_UNAVAILABLE;
    if (!lstat(path,&s)) return kind(&s);
    return errno==ENOENT || errno==ENOTDIR ? DX_PATH_MISSING : DX_PATH_UNAVAILABLE;
}
bool dx_path_independent_file(const char *path) { struct stat s; return path && *path && !lstat(path,&s) && S_ISREG(s.st_mode) && s.st_nlink==1; }
#else
struct DxDirectory { bool ended; };
DxPathKind dx_path_kind(const char *path) { (void)path; return DX_PATH_UNAVAILABLE; }
bool dx_path_independent_file(const char *path) { (void)path; return false; }
#endif
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
DxDirectoryOpen dx_directory_open(const char *path,DxDirectory **out,char **error) {
    if (!path || !*path || strlen(path)>65536 || !out || *out) { fail(error,"Choose a directory and an empty native cursor."); return DX_DIRECTORY_ERROR; }
#ifndef _WIN32
    DIR *handle=opendir(path);
    if (!handle) {
        if (errno==ENOENT) return DX_DIRECTORY_MISSING;
        fail(error,"Cannot open the native directory."); return DX_DIRECTORY_ERROR;
    }
    DxDirectory *directory=dx_alloc(sizeof(*directory)); directory->handle=handle; *out=directory; return DX_DIRECTORY_OPEN;
#else
    fail(error,"Streaming native directory enumeration is unavailable on this host."); return DX_DIRECTORY_ERROR;
#endif
}
DxDirectoryNext dx_directory_next(DxDirectory *directory,DxDirectoryEntry *out,char **error) {
    if (!directory || !out) { fail(error,"Choose an open native directory cursor."); return DX_DIRECTORY_FAILED; }
    if (directory->ended) return DX_DIRECTORY_END;
#ifndef _WIN32
    struct dirent *entry;
    for (;;) {
        errno=0; entry=readdir(directory->handle);
        if (!entry) {
            if (errno) { fail(error,"Native directory enumeration failed."); return DX_DIRECTORY_FAILED; }
            directory->ended=true; return DX_DIRECTORY_END;
        }
        if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) break;
    }
    struct stat s; DxPathKind type;
    if (!fstatat(dirfd(directory->handle),entry->d_name,&s,AT_SYMLINK_NOFOLLOW)) type=kind(&s);
    else if (errno==ENOENT || errno==ENOTDIR) type=DX_PATH_MISSING;
    else { fail(error,"Cannot inspect a native directory entry."); return DX_DIRECTORY_FAILED; }
    *out=(DxDirectoryEntry){entry->d_name,type}; return DX_DIRECTORY_ENTRY;
#else
    fail(error,"Streaming native directory enumeration is unavailable on this host."); return DX_DIRECTORY_FAILED;
#endif
}
void dx_directory_close(DxDirectory *directory) {
    if (!directory) return;
#ifndef _WIN32
    closedir(directory->handle);
#endif
    free(directory);
}
