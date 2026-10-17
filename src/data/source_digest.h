#ifndef DUXCEL_SOURCE_DIGEST_H
#define DUXCEL_SOURCE_DIGEST_H
#include <stdbool.h>
#include <stdint.h>
#define DX_SOURCE_DIGEST_BYTES 32u
#define DX_SOURCE_DIGEST_BUFFER_BYTES 65536u
#define DX_SOURCE_DIGEST_ERROR_BYTES 512u
/* Exact macOS file identity/metadata. Access time is deliberately excluded:
   reading can change it. All fields are scalar and own no external storage. */
typedef struct {
    uint64_t device,inode,size,links;
    uint32_t mode,uid,gid,flags,generation;
    int64_t modified_seconds,changed_seconds,created_seconds;
    uint32_t modified_nanoseconds,changed_nanoseconds,created_nanoseconds;
} SourceFileIdentity;
typedef struct {
    SourceFileIdentity identity;
    uint64_t bytes,observed_stamp; /* Stamp matches dx_file_stamp; it is NOT a digest. */
    unsigned char sha256[DX_SOURCE_DIGEST_BYTES];
} SourceDigest;
typedef enum {
    SOURCE_DIGEST_OK,SOURCE_DIGEST_CANCELLED,SOURCE_DIGEST_CHANGED,
    SOURCE_DIGEST_ERROR,SOURCE_DIGEST_UNSUPPORTED
} SourceDigestStatus;
/* Called after open/identity checks, before the first read and between reads,
   including before EOF/final publication. Return true to cancel. Never retain
   helper-owned state; bytes_read is progress, not a successful result. */
typedef bool (*SourceDigestCancel)(void *context,uint64_t bytes_read);
/* Synchronous, intended for an existing worker thread, never a paint callback.
   One fixed 64 KiB buffer and SDK SHA256 state; no helper-owned heap allocation
   or FD escapes. out is preserved on every failure. Optional error points to at least
   DX_SOURCE_DIGEST_ERROR_BYTES caller-owned bytes; it is always NUL terminated.
   Reject final-component symlinks and nonregular files without blocking on a
   FIFO/device; parent symlinks are allowed, with resolved identity rechecked.
   macOS checks path and open descriptor metadata before/after all reads and
   requires the exact initial byte count. This detects observed changes, NOT a
   transactional snapshot or hostile mutate-and-restore attacks. The caller must
   separately bind this result to its source/reader/schema/request lifetime;
   hashing alone neither activates styles nor verifies an earlier displayed Page.
   Other platforms (including Wasm) explicitly return UNSUPPORTED without I/O. */
SourceDigestStatus source_digest_file(const char *path,SourceDigestCancel cancelled,void *context,
    SourceDigest *out,char error[DX_SOURCE_DIGEST_ERROR_BYTES]);
#endif
