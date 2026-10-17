#include "record_sql.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RecordSQLStatus fail(RecordSQLStatus status,char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return status;
}
static size_t bounded_length(const char *s,size_t limit) {
    size_t n=0; if (!s) return 0; while (n<=limit && s[n]) n++; return n;
}
static bool add_bytes(size_t *total,size_t count,size_t limit) {
    if (count>limit || *total>limit-count) return false; *total+=count; return true;
}
static unsigned char folded(unsigned char c) { return c>='A' && c<='Z' ? (unsigned char)(c+'a'-'A') : c; }
static int compare_names(const void *left,const void *right) {
    const unsigned char *a=(const unsigned char *)*(const char *const *)left,*b=(const unsigned char *)*(const char *const *)right;
    while (*a && *b && folded(*a)==folded(*b)) { a++; b++; } return (int)folded(*a)-(int)folded(*b);
}
static bool has_name(const char *const *sorted,size_t count,const char *name) {
    return bsearch(&name,sorted,count,sizeof(*sorted),compare_names)!=NULL;
}
static bool utf8(const char *text,size_t n) {
    const unsigned char *s=(const unsigned char *)text;
    for (size_t i=0;i<n;) {
        unsigned char c=s[i++]; if (c<128) continue;
        size_t tail; unsigned value,minimum;
        if (c>=0xc2 && c<=0xdf) { tail=1; value=c&31; minimum=0x80; }
        else if (c>=0xe0 && c<=0xef) { tail=2; value=c&15; minimum=0x800; }
        else if (c>=0xf0 && c<=0xf4) { tail=3; value=c&7; minimum=0x10000; }
        else return false;
        if (tail>n-i) return false;
        while (tail--) { unsigned char next=s[i++]; if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&63); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static bool number_valid(NumberType type) {
    return type.kind==NUM_DECIMAL ? type.precision && type.precision<=38 && type.scale<=type.precision : (unsigned)type.kind<=NUM_FLOAT && !type.precision && !type.scale;
}
void record_sql_free(RecordSQL *query) {
    if (!query) return; free(query->relation); free(query->identity_name); *query=(RecordSQL){0};
}
RecordSQLStatus record_sql_build(RecordSQL *out,const Analysis *a,const char *const *names,size_t count,bool single_regular_file,bool parameters,char **error) {
    if (!out || !a || !names || !count || count>DX_RECORD_SQL_COLUMNS || count>SIZE_MAX/sizeof(*names)) return fail(RECORD_SQL_INVALID,error,"Invalid raw-record schema or output.");
    if (a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || (a->filter_count && !a->filter_column) || (a->sort_count && !a->sort_column)) return fail(RECORD_SQL_INVALID,error,"Invalid raw-record filter or sort counts.");
    if ((unsigned)a->kind>SOURCE_DEMO || (unsigned)a->aggregate>AGG_MAX) return fail(RECORD_SQL_INVALID,error,"Invalid raw-record source model.");
    if (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET && a->kind!=SOURCE_DEMO) return fail(RECORD_SQL_UNSUPPORTED,error,"Record ordinals require a raw CSV, Parquet or demo source.");
    if (a->query || a->unique_rows || a->group_column || a->measure_column) return fail(RECORD_SQL_UNSUPPORTED,error,"Queries, DISTINCT and aggregation have no raw-record projection.");
    if (a->kind!=SOURCE_DEMO && (!a->path || !*a->path)) return fail(RECORD_SQL_INVALID,error,"A raw-record file source needs a path.");
    if (a->kind!=SOURCE_DEMO && !single_regular_file) return fail(RECORD_SQL_UNSUPPORTED,error,"Record ordinals require one caller-verified regular file.");

    size_t bytes=0,projection_bytes=0; bool supported=true;
    const char **sorted=dx_alloc(count*sizeof(*sorted));
    for (size_t i=0;i<count;i++) {
        if (!names[i] || !*names[i]) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Invalid raw-record business field."); }
        size_t n=bounded_length(names[i],DX_RECORD_SQL_NAME_BYTES);
        if (n>DX_RECORD_SQL_NAME_BYTES) { free(sorted); return fail(RECORD_SQL_UNSUPPORTED,error,"Business field exceeds the optional record-name budget."); }
        if (!utf8(names[i],n)) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Raw-record field names must be UTF-8."); }
        supported&=add_bytes(&bytes,n+1,DX_RECORD_SQL_INPUT_BYTES);
        size_t quoted=n+2; for (size_t j=0;j<n;j++) quoted+=names[i][j]=='"';
        if (!add_bytes(&projection_bytes,quoted+(i!=0),DX_RECORD_SQL_BYTES)) supported=false;
        sorted[i]=names[i];
    }
    qsort(sorted,count,sizeof(*sorted),compare_names);
    for (size_t i=1;i<count;i++) if (!compare_names(sorted+i-1,sorted+i)) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Duplicate raw-record business field binding."); }
    if (a->kind==SOURCE_PARQUET && has_name(sorted,count,"file_row_number")) { free(sorted); return fail(RECORD_SQL_UNSUPPORTED,error,"Parquet business field file_row_number conflicts with reader metadata."); }

    const char *texts[]={a->path,a->schema,a->table,a->filter_column,a->filter_value,a->sort_column};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (texts[i]) { size_t n=bounded_length(texts[i],DX_RECORD_SQL_INPUT_BYTES); supported&=n<=DX_RECORD_SQL_INPUT_BYTES && add_bytes(&bytes,n+1,DX_RECORD_SQL_INPUT_BYTES); }
    for (size_t i=0;i<a->filter_count;i++) {
        const char *parts[]={a->filters[i].column,a->filters[i].value};
        for (size_t j=0;j<2;j++) if (parts[j]) { size_t n=bounded_length(parts[j],DX_RECORD_SQL_INPUT_BYTES); supported&=n<=DX_RECORD_SQL_INPUT_BYTES && add_bytes(&bytes,n+1,DX_RECORD_SQL_INPUT_BYTES); }
    }
    for (size_t i=0;i<a->sort_count;i++) if (a->sorts[i].column) { size_t n=bounded_length(a->sorts[i].column,DX_RECORD_SQL_INPUT_BYTES); supported&=n<=DX_RECORD_SQL_INPUT_BYTES && add_bytes(&bytes,n+1,DX_RECORD_SQL_INPUT_BYTES); }
    if (!supported) { free(sorted); return fail(RECORD_SQL_UNSUPPORTED,error,"Source/schema text exceeds the optional raw-record SQL budget."); }
    if (!analysis_filters_valid(a,error) || !analysis_sorts_valid(a,error)) { free(sorted); return RECORD_SQL_INVALID; }
    for (size_t i=0;i<a->filter_count+(a->filter_column ? 1 : 0);i++) {
        if (!has_name(sorted,count,i ? a->filters[i-1].column : a->filter_column)) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Raw-record filter is not a business field."); }
        if (!number_valid(i ? a->filters[i-1].number : a->filter_number)) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Invalid raw-record numeric filter type."); }
    }
    for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) if (!has_name(sorted,count,analysis_sort_at(a,i).column)) { free(sorted); return fail(RECORD_SQL_INVALID,error,"Raw-record sort is not a business field."); }

    char alias[64]; size_t suffix=0;
    do { if (suffix) snprintf(alias,sizeof(alias),"__duxcel_record_id_%zu",suffix); else snprintf(alias,sizeof(alias),"__duxcel_record_id"); suffix++; } while (has_name(sorted,count,alias));
    free(sorted);
    char *projection=dx_alloc(projection_bytes+1),*at=projection;
    for (size_t i=0;i<count;i++) {
        if (i) *at++=','; *at++='"';
        for (const char *p=names[i];*p;p++) { if (*p=='"') *at++='"'; *at++=*p; } *at++='"';
    }
    *at=0;
    RecordSQL next={.identity_name=dx_strdup(alias),.business_columns=count,.identity_column=count,.kind=a->kind==SOURCE_PARQUET ? RECORD_ID_PARQUET : RECORD_ID_WINDOW};
    char *raw=analysis_source_sql(a,a->kind==SOURCE_PARQUET),*quoted=dx_quote(alias,'"');
    const char *identity=a->kind==SOURCE_PARQUET ? "CAST(\"file_row_number\" AS UBIGINT)+1" : "row_number() OVER ()::UBIGINT";
    char *projected=dx_printf("SELECT %s,%s AS %s\nFROM (\n%s\n) AS record_input",projection,identity,quoted,raw);
    free(projection); free(raw); next.relation=analysis_wrap_sql(a,parameters,projected);
    if (a->sort_column) dx_append(&next.relation,", %s ASC",quoted); free(quoted);
    if (strlen(next.relation)>DX_RECORD_SQL_BYTES) { record_sql_free(&next); return fail(RECORD_SQL_UNSUPPORTED,error,"Optional raw-record relation exceeds 1 MiB."); }
    record_sql_free(out); *out=next; return RECORD_SQL_READY;
}
