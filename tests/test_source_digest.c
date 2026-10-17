#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "data/source_digest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL source digest:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
static void identity_comparison(void) {
    SourceFileIdentity a,b; memset(&a,0xa5,sizeof(a)); memset(&b,0x5a,sizeof(b));
#define IDENTITY_FIELDS(X) X(device) X(inode) X(size) X(links) X(mode) X(uid) X(gid) X(flags) X(generation) X(modified_seconds) X(modified_nanoseconds) X(changed_seconds) X(changed_nanoseconds) X(created_seconds) X(created_nanoseconds)
#define ZERO_FIELD(field) a.field=b.field=0;
    IDENTITY_FIELDS(ZERO_FIELD)
#undef ZERO_FIELD
    CHECK(source_file_identity_same(&a,&b)); /* Deliberately different padding. */
#define CHANGE_FIELD(field) do { SourceFileIdentity changed=a; changed.field++; CHECK(!source_file_identity_same(&a,&changed)); CHECK(!source_file_identity_same(&changed,&a)); } while (0);
    IDENTITY_FIELDS(CHANGE_FIELD)
#undef CHANGE_FIELD
#undef IDENTITY_FIELDS
    CHECK(!source_file_identity_same(NULL,&a) && !source_file_identity_same(&a,NULL) && !source_file_identity_same(NULL,NULL));
}
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
static void write_bytes(const char *path,const void *bytes,size_t length) {
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600); CHECK(fd>=0);
    for (size_t offset=0;offset<length;) { ssize_t n=write(fd,(const unsigned char *)bytes+offset,length-offset); if (n<0 && errno==EINTR) continue; CHECK(n>0); offset+=(size_t)n; }
    CHECK(close(fd)==0);
}
static void write_large(const char *path) {
    FILE *file=fopen(path,"wb"); CHECK(file); unsigned char block[4096]; memset(block,'a',sizeof(block));
    for (size_t offset=0;offset<1000000;) { size_t n=1000000-offset; if (n>sizeof(block)) n=sizeof(block); CHECK(fwrite(block,1,n,file)==n); offset+=n; }
    CHECK(fclose(file)==0);
}
static void hex_digest(char out[65],const unsigned char bytes[DX_SOURCE_DIGEST_BYTES]) {
    static const char hex[]="0123456789abcdef";
    for (size_t i=0;i<DX_SOURCE_DIGEST_BYTES;i++) { out[i*2]=hex[bytes[i]>>4]; out[i*2+1]=hex[bytes[i]&15]; } out[64]=0;
}
static SourceDigest verify(const char *path,const char *expected,uint64_t size) {
    char error[DX_SOURCE_DIGEST_ERROR_BYTES],hex[65]; memset(error,0xa5,sizeof(error)); SourceDigest result;
    CHECK(source_digest_file(path,NULL,NULL,&result,error)==SOURCE_DIGEST_OK && !error[0]); hex_digest(hex,result.sha256); CHECK(!strcmp(hex,expected)); CHECK(result.bytes==size && result.identity.size==size);
    struct stat file; CHECK(lstat(path,&file)==0);
    CHECK(result.identity.device==(uint64_t)file.st_dev && result.identity.inode==(uint64_t)file.st_ino && result.identity.links==(uint64_t)file.st_nlink);
    CHECK(result.identity.mode==(uint32_t)file.st_mode && result.identity.uid==file.st_uid && result.identity.gid==file.st_gid && result.identity.flags==file.st_flags && result.identity.generation==file.st_gen);
    CHECK(result.identity.modified_seconds==file.st_mtimespec.tv_sec && result.identity.modified_nanoseconds==(uint32_t)file.st_mtimespec.tv_nsec && result.identity.changed_seconds==file.st_ctimespec.tv_sec && result.identity.changed_nanoseconds==(uint32_t)file.st_ctimespec.tv_nsec);
    CHECK(result.identity.created_seconds==file.st_birthtimespec.tv_sec && result.identity.created_nanoseconds==(uint32_t)file.st_birthtimespec.tv_nsec);
    SourceFileIdentity captured; CHECK(source_file_identity(path,&captured,error)==SOURCE_DIGEST_OK && !error[0]); CHECK(source_file_identity_same(&captured,&result.identity));
    uint64_t stamp=(uint64_t)file.st_size ^ (uint64_t)file.st_mtime*UINT64_C(1099511628211) ^ (uint64_t)file.st_ino ^ (uint64_t)file.st_mtimespec.tv_nsec*UINT64_C(1469598103934665603);
    CHECK(result.observed_stamp==stamp); return result;
}
static void rejected(const char *path,SourceDigestCancel cancel,void *context,SourceDigestStatus expected) {
    SourceDigest result,before; memset(&result,0xa5,sizeof(result)); memcpy(&before,&result,sizeof(before));
    struct { char prefix; char error[DX_SOURCE_DIGEST_ERROR_BYTES]; char suffix; } diagnostic;
    memset(&diagnostic,0xa5,sizeof(diagnostic));
    CHECK(source_digest_file(path,cancel,context,&result,diagnostic.error)==expected);
    CHECK(!memcmp(&result,&before,sizeof(result)) && diagnostic.prefix==(char)0xa5 && diagnostic.suffix==(char)0xa5);
    CHECK(diagnostic.error[0] && memchr(diagnostic.error,0,sizeof(diagnostic.error)));
}
static void identity_rejected(const char *path) {
    SourceFileIdentity result,before; memset(&result,0xa5,sizeof(result)); memcpy(&before,&result,sizeof(before));
    struct { char prefix,error[DX_SOURCE_DIGEST_ERROR_BYTES],suffix; } diagnostic; memset(&diagnostic,0xa5,sizeof(diagnostic));
    CHECK(source_file_identity(path,&result,diagnostic.error)==SOURCE_DIGEST_ERROR);
    CHECK(!memcmp(&result,&before,sizeof(result)) && diagnostic.prefix==(char)0xa5 && diagnostic.suffix==(char)0xa5);
    CHECK(*diagnostic.error && memchr(diagnostic.error,0,sizeof(diagnostic.error)));
}
static void basic_tests(const char *directory) {
    char path[1024],alias[1024],nested[1200],missing[1024];
    snprintf(path,sizeof(path),"%s/source.bin",directory); snprintf(alias,sizeof(alias),"%s/alias",directory); snprintf(missing,sizeof(missing),"%s/missing",directory);
    static const unsigned char binary[]={0,1,0,255,10,0,'N','U','L','L'};
    write_bytes(path,"",0); verify(path,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",0);
    write_bytes(path,"abc",3); verify(path,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",3);
    write_bytes(path,binary,sizeof(binary)); verify(path,"4a6bdd7400b1756ac981153ad2ce6187101c316c2a5b7d4f87fef953e5237aac",sizeof(binary));
    FILE *unchanged=fopen(path,"rb"); unsigned char readback[sizeof(binary)+1]; CHECK(unchanged && fread(readback,1,sizeof(readback),unchanged)==sizeof(binary) && !memcmp(readback,binary,sizeof(binary)) && fclose(unchanged)==0);
    SourceDigest retained=verify(path,"4a6bdd7400b1756ac981153ad2ce6187101c316c2a5b7d4f87fef953e5237aac",sizeof(binary));
    SourceDigest second; CHECK(source_digest_file(path,NULL,NULL,&second,NULL)==SOURCE_DIGEST_OK && !memcmp(retained.sha256,second.sha256,DX_SOURCE_DIGEST_BYTES));
    if (geteuid()!=0) {
        CHECK(chmod(path,0000)==0); rejected(path,NULL,NULL,SOURCE_DIGEST_ERROR);
        SourceFileIdentity unreadable; CHECK(source_file_identity(path,&unreadable,NULL)==SOURCE_DIGEST_OK && !(unreadable.mode&0777)); /* Stat only; no content permission required. */
        CHECK(chmod(path,0600)==0); verify(path,"4a6bdd7400b1756ac981153ad2ce6187101c316c2a5b7d4f87fef953e5237aac",sizeof(binary));
    }
    rejected(NULL,NULL,NULL,SOURCE_DIGEST_ERROR); rejected("",NULL,NULL,SOURCE_DIGEST_ERROR); rejected(missing,NULL,NULL,SOURCE_DIGEST_ERROR); rejected(directory,NULL,NULL,SOURCE_DIGEST_ERROR);
    identity_rejected(NULL); identity_rejected(""); identity_rejected(missing); identity_rejected(directory);
    char error[DX_SOURCE_DIGEST_ERROR_BYTES]; CHECK(source_digest_file(path,NULL,NULL,NULL,error)==SOURCE_DIGEST_ERROR && *error);
    CHECK(source_file_identity(path,NULL,error)==SOURCE_DIGEST_ERROR && *error);
    CHECK(symlink(path,alias)==0); rejected(alias,NULL,NULL,SOURCE_DIGEST_ERROR); identity_rejected(alias); CHECK(unlink(alias)==0);
    CHECK(symlink(missing,alias)==0); rejected(alias,NULL,NULL,SOURCE_DIGEST_ERROR); identity_rejected(alias); CHECK(unlink(alias)==0);
    CHECK(mkfifo(alias,0600)==0); rejected(alias,NULL,NULL,SOURCE_DIGEST_ERROR); identity_rejected(alias); CHECK(unlink(alias)==0);
    CHECK(link(path,alias)==0); SourceDigest linked=verify(alias,"4a6bdd7400b1756ac981153ad2ce6187101c316c2a5b7d4f87fef953e5237aac",sizeof(binary)); CHECK(linked.identity.inode==retained.identity.inode && linked.identity.links==2); CHECK(unlink(alias)==0);
    CHECK(symlink(directory,alias)==0); snprintf(nested,sizeof(nested),"%s/source.bin",alias); verify(nested,"4a6bdd7400b1756ac981153ad2ce6187101c316c2a5b7d4f87fef953e5237aac",sizeof(binary)); CHECK(unlink(alias)==0);
    CHECK(unlink(path)==0); CHECK(retained.bytes==sizeof(binary) && retained.sha256[0]==0x4a); /* No result borrows a path or FD. */
}
typedef struct { unsigned calls; uint64_t last,threshold; } Cancellation;
static bool cancel_at(void *context,uint64_t bytes) { Cancellation *cancel=context; cancel->calls++; cancel->last=bytes; return bytes>=cancel->threshold; }
typedef enum { MUTATE_TRUNCATE,MUTATE_APPEND,MUTATE_SAME_SIZE,MUTATE_REPLACE,MUTATE_SYMLINK,MUTATE_UNLINK,MUTATE_MODE,MUTATE_FINAL } MutationKind;
typedef struct { const char *path,*replacement; MutationKind kind; bool done; unsigned finals; uint64_t trigger; } Mutation;
static bool mutate_between_reads(void *context,uint64_t bytes) {
    Mutation *mutation=context;
    if (mutation->done || bytes<mutation->trigger) return false;
    if (mutation->kind==MUTATE_FINAL && ++mutation->finals<2) return false;
    mutation->done=true;
    if (mutation->kind==MUTATE_REPLACE || mutation->kind==MUTATE_FINAL) CHECK(rename(mutation->replacement,mutation->path)==0);
    else if (mutation->kind==MUTATE_SYMLINK) { CHECK(unlink(mutation->path)==0); CHECK(symlink(mutation->replacement,mutation->path)==0); }
    else if (mutation->kind==MUTATE_UNLINK) CHECK(unlink(mutation->path)==0);
    else if (mutation->kind==MUTATE_MODE) CHECK(chmod(mutation->path,0700)==0);
    else {
        int flags=O_WRONLY|(mutation->kind==MUTATE_TRUNCATE ? O_TRUNC : mutation->kind==MUTATE_APPEND ? O_APPEND : 0);
        int fd=open(mutation->path,flags); CHECK(fd>=0);
        if (mutation->kind!=MUTATE_TRUNCATE) CHECK(write(fd,"z",1)==1);
        if (mutation->kind==MUTATE_SAME_SIZE) {
            struct stat file; CHECK(fstat(fd,&file)==0); struct timespec times[]={file.st_atimespec,file.st_mtimespec}; times[1].tv_sec+=1;
            CHECK(futimens(fd,times)==0); /* Deterministic metadata mismatch, no sleeps/timestamp race. */
        }
        CHECK(close(fd)==0);
    }
    return false;
}
static void mutation_tests(const char *directory) {
    char path[1024],replacement[1024]; snprintf(path,sizeof(path),"%s/large.bin",directory); snprintf(replacement,sizeof(replacement),"%s/replacement.bin",directory);
    static const char million[]="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    write_large(path); verify(path,million,1000000);
    Cancellation cancellation={.threshold=0}; rejected(path,cancel_at,&cancellation,SOURCE_DIGEST_CANCELLED); CHECK(cancellation.calls==1 && !cancellation.last);
    cancellation=(Cancellation){.threshold=DX_SOURCE_DIGEST_BUFFER_BYTES}; rejected(path,cancel_at,&cancellation,SOURCE_DIGEST_CANCELLED); CHECK(cancellation.calls==2 && cancellation.last==DX_SOURCE_DIGEST_BUFFER_BYTES);
    verify(path,million,1000000); /* Cancellation never writes or replaces the source. */
    for (MutationKind kind=MUTATE_TRUNCATE;kind<=MUTATE_FINAL;kind++) {
        write_large(path); write_large(replacement);
        struct stat before; CHECK(lstat(path,&before)==0);
        Mutation mutation={.path=path,.replacement=replacement,.kind=kind,.trigger=kind==MUTATE_FINAL ? 1000000 : DX_SOURCE_DIGEST_BUFFER_BYTES};
        rejected(path,mutate_between_reads,&mutation,SOURCE_DIGEST_CHANGED); CHECK(mutation.done);
        if (kind==MUTATE_REPLACE || kind==MUTATE_FINAL) { struct stat after; CHECK(lstat(path,&after)==0 && after.st_ino!=before.st_ino); verify(path,million,1000000); }
        if (kind!=MUTATE_UNLINK) CHECK(unlink(path)==0);
        if (kind!=MUTATE_REPLACE && kind!=MUTATE_FINAL) CHECK(unlink(replacement)==0);
    }
}
int main(void) {
    identity_comparison();
    char directory[]="/tmp/duxcel-source-digest-XXXXXX"; CHECK(mkdtemp(directory)!=NULL);
    basic_tests(directory); mutation_tests(directory); CHECK(rmdir(directory)==0);
    printf("PASS source-digest: %u checks\n",checks); return 0;
}
#else
int main(void) {
    identity_comparison();
    SourceDigest result,before; memset(&result,0xa5,sizeof(result)); memcpy(&before,&result,sizeof(result)); char error[DX_SOURCE_DIGEST_ERROR_BYTES];
    CHECK(source_digest_file("must-not-be-opened",NULL,NULL,&result,error)==SOURCE_DIGEST_UNSUPPORTED);
    CHECK(*error && !memcmp(&result,&before,sizeof(result)));
    SourceFileIdentity captured,prior; memset(&captured,0xa5,sizeof(captured)); memcpy(&prior,&captured,sizeof(prior));
    CHECK(source_file_identity("must-not-be-opened",&captured,error)==SOURCE_DIGEST_UNSUPPORTED);
    CHECK(*error && !memcmp(&captured,&prior,sizeof(captured)));
    printf("PASS source-digest unsupported: %u checks\n",checks); return 0;
}
#endif
