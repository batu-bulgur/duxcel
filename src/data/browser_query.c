#include "browser_query.h"
#include "page_wire.h"
#include <ctype.h>
#include <inttypes.h>
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
static bool pivot_shape(const PivotDefinition *p,char **error) {
    if (!p) return fail(error,"Choose a valid matrix definition.");
    for (size_t role=0;role<4;role++) if (p->counts[role]>DX_PIVOT_FIELDS) return fail(error,"Matrix shelves exceed their bounded field limit.");
    if (p->counts[PIVOT_COLUMNS]) return fail(error,"Browser matrices currently support Rows and Values. Column-field pivots are not connected yet.");
    if (!p->counts[PIVOT_VALUES]) return fail(error,"Add a field to Values to build the matrix.");
    size_t budget=0;
    for (size_t role=0;role<4;role++) for (size_t i=0;i<p->counts[role];i++) {
        const PivotField *f=&p->fields[role][i];
        if (!f->name || !*f->name || !text_budget(f->name,4096,&budget) || !text_budget(f->value,DX_BROWSER_SQL_BYTES,&budget) || !number_valid(f->number)) return fail(error,"Invalid browser matrix field metadata.");
    }
    return pivot_valid(p,error);
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
    if (!page || !grand || page==grand) return fail(error,"Choose separate owned matrix and total pages.");
    if (!pivot_schema_valid(page,p,error)) return false;
    size_t rows=p->counts[PIVOT_ROWS],measures=p->counts[PIVOT_VALUES];
    if (page->totals || page->count_pending || page->row_count>DX_PAGE_ROWS || (page->row_count && !page->cells) ||
        page->offset>UINT64_MAX-page->row_count || (page->row_count && page->offset+page->row_count>page->total) ||
        grand->row_count!=1 || grand->column_count!=measures || !grand->columns || !grand->cells || grand->offset || grand->total!=1 || grand->count_pending || grand->totals) return fail(error,"Matrix page and source totals have incompatible extents.");
    if (!pivot_page_bytes(page,error) || !pivot_page_bytes(grand,error)) return false;
    size_t used=page->bytes;
    if (!pivot_add(&used,grand->bytes,error) || !pivot_add(&used,sizeof(PivotTotals),error)) return false;
    size_t row_bytes=measures*sizeof(Column)+page->row_count*measures*sizeof(Cell); if (!pivot_add(&used,row_bytes,error)) return false;
    char *labels[DX_PIVOT_FIELDS]={0},*captions[DX_PIVOT_FIELDS]={0}; bool ok=false;
    for (size_t c=0;c<rows;c++) if (p->fields[PIVOT_ROWS][c].grain) {
        labels[c]=pivot_field_label(&p->fields[PIVOT_ROWS][c]); size_t old=strlen(page->columns[c].name)+1;
        if (used<old) { fail(error,"Invalid matrix memory accounting."); goto done; } used-=old;
        if (!pivot_add(&used,strlen(labels[c])+1,error)) goto done;
    }
    for (size_t m=0;m<measures;m++) {
        const Column *source=&page->columns[rows+m],*total=&grand->columns[m]; char name[32]; snprintf(name,sizeof(name),"total_%zu",m);
        if (!total->name || strcmp(total->name,name) || !total->type_name || strcmp(total->type_name,source->type_name) || total->type!=source->type || !same_number(total->number_type,source->number_type)) { fail(error,"Source totals no longer match matrix measure types."); goto done; }
        captions[m]=aggregate_measure_label(p->fields[PIVOT_VALUES][m].aggregate,p->fields[PIVOT_VALUES][m].name);
        size_t extra=strlen(captions[m])+1+strlen(source->type_name)+1+(size_t)snprintf(name,sizeof(name),"m%zu",m)+1;
        if (!pivot_add(&used,extra,error)) goto done; row_bytes+=extra-(strlen(captions[m])+1);
        for (size_t r=0;r<page->row_count;r++) {
            const Cell *cell=&page->cells[r*page->column_count+rows+m]; size_t length=0;
            if (cell->is_null ? cell->text!=NULL : !text_size(cell->text,DX_PAGE_WIRE_TEXT_BYTES,&length)) { fail(error,"Invalid exact matrix measure text."); goto done; }
            if (!cell->is_null) { if (!pivot_add(&used,length+1,error)) goto done; row_bytes+=length+1; }
        }
    }
    PivotTotals *totals=dx_alloc(sizeof(*totals)); totals->rows=(Page){.column_count=measures,.row_count=page->row_count,.offset=page->offset,.total=page->total,.bytes=row_bytes};
    totals->rows.columns=dx_alloc(measures*sizeof(Column)); if (page->row_count) totals->rows.cells=dx_alloc(page->row_count*measures*sizeof(Cell));
    for (size_t m=0;m<measures;m++) {
        const Column *source=&page->columns[rows+m];
        totals->rows.columns[m]=(Column){.name=dx_printf("m%zu",m),.type_name=dx_strdup(source->type_name),.type=source->type,.number_type=source->number_type,.numeric=source->numeric};
        for (size_t r=0;r<page->row_count;r++) { const Cell *cell=&page->cells[r*page->column_count+rows+m]; totals->rows.cells[r*measures+m]=*cell; totals->rows.cells[r*measures+m].text=cell->is_null ? NULL : dx_strdup(cell->text); }
        page->columns[rows+m].caption=captions[m]; captions[m]=NULL;
    }
    for (size_t c=0;c<rows;c++) if (labels[c]) { free(page->columns[c].name); page->columns[c].name=labels[c]; labels[c]=NULL; page->columns[c].date_grain=p->fields[PIVOT_ROWS][c].grain; }
    totals->columns=*grand; memset(grand,0,sizeof(*grand)); page->totals=totals; page->bytes=used; ok=true;
done:
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) { free(labels[i]); free(captions[i]); } return ok;
}
