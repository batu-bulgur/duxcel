#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "file_lease.h"
#include "../common.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
struct DxFileLease { HANDLE file; };
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
struct DxFileLease { int file; };
#endif
static DxFileLeaseStatus failure(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return DX_FILE_LEASE_ERROR;
}
DxFileLeaseStatus dx_file_lease_acquire(const char *path,bool create,DxFileLease **out,char **error) {
    if (!path || !*path || strlen(path)>65536 || !out || *out) return failure(error,"Choose a private recovery lease path and an empty output handle.");
#ifdef _WIN32
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    if (!length || length>32767) return failure(error,"The recovery lease path is invalid.");
    wchar_t *wide=dx_alloc((size_t)length*sizeof(*wide));
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,length)) { free(wide); return failure(error,"Cannot resolve the recovery lease path."); }
    HANDLE file=CreateFileW(wide,GENERIC_READ|GENERIC_WRITE,0,NULL,create ? OPEN_ALWAYS : OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    DWORD code=GetLastError(); free(wide);
    if (file==INVALID_HANDLE_VALUE) {
        if (code==ERROR_SHARING_VIOLATION || code==ERROR_LOCK_VIOLATION) return DX_FILE_LEASE_BUSY;
        return failure(error,"Cannot open the private recovery lease.");
    }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(file,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks!=1 || info.nFileSizeHigh || info.nFileSizeLow) {
        CloseHandle(file); return failure(error,"Recovery ownership requires a regular zero-byte private lease file.");
    }
#else
    int flags=O_RDWR|O_CLOEXEC|O_NOFOLLOW; if (create) flags|=O_CREAT;
    int file=open(path,flags,0600);
    if (file<0) return failure(error,"Cannot open the private recovery lease.");
    struct stat info;
    if (fstat(file,&info) || !S_ISREG(info.st_mode) || info.st_nlink!=1 || info.st_size) {
        close(file); return failure(error,"Recovery ownership requires a regular zero-byte private lease file.");
    }
    if (flock(file,LOCK_EX|LOCK_NB)) {
        int code=errno; close(file);
        if (code==EWOULDBLOCK || code==EAGAIN) return DX_FILE_LEASE_BUSY;
        return failure(error,"Cannot claim exclusive recovery ownership.");
    }
#endif
    DxFileLease *lease=dx_alloc(sizeof(*lease)); lease->file=file; *out=lease; return DX_FILE_LEASE_ACQUIRED;
}
void dx_file_lease_release(DxFileLease *lease) {
    if (!lease) return;
#ifdef _WIN32
    CloseHandle(lease->file);
#else
    flock(lease->file,LOCK_UN); close(lease->file);
#endif
    free(lease);
}
