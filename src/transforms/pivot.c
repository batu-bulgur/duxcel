#include "pivot.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
static bool fail(char **error,const char *text) { free(*error); *error=dx_strdup(text); return false; }
static void field_free(PivotField *f) { free(f->name); free(f->value); memset(f,0,sizeof(*f)); }
void pivot_free(PivotDefinition *p) {
    for (int r=0;r<4;r++) for (size_t i=0;i<p->counts[r] && i<DX_PIVOT_FIELDS;i++) field_free(&p->fields[r][i]);
    memset(p,0,sizeof(*p));
}
PivotDefinition pivot_copy(const PivotDefinition *p) {
    PivotDefinition out=*p;
    for (int r=0;r<4;r++) for (size_t i=0;i<p->counts[r];i++) {
        out.fields[r][i].name=dx_strdup(p->fields[r][i].name);
        out.fields[r][i].value=p->fields[r][i].value ? dx_strdup(p->fields[r][i].value) : NULL;
    }
    return out;
}
void pivot_remove(PivotDefinition *p,PivotRole role,size_t index) {
    if ((unsigned)role>PIVOT_FILTERS || index>=p->counts[role]) return;
    field_free(&p->fields[role][index]);
    memmove(&p->fields[role][index],&p->fields[role][index+1],(--p->counts[role]-index)*sizeof(PivotField));
    memset(&p->fields[role][p->counts[role]],0,sizeof(PivotField));
}
bool pivot_valid(const PivotDefinition *p,char **error) {
    for (int r=0;r<4;r++) {
        if (p->counts[r]>DX_PIVOT_FIELDS) return fail(error,"Use up to eight fields in each pivot shelf.");
        for (size_t i=0;i<p->counts[r];i++) {
            const PivotField *f=&p->fields[r][i];
            if (!f->name || !*f->name || strlen(f->name)>4096 || (unsigned)f->aggregate>AGG_MAX || (unsigned)f->filter>FILTER_OP_MAX || (unsigned)f->grain>PIVOT_DATE_MONTH || (r>=PIVOT_VALUES && f->grain!=PIVOT_DATE_EXACT) || (f->value && strlen(f->value)>8*1024*1024) || !analysis_filter_value_valid(f->filter,f->value)) return fail(error,"Invalid pivot field definition.");
            for (size_t j=0;j<i;j++) if (!strcmp(f->name,p->fields[r][j].name) && f->grain==p->fields[r][j].grain && (r!=PIVOT_VALUES || f->aggregate==p->fields[r][j].aggregate)) return fail(error,"That field is already on this shelf.");
            if (r==PIVOT_ROWS) for (size_t j=0;j<p->counts[PIVOT_COLUMNS];j++) if (!strcmp(f->name,p->fields[PIVOT_COLUMNS][j].name) && f->grain==p->fields[PIVOT_COLUMNS][j].grain) return fail(error,"A field and date grain can be on Rows or Columns at a time.");
        }
    }
    return true;
}
bool pivot_assign(PivotDefinition *p,PivotRole role,const char *name,Aggregate aggregate,char **error) {
    return pivot_assign_grain(p,role,name,aggregate,PIVOT_DATE_EXACT,error);
}
bool pivot_assign_grain(PivotDefinition *p,PivotRole role,const char *name,Aggregate aggregate,PivotDateGrain grain,char **error) {
    if ((unsigned)role>PIVOT_FILTERS || !name || !*name || strlen(name)>4096 || (unsigned)aggregate>AGG_MAX || (unsigned)grain>PIVOT_DATE_MONTH || (role>=PIVOT_VALUES && grain!=PIVOT_DATE_EXACT)) return fail(error,"Invalid pivot assignment.");
    for (size_t i=0;i<p->counts[role];i++) if (!strcmp(p->fields[role][i].name,name) && p->fields[role][i].grain==grain && (role!=PIVOT_VALUES || p->fields[role][i].aggregate==aggregate)) return true;
    if (p->counts[role]==DX_PIVOT_FIELDS) return fail(error,"Use up to eight fields in each shelf.");
    char *owned=dx_strdup(name);
    if (role==PIVOT_ROWS || role==PIVOT_COLUMNS) {
        PivotRole other=role==PIVOT_ROWS ? PIVOT_COLUMNS : PIVOT_ROWS;
        for (size_t i=0;i<p->counts[other];i++) if (!strcmp(p->fields[other][i].name,name) && p->fields[other][i].grain==grain) { pivot_remove(p,other,i); break; }
    }
    p->fields[role][p->counts[role]++]=(PivotField){.name=owned,.aggregate=aggregate,.grain=grain,.filter=role==PIVOT_FILTERS ? FILTER_NOT_IN : FILTER_NOT_NULL,.value=role==PIVOT_FILTERS ? dx_strdup("[]") : NULL}; return true;
}
bool pivot_set_grain(PivotDefinition *p,PivotRole role,size_t index,PivotDateGrain grain,char **error) {
    if ((unsigned)role>PIVOT_COLUMNS || index>=p->counts[role] || (unsigned)grain>PIVOT_DATE_MONTH) return fail(error,"Choose a date field on Rows or Columns.");
    PivotDateGrain old=p->fields[role][index].grain; p->fields[role][index].grain=grain;
    if (!pivot_valid(p,error)) { p->fields[role][index].grain=old; return false; }
    return true;
}
static bool identifier_same(const char *a,const char *b) {
    while (*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { a++; b++; }
    return !*a && !*b;
}
char *pivot_field_key(const PivotDefinition *p,PivotRole role,size_t index) {
    const PivotField *f=&p->fields[role][index]; if (!f->grain) return dx_strdup(f->name);
    for (unsigned suffix=0;;suffix++) {
        char *key=dx_printf("__duxcel_date_%u_%zu_%u",(unsigned)role,index,suffix); bool collision=false;
        for (int r=0;r<4;r++) for (size_t i=0;i<p->counts[r];i++) collision|=identifier_same(key,p->fields[r][i].name);
        if (!collision) return key; free(key);
    }
}
char *pivot_field_label(const PivotField *f) {
    static const char *names[]={"","YEAR","QUARTER","MONTH"};
    return f->grain ? dx_printf("%s(%s)",names[f->grain],f->name) : dx_strdup(f->name);
}
void pivot_date_label(PivotDateGrain grain,const char *raw,char output[64]) {
    static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    unsigned year=0,month=0,day=0; char end=0;
    if (grain==PIVOT_DATE_MONTH && raw && sscanf(raw,"%u-%u-%u%c",&year,&month,&day,&end)==3 && month>=1 && month<=12 && day==1)
        snprintf(output,64,"%s %u",months[month-1],year);
    else snprintf(output,64,"%s",raw ? raw : "");
}
bool pivot_move(PivotDefinition *p,PivotRole from,size_t index,PivotRole to,size_t destination,char **error) {
    if ((unsigned)from>PIVOT_FILTERS || (unsigned)to>PIVOT_FILTERS || index>=p->counts[from]) return fail(error,"Invalid pivot field move.");
    PivotDefinition copy=pivot_copy(p); PivotField field=copy.fields[from][index];
    char *name=dx_strdup(field.name),*value=field.value ? dx_strdup(field.value) : NULL;
    Aggregate agg=field.aggregate; FilterOp filter=field.filter; NumberType number=field.number;
    PivotDateGrain grain=to<PIVOT_VALUES ? field.grain : PIVOT_DATE_EXACT;
    pivot_remove(&copy,from,index);
    if (!pivot_assign_grain(&copy,to,name,agg,grain,error)) { free(name); free(value); pivot_free(&copy); return false; }
    size_t last=copy.counts[to]-1; for (size_t i=0;i<copy.counts[to];i++) if (!strcmp(copy.fields[to][i].name,name) && copy.fields[to][i].grain==grain && (to!=PIVOT_VALUES || copy.fields[to][i].aggregate==agg)) { last=i; break; }
    free(name);
    if (from==PIVOT_FILTERS && to==PIVOT_FILTERS) { free(copy.fields[to][last].value); copy.fields[to][last].value=value; value=NULL; copy.fields[to][last].filter=filter; copy.fields[to][last].number=number; }
    free(value); field=copy.fields[to][last];
    memmove(&copy.fields[to][last],&copy.fields[to][last+1],(copy.counts[to]-last-1)*sizeof(field));
    if (destination>=copy.counts[to]) destination=copy.counts[to]-1;
    memmove(&copy.fields[to][destination+1],&copy.fields[to][destination],(copy.counts[to]-destination-1)*sizeof(field)); copy.fields[to][destination]=field;
    pivot_free(p); *p=copy; return true;
}
char *pivot_input_sql(const Analysis *source,const PivotDefinition *p) {
    Analysis unordered=*source; unordered.sort_column=NULL; unordered.sort_count=0;
    char *base=analysis_sql(&unordered,false),*project=dx_strdup("");
    for (int r=0;r<3;r++) for (size_t i=0;i<p->counts[r];i++) {
        const PivotField *f=&p->fields[r][i]; bool exists=false;
        for (int rr=0;rr<=r;rr++) for (size_t j=0;j<(rr==r ? i : p->counts[rr]);j++) if (!f->grain && !p->fields[rr][j].grain && !strcmp(f->name,p->fields[rr][j].name)) exists=true;
        if (!exists) {
            char *col=dx_quote(f->name,'"'); const char *separator=*project ? "," : "";
            if (f->grain) {
                char *key=pivot_field_key(p,(PivotRole)r,i),*alias=dx_quote(key,'"');
                if (f->grain==PIVOT_DATE_YEAR) dx_append(&project,"%syear(%s) AS %s",separator,col,alias);
                else if (f->grain==PIVOT_DATE_QUARTER) dx_append(&project,"%s('Q'||quarter(%s)::VARCHAR) AS %s",separator,col,alias);
                else dx_append(&project,"%sdate_trunc('month',%s)::DATE AS %s",separator,col,alias);
                free(key); free(alias);
            } else dx_append(&project,"%s%s",separator,col);
            free(col);
        }
    }
    if (!*project) dx_append(&project,"1 AS _row");
    char *input=dx_printf("SELECT %s FROM (%s) pivot_source",project,base); free(project); free(base);
    for (size_t i=0;i<p->counts[PIVOT_FILTERS];i++) {
        const PivotField *f=&p->fields[PIVOT_FILTERS][i]; char *condition=analysis_filter_typed(f->name,f->filter,f->value,NULL,f->number);
        dx_append(&input,"%s %s",i ? " AND" : " WHERE",condition); free(condition);
    }
    return input;
}
char *pivot_sql(const Analysis *source,const PivotDefinition *p) { return pivot_sql_domains(source,p,NULL); }
char *pivot_totals_sql(const Analysis *source,const PivotDefinition *p) {
    size_t rows=p->counts[PIVOT_ROWS],columns=p->counts[PIVOT_COLUMNS],dims=rows+columns;
    char *input=pivot_input_sql(source,p),*project=dx_strdup(""),*select=dx_strdup(""),*groups=dx_strdup("");
    for (size_t d=0;d<dims;d++) {
        PivotRole role=d<rows ? PIVOT_ROWS : PIVOT_COLUMNS; size_t i=d<rows ? d : d-rows;
        char *key=pivot_field_key(p,role,i),*quoted=dx_quote(key,'"');
        dx_append(&project,"%s%s AS d%zu",*project ? "," : "",quoted,d);
        dx_append(&select,"%sd%zu",*select ? "," : "",d);
        dx_append(&groups,"%sd%zu",*groups ? "," : "",d); free(key); free(quoted);
    }
    for (size_t i=0;i<p->counts[PIVOT_VALUES];i++) {
        const PivotField *field=&p->fields[PIVOT_VALUES][i]; char *quoted=dx_quote(field->name,'"');
        dx_append(&project,"%s%s AS v%zu",*project ? "," : "",quoted,i); free(quoted);
        dx_append(&select,"%s%s(%s",*select ? "," : "",field->aggregate==AGG_DISTINCT ? "COUNT" : aggregate_label(field->aggregate),field->aggregate==AGG_DISTINCT ? "DISTINCT " : "");
        if (field->aggregate==AGG_COUNT) dx_append(&select,"*"); else dx_append(&select,"v%zu",i);
        dx_append(&select,") AS m%zu",i);
    }
    if (dims) dx_append(&select,",GROUPING_ID(%s) AS mask",groups); else dx_append(&select,",0::BIGINT AS mask");
    char *sql=dx_printf("SELECT %s FROM (SELECT %s FROM (%s) totals_source) totals_input",select,project,input);
    if (dims) {
        dx_append(&sql," GROUP BY GROUPING SETS (");
        if (rows) { dx_append(&sql,"("); for (size_t i=0;i<rows;i++) dx_append(&sql,"%sd%zu",i ? "," : "",i); dx_append(&sql,"),"); }
        if (columns) { dx_append(&sql,"("); for (size_t i=0;i<columns;i++) dx_append(&sql,"%sd%zu",i ? "," : "",rows+i); dx_append(&sql,"),"); }
        dx_append(&sql,"())");
    }
    free(input); free(project); free(select); free(groups); return sql;
}
char *pivot_sql_domains(const Analysis *source,const PivotDefinition *p,char *const *domains) {
    char *input=pivot_input_sql(source,p),*sql=dx_printf("PIVOT (%s)",input); free(input);
    if (p->counts[PIVOT_COLUMNS]) {
        dx_append(&sql," ON ");
        for (size_t i=0;i<p->counts[PIVOT_COLUMNS];i++) { char *key=pivot_field_key(p,PIVOT_COLUMNS,i),*col=dx_quote(key,'"'); dx_append(&sql,"%s%s",i ? "," : "",col); if (domains && domains[i]) dx_append(&sql," IN (%s)",domains[i]); free(key); free(col); }
    }
    dx_append(&sql," USING ");
    for (size_t i=0;i<p->counts[PIVOT_VALUES];i++) {
        const PivotField *f=&p->fields[PIVOT_VALUES][i]; char *col=dx_quote(f->name,'"'); char *label=aggregate_measure_label(f->aggregate,f->name),*alias=dx_quote(label,'"');
        dx_append(&sql,"%s%s(%s%s) AS %s",i ? "," : "",f->aggregate==AGG_DISTINCT ? "COUNT" : aggregate_label(f->aggregate),f->aggregate==AGG_DISTINCT ? "DISTINCT " : "",f->aggregate==AGG_COUNT ? "*" : col,alias);
        free(col); free(label); free(alias);
    }
    if (p->counts[PIVOT_ROWS]) {
        dx_append(&sql," GROUP BY ");
        for (size_t i=0;i<p->counts[PIVOT_ROWS];i++) { char *key=pivot_field_key(p,PIVOT_ROWS,i),*col=dx_quote(key,'"'); dx_append(&sql,"%s%s",i ? "," : "",col); free(key); free(col); }
    }
    char *ordered=dx_printf("SELECT * FROM (%s) pivot_result",sql); free(sql);
    if (p->counts[PIVOT_ROWS]) {
        dx_append(&ordered," ORDER BY ");
        for (size_t i=0;i<p->counts[PIVOT_ROWS];i++) { char *key=pivot_field_key(p,PIVOT_ROWS,i),*col=dx_quote(key,'"'); dx_append(&ordered,"%s%s NULLS FIRST",i ? "," : "",col); free(key); free(col); }
    }
    return ordered;
}
