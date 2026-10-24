#ifndef DUXCEL_VERSION_SQL_H
#define DUXCEL_VERSION_SQL_H
#include <duckdb.h>
#include <stdbool.h>
/* Native internal transaction proof. The caller exclusively owns the borrowed
   connection. Two stable uint64 SQL transaction IDs reject autocommit before
   metadata IO; no BEGIN/COMMIT/ROLLBACK or connection lifetime escapes here. */
bool workbook_version_sql_transaction(duckdb_connection connection,char **error);
#endif
