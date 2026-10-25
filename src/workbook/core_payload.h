#ifndef DUXCEL_CORE_PAYLOAD_H
#define DUXCEL_CORE_PAYLOAD_H
#include "save.h"
#include "../data/source_digest.h"
typedef struct WorkbookCorePayload WorkbookCorePayload;
typedef enum { CORE_RESTORE_COPY,CORE_RESTORE_VERIFY,CORE_RESTORE_ADMIT,CORE_RESTORE_RECHECK } WorkbookCoreRestorePhase;
typedef struct {
    WorkbookId document;
    uint64_t bytes;
    unsigned encoding;
    unsigned char sha256[DX_SOURCE_DIGEST_BYTES];
} WorkbookCorePayloadInfo;
typedef struct {
    WorkbookSaveControl save;
    SourceDigestCancel cancelled; /* Hash-stage cancellation/progress, bounded 64 KiB reads. */
    void *context;
} WorkbookCorePayloadControl;
typedef enum { CORE_PAYLOAD_READY,CORE_PAYLOAD_FAILED,CORE_PAYLOAD_CANCELLED,CORE_PAYLOAD_UNSUPPORTED } WorkbookCorePayloadResult;
/* Native worker operation. Borrows an immutable snapshot and existing parent
   folder. Produces a new private child, held lease and closed v15 core file;
   never overwrites an existing artifact or publishes user index metadata.
   SHA256 identifies these exact bytes, not semantic equality of two captures.
   External data and history/results absent from the core codec are not retained:
   this leaf is deliberately NOT a complete WorkbookVersionRoot descriptor.
   out must be initialized NULL; failure/cancel preserves it and cleans ordinary
   private artifacts. Unexpected files are kept, never recursively deleted.
   macOS SDK hashing is validated; other hosts fail explicitly unsupported. */
WorkbookCorePayloadResult workbook_core_payload_capture(const Workbook *book,const char *parent,
    WorkbookCorePayloadControl control,WorkbookCorePayload **out,char **error);
typedef struct {
    /* Fill exactly bytes at the next sequential offset. buffer is borrowed for
       this call; no pointer escapes. False aborts restoration, preserving out. */
    bool (*read)(void *context,uint64_t offset,void *buffer,size_t bytes,char **error);
    bool (*cancelled)(void *context,WorkbookCoreRestorePhase phase,uint64_t bytes);
    void *context;
} WorkbookCorePayloadInput;
/* Reconstruct an untrusted encoded leaf under explicit disk reservation. Fixed
   64 KiB input blocks, private lease/closed file, exact hash/length, strict v15
   core/document/absolute-locator/nonrecursive admission, then post-load proof.
   No source query is executed. Input may not mutate the staged file/model or
   reenter payload APIs. Failure/cancel keeps out unchanged and cleans ordinary
   private staging. It does not certify retained assets outside the core codec. */
WorkbookCorePayloadResult workbook_core_payload_restore(const WorkbookCorePayloadInfo *expected,
    const char *parent,uint64_t reserved_bytes,WorkbookCorePayloadInput input,
    WorkbookCorePayload **out,char **error);
/* Borrowed immutable values/paths, valid until retirement/free. No mutable
   database connection escapes capture. Consumers must keep the handle alive,
   use read-only access and verify immediately before consuming the file. The
   lease excludes cooperating owners, not arbitrary external file writers. */
const WorkbookCorePayloadInfo *workbook_core_payload_info(const WorkbookCorePayload *payload);
/* Actual persisted command revision recorded from capture input or the strict
   restored model. No caller-supplied expected descriptor can assign it.
   Failure leaves out unchanged; this scalar alone is not content equality. */
bool workbook_core_payload_revision(const WorkbookCorePayload *payload,uint64_t *out);
const char *workbook_core_payload_path(const WorkbookCorePayload *payload);
const char *workbook_core_payload_directory(const WorkbookCorePayload *payload);
WorkbookCorePayloadResult workbook_core_payload_verify(const WorkbookCorePayload *payload,
    SourceDigestCancel cancelled,void *context,char **error);
/* Explicit disposal reports failed cleanup and preserves changed/unknown files
   before any deletion. Success is idempotent; the handle remains owned until
   free. Free attempts this cleanup, then releases lease/memory even on refusal.
   Call retire first when the caller needs cleanup failure feedback. No cleanup
   path touches an external source or recursively removes a directory. */
bool workbook_core_payload_retire(WorkbookCorePayload *payload,char **error);
void workbook_core_payload_free(WorkbookCorePayload *payload);
#endif
