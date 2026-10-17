#ifndef DUXCEL_REVIEW_STORE_H
#define DUXCEL_REVIEW_STORE_H
#include <stdbool.h>
#include <stddef.h>

#define DX_REVIEW_STORE_ID_MAX 95
#define DX_REVIEW_STORE_JSON_MAX (4u*1024u*1024u)
#define DX_REVIEW_STORE_PNG_MAX (128u*1024u*1024u)

/* Native adapter only; no OS handles enter the review protocol. IDs are
   nonempty ASCII alphanumeric, underscore or hyphen, at most 95 bytes.
   The override is an absolute project outbox; otherwise the platform-selected
   private application-data root is suffixed by project_id. No I/O is performed
   by default_root. Output/error buffers remain caller-owned. */
bool dx_review_store_default_root(const char *project_id,char *output,size_t capacity,char *error,size_t error_capacity);

/* Atomically publish root/report_id/{manifest.json,frame.png}. The caller owns
   immutable schema-validated JSON and encoded PNG until this call returns.
   Never replace an existing report, including an empty directory. A failure
   removes only this call's private staging files; completed reports are kept.
   POSIX success flushes both files, the staging directory and its parent;
   the Windows seam uses flushed files and a write-through no-replace move.
   macOS is the currently validated adapter. */
bool dx_review_store_write(const char *root,const char *report_id,const char *manifest_json,
                          const unsigned char *png,size_t png_length,char *error,size_t error_capacity);
#endif
