#ifndef DUXCEL_TEST_LEGACY_WORKBOOK_H
#define DUXCEL_TEST_LEGACY_WORKBOOK_H
#include "workbook/workbook_codec.h"
#include <duckdb.h>
#include <stdio.h>
/* Historical component fixtures deliberately retain v12/v13/v14 semantics.
   Save with the real current writer first, then remove only the known newer
   metadata from these bounded test checkpoints. Never used by product builds.
   This creates no synthetic source data and is not another workbook serializer. */
static bool test_legacy_downgrade(const char *path,unsigned version) {
    if (version<1 || version>14) return false;
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0}; bool ok=false;
    if (duckdb_open(path,&db)!=DuckDBSuccess || duckdb_connect(db,&c)!=DuckDBSuccess) goto done;
    char sql[1200]; snprintf(sql,sizeof(sql),
        "BEGIN; DROP TABLE _duxcel_workspace.workbook_identity; DROP TABLE _duxcel_workspace.sheet_identity; UPDATE _duxcel_workspace.meta SET version=%u; %s%s COMMIT",version,
        version<14 ? "DROP TABLE _duxcel_workspace.format_state; DROP TABLE _duxcel_workspace.source_version_counters; DROP TABLE _duxcel_workspace.source_versions; DROP TABLE _duxcel_workspace.source_bindings; DROP TABLE _duxcel_workspace.format_records; DROP TABLE _duxcel_workspace.format_fields;" : "",
        version<13 ? "DROP TABLE _duxcel_workspace.object_counters; DROP TABLE _duxcel_workspace.format_scopes; DROP TABLE _duxcel_workspace.format_selectors; DROP TABLE _duxcel_workspace.format_guards;" : "");
    ok=duckdb_query(c,sql,&r)==DuckDBSuccess;
 done:
    duckdb_destroy_result(&r); if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db); return ok;
}
static bool test_legacy_save(const Workbook *book,const char *path,char **error) {
    unsigned version=workbook_codec_content_version(book);
    /* Preserve the old suite's no-downgrade assertion for an existing archive. */
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0}; duckdb_config config=NULL;
    FILE *file=fopen(path,"rb");
    if (file) {
        fclose(file);
        if (duckdb_create_config(&config)==DuckDBSuccess && duckdb_set_config(config,"access_mode","READ_ONLY")==DuckDBSuccess &&
            duckdb_open_ext(path,&db,config,NULL)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess &&
            duckdb_query(c,"SELECT version FROM _duxcel_workspace.meta",&r)==DuckDBSuccess && duckdb_row_count(&r)==1) {
            int old=duckdb_value_int32(&r,0,0); if (old>=12 && old<=14 && (unsigned)old>version) version=(unsigned)old;
        }
        duckdb_destroy_result(&r); if (c) duckdb_disconnect(&c); if (db) duckdb_close(&db); if (config) duckdb_destroy_config(&config);
    }
    if (!workbook_save(book,path,error)) return false;
    return test_legacy_downgrade(path,version);
}
#endif
