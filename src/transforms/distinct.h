#ifndef DUXCEL_DISTINCT_QUERY_H
#define DUXCEL_DISTINCT_QUERY_H
#include "pivot.h"
#define DX_DISTINCT_QUERY_ROWS 128
/* Owned SQL, no borrowed Analysis/Pivot pointers. keep_primary_filter mirrors
   the sanitized source: native execution must clear its binding Analysis's
   filter_column when false, so $__duxcel_filter is bound only when retained.
   Native callers resolve source/pivot numeric types before building. */
typedef struct { char *relation; bool keep_primary_filter; } DistinctQuery;
/* out is zero-initialized or owned. Failure preserves out. parameters=true
   keeps the native primary parameter; false inlines all browser predicates. */
bool distinct_query_build(DistinctQuery *out,const Analysis *source,const PivotDefinition *context,
    const char *column,const char *search,int level,const char *start,const char *end,bool parameters,char **error);
char *distinct_page_sql(const DistinctQuery *query,uint64_t offset);
char *distinct_count_sql(const DistinctQuery *query);
void distinct_query_free(DistinctQuery *query);
#endif
