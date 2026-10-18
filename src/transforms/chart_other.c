#include "chart_other.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
void chart_other_plan_free(ChartOtherPlan *plan) {
    if (!plan) return;
    free(plan->sql); memset(plan,0,sizeof(*plan));
}
bool chart_other_rank(const Page *p,const PivotDefinition *d,size_t measure,ChartOtherRank *out,char **error) {
    if (!p || !d || !out) return fail(error,"Chart Other requires a captured pivot page.");
    for (int role=0;role<4;role++) if (d->counts[role]>DX_PIVOT_FIELDS) return fail(error,"Invalid Chart Other shelf arity.");
    size_t keys=d->counts[PIVOT_ROWS],measures=d->counts[PIVOT_VALUES];
    if (!measures || measure>=measures || !p->row_count || p->row_count>DX_PAGE_ROWS ||
        p->column_count<=keys || p->column_count>128 || (p->column_count-keys)%measures || !p->cells || !p->columns)
        return fail(error,"Chart Other requires a bounded numeric pivot page.");
    size_t categories=(p->column_count-keys)/measures;
    for (size_t c=0;c<categories;c++) if (!p->columns[keys+c*measures+measure].numeric)
        return fail(error,"Choose a numeric plot measure for Chart Other.");
    ChartOtherRank next={.keys=keys,.measures=measures,.categories=categories,.loaded_rows=p->row_count,
                         .offset=p->offset,.total=p->total,.truncated=p->offset || p->total>p->row_count};
    size_t rank=measure;
    for (size_t m=0;m<measures;m++) if (d->fields[PIVOT_VALUES][m].aggregate==AGG_SUM) { rank=m; break; }
    next.rank_measure=rank;
    double scale=0,scores[DX_PAGE_ROWS]={0};
    for (size_t r=0;r<p->row_count;r++) for (size_t c=0;c<categories;c++) {
        const Cell *cell=&p->cells[r*p->column_count+keys+c*measures+rank];
        if (!cell->is_null && isfinite(cell->number)) scale=fmax(scale,fabs(cell->number));
    }
    if (!scale) scale=1;
    for (size_t r=0;r<p->row_count;r++) for (size_t c=0;c<categories;c++) {
        const Cell *cell=&p->cells[r*p->column_count+keys+c*measures+rank];
        if (!cell->is_null && isfinite(cell->number)) scores[r]+=cell->number/scale;
    }
    next.top_count=p->row_count<3 ? p->row_count : 3;
    for (size_t i=0;i<next.top_count;i++) {
        size_t best=SIZE_MAX;
        for (size_t r=0;r<p->row_count;r++) {
            bool selected=false;
            for (size_t j=0;j<i;j++) selected|=next.top_rows[j]==r;
            if (!selected && (best==SIZE_MAX || fabs(scores[r])>fabs(scores[best]))) best=r;
        }
        next.top_rows[i]=best;
    }
    next.has_other=p->row_count>next.top_count; *out=next; return true;
}

typedef struct { char *data; size_t length,limit; bool valid; } Sql;
static void append(Sql *s,const char *text) {
    size_t count=strlen(text);
    if (!s->valid || count>s->limit-s->length) { s->valid=false; return; }
    if (s->data) memcpy(s->data+s->length,text,count);
    s->length+=count;
}
static void quote(Sql *s,const char *text,char delimiter) {
    char atom[2]={delimiter,0}; append(s,atom);
    for (const char *p=text;*p && s->valid;p++) {
        atom[0]=*p; append(s,atom); if (*p==delimiter) append(s,atom);
    }
    atom[0]=delimiter; append(s,atom);
}
static bool scalar_key(duckdb_type type) {
    switch (type) {
    case DUCKDB_TYPE_BOOLEAN: case DUCKDB_TYPE_TINYINT: case DUCKDB_TYPE_SMALLINT: case DUCKDB_TYPE_INTEGER:
    case DUCKDB_TYPE_BIGINT: case DUCKDB_TYPE_UTINYINT: case DUCKDB_TYPE_USMALLINT: case DUCKDB_TYPE_UINTEGER:
    case DUCKDB_TYPE_UBIGINT: case DUCKDB_TYPE_HUGEINT: case DUCKDB_TYPE_UHUGEINT: case DUCKDB_TYPE_FLOAT:
    case DUCKDB_TYPE_DOUBLE: case DUCKDB_TYPE_DECIMAL: case DUCKDB_TYPE_VARCHAR: case DUCKDB_TYPE_DATE:
    case DUCKDB_TYPE_TIME: case DUCKDB_TYPE_TIMESTAMP: case DUCKDB_TYPE_TIMESTAMP_S: case DUCKDB_TYPE_TIMESTAMP_MS:
    case DUCKDB_TYPE_TIMESTAMP_NS: case DUCKDB_TYPE_TIMESTAMP_TZ: case DUCKDB_TYPE_INTERVAL: case DUCKDB_TYPE_UUID:
    case DUCKDB_TYPE_ENUM: return true;
    default: return false;
    }
}
static bool temporal_key(duckdb_type type) {
    return type==DUCKDB_TYPE_DATE || type==DUCKDB_TYPE_TIMESTAMP || type==DUCKDB_TYPE_TIMESTAMP_S ||
           type==DUCKDB_TYPE_TIMESTAMP_MS || type==DUCKDB_TYPE_TIMESTAMP_NS || type==DUCKDB_TYPE_TIMESTAMP_TZ;
}
static bool text_budget(const char *text,size_t *used,size_t limit) {
    if (!text) return true;
    size_t remaining=limit-*used,count=0;
    while (count<remaining && text[count]) count++;
    if (count==remaining) return false;
    *used+=count+1; return true;
}
static bool selected(const ChartOtherRank *rank,size_t row) {
    for (size_t i=0;i<rank->top_count;i++) if (rank->top_rows[i]==row) return true;
    return false;
}
static bool metadata(const Analysis *a,const PivotDefinition *d,const Page *p,const ChartOtherRank *rank,
                     const duckdb_type *types,size_t type_count,size_t budget,char **error) {
    if (a->kind<=SOURCE_NONE || a->kind>SOURCE_DEMO) return fail(error,"Chart Other requires an actual source definition.");
    size_t used=analysis_text_bytes(a);
    if (used==SIZE_MAX || used>budget) return fail(error,"Chart Other source definition exceeds its SQL budget.");
    for (int role=0;role<4;role++) for (size_t i=0;i<d->counts[role];i++)
        if (!text_budget(d->fields[role][i].name,&used,budget) || !text_budget(d->fields[role][i].value,&used,budget))
            return fail(error,"Chart Other definition exceeds its SQL budget.");
    char *validation=NULL;
    bool valid=pivot_valid(d,&validation) && analysis_query_valid(a,&validation) && analysis_filters_valid(a,&validation) && analysis_sorts_valid(a,&validation);
    if (!valid) { bool result=fail(error,validation ? validation : "Invalid Chart Other definition."); free(validation); return result; }
    free(validation);
    if (type_count!=d->counts[PIVOT_COLUMNS] || (type_count && !types)) return fail(error,"Chart Other requires the captured Column key types in shelf order.");
    for (size_t k=0;k<type_count;k++) {
        if (!scalar_key(types[k])) return fail(error,"Chart Other supports scalar Column keys only.");
        if (d->fields[PIVOT_COLUMNS][k].grain && !temporal_key(types[k])) return fail(error,"Chart Other date grains require original DATE or TIMESTAMP fields.");
    }
    if (!rank->keys && rank->has_other) return fail(error,"Chart Other requires real Row keys.");
    if (!d->counts[PIVOT_COLUMNS] && rank->categories!=1) return fail(error,"Chart Other category count does not match its pivot definition.");
    if (p->count_pending) return fail(error,"Wait for the pivot page before aggregating Chart Other.");
    for (size_t k=0;k<rank->keys;k++) {
        if (!scalar_key(p->columns[k].type)) return fail(error,"Chart Other supports scalar Row keys only.");
        /* Browser wire metadata preserves unknown/nested SQL type names while
           its fallback enum is VARCHAR. Do not treat that fallback as proof. */
        if (p->columns[k].type==DUCKDB_TYPE_VARCHAR && (!p->columns[k].type_name || strcmp(p->columns[k].type_name,"VARCHAR")))
            return fail(error,"Chart Other requires a known scalar Row SQL type.");
        if (p->columns[k].date_grain!=d->fields[PIVOT_ROWS][k].grain) return fail(error,"Chart Other Row grain does not match the captured page.");
        for (size_t r=0;r<p->row_count;r++) {
            const Cell *cell=&p->cells[r*p->column_count+k];
            if ((!cell->is_null && !cell->text) || (!cell->is_null && !text_budget(cell->text,&used,budget)))
                return fail(error,"Chart Other Row keys are unavailable or exceed its SQL budget.");
        }
    }
    for (size_t c=rank->keys;c<p->column_count;c++) {
        const Column *column=&p->columns[c];
        if (column->category_count!=d->counts[PIVOT_COLUMNS] || (column->category_count && !column->categories))
            return fail(error,"Chart Other category arity does not match its pivot definition.");
        for (size_t k=0;k<column->category_count;k++) {
            if (column->category_grains[k]!=d->fields[PIVOT_COLUMNS][k].grain || !text_budget(column->categories[k],&used,budget))
                return fail(error,"Chart Other category keys do not match the captured grain or budget.");
            if (c>rank->keys && (c-rank->keys)%rank->measures) {
                const Column *first=&p->columns[c-(c-rank->keys)%rank->measures];
                if (!first->categories || (first->categories[k] && column->categories[k] ? strcmp(first->categories[k],column->categories[k]) : first->categories[k]!=column->categories[k]))
                    return fail(error,"Chart Other category order differs between measures.");
            }
        }
    }
    for (size_t r=0;r<p->row_count;r++) for (size_t previous=0;previous<r;previous++) {
        bool same=true;
        for (size_t k=0;k<rank->keys;k++) {
            const Cell *x=&p->cells[r*p->column_count+k],*y=&p->cells[previous*p->column_count+k];
            if (x->is_null!=y->is_null || (!x->is_null && strcmp(x->text,y->text))) { same=false; break; }
        }
        if (same) return fail(error,"Chart Other requires distinct canonical loaded Row tuples.");
    }
    return true;
}
static void predicate(Sql *sql,const char *key,const char *value,bool is_null) {
    quote(sql,key,'"'); append(sql," IS NOT DISTINCT FROM ");
    if (is_null) append(sql,"NULL");
    else { append(sql,"cast_to_type("); quote(sql,value,'\''); append(sql,","); quote(sql,key,'"'); append(sql,")"); }
}
static void generate(Sql *sql,const char *input,const PivotDefinition *d,const Page *p,const ChartOtherRank *rank,char *const *rows,char *const *columns) {
    append(sql,"WITH __dx_chart_input AS ("); append(sql,input); append(sql,"), __dx_chart_members AS (SELECT * FROM __dx_chart_input WHERE ");
    bool first=true;
    for (size_t r=0;r<p->row_count;r++) if (!selected(rank,r)) {
        append(sql,first ? "(" : " OR ("); first=false;
        for (size_t k=0;k<rank->keys;k++) {
            const Cell *cell=&p->cells[r*p->column_count+k]; if (k) append(sql," AND "); predicate(sql,rows[k],cell->text,cell->is_null);
        }
        append(sql,")");
    }
    append(sql,") SELECT ");
    for (size_t c=rank->keys;c<p->column_count;c++) {
        size_t m=(c-rank->keys)%rank->measures; const PivotField *field=&d->fields[PIVOT_VALUES][m];
        if (c>rank->keys) append(sql,",");
        append(sql,field->aggregate==AGG_DISTINCT ? "COUNT(DISTINCT " : aggregate_label(field->aggregate));
        if (field->aggregate!=AGG_DISTINCT) append(sql,"(");
        if (field->aggregate==AGG_COUNT) append(sql,"*"); else quote(sql,field->name,'"');
        append(sql,")");
        if (d->counts[PIVOT_COLUMNS]) {
            append(sql," FILTER (WHERE ");
            for (size_t k=0;k<d->counts[PIVOT_COLUMNS];k++) {
                if (k) append(sql," AND "); predicate(sql,columns[k],p->columns[c].categories[k],!p->columns[c].categories[k]);
            }
            append(sql,")");
        }
        char ordinal[40]; snprintf(ordinal,sizeof(ordinal)," AS __dx_other_%zu",c-rank->keys); append(sql,ordinal);
    }
    append(sql," FROM __dx_chart_members");
}
static bool owned_add(size_t *used,size_t bytes,size_t limit) {
    if (bytes>limit || *used>limit-bytes) return false;
    *used+=bytes; return true;
}
bool chart_other_plan_retained(const Analysis *a,const PivotDefinition *d,const Page *p,size_t measure,
                      const duckdb_type *types,size_t type_count,size_t budget,size_t retained,size_t owned_limit,ChartOtherPlan *out,char **error) {
    if (!a || !out || !budget) return fail(error,"Chart Other requires an output plan and SQL budget.");
    size_t used=retained,old_bytes=out->sql ? strlen(out->sql) : 0;
    if ((out->sql && old_bytes==SIZE_MAX) || !owned_add(&used,out->sql ? old_bytes+1 : 0,owned_limit))
        return fail(error,"Chart Other retained ownership exceeds its construction limit.");
    if (budget>DX_CHART_OTHER_SQL_BYTES) budget=DX_CHART_OTHER_SQL_BYTES;
    ChartOtherPlan next={0};
    if (!chart_other_rank(p,d,measure,&next.rank,error) || !metadata(a,d,p,&next.rank,types,type_count,budget,error)) return false;
    if (!next.rank.has_other) { chart_other_plan_free(out); *out=next; return true; }
    if (used==owned_limit) return fail(error,"Chart Other construction exceeds its owned-buffer limit.");
    char *input=pivot_input_sql(a,d),*rows[DX_PIVOT_FIELDS]={0},*columns[DX_PIVOT_FIELDS]={0};
    bool admitted=owned_add(&used,strlen(input)+1,owned_limit);
    for (size_t k=0;admitted && k<next.rank.keys;k++) {
        rows[k]=pivot_field_key(d,PIVOT_ROWS,k); admitted=owned_add(&used,strlen(rows[k])+1,owned_limit);
    }
    for (size_t k=0;admitted && k<d->counts[PIVOT_COLUMNS];k++) {
        columns[k]=pivot_field_key(d,PIVOT_COLUMNS,k); admitted=owned_add(&used,strlen(columns[k])+1,owned_limit);
    }
    if (!admitted) {
        free(input); for (size_t k=0;k<DX_PIVOT_FIELDS;k++) { free(rows[k]); free(columns[k]); }
        return fail(error,"Chart Other construction exceeds its owned-buffer limit.");
    }
    Sql sql={.limit=budget-1,.valid=true}; generate(&sql,input,d,p,&next.rank,rows,columns);
    if (sql.valid && (admitted=owned_add(&used,sql.length+1,owned_limit))) {
        next.sql=dx_alloc(sql.length+1); next.sql_bytes=sql.length+1;
        sql.data=next.sql; sql.length=0; generate(&sql,input,d,p,&next.rank,rows,columns); next.sql[sql.length]=0;
    }
    free(input);
    for (size_t k=0;k<DX_PIVOT_FIELDS;k++) { free(rows[k]); free(columns[k]); }
    if (!admitted) { chart_other_plan_free(&next); return fail(error,"Chart Other construction exceeds its owned-buffer limit."); }
    if (!sql.valid) { chart_other_plan_free(&next); return fail(error,"Chart Other SQL exceeds its memory limit."); }
    chart_other_plan_free(out); *out=next; return true;
}
bool chart_other_plan(const Analysis *a,const PivotDefinition *d,const Page *p,size_t measure,
                      const duckdb_type *types,size_t type_count,size_t budget,ChartOtherPlan *out,char **error) {
    return chart_other_plan_retained(a,d,p,measure,types,type_count,budget,0,SIZE_MAX,out,error);
}
