#include "browser_query.h"
#include "page_wire.h"
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
static bool text_size(const char *text,size_t limit,size_t *size) {
    if (!text) return false;
    size_t length=0; while (length<=limit && text[length]) length++;
    if (length>limit) return false;
    if (size) *size=length; return true;
}
static bool text_budget(const char *text,size_t limit,size_t *budget) {
    if (!text) return true;
    size_t size; if (!text_size(text,limit,&size) || size+1>DX_BROWSER_SQL_BYTES-*budget) return false;
    *budget+=size+1; return true;
}
static bool number_valid(NumberType type) {
    return (unsigned)type.kind<=NUM_FLOAT && (type.kind==NUM_DECIMAL ? type.precision && type.precision<=38 && type.scale<=type.precision : !type.precision && !type.scale);
}
static bool same_name(const char *left,const char *right) {
    while (*left && *right) {
        unsigned char a=(unsigned char)*left++,b=(unsigned char)*right++;
        if (a>='A' && a<='Z') a=(unsigned char)(a-'A'+'a');
        if (b>='A' && b<='Z') b=(unsigned char)(b-'A'+'a');
        if (a!=b) return false;
    }
    return *left==*right;
}
static char *bounded_sql(char *sql,char **error) {
    if (!text_size(sql,DX_BROWSER_SQL_BYTES,NULL)) { free(sql); fail(error,"Browser query exceeds 1 MiB. Use fewer fields or filter values."); return NULL; }
    return sql;
}
static bool relation_valid(const char *relation,char **error) {
    size_t size;
    if (!text_size(relation,DX_BROWSER_SQL_BYTES,&size) || !size) return fail(error,"A nonempty browser relation of up to 1 MiB is required.");
    for (size_t i=0;i<size;i++) if (!isspace((unsigned char)relation[i])) return true;
    return fail(error,"Choose a nonempty browser relation.");
}
char *browser_relation_sql(const Analysis *a,const PivotDefinition *pivot,char **error) {
    if (!a || (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET && a->kind!=SOURCE_DEMO)) {
        fail(error,"Choose CSV, Parquet or demo data. Attached database files are not available in the browser proof."); return NULL;
    }
    if (a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || (unsigned)a->aggregate>AGG_MAX ||
        (a->kind!=SOURCE_DEMO && (!a->path || !*a->path))) { fail(error,"Invalid browser source definition."); return NULL; }
    size_t budget=0;
    const char *names[]={a->path,a->schema,a->table,a->filter_column,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<sizeof(names)/sizeof(*names);i++) if (!text_budget(names[i],4096,&budget)) { fail(error,"Browser source names exceed their text limit."); return NULL; }
    if ((a->group_column && !*a->group_column) || (a->measure_column && !*a->measure_column) ||
        !text_budget(a->query,DX_QUERY_BYTES,&budget) || !text_budget(a->filter_value,DX_BROWSER_SQL_BYTES,&budget) || !number_valid(a->filter_number)) {
        fail(error,"Invalid browser query, aggregation or filter metadata."); return NULL;
    }
    for (size_t i=0;i<a->filter_count;i++) if (!text_budget(a->filters[i].column,4096,&budget) ||
        !text_budget(a->filters[i].value,DX_BROWSER_SQL_BYTES,&budget) || !number_valid(a->filters[i].number)) {
        fail(error,"Browser filters exceed their text limit."); return NULL;
    }
    for (size_t i=0;i<a->sort_count;i++) if (!text_budget(a->sorts[i].column,4096,&budget)) { fail(error,"Browser sorts exceed their text limit."); return NULL; }
    char *validation=NULL;
    bool valid=analysis_query_valid(a,&validation) && analysis_filters_valid(a,&validation) && analysis_sorts_valid(a,&validation);
    if (valid && pivot) {
        /* pivot_valid compares Rows against Columns before visiting Columns. */
        for (size_t r=0;r<4;r++) if (pivot->counts[r]>DX_PIVOT_FIELDS) valid=false;
        if (!pivot->counts[PIVOT_VALUES]) valid=false;
        for (size_t r=0;valid && r<4;r++) for (size_t i=0;i<pivot->counts[r];i++) {
            const PivotField *field=&pivot->fields[r][i];
            if (!field->name || !*field->name || !text_budget(field->name,4096,&budget) || !text_budget(field->value,DX_BROWSER_SQL_BYTES,&budget) || !number_valid(field->number)) valid=false;
        }
        if (valid) valid=pivot_valid(pivot,&validation);
    }
    if (!valid) { fail(error,validation ? validation : "Invalid browser pivot definition or text limit."); free(validation); return NULL; }
    free(validation); return bounded_sql(pivot ? pivot_sql(a,pivot) : analysis_sql(a,false),error);
}
char *browser_describe_sql(const char *relation,char **error) {
    if (!relation_valid(relation,error)) return NULL;
    return bounded_sql(dx_printf("DESCRIBE SELECT * FROM (%s) AS _duxcel_relation",relation),error);
}
char *browser_count_sql(const char *relation,char **error) {
    if (!relation_valid(relation,error)) return NULL;
    return bounded_sql(dx_printf("SELECT COUNT(*)::UBIGINT AS \"__duxcel_count\" FROM (%s) AS _duxcel_relation",relation),error);
}
static bool schema_names_valid(const Page *schema,char **error) {
    if (!schema || !schema->column_count || schema->column_count>DX_PAGE_WIRE_COLUMNS || !schema->columns) return fail(error,"Browser queries require one to 256 described columns.");
    for (size_t i=0;i<schema->column_count;i++) {
        const char *name=schema->columns[i].name;
        if (!text_size(name,4096,NULL) || !*name) return fail(error,"Invalid browser column name.");
        for (size_t j=0;j<i;j++) if (same_name(name,schema->columns[j].name)) return fail(error,"Duplicate browser column names require distinct SQL aliases.");
    }
    return true;
}
bool browser_schema_from_describe(Page *out,const Page *describe,char **error) {
    static const char *headers[]={"column_name","column_type","null","key","default","extra"};
    if (!out || !describe || describe->column_count!=6 || describe->row_count>DX_PAGE_WIRE_COLUMNS || !describe->columns ||
        (describe->row_count && !describe->cells) || describe->offset || describe->total!=describe->row_count) return fail(error,"Expected a complete six-column DESCRIBE reply.");
    for (size_t i=0;i<6;i++) if (!text_size(describe->columns[i].name,32,NULL) || !same_name(describe->columns[i].name,headers[i])) return fail(error,"Unexpected DESCRIBE reply headers.");
    duckdb_type types[DX_PAGE_WIRE_COLUMNS]; NumberType numbers[DX_PAGE_WIRE_COLUMNS];
    for (size_t row=0;row<describe->row_count;row++) {
        const Cell *name=&describe->cells[row*6],*type=&describe->cells[row*6+1];
        if (name->is_null || type->is_null || !text_size(name->text,4096,NULL) || !*name->text ||
            !page_type_from_sql(type->text,&types[row],&numbers[row])) return fail(error,"Invalid DESCRIBE column name or SQL type.");
        for (size_t previous=0;previous<row;previous++) if (same_name(name->text,describe->cells[previous*6].text)) return fail(error,"Duplicate browser column names require distinct SQL aliases.");
    }
    Page schema={.column_count=describe->row_count};
    if (schema.column_count) schema.columns=dx_alloc(schema.column_count*sizeof(*schema.columns));
    schema.bytes=schema.column_count*sizeof(*schema.columns);
    for (size_t i=0;i<schema.column_count;i++) {
        Column *column=&schema.columns[i]; column->name=dx_strdup(describe->cells[i*6].text); column->type_name=dx_strdup(describe->cells[i*6+1].text);
        column->type=types[i]; column->number_type=numbers[i]; column->numeric=numbers[i].kind!=NUM_NONE;
        schema.bytes+=strlen(column->name)+strlen(column->type_name)+2;
    }
    page_free(out); *out=schema; return true;
}
char *browser_page_sql(const char *relation,const Page *schema,uint64_t offset,char **error) {
    if (!relation_valid(relation,error) || !schema_names_valid(schema,error)) return NULL;
    if (offset>UINT64_MAX-DX_PAGE_ROWS) { fail(error,"Browser page interval overflows uint64."); return NULL; }
    char *sql=dx_strdup("SELECT ");
    for (size_t i=0;i<schema->column_count;i++) {
        char *name=dx_quote(schema->columns[i].name,'"'); dx_append(&sql,"%sCAST(%s AS VARCHAR) AS %s",i ? "," : "",name,name); free(name);
        if (strlen(sql)>DX_BROWSER_SQL_BYTES) return bounded_sql(sql,error);
    }
    dx_append(&sql," FROM (%s) AS _duxcel_relation LIMIT %d OFFSET %" PRIu64,relation,DX_PAGE_ROWS,offset);
    return bounded_sql(sql,error);
}
static bool pivot_shape_all(const PivotDefinition *p,char **error) {
    if (!p) return fail(error,"Choose a valid matrix definition.");
    for (size_t role=0;role<4;role++) if (p->counts[role]>DX_PIVOT_FIELDS) return fail(error,"Matrix shelves exceed their bounded field limit.");
    if (!p->counts[PIVOT_VALUES]) return fail(error,"Add a field to Values to build the matrix.");
    size_t budget=0;
    for (size_t role=0;role<4;role++) for (size_t i=0;i<p->counts[role];i++) {
        const PivotField *f=&p->fields[role][i];
        if (!f->name || !*f->name || !text_budget(f->name,4096,&budget) || !text_budget(f->value,DX_BROWSER_SQL_BYTES,&budget) || !number_valid(f->number)) return fail(error,"Invalid browser matrix field metadata.");
    }
    return pivot_valid(p,error);
}
static bool pivot_shape(const PivotDefinition *p,char **error) {
    if (!pivot_shape_all(p,error)) return false;
    return !p->counts[PIVOT_COLUMNS] || fail(error,"Browser matrices currently support Rows and Values. Column-field pivots are not connected yet.");
}
char *browser_pivot_totals_sql(const Analysis *source,const PivotDefinition *p,char **error) {
    if (!pivot_shape(p,error)) return NULL;
    char *validated=browser_relation_sql(source,p,error); if (!validated) return NULL; free(validated);
    char *base=pivot_totals_sql(source,p),*sql=dx_strdup("SELECT ");
    for (size_t i=0;i<p->counts[PIVOT_VALUES];i++) dx_append(&sql,"%sCAST(m%zu AS VARCHAR) AS total_%zu",i ? "," : "",i,i);
    dx_append(&sql," FROM (%s) AS _duxcel_matrix_totals WHERE mask=%" PRIu64 " LIMIT 1",base,(UINT64_C(1)<<p->counts[PIVOT_ROWS])-1); free(base);
    return bounded_sql(sql,error);
}
static bool same_number(NumberType a,NumberType b) { return a.kind==b.kind && a.precision==b.precision && a.scale==b.scale; }
static bool pivot_schema_valid(const Page *schema,const PivotDefinition *p,char **error) {
    if (!pivot_shape(p,error) || !schema_names_valid(schema,error)) return false;
    size_t rows=p->counts[PIVOT_ROWS],measures=p->counts[PIVOT_VALUES];
    if (schema->column_count!=rows+measures) return fail(error,"Matrix result no longer matches its Rows and Values fields.");
    for (size_t c=0;c<schema->column_count;c++) {
        const Column *column=&schema->columns[c]; duckdb_type type; NumberType number;
        if (!page_type_from_sql(column->type_name,&type,&number) || column->type!=type || !same_number(column->number_type,number) || column->numeric!=(number.kind!=NUM_NONE)) return fail(error,"Matrix SQL types no longer match its typed result.");
        char *name=c<rows ? pivot_field_key(p,PIVOT_ROWS,c) : aggregate_measure_label(p->fields[PIVOT_VALUES][c-rows].aggregate,p->fields[PIVOT_VALUES][c-rows].name);
        bool matches=!strcmp(column->name,name); free(name);
        if (!matches) return fail(error,"Matrix result field names no longer match its definition.");
    }
    return true;
}
bool browser_pivot_totals_schema(Page *out,const Page *schema,const PivotDefinition *p,char **error) {
    if (!out) return fail(error,"Choose an owned matrix totals schema.");
    if (!pivot_schema_valid(schema,p,error)) return false;
    Page totals={.column_count=p->counts[PIVOT_VALUES]}; totals.columns=dx_alloc(totals.column_count*sizeof(Column)); totals.bytes=totals.column_count*sizeof(Column);
    for (size_t i=0;i<totals.column_count;i++) {
        const Column *source=&schema->columns[p->counts[PIVOT_ROWS]+i]; Column *column=&totals.columns[i];
        *column=(Column){.name=dx_printf("total_%zu",i),.type_name=dx_strdup(source->type_name),.type=source->type,.number_type=source->number_type,.numeric=source->numeric};
        totals.bytes+=strlen(column->name)+strlen(column->type_name)+2;
    }
    page_free(out); *out=totals; return true;
}
static bool pivot_add(size_t *bytes,size_t add,char **error) {
    if (add>DX_PAGE_WIRE_TEXT_BYTES || *bytes>DX_PAGE_WIRE_TEXT_BYTES-add) return fail(error,"Matrix page and totals exceed their combined 4 MiB memory limit.");
    *bytes+=add; return true;
}
static bool pivot_page_bytes(const Page *page,char **error) {
    if (page->records.ids || page->records.source_key || page->records.count || page->records.stamp) return fail(error,"Matrix results cannot carry raw-source record IDs.");
    size_t bytes=page->column_count*sizeof(Column)+page->column_count*page->row_count*sizeof(Cell);
    for (size_t c=0;c<page->column_count;c++) {
        if (page->columns[c].caption || page->columns[c].detail || page->columns[c].category_count || page->columns[c].categories) return fail(error,"Matrix result is already decorated.");
        size_t name=0,type=0;
        if (!text_size(page->columns[c].name,4096,&name) || !text_size(page->columns[c].type_name,128,&type) || !pivot_add(&bytes,name+type+2,error)) return fail(error,"Invalid matrix column memory accounting.");
    }
    for (size_t i=0;i<page->column_count*page->row_count;i++) {
        const Cell *cell=&page->cells[i]; size_t length=0;
        if (cell->is_null ? cell->text!=NULL : !text_size(cell->text,DX_PAGE_WIRE_TEXT_BYTES,&length)) return fail(error,"Invalid exact matrix cell text.");
        if (!cell->is_null && !pivot_add(&bytes,length+1,error)) return false;
    }
    size_t sql=0; if (page->sql && (!text_size(page->sql,DX_BROWSER_SQL_BYTES,&sql) || !pivot_add(&bytes,sql+1,error))) return false;
    if (page->bytes<bytes || page->bytes>DX_PAGE_WIRE_TEXT_BYTES) return fail(error,"Invalid matrix page memory accounting.");
    return true;
}
bool browser_pivot_complete(Page *page,Page *grand,const PivotDefinition *p,char **error) {
    if (!page || page==grand) return fail(error,"Choose separate owned matrix and total pages.");
    if (!pivot_schema_valid(page,p,error)) return false;
    size_t rows=p->counts[PIVOT_ROWS],measures=p->counts[PIVOT_VALUES];
    if (page->totals || page->count_pending || page->row_count>DX_PAGE_ROWS || (page->row_count && !page->cells) ||
        page->offset>UINT64_MAX-page->row_count || (page->row_count && page->offset+page->row_count>page->total) ||
        (grand && (grand->row_count!=1 || grand->column_count!=measures || !grand->columns || !grand->cells || grand->offset || grand->total!=1 || grand->count_pending || grand->totals))) return fail(error,"Matrix page and source totals have incompatible extents.");
    if (!pivot_page_bytes(page,error) || (grand && !pivot_page_bytes(grand,error))) return false;
    size_t used=page->bytes;
    if (grand && (!pivot_add(&used,grand->bytes,error) || !pivot_add(&used,sizeof(PivotTotals),error))) return false;
    size_t row_bytes=grand ? measures*sizeof(Column)+page->row_count*measures*sizeof(Cell) : 0; if (!pivot_add(&used,row_bytes,error)) return false;
    char *labels[DX_PIVOT_FIELDS]={0},*captions[DX_PIVOT_FIELDS]={0}; bool ok=false;
    for (size_t c=0;c<rows;c++) if (p->fields[PIVOT_ROWS][c].grain) {
        labels[c]=pivot_field_label(&p->fields[PIVOT_ROWS][c]); size_t old=strlen(page->columns[c].name)+1;
        if (used<old) { fail(error,"Invalid matrix memory accounting."); goto done; } used-=old;
        if (!pivot_add(&used,strlen(labels[c])+1,error)) goto done;
    }
    for (size_t m=0;m<measures;m++) {
        const Column *source=&page->columns[rows+m],*total=grand ? &grand->columns[m] : NULL; char name[32]; snprintf(name,sizeof(name),"total_%zu",m);
        if (total && (!total->name || strcmp(total->name,name) || !total->type_name || strcmp(total->type_name,source->type_name) || total->type!=source->type || !same_number(total->number_type,source->number_type) || total->numeric!=source->numeric)) { fail(error,"Source totals no longer match matrix measure types."); goto done; }
        captions[m]=aggregate_measure_label(p->fields[PIVOT_VALUES][m].aggregate,p->fields[PIVOT_VALUES][m].name);
        size_t copied=grand ? strlen(source->type_name)+1+(size_t)snprintf(name,sizeof(name),"m%zu",m)+1 : 0;
        size_t extra=strlen(captions[m])+1+copied;
        if (!pivot_add(&used,extra,error)) goto done; row_bytes+=extra-(strlen(captions[m])+1);
        for (size_t r=0;grand && r<page->row_count;r++) {
            const Cell *cell=&page->cells[r*page->column_count+rows+m]; size_t length=0;
            if (cell->is_null ? cell->text!=NULL : !text_size(cell->text,DX_PAGE_WIRE_TEXT_BYTES,&length)) { fail(error,"Invalid exact matrix measure text."); goto done; }
            if (!cell->is_null) { if (!pivot_add(&used,length+1,error)) goto done; row_bytes+=length+1; }
        }
    }
    PivotTotals *totals=grand ? dx_alloc(sizeof(*totals)) : NULL;
    if (totals) { totals->rows=(Page){.column_count=measures,.row_count=page->row_count,.offset=page->offset,.total=page->total,.bytes=row_bytes};
        totals->rows.columns=dx_alloc(measures*sizeof(Column)); if (page->row_count) totals->rows.cells=dx_alloc(page->row_count*measures*sizeof(Cell)); }
    for (size_t m=0;m<measures;m++) {
        const Column *source=&page->columns[rows+m];
        if (totals) { totals->rows.columns[m]=(Column){.name=dx_printf("m%zu",m),.type_name=dx_strdup(source->type_name),.type=source->type,.number_type=source->number_type,.numeric=source->numeric};
            for (size_t r=0;r<page->row_count;r++) { const Cell *cell=&page->cells[r*page->column_count+rows+m]; totals->rows.cells[r*measures+m]=*cell; totals->rows.cells[r*measures+m].text=cell->is_null ? NULL : dx_strdup(cell->text); } }
        page->columns[rows+m].caption=captions[m]; captions[m]=NULL;
    }
    for (size_t c=0;c<rows;c++) if (labels[c]) { free(page->columns[c].name); page->columns[c].name=labels[c]; labels[c]=NULL; page->columns[c].date_grain=p->fields[PIVOT_ROWS][c].grain; }
    if (totals) { totals->columns=*grand; memset(grand,0,sizeof(*grand)); } page->totals=totals; page->bytes=used; ok=true;
done:
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) { free(labels[i]); free(captions[i]); } return ok;
}

void browser_pivot_plan_free(BrowserPivotPlan *plan) {
    if (!plan) return;
    for (size_t i=0;plan->headers && i<plan->count && i<128;i++) page_column_free(&plan->headers[i]);
    free(plan->headers); free(plan->sql); memset(plan,0,sizeof(*plan));
}
static bool columns_shape(const PivotDefinition *p,char **error) {
    return pivot_shape_all(p,error) && (p->counts[PIVOT_COLUMNS] || fail(error,"Choose at least one Columns field for a bounded pivot plan."));
}
char *browser_pivot_domain_sql(const Analysis *source,const PivotDefinition *p,size_t field,char **error) {
    if (!columns_shape(p,error)) return NULL;
    if (field>=p->counts[PIVOT_COLUMNS]) { fail(error,"Invalid pivot column domain index."); return NULL; }
    char *valid=browser_relation_sql(source,p,error); if (!valid) return NULL; free(valid);
    char *input=pivot_input_sql(source,p),*key=pivot_field_key(p,PIVOT_COLUMNS,field),*name=dx_quote(key,'"');
    /* DISTINCT and ordering precede VARCHAR projection, so large numbers and
       temporal categories keep their SQL identity and native ordering. */
    char *sql=dx_printf("SELECT CAST(_duxcel_domain_value AS VARCHAR) AS category FROM (SELECT DISTINCT %s AS _duxcel_domain_value FROM (%s) AS _duxcel_domain_input ORDER BY _duxcel_domain_value NULLS FIRST LIMIT 129) AS _duxcel_domain ORDER BY _duxcel_domain_value NULLS FIRST",name,input);
    free(input); free(key); free(name); return bounded_sql(sql,error);
}
static char *plan_column_name(const PivotDefinition *p,size_t index) {
    for (unsigned suffix=0;;suffix++) {
        char *name=dx_printf("__duxcel_pivot_value_%zu_%u",index,suffix); bool collision=false;
        for (size_t r=0;r<p->counts[PIVOT_ROWS];r++) { char *key=pivot_field_key(p,PIVOT_ROWS,r); collision|=same_name(key,name); free(key); }
        if (!collision) return name; free(name);
    }
}
bool browser_pivot_plan_build(BrowserPivotPlan *out,const Analysis *source,const PivotDefinition *p,const Page *domains,char **error) {
    if (!out || !domains || !columns_shape(p,error)) return fail(error,"Choose owned pivot domains and a plan output.");
    char *valid=browser_relation_sql(source,p,error); if (!valid) return false; free(valid);
    size_t counts[DX_PIVOT_FIELDS]={0},product=1,used=0,keys=p->counts[PIVOT_ROWS],measures=p->counts[PIVOT_VALUES];
    BrowserPivotPlan plan={0}; char *lists[DX_PIVOT_FIELDS]={0}; bool ok=false;
    for (size_t f=0;f<p->counts[PIVOT_COLUMNS];f++) {
        const Page *domain=&domains[f];
        if (domain->column_count!=1 || !domain->columns || domain->row_count>128 || (domain->row_count && !domain->cells) || domain->offset || domain->totals ||
            !domain->columns[0].name || strcmp(domain->columns[0].name,"category") || !domain->columns[0].type_name || strcmp(domain->columns[0].type_name,"VARCHAR") ||
            domain->columns[0].type!=DUCKDB_TYPE_VARCHAR || domain->columns[0].number_type.kind!=NUM_NONE || domain->columns[0].numeric) { fail(error,"Pivot exceeds 128 result columns or has an invalid column domain."); goto done; }
        if (!pivot_page_bytes(domain,error) || !pivot_add(&used,domain->bytes,error)) goto done;
        counts[f]=domain->row_count ? domain->row_count : 1; plan.empty_input|=!domain->row_count;
        if (counts[f]>(128-keys)/(product*measures)) { fail(error,"Pivot exceeds 128 result columns. Filter the column fields or use fewer measures."); goto done; }
        product*=counts[f]; lists[f]=dx_strdup("");
        for (size_t i=0;i<counts[f];i++) {
            const char *raw=domain->row_count && !domain->cells[i].is_null ? domain->cells[i].text : NULL;
            for (size_t j=0;j<i && domain->row_count;j++) if (domain->cells[i].is_null==domain->cells[j].is_null && (domain->cells[i].is_null || !strcmp(raw,domain->cells[j].text))) { fail(error,"Pivot column domains must contain unique typed categories."); goto done; }
            char *literal=raw ? dx_quote(raw,'\'') : dx_strdup("NULL");
            dx_append(&lists[f],"%s%s AS \"category_%zu\"",i ? "," : "",literal,i); free(literal);
            if (!text_size(lists[f],DX_BROWSER_SQL_BYTES,NULL)) { fail(error,"Pivot column definitions exceed the 1 MiB SQL limit."); goto done; }
        }
    }
    char *base=pivot_sql_domains(source,p,lists); plan.count=product*measures;
    plan.sql=dx_printf("SELECT * FROM (%s) AS _duxcel_bounded_pivot(",base); free(base);
    for (size_t c=0;c<keys+plan.count;c++) {
        char *name=c<keys ? pivot_field_key(p,PIVOT_ROWS,c) : plan_column_name(p,c-keys),*quoted=dx_quote(name,'"');
        dx_append(&plan.sql,"%s%s",c ? "," : "",quoted); free(name); free(quoted);
    }
    dx_append(&plan.sql,")");
    if (keys) { dx_append(&plan.sql," ORDER BY "); for (size_t c=0;c<keys;c++) { char *key=pivot_field_key(p,PIVOT_ROWS,c),*quoted=dx_quote(key,'"'); dx_append(&plan.sql,"%s%s NULLS FIRST",c ? "," : "",quoted); free(key); free(quoted); } }
    if (!text_size(plan.sql,DX_BROWSER_SQL_BYTES,NULL)) { fail(error,"Browser pivot SQL exceeds 1 MiB. Narrow the column categories."); goto done; }
    plan.bytes=strlen(plan.sql)+1+plan.count*sizeof(Column); if (!pivot_add(&used,plan.bytes,error)) goto done;
    plan.headers=dx_alloc(plan.count*sizeof(Column));
    for (size_t group=0;group<product;group++) {
        size_t positions[DX_PIVOT_FIELDS]={0},remaining=group;
        for (int f=(int)p->counts[PIVOT_COLUMNS]-1;f>=0;f--) { positions[f]=remaining%counts[f]; remaining/=counts[f]; }
        for (size_t m=0;m<measures;m++) {
            size_t at=group*measures+m; Column *header=&plan.headers[at]; header->name=plan_column_name(p,at);
            const PivotField *measure=&p->fields[PIVOT_VALUES][m]; char *label=aggregate_measure_label(measure->aggregate,measure->name);
            header->caption=dx_strdup("");
            if (!plan.empty_input) {
                header->category_count=p->counts[PIVOT_COLUMNS]; header->categories=dx_alloc(header->category_count*sizeof(char *));
                for (size_t f=0;f<header->category_count;f++) {
                    const Cell *cell=&domains[f].cells[positions[f]]; const char *raw=cell->is_null ? NULL : cell->text; char display[64];
                    header->category_grains[f]=p->fields[PIVOT_COLUMNS][f].grain;
                    if (raw) header->categories[f]=dx_strdup(raw);
                    pivot_date_label(header->category_grains[f],raw,display);
                    dx_append(&header->caption,"%s%s",f ? " · " : "",raw ? header->category_grains[f] ? display : raw : "NULL");
                }
                if (measures>1) header->detail=dx_strdup(label);
            } else { free(header->caption); header->caption=dx_strdup(label); }
            free(label);
            size_t added=strlen(header->name)+strlen(header->caption)+2+(header->detail ? strlen(header->detail)+1 : 0)+header->category_count*sizeof(char *);
            for (size_t f=0;f<header->category_count;f++) if (header->categories[f]) added+=strlen(header->categories[f])+1;
            if (!pivot_add(&used,added,error)) goto done; plan.bytes+=added;
        }
    }
    browser_pivot_plan_free(out); *out=plan; memset(&plan,0,sizeof(plan)); ok=true;
done:
    for (size_t f=0;f<DX_PIVOT_FIELDS;f++) free(lists[f]); browser_pivot_plan_free(&plan); return ok;
}
static bool plan_valid(const BrowserPivotPlan *plan,const PivotDefinition *p,char **error) {
    if (!columns_shape(p,error)) return false;
    if (!plan || !plan->headers || !plan->count || plan->count>128-p->counts[PIVOT_ROWS] || plan->count%p->counts[PIVOT_VALUES] || !text_size(plan->sql,DX_BROWSER_SQL_BYTES,NULL) || !plan->bytes || plan->bytes>DX_PAGE_WIRE_TEXT_BYTES) return fail(error,"Invalid owned browser pivot plan.");
    size_t used=strlen(plan->sql)+1+plan->count*sizeof(Column);
    for (size_t c=0;c<plan->count;c++) {
        const Column *h=&plan->headers[c]; char *name=plan_column_name(p,c); bool valid=h->name && !strcmp(h->name,name); free(name);
        if (!valid || !text_size(h->caption,DX_PAGE_WIRE_TEXT_BYTES,NULL) || (h->detail && !text_size(h->detail,4096+32,NULL)) || h->category_count!=(plan->empty_input ? 0 : p->counts[PIVOT_COLUMNS]) || (h->category_count && !h->categories)) return fail(error,"Invalid ordinal pivot header metadata.");
        size_t added=strlen(h->name)+strlen(h->caption)+2+(h->detail ? strlen(h->detail)+1 : 0)+h->category_count*sizeof(char *);
        for (size_t f=0;f<h->category_count;f++) {
            size_t length=0; if (h->category_grains[f]!=p->fields[PIVOT_COLUMNS][f].grain || (h->categories[f] && !text_size(h->categories[f],DX_PAGE_WIRE_TEXT_BYTES,&length))) return fail(error,"Invalid typed pivot category metadata.");
            if (h->categories[f]) added+=length+1;
        }
        if (!pivot_add(&used,added,error)) return false;
    }
    return plan->bytes>=used || fail(error,"Invalid pivot plan memory accounting.");
}
static bool planned_schema_valid(const Page *schema,const PivotDefinition *p,const BrowserPivotPlan *plan,char **error) {
    if (!plan_valid(plan,p,error) || !schema_names_valid(schema,error)) return false;
    size_t keys=p->counts[PIVOT_ROWS],measures=p->counts[PIVOT_VALUES];
    if (schema->column_count!=keys+plan->count) return fail(error,"Pivot result no longer matches its bounded column domains.");
    for (size_t c=0;c<schema->column_count;c++) {
        const Column *col=&schema->columns[c]; duckdb_type type; NumberType number;
        if (!page_type_from_sql(col->type_name,&type,&number) || col->type!=type || !same_number(col->number_type,number) || col->numeric!=(number.kind!=NUM_NONE)) return fail(error,"Pivot result SQL type metadata changed.");
        char *key=c<keys ? pivot_field_key(p,PIVOT_ROWS,c) : NULL; const char *name=key ? key : plan->headers[c-keys].name;
        bool matches=!strcmp(name,col->name); free(key); if (!matches) return fail(error,"Pivot ordinal result aliases changed.");
        if (c>=keys+measures) { const Column *first=&schema->columns[keys+(c-keys)%measures]; if (strcmp(first->type_name,col->type_name)) return fail(error,"Pivot category measure SQL types differ."); }
    }
    return true;
}
char *browser_pivot_readout_sql(const Analysis *source,const PivotDefinition *p,const BrowserPivotPlan *plan,bool rows,uint64_t offset,char **error) {
    if (!plan_valid(plan,p,error)) return NULL;
    char *valid=browser_relation_sql(source,p,error); if (!valid) return NULL; free(valid);
    if (rows && offset>UINT64_MAX-DX_PAGE_ROWS) { fail(error,"Pivot totals page interval overflows uint64."); return NULL; }
    size_t keys=p->counts[PIVOT_ROWS],dims=p->counts[PIVOT_COLUMNS],measures=p->counts[PIVOT_VALUES];
    uint64_t row_mask=(UINT64_C(1)<<dims)-1,column_mask=((UINT64_C(1)<<keys)-1)<<dims,grand_mask=(UINT64_C(1)<<(keys+dims))-1;
    char *base=pivot_totals_sql(source,p),*sql=dx_strdup("SELECT ");
    if (rows) {
        for (size_t m=0;m<measures;m++) dx_append(&sql,"%sCAST(m%zu AS VARCHAR) AS m%zu",m ? "," : "",m,m);
        dx_append(&sql," FROM (%s) AS _duxcel_matrix_rows WHERE mask=%" PRIu64,base,row_mask);
        if (keys) { dx_append(&sql," ORDER BY "); for (size_t r=0;r<keys;r++) dx_append(&sql,"%sd%zu NULLS FIRST",r ? "," : "",r); }
        dx_append(&sql," LIMIT %d OFFSET %" PRIu64,DX_PAGE_ROWS,offset);
    } else {
        size_t categories=plan->empty_input ? 0 : plan->count;
        for (size_t c=0;c<categories+measures;c++) {
            size_t m=c<categories ? c%measures : c-categories; bool count=p->fields[PIVOT_VALUES][m].aggregate==AGG_COUNT || p->fields[PIVOT_VALUES][m].aggregate==AGG_DISTINCT;
            dx_append(&sql,"%sCAST(%smax(m%zu) FILTER (WHERE mask=%" PRIu64,c ? "," : "",count ? "coalesce(" : "",m,c<categories ? column_mask : grand_mask);
            if (c<categories) for (size_t f=0;f<dims;f++) { const char *raw=plan->headers[c].categories[f]; char *literal=raw ? dx_quote(raw,'\'') : dx_strdup("NULL"); dx_append(&sql," AND d%zu IS NOT DISTINCT FROM %s",keys+f,literal); free(literal); }
            dx_append(&sql,")%s AS VARCHAR) AS total_%zu",count ? ",0)" : "",c);
            if (!text_size(sql,DX_BROWSER_SQL_BYTES,NULL)) { free(sql); free(base); fail(error,"Pivot category totals SQL exceeds 1 MiB."); return NULL; }
        }
        dx_append(&sql," FROM (%s) AS _duxcel_matrix_columns",base);
    }
    free(base); return bounded_sql(sql,error);
}
bool browser_pivot_readout_schema(Page *out,const Page *schema,const PivotDefinition *p,const BrowserPivotPlan *plan,bool rows,char **error) {
    if (!out || !planned_schema_valid(schema,p,plan,error)) return false;
    size_t measures=p->counts[PIVOT_VALUES],categories=plan->empty_input ? 0 : plan->count;
    Page result={.column_count=rows ? measures : categories+measures}; result.columns=dx_alloc(result.column_count*sizeof(Column)); result.bytes=result.column_count*sizeof(Column);
    for (size_t c=0;c<result.column_count;c++) {
        const Column *source=&schema->columns[p->counts[PIVOT_ROWS]+c%measures]; Column *col=&result.columns[c];
        *col=(Column){.name=dx_printf(rows ? "m%zu" : "total_%zu",c),.type_name=dx_strdup(source->type_name),.type=source->type,.number_type=source->number_type,.numeric=source->numeric};
        result.bytes+=strlen(col->name)+strlen(col->type_name)+2;
    }
    page_free(out); *out=result; return true;
}
static bool readout_matches(const Page *page,const Page *body,const PivotDefinition *p,const BrowserPivotPlan *plan,bool rows,char **error) {
    size_t measures=p->counts[PIVOT_VALUES],count=rows ? measures : (plan->empty_input ? 0 : plan->count)+measures;
    if (!page || page->column_count!=count || !page->columns || (page->row_count && !page->cells) || page->count_pending || page->totals || page->row_count!=(rows ? body->row_count : 1) || page->offset!=(rows ? body->offset : 0) || page->total!=(rows ? body->total : 1)) return fail(error,"Source pivot totals no longer match the result page.");
    for (size_t c=0;c<count;c++) {
        char name[32]; snprintf(name,sizeof(name),rows ? "m%zu" : "total_%zu",c); const Column *col=&page->columns[c],*measure=&body->columns[p->counts[PIVOT_ROWS]+c%measures];
        if (!col->name || strcmp(col->name,name) || !col->type_name || strcmp(col->type_name,measure->type_name) || col->type!=measure->type || !same_number(col->number_type,measure->number_type) || col->numeric!=measure->numeric) return fail(error,"Source pivot totals measure types changed.");
    }
    return pivot_page_bytes(page,error);
}
bool browser_pivot_plan_complete(Page *page,Page *rows,Page *columns,const PivotDefinition *p,const BrowserPivotPlan *plan,char **error) {
    if (!page || page==rows || page==columns || (rows && rows==columns) || (rows!=NULL)!=(columns!=NULL)) return fail(error,"Choose independent owned pivot pages and paired readouts.");
    if (!planned_schema_valid(page,p,plan,error)) return false;
    if (page->totals || page->count_pending || page->row_count>DX_PAGE_ROWS || (page->row_count && !page->cells) || page->offset>UINT64_MAX-page->row_count || (page->row_count && page->offset+page->row_count>page->total)) return fail(error,"Invalid exact-count pivot page extent.");
    if (!pivot_page_bytes(page,error) || (rows && (!readout_matches(rows,page,p,plan,true,error) || !readout_matches(columns,page,p,plan,false,error)))) return false;
    size_t used=page->bytes;
    if (rows && (!pivot_add(&used,rows->bytes,error) || !pivot_add(&used,columns->bytes,error) || !pivot_add(&used,sizeof(PivotTotals),error))) return false;
    char *labels[DX_PIVOT_FIELDS]={0}; bool ok=false;
    for (size_t r=0;r<p->counts[PIVOT_ROWS];r++) if (p->fields[PIVOT_ROWS][r].grain) {
        labels[r]=pivot_field_label(&p->fields[PIVOT_ROWS][r]); used-=strlen(page->columns[r].name)+1;
        if (!pivot_add(&used,strlen(labels[r])+1,error)) goto done;
    }
    for (size_t c=0;c<plan->count;c++) {
        const Column *h=&plan->headers[c]; size_t extra=strlen(h->caption)+1+(h->detail ? strlen(h->detail)+1 : 0)+h->category_count*sizeof(char *);
        for (size_t f=0;f<h->category_count;f++) if (h->categories[f]) extra+=strlen(h->categories[f])+1;
        if (!pivot_add(&used,extra,error)) goto done;
    }
    for (size_t c=0;c<plan->count;c++) {
        const Column *h=&plan->headers[c]; Column *col=&page->columns[p->counts[PIVOT_ROWS]+c];
        col->caption=dx_strdup(h->caption); if (h->detail) col->detail=dx_strdup(h->detail);
        col->category_count=h->category_count; if (h->category_count) col->categories=dx_alloc(h->category_count*sizeof(char *));
        memcpy(col->category_grains,h->category_grains,sizeof(col->category_grains));
        for (size_t f=0;f<h->category_count;f++) if (h->categories[f]) col->categories[f]=dx_strdup(h->categories[f]);
    }
    for (size_t r=0;r<p->counts[PIVOT_ROWS];r++) if (labels[r]) { free(page->columns[r].name); page->columns[r].name=labels[r]; labels[r]=NULL; page->columns[r].date_grain=p->fields[PIVOT_ROWS][r].grain; }
    if (rows) { page->totals=dx_alloc(sizeof(PivotTotals)); page->totals->rows=*rows; page->totals->columns=*columns; page->totals->has_columns=!plan->empty_input; memset(rows,0,sizeof(*rows)); memset(columns,0,sizeof(*columns)); }
    page->bytes=used; ok=true;
done:
    for (size_t r=0;r<DX_PIVOT_FIELDS;r++) free(labels[r]); return ok;
}

char *browser_statistics_cells_sql(const char *relation,const Page *schema,uint64_t first,uint64_t rows,size_t column,size_t columns,char **error) {
    if (!relation_valid(relation,error) || !schema_names_valid(schema,error)) return NULL;
    if (!columns || columns>256 || first>DX_SHEET_ROWS || rows>DX_SHEET_ROWS-first || column>=schema->column_count || columns>schema->column_count-column) { fail(error,"Selection statistics support up to 256 described columns inside the worksheet."); return NULL; }
    char *projection=dx_strdup(""),*values=dx_strdup(""),*sql=NULL;
    for (size_t i=0;i<columns;i++) {
        const Column *c=&schema->columns[column+i]; duckdb_type type; NumberType number;
        if (!page_type_from_sql(c->type_name,&type,&number) || c->type!=type || !same_number(c->number_type,number) || c->numeric!=(number.kind!=NUM_NONE)) { fail(error,"Selection statistics SQL type metadata changed."); goto done; }
        char *quoted=dx_quote(c->name,'"'); dx_append(&projection,"%s%s AS c%zu",i ? "," : "",quoted,i); free(quoted);
        bool numeric=number.kind!=NUM_NONE,floating=number.kind==NUM_FLOAT;
        char *units=numeric && !floating ? dx_printf("CAST(replace(CAST(c%zu AS VARCHAR),'.','') || repeat('0',%u) AS BIGNUM)",i,38-(number.kind==NUM_DECIMAL ? number.scale : 0)) : dx_strdup("NULL::BIGNUM");
        char *value=numeric ? dx_printf("CAST(c%zu AS DOUBLE)",i) : dx_strdup("NULL::DOUBLE");
        char *text=floating ? dx_printf("CASE WHEN c%zu=0 THEN '0' ELSE CAST(c%zu AS VARCHAR) END",i,i) : dx_printf("CAST(c%zu AS VARCHAR)",i);
        dx_append(&values,"%sSELECT c%zu IS NOT NULL AS present,%s AS numeric,typeof(c%zu) AS type,%s AS text,%s AS exact,%s AS value,%s AS floating FROM selected",i ? " UNION ALL " : "",i,numeric ? "true" : "false",i,text,units,value,floating ? "true" : "false");
        free(units); free(value); free(text);
        if (strlen(projection)+strlen(values)>DX_BROWSER_SQL_BYTES) { fail(error,"Selection statistics query exceeds 1 MiB."); goto done; }
    }
    sql=bounded_sql(dx_printf("WITH selected AS MATERIALIZED (SELECT %s FROM (%s) AS source LIMIT %" PRIu64 " OFFSET %" PRIu64 ") %s",projection,relation,rows,first,values),error);
done:
    free(projection); free(values); return sql;
}
char *browser_statistics_aggregate_sql(const char *cells,const uint64_t *logical_cells,char **error) {
    if (!relation_valid(cells,error)) return NULL;
    if (logical_cells && *logical_cells>DX_SHEET_ROWS*UINT64_C(16384)) { fail(error,"Invalid logical worksheet statistics area."); return NULL; }
    char *count=logical_cells ? dx_printf("%" PRIu64 "::UBIGINT",*logical_cells) : dx_strdup("count(*)");
    char *sql=dx_printf("WITH cells AS (%s) SELECT CAST(%s AS VARCHAR) AS cells,CAST(count(*) FILTER (WHERE present) AS VARCHAR) AS filled,CAST(count(*) FILTER (WHERE numeric AND present) AS VARCHAR) AS numeric,CAST(count(DISTINCT (type,text)) FILTER (WHERE present) AS VARCHAR) AS distinct_values,"
        "CAST(sum(exact) AS VARCHAR) AS sum_exact,CAST(min(exact) AS VARCHAR) AS min_exact,CAST(max(exact) AS VARCHAR) AS max_exact,"
        "CAST(sum(value) AS VARCHAR) AS sum_float,CAST(avg(value) AS VARCHAR) AS average_float,CAST(min(value) AS VARCHAR) AS min_float,CAST(max(value) AS VARCHAR) AS max_float,"
        "CAST(coalesce(bool_or(floating AND present),false) AS VARCHAR) AS approximate,CAST(coalesce(bool_or(numeric AND present AND NOT isfinite(value)),false) AS VARCHAR) AS non_finite FROM cells",cells,count);
    free(count); return bounded_sql(sql,error);
}
char *browser_statistics_sql(const char *relation,const Page *schema,uint64_t first,uint64_t rows,size_t column,size_t columns,char **error) {
    char *cells=browser_statistics_cells_sql(relation,schema,first,rows,column,columns,error);
    if (!cells) return NULL;
    char *sql=browser_statistics_aggregate_sql(cells,NULL,error); free(cells); return sql;
}
char *browser_statistics_manual_sql(char *const *texts,size_t count,char **error) {
    if (count>32768 || (count && !texts)) { fail(error,"Invalid mixed statistics labels."); return NULL; }
    if (!count) return dx_strdup("SELECT false AS present,false AS numeric,NULL::VARCHAR AS type,NULL::VARCHAR AS text,NULL::BIGNUM AS exact,NULL::DOUBLE AS value,false AS floating WHERE false");
    char *values=dx_strdup(""); size_t size=0;
    for (size_t i=0;i<count;i++) {
        size_t length; if (!text_size(texts[i],DX_BROWSER_SQL_BYTES,&length)) { free(values); fail(error,"Invalid mixed statistics label text."); return NULL; }
        size_t escaped=length; for (size_t j=0;j<length;j++) escaped+=texts[i][j]=='\'';
        if (escaped>DX_BROWSER_SQL_BYTES-5 || size>DX_BROWSER_SQL_BYTES-escaped-5) { free(values); fail(error,"Selection statistics query exceeds 1 MiB."); return NULL; }
        char *quoted=dx_quote(texts[i],'\''); dx_append(&values,"%s(%s)",i ? "," : "",quoted); free(quoted); size+=escaped+5;
    }
    char *sql=dx_printf("SELECT true AS present,false AS numeric,'VARCHAR'::VARCHAR AS type,v AS text,NULL::BIGNUM AS exact,NULL::DOUBLE AS value,false AS floating FROM (VALUES %s) AS labels(v)",values);
    free(values); return bounded_sql(sql,error);
}
bool browser_statistics_append_cells(char **cells,const char *range,char **error) {
    if (!cells || !relation_valid(*cells,error) || !relation_valid(range,error)) return false;
    size_t a=strlen(*cells),b=strlen(range);
    if (a>DX_BROWSER_SQL_BYTES-64 || b>DX_BROWSER_SQL_BYTES-a-64) return fail(error,"Selection statistics query exceeds 1 MiB.");
    dx_append(cells," UNION ALL SELECT * FROM (%s) AS range_cells WHERE present",range); return true;
}
static bool statistics_uint64(const char *text,uint64_t *out) {
    if (!text || !*text) return false; uint64_t value=0;
    for (const char *p=text;*p;p++) { if (*p<'0' || *p>'9' || value>(UINT64_MAX-(unsigned)(*p-'0'))/10) return false; value=value*10+(unsigned)(*p-'0'); }
    *out=value; return true;
}
static bool statistics_boolean(const Cell *cell,bool *out) {
    if (cell->is_null || !cell->text) return false;
    if (!strcmp(cell->text,"true")) { *out=true; return true; }
    if (!strcmp(cell->text,"false")) { *out=false; return true; } return false;
}
static bool statistics_double(const Cell *cell,double *out) {
    if (cell->is_null) return true;
    if (!cell->text || !*cell->text || isspace((unsigned char)*cell->text)) return false;
    char *end=NULL; double value=strtod(cell->text,&end); if (!end || end==cell->text || *end) return false; *out=value; return true;
}
static bool statistics_units(const Cell *cell,char **out) {
    if (cell->is_null) return true;
    size_t size; if (!text_size(cell->text,128,&size) || !size) return false;
    const char *digits=cell->text+(*cell->text=='-'); size_t length=size-(size_t)(digits!=cell->text),sign=digits!=cell->text;
    if (!length) return false;
    for (size_t i=0;i<length;i++) if (digits[i]<'0' || digits[i]>'9') return false;
    char *text=dx_alloc(size+42); size_t at=0,whole=length>38 ? length-38 : 0;
    if (sign) text[at++]='-';
    if (whole) { memcpy(text+at,digits,whole); at+=whole; text[at++]='.'; memcpy(text+at,digits+whole,38); at+=38; }
    else { text[at++]='0'; text[at++]='.'; memset(text+at,'0',38-length); at+=38-length; memcpy(text+at,digits,length); at+=length; }
    while (at && text[at-1]=='0') at--; if (at && text[at-1]=='.') at--; text[at]=0;
    if (!strcmp(text,"-0")) { text[0]='0'; text[1]=0; } *out=dx_realloc(text,strlen(text)+1); return true;
}
static bool statistics_budget(size_t *used,size_t bytes,char **error) {
    if (bytes>DX_PAGE_WIRE_TEXT_BYTES || *used>DX_PAGE_WIRE_TEXT_BYTES-bytes) return fail(error,"Selection statistics exceed their combined 4 MiB memory limit.");
    *used+=bytes; return true;
}
static bool statistics_definition_bytes(const Analysis *source,bool optional,size_t *used,char **error) {
    if (!source) return fail(error,"Missing mixed statistics source definition.");
    if (optional && source->kind==SOURCE_NONE) {
        if (source->path || source->schema || source->table || source->query || source->filter_column || source->filter_value || source->group_column || source->measure_column || source->sort_column || source->filter_count || source->sort_count || source->unique_rows) return fail(error,"Invalid empty mixed statistics source definition.");
        return true;
    }
    char *sql=browser_relation_sql(source,NULL,error); if (!sql) return false; free(sql);
    return statistics_budget(used,analysis_text_bytes(source),error);
}
bool browser_statistics_request_valid(const Analysis *primary,const StatisticsRequest *request,size_t *owned_bytes,char **error) {
    if (!request || request->count>DX_MAX_OBJECTS+1 || request->sources.count>DX_MAX_OBJECTS || request->text_count>32768 || request->text_capacity>32768 || request->text_count>request->text_capacity || (request->text_capacity && !request->texts) || (!request->text_capacity && request->texts) || request->cells>DX_SHEET_ROWS*UINT64_C(16384) || request->text_count>request->cells) return fail(error,"Invalid mixed statistics snapshot bounds.");
    size_t used=sizeof(*request),sources=0,texts=0;
    if (!statistics_budget(&used,request->text_capacity*sizeof(char *),error) || !statistics_definition_bytes(primary,true,&used,error)) return false;
    for (size_t i=0;i<request->sources.count;i++) {
        if (!request->sources.ids[i]) return fail(error,"Invalid mixed statistics source identity.");
        for (size_t j=0;j<i;j++) if (request->sources.ids[i]==request->sources.ids[j]) return fail(error,"Duplicate mixed statistics source identity.");
        size_t before=used;
        if (!statistics_definition_bytes(&request->sources.items[i],false,&used,error)) return false;
        sources+=used-before;
    }
    if (request->sources.bytes!=sources) return fail(error,"Mixed statistics source byte accounting changed.");
    uint64_t area=request->text_count;
    for (size_t i=0;i<request->count;i++) {
        const StatisticsRange *range=&request->ranges[i];
        if (range->source>request->sources.count || !range->columns || range->columns>256 || range->column>=256 || range->columns>256-range->column || range->first>DX_SHEET_ROWS || range->rows>DX_SHEET_ROWS-range->first || range->rows>(request->cells-area)/range->columns) return fail(error,"Invalid mixed statistics range inside the worksheet.");
        if (range->expected_source_stamp) return fail(error,"Native source stamps cannot be verified by the browser engine.");
        const Analysis *source=range->source ? &request->sources.items[range->source-1] : primary;
        bool pivot=false;
        for (size_t role=0;role<4;role++) {
            if (range->pivot.counts[role]>DX_PIVOT_FIELDS) return fail(error,"Mixed statistics shelves exceed their bounded field limit.");
            pivot|=range->pivot.counts[role]!=0;
        }
        if (!range->is_pivot && pivot) return fail(error,"A mixed table range cannot contain matrix shelves.");
        char *sql=browser_relation_sql(source,range->is_pivot ? &range->pivot : NULL,error); if (!sql) return false; free(sql);
        for (size_t role=0;role<4;role++) for (size_t field=0;field<range->pivot.counts[role];field++) {
            const PivotField *f=&range->pivot.fields[role][field];
            if (!statistics_budget(&used,strlen(f->name)+1+(f->value ? strlen(f->value)+1 : 0),error)) return false;
        }
        area+=range->rows*range->columns;
    }
    for (size_t i=0;i<request->text_count;i++) {
        size_t length; if (!text_size(request->texts[i],DX_BROWSER_SQL_BYTES,&length)) return fail(error,"Invalid mixed statistics label text.");
        if (!statistics_budget(&used,length+1,error)) return false; texts+=length+1;
    }
    if (request->text_bytes!=texts) return fail(error,"Mixed statistics label byte accounting changed.");
    char *manual=browser_statistics_manual_sql(request->texts,request->text_count,error); if (!manual) return false;
    size_t definitions=used; bool valid=statistics_budget(&used,strlen(manual)+1,error); free(manual);
    if (valid && owned_bytes) *owned_bytes=definitions; return valid;
}
bool browser_statistics_from_page(SelectionStats *out,const Page *page,uint64_t max_cells,size_t retained,char **error) {
    static const char *names[]={"cells","filled","numeric","distinct_values","sum_exact","min_exact","max_exact","sum_float","average_float","min_float","max_float","approximate","non_finite"};
    if (!out || !page || page->column_count!=13 || page->row_count!=1 || page->offset || page->total!=1 || page->count_pending || page->totals || !page->columns || !page->cells || max_cells>DX_SHEET_ROWS*UINT64_C(16384) || retained>DX_PAGE_WIRE_TEXT_BYTES) return fail(error,"Invalid scalar selection statistics reply.");
    for (size_t c=0;c<13;c++) { const Column *col=&page->columns[c]; if (!col->name || strcmp(col->name,names[c]) || !col->type_name || strcmp(col->type_name,"VARCHAR") || col->type!=DUCKDB_TYPE_VARCHAR || !same_number(col->number_type,(NumberType){0}) || col->numeric) return fail(error,"Unexpected selection statistics scalar descriptors."); }
    if (!pivot_page_bytes(page,error) || !statistics_budget(&retained,page->bytes,error) || !statistics_budget(&retained,sizeof(SelectionStats),error)) return false;
    SelectionStats stats={0}; char *exact[3]={0}; double numbers[4]={0}; bool ok=false,moved_exact=false;
    uint64_t *counts[]={&stats.cells,&stats.filled,&stats.numeric,&stats.distinct};
    for (size_t c=0;c<4;c++) if (page->cells[c].is_null || !statistics_uint64(page->cells[c].text,counts[c])) { fail(error,"Invalid exact selection statistics count."); goto done; }
    if (stats.cells>max_cells || stats.filled>stats.cells || stats.numeric>stats.filled || stats.distinct>stats.filled || !statistics_boolean(&page->cells[11],&stats.approximate) || !statistics_boolean(&page->cells[12],&stats.non_finite) || (stats.approximate && !stats.numeric) || (stats.non_finite && !stats.approximate)) { fail(error,"Inconsistent selection statistics counts or flags."); goto done; }
    for (size_t i=0;i<3;i++) if (!statistics_units(&page->cells[4+i],&exact[i])) { fail(error,"Invalid exact scale-38 statistics text."); goto done; }
    for (size_t i=0;i<3;i++) if (exact[i] && !statistics_budget(&retained,strlen(exact[i])+1,error)) goto done;
    for (size_t i=0;i<4;i++) if (!statistics_double(&page->cells[7+i],&numbers[i])) { fail(error,"Invalid approximate statistics text."); goto done; }
    if (!stats.numeric) { for (size_t c=4;c<11;c++) if (!page->cells[c].is_null) { fail(error,"Empty numeric statistics must have NULL aggregates."); goto done; } }
    else if (!stats.non_finite) {
        if (page->cells[7].is_null || page->cells[8].is_null || page->cells[9].is_null || page->cells[10].is_null || (!stats.approximate && (page->cells[4].is_null || page->cells[5].is_null || page->cells[6].is_null))) { fail(error,"Present numeric statistics require source aggregates."); goto done; }
        if (stats.approximate) {
            if (isfinite(numbers[0])) stats.sum=dx_printf("%.15g",numbers[0]); else stats.sum_overflow=true;
            if (isfinite(numbers[2])) stats.minimum=dx_printf("%.15g",numbers[2]);
            if (isfinite(numbers[3])) stats.maximum=dx_printf("%.15g",numbers[3]);
        } else { stats.sum=exact[0]; stats.minimum=exact[1]; stats.maximum=exact[2]; memset(exact,0,sizeof(exact)); moved_exact=true; }
        if (isfinite(numbers[1])) stats.average=dx_printf("%.15g",numbers[1]); else stats.average_overflow=true;
    }
    char *texts[]={stats.sum,stats.average,stats.minimum,stats.maximum};
    for (size_t i=0;i<4;i++) if (texts[i] && !(moved_exact && i!=1) && !statistics_budget(&retained,strlen(texts[i])+1,error)) goto done;
    statistics_free(out); *out=stats; memset(&stats,0,sizeof(stats)); ok=true;
done:
    for (size_t i=0;i<3;i++) free(exact[i]); statistics_free(&stats); return ok;
}
