#include "data/page_wire.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,releases;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL page wire:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
typedef struct { const char *name,*type,*kind; uint32_t precision,scale; } Descriptor;
typedef struct {
    unsigned char *bytes; size_t size,capacity;
    size_t descriptor[DX_PAGE_WIRE_COLUMNS],name[DX_PAGE_WIRE_COLUMNS],type[DX_PAGE_WIRE_COLUMNS],kind[DX_PAGE_WIRE_COLUMNS];
    size_t offsets,nulls,text;
} Wire;
static void append(Wire *wire,const void *bytes,size_t size) {
    if (wire->size+size>wire->capacity) {
        wire->capacity=(wire->size+size)*2+64; wire->bytes=dx_realloc(wire->bytes,wire->capacity);
    }
    if (size) memcpy(wire->bytes+wire->size,bytes,size); wire->size+=size;
}
static void set_word(Wire *wire,size_t at,uint32_t word) {
    CHECK(at+4<=wire->size);
    for (unsigned i=0;i<4;i++) wire->bytes[at+i]=(unsigned char)(word>>(i*8));
}
static void word(Wire *wire,uint32_t value) {
    unsigned char bytes[4]; for (unsigned i=0;i<4;i++) bytes[i]=(unsigned char)(value>>(i*8)); append(wire,bytes,4);
}
static Wire encode(const Descriptor *columns,uint32_t count,uint32_t rows,const char *const *values) {
    Wire wire={0}; size_t cells=(size_t)count*rows; uint32_t text_bytes=0;
    for (size_t i=0;i<cells;i++) if (values[i]) text_bytes+=(uint32_t)strlen(values[i]);
    word(&wire,DX_PAGE_WIRE_MAGIC); word(&wire,1); word(&wire,count); word(&wire,rows); word(&wire,text_bytes);
    for (size_t i=0;i<count;i++) {
        const Descriptor *column=&columns[i]; wire.descriptor[i]=wire.size;
        word(&wire,(uint32_t)strlen(column->name)); word(&wire,(uint32_t)strlen(column->type)); word(&wire,(uint32_t)strlen(column->kind));
        word(&wire,column->precision); word(&wire,column->scale);
        wire.name[i]=wire.size; append(&wire,column->name,strlen(column->name));
        wire.type[i]=wire.size; append(&wire,column->type,strlen(column->type));
        wire.kind[i]=wire.size; append(&wire,column->kind,strlen(column->kind));
    }
    wire.offsets=wire.size; word(&wire,0); text_bytes=0;
    for (size_t i=0;i<cells;i++) { if (values[i]) text_bytes+=(uint32_t)strlen(values[i]); word(&wire,text_bytes); }
    wire.nulls=wire.size;
    for (size_t i=0;i<cells;i++) { unsigned char is_null=values[i] ? 0 : 1; append(&wire,&is_null,1); }
    wire.text=wire.size;
    for (size_t i=0;i<cells;i++) if (values[i]) append(&wire,values[i],strlen(values[i]));
    return wire;
}
static void release(void *text) { releases++; free(text); }
static Page retained_page(void) {
    Page page={.column_count=1,.row_count=1,.offset=17,.total=300,.bytes=999,.count_pending=true,.query_ms=12,.release_cell_text=release};
    page.columns=dx_alloc(sizeof(*page.columns)); page.cells=dx_alloc(sizeof(*page.cells));
    page.columns[0]=(Column){.name=dx_strdup("Keep"),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    page.cells[0].text=dx_strdup("Retained NULL literal"); page.sql=dx_strdup("Existing SQL");
    page.records=(PageRecords){.ids=dx_alloc(sizeof(uint64_t)),.source_key=dx_strdup("retained raw reader"),.count=1,.stamp=UINT64_MAX};
    page.records.ids[0]=UINT64_C(9007199254740993); return page;
}
static void reject(Page *page,const void *bytes,size_t size,uint64_t offset,uint64_t total,bool pending) {
    Page before=*page; unsigned released=releases; char *error=dx_strdup("Previous error");
    CHECK(!page_from_wire(page,bytes,size,offset,total,pending,&error));
    CHECK(error && *error && strcmp(error,"Previous error"));
    CHECK(!memcmp(page,&before,sizeof(before)) && releases==released);
    CHECK(!strcmp(page->cells[0].text,"Retained NULL literal") && !strcmp(page->columns[0].name,"Keep"));
    CHECK(page->records.count==1 && page->records.stamp==UINT64_MAX && !strcmp(page->records.source_key,"retained raw reader"));
    uint64_t id=0; CHECK(page_record_id(page,17,&id) && id==UINT64_C(9007199254740993));
    free(error);
}
static size_t allocated_bytes(const Page *page) {
    size_t result=page->column_count*sizeof(Column)+page->column_count*page->row_count*sizeof(Cell);
    for (size_t i=0;i<page->column_count;i++) result+=strlen(page->columns[i].name)+strlen(page->columns[i].type_name)+2;
    for (size_t i=0;i<page->column_count*page->row_count;i++) if (page->cells[i].text) result+=strlen(page->cells[i].text)+1;
    return result;
}
static void test_exact_owned_values(void) {
    const Descriptor columns[]={
        {"Missing","VARCHAR","none",0,0},{"Empty","VARCHAR","none",0,0},{"Literal","VARCHAR","none",0,0},
        {"Wide","BIGINT","i64",0,0},{"Unsigned","UBIGINT","u64",0,0},{"Huge","HUGEINT","i128",0,0},
        {"Uhuge","UHUGEINT","u128",0,0},{"Amount","DECIMAL(38,18)","decimal",38,18},
        {"Float","DOUBLE","float",0,0},{"Boolean","BOOLEAN","none",0,0},{"City \xc3\xa9","VARCHAR","none",0,0}
    };
    const char *values[]={NULL,"","NULL","9007199254740993","18446744073709551615","170141183460469231731687303715884105727",
        "340282366920938463463374607431768211455","-12345678901234567890.123456789012345678","-0","true","\xf0\x9f\xa6\x86 \xe4\xb8\xad\xe6\x96\x87"};
    size_t count=sizeof(columns)/sizeof(*columns); Wire wire=encode(columns,(uint32_t)count,1,values); Page page=retained_page();
    unsigned released=releases; char *error=NULL;
    CHECK(page_from_wire(&page,wire.bytes,wire.size,UINT64_C(9007199254740993),UINT64_C(9007199254742000),true,&error));
    CHECK(!error && releases==released+1 && !page.release_cell_text && !page.sql && !page.relations && !page.totals);
    CHECK(!page.records.ids && !page.records.source_key && !page.records.count && !page.records.stamp);
    uint64_t id=71; CHECK(!page_record_id(&page,UINT64_C(9007199254740993),&id) && id==71);
    CHECK(page.offset==UINT64_C(9007199254740993) && page.total==UINT64_C(9007199254742000) && page.count_pending);
    CHECK(page.column_count==count && page.row_count==1 && page.bytes==allocated_bytes(&page));
    memset(wire.bytes,'x',wire.size); free(wire.bytes); /* No pointers into the source transport survive. */
    for (size_t i=0;i<count;i++) {
        CHECK(!strcmp(page.columns[i].name,columns[i].name) && !strcmp(page.columns[i].type_name,columns[i].type));
        CHECK(page.cells[i].is_null==(values[i]==NULL));
        CHECK(values[i] ? page.cells[i].text && !strcmp(page.cells[i].text,values[i]) : !page.cells[i].text);
    }
    CHECK(page.cells[1].text && !*page.cells[1].text && !page.cells[1].is_null);
    CHECK(page.columns[3].type==DUCKDB_TYPE_BIGINT && page.columns[3].number_type.kind==NUM_I64 && page.columns[3].numeric);
    CHECK(page.columns[5].number_type.kind==NUM_I128 && page.columns[6].number_type.kind==NUM_U128);
    CHECK(page.columns[7].type==DUCKDB_TYPE_DECIMAL && page.columns[7].number_type.precision==38 && page.columns[7].number_type.scale==18);
    CHECK(page.cells[3].number==strtod(values[3],NULL) && page.cells[7].number==strtod(values[7],NULL));
    CHECK(signbit(page.cells[8].number) && !page.columns[9].numeric && page.cells[9].number==0);
    page_free(&page); page_free(&page); CHECK(!page.cells && !page.columns && !page.bytes && !page.release_cell_text);
}
static void test_row_major_and_replacement(void) {
    const Descriptor columns[]={{"Count","BIGINT","i64",0,0},{"Label","VARCHAR","none",0,0},{"Amount","DECIMAL(5,2)","decimal",5,2}};
    const char *values[]={"-10",NULL,"0.00","9007199254740993","","-123.45","0","NULL",NULL};
    Wire wire=encode(columns,3,3,values); Page page={0}; char *error=dx_strdup("Retained diagnostic");
    for (unsigned repeat=0;repeat<8;repeat++) {
        CHECK(page_from_wire(&page,wire.bytes,wire.size,256,259,false,&error));
        CHECK(!strcmp(error,"Retained diagnostic") && page.bytes==allocated_bytes(&page));
        for (size_t i=0;i<9;i++) CHECK(values[i] ? page.cells[i].text && !strcmp(page.cells[i].text,values[i]) && !page.cells[i].is_null : page.cells[i].is_null && !page.cells[i].text);
        CHECK(page.cells[0].number==-10 && page.cells[3].number==strtod(values[3],NULL) && page.cells[5].number==-123.45);
    }
    page_free(&page); free(error);
    /* The old Page may own the incoming allocation. Copy before releasing it. */
    page=retained_page(); free(page.cells[0].text); page.cells[0].text=(char *)wire.bytes;
    unsigned released=releases;
    CHECK(page_from_wire(&page,wire.bytes,wire.size,256,259,false,NULL));
    CHECK(releases==released+1 && !strcmp(page.cells[3].text,"9007199254740993"));
    page_free(&page); page_free(&page);
    /* Provenance owns C allocations independently of the cell-text allocator.
       The incoming transport may itself live in that old owned allocation. */
    wire=encode(columns,3,3,values); page=retained_page(); free(page.records.ids); page.records.ids=(uint64_t *)wire.bytes;
    released=releases;
    CHECK(page_from_wire(&page,wire.bytes,wire.size,256,259,false,NULL));
    CHECK(releases==released+1 && !page.records.ids && !page.records.source_key && !page.records.count && !page.records.stamp);
    CHECK(!strcmp(page.cells[3].text,"9007199254740993")); page_free(&page); page_free(&page);
}
static void test_record_lookup_bounds(void) {
    Page page=retained_page(); uint64_t id=71;
    CHECK(!page_record_id(NULL,17,&id) && id==71); CHECK(!page_record_id(&page,17,NULL));
    CHECK(!page_record_id(&page,16,&id) && id==71); CHECK(!page_record_id(&page,18,&id) && id==71);
    Page view=page; view.row_count=0; CHECK(!page_record_id(&view,17,&id) && id==71);
    view=page; view.records.count=0; CHECK(!page_record_id(&view,17,&id) && id==71);
    view=page; view.records.ids=NULL; CHECK(!page_record_id(&view,17,&id) && id==71);
    view=page; view.records.source_key=NULL; CHECK(!page_record_id(&view,17,&id) && id==71);
    page.records.ids[0]=0; CHECK(!page_record_id(&page,17,&id) && id==71); page.records.ids[0]=UINT64_MAX;
    /* A bounded borrow can rebase display rows; total is not its loaded extent. */
    view=page; view.offset=UINT64_MAX; view.total=0;
    CHECK(page_record_id(&view,UINT64_MAX,&id) && id==UINT64_MAX);
    CHECK(!page_record_id(&view,UINT64_MAX-1,&id) && id==UINT64_MAX);
    page_free(&page);
    CHECK(!page.records.ids && !page.records.source_key && !page.records.count && !page.records.stamp);
    CHECK(!page_record_id(&page,17,&id) && id==UINT64_MAX); page_free(&page);
}
static void test_descriptor_types(void) {
    static const struct { Descriptor column; duckdb_type type; NumberKind number; } cases[]={
        {{"a","tinyint","i8",0,0},DUCKDB_TYPE_TINYINT,NUM_I8},{{"a","SMALLINT","i16",0,0},DUCKDB_TYPE_SMALLINT,NUM_I16},
        {{"a","INTEGER","i32",0,0},DUCKDB_TYPE_INTEGER,NUM_I32},{{"a","UTINYINT","u8",0,0},DUCKDB_TYPE_UTINYINT,NUM_U8},
        {{"a","USMALLINT","u16",0,0},DUCKDB_TYPE_USMALLINT,NUM_U16},{{"a","UINTEGER","u32",0,0},DUCKDB_TYPE_UINTEGER,NUM_U32},
        {{"a","FLOAT","float",0,0},DUCKDB_TYPE_FLOAT,NUM_FLOAT},{{"a","DATE","none",0,0},DUCKDB_TYPE_DATE,NUM_NONE},
        {{"a","TIMESTAMP","none",0,0},DUCKDB_TYPE_TIMESTAMP,NUM_NONE},{{"a","TIMESTAMP_S","none",0,0},DUCKDB_TYPE_TIMESTAMP_S,NUM_NONE},
        {{"a","TIMESTAMP_MS","none",0,0},DUCKDB_TYPE_TIMESTAMP_MS,NUM_NONE},{{"a","TIMESTAMP_NS","none",0,0},DUCKDB_TYPE_TIMESTAMP_NS,NUM_NONE},
        {{"a","TIMESTAMPTZ","none",0,0},DUCKDB_TYPE_TIMESTAMP_TZ,NUM_NONE},{{"a","timestamp with time zone","none",0,0},DUCKDB_TYPE_TIMESTAMP_TZ,NUM_NONE},
        {{"a","TIME","none",0,0},DUCKDB_TYPE_TIME,NUM_NONE},{{"a","TIME_NS","none",0,0},DUCKDB_TYPE_TIME_NS,NUM_NONE},
        {{"a","TIMETZ","none",0,0},DUCKDB_TYPE_TIME_TZ,NUM_NONE},{{"a","SQLNULL","none",0,0},DUCKDB_TYPE_SQLNULL,NUM_NONE},
        {{"a","CUSTOM MONEY","decimal",25,4},DUCKDB_TYPE_VARCHAR,NUM_DECIMAL},{{"a","CUSTOM INTEGER","i64",0,0},DUCKDB_TYPE_VARCHAR,NUM_I64},
        {{"a","struct(a INTEGER)","none",0,0},DUCKDB_TYPE_VARCHAR,NUM_NONE},{{"","numeric( 5, 2 )","decimal",5,2},DUCKDB_TYPE_DECIMAL,NUM_DECIMAL},
        {{"a","DECIMAL","decimal",38,38},DUCKDB_TYPE_DECIMAL,NUM_DECIMAL},{{"a","NUMERIC","decimal",1,0},DUCKDB_TYPE_DECIMAL,NUM_DECIMAL}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        Wire wire=encode(&cases[i].column,1,0,NULL); Page page={0};
        CHECK(page_from_wire(&page,wire.bytes,wire.size,0,0,false,NULL));
        CHECK(page.column_count==1 && !page.row_count && !page.cells);
        CHECK(page.columns[0].type==cases[i].type && page.columns[0].number_type.kind==cases[i].number && page.columns[0].numeric==(cases[i].number!=NUM_NONE));
        CHECK(!strcmp(page.columns[0].type_name,cases[i].column.type) && page.bytes==allocated_bytes(&page));
        page_free(&page); free(wire.bytes);
    }
}
static void test_bad_headers_and_lengths(void) {
    Descriptor column={"Name","VARCHAR","none",0,0}; const char *values[]={"\xc3\xa9","","NULL",NULL};
    Wire valid=encode(&column,1,4,values); Page page=retained_page();
    for (size_t size=0;size<valid.size;size++) reject(&page,valid.bytes,size,0,4,false);
    reject(&page,NULL,valid.size,0,4,false);
    reject(&page,valid.bytes,SIZE_MAX,0,4,false);
    reject(&page,valid.bytes,DX_PAGE_WIRE_BYTES+1,0,4,false);
    reject(&page,valid.bytes,valid.size,UINT64_MAX-2,UINT64_MAX,true);
    reject(&page,valid.bytes,valid.size,5,8,false);
    reject(&page,valid.bytes,valid.size,5,8,true);
    CHECK(!page_from_wire(NULL,valid.bytes,valid.size,0,4,false,NULL));
    const struct { size_t at; uint32_t value; } bad[]={
        {0,0},{4,0},{4,2},{8,257},{8,UINT32_MAX},{12,257},{12,UINT32_MAX},{16,DX_PAGE_WIRE_TEXT_BYTES+1},
        {20,4097},{24,129},{28,17},{28,0},{32,1},{36,1},
        {valid.offsets,1},{valid.offsets+4,UINT32_MAX},{valid.offsets+8,0},{valid.offsets+16,0}
    };
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        Wire wire=encode(&column,1,4,values); set_word(&wire,bad[i].at,bad[i].value);
        reject(&page,wire.bytes,wire.size,0,4,false); free(wire.bytes);
    }
    Wire trailing=encode(&column,1,4,values); unsigned char zero=0; append(&trailing,&zero,1);
    reject(&page,trailing.bytes,trailing.size,0,4,false); free(trailing.bytes);
    for (size_t i=0;i<4;i++) {
        Wire wire=encode(&column,1,4,values); wire.bytes[wire.nulls+i]=2;
        reject(&page,wire.bytes,wire.size,0,4,false); free(wire.bytes);
    }
    Wire null_text=encode(&column,1,4,values); null_text.bytes[null_text.nulls]=1;
    reject(&page,null_text.bytes,null_text.size,0,4,false); free(null_text.bytes);
    CHECK(!page_from_wire(&page,valid.bytes,3,0,4,false,NULL));
    page_free(&page); free(valid.bytes);
}
static void test_bad_metadata(void) {
    static const Descriptor bad[]={
        {"a","DECIMAL(39,1)","decimal",39,1},{"a","DECIMAL(1,2)","decimal",1,2},{"a","DECIMAL","decimal",0,0},
        {"a","DECIMAL","decimal",38,UINT32_MAX},{"a","BIGINT","i64",1,0},{"a","BIGINT","i64",0,1},
        {"a","VARCHAR","none",38,0},{"a","FLOAT","float",0,1},{"a","FLOAT","FLOAT",0,0},{"a","VARCHAR","",0,0},
        {"a","BIGINT","i256",0,0},{"a","DECIMAL","decimal",UINT32_MAX,0}
    };
    Page page=retained_page();
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        Wire wire=encode(&bad[i],1,0,NULL); reject(&page,wire.bytes,wire.size,0,0,false); free(wire.bytes);
    }
    Descriptor column={"\xc3\xa9","VARCHAR","none",0,0};
    for (unsigned i=0;i<3;i++) {
        Wire wire=encode(&column,1,0,NULL); size_t at=i==0 ? wire.name[0] : i==1 ? wire.type[0] : wire.kind[0];
        wire.bytes[at]=0; reject(&page,wire.bytes,wire.size,0,0,false);
        wire.bytes[at]=0xc0; reject(&page,wire.bytes,wire.size,0,0,false); free(wire.bytes);
    }
    char *name=dx_alloc(4098); memset(name,'a',4097); name[4097]=0;
    column=(Descriptor){name,"VARCHAR","none",0,0}; Wire wire=encode(&column,1,0,NULL);
    reject(&page,wire.bytes,wire.size,0,0,false); free(wire.bytes); free(name); page_free(&page);
}
static void test_strict_utf8(void) {
    static const struct { unsigned char bytes[5]; size_t size; } invalid[]={
        {{0},1},{{0x80},1},{{0xc0,0x80},2},{{0xc1,0xbf},2},{{0xc2},1},{{0xc2,'A'},2},
        {{0xe0,0x80,0x80},3},{{0xed,0xa0,0x80},3},{{0xed,0xbf,0xbf},3},{{0xe2,0x82},2},
        {{0xf0,0x80,0x80,0x80},4},{{0xf4,0x90,0x80,0x80},4},{{0xf5,0x80,0x80,0x80},4},
        {{0xff},1},{{'a',0,'b'},3},{{0xf0,0x9f,0xa6},3}
    };
    Descriptor column={"UTF8","VARCHAR","none",0,0}; Page page=retained_page();
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        const char *values[]={"abcde"}; Wire wire=encode(&column,1,1,values);
        memcpy(wire.bytes+wire.text,invalid[i].bytes,invalid[i].size); wire.size=wire.text+invalid[i].size;
        set_word(&wire,16,(uint32_t)invalid[i].size); set_word(&wire,wire.offsets+4,(uint32_t)invalid[i].size);
        reject(&page,wire.bytes,wire.size,0,1,false); free(wire.bytes);
    }
    const char *split[]={"\xc3\xa9",""}; Wire wire=encode(&column,1,2,split); set_word(&wire,wire.offsets+4,1);
    reject(&page,wire.bytes,wire.size,0,2,false); free(wire.bytes); page_free(&page);
    const char *valid[]={"\x01\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"};
    wire=encode(&column,1,1,valid); page=(Page){0}; CHECK(page_from_wire(&page,wire.bytes,wire.size,0,1,false,NULL));
    CHECK(!strcmp(page.cells[0].text,valid[0])); page_free(&page); free(wire.bytes);
}
static void test_limits_and_empty_pages(void) {
    Wire empty=encode(NULL,0,0,NULL); Page page=retained_page(); unsigned released=releases;
    CHECK(page_from_wire(&page,empty.bytes,empty.size,UINT64_MAX,0,true,NULL));
    CHECK(!page.columns && !page.cells && !page.bytes && page.offset==UINT64_MAX && !page.total && page.count_pending);
    CHECK(releases==released+1 && !page.records.ids && !page.records.source_key && !page.records.count && !page.records.stamp);
    uint64_t id=71; CHECK(!page_record_id(&page,UINT64_MAX,&id) && id==71);
    CHECK(page_from_wire(&page,empty.bytes,empty.size,UINT64_MAX,UINT64_MAX,false,NULL));
    page_free(&page); free(empty.bytes);
    Descriptor columns[256]; for (size_t i=0;i<256;i++) columns[i]=(Descriptor){"a","VARCHAR","none",0,0};
    const char **values=dx_alloc(256*256*sizeof(*values)); Wire maximum=encode(columns,256,256,values);
    CHECK(page_from_wire(&page,maximum.bytes,maximum.size,UINT64_MAX-256,UINT64_MAX,false,NULL));
    CHECK(page.column_count==256 && page.row_count==256 && page.offset==UINT64_MAX-256 && page.bytes==allocated_bytes(&page));
    bool all_null=true; for (size_t i=0;i<256*256;i++) if (!page.cells[i].is_null || page.cells[i].text) all_null=false;
    CHECK(all_null);
    page_free(&page); free(maximum.bytes); free(values);
    char *text=dx_alloc(DX_PAGE_WIRE_TEXT_BYTES+1); memset(text,'x',DX_PAGE_WIRE_TEXT_BYTES);
    const char *large_values[]={text}; Wire large=encode(columns,1,1,large_values);
    CHECK(page_from_wire(&page,large.bytes,large.size,0,1,false,NULL));
    CHECK(strlen(page.cells[0].text)==DX_PAGE_WIRE_TEXT_BYTES && page.bytes==allocated_bytes(&page));
    page_free(&page); free(large.bytes); free(text);
    char name[4097],type[129]; memset(name,'n',sizeof(name)-1); name[4096]=0; memset(type,'t',sizeof(type)-1); type[128]=0;
    Descriptor longest={name,type,"none",0,0}; Wire names=encode(&longest,1,0,NULL);
    CHECK(page_from_wire(&page,names.bytes,names.size,0,0,false,NULL));
    CHECK(strlen(page.columns[0].name)==4096 && strlen(page.columns[0].type_name)==128 && page.columns[0].type==DUCKDB_TYPE_VARCHAR);
    page_free(&page); free(names.bytes);
}
static void test_copy_envelope(void) {
    const Descriptor descriptor[]={ {"v","VARCHAR","none",0,0},{"w","VARCHAR","none",0,0} };
    const char **values=dx_alloc(20002*sizeof(*values));
    for (size_t i=0;i<20002;i++) values[i]=i%4==0 ? NULL : i%4==1 ? "" : i%4==2 ? "NULL" : "9007199254740993";
    Wire wire=encode(descriptor,1,20000,values); Page out=retained_page(); char *error=NULL;
    reject(&out,wire.bytes,wire.size,0,20000,false);
    CHECK(page_from_wire_copy(&out,wire.bytes,wire.size,0,20000,false,&error) && !error);
    CHECK(out.row_count==20000 && out.column_count==1 && out.bytes==allocated_bytes(&out));
    CHECK(out.cells[0].is_null && out.cells[1].text && !*out.cells[1].text && !strcmp(out.cells[2].text,"NULL") && !strcmp(out.cells[19999].text,"9007199254740993"));
    size_t budget=out.bytes; page_free(&out); out=retained_page(); Page unchanged=out; unsigned released=releases;
    CHECK(!page_from_wire_copy_owned(&out,wire.bytes,wire.size,0,20000,false,budget-1,&error) && error && !memcmp(&out,&unchanged,sizeof(out)) && releases==released); free(error); error=NULL;
    CHECK(page_from_wire_copy_owned(&out,wire.bytes,wire.size,0,20000,false,budget,&error) && !error && out.bytes==budget && releases==released+1);
    memset(wire.bytes,0,wire.size); free(wire.bytes); CHECK(!strcmp(out.cells[19999].text,"9007199254740993")); page_free(&out);
    wire=encode(descriptor,2,10001,values); out=retained_page(); Page before=out;
    CHECK(!page_from_wire_copy(&out,wire.bytes,wire.size,0,10001,false,&error) && error && !memcmp(&out,&before,sizeof(out)));
    free(error); error=NULL; free(wire.bytes);
    wire=encode(descriptor,1,20000,values); set_word(&wire,12,20001);
    CHECK(!page_from_wire_copy(&out,wire.bytes,wire.size,0,20001,false,&error) && error && !memcmp(&out,&before,sizeof(out)));
    free(error); error=NULL; set_word(&wire,12,20000); wire.bytes[wire.nulls]=2;
    CHECK(!page_from_wire_copy(&out,wire.bytes,wire.size,0,20000,false,&error) && error && !memcmp(&out,&before,sizeof(out)));
    free(error); free(wire.bytes); free(values); page_free(&out);
    const char *missing[]={NULL}; wire=encode(descriptor,1,1,missing); out=retained_page(); unchanged=out;
    budget=sizeof(Column)+sizeof(Cell)+strlen(descriptor[0].name)+strlen(descriptor[0].type)+2;
    error=NULL; CHECK(!page_from_wire_owned(&out,wire.bytes,wire.size,0,1,false,budget-1,&error) && error && !memcmp(&out,&unchanged,sizeof(out))); free(error); error=NULL;
    CHECK(page_from_wire_owned(&out,wire.bytes,wire.size,0,1,false,budget,&error) && !error && out.bytes==budget && out.cells[0].is_null);
    free(wire.bytes); page_free(&out);
}
/* A trusted active workbook cursor selects this entry explicitly. The wire
   itself cannot opt into a larger envelope, nor may ordinary pages select it. */
static void reject_workbook_sorts(Page *out,const Wire *wire,size_t size,uint64_t offset,uint64_t total,bool pending,size_t budget) {
    unsigned char before[sizeof(*out)];memcpy(before,out,sizeof(before));
    unsigned released=releases;char *error=dx_strdup("Previous error");
    CHECK(!page_from_wire_workbook_sorts_owned(out,wire->bytes,size,offset,total,pending,budget,&error));
    CHECK(error&&*error&&strcmp(error,"Previous error"));
    CHECK(!memcmp(out,before,sizeof(before))&&releases==released);
    CHECK(!strcmp(out->cells[0].text,"Retained NULL literal")&&!strcmp(out->columns[0].name,"Keep")&&out->records.count==1&&out->records.stamp==UINT64_MAX);
    free(error);
}
static void test_workbook_sorts_envelope(void) {
    const Descriptor columns[]={
        {"sheet_id","INTEGER","i32",0,0},{"position","INTEGER","i32",0,0},
        {"name","VARCHAR","none",0,0},{"descending","BOOLEAN","none",0,0},{"extra","VARCHAR","none",0,0}
    };
    const char *names[]={NULL,"","NULL","9007199254740993","18446744073709551615","İstanbul 🦆"};
    const char **values=dx_alloc(450*5*sizeof(*values));
    for(size_t row=0;row<450;row++){
        values[row*4]="63";values[row*4+1]="6";values[row*4+2]=names[row%6];values[row*4+3]=row%2?"true":"false";
    }
    Wire wire=encode(columns,4,449,values);Page out=retained_page();char *error=NULL;
    // The normal decoder still rejects a 257+ row envelope. The metadata path
    // is opt-in and can preserve the sentinel's honest incomplete count.
    reject(&out,wire.bytes,wire.size,0,449,true);
    unsigned released=releases;
    CHECK(page_from_wire_workbook_sorts_owned(&out,wire.bytes,wire.size,0,449,true,SIZE_MAX,&error)&&!error);
    CHECK(out.row_count==449&&out.column_count==4&&out.total==449&&out.count_pending&&releases==released+1);
    CHECK(out.bytes==allocated_bytes(&out)&&!out.records.ids&&!out.records.source_key&&!out.sql&&!out.totals&&!out.other);
    CHECK(out.columns[0].type==DUCKDB_TYPE_INTEGER&&out.columns[0].number_type.kind==NUM_I32&&out.columns[3].type==DUCKDB_TYPE_BOOLEAN);
    for(size_t row=0;row<449;row++){
        const Cell *cell=out.cells+row*4+2;const char *expected=names[row%6];
        CHECK(cell->is_null==(expected==NULL));CHECK(expected?cell->text&&!strcmp(cell->text,expected):!cell->text);
    }
    size_t owned=out.bytes;page_free(&out);out=retained_page();
    reject_workbook_sorts(&out,&wire,wire.size,0,449,true,owned-1);
    released=releases;CHECK(page_from_wire_workbook_sorts_owned(&out,wire.bytes,wire.size,0,449,true,owned,&error)&&!error&&out.bytes==owned&&releases==released+1);
    memset(wire.bytes,0,wire.size);free(wire.bytes);
    CHECK(!strcmp(out.cells[3*4+2].text,"9007199254740993")&&!strcmp(out.cells[4*4+2].text,"18446744073709551615"));page_free(&out);
    wire=encode(columns,4,257,values);out=retained_page();reject(&out,wire.bytes,wire.size,0,257,false);free(wire.bytes);
    wire=encode(columns,4,450,values);reject_workbook_sorts(&out,&wire,wire.size,0,450,true,SIZE_MAX);free(wire.bytes);
    for(size_t count=0;count<=5;count++)if(count!=4){
        wire=encode(columns,(uint32_t)count,1,values);reject_workbook_sorts(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
    }
    wire=encode(columns,4,1,values);
    reject_workbook_sorts(&out,&wire,wire.size,1,2,false,SIZE_MAX);
    reject_workbook_sorts(&out,&wire,DX_PAGE_WIRE_BYTES+1u,0,1,false,SIZE_MAX);
    wire.bytes[wire.nulls]=2;reject_workbook_sorts(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
    wire=encode(columns,4,0,NULL);CHECK(page_from_wire_workbook_sorts_owned(&out,wire.bytes,wire.size,0,0,false,SIZE_MAX,&error)&&!error&&out.row_count==0&&out.column_count==4);free(wire.bytes);page_free(&out);
    // A tiny NULL-only result also needs the descriptor/Cell storage budget;
    // it cannot bypass owned-byte admission simply because text is empty.
    const char *missing[]={NULL,NULL,NULL,NULL};wire=encode(columns,4,1,missing);
    size_t minimum=4*sizeof(Column)+4*sizeof(Cell);
    for(size_t i=0;i<4;i++)minimum+=strlen(columns[i].name)+strlen(columns[i].type)+2;
    out=retained_page();reject_workbook_sorts(&out,&wire,wire.size,0,1,false,minimum-1);
    CHECK(page_from_wire_workbook_sorts_owned(&out,wire.bytes,wire.size,0,1,false,minimum,&error)&&!error&&out.bytes==minimum);
    for(size_t i=0;i<4;i++)CHECK(out.cells[i].is_null&&!out.cells[i].text);
    free(wire.bytes);page_free(&out);free(values);
}
static void test_workbook_sorts_text_bound(void) {
    const Descriptor columns[]={{"sheet_id","INTEGER","i32",0,0},{"position","INTEGER","i32",0,0},{"name","VARCHAR","none",0,0},{"descending","BOOLEAN","none",0,0}};
    char *text=dx_alloc(DX_PAGE_WIRE_TEXT_BYTES+2);memset(text,'x',DX_PAGE_WIRE_TEXT_BYTES);text[DX_PAGE_WIRE_TEXT_BYTES]=0;
    const char *values[]={NULL,NULL,text,NULL};Wire wire=encode(columns,4,1,values);Page out={0};char *error=NULL;
    CHECK(page_from_wire_workbook_sorts_owned(&out,wire.bytes,wire.size,0,1,false,SIZE_MAX,&error)&&!error);
    CHECK(strlen(out.cells[2].text)==DX_PAGE_WIRE_TEXT_BYTES&&out.bytes==allocated_bytes(&out));page_free(&out);free(wire.bytes);
    text[DX_PAGE_WIRE_TEXT_BYTES]='x';text[DX_PAGE_WIRE_TEXT_BYTES+1]=0;wire=encode(columns,4,1,values);out=retained_page();
    reject_workbook_sorts(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);free(text);page_free(&out);
}
static const Descriptor object_columns[]={
    {"sheet_id","INTEGER","i32",0,0},{"id","UBIGINT","u64",0,0},{"name","VARCHAR","none",0,0},
    {"row_id","UBIGINT","u64",0,0},{"column_id","INTEGER","i32",0,0},{"height","UBIGINT","u64",0,0},
    {"width","INTEGER","i32",0,0},{"kind","INTEGER","i32",0,0},{"mark","INTEGER","i32",0,0},
    {"source_id","UBIGINT","u64",0,0},{"chart_measure","INTEGER","i32",0,0},{"extra","VARCHAR","none",0,0}
};
static void reject_workbook_objects(Page *out,const Wire *wire,size_t size,uint64_t offset,uint64_t total,bool pending,size_t budget) {
    Page before=*out;unsigned released=releases;char *error=dx_strdup("Previous error");
    unsigned char *source=dx_alloc(wire->size);memcpy(source,wire->bytes,wire->size);
    CHECK(!page_from_wire_workbook_objects_owned(out,wire->bytes,size,offset,total,pending,budget,&error));
    CHECK(error&&*error&&strcmp(error,"Previous error"));
    CHECK(!memcmp(out,&before,sizeof(before))&&releases==released&&!memcmp(wire->bytes,source,wire->size));
    CHECK(!strcmp(out->cells[0].text,"Retained NULL literal")&&!strcmp(out->columns[0].name,"Keep")&&out->records.count==1&&out->records.stamp==UINT64_MAX);
    free(source);free(error);
}
static void test_workbook_objects_envelope(void) {
    const char *names[]={NULL,"","NULL","Owner 'İ' 🦆","9007199254740993","18446744073709551615"};
    const char **values=dx_alloc(2050u*11u*sizeof(*values));
    for(size_t row=0;row<2050;row++){
        const char *cells[]={"63","18446744073709551615",names[row%6],"9007199254740993","16383","12","128","0","0","9007199254740994","0"};
        memcpy(values+row*11,cells,sizeof(cells));
    }
    Wire wire=encode(object_columns,11,2049,values);Page out=retained_page();char *error=NULL;
    reject(&out,wire.bytes,wire.size,0,2049,true);
    reject_workbook_sorts(&out,&wire,wire.size,0,2049,true,SIZE_MAX);
    Page before=out;unsigned released=releases;
    CHECK(!page_from_wire_copy_owned(&out,wire.bytes,wire.size,0,2049,true,SIZE_MAX,&error)&&error&&!memcmp(&out,&before,sizeof(out))&&releases==released);
    free(error);error=NULL;
    unsigned char *source=dx_alloc(wire.size);memcpy(source,wire.bytes,wire.size);
    CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,2049,true,SIZE_MAX,&error)&&!error);
    CHECK(out.row_count==2049&&out.column_count==11&&out.total==2049&&out.count_pending&&releases==released+1);
    CHECK(!memcmp(wire.bytes,source,wire.size));free(source);
    CHECK(out.bytes==allocated_bytes(&out)&&!out.sql&&!out.totals&&!out.other&&!out.records.ids&&!out.records.source_key);
    CHECK(out.columns[1].type==DUCKDB_TYPE_UBIGINT&&out.columns[1].number_type.kind==NUM_U64&&out.columns[3].number_type.kind==NUM_U64);
    for(size_t i=0;i<2049u*11u;i++){
        CHECK(out.cells[i].is_null==(values[i]==NULL));CHECK(values[i]?out.cells[i].text&&!strcmp(out.cells[i].text,values[i]):!out.cells[i].text);
    }
    size_t owned=out.bytes;page_free(&out);out=retained_page();
    reject_workbook_objects(&out,&wire,wire.size,0,2049,true,owned-1);
    released=releases;CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,2049,true,owned,&error)&&!error&&out.bytes==owned&&releases==released+1);
    memset(wire.bytes,0,wire.size);free(wire.bytes);
    CHECK(!strcmp(out.cells[2048u*11u+1].text,"18446744073709551615")&&!strcmp(out.cells[2048u*11u+3].text,"9007199254740993"));page_free(&out);
    wire=encode(object_columns,11,2048,values);CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,2048,false,SIZE_MAX,&error)&&!error&&out.row_count==2048&&!out.count_pending);free(wire.bytes);page_free(&out);
    out=retained_page();wire=encode(object_columns,11,2050,values);reject_workbook_objects(&out,&wire,wire.size,0,2050,true,SIZE_MAX);free(wire.bytes);
    for(size_t count=0;count<=12;count++)if(count!=11){
        wire=encode(object_columns,(uint32_t)count,1,values);reject_workbook_objects(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
    }
    wire=encode(object_columns,11,1,values);
    reject_workbook_objects(&out,&wire,wire.size,1,2,false,SIZE_MAX);
    reject_workbook_objects(&out,&wire,wire.size,0,0,false,SIZE_MAX);
    reject_workbook_objects(&out,&wire,DX_PAGE_WIRE_BYTES+1u,0,1,false,SIZE_MAX);
    wire.bytes[wire.nulls]=2;reject_workbook_objects(&out,&wire,wire.size,0,1,false,SIZE_MAX);wire.bytes[wire.nulls]=0;
    wire.bytes[wire.text]=0;reject_workbook_objects(&out,&wire,wire.size,0,1,false,SIZE_MAX);
    wire.bytes[wire.text]=0xff;reject_workbook_objects(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
    wire=encode(object_columns,11,0,NULL);CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,0,false,SIZE_MAX,&error)&&!error&&out.row_count==0&&out.column_count==11);free(wire.bytes);page_free(&out);
    const char *missing[11]={0};wire=encode(object_columns,11,1,missing);
    size_t minimum=11u*sizeof(Column)+11u*sizeof(Cell);
    for(size_t i=0;i<11;i++)minimum+=strlen(object_columns[i].name)+strlen(object_columns[i].type)+2;
    out=retained_page();reject_workbook_objects(&out,&wire,wire.size,0,1,false,minimum-1);
    CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,1,false,minimum,&error)&&!error&&out.bytes==minimum);
    for(size_t i=0;i<11;i++)CHECK(out.cells[i].is_null&&!out.cells[i].text);
    free(wire.bytes);page_free(&out);free(values);
}
static void test_workbook_objects_text_bound(void) {
    char *text=dx_alloc(DX_PAGE_WIRE_TEXT_BYTES+2u);memset(text,'x',DX_PAGE_WIRE_TEXT_BYTES);text[DX_PAGE_WIRE_TEXT_BYTES]=0;
    const char *values[11]={0};values[2]=text;Wire wire=encode(object_columns,11,1,values);Page out={0};char *error=NULL;
    CHECK(page_from_wire_workbook_objects_owned(&out,wire.bytes,wire.size,0,1,false,SIZE_MAX,&error)&&!error);
    CHECK(strlen(out.cells[2].text)==DX_PAGE_WIRE_TEXT_BYTES&&out.bytes==allocated_bytes(&out));page_free(&out);free(wire.bytes);
    text[DX_PAGE_WIRE_TEXT_BYTES]='x';text[DX_PAGE_WIRE_TEXT_BYTES+1u]=0;wire=encode(object_columns,11,1,values);out=retained_page();
    reject_workbook_objects(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);free(text);page_free(&out);
}
static const Descriptor owner_columns[]={
    {"sheet_id","INTEGER","i32",0,0},{"object_id","UBIGINT","u64",0,0},
    {"query","VARCHAR","none",0,0},{"extra","VARCHAR","none",0,0}
};
static void reject_workbook_owners(Page *out,const Wire *wire,size_t size,uint64_t offset,uint64_t total,bool pending,size_t budget) {
    Page before=*out;unsigned released=releases;char *error=dx_strdup("Previous error");
    unsigned char *source=dx_alloc(wire->size);memcpy(source,wire->bytes,wire->size);
    CHECK(!page_from_wire_workbook_owners_owned(out,wire->bytes,size,offset,total,pending,budget,&error));
    CHECK(error&&*error&&strcmp(error,"Previous error"));
    CHECK(!memcmp(out,&before,sizeof(before))&&releases==released&&!memcmp(wire->bytes,source,wire->size));
    CHECK(!strcmp(out->cells[0].text,"Retained NULL literal")&&!strcmp(out->columns[0].name,"Keep")&&out->records.count==1&&out->records.stamp==UINT64_MAX);
    free(source);free(error);
}
static void test_workbook_owners_envelope(void) {
    const char *texts[]={NULL,"","NULL","SELECT 'İstanbul 🦆', 'apostrophe''s' FROM input;","9007199254740993","18446744073709551615"};
    const char *ids[]={"0","9007199254740993","18446744073709551615"};const char **values=dx_alloc(2114u*3u*sizeof(*values));
    for(size_t row=0;row<2114;row++){values[row*3]="63";values[row*3+1]=ids[row%3];values[row*3+2]=texts[row%6];}
    Wire wire=encode(owner_columns,3,2113,values);Page out=retained_page();char *error=NULL;
    reject(&out,wire.bytes,wire.size,0,2113,true);reject_workbook_sorts(&out,&wire,wire.size,0,2113,true,SIZE_MAX);reject_workbook_objects(&out,&wire,wire.size,0,2113,true,SIZE_MAX);
    unsigned char *source=dx_alloc(wire.size);memcpy(source,wire.bytes,wire.size);unsigned released=releases;
    CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,2113,true,SIZE_MAX,&error)&&!error);
    CHECK(out.row_count==2113&&out.column_count==3&&out.total==2113&&out.count_pending&&releases==released+1);
    CHECK(!memcmp(wire.bytes,source,wire.size));free(source);
    CHECK(out.bytes==allocated_bytes(&out)&&!out.sql&&!out.totals&&!out.other&&!out.records.ids&&!out.records.source_key);
    CHECK(out.columns[1].type==DUCKDB_TYPE_UBIGINT&&out.columns[1].number_type.kind==NUM_U64);
    for(size_t i=0;i<2113u*3u;i++){CHECK(out.cells[i].is_null==(values[i]==NULL));CHECK(values[i]?out.cells[i].text&&!strcmp(out.cells[i].text,values[i]):!out.cells[i].text);}
    size_t owned=out.bytes;page_free(&out);out=retained_page();reject_workbook_owners(&out,&wire,wire.size,0,2113,true,owned-1);
    released=releases;CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,2113,true,owned,&error)&&!error&&out.bytes==owned&&releases==released+1);
    memset(wire.bytes,0,wire.size);free(wire.bytes);CHECK(!strcmp(out.cells[4].text,"9007199254740993")&&!strcmp(out.cells[7].text,"18446744073709551615")&&!strcmp(out.cells[3*3+2].text,texts[3]));page_free(&out);
    const size_t valid_rows[]={2048,2112};
    for(size_t i=0;i<sizeof(valid_rows)/sizeof(*valid_rows);i++){
        size_t rows=valid_rows[i];wire=encode(owner_columns,3,(uint32_t)rows,values);
        CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,rows,false,SIZE_MAX,&error)&&!error&&out.row_count==rows&&!out.count_pending);free(wire.bytes);page_free(&out);
    }
    out=retained_page();wire=encode(owner_columns,3,2114,values);reject_workbook_owners(&out,&wire,wire.size,0,2114,true,SIZE_MAX);free(wire.bytes);
    for(size_t count=0;count<=4;count++)if(count!=3){wire=encode(owner_columns,(uint32_t)count,1,values);reject_workbook_owners(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);}
    wire=encode(owner_columns,3,1,values);reject_workbook_owners(&out,&wire,wire.size,1,2,false,SIZE_MAX);reject_workbook_owners(&out,&wire,wire.size,0,0,false,SIZE_MAX);
    reject_workbook_owners(&out,&wire,DX_PAGE_WIRE_BYTES+1u,0,1,false,SIZE_MAX);
    wire.bytes[wire.nulls]=2;reject_workbook_owners(&out,&wire,wire.size,0,1,false,SIZE_MAX);wire.bytes[wire.nulls]=0;
    wire.bytes[wire.text]=0;reject_workbook_owners(&out,&wire,wire.size,0,1,false,SIZE_MAX);wire.bytes[wire.text]=0xff;reject_workbook_owners(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
    wire=encode(owner_columns,3,0,NULL);CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,0,false,SIZE_MAX,&error)&&!error&&out.row_count==0&&out.column_count==3);free(wire.bytes);page_free(&out);
    const char *missing[3]={0};wire=encode(owner_columns,3,1,missing);size_t minimum=3u*sizeof(Column)+3u*sizeof(Cell);
    for(size_t i=0;i<3;i++)minimum+=strlen(owner_columns[i].name)+strlen(owner_columns[i].type)+2;
    out=retained_page();reject_workbook_owners(&out,&wire,wire.size,0,1,false,minimum-1);
    CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,1,false,minimum,&error)&&!error&&out.bytes==minimum);
    for(size_t i=0;i<3;i++)CHECK(out.cells[i].is_null&&!out.cells[i].text);free(wire.bytes);page_free(&out);
    const Descriptor option_columns[]={{"sheet_id","INTEGER","i32",0,0},{"object_id","UBIGINT","u64",0,0},{"unique_rows","BOOLEAN","none",0,0}};
    const char *options[]={"63","18446744073709551615","true","63","0","false","63","9007199254740993",NULL};wire=encode(option_columns,3,3,options);
    CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,3,false,SIZE_MAX,&error)&&!error&&out.columns[2].type==DUCKDB_TYPE_BOOLEAN&&out.columns[2].number_type.kind==NUM_NONE);
    CHECK(!strcmp(out.cells[2].text,"true")&&!strcmp(out.cells[5].text,"false")&&out.cells[8].is_null&&!out.cells[8].text);free(wire.bytes);page_free(&out);free(values);
}
static void test_workbook_owners_text_bound(void) {
    char *text=dx_alloc(DX_PAGE_WIRE_TEXT_BYTES+2u);memset(text,'x',DX_PAGE_WIRE_TEXT_BYTES);text[DX_PAGE_WIRE_TEXT_BYTES]=0;
    const char *values[]={NULL,NULL,text};Wire wire=encode(owner_columns,3,1,values);Page out={0};char *error=NULL;
    CHECK(page_from_wire_workbook_owners_owned(&out,wire.bytes,wire.size,0,1,false,SIZE_MAX,&error)&&!error);
    CHECK(strlen(out.cells[2].text)==DX_PAGE_WIRE_TEXT_BYTES&&out.bytes==allocated_bytes(&out));page_free(&out);free(wire.bytes);
    text[DX_PAGE_WIRE_TEXT_BYTES]='x';text[DX_PAGE_WIRE_TEXT_BYTES+1u]=0;wire=encode(owner_columns,3,1,values);out=retained_page();
    reject_workbook_owners(&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);free(text);page_free(&out);
}
/* These synthetic I/U/B/T sidecars validate transport only. The codec still
   owns each real family's original schema/owner/position/value validation. */
typedef bool (*TableDecode)(Page *,const void *,size_t,uint64_t,uint64_t,bool,size_t,char **);
static const struct { const char *name; size_t rows,columns; TableDecode decode; } table_policies[]={
    {"sources",2049,14,page_from_wire_workbook_table_sources_owned},
    {"filters",30721,6,page_from_wire_workbook_table_filters_owned},
    {"sorts",14337,5,page_from_wire_workbook_table_sorts_owned}
};
static const Descriptor table_columns[]={
    {"sheet_id","INTEGER","i32",0,0},{"object_id","UBIGINT","u64",0,0},{"enabled","BOOLEAN","none",0,0},
    {"text_3","VARCHAR","none",0,0},{"text_4","VARCHAR","none",0,0},{"text_5","VARCHAR","none",0,0},
    {"text_6","VARCHAR","none",0,0},{"text_7","VARCHAR","none",0,0},{"text_8","VARCHAR","none",0,0},
    {"text_9","VARCHAR","none",0,0},{"text_10","VARCHAR","none",0,0},{"text_11","VARCHAR","none",0,0},
    {"text_12","VARCHAR","none",0,0},{"text_13","VARCHAR","none",0,0},{"extra","VARCHAR","none",0,0}
};
static Page table_retained(void) {
    Page out=retained_page();out.records.file_identity_valid=true;
    out.records.file_identity=(SourceFileIdentity){.device=7,.inode=UINT64_MAX,.size=99,.links=1,.mode=0100600,.modified_seconds=23,.modified_nanoseconds=17};
    return out;
}
static void reject_table(TableDecode decode,Page *out,const Wire *wire,size_t size,uint64_t offset,uint64_t total,bool pending,size_t budget) {
    unsigned char before[sizeof(*out)];memcpy(before,out,sizeof(before));unsigned released=releases;
    unsigned char *input=dx_alloc(wire->size);memcpy(input,wire->bytes,wire->size);char *error=dx_strdup("Previous error");
    SourceFileIdentity identity=out->records.file_identity;
    CHECK(!decode(out,wire->bytes,size,offset,total,pending,budget,&error));
    CHECK(error&&*error&&strcmp(error,"Previous error"));CHECK(!memcmp(out,before,sizeof(before))&&released==releases);
    CHECK(!memcmp(wire->bytes,input,wire->size)&&out->records.file_identity_valid&&source_file_identity_same(&out->records.file_identity,&identity));
    uint64_t id=0;CHECK(page_record_id(out,17,&id)&&id==UINT64_C(9007199254740993));
    CHECK(!strcmp(out->records.source_key,"retained raw reader")&&!strcmp(out->sql,"Existing SQL")&&!strcmp(out->cells[0].text,"Retained NULL literal"));
    free(input);free(error);
}
static size_t table_owned(size_t columns,size_t rows,const char *const *values) {
    size_t result=columns*sizeof(Column)+columns*rows*sizeof(Cell);
    for(size_t i=0;i<columns;i++)result+=strlen(table_columns[i].name)+strlen(table_columns[i].type)+2;
    for(size_t i=0;i<columns*rows;i++)if(values[i])result+=strlen(values[i])+1;
    return result;
}
static void test_table_envelopes(void) {
    const char *texts[]={NULL,"","NULL","İstanbul 🦆 'quoted'"};
    for(size_t p=0;p<sizeof(table_policies)/sizeof(*table_policies);p++){
        size_t rows=table_policies[p].rows,columns=table_policies[p].columns;TableDecode decode=table_policies[p].decode;
        const char **values=dx_alloc(rows*columns*sizeof(*values));
        for(size_t r=0;r<rows;r++)for(size_t c=0;c<columns;c++)values[r*columns+c]=c==0?"63":c==1?(r%2?"9007199254740993":"18446744073709551615"):
            c==2?(r%3==2?NULL:r%3?"false":"true"):texts[(r+c)%4];
        Wire wire=encode(table_columns,(uint32_t)columns,(uint32_t)rows,values);Page out=table_retained();char *error=NULL;
        // A full transport ceiling includes the codec's overflow sentinel.
        // Once/configuration is enough: verify dimensions and representatives,
        // not one CHECK per cell of the184,326-cell filters result.
        size_t owned=table_owned(columns,rows,values);CHECK(owned<=32u*1024u*1024u&&wire.size<=DX_PAGE_WIRE_BYTES);
        reject_table(decode,&out,&wire,wire.size,0,rows,true,owned-1);
        const TableDecode old_policies[]={page_from_wire_owned,page_from_wire_copy_owned,page_from_wire_workbook_sorts_owned,page_from_wire_workbook_objects_owned,page_from_wire_workbook_owners_owned};
        for(size_t old=0;old<sizeof(old_policies)/sizeof(*old_policies);old++)reject_table(old_policies[old],&out,&wire,wire.size,0,rows,true,SIZE_MAX);
        for(size_t other=0;other<sizeof(table_policies)/sizeof(*table_policies);other++)if(other!=p)
            reject_table(table_policies[other].decode,&out,&wire,wire.size,0,rows,true,SIZE_MAX);
        unsigned released=releases;unsigned char *input=dx_alloc(wire.size);memcpy(input,wire.bytes,wire.size);
        CHECK(decode(&out,wire.bytes,wire.size,0,rows,true,owned,&error)&&!error&&releases==released+1);
        CHECK(out.row_count==rows&&out.column_count==columns&&out.count_pending&&out.total==rows&&out.bytes==owned&&out.bytes==allocated_bytes(&out));
        CHECK(!memcmp(input,wire.bytes,wire.size)&&!out.records.ids&&!out.records.source_key&&!out.records.file_identity_valid&&!out.sql&&!out.totals&&!out.other);free(input);
        CHECK(out.columns[0].type==DUCKDB_TYPE_INTEGER&&out.columns[0].number_type.kind==NUM_I32&&out.columns[1].type==DUCKDB_TYPE_UBIGINT&&out.columns[1].number_type.kind==NUM_U64&&out.columns[2].type==DUCKDB_TYPE_BOOLEAN);
        const size_t samples[]={0,1,2,3,rows-1};
        for(size_t i=0;i<sizeof(samples)/sizeof(*samples);i++)for(size_t c=0;c<columns;c++){
            size_t at=samples[i]*columns+c;CHECK(out.cells[at].is_null==(values[at]==NULL));CHECK(values[at]?out.cells[at].text&&!strcmp(out.cells[at].text,values[at]):!out.cells[at].text);
        }
        printf("Table transport %s: %zu rows x %zu columns, %zu exact C-owned bytes, %zu wire bytes (32MiB caller reservation ceiling; no4MiB input/engine/RSS claim)\n",table_policies[p].name,rows,columns,out.bytes,wire.size);
        memset(wire.bytes,0,wire.size);free(wire.bytes);CHECK(!strcmp(out.cells[(rows-1)*columns+1].text,values[(rows-1)*columns+1])&&!strcmp(out.columns[3].name,"text_3"));page_free(&out);
        // Header/shape errors and byte budgets use small inputs after the one
        // full-size decode. Old Pages, record identity and release counts survive.
        wire=encode(table_columns,(uint32_t)columns,1,values);out=table_retained();
        reject_table(decode,&out,&wire,wire.size,0,1,false,0);
        reject_table(decode,&out,&wire,wire.size,1,2,false,SIZE_MAX);reject_table(decode,&out,&wire,wire.size,0,0,false,SIZE_MAX);
        reject_table(decode,&out,&wire,DX_PAGE_WIRE_BYTES+1u,0,1,false,SIZE_MAX);reject_table(decode,&out,&wire,wire.size-1,0,1,false,SIZE_MAX);
        wire.bytes[wire.nulls]=2;reject_table(decode,&out,&wire,wire.size,0,1,false,SIZE_MAX);wire.bytes[wire.nulls]=0;
        wire.bytes[wire.text]=0;reject_table(decode,&out,&wire,wire.size,0,1,false,SIZE_MAX);wire.bytes[wire.text]=0xff;reject_table(decode,&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);
        wire=encode(table_columns,(uint32_t)columns,1,values);set_word(&wire,12,(uint32_t)rows+1);reject_table(decode,&out,&wire,wire.size,0,rows+1,true,SIZE_MAX);free(wire.bytes);
        const size_t bad_columns[]={0,columns-1,columns+1};
        for(size_t i=0;i<3;i++){wire=encode(table_columns,(uint32_t)bad_columns[i],1,values);reject_table(decode,&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);}
        wire=encode(table_columns,(uint32_t)columns,0,NULL);CHECK(decode(&out,wire.bytes,wire.size,0,0,false,SIZE_MAX,&error)&&!error&&!out.row_count&&out.column_count==columns);free(wire.bytes);page_free(&out);
        wire=encode(table_columns,(uint32_t)columns,1,values);out=table_retained();size_t one=table_owned(columns,1,values);
        reject_table(decode,&out,&wire,wire.size,0,1,false,one-1);CHECK(decode(&out,wire.bytes,wire.size,0,1,false,one+1,&error)&&!error&&out.bytes==one);free(wire.bytes);page_free(&out);
        free(values);
    }
}
static void test_table_text_bounds(void) {
    for(size_t p=0;p<sizeof(table_policies)/sizeof(*table_policies);p++){
        size_t columns=table_policies[p].columns;TableDecode decode=table_policies[p].decode;
        char *text=dx_alloc(DX_PAGE_WIRE_TEXT_BYTES+2u);memset(text,'x',DX_PAGE_WIRE_TEXT_BYTES);text[DX_PAGE_WIRE_TEXT_BYTES]=0;
        const char *values[14]={0};values[3]=text;Wire wire=encode(table_columns,(uint32_t)columns,1,values);Page out={0};char *error=NULL;
        CHECK(decode(&out,wire.bytes,wire.size,0,1,false,32u*1024u*1024u,&error)&&!error&&strlen(out.cells[3].text)==DX_PAGE_WIRE_TEXT_BYTES);free(wire.bytes);page_free(&out);
        text[DX_PAGE_WIRE_TEXT_BYTES]='x';text[DX_PAGE_WIRE_TEXT_BYTES+1u]=0;wire=encode(table_columns,(uint32_t)columns,1,values);out=table_retained();
        reject_table(decode,&out,&wire,wire.size,0,1,false,SIZE_MAX);free(wire.bytes);free(text);page_free(&out);
    }
}
int main(void) {
    test_workbook_owners_envelope();test_workbook_owners_text_bound();
    test_workbook_objects_envelope();test_workbook_objects_text_bound();
    test_workbook_sorts_envelope();test_workbook_sorts_text_bound();
    test_copy_envelope();
    test_exact_owned_values(); test_row_major_and_replacement(); test_record_lookup_bounds(); test_descriptor_types(); test_bad_headers_and_lengths(); test_bad_metadata(); test_strict_utf8(); test_limits_and_empty_pages();
    unsigned original=checks;CHECK(original==60186);test_table_envelopes();test_table_text_bounds();
    printf("PASS: %u page wire checks (60186 original retained; %u Table checks)\n",checks,checks-original); return 0;
}
