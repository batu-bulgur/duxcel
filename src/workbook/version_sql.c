#include "version_sql.h"
#include "../common.h"
#include <stdlib.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message ? message : "Cannot verify the version transaction owner."); } return false; }
static bool transaction_id(duckdb_connection c,uint64_t *out,char **error) {
    duckdb_result r={0}; bool ok=duckdb_query(c,"SELECT current_transaction_id()",&r)==DuckDBSuccess;
    if (!ok) fail(error,duckdb_result_error(&r));
    else if (duckdb_row_count(&r)!=1 || duckdb_column_count(&r)!=1 || duckdb_column_type(&r,0)!=DUCKDB_TYPE_UBIGINT || duckdb_value_is_null(&r,0,0)) ok=fail(error,"Cannot verify the version transaction owner.");
    if (ok) *out=duckdb_value_uint64(&r,0,0); duckdb_destroy_result(&r); return ok;
}
bool workbook_version_sql_transaction(duckdb_connection c,char **error) {
    uint64_t a=0,b=0;
    if (!c || !transaction_id(c,&a,error) || !transaction_id(c,&b,error)) return false;
    return a==b || fail(error,"Version codec IO requires a caller-owned explicit transaction; autocommit was rejected.");
}
