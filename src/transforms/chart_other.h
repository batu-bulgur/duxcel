#ifndef DUXCEL_CHART_OTHER_H
#define DUXCEL_CHART_OTHER_H
#include "../data/page.h"

#define DX_CHART_OTHER_SQL_BYTES ((size_t)4*1024*1024)
/* Borrowed Page ranking, never source/global rank. The first SUM measure ranks
   when present; otherwise plot_measure ranks. Ties retain Page row order. */
typedef struct {
    size_t top_rows[3],top_count,rank_measure,keys,measures,categories,loaded_rows;
    uint64_t offset,total;
    bool has_other,truncated;
} ChartOtherRank;
typedef struct { char *sql; size_t sql_bytes; ChartOtherRank rank; } ChartOtherPlan;
/* No allocation on success. Inputs remain borrowed and unchanged. Failure
   preserves out; it may replace the caller-owned error string. */
bool chart_other_rank(const Page *page,const PivotDefinition *definition,size_t plot_measure,
                      ChartOtherRank *out,char **error);
/* One rank variant; all assigned category/measure columns in existing order.
   SQL aggregates the exact union of non-top LOADED row keys from the actual
   captured Page, after the source definition and Pivot filters/date grains.
   cast_to_type binds escaped key literals to their projected source expression;
   no type name from Page metadata is interpolated as SQL. Complex Row key types
   are rejected. column_key_types/count must describe the original Column
   fields in shelf order from that same captured source schema (before grains).
   Count must exactly match the shelf; NULL is allowed only without Columns.
   Types remain borrowed during this call and are never retained. Date grains
   require original DATE/TIMESTAMP types. No describe/type-guard query is added.

   Caller owns a valid captured Analysis/Pivot and a matching complete typed
   Pivot Page. This helper does not prove source/version/request routing, source
   immutability, temporal schema, or freshness. Those remain publication fences.
   SQL success owns no borrowed strings; <=3 groups produce sql=NULL/no query.
   max_sql_bytes is an output bound (including NUL), capped at 4 MiB; it is not
   an allocator/RSS/engine bound on the existing source SQL helpers.
   out must be zero-initialized or an independently owned plan. Success replaces
   it, failure preserves it. No App/Worker/codec activation is provided here. */
bool chart_other_plan(const Analysis *source,const PivotDefinition *definition,const Page *page,
                      size_t plot_measure,const duckdb_type *column_key_types,size_t column_key_count,
                      size_t max_sql_bytes,ChartOtherPlan *out,char **error);
void chart_other_plan_free(ChartOtherPlan *plan);
#endif
