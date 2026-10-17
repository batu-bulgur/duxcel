#ifndef DUXCEL_RECORD_SQL_H
#define DUXCEL_RECORD_SQL_H
#include "analysis.h"
#define DX_RECORD_SQL_COLUMNS 16384
#define DX_RECORD_SQL_NAME_BYTES 4096
#define DX_RECORD_SQL_INPUT_BYTES 65536
#define DX_RECORD_SQL_BYTES (1024*1024)
typedef enum { RECORD_SQL_READY,RECORD_SQL_UNSUPPORTED,RECORD_SQL_INVALID } RecordSQLStatus;
typedef enum { RECORD_ID_WINDOW,RECORD_ID_PARQUET } RecordIDKind;
typedef struct {
    char *relation,*identity_name;
    size_t business_columns,identity_column;
    RecordIDKind kind;
} RecordSQL;
/* Optional session provenance only, never a persisted source-version/key policy.
   names is the caller-validated original business schema in output order, with
   no reader metadata. File callers attest to one regular file; no filesystem
   work occurs here. Native execution must preserve insertion order and resolve
   exact numeric filter types first, as for normal Analysis execution.
   ID is the final UBIGINT column (1-based raw ordinal), not a business field.
   Requested sorts gain an ordinal tie-breaker; unsorted sources gain no sort.
   The optional input budget counts string bytes including NUL terminators;
   exceeding a text/SQL budget is UNSUPPORTED, never a business-query failure.
   parameters=true preserves the existing $__duxcel_filter binding contract.
   out is zero-initialized or owned. All failures preserve it; UNSUPPORTED means
   real business SQL may still load without this optional projection. INVALID
   denotes malformed bounded input. Owned strings are independent of inputs. */
RecordSQLStatus record_sql_build(RecordSQL *out,const Analysis *source,
    const char *const *names,size_t count,bool single_regular_file,bool parameters,char **error);
void record_sql_free(RecordSQL *query);
#endif
