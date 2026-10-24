#ifndef DUXCEL_VERSION_READ_H
#define DUXCEL_VERSION_READ_H
#include "version_index.h"
#include <duckdb.h>
typedef struct WorkbookVersionMetadata WorkbookVersionMetadata;
typedef struct { bool (*cancelled)(void *context); void *context; } WorkbookVersionReadControl;
typedef enum { VERSION_METADATA_READY,VERSION_METADATA_FAILED,VERSION_METADATA_CANCELLED } WorkbookVersionReadResult;
/* Native bounded metadata admission under an exclusively borrowed connection
   and caller-owned explicit transaction. Never starts/commits/rolls back or
   writes SQL. Strict codec-v1 shape/types/NULLs, positions, opaque keys, UTF-8
   labels, references, generations and aggregate reserved bytes are checked.
   Uses bounded preflight observations before returning any label/key arrays.
   out must be initialized NULL and remains unchanged on every failure/cancel.
   READY certifies INDEX METADATA ONLY: chunks, hashes, embedded core admission
   and complete asset closure MUST be verified separately before publication. */
WorkbookVersionReadResult workbook_version_metadata_read(duckdb_connection connection,uint64_t reserved_bytes,
    WorkbookVersionReadControl control,WorkbookVersionMetadata **out,char **error);
/* Copy a bounded pure index into independently owned metadata, without SQL or
   asset admission. Label spans need not be NUL-terminated. out must be NULL;
   every failure preserves it. Used for an opened legacy Saved baseline. */
bool workbook_version_metadata_copy(const WorkbookVersionIndex *index,uint64_t reserved_bytes,
    WorkbookVersionMetadata **out,char **error);
/* The returned index borrows immutable handle-owned arrays/labels until free. */
const WorkbookVersionIndex *workbook_version_metadata_index(const WorkbookVersionMetadata *metadata);
void workbook_version_metadata_free(WorkbookVersionMetadata *metadata);
#endif
