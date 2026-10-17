#include "analysis.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

Analysis analysis_copy(const Analysis *a) {
    Analysis b = *a;
#define COPY(field) b.field = a->field ? dx_strdup(a->field) : NULL
    COPY(path); COPY(schema); COPY(table); COPY(query); COPY(filter_column); COPY(filter_value);
    COPY(group_column); COPY(measure_column); COPY(sort_column);
    for (size_t i=0;i<a->filter_count;i++) b.filters[i]=(AnalysisFilter){dx_strdup(a->filters[i].column),a->filters[i].value ? dx_strdup(a->filters[i].value) : NULL,a->filters[i].op,a->filters[i].number};
    for (size_t i=0;i<a->sort_count;i++) b.sorts[i]=(AnalysisSort){dx_strdup(a->sorts[i].column),a->sorts[i].descending};
#undef COPY
    return b;
}
void analysis_free(Analysis *a) {
    free(a->path); free(a->schema); free(a->table); free(a->query); free(a->filter_column);
    free(a->filter_value); free(a->group_column); free(a->measure_column); free(a->sort_column);
    for (size_t i=0;i<a->filter_count;i++) { free(a->filters[i].column); free(a->filters[i].value); }
    for (size_t i=0;i<a->sort_count;i++) free(a->sorts[i].column);
    memset(a, 0, sizeof(*a));
}
size_t analysis_text_bytes(const Analysis *a) {
    if (a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS) return SIZE_MAX;
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    size_t bytes=0; for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (texts[i]) bytes+=strlen(texts[i])+1;
    for (size_t i=0;i<a->filter_count;i++) { if (a->filters[i].column) bytes+=strlen(a->filters[i].column)+1; if (a->filters[i].value) bytes+=strlen(a->filters[i].value)+1; }
    for (size_t i=0;i<a->sort_count;i++) if (a->sorts[i].column) bytes+=strlen(a->sorts[i].column)+1;
    return bytes;
}
static bool filter_error(char **error,const char *text) { if (error) { free(*error); *error=dx_strdup(text); } return false; }
bool analysis_query_valid(const Analysis *a,char **error) {
    if (a->unique_rows && a->kind==SOURCE_NONE) return filter_error(error,"Open a data source before removing duplicate rows.");
    return !a->query || (a->kind!=SOURCE_NONE && *a->query && strlen(a->query)<=DX_QUERY_BYTES) || filter_error(error,"Use a source query of up to 64 KiB.");
}
bool analysis_query_set(Analysis *a,const char *query,char **error) {
    if (query && strlen(query)>DX_QUERY_BYTES) return filter_error(error,"Use a source query of up to 64 KiB.");
    const char *start=query ? query : "";
    size_t length=strlen(start); while (length && isspace((unsigned char)start[length-1])) length--;
    if (length && start[length-1]==';') { length--; while (length && isspace((unsigned char)start[length-1])) length--; }
    if (length && a->kind==SOURCE_NONE) return filter_error(error,"Open a data source before editing its query.");
    char *copy=length ? dx_alloc(length+1) : NULL; if (copy) { memcpy(copy,start,length); copy[length]=0; }
    free(a->query); a->query=copy; return true;
}
static bool same_column(const char *a,const char *b) {
    if (!a || !b) return a==b;
    while (*a && *b) { unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++; if (x>='A' && x<='Z') x+=32; if (y>='A' && y<='Z') y+=32; if (x!=y) return false; }
    return *a==*b;
}
static bool same_text(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
bool analysis_same_result(const Analysis *a,const Analysis *b) {
    if (!analysis_query_valid(a,NULL) || !analysis_query_valid(b,NULL) || !analysis_filters_valid(a,NULL) || !analysis_filters_valid(b,NULL) || !analysis_sorts_valid(a,NULL) || !analysis_sorts_valid(b,NULL)) return false;
    if (a->kind!=b->kind || a->unique_rows!=b->unique_rows || !same_text(a->path,b->path) || !same_text(a->schema,b->schema) || !same_text(a->table,b->table) || !same_text(a->query,b->query) ||
        !same_text(a->group_column,b->group_column) || !same_text(a->measure_column,b->measure_column) || (a->group_column && a->aggregate!=b->aggregate) || !analysis_sorts_equal(a,b)) return false;
    size_t count=a->filter_column ? a->filter_count+1 : 0,other=b->filter_column ? b->filter_count+1 : 0;
    if (count!=other) return false;
    bool matched[DX_ANALYSIS_FILTERS]={0};
    for (size_t i=0;i<count;i++) {
        const char *column=i ? a->filters[i-1].column : a->filter_column,*value=i ? a->filters[i-1].value : a->filter_value;
        FilterOp op=i ? a->filters[i-1].op : a->filter_op; bool found=false;
        for (size_t j=0;j<other;j++) if (!matched[j]) {
            const char *name=j ? b->filters[j-1].column : b->filter_column,*text=j ? b->filters[j-1].value : b->filter_value;
            FilterOp filter=j ? b->filters[j-1].op : b->filter_op;
            if (same_column(column,name) && op==filter && same_text(value,text)) { matched[j]=true; found=true; break; }
        }
        if (!found) return false;
    }
    return true;
}
AnalysisSort analysis_sort_at(const Analysis *a,size_t index) {
    return !index && a->sort_column ? (AnalysisSort){a->sort_column,a->descending} : index && index<=a->sort_count ? a->sorts[index-1] : (AnalysisSort){0};
}
bool analysis_sorts_valid(const Analysis *a,char **error) {
    if (a->sort_count>=DX_ANALYSIS_SORTS || (a->sort_count && !a->sort_column)) return filter_error(error,"Invalid sort order.");
    for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) {
        const char *name=analysis_sort_at(a,i).column;
        if (!name || !*name || strlen(name)>4096) return filter_error(error,"Invalid sort column.");
        for (size_t j=0;j<i;j++) if (same_column(name,analysis_sort_at(a,j).column)) return filter_error(error,"Duplicate sort column.");
    }
    return true;
}
void analysis_sorts_clear(Analysis *a) {
    free(a->sort_column); a->sort_column=NULL; a->descending=false;
    for (size_t i=0;i<a->sort_count;i++) free(a->sorts[i].column);
    memset(a->sorts,0,sizeof(a->sorts)); a->sort_count=0;
}
bool analysis_sort_set(Analysis *a,const char *column,bool descending,bool append,char **error) {
    if (!analysis_sorts_valid(a,error)) return false;
    if (!column || !*column || strlen(column)>4096) return filter_error(error,"Choose a valid sort column.");
    if (append) for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) if (same_column(column,analysis_sort_at(a,i).column)) {
        if (!i) a->descending=descending; else a->sorts[i-1].descending=descending; return true;
    }
    if (append && a->sort_column && a->sort_count==DX_ANALYSIS_SORTS-1) return filter_error(error,"Use up to eight sort columns.");
    char *name=dx_strdup(column);
    if (!append) analysis_sorts_clear(a);
    if (!a->sort_column) { a->sort_column=name; a->descending=descending; }
    else a->sorts[a->sort_count++]=(AnalysisSort){name,descending};
    return true;
}
static void sorts_assign(Analysis *a,const AnalysisSort *all,size_t count) {
    a->sort_column=count ? all[0].column : NULL; a->descending=count ? all[0].descending : false;
    a->sort_count=count ? count-1 : 0; memset(a->sorts,0,sizeof(a->sorts)); if (count>1) memcpy(a->sorts,all+1,(count-1)*sizeof(*all));
}
void analysis_sort_remove(Analysis *a,size_t index) {
    size_t count=a->sort_count+(a->sort_column ? 1 : 0); if (index>=count) return;
    AnalysisSort all[DX_ANALYSIS_SORTS]; for (size_t i=0;i<count;i++) all[i]=analysis_sort_at(a,i);
    free(all[index].column); memmove(all+index,all+index+1,(count-index-1)*sizeof(*all)); sorts_assign(a,all,count-1);
}
bool analysis_sort_move(Analysis *a,size_t from,size_t to) {
    size_t count=a->sort_count+(a->sort_column ? 1 : 0); if (from>=count || to>=count) return false;
    AnalysisSort all[DX_ANALYSIS_SORTS]; for (size_t i=0;i<count;i++) all[i]=analysis_sort_at(a,i);
    AnalysisSort moving=all[from]; if (from<to) memmove(all+from,all+from+1,(to-from)*sizeof(*all)); else memmove(all+to+1,all+to,(from-to)*sizeof(*all)); all[to]=moving; sorts_assign(a,all,count); return true;
}
bool analysis_sorts_equal(const Analysis *a,const Analysis *b) {
    if (!analysis_sorts_valid(a,NULL) || !analysis_sorts_valid(b,NULL)) return false;
    size_t count=a->sort_count+(a->sort_column ? 1 : 0); if (count!=b->sort_count+(b->sort_column ? 1 : 0)) return false;
    for (size_t i=0;i<count;i++) { AnalysisSort x=analysis_sort_at(a,i),y=analysis_sort_at(b,i); if (!x.column || !y.column || strcmp(x.column,y.column) || x.descending!=y.descending) return false; }
    return true;
}
bool analysis_filters_valid(const Analysis *a,char **error) {
    if (a->filter_count>=DX_ANALYSIS_FILTERS || (a->filter_count && !a->filter_column)) return filter_error(error,"Invalid filter list.");
    size_t bytes=0;
    for (size_t i=0;i<a->filter_count+(a->filter_column ? 1 : 0);i++) {
        const char *column=i ? a->filters[i-1].column : a->filter_column,*value=i ? a->filters[i-1].value : a->filter_value;
        FilterOp op=i ? a->filters[i-1].op : a->filter_op;
        if (!column || !*column || strlen(column)>4096 || (unsigned)op>FILTER_OP_MAX || (value && strlen(value)>4*1024*1024) || !analysis_filter_value_valid(op,value)) return filter_error(error,"Invalid column filter.");
        bytes+=strlen(column)+1+(value ? strlen(value)+1 : 0);
        for (size_t j=0;j<i;j++) if (same_column(column,j ? a->filters[j-1].column : a->filter_column)) return filter_error(error,"Duplicate column filter.");
    }
    return bytes<=8*1024*1024 || filter_error(error,"Column filters exceed 8 MiB. Use fewer selected exceptions.");
}
bool analysis_filter_get(const Analysis *a,const char *column,FilterOp *op,const char **value) {
    if (a->filter_column && same_column(a->filter_column,column)) { if (op) *op=a->filter_op; if (value) *value=a->filter_value; return true; }
    for (size_t i=0;i<a->filter_count;i++) if (same_column(a->filters[i].column,column)) { if (op) *op=a->filters[i].op; if (value) *value=a->filters[i].value; return true; }
    return false;
}
bool analysis_filter_set(Analysis *a,const char *column,FilterOp op,const char *value,char **error) {
    if (!column || !*column || strlen(column)>4096 || (unsigned)op>FILTER_OP_MAX || (value && strlen(value)>4*1024*1024) || !analysis_filter_value_valid(op,value)) return filter_error(error,"Invalid column filter.");
    bool primary=a->filter_column && same_column(a->filter_column,column); size_t at=a->filter_count;
    for (size_t i=0;i<a->filter_count;i++) if (same_column(a->filters[i].column,column)) at=i;
    if (!primary && at==a->filter_count && a->filter_column && a->filter_count==DX_ANALYSIS_FILTERS-1) return filter_error(error,"Use up to 16 simultaneous column filters.");
    char *name=dx_strdup(column),*text=value ? dx_strdup(value) : NULL;
    size_t bytes=strlen(name)+1+(text ? strlen(text)+1 : 0);
    if (a->filter_column && !primary) bytes+=strlen(a->filter_column)+1+(a->filter_value ? strlen(a->filter_value)+1 : 0);
    for (size_t i=0;i<a->filter_count;i++) if (i!=at) bytes+=strlen(a->filters[i].column)+1+(a->filters[i].value ? strlen(a->filters[i].value)+1 : 0);
    if (bytes>8*1024*1024) { free(name); free(text); return filter_error(error,"Column filters exceed 8 MiB. Use fewer selected exceptions."); }
    if (primary || !a->filter_column) { free(a->filter_column); free(a->filter_value); a->filter_column=name; a->filter_value=text; a->filter_op=op; a->filter_number=(NumberType){0}; }
    else { if (at<a->filter_count) { free(a->filters[at].column); free(a->filters[at].value); } else a->filter_count++; a->filters[at]=(AnalysisFilter){name,text,op,{0}}; }
    return true;
}
void analysis_filter_remove(Analysis *a,const char *column) {
    if (a->filter_column && same_column(a->filter_column,column)) {
        free(a->filter_column); free(a->filter_value); a->filter_column=a->filter_value=NULL; a->filter_number=(NumberType){0};
        if (a->filter_count) { AnalysisFilter first=a->filters[0]; a->filter_column=first.column; a->filter_value=first.value; a->filter_op=first.op; a->filter_number=first.number; memmove(a->filters,a->filters+1,(--a->filter_count)*sizeof(*a->filters)); memset(a->filters+a->filter_count,0,sizeof(*a->filters)); }
        return;
    }
    for (size_t i=0;i<a->filter_count;i++) if (same_column(a->filters[i].column,column)) {
        free(a->filters[i].column); free(a->filters[i].value); memmove(a->filters+i,a->filters+i+1,(--a->filter_count-i)*sizeof(*a->filters)); memset(a->filters+a->filter_count,0,sizeof(*a->filters)); return;
    }
}
void analysis_filters_clear(Analysis *a) {
    while (a->filter_column) analysis_filter_remove(a,a->filter_column);
}
bool analysis_filter_move(Analysis *a,size_t from,size_t to) {
    if (!a->filter_column || from>a->filter_count || to>a->filter_count) return false;
    AnalysisFilter all[DX_ANALYSIS_FILTERS]; all[0]=(AnalysisFilter){a->filter_column,a->filter_value,a->filter_op,a->filter_number}; memcpy(all+1,a->filters,a->filter_count*sizeof(*all));
    AnalysisFilter moving=all[from]; if (from<to) memmove(all+from,all+from+1,(to-from)*sizeof(*all)); else memmove(all+to+1,all+to,(from-to)*sizeof(*all)); all[to]=moving;
    a->filter_column=all[0].column; a->filter_value=all[0].value; a->filter_op=all[0].op; a->filter_number=all[0].number; memcpy(a->filters,all+1,a->filter_count*sizeof(*all)); return true;
}
void analysis_filter_number(Analysis *a,const char *column,NumberType type) {
    if (a->filter_column && same_column(a->filter_column,column)) a->filter_number=type;
    for (size_t i=0;i<a->filter_count;i++) if (same_column(a->filters[i].column,column)) a->filters[i].number=type;
}
void analysis_merge_types(Analysis *a,const Analysis *resolved) {
    if (resolved->filter_column) analysis_filter_number(a,resolved->filter_column,resolved->filter_number);
    for (size_t i=0;i<resolved->filter_count;i++) analysis_filter_number(a,resolved->filters[i].column,resolved->filters[i].number);
}
static bool ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    if (a < b) return false;
    for (size_t i = 0; i < b; i++) if (tolower((unsigned char)s[a-b+i]) != suffix[i]) return false;
    return true;
}
SourceKind source_kind(const char *p) {
    if (ends_with(p, ".csv") || ends_with(p, ".tsv") || ends_with(p, ".csv.gz")) return SOURCE_CSV;
    if (ends_with(p, ".parquet")) return SOURCE_PARQUET;
    if (ends_with(p, ".duckdb") || ends_with(p, ".db")) return SOURCE_DATABASE;
    return SOURCE_NONE;
}
const char *filter_label(FilterOp op) {
    static const char *labels[] = {"Contains", "Equals", "Greater than", "Less than", "Is NULL", "Is not NULL", "Selected values", "Excluded values", "Selected dates", "Excluded dates", "Between", "Values and condition"};
    return op >= FILTER_CONTAINS && op <= FILTER_OP_MAX ? labels[op] : "Invalid";
}
const char *aggregate_label(Aggregate agg) {
    static const char *labels[] = {"SUM", "COUNT", "COUNT DISTINCT", "AVG", "MIN", "MAX"};
    return agg >= AGG_SUM && agg <= AGG_MAX ? labels[agg] : "COUNT";
}
const char *aggregate_caption(Aggregate agg) {
    return agg==AGG_COUNT ? "Row count" : agg==AGG_DISTINCT ? "Distinct count" : aggregate_label(agg);
}
char *aggregate_measure_label(Aggregate agg,const char *field) {
    /* Keep COUNT(*) semantics of existing workbooks, including missing values. */
    return agg==AGG_COUNT ? dx_printf("COUNT(*) [%s]",field) : dx_printf("%s(%s)",aggregate_label(agg),field);
}
char *analysis_filter_expression(const char *column,FilterOp op,const char *text,const char *parameter) { return analysis_filter_typed(column,op,text,parameter,(NumberType){0}); }
char *analysis_filter_typed(const char *column,FilterOp op,const char *text,const char *parameter,NumberType type) {
    if (op==FILTER_COMPOSITE) {
        FilterOp list_op,condition_op; char *list=NULL,*term=NULL;
        if (!analysis_filter_unpack(text,&list_op,&list,&condition_op,&term)) return dx_strdup("false");
        char *lp=parameter ? dx_printf("json_extract_string(%s, '$[1]')",parameter) : NULL;
        char *cp=parameter ? dx_printf("json_extract_string(%s, '$[3]')",parameter) : NULL;
        char *left=analysis_filter_typed(column,list_op,list,lp,type),*right=analysis_filter_typed(column,condition_op,term,cp,type);
        char *sql=dx_printf("(%s AND %s)",left,right);
        free(list); free(term); free(lp); free(cp); free(left); free(right); return sql;
    }
    if (op==FILTER_BETWEEN) {
        char **bounds=NULL; size_t count=0;
        if (!analysis_parse_values(text,&bounds,&count)) return dx_strdup("false");
        if (count!=2 || !bounds[0] || !bounds[1]) { for (size_t i=0;i<count;i++) free(bounds[i]); free(bounds); return dx_strdup("false"); }
        char *lo=parameter ? dx_printf("json_extract_string(%s, '$[0]')",parameter) : NULL;
        char *hi=parameter ? dx_printf("json_extract_string(%s, '$[1]')",parameter) : NULL;
        /* Negated strict predicates retain exact numeric conversion and inclusive bounds. */
        char *left=analysis_filter_typed(column,FILTER_LESS,bounds[0],lo,type),*right=analysis_filter_typed(column,FILTER_GREATER,bounds[1],hi,type);
        char *col=dx_quote(column,'"'); char *sql=dx_printf("(%s IS NOT NULL AND NOT (%s) AND NOT (%s))",col,left,right); free(col);
        free(bounds[0]); free(bounds[1]); free(bounds); free(lo); free(hi); free(left); free(right); return sql;
    }
    char *col=dx_quote(column,'"'),*value=parameter ? dx_strdup(parameter) : dx_quote(text ? text : "",'\''),*condition;
    if (op>=FILTER_EQUAL && op<=FILTER_LESS && type.kind!=NUM_NONE) {
        condition=number_filter_sql(type,col,op==FILTER_GREATER ? NUMBER_GREATER : op==FILTER_LESS ? NUMBER_LESS : NUMBER_EQUAL,text,value);
        if (condition) { free(col); free(value); return condition; }
    }
    if (op>=FILTER_IN && op<=FILTER_DATE_NOT_IN) {
        char *match=op>=FILTER_DATE_IN ? dx_printf("EXISTS (SELECT 1 FROM json_each(%s) selected WHERE %s >= cast_to_type(split_part(json_extract_string(selected.value, '$'), chr(9), 1), %s) AND %s < cast_to_type(split_part(json_extract_string(selected.value, '$'), chr(9), 2), %s))",value,col,col,col,col) :
            dx_printf("CAST(%s AS VARCHAR) IN (SELECT json_extract_string(value, '$') FROM json_each(%s))",col,value);
        condition=dx_printf("%s((%s IS NOT NULL AND COALESCE(%s,false)) OR (%s IS NULL AND json_contains(%s, 'null')))",op==FILTER_NOT_IN || op==FILTER_DATE_NOT_IN ? "NOT " : "",col,match,col,value); free(match);
    } else if (op==FILTER_CONTAINS) condition=dx_printf("contains(CAST(%s AS VARCHAR), %s)",col,value);
    else if (op>=FILTER_NULL) condition=dx_printf("%s IS %sNULL",col,op==FILTER_NOT_NULL ? "NOT " : "");
    else condition=dx_printf("%s %s cast_to_type(%s, %s)",col,op==FILTER_GREATER ? ">" : op==FILTER_LESS ? "<" : "=",value,col);
    free(col); free(value); return condition;
}
char *analysis_source_sql(const Analysis *a,bool parquet_row_numbers) {
    char *sql = NULL;
    if (a->kind == SOURCE_DEMO) {
        sql = dx_strdup("SELECT i::BIGINT AS \"Transaction ID\",\n"
            "  (DATE '2026-01-01' + (i % 280)::INTEGER) AS \"Date\",\n"
            "  CASE i % 4 WHEN 0 THEN 'İstanbul' WHEN 1 THEN 'Ankara' WHEN 2 THEN 'İzmir' ELSE 'Bursa' END AS \"City\",\n"
            "  ((i * 137 % 100000) / 100.0)::DECIMAL(12,2) AS \"Amount\",\n"
            "  (i % 3 = 0) AS \"Approved\",\n"
            "  CASE WHEN i % 11 = 0 THEN NULL ELSE 'Analysis ' || i END AS \"Note\"\n"
            "FROM range(1000000) t(i)");
    } else if (a->kind == SOURCE_DATABASE) {
        char *schema = dx_quote(a->schema ? a->schema : "main", '"');
        char *table = dx_quote(a->table ? a->table : "", '"');
        sql = dx_printf("SELECT * FROM \"source_db\".%s.%s", schema, table);
        free(schema); free(table);
    } else {
        char *path = dx_quote(a->path ? a->path : "", '\'');
        sql = dx_printf("SELECT * FROM %s(%s%s)", a->kind == SOURCE_PARQUET ? "read_parquet" : "read_csv_auto", path, a->kind == SOURCE_PARQUET ? parquet_row_numbers ? ",file_row_number=true" : "" : ",allow_quoted_nulls=false");
        free(path);
    }
    return sql;
}
char *analysis_wrap_sql(const Analysis *a,bool parameters,char *sql) {
    if (a->query) {
        char *next=dx_printf("WITH input AS (\n%s\n)\nSELECT * FROM (\n%s\n) AS source_query",sql,a->query);
        free(sql); sql=next;
    }
    if (a->unique_rows) {
        char *next=dx_printf("SELECT DISTINCT * FROM (\n%s\n) AS unique_rows",sql);
        free(sql); sql=next;
    }
    if (a->filter_column) {
        char *condition=analysis_filter_typed(a->filter_column,a->filter_op,a->filter_value,parameters ? "$__duxcel_filter" : NULL,a->filter_number);
        char *next=dx_printf("SELECT * FROM (\n%s\n) AS filtered\nWHERE %s",sql,condition);
        free(sql); free(condition); sql=next;
    }
    for (size_t i=0;i<a->filter_count;i++) {
        const AnalysisFilter *f=&a->filters[i]; char *condition=analysis_filter_typed(f->column,f->op,f->value,NULL,f->number);
        char *next=dx_printf("SELECT * FROM (\n%s\n) AS filtered\nWHERE %s",sql,condition); free(sql); free(condition); sql=next;
    }
    if (a->group_column) {
        char *key = dx_quote(a->group_column, '"');
        char *measure = dx_quote(a->measure_column ? a->measure_column : a->group_column, '"');
        const char *fn = aggregate_label(a->aggregate);
        char *expr = a->aggregate == AGG_DISTINCT ? dx_printf("COUNT(DISTINCT %s)", measure) :
            dx_printf("%s(%s)", fn, a->aggregate == AGG_COUNT ? "*" : measure);
        /* Exclude the key spelling from the generated measure alias, including odd column names. */
        const char *alias = strcmp(a->group_column, "Value") == 0 ? "Aggregate value" : "Value";
        char *next = dx_printf("SELECT * FROM (PIVOT (\n%s\n) USING %s AS \"%s\" GROUP BY %s) AS pivot_result", sql, expr, alias, key);
        free(sql); sql = next; free(key); free(measure); free(expr);
    }
    if (a->sort_column) {
        dx_append(&sql,"\nORDER BY ");
        for (size_t i=0;i<=a->sort_count;i++) {
            AnalysisSort sort=analysis_sort_at(a,i); char *col=dx_quote(sort.column,'"');
            dx_append(&sql,"%s%s %s NULLS LAST",i ? ", " : "",col,sort.descending ? "DESC" : "ASC"); free(col);
        }
    }
    return sql;
}
char *analysis_sql(const Analysis *a,bool parameters) {
    return analysis_wrap_sql(a,parameters,analysis_source_sql(a,false));
}
char *analysis_values_json(char *const *values,size_t count) {
    size_t bytes=3;
    for (size_t i=0;i<count;i++) {
        if (!values[i]) bytes+=5;
        else { bytes+=3; for (const unsigned char *p=(const unsigned char *)values[i];*p;p++) bytes+=*p<32 ? 6 : (*p=='"'||*p=='\\' ? 2 : 1); }
    }
    char *out=dx_alloc(bytes),*p=out; *p++='[';
    for (size_t i=0;i<count;i++) {
        if (i) *p++=',';
        if (!values[i]) { memcpy(p,"null",4); p+=4; continue; }
        *p++='"';
        for (const unsigned char *s=(const unsigned char *)values[i];*s;s++) {
            if (*s<32) { snprintf(p,7,"\\u%04x",*s); p+=6; }
            else { if (*s=='"'||*s=='\\') *p++='\\'; *p++=(char)*s; }
        }
        *p++='"';
    }
    *p++=']'; *p=0; return out;
}
static void json_space(const char **s) { while (**s==' ' || **s=='\t' || **s=='\r' || **s=='\n') (*s)++; }
static bool json_hex(const char **s,unsigned *value) {
    *value=0;
    for (int i=0;i<4;i++) {
        unsigned char c=(unsigned char)**s; if (!isxdigit(c)) return false;
        *value=*value*16+(c<='9' ? c-'0' : (unsigned)(tolower(c)-'a'+10)); (*s)++;
    }
    return true;
}
bool analysis_parse_values(const char *s,char ***values,size_t *count) {
    *values=NULL; *count=0; if (!s || strlen(s)>8*1024*1024) return false;
    /* One scratch buffer; retained strings use their actual lengths, not the remaining JSON size. */
    char *buffer=dx_alloc(strlen(s)+1); size_t capacity=0; bool after_comma=false;
    json_space(&s); if (*s++!='[') goto failed;
    for (;;) {
        json_space(&s); if (*s==']') { if (after_comma) goto failed; s++; break; }
        char *value=NULL; size_t n=0;
        if (!strncmp(s,"null",4)) s+=4;
        else if (*s=='"') {
            s++;
            while (*s && *s!='"') {
                unsigned cp=(unsigned char)*s++;
                if (cp<32) goto failed;
                if (cp=='\\') {
                    char escape=*s++; if (!escape) goto failed;
                    if (escape=='u') {
                        if (!json_hex(&s,&cp) || !cp) goto failed;
                        if (cp>=0xd800 && cp<=0xdbff) {
                            unsigned low; if (*s++!='\\' || *s++!='u' || !json_hex(&s,&low) || low<0xdc00 || low>0xdfff) goto failed;
                            cp=0x10000+(cp-0xd800)*1024+low-0xdc00;
                        } else if (cp>=0xdc00 && cp<=0xdfff) goto failed;
                        if (cp>=0x10000) buffer[n++]=(char)(0xf0|(cp>>18));
                        if (cp>=0x800) buffer[n++]=(char)((cp>=0x10000 ? 0x80 : 0xe0)|((cp>>12)&0x3f));
                        if (cp>=0x80) buffer[n++]=(char)((cp>=0x800 ? 0x80 : 0xc0)|((cp>>6)&0x3f));
                        buffer[n++]=(char)(cp>=0x80 ? 0x80|(cp&0x3f) : cp); continue;
                    }
                    if (escape=='n') cp='\n'; else if (escape=='r') cp='\r'; else if (escape=='t') cp='\t';
                    else if (escape=='b') cp='\b'; else if (escape=='f') cp='\f';
                    else if (escape=='"'||escape=='\\'||escape=='/') cp=(unsigned char)escape; else goto failed;
                }
                buffer[n++]=(char)cp;
            }
            if (*s++!='"') goto failed; buffer[n]=0; value=dx_strdup(buffer);
        } else goto failed;
        if (*count==4096) { free(value); goto failed; }
        if (*count==capacity) { capacity=capacity ? capacity*2 : 16; *values=dx_realloc(*values,capacity*sizeof(char *)); }
        (*values)[(*count)++]=value;
        json_space(&s); if (*s==']') { s++; break; } if (*s++!=',') goto failed; after_comma=true;
    }
    json_space(&s); if (*s) goto failed; free(buffer); return true;
 failed:
    free(buffer); for (size_t i=0;i<*count;i++) free((*values)[i]); free(*values); *values=NULL; *count=0; return false;
}
static void values_free(char **values,size_t count) { for (size_t i=0;i<count;i++) free(values[i]); free(values); }
bool analysis_filter_value_valid(FilterOp op,const char *value) {
    if ((unsigned)op>FILTER_OP_MAX) return false;
    if (op==FILTER_COMPOSITE) {
        FilterOp list_op,condition_op; char *list=NULL,*condition=NULL;
        bool ok=analysis_filter_unpack(value,&list_op,&list,&condition_op,&condition);
        free(list); free(condition); return ok;
    }
    if (op!=FILTER_BETWEEN) return true; /* Preserve validation of legacy simple filters. */
    char **bounds=NULL; size_t count=0; bool ok=analysis_parse_values(value,&bounds,&count);
    ok=ok && count==2 && bounds[0] && bounds[1]; values_free(bounds,count); return ok;
}
bool analysis_filter_unpack(const char *payload,FilterOp *list_op,char **list,FilterOp *condition_op,char **condition) {
    *list=NULL; *condition=NULL; char **parts=NULL; size_t count=0;
    if (!analysis_parse_values(payload,&parts,&count)) return false;
    bool ok=count==4 && parts[0] && parts[1] && parts[2] && parts[3]; long lp=-1,cp=-1;
    if (ok) {
        char *end=NULL; lp=strtol(parts[0],&end,10); ok=*parts[0] && !*end && lp>=FILTER_IN && lp<=FILTER_DATE_NOT_IN;
        cp=strtol(parts[2],&end,10); ok=ok && *parts[2] && !*end && ((cp>=FILTER_CONTAINS && cp<=FILTER_NOT_NULL) || cp==FILTER_BETWEEN);
    }
    if (ok) {
        char **values=NULL; size_t n=0; ok=analysis_parse_values(parts[1],&values,&n); values_free(values,n);
        ok=ok && analysis_filter_value_valid((FilterOp)cp,parts[3]);
    }
    if (ok) { *list_op=(FilterOp)lp; *condition_op=(FilterOp)cp; *list=parts[1]; *condition=parts[3]; parts[1]=parts[3]=NULL; }
    values_free(parts,count); return ok;
}
char *analysis_filter_pack(FilterOp list_op,const char *list,FilterOp condition_op,const char *condition) {
    char lo[16],co[16]; snprintf(lo,sizeof(lo),"%d",list_op); snprintf(co,sizeof(co),"%d",condition_op);
    char *parts[]={lo,(char *)list,co,(char *)condition}; char *payload=analysis_values_json(parts,4);
    if (!analysis_filter_value_valid(FILTER_COMPOSITE,payload)) { free(payload); return NULL; } return payload;
}
typedef struct { const char *lo,*hi; size_t n; } DateRange;
static DateRange date_range(const char *key) { const char *tab=key ? strchr(key,'\t') : NULL; return (DateRange){key,tab ? tab+1 : NULL,tab ? (size_t)(tab-key) : 0}; }
static int bound_compare(const char *a,size_t na,const char *b,size_t nb) { size_t n=na<nb ? na : nb; int cmp=memcmp(a,b,n); return cmp ? cmp : (na>nb)-(na<nb); }
static bool same_key(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
bool analysis_value_checked(char *const *values,size_t count,bool all,bool dates,const char *key,bool *mixed) {
    *mixed=false; DateRange range=dates ? date_range(key) : (DateRange){0};
    if (!range.hi) { for (size_t i=0;i<count;i++) if (same_key(values[i],key)) return !all; return all; }
    const char *cursor=range.lo; size_t cursor_len=range.n; bool any=false,advanced;
    do {
        advanced=false;
        for (size_t i=0;i<count;i++) {
            DateRange part=date_range(values[i]); if (!part.hi) continue;
            if (bound_compare(part.lo,part.n,range.hi,strlen(range.hi))<0 && bound_compare(part.hi,strlen(part.hi),range.lo,range.n)>0) any=true;
            if (bound_compare(part.lo,part.n,cursor,cursor_len)<=0 && bound_compare(part.hi,strlen(part.hi),cursor,cursor_len)>0) {
                cursor=part.hi; cursor_len=strlen(cursor); advanced=true;
            }
        }
        if (bound_compare(cursor,cursor_len,range.hi,strlen(range.hi))>=0) return !all;
    } while (advanced);
    *mixed=any; return all;
}
static void add_choice(char ***values,size_t *count,char *key) { *values=dx_realloc(*values,(*count+1)*sizeof(char *)); (*values)[(*count)++]=key; }
bool analysis_toggle_value(char ***values,size_t *count,bool all,bool dates,const char *key) {
    DateRange target=dates ? date_range(key) : (DateRange){0}; bool mixed=false,checked=analysis_value_checked(*values,*count,all,dates,key,&mixed);
    if (!target.hi) {
        for (size_t i=0;i<*count;i++) if (same_key((*values)[i],key)) { free((*values)[i]); memmove(*values+i,*values+i+1,(--*count-i)*sizeof(char *)); return true; }
        size_t bytes=key ? strlen(key)+1 : 0;
        for (size_t i=0;i<*count;i++) if ((*values)[i]) bytes+=strlen((*values)[i])+1;
        if (*count>=4096 || bytes>4*1024*1024) return false; add_choice(values,count,key ? dx_strdup(key) : NULL); return true;
    }
    char **next=NULL; size_t n=0;
    for (size_t i=0;i<*count;i++) {
        DateRange part=date_range((*values)[i]);
        if (!part.hi || bound_compare(part.lo,part.n,target.hi,strlen(target.hi))>=0 || bound_compare(part.hi,strlen(part.hi),target.lo,target.n)<=0) {
            add_choice(&next,&n,(*values)[i] ? dx_strdup((*values)[i]) : NULL); continue;
        }
        if (bound_compare(part.lo,part.n,target.lo,target.n)<0) add_choice(&next,&n,dx_printf("%.*s\t%.*s",(int)part.n,part.lo,(int)target.n,target.lo));
        if (bound_compare(part.hi,strlen(part.hi),target.hi,strlen(target.hi))>0) add_choice(&next,&n,dx_printf("%s\t%s",target.hi,part.hi));
    }
    if (checked==all) add_choice(&next,&n,dx_strdup(key));
    if (n>4096) { for (size_t i=0;i<n;i++) free(next[i]); free(next); return false; }
    for (size_t i=0;i<*count;i++) free((*values)[i]); free(*values); *values=next; *count=n; return true;
}
bool analysis_toggle_values(char ***values,size_t *count,bool all,bool dates,char *const *keys,size_t key_count) {
    if (*count>4096 || key_count>4096) return false;
    bool remove=key_count>0;
    for (size_t i=0;i<key_count;i++) { bool mixed=false; remove&=analysis_value_checked(*values,*count,all,dates,keys[i],&mixed) && !mixed; }
    char **next=dx_alloc(*count*sizeof(char *)); size_t n=*count;
    for (size_t i=0;i<n;i++) next[i]=(*values)[i] ? dx_strdup((*values)[i]) : NULL;
    for (size_t i=0;i<key_count;i++) {
        bool mixed=false,checked=analysis_value_checked(next,n,all,dates,keys[i],&mixed);
        for (int pass=0;pass<2 && (checked==remove || mixed);pass++) {
            if (!analysis_toggle_value(&next,&n,all,dates,keys[i])) goto failed;
            checked=analysis_value_checked(next,n,all,dates,keys[i],&mixed);
        }
        if (checked==remove || mixed) goto failed;
    }
    size_t bytes=0; for (size_t i=0;i<n;i++) if (next[i]) bytes+=strlen(next[i])+1;
    if (bytes>4*1024*1024) goto failed;
    values_free(*values,*count); *values=next; *count=n; return true;
failed:
    values_free(next,n); return false;
}
char *analysis_page_sql(const Analysis *a, uint64_t offset, uint64_t limit) {
    char *sql = analysis_sql(a, true);
    dx_append(&sql, "\nLIMIT %" PRIu64 " OFFSET %" PRIu64, limit, offset); return sql;
}
