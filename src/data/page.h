#ifndef DUXCEL_PAGE_H
#define DUXCEL_PAGE_H
#include <duckdb.h> /* SQL type IDs only; this model owns no engine handles. */
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
/* Optional native raw-reader provenance. IDs are one-based original records,
   not result offsets. source_key/stamp identify this session's reader/cache
   snapshot, NOT a verified persistent source version or a format binding.
   Both pointers use malloc/free. A borrowed prefix may shorten count; bounds
   never follow Page.total or an infinite-canvas geometry view. */
typedef struct {
    uint64_t *ids;
    char *source_key;
    size_t count;
    uint64_t stamp;
} PageRecords;
/* Page owns columns/cells/relations, including all text. Never shared while mutable.
   Every non-NULL Cell.text uses release_cell_text, or free when the callback is
   NULL. Set the callback before storing text; it must remain valid until cleanup.
   Other fields use the C allocator. Moving a Page transfers this entire contract;
   nested totals pages each retain their own independently matched allocator. */
typedef struct {
    Column *columns;
    char *sql; /* Executed pivot relation, including resolved NULL column domains. */
    Cell *cells;
    void (*release_cell_text)(void *text);
    Relation *relations;
    size_t column_count, row_count, relation_count, bytes;
    uint64_t offset, total;
    bool count_pending; /* total is an observed lower bound until the count reply. */
    double query_ms;
    PivotTotals *totals; /* Optional owned matrix readouts; not ordinary result rows. */
    PageRecords records;
} Page;
struct PivotTotals {
    Page rows; /* One aggregate per measure, aligned to the owning page's rows. */
    Page columns; /* One row: category/measure totals, then overall measures. */
    bool has_columns;
};
/* Schema/catalog strings and arrays use malloc/free. These helpers also serve
   native engine caches; page_free additionally resets the entire moved value. */
void page_column_free(Column *column);
void page_relations_free(Relation *relations,size_t count);
void page_free(Page *page);
/* Result row in this Page's display offset. Works on a non-owning bounded
   prefix too; no allocation/lookup outside the loaded row array. */
bool page_record_id(const Page *page,uint64_t row,uint64_t *id);
void distinct_free(DistinctPage *page);
#endif
