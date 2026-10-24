#ifndef DUXCEL_ARCHIVE_READ_H
#define DUXCEL_ARCHIVE_READ_H
#include "version_restore.h"
#define DX_VERSIONED_WORKSPACE_VERSION 16u
typedef struct WorkbookArchiveCandidate WorkbookArchiveCandidate;
typedef enum { ARCHIVE_READ_HEADER,ARCHIVE_READ_HEAD,ARCHIVE_READ_INDEX,ARCHIVE_READ_COPY,ARCHIVE_READ_VERIFY,ARCHIVE_READ_ADMIT,ARCHIVE_READ_RECHECK } WorkbookArchiveReadPhase;
typedef struct {
    /* Worker callback; true cancels before publication. Never mutate input,
       reenter persistence or retain borrowed helper data. */
    bool (*cancelled)(void *context,WorkbookArchiveReadPhase phase,size_t root,uint64_t bytes);
    void *context;
} WorkbookArchiveReadControl;
typedef enum { ARCHIVE_READ_READY,ARCHIVE_READ_FAILED,ARCHIVE_READ_CANCELLED,ARCHIVE_READ_UNSUPPORTED } WorkbookArchiveReadResult;
/* Native staged reader for the exact current container (v16), whose retained
   roots use the exact current core payload (v15). Older external formats are
   unsupported during prerelease development; no migration runs on Open.
   The file is read-only. A dependent WAL rejects. Retained document identities
   must match the head, and native source identity is rechecked after all reads.
   Private roots own independent closed bytes.
   Existing staging parent/reservation and NULL out are required. Caller adopts
   only a READY candidate; failure/cancel preserves out and ordinary staging.
   macOS identity is validated; other hosts explicitly return UNSUPPORTED. This
   API does not activate ordinary App Open or the v16 writer by itself. */
WorkbookArchiveReadResult workbook_archive_read(const char *path,const char *parent,uint64_t reserved_bytes,
    WorkbookArchiveReadControl control,WorkbookArchiveCandidate **out,char **error);
const Workbook *workbook_archive_book(const WorkbookArchiveCandidate *candidate);
const WorkbookVersionCoreSet *workbook_archive_versions(const WorkbookArchiveCandidate *candidate);
unsigned workbook_archive_file_version(const WorkbookArchiveCandidate *candidate);
const SourceFileIdentity *workbook_archive_identity(const WorkbookArchiveCandidate *candidate);
Workbook *workbook_archive_take_book(WorkbookArchiveCandidate *candidate);
WorkbookVersionCoreSet *workbook_archive_take_versions(WorkbookArchiveCandidate *candidate);
void workbook_archive_candidate_free(WorkbookArchiveCandidate *candidate);
#endif
