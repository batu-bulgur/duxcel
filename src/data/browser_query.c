#include "browser_query.h"
#include "page_wire.h"
#include <ctype.h>
#include <inttypes.h>
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
