#ifndef DUXCEL_BROWSER_QUERY_H
#define DUXCEL_BROWSER_QUERY_H
#include "page.h"

#define DX_BROWSER_SQL_BYTES (1024u*1024u)

/* These pure C helpers own returned SQL and schema strings with free(). No
   engine calls or host paths are resolved here. SQL is generated from the real
   Analysis/Pivot definitions, with inline escaped filters, for the official
   browser Worker. NONE and attached native databases are unavailable in W2. */
char *browser_relation_sql(const Analysis *analysis,const PivotDefinition *pivot,char **error);
char *browser_describe_sql(const char *relation,char **error);
char *browser_count_sql(const char *relation,char **error);
/* Six-column DESCRIBE reply -> owned zero-row schema. Standard column headers,
   names/types, <=256 rows and unique case-insensitive names are validated.
   Unknown SQL types retain their exact type_name with VARCHAR/NUM_NONE fallback.
   out must be zero-initialized or owned; failure leaves it unchanged. */
bool browser_schema_from_describe(Page *out,const Page *describe,char **error);
/* Canonical text projection keeps SQL NULL distinct and preserves exact raw
   numbers/temporal text. The original typed schema accompanies its wire reply.
   The full fixed 256-row interval must fit uint64_t. All SQL is <=1 MiB. */
char *browser_page_sql(const char *relation,const Page *schema,uint64_t offset,char **error);
/* First matrix slice: Rows/Values/Filters, including row date grains. Columns
   require a later bounded typed-domain adapter and fail explicitly here. */
char *browser_pivot_totals_sql(const Analysis *source,const PivotDefinition *pivot,char **error);
/* Owned total_i SQL descriptors from the validated result schema. Failure
   preserves out. Returned schema is suitable for canonical grand-total SQL. */
bool browser_pivot_totals_schema(Page *out,const Page *schema,const PivotDefinition *pivot,char **error);
/* Complete a fresh, undecorated, exact-count pivot result. Optional grand owns a
   source-derived aggregate row, with total_i descriptors. On success it moves
   into page->totals; NULL decorates without totals. Row totals copy exact result
   measure cells. Both inputs remain unchanged on failure. Combined owned output
   is bounded to 4 MiB. */
bool browser_pivot_complete(Page *page,Page *grand,const PivotDefinition *pivot,char **error);

/* Columns use bounded, typed SQL DISTINCT domains before canonical text is
   transported. A plan owns its real PIVOT relation and ordinal header metadata;
   it owns no engine handles or borrowed domain pages. Zero initialize/free it. */
typedef struct {
    char *sql;
    Column *headers;
    size_t count,bytes;
    bool empty_input;
} BrowserPivotPlan;
char *browser_pivot_domain_sql(const Analysis *source,const PivotDefinition *pivot,size_t field,char **error);
bool browser_pivot_plan_build(BrowserPivotPlan *out,const Analysis *source,const PivotDefinition *pivot,const Page *domains,char **error);
void browser_pivot_plan_free(BrowserPivotPlan *plan);
/* Source GROUPING SETS readouts: rows=true is the matching result-page interval;
   false is one row of category/measure totals followed by overall measures. */
char *browser_pivot_readout_sql(const Analysis *source,const PivotDefinition *pivot,const BrowserPivotPlan *plan,bool rows,uint64_t offset,char **error);
bool browser_pivot_readout_schema(Page *out,const Page *schema,const PivotDefinition *pivot,const BrowserPivotPlan *plan,bool rows,char **error);
/* Both readouts must be supplied together or both NULL. Success moves readouts
   and owns cloned header metadata. Failure preserves every input. <=4 MiB. */
bool browser_pivot_plan_complete(Page *page,Page *rows,Page *columns,const PivotDefinition *pivot,const BrowserPivotPlan *plan,char **error);
/* A direct typed source/table/analytical-result relation. DISTINCT and BIGNUM
   arithmetic execute over the full selected interval, never a loaded page.
   The scalar projection has 13 named VARCHAR columns and one exact result row. */
char *browser_statistics_sql(const char *relation,const Page *schema,uint64_t first,uint64_t rows,size_t column,size_t columns,char **error);
/* Decode atomically, retaining exact sums/min/max and native approximate/NaN/
   overflow meanings. retained_bytes accounts for the operation's other owned
   schema/relation buffers; scalar + retained + new statistics must fit 4 MiB. */
bool browser_statistics_from_page(SelectionStats *out,const Page *scalar,uint64_t max_cells,size_t retained_bytes,char **error);
#endif
