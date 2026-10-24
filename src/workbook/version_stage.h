#ifndef DUXCEL_VERSION_STAGE_H
#define DUXCEL_VERSION_STAGE_H
#include "version_index.h"
#include "core_payload.h"
#include <duckdb.h>
typedef enum { VERSION_STAGE_VERIFY,VERSION_STAGE_CHUNKS,VERSION_STAGE_RECHECK } WorkbookVersionStagePhase;
typedef struct {
    bool (*cancelled)(void *context,WorkbookVersionStagePhase phase,size_t root,uint64_t bytes);
    void *context;
} WorkbookVersionStageControl;
typedef enum { VERSION_STAGE_READY,VERSION_STAGE_FAILED,VERSION_STAGE_CANCELLED } WorkbookVersionStageResult;
/* Native codec, NOT a file Save/publication operation. Borrows an exclusively
   owned connection already in a caller-owned explicit transaction, and immutable
   index/leaves for the entire call. Rejects autocommit and an existing reserved
   _duxcel_versions schema before mutation. The transaction owner MUST ROLLBACK
   on any non-READY result, and must validate the whole outer archive before
   COMMIT. This helper never commits/rolls back/closes the borrowed connection.
   Leaves correspond exactly to index.roots order, identity/hash/size/encoding;
   their aggregate bytes must fit the caller's explicit storage reservation.
   Writes fresh codec-v1 metadata and exact64 KiB BLOB chunks, verifying each
   closed leaf before and after consumption. No external source queries, user
   pointer publication or standalone .dxls format compatibility is claimed. */
WorkbookVersionStageResult workbook_version_stage(duckdb_connection connection,
    const WorkbookVersionIndex *index,const WorkbookCorePayload *const *leaves,size_t count,
    uint64_t reserved_bytes,WorkbookVersionStageControl control,char **error);
#endif
