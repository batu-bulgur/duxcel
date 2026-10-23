#ifndef DUXCEL_FILE_LEASE_H
#define DUXCEL_FILE_LEASE_H
#include <stdbool.h>
typedef struct DxFileLease DxFileLease;
typedef enum { DX_FILE_LEASE_ACQUIRED,DX_FILE_LEASE_BUSY,DX_FILE_LEASE_ERROR } DxFileLeaseStatus;
/* Native process ownership for a fixed private zero-byte lease file. Nonblocking;
   busy is not an error. create=false never creates missing files. The initialized
   *out is unchanged on failure; caller owns an acquired lease until release.
   Never truncate/unlink on acquisition/release: replacing the inode could let
   another process acquire a different lock for the same generation. */
DxFileLeaseStatus dx_file_lease_acquire(const char *path,bool create,DxFileLease **out,char **error);
void dx_file_lease_release(DxFileLease *lease);
#endif
