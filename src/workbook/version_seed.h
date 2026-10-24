#ifndef DUXCEL_VERSION_SEED_H
#define DUXCEL_VERSION_SEED_H
#include "archive_read.h"
typedef struct { WorkbookCorePayloadControl capture; WorkbookVersionRestoreControl admit; } WorkbookVersionSeedControl;
/* Native worker-only runtime baseline for an already admitted, unchanged
   v1-v15 saved file. Captures its opened head as Saved R before App edits can
   begin, so the first private R+1 checkpoint can retain R without rewriting the
   original legacy file. This is migration of known saved state, not a new user
   Save/tag. Candidate/model stay untouched; NULL out is consumed only on READY.
   Reject detached/dirty/versioned candidates and observed source changes. */
WorkbookSaveResult workbook_version_seed_legacy(const WorkbookArchiveCandidate *candidate,const char *parent,
    uint64_t reserved_bytes,WorkbookVersionSeedControl control,WorkbookVersionCoreSet **out,char **error);
#endif
