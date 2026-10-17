#ifndef DUXCEL_ANALYSIS_H
#define DUXCEL_ANALYSIS_H
#include "../common.h"
#include "number.h"
typedef enum { SOURCE_NONE, SOURCE_CSV, SOURCE_PARQUET, SOURCE_DATABASE, SOURCE_DEMO } SourceKind;
typedef enum { FILTER_CONTAINS, FILTER_EQUAL, FILTER_GREATER, FILTER_LESS, FILTER_NULL, FILTER_NOT_NULL,
    FILTER_IN, FILTER_NOT_IN, FILTER_DATE_IN, FILTER_DATE_NOT_IN, FILTER_BETWEEN, FILTER_COMPOSITE } FilterOp;
#define FILTER_OP_MAX FILTER_COMPOSITE
typedef enum { AGG_SUM, AGG_COUNT, AGG_DISTINCT, AGG_AVG, AGG_MIN, AGG_MAX } Aggregate;
#define DX_ANALYSIS_FILTERS 16
#define DX_ANALYSIS_SORTS 8
#define DX_QUERY_BYTES 65536
typedef struct { char *column,*value; FilterOp op; NumberType number; } AnalysisFilter;
typedef struct { char *column; bool descending; } AnalysisSort;
typedef struct {
    SourceKind kind;
    char *path, *schema, *table;
    char *query; /* owned SELECT relation over the original source named input */
    bool unique_rows; /* DISTINCT across source-query output, before filtering/aggregation */
    char *filter_column, *filter_value;
    FilterOp filter_op;
    NumberType filter_number; /* Derived schema, never persisted. */
    char *group_column, *measure_column;
    Aggregate aggregate;
    char *sort_column;
    bool descending;
    AnalysisSort sorts[DX_ANALYSIS_SORTS-1]; size_t sort_count;
    AnalysisFilter filters[DX_ANALYSIS_FILTERS-1]; size_t filter_count;
} Analysis;
Analysis analysis_copy(const Analysis *a);
void analysis_free(Analysis *a);
size_t analysis_text_bytes(const Analysis *a); /* owned string payload; SIZE_MAX for invalid counts */
bool analysis_query_valid(const Analysis *a,char **error);
bool analysis_query_set(Analysis *a,const char *query,char **error); /* empty resets; trims final semicolon */
bool analysis_filters_valid(const Analysis *a,char **error);
bool analysis_filter_get(const Analysis *a,const char *column,FilterOp *op,const char **value);
bool analysis_filter_set(Analysis *a,const char *column,FilterOp op,const char *value,char **error);
void analysis_filter_remove(Analysis *a,const char *column);
void analysis_filters_clear(Analysis *a);
bool analysis_filter_move(Analysis *a,size_t from,size_t to);
void analysis_filter_number(Analysis *a,const char *column,NumberType type);
void analysis_merge_types(Analysis *target,const Analysis *resolved);
AnalysisSort analysis_sort_at(const Analysis *analysis,size_t index); /* borrowed column */
bool analysis_sort_set(Analysis *analysis,const char *column,bool descending,bool append,char **error);
void analysis_sort_remove(Analysis *analysis,size_t index);
bool analysis_sort_move(Analysis *analysis,size_t from,size_t to);
void analysis_sorts_clear(Analysis *analysis);
bool analysis_sorts_valid(const Analysis *analysis,char **error);
bool analysis_sorts_equal(const Analysis *left,const Analysis *right);
/* Filter display order and derived type bindings do not change the result. */
bool analysis_same_result(const Analysis *left,const Analysis *right);
SourceKind source_kind(const char *path);
const char *filter_label(FilterOp op);
const char *aggregate_label(Aggregate agg);
const char *aggregate_caption(Aggregate agg);
char *aggregate_measure_label(Aggregate aggregate,const char *field);
/* Generated SQL owns no renderer state. Execution binds filter_value to $__duxcel_filter when present. */
char *analysis_sql(const Analysis *a, bool parameters);
/* Mechanical SQL building blocks; callers validate the model. The metadata
   option changes only the Parquet reader, not the business transformation path.
   analysis_wrap_sql takes ownership of owned_source, even without transforms. */
char *analysis_source_sql(const Analysis *a,bool parquet_row_numbers);
char *analysis_wrap_sql(const Analysis *a,bool parameters,char *owned_source);
char *analysis_filter_expression(const char *column,FilterOp op,const char *text,const char *parameter);
char *analysis_filter_typed(const char *column,FilterOp op,const char *text,const char *parameter,NumberType type);
char *analysis_page_sql(const Analysis *a, uint64_t offset, uint64_t limit);
char *analysis_values_json(char *const *values, size_t count); /* NULL item = SQL NULL */
bool analysis_parse_values(const char *json, char ***values, size_t *count);
/* Composite payload owns two independent predicates over the same column.
   List and condition remain encoded strings so history/workbook ownership stays unchanged. */
char *analysis_filter_pack(FilterOp list_op,const char *list,FilterOp condition_op,const char *condition);
bool analysis_filter_unpack(const char *payload,FilterOp *list_op,char **list,FilterOp *condition_op,char **condition);
bool analysis_filter_value_valid(FilterOp op,const char *value);
bool analysis_value_checked(char *const *values, size_t count, bool all, bool dates, const char *key, bool *mixed);
bool analysis_toggle_value(char ***values, size_t *count, bool all, bool dates, const char *key);
/* Toggle a searched/scoped checklist as one atomic change. NULL is a typed key. */
bool analysis_toggle_values(char ***values,size_t *count,bool all,bool dates,char *const *keys,size_t key_count);
#endif
