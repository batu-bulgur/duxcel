#ifndef DUXCEL_DIRECTORY_H
#define DUXCEL_DIRECTORY_H
#include <stdbool.h>
typedef struct DxDirectory DxDirectory;
typedef enum { DX_PATH_MISSING,DX_PATH_FILE,DX_PATH_DIRECTORY,DX_PATH_SYMLINK,DX_PATH_OTHER,DX_PATH_UNAVAILABLE } DxPathKind;
typedef enum { DX_DIRECTORY_OPEN,DX_DIRECTORY_MISSING,DX_DIRECTORY_ERROR } DxDirectoryOpen;
typedef enum { DX_DIRECTORY_ENTRY,DX_DIRECTORY_END,DX_DIRECTORY_FAILED } DxDirectoryNext;
typedef struct { const char *name; DxPathKind kind; } DxDirectoryEntry;
/* Native no-follow metadata and one-owner streaming directory cursor. No
   recursion or unbounded filename inventory. Entry name borrows cursor storage
   until next/close. Missing/removed entries are distinct from IO failures. */
DxPathKind dx_path_kind(const char *path);
bool dx_path_independent_file(const char *path); /* Regular, no final symlink, exactly one link. */
DxDirectoryOpen dx_directory_open(const char *path,DxDirectory **out,char **error);
DxDirectoryNext dx_directory_next(DxDirectory *directory,DxDirectoryEntry *out,char **error);
void dx_directory_close(DxDirectory *directory);
#endif
