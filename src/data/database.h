#ifndef DUXCEL_DATABASE_H
#define DUXCEL_DATABASE_H
#include "page.h"
typedef struct {
    duckdb_database db;
    duckdb_connection connection;
    char *attached_path;
    char *temp_directory; /* owned private spill directory, independent of cwd */
    char *query_validation_key;
    duckdb_prepared_statement schema_statement[2];
    char *schema_key[2];
    duckdb_prepared_statement page_statement;
    char *page_key;
    char *page_record_alias,*page_record_scope;
    size_t page_business_columns;
    uint64_t page_record_stamp;
    char *pivot_key;
    char *pivot_sql;
    Column *pivot_headers;
    size_t pivot_columns;
    bool pivot_configured;
    bool pivot_totals_valid;
    unsigned char pivot_other_keys; /* UNKNOWN/SAFE/UNSAFE, tied to pivot_key. */
    Relation *relations;
    size_t relation_count;
    bool (*cancelled)(void *context);
    void *cancel_context;
    uint64_t query_count, count_queries,page_prepares,relation_prepares;
} Database;
bool database_open(Database *db, char **error);
bool database_refresh(Database *db,char **error); /* worker only; invalidate plans/catalog */
bool database_validate_relation(Database *db,Analysis *analysis,char **error); /* parse/bind only; no data/count */
void database_close(Database *db);
/* Source stamps reject observed in-session file changes, not cross-session identity. */
bool database_source_matches(const Analysis *source,uint64_t expected_source_stamp,char **error);
/* One native raw CSV/Parquet result interval, under its captured exact file
   identity and mandatory nonzero stamp. One bounded identity-only execution
   collects at most 20,000 IDs (+one overflow sentinel), with no count/business
   Page or repeated OFFSET execution. Cancellation is checked between <=256-ID
   conversion batches. Unsupported identity/projection never falls back to
   positional data. out is zero-initialized or owned: failure preserves it,
   success replaces it. Analysis may acquire resolved numeric filter bindings.
   Request/owner/version/latest-publication routing remains caller-owned. */
bool database_collect_records(Database *db,Analysis *source,uint64_t first,uint64_t rows,
    const SourceFileIdentity *expected_identity,uint64_t expected_stamp,RecordSelection *out,char **error);
bool database_copy_bound(Database *db,Analysis *source,uint64_t first,uint64_t rows,size_t column,size_t columns,uint64_t expected_source_stamp,char **tsv,char **error);
/* Bound Page output must be zero-initialized or own a valid Page. Failure
   preserves it; success replaces its owned contents. */
bool database_copy_page_bound(Database *db,Analysis *source,const PivotDefinition *pivot,uint64_t first,size_t rows,size_t column,size_t columns,uint64_t expected_source_stamp,Page *page,char **error);
bool database_statistics_bound(Database *db,Analysis *source,const PivotDefinition *pivot,uint64_t first,uint64_t rows,size_t column,size_t columns,uint64_t expected_source_stamp,SelectionStats *stats,char **error);
bool database_export_parquet_bound(Database *db,Analysis *source,const char *path,uint64_t expected_source_stamp,uint64_t *rows,char **error);
bool database_export_pivot_parquet_bound(Database *db,Analysis *source,const PivotDefinition *pivot,const char *path,uint64_t expected_source_stamp,uint64_t *rows,char **error);
bool database_fetch(Database *db, Analysis *a, uint64_t offset, int64_t known_total, Page *page, char **error);
bool database_fetch_window(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_fetch_table(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_fetch_table_window(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_count(Database *db,Analysis *analysis,uint64_t *total,char **error);
/* Copy uses the same source attachment established by a successful fetch. */
bool database_copy(Database *db, Analysis *a, uint64_t first_row, uint64_t rows,
                   size_t first_col, size_t cols, char **tsv, char **error);
bool database_copy_page(Database *db,Analysis *analysis,const PivotDefinition *pivot,uint64_t first,size_t rows,size_t column,size_t columns,Page *page,char **error);
bool database_distinct(Database *db, Analysis *analysis, const char *column, const char *search,
    int date_level, const char *date_start, const char *date_end, uint64_t offset, int64_t known_total,
    DistinctPage *page, char **error);
bool database_execute(Database *db, const char *sql, duckdb_result *result, char **error);
bool database_export_parquet(Database *db,Analysis *source,const char *path,uint64_t *rows,char **error);
bool database_export_pivot_parquet(Database *db,Analysis *source,const PivotDefinition *pivot,const char *path,uint64_t *rows,char **error);
bool database_pivot(Database *db,Analysis *source,const PivotDefinition *definition,uint64_t offset,int64_t known_total,Page *page,char **error);
/* Opt-in source-derived chart readouts. Stages a complete matrix, optional
   totals and <=8 loaded-group ranking variants before replacing an owned out.
   Local unsafe/unsupported/limit cases retain the valid matrix with an inline
   status; query/cancellation/observed-file failures preserve out. A zero stamp
   captures the current eligible CSV/Parquet observation; nonzero must match.
   Same-query BOOLEAN witnesses gate NUL-free keys. This observes the declared
   regular source file, not arbitrary SQL dependencies, query determinism or
   durable identity. Attached databases have no all-pass immutable snapshot and
   retain their ordinary matrix/totals with Other unsupported.
   Combined Page+Other+retained plan staging is <=4 MiB; engine/allocator peaks
   and temporary existing SQL helper construction are separate costs. */
bool database_pivot_with_other(Database *db,Analysis *source,const PivotDefinition *definition,
    uint64_t offset,int64_t known_total,bool include_totals,uint64_t expected_source_stamp,
    Page *out,char **error);
bool database_pivot_totals(Database *db,Analysis *source,const PivotDefinition *definition,Page *page,char **error);
bool database_pivot_distinct(Database *db,Analysis *source,const PivotDefinition *context,const char *column,const char *search,int date_level,const char *date_start,const char *date_end,uint64_t offset,int64_t known_total,DistinctPage *page,char **error);
bool database_pivot_copy(Database *db,Analysis *source,const PivotDefinition *definition,uint64_t first,uint64_t rows,size_t column,size_t columns,bool header,char **tsv,char **error);
/* One source/pivot result rectangle, up to 256 columns; aggregation stays in
   DuckDB. Missing values do not count as filled/distinct. Empty VARCHAR does.
   Distinct compares (SQL type, value). Integers/DECIMAL sum/min/max are exact;
   floating/mixed numeric results and averages are explicitly approximate.
   Zero-initialize stats before first use. Failure preserves its owned result. */
bool database_statistics(Database *db,Analysis *source,const PivotDefinition *pivot,uint64_t first,uint64_t rows,size_t column,size_t columns,SelectionStats *stats,char **error);
bool database_statistics_ranges(Database *db,Analysis *source,StatisticsRequest *request,SelectionStats *stats,char **error);
#endif
