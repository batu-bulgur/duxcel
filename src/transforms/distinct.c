#include "distinct.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool bounded(const char *text,size_t maximum) {
    if (!text) return true;
    size_t size=0; while (size<=maximum && text[size]) size++;
    return size<=maximum;
}
void distinct_query_free(DistinctQuery *query) {
    if (!query) return;
    free(query->relation); memset(query,0,sizeof(*query));
}
bool distinct_query_build(DistinctQuery *out,const Analysis *a,const PivotDefinition *context,
    const char *column,const char *search,int level,const char *start,const char *end,bool parameters,char **error) {
    if (level < -1 || level>4) return fail(error,"Invalid date hierarchy level");
    if (!out || !a || a->kind<SOURCE_CSV || a->kind>SOURCE_DEMO || !column || !*column ||
        !bounded(column,4096) || !bounded(search,4*1024*1024) || !bounded(start,4096) || !bounded(end,4096) ||
        !bounded(a->group_column,4096) || !bounded(a->measure_column,4096) || (a->group_column && (unsigned)a->aggregate>AGG_MAX))
        return fail(error,"Invalid available-value source or field.");
    if (!analysis_query_valid(a,error) || !analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error)) return false;
    if (context) {
        /* pivot_valid's Rows comparison visits Columns before validating it. */
        for (size_t r=0;r<4;r++) if (context->counts[r]>DX_PIVOT_FIELDS) return fail(error,"Invalid pivot shelf count.");
        for (size_t r=0;r<4;r++) for (size_t i=0;i<context->counts[r];i++) if (!context->fields[r][i].name) return fail(error,"Invalid pivot field definition.");
        char *validation=NULL; bool valid=pivot_valid(context,&validation);
        if (!valid) fail(error,validation); free(validation); if (!valid) return false;
    }
    Analysis source=*a; if (!context) { source.group_column=NULL; source.measure_column=NULL; } source.sort_column=NULL; source.sort_count=0;
    if (!context) {
        if (source.filter_column && !strcmp(source.filter_column,column)) source.filter_column=NULL;
        source.filter_count=0; for (size_t i=0;i<a->filter_count;i++) if (strcmp(a->filters[i].column,column)) source.filters[source.filter_count++]=a->filters[i];
    }
    char *base=analysis_sql(&source,parameters),*col=dx_quote(column,'"'),*find=dx_quote(search ? search : "",'\'');
    if (context) for (size_t i=0;i<context->counts[PIVOT_FILTERS];i++) {
        const PivotField *f=&context->fields[PIVOT_FILTERS][i]; if (!strcmp(f->name,column)) continue;
        char *condition=analysis_filter_typed(f->name,f->filter,f->value,NULL,f->number),*next=dx_printf("SELECT * FROM (%s) pivot_available WHERE %s",base,condition);
        free(base); free(condition); base=next;
    }
    char *where=dx_printf("contains(COALESCE(CAST(%s AS VARCHAR), 'NULL'), %s)",col,find);
    if (start && *start && end && *end) {
        char *lo=dx_quote(start,'\''),*hi=dx_quote(end,'\'');
        dx_append(&where," AND %s >= cast_to_type(%s,%s) AND %s < cast_to_type(%s,%s)",col,lo,col,col,hi,col); free(lo); free(hi);
    }
    const char *units[]={"year","month","day","hour","minute"};
    const char *formats[]={"%Y","%B","%d","%H:00","%H:%M"};
    char *value=level<0 ? dx_printf("CAST(%s AS VARCHAR)",col) : dx_printf("CAST(date_trunc('%s',%s) AS VARCHAR)",units[level],col);
    char *label=level<0 ? dx_strdup(value) : dx_printf("strftime(date_trunc('%s',%s),'%s')",units[level],col,formats[level]);
    char *upper=level<0 ? dx_strdup("NULL::VARCHAR") : dx_printf("CAST(date_trunc('%s',%s)+INTERVAL '1 %s' AS VARCHAR)",units[level],col,units[level]);
    DistinctQuery query={.relation=dx_printf("SELECT DISTINCT %s AS value,%s AS label,%s AS upper_bound FROM (%s) available WHERE %s",value,label,upper,base,where),.keep_primary_filter=source.filter_column!=NULL};
    free(base); free(col); free(find); free(where); free(value); free(label); free(upper);
    distinct_query_free(out); *out=query; return true;
}
char *distinct_page_sql(const DistinctQuery *query,uint64_t offset) {
    return query && query->relation ? dx_printf("SELECT * FROM (%s) distinct_values ORDER BY value NULLS FIRST LIMIT %d OFFSET %" PRIu64,query->relation,DX_DISTINCT_QUERY_ROWS,offset) : NULL;
}
char *distinct_count_sql(const DistinctQuery *query) {
    return query && query->relation ? dx_printf("SELECT count(*) FROM (%s) counted",query->relation) : NULL;
}
