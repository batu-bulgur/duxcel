#ifndef DUXCEL_DATABASE_H
#define DUXCEL_DATABASE_H
#include <duckdb.h>
#include "../transforms/analysis.h"
#include "../transforms/pivot.h"
#define DX_PAGE_ROWS 256
#define DX_COPY_CELLS 20000
#define DX_DISTINCT_ROWS 128
typedef struct { char *label,*value,*upper,*key; bool is_null; } DistinctValue;
typedef struct {
    DistinctValue *values;
    size_t count,bytes;
    uint64_t offset,total;
} DistinctPage;
typedef struct {
    char *name,*type_name,*caption,*detail;
    /* Typed ON components, in shelf order. NULL pointers are SQL NULL; "" is text. */
    char **categories;
    size_t category_count;
    PivotDateGrain date_grain,category_grains[DX_PIVOT_FIELDS];
    duckdb_type type;
    NumberType number_type;
    bool numeric,filtered;
    unsigned char sort_priority;
    bool sort_descending;
} Column;
typedef struct { char *text; bool is_null; double number; } Cell;
typedef struct {
    uint64_t cells,filled,numeric,distinct;
    char *sum,*average,*minimum,*maximum;
    bool approximate,non_finite,sum_overflow,average_overflow;
    double query_ms;
} SelectionStats;
void statistics_free(SelectionStats *stats);
/* An asynchronous request owns each table definition once, even when several
   pivot intersections use it. 0 selects the job's primary analysis. */
typedef struct {
    Analysis items[DX_MAX_OBJECTS]; uint64_t ids[DX_MAX_OBJECTS];
    size_t count,bytes;
} SourceSnapshot;
size_t source_snapshot_bind(SourceSnapshot *snapshot,uint64_t id,const Analysis *source,char **error); /* 1-based; SIZE_MAX on error */
void source_snapshot_free(SourceSnapshot *snapshot);
/* Owned, bounded snapshot for mixed worksheet selection statistics. Rectangles
   are disjoint source/pivot result intersections; sparse labels stay VARCHAR. */
typedef struct {
    bool is_pivot;
    PivotDefinition pivot;
    size_t source; /* 1-based SourceSnapshot index, or 0 for the primary source */
    uint64_t first,rows;
    size_t column,columns;
} StatisticsRange;
typedef struct {
    SourceSnapshot sources;
    StatisticsRange ranges[DX_MAX_OBJECTS+1];
    size_t count;
    char **texts;
    size_t text_count,text_capacity,text_bytes;
    uint64_t cells;
} StatisticsRequest;
void statistics_request_free(StatisticsRequest *request);
bool statistics_request_text(StatisticsRequest *request,const char *text,char **error);

typedef struct { char *schema, *name, *label; } Relation;
typedef struct PivotTotals PivotTotals;
/* Page owns columns/cells/relations, including all text. Never shared while mutable. */
typedef struct {
    Column *columns;
    char *sql; /* Executed pivot relation, including resolved NULL column domains. */
    Cell *cells;
    Relation *relations;
    size_t column_count, row_count, relation_count, bytes;
    uint64_t offset, total;
    bool count_pending; /* total is an observed lower bound until the count reply. */
    double query_ms;
    PivotTotals *totals; /* Optional owned matrix readouts; not ordinary result rows. */
} Page;
struct PivotTotals {
    Page rows; /* One aggregate per measure, aligned to the owning page's rows. */
    Page columns; /* One row: category/measure totals, then overall measures. */
    bool has_columns;
};
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
    char *pivot_key;
    char *pivot_sql;
    Column *pivot_headers;
    size_t pivot_columns;
    bool pivot_configured;
    bool pivot_totals_valid;
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
bool database_fetch(Database *db, Analysis *a, uint64_t offset, int64_t known_total, Page *page, char **error);
bool database_fetch_window(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_fetch_table(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_fetch_table_window(Database *db,Analysis *analysis,uint64_t offset,int64_t known_total,Page *page,char **error);
bool database_count(Database *db,Analysis *analysis,uint64_t *total,char **error);
/* Copy uses the same source attachment established by a successful fetch. */
bool database_copy(Database *db, Analysis *a, uint64_t first_row, uint64_t rows,
                   size_t first_col, size_t cols, char **tsv, char **error);
bool database_copy_page(Database *db,Analysis *analysis,const PivotDefinition *pivot,uint64_t first,size_t rows,size_t column,size_t columns,Page *page,char **error);
void page_free(Page *page);
bool database_distinct(Database *db, Analysis *analysis, const char *column, const char *search,
    int date_level, const char *date_start, const char *date_end, uint64_t offset, int64_t known_total,
    DistinctPage *page, char **error);
void distinct_free(DistinctPage *page);
bool database_execute(Database *db, const char *sql, duckdb_result *result, char **error);
bool database_export_parquet(Database *db,Analysis *source,const char *path,uint64_t *rows,char **error);
bool database_export_pivot_parquet(Database *db,Analysis *source,const PivotDefinition *pivot,const char *path,uint64_t *rows,char **error);
bool database_pivot(Database *db,Analysis *source,const PivotDefinition *definition,uint64_t offset,int64_t known_total,Page *page,char **error);
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
