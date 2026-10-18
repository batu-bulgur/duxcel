#include "page_wire.h"
#include <stdlib.h>
#include <string.h>

typedef struct { const unsigned char *bytes; size_t size,position; } Reader;
typedef struct { const unsigned char *bytes; size_t size; } Text;
typedef struct { Text name,type,kind; NumberType number; duckdb_type sql_type; } WireColumn;

static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); }
    return false;
}
static uint32_t u32(const unsigned char *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1]<<8 |
        (uint32_t)bytes[2]<<16 | (uint32_t)bytes[3]<<24;
}
static bool take(Reader *reader,size_t size,Text *text) {
    if (size>reader->size-reader->position) return false;
    *text=(Text){reader->bytes+reader->position,size}; reader->position+=size; return true;
}
static bool word(Reader *reader,uint32_t *value) {
    Text text; if (!take(reader,4,&text)) return false;
    *value=u32(text.bytes); return true;
}
/* Strict scalar-value UTF-8, rejecting NUL, overlong forms, surrogates and
   values beyond U+10FFFF. Validate each cell separately, including boundaries. */
static bool utf8(Text text) {
    for (size_t i=0;i<text.size;) {
        unsigned char first=text.bytes[i++];
        if (!first) return false;
        if (first<0x80) continue;
        unsigned count; uint32_t value,minimum;
        if (first>=0xc2 && first<=0xdf) { count=1; value=first&0x1f; minimum=0x80; }
        else if (first>=0xe0 && first<=0xef) { count=2; value=first&0x0f; minimum=0x800; }
        else if (first>=0xf0 && first<=0xf4) { count=3; value=first&7; minimum=0x10000; }
        else return false;
        if (count>text.size-i) return false;
        while (count--) {
            unsigned char next=text.bytes[i++]; if ((next&0xc0)!=0x80) return false;
            value=(value<<6)|(next&0x3f);
        }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static bool equals(Text text,const char *value) {
    size_t size=strlen(value); return text.size==size && !memcmp(text.bytes,value,size);
}
static unsigned char upper(unsigned char ch) { return ch>='a' && ch<='z' ? (unsigned char)(ch-'a'+'A') : ch; }
static bool type_equals(Text text,const char *value) {
    if (text.size!=strlen(value)) return false;
    for (size_t i=0;i<text.size;i++) if (upper(text.bytes[i])!=(unsigned char)value[i]) return false;
    return true;
}
static bool decimal_name(Text text,const char *base,NumberType *number) {
    size_t length=strlen(base);
    if (text.size<=length || text.bytes[length]!='(') return false;
    if (!type_equals((Text){text.bytes,length},base)) return false;
    size_t at=length+1; unsigned precision=0,scale=0; bool digits=false;
    while (at<text.size && text.bytes[at]==' ') at++;
    while (at<text.size && text.bytes[at]>='0' && text.bytes[at]<='9') {
        precision=precision*10+text.bytes[at++]-'0'; digits=true;
        if (precision>38) return false;
    }
    while (at<text.size && text.bytes[at]==' ') at++;
    if (!digits || !precision || at==text.size || text.bytes[at++]!=',') return false;
    digits=false;
    while (at<text.size && text.bytes[at]==' ') at++;
    while (at<text.size && text.bytes[at]>='0' && text.bytes[at]<='9') {
        scale=scale*10+text.bytes[at++]-'0'; digits=true;
        if (scale>38) return false;
    }
    while (at<text.size && text.bytes[at]==' ') at++;
    if (!digits || scale>precision || at+1!=text.size || text.bytes[at]!=')') return false;
    if (number) *number=(NumberType){NUM_DECIMAL,(unsigned char)precision,(unsigned char)scale};
    return true;
}
static duckdb_type sql_type(Text text) {
    static const struct { const char *name; duckdb_type type; } types[]={
        {"BOOLEAN",DUCKDB_TYPE_BOOLEAN},{"BOOL",DUCKDB_TYPE_BOOLEAN},
        {"TINYINT",DUCKDB_TYPE_TINYINT},{"SMALLINT",DUCKDB_TYPE_SMALLINT},
        {"INTEGER",DUCKDB_TYPE_INTEGER},{"INT",DUCKDB_TYPE_INTEGER},
        {"BIGINT",DUCKDB_TYPE_BIGINT},{"HUGEINT",DUCKDB_TYPE_HUGEINT},
        {"UTINYINT",DUCKDB_TYPE_UTINYINT},{"USMALLINT",DUCKDB_TYPE_USMALLINT},
        {"UINTEGER",DUCKDB_TYPE_UINTEGER},{"UBIGINT",DUCKDB_TYPE_UBIGINT},
        {"UHUGEINT",DUCKDB_TYPE_UHUGEINT},{"FLOAT",DUCKDB_TYPE_FLOAT},
        {"REAL",DUCKDB_TYPE_FLOAT},{"DOUBLE",DUCKDB_TYPE_DOUBLE},
        {"DOUBLE PRECISION",DUCKDB_TYPE_DOUBLE},{"DECIMAL",DUCKDB_TYPE_DECIMAL},
        {"NUMERIC",DUCKDB_TYPE_DECIMAL},{"DATE",DUCKDB_TYPE_DATE},
        {"TIME",DUCKDB_TYPE_TIME},{"TIME_NS",DUCKDB_TYPE_TIME_NS},
        {"TIMETZ",DUCKDB_TYPE_TIME_TZ},{"TIME WITH TIME ZONE",DUCKDB_TYPE_TIME_TZ},
        {"TIMESTAMP",DUCKDB_TYPE_TIMESTAMP},{"DATETIME",DUCKDB_TYPE_TIMESTAMP},
        {"TIMESTAMP_S",DUCKDB_TYPE_TIMESTAMP_S},{"TIMESTAMP_MS",DUCKDB_TYPE_TIMESTAMP_MS},
        {"TIMESTAMP_NS",DUCKDB_TYPE_TIMESTAMP_NS},{"TIMESTAMPTZ",DUCKDB_TYPE_TIMESTAMP_TZ},
        {"TIMESTAMP WITH TIME ZONE",DUCKDB_TYPE_TIMESTAMP_TZ},
        {"INTERVAL",DUCKDB_TYPE_INTERVAL},{"BLOB",DUCKDB_TYPE_BLOB},
        {"UUID",DUCKDB_TYPE_UUID},{"SQLNULL",DUCKDB_TYPE_SQLNULL},{"NULL",DUCKDB_TYPE_SQLNULL}
    };
    for (size_t i=0;i<sizeof(types)/sizeof(*types);i++) if (type_equals(text,types[i].name)) return types[i].type;
    if (decimal_name(text,"DECIMAL",NULL) || decimal_name(text,"NUMERIC",NULL)) return DUCKDB_TYPE_DECIMAL;
    return DUCKDB_TYPE_VARCHAR;
}
bool page_type_from_sql(const char *name,duckdb_type *type,NumberType *number) {
    if (!name || !type || !number) return false;
    size_t size=0; while (size<=128 && name[size]) size++;
    Text text={(const unsigned char *)name,size};
    if (!size || size>128 || !utf8(text)) return false;
    NumberType parsed={0}; duckdb_type id=sql_type(text);
    if (type_equals(text,"DECIMAL") || type_equals(text,"NUMERIC")) return false;
    const char *decimal_names[]={"DECIMAL","NUMERIC"};
    for (size_t i=0;i<2;i++) {
        size_t prefix=strlen(decimal_names[i]);
        if (size>prefix && name[prefix]=='(' && type_equals((Text){text.bytes,prefix},decimal_names[i])) {
            /* A described DECIMAL array is a complex value, rendered via its
               canonical text just like other unknown compound SQL types. */
            if (name[size-1]==']') continue;
            if (!decimal_name(text,decimal_names[i],&parsed)) return false;
        }
    }
    switch (id) {
    case DUCKDB_TYPE_TINYINT: parsed.kind=NUM_I8; break;
    case DUCKDB_TYPE_SMALLINT: parsed.kind=NUM_I16; break;
    case DUCKDB_TYPE_INTEGER: parsed.kind=NUM_I32; break;
    case DUCKDB_TYPE_BIGINT: parsed.kind=NUM_I64; break;
    case DUCKDB_TYPE_HUGEINT: parsed.kind=NUM_I128; break;
    case DUCKDB_TYPE_UTINYINT: parsed.kind=NUM_U8; break;
    case DUCKDB_TYPE_USMALLINT: parsed.kind=NUM_U16; break;
    case DUCKDB_TYPE_UINTEGER: parsed.kind=NUM_U32; break;
    case DUCKDB_TYPE_UBIGINT: parsed.kind=NUM_U64; break;
    case DUCKDB_TYPE_UHUGEINT: parsed.kind=NUM_U128; break;
    case DUCKDB_TYPE_FLOAT: case DUCKDB_TYPE_DOUBLE: parsed.kind=NUM_FLOAT; break;
    default: break;
    }
    *type=id; *number=parsed; return true;
}
static bool number_type(Text text,uint32_t precision,uint32_t scale,NumberType *out) {
    static const char *kinds[]={"none","i8","i16","i32","i64","i128","u8","u16","u32","u64","u128","decimal","float"};
    for (size_t i=0;i<sizeof(kinds)/sizeof(*kinds);i++) if (equals(text,kinds[i])) {
        if (i==NUM_DECIMAL ? (!precision || precision>38 || scale>precision) : (precision || scale)) return false;
        *out=(NumberType){(NumberKind)i,(unsigned char)precision,(unsigned char)scale}; return true;
    }
    return false;
}
static char *copy_text(Text text) {
    char *copy=dx_alloc(text.size+1); memcpy(copy,text.bytes,text.size); return copy;
}
typedef enum { WIRE_PAGE,WIRE_COPY,WIRE_WORKBOOK_SORTS,WIRE_WORKBOOK_OBJECTS,WIRE_WORKBOOK_OWNERS,
    WIRE_TABLE_SOURCES,WIRE_TABLE_FILTERS,WIRE_TABLE_SORTS } WirePolicy;
static bool decode_page(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,WirePolicy policy,size_t owned_budget,char **error) {
    if (!out || !bytes || size<20 || size>DX_PAGE_WIRE_BYTES) return fail(error,"Invalid DXP1 page size or destination.");
    Reader reader={bytes,size,0};
    uint32_t magic,version,columns,rows,text_bytes;
    if (!word(&reader,&magic) || !word(&reader,&version) || !word(&reader,&columns) ||
        !word(&reader,&rows) || !word(&reader,&text_bytes) || magic!=DX_PAGE_WIRE_MAGIC || version!=DX_PAGE_WIRE_VERSION)
        return fail(error,"Unsupported DXP1 page header.");
    uint32_t max_rows=DX_PAGE_ROWS,exact_columns=0;
    switch (policy) {
    case WIRE_COPY:max_rows=DX_PAGE_WIRE_COPY_CELLS;break;
    case WIRE_WORKBOOK_SORTS:max_rows=449;exact_columns=4;break;
    case WIRE_WORKBOOK_OBJECTS:max_rows=2049;exact_columns=11;break;
    case WIRE_WORKBOOK_OWNERS:max_rows=2113;exact_columns=3;break;
    case WIRE_TABLE_SOURCES:max_rows=2049;exact_columns=14;break;
    case WIRE_TABLE_FILTERS:max_rows=30721;exact_columns=6;break;
    case WIRE_TABLE_SORTS:max_rows=14337;exact_columns=5;break;
    case WIRE_PAGE:break;
    }
    if (columns>DX_PAGE_WIRE_COLUMNS || rows>max_rows ||
        (policy==WIRE_COPY && (uint64_t)columns*rows>DX_PAGE_WIRE_COPY_CELLS) ||
        (exact_columns && (columns!=exact_columns || offset)) || text_bytes>DX_PAGE_WIRE_TEXT_BYTES)
        return fail(error,"DXP1 page exceeds its row, column or text limit.");
    if (rows>UINT64_MAX-offset || (rows && total<offset+rows)) return fail(error,"DXP1 page row count or offset is inconsistent.");
    size_t owned=(size_t)columns*sizeof(Column)+(size_t)columns*rows*sizeof(Cell);
    WireColumn descriptors[DX_PAGE_WIRE_COLUMNS];
    for (size_t i=0;i<columns;i++) {
        WireColumn *column=&descriptors[i]; uint32_t name_size,type_size,kind_size,precision,scale;
        if (!word(&reader,&name_size) || !word(&reader,&type_size) || !word(&reader,&kind_size) ||
            !word(&reader,&precision) || !word(&reader,&scale) || name_size>4096 || type_size>128 || kind_size>16 ||
            !take(&reader,name_size,&column->name) || !take(&reader,type_size,&column->type) || !take(&reader,kind_size,&column->kind) ||
            !utf8(column->name) || !utf8(column->type) || !utf8(column->kind) || !number_type(column->kind,precision,scale,&column->number))
            return fail(error,"Invalid DXP1 column metadata.");
        column->sql_type=sql_type(column->type);
        owned+=column->name.size+column->type.size+2;
    }
    size_t cells=(size_t)columns*rows; Text offsets,nulls,text;
    if (!take(&reader,(cells+1)*4,&offsets) || !take(&reader,cells,&nulls) || !take(&reader,text_bytes,&text) || reader.position!=size)
        return fail(error,"Invalid DXP1 page buffer lengths.");
    uint32_t first=u32(offsets.bytes);
    if (first || u32(offsets.bytes+cells*4)!=text_bytes) return fail(error,"Invalid DXP1 text extent.");
    for (size_t i=0;i<cells;i++) {
        uint32_t next=u32(offsets.bytes+(i+1)*4);
        if (next<first || next>text_bytes || nulls.bytes[i]>1 || (nulls.bytes[i] && first!=next) || !utf8((Text){text.bytes+first,next-first}))
            return fail(error,"Invalid DXP1 cell offsets, NULL marker or UTF-8 text.");
        if (!nulls.bytes[i]) owned+=next-first+1;
        first=next;
    }
    if (owned>owned_budget) return fail(error,"Decoded DXP1 Page exceeds its available C-owned memory budget.");
    /* Nothing is published until the complete wire has been validated and all
       owned strings copied. The incoming bytes may even belong to the old Page. */
    Page page={.column_count=columns,.row_count=rows,.offset=offset,.total=total,.count_pending=count_pending};
    if (columns) page.columns=dx_alloc(columns*sizeof(*page.columns));
    if (cells) page.cells=dx_alloc(cells*sizeof(*page.cells));
    page.bytes=columns*sizeof(*page.columns)+cells*sizeof(*page.cells);
    for (size_t i=0;i<columns;i++) {
        WireColumn *descriptor=&descriptors[i]; Column *column=&page.columns[i];
        column->name=copy_text(descriptor->name); column->type_name=copy_text(descriptor->type);
        column->type=descriptor->sql_type; column->number_type=descriptor->number; column->numeric=descriptor->number.kind!=NUM_NONE;
        page.bytes+=descriptor->name.size+descriptor->type.size+2;
    }
    first=0;
    for (size_t i=0;i<cells;i++) {
        uint32_t next=u32(offsets.bytes+(i+1)*4); Cell *cell=&page.cells[i]; cell->is_null=nulls.bytes[i]!=0;
        if (!cell->is_null) {
            cell->text=copy_text((Text){text.bytes+first,next-first}); page.bytes+=next-first+1;
            if (page.columns[i%columns].numeric) cell->number=strtod(cell->text,NULL);
        }
        first=next;
    }
    page_free(out); *out=page; return true;
}
bool page_from_wire(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_PAGE,SIZE_MAX,error);
}
bool page_from_wire_copy(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_COPY,SIZE_MAX,error);
}
bool page_from_wire_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_PAGE,budget,error);
}
bool page_from_wire_copy_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_COPY,budget,error);
}
bool page_from_wire_workbook_sorts_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_WORKBOOK_SORTS,budget,error);
}
bool page_from_wire_workbook_objects_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_WORKBOOK_OBJECTS,budget,error);
}
bool page_from_wire_workbook_owners_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_WORKBOOK_OWNERS,budget,error);
}
bool page_from_wire_workbook_table_sources_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_TABLE_SOURCES,budget,error);
}
bool page_from_wire_workbook_table_filters_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_TABLE_FILTERS,budget,error);
}
bool page_from_wire_workbook_table_sorts_owned(Page *out,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool count_pending,size_t budget,char **error) {
    return decode_page(out,bytes,size,offset,total,count_pending,WIRE_TABLE_SORTS,budget,error);
}
