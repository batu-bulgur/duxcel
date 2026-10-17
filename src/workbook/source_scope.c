#include "source_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DXFS v1: magic, u16 version, u8 packet kind, u8 source tag, u32 total
   bytes, u32 reserved, u64 owner, u64 source version. All integers LE.
   RAW: u32 reader, u32 fields, u64 file bytes, SHA256[32], text engine,
        fields {u32 ordinal,u16 SQL tag,u8 precision,u8 scale,text name}.
   RESULT: u32 raw bytes, complete RAW packet, u32 filter count,
        filters {u32 ordinal,u32 FilterOp,nullable text value}, u32 sorts,
        sorts {u32 ordinal,u32 descending}. Text=u32 bytes, bytes, zero byte;
        UINT32_MAX alone means SQL NULL. No padding or arbitrary payloads. */
enum { SCOPE_HEADER=32,SCOPE_CODEC=1 };
typedef struct { const unsigned char *data; size_t bytes,at; bool ok; } Read;
typedef struct { unsigned char *data; size_t at; } Write;
typedef struct { uint32_t ordinal,op; const char *value; size_t bytes; } Filter;
typedef struct { uint32_t ordinal; bool descending; } Sort;
typedef struct {
    SourceScopeInfo info;
    Filter filters[DX_ANALYSIS_FILTERS]; size_t filter_count;
    Sort sorts[DX_ANALYSIS_SORTS]; size_t sort_count;
} Parsed;
typedef struct { unsigned tag; duckdb_type type; const char *name; NumberKind number; } SqlType;
/* Stable codec tags, independent of DuckDB's enum numbering. */
static const SqlType types[]={
    {1,DUCKDB_TYPE_BOOLEAN,"BOOLEAN",NUM_NONE},
    {2,DUCKDB_TYPE_TINYINT,"TINYINT",NUM_I8},{3,DUCKDB_TYPE_SMALLINT,"SMALLINT",NUM_I16},
    {4,DUCKDB_TYPE_INTEGER,"INTEGER",NUM_I32},{5,DUCKDB_TYPE_BIGINT,"BIGINT",NUM_I64},
    {6,DUCKDB_TYPE_HUGEINT,"HUGEINT",NUM_I128},
    {7,DUCKDB_TYPE_UTINYINT,"UTINYINT",NUM_U8},{8,DUCKDB_TYPE_USMALLINT,"USMALLINT",NUM_U16},
    {9,DUCKDB_TYPE_UINTEGER,"UINTEGER",NUM_U32},{10,DUCKDB_TYPE_UBIGINT,"UBIGINT",NUM_U64},
    {11,DUCKDB_TYPE_UHUGEINT,"UHUGEINT",NUM_U128},
    {12,DUCKDB_TYPE_FLOAT,"FLOAT",NUM_FLOAT},{13,DUCKDB_TYPE_DOUBLE,"DOUBLE",NUM_FLOAT},
    {14,DUCKDB_TYPE_DECIMAL,"DECIMAL",NUM_DECIMAL},
    {15,DUCKDB_TYPE_DATE,"DATE",NUM_NONE},{16,DUCKDB_TYPE_TIME,"TIME",NUM_NONE},
    {17,DUCKDB_TYPE_TIMESTAMP,"TIMESTAMP",NUM_NONE},{18,DUCKDB_TYPE_TIMESTAMP_S,"TIMESTAMP_S",NUM_NONE},
    {19,DUCKDB_TYPE_TIMESTAMP_MS,"TIMESTAMP_MS",NUM_NONE},{20,DUCKDB_TYPE_TIMESTAMP_NS,"TIMESTAMP_NS",NUM_NONE},
    {21,DUCKDB_TYPE_TIMESTAMP_TZ,"TIMESTAMPTZ",NUM_NONE},{22,DUCKDB_TYPE_TIME_TZ,"TIMETZ",NUM_NONE},
    {23,DUCKDB_TYPE_INTERVAL,"INTERVAL",NUM_NONE},{24,DUCKDB_TYPE_VARCHAR,"VARCHAR",NUM_NONE},
    {25,DUCKDB_TYPE_BLOB,"BLOB",NUM_NONE},{26,DUCKDB_TYPE_UUID,"UUID",NUM_NONE},
    {27,DUCKDB_TYPE_SQLNULL,"NULL",NUM_NONE}
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool add(size_t *total,size_t bytes,size_t limit) {
    if (bytes>limit || *total>limit-bytes) return false; *total+=bytes; return true;
}
static size_t length(const char *s,size_t limit) {
    if (!s) return SIZE_MAX; size_t n=0; while (n<=limit && s[n]) n++; return n;
}
static bool utf8(const unsigned char *s,size_t n) {
    for (size_t i=0;i<n;) {
        unsigned c=s[i++],value,minimum; size_t tail;
        if (!c) return false;
        if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) { value=c&31; minimum=0x80; tail=1; }
        else if (c>=0xe0 && c<=0xef) { value=c&15; minimum=0x800; tail=2; }
        else if (c>=0xf0 && c<=0xf4) { value=c&7; minimum=0x10000; tail=3; }
        else return false;
        if (tail>n-i) return false;
        while (tail--) { unsigned b=s[i++]; if ((b&0xc0)!=0x80) return false; value=(value<<6)|(b&63); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static bool text_size(const char *s,size_t limit,bool empty,size_t *n) {
    size_t bytes=length(s,limit);
    if (bytes>limit || (!empty && !bytes) || !utf8((const unsigned char *)s,bytes)) return false;
    *n=bytes; return true;
}
static bool filter_valid(FilterOp op,const char *value) {
    if (!analysis_filter_value_valid(op,value)) return false;
    if (op>=FILTER_IN && op<=FILTER_DATE_NOT_IN) {
        char **values=NULL; size_t count=0; bool valid=analysis_parse_values(value,&values,&count);
        for (size_t i=0;i<count;i++) free(values[i]); free(values); return valid;
    }
    return true;
}
static unsigned fold(unsigned c) { return c>='A' && c<='Z' ? c+32 : c; }
static int names_compare(const void *a,const void *b) {
    const unsigned char *x=(const unsigned char *)*(const char *const *)a,*y=(const unsigned char *)*(const char *const *)b;
    while (*x && *y && fold(*x)==fold(*y)) { x++; y++; } return (int)fold(*x)-(int)fold(*y);
}
static bool same_name(const char *a,const char *b) { return names_compare(&a,&b)==0; }
static const SqlType *sql_tag(unsigned tag) {
    for (size_t i=0;i<sizeof(types)/sizeof(*types);i++) if (types[i].tag==tag) return types+i; return NULL;
}
static const SqlType *sql_type(duckdb_type type) {
    for (size_t i=0;i<sizeof(types)/sizeof(*types);i++) if (types[i].type==type) return types+i; return NULL;
}
static bool number_valid(const SqlType *type,NumberType number) {
    return type && type->number==number.kind && (number.kind==NUM_DECIMAL ? number.precision>0 && number.precision<=38 && number.scale<=number.precision : !number.precision && !number.scale);
}
static const unsigned char *get(Read *r,size_t n) {
    if (!r->ok || n>r->bytes-r->at) { r->ok=false; return NULL; }
    const unsigned char *p=r->data+r->at; r->at+=n; return p;
}
static uint64_t integer(Read *r,size_t n) {
    const unsigned char *p=get(r,n); uint64_t value=0;
    if (p) for (size_t i=0;i<n;i++) value|=(uint64_t)p[i]<<(i*8); return value;
}
static const char *text(Read *r,size_t limit,bool nullable,bool empty,size_t *bytes) {
    uint32_t n=(uint32_t)integer(r,4);
    if (!r->ok) return NULL;
    if (n==UINT32_MAX && nullable) { if (bytes) *bytes=0; return NULL; }
    if (n>limit || (!empty && !n)) { r->ok=false; return NULL; }
    const unsigned char *p=get(r,(size_t)n+1);
    if (!p || p[n] || !utf8(p,n)) { r->ok=false; return NULL; }
    if (bytes) *bytes=n; return (const char *)p;
}
static void put(Write *w,const void *p,size_t n) { memcpy(w->data+w->at,p,n); w->at+=n; }
static void put_integer(Write *w,uint64_t v,size_t n) {
    for (size_t i=0;i<n;i++) { w->data[w->at++]=(unsigned char)v; v>>=8; }
}
static void put_text(Write *w,const char *p,size_t n) {
    put_integer(w,p ? n : UINT32_MAX,4); if (p) put(w,p,n+1);
}
static void header(Write *w,SourceScopeKind kind,SourceKind source,size_t bytes,uint64_t owner,uint64_t version) {
    put(w,"DXFS",4); put_integer(w,SCOPE_CODEC,2); put_integer(w,kind,1);
    put_integer(w,source==SOURCE_CSV ? 1 : 2,1); put_integer(w,bytes,4); put_integer(w,0,4);
    put_integer(w,owner,8); put_integer(w,version,8);
}
static bool read_header(Read *r,SourceScopeInfo *info,SourceScopeKind expected) {
    const unsigned char *magic=get(r,4); uint64_t codec=integer(r,2),kind=integer(r,1),source=integer(r,1),bytes=integer(r,4),reserved=integer(r,4);
    info->owner=integer(r,8); info->version=integer(r,8);
    if (!r->ok || !magic || memcmp(magic,"DXFS",4) || codec!=SCOPE_CODEC || kind!=(unsigned)expected || (source!=1 && source!=2) || bytes!=r->bytes || reserved || !info->version) return false;
    info->kind=expected; info->source_kind=source==1 ? SOURCE_CSV : SOURCE_PARQUET; return true;
}
static bool field_read(Read *r,size_t expected,SourceScopeField *field) {
    uint64_t ordinal=integer(r,4),tag=integer(r,2),precision=integer(r,1),scale=integer(r,1);
    const SqlType *type=sql_tag((unsigned)tag); const char *name=text(r,DX_SOURCE_SCOPE_NAME_BYTES,false,false,NULL);
    NumberType number={type ? type->number : NUM_NONE,(unsigned char)precision,(unsigned char)scale};
    if (!r->ok || ordinal!=expected || !number_valid(type,number)) return false;
    if (field) *field=(SourceScopeField){(uint32_t)ordinal,name,type->type,number}; return true;
}
static bool parse_raw(const void *data,size_t bytes,SourceScopeInfo *out) {
    if (!data || bytes<SCOPE_HEADER || bytes>=DX_SOURCE_SCOPE_BYTES) return false;
    Read r={data,bytes,0,true}; SourceScopeInfo info={0};
    if (!read_header(&r,&info,SOURCE_SCOPE_RAW)) return false;
    info.reader=(SourceReaderPolicy)integer(&r,4); info.field_count=(size_t)integer(&r,4); info.file_bytes=integer(&r,8); info.sha256=get(&r,DX_SOURCE_DIGEST_BYTES);
    info.engine_version=text(&r,DX_SOURCE_SCOPE_ENGINE_BYTES,false,false,NULL);
    if (!r.ok || info.reader!=(info.source_kind==SOURCE_CSV ? SOURCE_READER_CSV_ORDINAL_V1 : SOURCE_READER_PARQUET_ORDINAL_V1) || !info.field_count || info.field_count>DX_SOURCE_SCOPE_FIELDS) return false;
    info.fields=r.data+r.at; size_t start=r.at;
    const char *names[DX_SOURCE_SCOPE_FIELDS];
    for (size_t i=0;i<info.field_count;i++) { SourceScopeField field; if (!field_read(&r,i,&field)) return false; names[i]=field.name; }
    if (r.at!=r.bytes) return false;
    qsort(names,info.field_count,sizeof(*names),names_compare);
    for (size_t i=1;i<info.field_count;i++) if (!names_compare(names+i-1,names+i)) return false;
    info.raw=data; info.raw_bytes=bytes; info.field_bytes=r.at-start; *out=info; return true;
}
static bool parse(const void *data,size_t bytes,Parsed *out) {
    if (!data || bytes<SCOPE_HEADER || bytes>=DX_SOURCE_SCOPE_BYTES) return false;
    const unsigned char *p=data;
    if (p[6]==SOURCE_SCOPE_RAW) { Parsed parsed={0}; if (!parse_raw(data,bytes,&parsed.info)) return false; *out=parsed; return true; }
    Read r={data,bytes,0,true}; Parsed parsed={0}; SourceScopeInfo outer={0};
    if (!read_header(&r,&outer,SOURCE_SCOPE_RESULT)) return false;
    size_t raw_bytes=(size_t)integer(&r,4); const unsigned char *raw=get(&r,raw_bytes);
    if (!r.ok || !parse_raw(raw,raw_bytes,&parsed.info) || outer.owner!=parsed.info.owner || outer.version!=parsed.info.version || outer.source_kind!=parsed.info.source_kind) return false;
    parsed.info.kind=SOURCE_SCOPE_RESULT;
    parsed.filter_count=(size_t)integer(&r,4);
    if (!r.ok || parsed.filter_count>DX_ANALYSIS_FILTERS) return false;
    for (size_t i=0;i<parsed.filter_count;i++) {
        Filter *filter=parsed.filters+i; filter->ordinal=(uint32_t)integer(&r,4); filter->op=(uint32_t)integer(&r,4);
        filter->value=text(&r,DX_SOURCE_SCOPE_TEXT_BYTES,true,true,&filter->bytes);
        if (!r.ok || filter->ordinal>=parsed.info.field_count || (i && filter->ordinal<=parsed.filters[i-1].ordinal) || filter->op>FILTER_OP_MAX || !filter_valid((FilterOp)filter->op,filter->value)) return false;
    }
    parsed.sort_count=(size_t)integer(&r,4);
    if (!r.ok || parsed.sort_count>DX_ANALYSIS_SORTS) return false;
    for (size_t i=0;i<parsed.sort_count;i++) {
        uint64_t ordinal=integer(&r,4),direction=integer(&r,4);
        if (!r.ok || ordinal>=parsed.info.field_count || direction>1) return false;
        for (size_t j=0;j<i;j++) if (parsed.sorts[j].ordinal==ordinal) return false;
        parsed.sorts[i]=(Sort){(uint32_t)ordinal,direction!=0};
    }
    if (!r.ok || r.at!=r.bytes) return false; *out=parsed; return true;
}
bool source_scope_validate(const void *data,size_t bytes,SourceScopeInfo *out,char **error) {
    Parsed parsed;
    if (!out || !parse(data,bytes,&parsed)) return fail(error,"Invalid or unsupported canonical source scope packet.");
    *out=parsed.info; return true;
}
bool source_scope_field(const void *data,size_t bytes,size_t ordinal,SourceScopeField *out,char **error) {
    Parsed parsed;
    if (!out || !parse(data,bytes,&parsed) || ordinal>=parsed.info.field_count) return fail(error,"Invalid canonical source field ordinal.");
    Read r={parsed.info.fields,parsed.info.field_bytes,0,true}; SourceScopeField field;
    for (size_t i=0;i<=ordinal;i++) if (!field_read(&r,i,&field)) return fail(error,"Invalid canonical source field.");
    *out=field; return true;
}
static bool source_valid(const Analysis *a,char **error) {
    size_t n;
    if (!a || (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET) || !text_size(a->path,DX_SOURCE_SCOPE_TEXT_BYTES,false,&n) || strpbrk(a->path,"*?[]") || a->schema || a->table || a->query || a->unique_rows || a->group_column || a->measure_column || (unsigned)a->aggregate>AGG_MAX || a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || (a->filter_count && !a->filter_column) || (a->sort_count && !a->sort_column)) return fail(error,"Canonical record scopes require one raw CSV or Parquet source.");
    for (size_t i=0;i<a->filter_count+(a->filter_column ? 1 : 0);i++) {
        const char *name=i ? a->filters[i-1].column : a->filter_column,*value=i ? a->filters[i-1].value : a->filter_value;
        FilterOp op=i ? a->filters[i-1].op : a->filter_op;
        if (!text_size(name,DX_SOURCE_SCOPE_NAME_BYTES,false,&n) || (value && !text_size(value,DX_SOURCE_SCOPE_TEXT_BYTES,true,&n)) || !filter_valid(op,value)) return fail(error,"Source filter payload exceeds or violates the canonical UTF-8 scope limit.");
    }
    for (size_t i=0;i<a->sort_count+(a->sort_column ? 1 : 0);i++) if (!text_size(analysis_sort_at(a,i).column,DX_SOURCE_SCOPE_NAME_BYTES,false,&n)) return fail(error,"Invalid canonical source sort field.");
    return analysis_filters_valid(a,error) && analysis_sorts_valid(a,error);
}
static bool column_valid(const Column *column,const SqlType **out,size_t *name_bytes) {
    const SqlType *type=sql_type(column->type); char decimal[32]; const char *name=type ? type->name : NULL;
    if (!number_valid(type,column->number_type) || column->numeric!=(type->number!=NUM_NONE) || !text_size(column->name,DX_SOURCE_SCOPE_NAME_BYTES,false,name_bytes)) return false;
    if (type->number==NUM_DECIMAL) { snprintf(decimal,sizeof(decimal),"DECIMAL(%u,%u)",column->number_type.precision,column->number_type.scale); name=decimal; }
    size_t n;
    if (!text_size(column->type_name,128,false,&n) || strcmp(column->type_name,name)) return false;
    *out=type; return true;
}
static bool schema_valid(const Page *schema,size_t *bytes,char **error) {
    if (!schema || !schema->columns || !schema->column_count || schema->column_count>DX_SOURCE_SCOPE_FIELDS) return fail(error,"A canonical scope needs the complete original typed source schema.");
    const char *names[DX_SOURCE_SCOPE_FIELDS]; size_t total=0;
    for (size_t i=0;i<schema->column_count;i++) {
        const SqlType *type; size_t n;
        if (!column_valid(schema->columns+i,&type,&n)) return fail(error,"Source schema has an unsupported or inconsistent exact logical type.");
        if (!add(&total,13+n,DX_SOURCE_SCOPE_BYTES-1)) return fail(error,"Canonical source schema exceeds the 256 KiB packet limit.");
        names[i]=schema->columns[i].name;
    }
    qsort(names,schema->column_count,sizeof(*names),names_compare);
    for (size_t i=1;i<schema->column_count;i++) if (!names_compare(names+i-1,names+i)) return fail(error,"Ambiguous original source field names cannot bind record styles.");
    *bytes=total; return true;
}
static size_t field_ordinal(const SourceScopeInfo *info,const char *name,SourceScopeField *out) {
    Read r={info->fields,info->field_bytes,0,true};
    for (size_t i=0;i<info->field_count;i++) { SourceScopeField field; if (!field_read(&r,i,&field)) return SIZE_MAX; if (same_name(name,field.name)) { if (out) *out=field; return i; } }
    return SIZE_MAX;
}
static int filters_compare(const void *a,const void *b) {
    uint32_t x=((const Filter *)a)->ordinal,y=((const Filter *)b)->ordinal; return x<y ? -1 : x>y;
}
static bool transforms(const SourceScopeInfo *info,const Analysis *a,Parsed *out,char **error) {
    if (!source_valid(a,error)) return false;
    if (a->kind!=info->source_kind) return fail(error,"The Analysis source kind does not match its canonical binding.");
    Parsed next={0}; next.filter_count=a->filter_count+(a->filter_column ? 1 : 0); next.sort_count=a->sort_count+(a->sort_column ? 1 : 0);
    for (size_t i=0;i<next.filter_count;i++) {
        const char *name=i ? a->filters[i-1].column : a->filter_column,*value=i ? a->filters[i-1].value : a->filter_value;
        NumberType number=i ? a->filters[i-1].number : a->filter_number; SourceScopeField field;
        FilterOp op=i ? a->filters[i-1].op : a->filter_op; size_t ordinal=field_ordinal(info,name,&field);
        if (ordinal==SIZE_MAX) return fail(error,"Source filter is not an original schema field.");
        if (number.kind!=field.number.kind || number.precision!=field.number.precision || number.scale!=field.number.scale) return fail(error,"Source filter numeric binding is unresolved or differs from its exact schema field.");
        next.filters[i]=(Filter){(uint32_t)ordinal,(uint32_t)op,value,value ? strlen(value) : 0};
    }
    qsort(next.filters,next.filter_count,sizeof(*next.filters),filters_compare);
    for (size_t i=0;i<next.sort_count;i++) {
        AnalysisSort sort=analysis_sort_at(a,i); size_t ordinal=field_ordinal(info,sort.column,NULL);
        if (ordinal==SIZE_MAX) return fail(error,"Source sort is not an original schema field.");
        next.sorts[i]=(Sort){(uint32_t)ordinal,sort.descending};
    }
    *out=next; return true;
}
void source_packet_free(SourcePacket *packet) { if (packet) { free(packet->data); *packet=(SourcePacket){0}; } }
bool source_scope_raw_build(uint64_t owner,uint64_t version,const Analysis *source,const Page *schema,const SourceDigest *verified,const char *engine,SourcePacket *out,char **error) {
    size_t fields,engine_bytes;
    if (!out || !version || !verified || verified->bytes!=verified->identity.size || !text_size(engine,DX_SOURCE_SCOPE_ENGINE_BYTES,false,&engine_bytes)) return fail(error,"Invalid verified source scope input.");
    if (!source_valid(source,error) || !schema_valid(schema,&fields,error)) return false;
    size_t bytes=SCOPE_HEADER+48;
    if (!add(&bytes,engine_bytes+5,DX_SOURCE_SCOPE_BYTES-1) || !add(&bytes,fields,DX_SOURCE_SCOPE_BYTES-1)) return fail(error,"Canonical source scope exceeds its 256 KiB packet limit.");
    SourcePacket next={dx_alloc(bytes),bytes}; Write w={next.data,0}; header(&w,SOURCE_SCOPE_RAW,source->kind,bytes,owner,version);
    put_integer(&w,source->kind==SOURCE_CSV ? SOURCE_READER_CSV_ORDINAL_V1 : SOURCE_READER_PARQUET_ORDINAL_V1,4); put_integer(&w,schema->column_count,4); put_integer(&w,verified->bytes,8); put(&w,verified->sha256,DX_SOURCE_DIGEST_BYTES); put_text(&w,engine,engine_bytes);
    for (size_t i=0;i<schema->column_count;i++) {
        const Column *col=schema->columns+i; const SqlType *type=sql_type(col->type);
        put_integer(&w,i,4); put_integer(&w,type->tag,2); put_integer(&w,col->number_type.precision,1); put_integer(&w,col->number_type.scale,1); put_text(&w,col->name,strlen(col->name));
    }
    /* Validate transform fields too, though raw identity excludes transforms. */
    Parsed parsed,shape;
    if (w.at!=bytes || !parse(next.data,next.bytes,&parsed) || !transforms(&parsed.info,source,&shape,error)) { source_packet_free(&next); return error && *error ? false : fail(error,"Could not encode a canonical raw source scope."); }
    source_packet_free(out); *out=next; return true;
}
bool source_scope_result_build(const SourcePacket *raw,const Analysis *analysis,SourcePacket *out,char **error) {
    Parsed parsed,shape;
    if (!out || !raw || !parse(raw->data,raw->bytes,&parsed) || parsed.info.kind!=SOURCE_SCOPE_RAW) return fail(error,"A result scope needs a complete canonical raw binding.");
    if (!transforms(&parsed.info,analysis,&shape,error)) return false;
    size_t bytes=SCOPE_HEADER+12;
    if (!add(&bytes,raw->bytes,DX_SOURCE_SCOPE_BYTES-1)) return fail(error,"Canonical result scope exceeds its 256 KiB packet limit.");
    for (size_t i=0;i<shape.filter_count;i++) if (!add(&bytes,12+(shape.filters[i].value ? shape.filters[i].bytes+1 : 0),DX_SOURCE_SCOPE_BYTES-1)) return fail(error,"Canonical result filters exceed their packet budget.");
    if (!add(&bytes,shape.sort_count*8,DX_SOURCE_SCOPE_BYTES-1)) return fail(error,"Canonical result sorts exceed their packet budget.");
    SourcePacket next={dx_alloc(bytes),bytes}; Write w={next.data,0}; header(&w,SOURCE_SCOPE_RESULT,parsed.info.source_kind,bytes,parsed.info.owner,parsed.info.version); put_integer(&w,raw->bytes,4); put(&w,raw->data,raw->bytes); put_integer(&w,shape.filter_count,4);
    for (size_t i=0;i<shape.filter_count;i++) { Filter f=shape.filters[i]; put_integer(&w,f.ordinal,4); put_integer(&w,f.op,4); put_text(&w,f.value,f.bytes); }
    put_integer(&w,shape.sort_count,4);
    for (size_t i=0;i<shape.sort_count;i++) { put_integer(&w,shape.sorts[i].ordinal,4); put_integer(&w,shape.sorts[i].descending,4); }
    if (w.at!=bytes) { source_packet_free(&next); return fail(error,"Could not encode a canonical result scope."); }
    source_packet_free(out); *out=next; return true;
}
bool source_scope_schema_matches(const void *data,size_t bytes,const Page *schema,char **error) {
    Parsed parsed; size_t owned;
    if (!parse(data,bytes,&parsed)) return fail(error,"Invalid canonical source scope packet.");
    if (!schema_valid(schema,&owned,error)) return false;
    if (schema->column_count!=parsed.info.field_count) return fail(error,"Source field count no longer matches its canonical scope.");
    Read r={parsed.info.fields,parsed.info.field_bytes,0,true};
    for (size_t i=0;i<schema->column_count;i++) {
        SourceScopeField field; const Column *column=schema->columns+i;
        if (!field_read(&r,i,&field) || strcmp(field.name,column->name) || field.type!=column->type || field.number.kind!=column->number_type.kind || field.number.precision!=column->number_type.precision || field.number.scale!=column->number_type.scale) return fail(error,"Source field order, name or exact type no longer matches its canonical scope.");
    }
    return true;
}
static bool entry_valid(const SourceVersion *entry,uint64_t owners,size_t *bytes,char **error) {
    size_t n; Parsed parsed;
    if (!entry || !entry->version || entry->owner>owners || (entry->kind!=SOURCE_CSV && entry->kind!=SOURCE_PARQUET) || !text_size(entry->path,DX_SOURCE_SCOPE_TEXT_BYTES,false,&n) || strpbrk(entry->path,"*?[]") || ((entry->raw.data!=NULL)!=(entry->raw.bytes!=0))) return fail(error,"Invalid source-version owner, locator or pending state.");
    if (entry->raw.bytes && (!parse(entry->raw.data,entry->raw.bytes,&parsed) || parsed.info.kind!=SOURCE_SCOPE_RAW || parsed.info.owner!=entry->owner || parsed.info.version!=entry->version || parsed.info.source_kind!=entry->kind)) return fail(error,"Source-version proof does not match its owner, version or kind.");
    size_t owned=n+1;
    if (!add(&owned,entry->raw.bytes,DX_SOURCE_VERSIONS_BYTES)) return fail(error,"Source-version proof exceeds its owned storage limit.");
    *bytes=owned; return true;
}
bool source_versions_valid(const SourceVersions *versions,uint64_t owners,char **error) {
    if (!versions || versions->count>DX_SOURCE_VERSIONS || versions->capacity>DX_SOURCE_VERSIONS || versions->count>versions->capacity || ((versions->items!=NULL)!=(versions->capacity!=0))) return fail(error,"Invalid bounded source-version ledger.");
    size_t bytes=versions->capacity*sizeof(*versions->items);
    for (size_t i=0;i<versions->count;i++) {
        size_t owned;
        if (!entry_valid(versions->items+i,owners,&owned,error)) return false;
        if (versions->items[i].version>versions->high_water || (i && versions->items[i].version<=versions->items[i-1].version)) return fail(error,"Source-version IDs exceed or violate their monotonic high-water.");
        if (!add(&bytes,owned,DX_SOURCE_VERSIONS_BYTES)) return fail(error,"Source-version ledger exceeds its 1 MiB owned storage limit.");
    }
    for (size_t i=versions->count;i<versions->capacity;i++) {
        const SourceVersion *entry=versions->items+i;
        if (entry->path || entry->raw.data || entry->raw.bytes || entry->version || entry->owner || entry->kind) return fail(error,"Unused source-version slots must own no state.");
    }
    if (bytes!=versions->bytes || bytes>DX_SOURCE_VERSIONS_BYTES) return fail(error,"Invalid source-version owned-byte accounting.");
    return true;
}
void source_versions_free(SourceVersions *versions) {
    if (!versions) return;
    size_t capacity=versions->capacity<DX_SOURCE_VERSIONS ? versions->capacity : DX_SOURCE_VERSIONS;
    if (versions->items) for (size_t i=0;i<capacity;i++) { free(versions->items[i].path); source_packet_free(&versions->items[i].raw); }
    free(versions->items); *versions=(SourceVersions){0};
}
static void entry_copy(SourceVersion *out,const SourceVersion *from) {
    *out=*from; out->path=dx_strdup(from->path);
    if (from->raw.bytes) { out->raw.data=dx_alloc(from->raw.bytes); memcpy(out->raw.data,from->raw.data,from->raw.bytes); }
}
bool source_versions_copy(SourceVersions *out,const SourceVersions *from,char **error) {
    if (!out || !source_versions_valid(from,UINT64_MAX,error)) return out ? false : fail(error,"Missing source-version copy destination.");
    if (out==from) return true;
    SourceVersions next={.high_water=from->high_water,.count=from->count,.capacity=from->count};
    next.bytes=next.capacity*sizeof(*next.items);
    if (next.count) next.items=dx_alloc(next.count*sizeof(*next.items));
    for (size_t i=0;i<next.count;i++) { size_t bytes=0; entry_valid(from->items+i,UINT64_MAX,&bytes,NULL); entry_copy(next.items+i,from->items+i); next.bytes+=bytes; }
    source_versions_free(out); *out=next; return true;
}
bool source_versions_plan(const SourceVersions *current,const SourceVersion *entry,bool issue,SourceVersions *out,char **error) {
    size_t entry_bytes,index=SIZE_MAX;
    if (!out || out==current) return fail(error,"Source-version planning requires a distinct owned output.");
    if (!source_versions_valid(current,UINT64_MAX,error) || !entry_valid(entry,UINT64_MAX,&entry_bytes,error)) return false;
    if (issue) {
        if (current->high_water==UINT64_MAX || entry->version!=current->high_water+1 || current->count==DX_SOURCE_VERSIONS) return fail(error,"Source-version high-water or retained-entry capacity is exhausted.");
        index=current->count;
    } else {
        for (size_t i=0;i<current->count;i++) if (current->items[i].version==entry->version) index=i;
        if (index==SIZE_MAX) return fail(error,"Cannot complete an unknown source version.");
        const SourceVersion *old=current->items+index;
        if (old->raw.bytes || !entry->raw.bytes || old->owner!=entry->owner || old->kind!=entry->kind || strcmp(old->path,entry->path)) return fail(error,"Only an unchanged pending source version can be completed once.");
    }
    size_t count=current->count+(issue ? 1 : 0),bytes=count*sizeof(SourceVersion);
    for (size_t i=0;i<count;i++) {
        size_t owned=entry_bytes;
        if (i!=index && !entry_valid(current->items+i,UINT64_MAX,&owned,error)) return false;
        if (!add(&bytes,owned,DX_SOURCE_VERSIONS_BYTES)) return fail(error,"The planned source-version ledger exceeds its 1 MiB owned limit.");
    }
    SourceVersions next={.high_water=issue ? entry->version : current->high_water,.count=count,.capacity=count,.bytes=bytes}; next.items=dx_alloc(count*sizeof(*next.items));
    for (size_t i=0;i<count;i++) entry_copy(next.items+i,i==index ? entry : current->items+i);
    source_versions_free(out); *out=next; return true;
}
bool source_versions_scope_matches(const SourceVersions *versions,uint64_t owner,uint64_t version,const Analysis *current,const void *scope,size_t bytes,char **error) {
    Parsed parsed,shape;
    if (!source_versions_valid(versions,UINT64_MAX,error) || !source_valid(current,error)) return false;
    if (!version || !parse(scope,bytes,&parsed) || parsed.info.owner!=owner || parsed.info.version!=version) return fail(error,"Source scope does not match the current owner/version.");
    const SourceVersion *entry=NULL;
    for (size_t i=0;i<versions->count;i++) if (versions->items[i].version==version) entry=versions->items+i;
    if (!entry || !entry->raw.bytes || entry->owner!=owner || entry->kind!=current->kind || strcmp(entry->path,current->path) || entry->raw.bytes!=parsed.info.raw_bytes || memcmp(entry->raw.data,parsed.info.raw,entry->raw.bytes)) return fail(error,"Source scope is pending or differs from its exact version binding.");
    if (!transforms(&parsed.info,current,&shape,error)) return false;
    if (parsed.info.kind==SOURCE_SCOPE_RESULT) {
        if (shape.filter_count!=parsed.filter_count || shape.sort_count!=parsed.sort_count) return fail(error,"Source result filters or sort order changed.");
        for (size_t i=0;i<shape.filter_count;i++) {
            Filter a=shape.filters[i],b=parsed.filters[i];
            if (a.ordinal!=b.ordinal || a.op!=b.op || (a.value && b.value ? strcmp(a.value,b.value)!=0 : a.value!=b.value)) return fail(error,"Source result filter definition changed.");
        }
        for (size_t i=0;i<shape.sort_count;i++) if (shape.sorts[i].ordinal!=parsed.sorts[i].ordinal || shape.sorts[i].descending!=parsed.sorts[i].descending) return fail(error,"Source result sort order changed.");
    }
    return true;
}
