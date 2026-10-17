#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "source_digest.h"
#include <stdio.h>
#include <string.h>
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static SourceDigestStatus failure(SourceDigestStatus status,char *error,const char *message) {
    if (error) snprintf(error,DX_SOURCE_DIGEST_ERROR_BYTES,"%s",message);
    return status;
}
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
static SourceDigestStatus io_failure(char *error,const char *operation,int saved_errno) {
    if (error) snprintf(error,DX_SOURCE_DIGEST_ERROR_BYTES,"Could not %s source file: %s",operation,strerror(saved_errno));
    return SOURCE_DIGEST_ERROR;
}
static SourceFileIdentity identity(const struct stat *file) {
    return (SourceFileIdentity){
        .device=(uint64_t)file->st_dev,.inode=(uint64_t)file->st_ino,.size=(uint64_t)file->st_size,.links=(uint64_t)file->st_nlink,
        .mode=(uint32_t)file->st_mode,.uid=(uint32_t)file->st_uid,.gid=(uint32_t)file->st_gid,.flags=file->st_flags,.generation=file->st_gen,
        .modified_seconds=(int64_t)file->st_mtimespec.tv_sec,.modified_nanoseconds=(uint32_t)file->st_mtimespec.tv_nsec,
        .changed_seconds=(int64_t)file->st_ctimespec.tv_sec,.changed_nanoseconds=(uint32_t)file->st_ctimespec.tv_nsec,
        .created_seconds=(int64_t)file->st_birthtimespec.tv_sec,.created_nanoseconds=(uint32_t)file->st_birthtimespec.tv_nsec
    };
}
static bool same_identity(SourceFileIdentity a,SourceFileIdentity b) {
    return a.device==b.device && a.inode==b.inode && a.size==b.size && a.links==b.links && a.mode==b.mode && a.uid==b.uid && a.gid==b.gid && a.flags==b.flags && a.generation==b.generation &&
        a.modified_seconds==b.modified_seconds && a.modified_nanoseconds==b.modified_nanoseconds && a.changed_seconds==b.changed_seconds && a.changed_nanoseconds==b.changed_nanoseconds && a.created_seconds==b.created_seconds && a.created_nanoseconds==b.created_nanoseconds;
}
static bool regular_identity(const struct stat *file,SourceFileIdentity expected) {
    return S_ISREG(file->st_mode) && file->st_size>=0 && same_identity(identity(file),expected);
}
/* CommonCrypto's streaming SHA256 ABI is supplied by the existing macOS SDK.
   Keep the local legacy-API deprecation suppression away from other modules. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
SourceDigestStatus source_digest_file(const char *path,SourceDigestCancel cancelled,void *context,SourceDigest *out,char error[DX_SOURCE_DIGEST_ERROR_BYTES]) {
    if (error) error[0]=0;
    if (!path || !*path || !out) return failure(SOURCE_DIGEST_ERROR,error,"Choose a source path and an owned digest result.");
#if !defined(__APPLE__) || defined(__EMSCRIPTEN__)
    (void)cancelled; (void)context;
    return failure(SOURCE_DIGEST_UNSUPPORTED,error,"Source-content verification requires the native macOS SDK; this platform is unsupported.");
#else
    struct stat before,opened,after,current;
    if (lstat(path,&before)!=0) return io_failure(error,"inspect",errno);
    if (!S_ISREG(before.st_mode) || before.st_size<0) return failure(SOURCE_DIGEST_ERROR,error,"Source-content verification requires a regular file; final symlinks are unsupported.");
    SourceDigest next={.identity=identity(&before)};
    /* NONBLOCK prevents a regular-file -> FIFO race from blocking open. */
    int descriptor=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if (descriptor<0) {
        int saved_errno=errno;
        if (lstat(path,&current)!=0 || !regular_identity(&current,next.identity)) return failure(SOURCE_DIGEST_CHANGED,error,"The source path changed before verification opened it.");
        return io_failure(error,"open",saved_errno);
    }
    SourceDigestStatus status=SOURCE_DIGEST_OK;
    if (fstat(descriptor,&opened)!=0) { status=io_failure(error,"inspect the open",errno); goto done; }
    if (!regular_identity(&opened,next.identity) || lstat(path,&current)!=0 || !regular_identity(&current,next.identity)) {
        status=failure(SOURCE_DIGEST_CHANGED,error,"The source path or descriptor changed before verification."); goto done;
    }
    CC_SHA256_CTX state;
    if (CC_SHA256_Init(&state)!=1) { status=failure(SOURCE_DIGEST_ERROR,error,"The macOS SHA256 verifier could not initialize."); goto done; }
    unsigned char buffer[DX_SOURCE_DIGEST_BUFFER_BYTES];
    for (;;) {
        if (cancelled && cancelled(context,next.bytes)) { status=failure(SOURCE_DIGEST_CANCELLED,error,"Source-content verification was cancelled."); goto done; }
        /* Read at most one byte past the captured size. An append-only writer
           cannot make verification chase an indefinitely growing source. */
        uint64_t remaining=next.identity.size-next.bytes;
        size_t limit=remaining>=sizeof(buffer) ? sizeof(buffer) : (size_t)remaining+1;
        ssize_t count=read(descriptor,buffer,limit);
        if (count<0) { if (errno==EINTR) continue; status=io_failure(error,"read",errno); goto done; }
        if (!count) break;
        if ((uint64_t)count>remaining) { status=failure(SOURCE_DIGEST_CHANGED,error,"The source grew during content verification."); goto done; }
        if (CC_SHA256_Update(&state,buffer,(CC_LONG)count)!=1) { status=failure(SOURCE_DIGEST_ERROR,error,"The macOS SHA256 verifier could not read the source bytes."); goto done; }
        next.bytes+=(uint64_t)count;
    }
    if (next.bytes!=next.identity.size || fstat(descriptor,&after)!=0 || lstat(path,&current)!=0 || !regular_identity(&after,next.identity) || !regular_identity(&current,next.identity)) {
        status=failure(SOURCE_DIGEST_CHANGED,error,"The source path, metadata or byte count changed during content verification."); goto done;
    }
    if (cancelled && cancelled(context,next.bytes)) { status=failure(SOURCE_DIGEST_CANCELLED,error,"Source-content verification was cancelled."); goto done; }
    /* A cancellation callback can observe a latest-request change and tests can
       deterministically replace files there; validate again before publication. */
    if (fstat(descriptor,&after)!=0 || lstat(path,&current)!=0 || !regular_identity(&after,next.identity) || !regular_identity(&current,next.identity)) {
        status=failure(SOURCE_DIGEST_CHANGED,error,"The source changed before verified content could be published."); goto done;
    }
    if (CC_SHA256_Final(next.sha256,&state)!=1) { status=failure(SOURCE_DIGEST_ERROR,error,"The macOS SHA256 verifier could not finish."); goto done; }
    next.observed_stamp=next.identity.size ^ (uint64_t)next.identity.modified_seconds*UINT64_C(1099511628211) ^ next.identity.inode ^ (uint64_t)next.identity.modified_nanoseconds*UINT64_C(1469598103934665603);
done:
    if (close(descriptor)!=0 && status==SOURCE_DIGEST_OK) status=io_failure(error,"close",errno);
    if (status==SOURCE_DIGEST_OK) *out=next;
    return status;
#endif
}
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#pragma clang diagnostic pop
#endif
