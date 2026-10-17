#include "workbook/source_scope.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL source scope:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error && **error && strlen(*error)<512); free(*error); *error=NULL; }
static Analysis source(void) { return (Analysis){.kind=SOURCE_CSV,.path="/private/source.csv"}; }
static SourceDigest digest(void) {
    SourceDigest result={.bytes=UINT64_C(9007199254740993),.observed_stamp=UINT64_MAX}; result.identity.size=result.bytes;
    for (size_t i=0;i<sizeof(result.sha256);i++) result.sha256[i]=(unsigned char)(i*7+3); return result;
}
static Column column(char *name,duckdb_type type,char *type_name,NumberType number) {
    return (Column){.name=name,.type=type,.type_name=type_name,.number_type=number,.numeric=number.kind!=NUM_NONE};
}
static Page schema(Column cols[5]) {
    cols[0]=column("Id",DUCKDB_TYPE_BIGINT,"BIGINT",(NumberType){NUM_I64,0,0});
    cols[1]=column("City",DUCKDB_TYPE_VARCHAR,"VARCHAR",(NumberType){0});
    cols[2]=column("Amount",DUCKDB_TYPE_DECIMAL,"DECIMAL(38,4)",(NumberType){NUM_DECIMAL,38,4});
    cols[3]=column("When",DUCKDB_TYPE_TIMESTAMP_NS,"TIMESTAMP_NS",(NumberType){0});
    cols[4]=column("NULL",DUCKDB_TYPE_SQLNULL,"NULL",(NumberType){0});
    return (Page){.columns=cols,.column_count=5};
}
static SourcePacket raw(uint64_t owner,uint64_t version,const Page *page) {
    Analysis a=source(); SourceDigest d=digest(); SourcePacket packet={0}; char *error=NULL;
    CHECK(source_scope_raw_build(owner,version,&a,page,&d,"v1.5.6",&packet,&error) && !error); return packet;
}
static SourcePacket result(const SourcePacket *packet,const Analysis *a) {
    SourcePacket output={0}; char *error=NULL; CHECK(source_scope_result_build(packet,a,&output,&error) && !error); return output;
}
static bool equal(SourcePacket a,SourcePacket b) { return a.bytes==b.bytes && !memcmp(a.data,b.data,a.bytes); }
static SourcePacket duplicate(SourcePacket packet) {
    SourcePacket copy={dx_alloc(packet.bytes),packet.bytes}; memcpy(copy.data,packet.data,packet.bytes); return copy;
}
static void le(unsigned char *p,uint64_t n,size_t bytes) { for (size_t i=0;i<bytes;i++) { p[i]=(unsigned char)n; n>>=8; } }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void rejected(const void *data,size_t bytes) {
    SourceScopeInfo info,before; memset(&info,0xa5,sizeof(info)); memcpy(&before,&info,sizeof(before)); char *error=NULL;
    CHECK(!source_scope_validate(data,bytes,&info,&error)); CHECK(!memcmp(&info,&before,sizeof(info))); clear_error(&error);
}
static void reject_raw(uint64_t version,const Analysis *a,const Page *page,const SourceDigest *d,const char *engine) {
    SourcePacket out={dx_alloc(4),4},before=out; memcpy(out.data,"KEEP",4); char *error=NULL;
    CHECK(!source_scope_raw_build(0,version,a,page,d,engine,&out,&error));
    CHECK(out.data==before.data && out.bytes==before.bytes && !memcmp(out.data,"KEEP",4)); clear_error(&error); source_packet_free(&out);
}
static void reject_result(const SourcePacket *packet,const Analysis *a) {
    SourcePacket out={dx_alloc(4),4},before=out; memcpy(out.data,"KEEP",4); char *error=NULL;
    CHECK(!source_scope_result_build(packet,a,&out,&error)); CHECK(out.data==before.data && out.bytes==4 && !memcmp(out.data,"KEEP",4)); clear_error(&error); source_packet_free(&out);
}
static void canonical_raw(void) {
    Column cols[5]; Page page=schema(cols); SourcePacket packet=raw(UINT64_MAX,UINT64_C(9007199254740993),&page);
    SourceScopeInfo info={0}; char *error=NULL;
    CHECK(source_scope_validate(packet.data,packet.bytes,&info,&error) && !error);
    CHECK(info.kind==SOURCE_SCOPE_RAW && info.source_kind==SOURCE_CSV && info.reader==SOURCE_READER_CSV_ORDINAL_V1 && info.owner==UINT64_MAX && info.version==UINT64_C(9007199254740993));
    CHECK(info.file_bytes==digest().bytes && !strcmp(info.engine_version,"v1.5.6") && info.field_count==5 && info.raw==packet.data && info.raw_bytes==packet.bytes);
    SourceDigest d=digest(); CHECK(!memcmp(info.sha256,d.sha256,sizeof(d.sha256)));
    static const unsigned char expected[]={ 'D','X','F','S',1,0,1,1 };
    CHECK(!memcmp(packet.data,expected,sizeof(expected)) && get32(packet.data+8)==packet.bytes && !get32(packet.data+12));
    CHECK(get32(packet.data+32)==1 && get32(packet.data+36)==5);
    for (size_t i=0;i<8;i++) CHECK(packet.data[16+i]==255);
    CHECK(packet.data[24]==1 && packet.data[30]==32 && packet.data[31]==0); /* No double or native-size integer encoding. */
    for (size_t i=0;i<page.column_count;i++) {
        SourceScopeField field={0}; CHECK(source_scope_field(packet.data,packet.bytes,i,&field,&error));
        CHECK(field.ordinal==i && !strcmp(field.name,cols[i].name) && field.type==cols[i].type && field.number.kind==cols[i].number_type.kind && field.number.precision==cols[i].number_type.precision && field.number.scale==cols[i].number_type.scale);
    }
    SourceScopeField field,before; memset(&field,0x5a,sizeof(field)); memcpy(&before,&field,sizeof(field));
    CHECK(!source_scope_field(packet.data,packet.bytes,5,&field,&error) && !memcmp(&field,&before,sizeof(field))); clear_error(&error);
    CHECK(source_scope_schema_matches(packet.data,packet.bytes,&page,&error));
    Analysis a=source(); a.filter_column="CITY"; a.filter_op=FILTER_EQUAL; a.filter_value="İstanbul"; a.sort_column="Amount"; a.descending=true;
    SourcePacket other={0}; CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && equal(packet,other));
    a.path="/new/location.csv"; d.observed_stamp=0; memset(&d.identity,0xa5,sizeof(d.identity)); d.identity.size=d.bytes;
    CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && equal(packet,other)); /* Locator is separately checked by ledger. */
    cols[1].caption="Presentation"; cols[1].detail="Ignored"; cols[1].filtered=true; cols[1].sort_priority=7; cols[1].sort_descending=true;
    CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && equal(packet,other));
    CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.7",&other,&error) && !equal(packet,other));
    d.sha256[0]^=1; CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && !equal(packet,other));
    d=digest(); d.bytes++; d.identity.size++; CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && !equal(packet,other));
    a.kind=SOURCE_PARQUET; CHECK(source_scope_raw_build(UINT64_MAX,info.version,&a,&page,&d,"v1.5.6",&other,&error) && !equal(packet,other));
    CHECK(source_scope_validate(other.data,other.bytes,&info,&error) && info.reader==SOURCE_READER_PARQUET_ORDINAL_V1);
    source_packet_free(&other); source_packet_free(&packet); source_packet_free(&packet); CHECK(!packet.data && !packet.bytes);
}
static void canonical_result(void) {
    Column cols[5]; Page page=schema(cols); SourcePacket packet=raw(17,23,&page); Analysis a=source(); char *error=NULL;
    a.filter_column="Amount"; a.filter_op=FILTER_GREATER; a.filter_value="9007199254740993.1250"; a.filter_number=(NumberType){NUM_DECIMAL,38,4};
    a.filters[0]=(AnalysisFilter){"CITY","O'Reilly\t\"quoted\"\n",FILTER_EQUAL,{0}}; a.filter_count=1;
    a.sort_column="City"; a.sorts[0]=(AnalysisSort){"Amount",true}; a.sort_count=1;
    SourcePacket first=result(&packet,&a),other={0}; SourceScopeInfo info;
    CHECK(source_scope_validate(first.data,first.bytes,&info,&error) && info.kind==SOURCE_SCOPE_RESULT && info.owner==17 && info.version==23 && info.raw_bytes==packet.bytes && !memcmp(info.raw,packet.data,packet.bytes));
    Analysis b=a; b.filter_column="city"; b.filter_value=a.filters[0].value; b.filter_op=FILTER_EQUAL; b.filter_number=(NumberType){0};
    b.filters[0]=(AnalysisFilter){"AMOUNT",a.filter_value,FILTER_GREATER,{NUM_DECIMAL,38,4}};
    b.sort_column="CITY"; b.sorts[0].column="amount";
    CHECK(source_scope_result_build(&packet,&b,&other,&error) && equal(first,other)); /* Filter order and case normalize after exact type validation. */
    b.descending=true; CHECK(source_scope_result_build(&packet,&b,&other,&error) && !equal(first,other)); b=a;
    b.sort_column="Amount"; b.descending=true; b.sorts[0]=(AnalysisSort){"City",false};
    CHECK(source_scope_result_build(&packet,&b,&other,&error) && !equal(first,other));
    b=a; b.filter_value="9007199254740993.1251"; CHECK(source_scope_result_build(&packet,&b,&other,&error) && !equal(first,other));
    b=(Analysis){.kind=SOURCE_CSV,.path=a.path,.filter_column="City",.filter_op=FILTER_EQUAL};
    SourcePacket null_value=result(&packet,&b); b.filter_value=""; SourcePacket empty_value=result(&packet,&b); b.filter_value="NULL"; SourcePacket literal_value=result(&packet,&b);
    CHECK(!equal(null_value,empty_value) && !equal(null_value,literal_value) && !equal(empty_value,literal_value));
    b.filter_op=FILTER_IN; b.filter_value="[null,\"\",\"NULL\",\"O'Reilly\"]"; CHECK(source_scope_result_build(&packet,&b,&other,&error));
    b.filter_value="not-json"; reject_result(&packet,&b);
    b.filter_op=FILTER_BETWEEN; b.filter_value="[\"1\",\"2\"]"; CHECK(source_scope_result_build(&packet,&b,&other,&error));
    b.filter_value="[\"one\"]"; reject_result(&packet,&b);
    b=a; b.filter_column="missing"; reject_result(&packet,&b); b=a; b.sort_column="missing"; reject_result(&packet,&b);
    b=a; b.filters[0].column="AMOUNT"; reject_result(&packet,&b); b=a; b.sorts[0].column="CITY"; reject_result(&packet,&b);
    b=a; b.query="SELECT * FROM input"; reject_result(&packet,&b); b=a; b.unique_rows=true; reject_result(&packet,&b);
    b=a; b.kind=SOURCE_PARQUET; reject_result(&packet,&b); reject_result(&first,&a);
    SourcePacket alias=duplicate(packet); CHECK(source_scope_result_build(&alias,&a,&alias,&error) && equal(first,alias));
    char value[]="mutable"; b=(Analysis){.kind=SOURCE_CSV,.path=a.path,.filter_column="City",.filter_op=FILTER_EQUAL,.filter_value=value};
    CHECK(source_scope_result_build(&packet,&b,&other,&error)); SourcePacket saved=duplicate(other); value[0]='X'; CHECK(equal(other,saved));
    SourcePacket *all[]={&packet,&first,&other,&null_value,&empty_value,&literal_value,&alias,&saved}; for (size_t i=0;i<sizeof(all)/sizeof(*all);i++) source_packet_free(all[i]);
}
static void reject_filter_binding(const Analysis *a,const Page *page,const SourcePacket *packet,const SourceVersions *ledger) {
    SourceDigest d=digest(); reject_raw(1,a,page,&d,"v1.5.6"); reject_result(packet,a); char *error=NULL;
    size_t bytes=ledger->bytes; unsigned char *proof=ledger->items[0].raw.data;
    CHECK(!source_versions_scope_matches(ledger,0,1,a,packet->data,packet->bytes,&error)); clear_error(&error);
    CHECK(ledger->bytes==bytes && ledger->high_water==1 && ledger->items[0].raw.data==proof && equal(ledger->items[0].raw,*packet));
}
static void exact_filter_bindings(void) {
    Column cols[5]; Page page=schema(cols); Analysis a=source(); SourcePacket packet=raw(0,1,&page),other={0}; SourceDigest d=digest(); char *error=NULL;
    SourceVersions empty={0},ledger={0}; SourceVersion entry={.version=1,.kind=SOURCE_CSV,.path=a.path,.raw=packet};
    CHECK(source_versions_plan(&empty,&entry,true,&ledger,&error));
    a.filter_column="Amount"; a.filter_op=FILTER_GREATER; a.filter_value="9007199254740993.1250"; a.filter_number=(NumberType){NUM_DECIMAL,38,4};
    a.filters[0]=(AnalysisFilter){"Id","9007199254740993",FILTER_EQUAL,{NUM_I64,0,0}}; a.filter_count=1;
    CHECK(source_scope_raw_build(0,1,&a,&page,&d,"v1.5.6",&other,&error) && equal(packet,other));
    SourcePacket output=result(&packet,&a); CHECK(source_versions_scope_matches(&ledger,0,1,&a,output.data,output.bytes,&error));
    const NumberType decimal_bad[]={ {0},{NUM_I64,0,0},{NUM_FLOAT,0,0},{NUM_DECIMAL,37,4},{NUM_DECIMAL,38,3},{NUM_DECIMAL,38,39},{NUM_DECIMAL,0,0} };
    for (size_t i=0;i<sizeof(decimal_bad)/sizeof(*decimal_bad);i++) { Analysis bad=a; bad.filter_number=decimal_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    const NumberType integer_bad[]={ {0},{NUM_I32,0,0},{NUM_I64,1,0},{NUM_I64,0,1},{NUM_DECIMAL,38,0} };
    for (size_t i=0;i<sizeof(integer_bad)/sizeof(*integer_bad);i++) { Analysis bad=a; bad.filters[0].number=integer_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    /* The primary and secondary paths must enforce the same schema contract. */
    a.filter_column="City"; a.filter_value="NULL"; a.filter_op=FILTER_EQUAL; a.filter_number=(NumberType){0};
    a.filters[0]=(AnalysisFilter){"Amount","-4.50",FILTER_LESS,{NUM_DECIMAL,38,4}};
    CHECK(source_scope_result_build(&packet,&a,&other,&error));
    for (size_t i=0;i<sizeof(decimal_bad)/sizeof(*decimal_bad);i++) { Analysis bad=a; bad.filters[0].number=decimal_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    const NumberType nonnumeric_bad[]={ {NUM_I64,0,0},{NUM_NONE,1,0},{NUM_NONE,0,1},{NUM_FLOAT,0,0} };
    for (size_t i=0;i<sizeof(nonnumeric_bad)/sizeof(*nonnumeric_bad);i++) { Analysis bad=a; bad.filter_number=nonnumeric_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    a.filter_column="Id"; a.filter_value="9223372036854775807"; a.filter_number=(NumberType){NUM_I64,0,0};
    a.filters[0]=(AnalysisFilter){"When","2026-10-18",FILTER_EQUAL,{0}};
    CHECK(source_scope_result_build(&packet,&a,&other,&error));
    for (size_t i=0;i<sizeof(integer_bad)/sizeof(*integer_bad);i++) { Analysis bad=a; bad.filter_number=integer_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    for (size_t i=0;i<sizeof(nonnumeric_bad)/sizeof(*nonnumeric_bad);i++) { Analysis bad=a; bad.filters[0].number=nonnumeric_bad[i]; reject_filter_binding(&bad,&page,&packet,&ledger); }
    a.filter_count=0; a.filters[0]=(AnalysisFilter){0}; a.filter_op=FILTER_NULL; a.filter_value=NULL;
    CHECK(source_scope_result_build(&packet,&a,&other,&error));
    a.filter_number=(NumberType){0}; reject_filter_binding(&a,&page,&packet,&ledger); /* Even unused numeric bindings must be resolved for proof construction. */
    source_packet_free(&packet); source_packet_free(&other); source_packet_free(&output); source_versions_free(&ledger);
}
static void invalid_inputs_and_schema(void) {
    Column cols[5]; Page page=schema(cols); Analysis a=source(); SourceDigest d=digest(); SourcePacket packet=raw(0,1,&page); char *error=NULL;
    reject_raw(0,&a,&page,&d,"engine"); reject_raw(1,NULL,&page,&d,"engine"); reject_raw(1,&a,NULL,&d,"engine"); reject_raw(1,&a,&page,NULL,"engine"); reject_raw(1,&a,&page,&d,NULL); reject_raw(1,&a,&page,&d,"");
    d.identity.size++; reject_raw(1,&a,&page,&d,"engine"); d=digest();
    SourceKind unsupported[]={SOURCE_NONE,SOURCE_DEMO,SOURCE_DATABASE,(SourceKind)99};
    for (size_t i=0;i<sizeof(unsupported)/sizeof(*unsupported);i++) { a.kind=unsupported[i]; reject_raw(1,&a,&page,&d,"engine"); } a=source();
    a.path=""; reject_raw(1,&a,&page,&d,"engine"); a.path="*.parquet"; reject_raw(1,&a,&page,&d,"engine"); a=source();
    a.query="SELECT * FROM input"; reject_raw(1,&a,&page,&d,"engine"); a=source(); a.unique_rows=true; reject_raw(1,&a,&page,&d,"engine"); a=source(); a.group_column="City"; reject_raw(1,&a,&page,&d,"engine"); a=source(); a.measure_column="Amount"; reject_raw(1,&a,&page,&d,"engine");
    a=source(); a.schema="main"; reject_raw(1,&a,&page,&d,"engine"); a=source(); a.table="table"; reject_raw(1,&a,&page,&d,"engine"); a=source();
    a.filter_count=SIZE_MAX; reject_raw(1,&a,&page,&d,"engine"); a=source(); a.sort_count=SIZE_MAX; reject_raw(1,&a,&page,&d,"engine"); a=source();
    a.filter_column="City"; a.filter_op=(FilterOp)UINT32_MAX; reject_raw(1,&a,&page,&d,"engine"); a=source();
    a.sort_column="Id"; a.sort_count=1; a.sorts[0]=(AnalysisSort){"ID",true}; reject_raw(1,&a,&page,&d,"engine"); a=source();
    page.column_count=SIZE_MAX; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); page.columns=NULL; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[1].name="ID"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); cols[1].name=""; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[1].name="\xed\xa0\x80"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[1].type=DUCKDB_TYPE_LIST; cols[1].type_name="LIST"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); cols[1].type=DUCKDB_TYPE_ENUM; cols[1].type_name="ENUM"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[0].numeric=false; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); cols[0].number_type.scale=1; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[0].type_name="INTEGER"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); cols[2].type_name="DECIMAL(38,3)"; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    cols[2].number_type.precision=39; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols); cols[2].number_type.scale=39; reject_raw(1,&a,&page,&d,"engine"); page=schema(cols);
    char *long_name=dx_alloc(DX_SOURCE_SCOPE_NAME_BYTES+2); memset(long_name,'n',DX_SOURCE_SCOPE_NAME_BYTES+1); cols[0].name=long_name; reject_raw(1,&a,&page,&d,"engine"); free(long_name); page=schema(cols);
    char *long_engine=dx_alloc(DX_SOURCE_SCOPE_ENGINE_BYTES+2); memset(long_engine,'e',DX_SOURCE_SCOPE_ENGINE_BYTES+1); reject_raw(1,&a,&page,&d,long_engine); free(long_engine);
    cols[0].name="Changed"; CHECK(!source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); clear_error(&error); page=schema(cols);
    Column swap=cols[0]; cols[0]=cols[1]; cols[1]=swap; CHECK(!source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); clear_error(&error); page=schema(cols);
    cols[2]=column("Amount",DUCKDB_TYPE_DECIMAL,"DECIMAL(38,3)",(NumberType){NUM_DECIMAL,38,3}); CHECK(!source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); clear_error(&error); page=schema(cols);
    page.column_count--; CHECK(!source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); clear_error(&error); source_packet_free(&packet);
}
static void all_known_types(void) {
    static const struct { duckdb_type type; char *name; NumberKind number; } known[]={
        {DUCKDB_TYPE_BOOLEAN,"BOOLEAN",NUM_NONE},{DUCKDB_TYPE_TINYINT,"TINYINT",NUM_I8},{DUCKDB_TYPE_SMALLINT,"SMALLINT",NUM_I16},{DUCKDB_TYPE_INTEGER,"INTEGER",NUM_I32},{DUCKDB_TYPE_BIGINT,"BIGINT",NUM_I64},{DUCKDB_TYPE_HUGEINT,"HUGEINT",NUM_I128},
        {DUCKDB_TYPE_UTINYINT,"UTINYINT",NUM_U8},{DUCKDB_TYPE_USMALLINT,"USMALLINT",NUM_U16},{DUCKDB_TYPE_UINTEGER,"UINTEGER",NUM_U32},{DUCKDB_TYPE_UBIGINT,"UBIGINT",NUM_U64},{DUCKDB_TYPE_UHUGEINT,"UHUGEINT",NUM_U128},
        {DUCKDB_TYPE_FLOAT,"FLOAT",NUM_FLOAT},{DUCKDB_TYPE_DOUBLE,"DOUBLE",NUM_FLOAT},{DUCKDB_TYPE_DATE,"DATE",NUM_NONE},{DUCKDB_TYPE_TIME,"TIME",NUM_NONE},
        {DUCKDB_TYPE_TIMESTAMP,"TIMESTAMP",NUM_NONE},{DUCKDB_TYPE_TIMESTAMP_S,"TIMESTAMP_S",NUM_NONE},{DUCKDB_TYPE_TIMESTAMP_MS,"TIMESTAMP_MS",NUM_NONE},{DUCKDB_TYPE_TIMESTAMP_NS,"TIMESTAMP_NS",NUM_NONE},{DUCKDB_TYPE_TIMESTAMP_TZ,"TIMESTAMPTZ",NUM_NONE},{DUCKDB_TYPE_TIME_TZ,"TIMETZ",NUM_NONE},
        {DUCKDB_TYPE_INTERVAL,"INTERVAL",NUM_NONE},{DUCKDB_TYPE_VARCHAR,"VARCHAR",NUM_NONE},{DUCKDB_TYPE_BLOB,"BLOB",NUM_NONE},{DUCKDB_TYPE_UUID,"UUID",NUM_NONE},{DUCKDB_TYPE_SQLNULL,"NULL",NUM_NONE}
    };
    char *error=NULL;
    for (size_t i=0;i<sizeof(known)/sizeof(*known);i++) {
        Column col=column("Field",known[i].type,known[i].name,(NumberType){known[i].number,0,0}); Page page={.columns=&col,.column_count=1}; SourcePacket packet=raw(0,1,&page); SourceScopeField field;
        CHECK(source_scope_field(packet.data,packet.bytes,0,&field,&error) && field.type==known[i].type && field.number.kind==known[i].number); CHECK(source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); source_packet_free(&packet);
    }
    for (unsigned precision=1;precision<=38;precision++) for (unsigned scale=0;scale<=precision;scale++) {
        char type[32]; snprintf(type,sizeof(type),"DECIMAL(%u,%u)",precision,scale); Column col=column("Exact",DUCKDB_TYPE_DECIMAL,type,(NumberType){NUM_DECIMAL,(unsigned char)precision,(unsigned char)scale}); Page page={.columns=&col,.column_count=1}; SourcePacket packet=raw(0,UINT64_MAX,&page); SourceScopeField field;
        CHECK(source_scope_field(packet.data,packet.bytes,0,&field,&error) && field.number.precision==precision && field.number.scale==scale); CHECK(source_scope_schema_matches(packet.data,packet.bytes,&page,&error)); source_packet_free(&packet);
    }
}
static void bad_byte(SourcePacket packet,size_t offset,unsigned char value) {
    SourcePacket copy=duplicate(packet); CHECK(offset<copy.bytes); copy.data[offset]=value; rejected(copy.data,copy.bytes); source_packet_free(&copy);
}
static void adversarial_packets(void) {
    Column cols[5]; Page page=schema(cols); SourcePacket packet=raw(0,1,&page);
    for (size_t i=0;i<packet.bytes;i++) rejected(packet.data,i);
    rejected(NULL,packet.bytes); rejected(packet.data,SIZE_MAX); rejected(packet.data,DX_SOURCE_SCOPE_BYTES);
    bad_byte(packet,0,'X'); bad_byte(packet,4,2); bad_byte(packet,5,1); bad_byte(packet,6,0); bad_byte(packet,7,99); bad_byte(packet,8,0); bad_byte(packet,12,1); bad_byte(packet,24,0); bad_byte(packet,32,2); bad_byte(packet,36,0); bad_byte(packet,80,255);
    SourceScopeInfo info; char *error=NULL; CHECK(source_scope_validate(packet.data,packet.bytes,&info,&error)); size_t field=(size_t)(info.fields-packet.data);
    bad_byte(packet,field,1); bad_byte(packet,field+4,255); bad_byte(packet,field+6,1); bad_byte(packet,field+8,0); bad_byte(packet,field+12,0xc0); bad_byte(packet,field+14,1);
    SourcePacket extra={dx_alloc(packet.bytes+1),packet.bytes+1}; memcpy(extra.data,packet.data,packet.bytes); extra.data[packet.bytes]=0; le(extra.data+8,extra.bytes,4); rejected(extra.data,extra.bytes); source_packet_free(&extra);
    Analysis a=source(); a.filter_column="City"; a.filter_op=FILTER_EQUAL; a.filter_value="NULL"; a.filters[0]=(AnalysisFilter){"Amount","-4.50",FILTER_GREATER,{NUM_DECIMAL,38,4}}; a.filter_count=1;
    a.sort_column="Id"; a.sorts[0]=(AnalysisSort){"Amount",true}; a.sort_count=1;
    SourcePacket output=result(&packet,&a); for (size_t i=0;i<output.bytes;i++) rejected(output.data,i);
    bad_byte(output,16,1); bad_byte(output,24,2); bad_byte(output,7,2); bad_byte(output,36+6,2);
    size_t filters=36+packet.bytes,first=filters+4,second=first+12+5;
    bad_byte(output,filters,17); bad_byte(output,first,5); bad_byte(output,first+4,255); bad_byte(output,first+8,255);
    bad_byte(output,second,1); /* Duplicate original ordinal. */
    size_t sorts=second+12+6,sort=sorts+4;
    CHECK(get32(output.data+sorts)==2 && output.data[sort]==0 && output.data[sort+8]==2);
    bad_byte(output,sorts,9); bad_byte(output,sort+4,2); bad_byte(output,sort+8,0);
    uint32_t random=UINT32_C(8132261);
    for (size_t i=0;i<2500;i++) {
        SourcePacket mutated=duplicate(i&1 ? packet : output); random=random*UINT32_C(1664525)+UINT32_C(1013904223); size_t at=random%mutated.bytes;
        random=random*UINT32_C(1664525)+UINT32_C(1013904223); mutated.data[at]^=(unsigned char)(1u<<(random%8)); SourceScopeInfo view,before; memset(&view,0x5a,sizeof(view)); memcpy(&before,&view,sizeof(view));
        bool accepted=source_scope_validate(mutated.data,mutated.bytes,&view,&error);
        if (accepted) CHECK((view.kind==SOURCE_SCOPE_RAW || view.kind==SOURCE_SCOPE_RESULT) && view.field_count==5 && view.raw_bytes<DX_SOURCE_SCOPE_BYTES);
        else { CHECK(!memcmp(&view,&before,sizeof(view))); clear_error(&error); }
        source_packet_free(&mutated);
    }
    source_packet_free(&packet); source_packet_free(&output);
}
/* Construct a genuinely valid packet at an exact byte boundary, not opaque
   padding accepted as schema. Every name is unique, valid and <=4096 bytes. */
static Page sized_schema(size_t packet_bytes) {
    size_t count=64,base=32+48+5+strlen("v1.5.6")+13*count; CHECK(packet_bytes>=base);
    size_t names=packet_bytes-base; Page page={.column_count=count,.columns=dx_alloc(count*sizeof(Column))};
    for (size_t i=0;i<count;i++) {
        size_t n=names/count+(i<names%count); CHECK(n>=8 && n<=DX_SOURCE_SCOPE_NAME_BYTES); char *name=dx_alloc(n+1); memset(name,'x',n); snprintf(name,8,"f%05zu_",i); name[7]='x';
        page.columns[i]=column(name,DUCKDB_TYPE_VARCHAR,"VARCHAR",(NumberType){0});
    }
    return page;
}
static void sized_schema_free(Page *page) { for (size_t i=0;i<page->column_count;i++) free(page->columns[i].name); free(page->columns); *page=(Page){0}; }
static void packet_budgets_and_ownership(void) {
    Page page=sized_schema(DX_SOURCE_SCOPE_BYTES-1); SourcePacket packet=raw(0,1,&page); CHECK(packet.bytes==DX_SOURCE_SCOPE_BYTES-1);
    Analysis a=source(); reject_result(&packet,&a);
    char *name=page.columns[0].name; size_t n=strlen(name); name=dx_realloc(name,n+2); name[n]='z'; name[n+1]=0; page.columns[0].name=name;
    SourceDigest d=digest(); reject_raw(1,&a,&page,&d,"v1.5.6"); sized_schema_free(&page); source_packet_free(&packet);
    page=sized_schema(200000); packet=raw(0,1,&page);
    size_t text_bytes=DX_SOURCE_SCOPE_BYTES-1-(32+12+packet.bytes+13); CHECK(text_bytes<DX_SOURCE_SCOPE_TEXT_BYTES);
    char *value=dx_alloc(text_bytes+2); memset(value,'v',text_bytes); a.filter_column=page.columns[0].name; a.filter_op=FILTER_CONTAINS; a.filter_value=value;
    SourcePacket output=result(&packet,&a); CHECK(output.bytes==DX_SOURCE_SCOPE_BYTES-1); value[text_bytes]='v'; reject_result(&packet,&a); value[text_bytes]=0;
    SourcePacket saved=duplicate(output); page.columns[0].name[0]='Q'; memset(d.sha256,0,sizeof(d.sha256)); value[0]='X'; CHECK(equal(output,saved)); sized_schema_free(&page); free(value);
    SourceScopeInfo info; char *error=NULL; CHECK(source_scope_validate(output.data,output.bytes,&info,&error) && info.field_count==64);
    source_packet_free(&packet); source_packet_free(&output); source_packet_free(&saved);
}
static void adopt(SourceVersions *live,SourceVersions *next) { source_versions_free(live); *live=*next; *next=(SourceVersions){0}; }
static void reject_plan(const SourceVersions *live,const SourceVersion *entry,bool issue,SourceVersions *out) {
    SourceVersions before=*out; char *error=NULL; unsigned char *raw_data=out->count ? out->items[0].raw.data : NULL; char *path=out->count ? out->items[0].path : NULL;
    CHECK(!source_versions_plan(live,entry,issue,out,&error)); CHECK(!memcmp(&before,out,sizeof(before)));
    if (out->count) CHECK(raw_data==out->items[0].raw.data && path==out->items[0].path);
    clear_error(&error);
}
static void ledger_lifecycle(void) {
    Column cols[5]; Page page=schema(cols); SourceVersions live={0},next={0},copy={0}; char *error=NULL; Analysis a=source();
    SourceVersion entry={.owner=0,.version=1,.kind=SOURCE_CSV,.path=a.path};
    CHECK(source_versions_valid(&live,0,&error) && source_versions_plan(&live,&entry,true,&next,&error));
    CHECK(!live.count && !live.high_water && next.high_water==1 && next.count==1 && next.capacity==1 && next.bytes==sizeof(SourceVersion)+strlen(a.path)+1);
    adopt(&live,&next); CHECK(source_versions_valid(&live,0,&error)); SourcePacket packet=raw(0,1,&page);
    CHECK(!source_versions_scope_matches(&live,0,1,&a,packet.data,packet.bytes,&error)); clear_error(&error);
    entry.raw=packet; CHECK(source_versions_plan(&live,&entry,false,&next,&error) && !live.items[0].raw.data);
    adopt(&live,&next); CHECK(live.items[0].path!=a.path && live.items[0].raw.data!=packet.data && equal(live.items[0].raw,packet));
    CHECK(source_versions_copy(&copy,&live,&error) && copy.items!=live.items && copy.items[0].path!=live.items[0].path && copy.items[0].raw.data!=live.items[0].raw.data);
    CHECK(source_versions_copy(&copy,&copy,&error) && source_versions_scope_matches(&live,0,1,&a,packet.data,packet.bytes,&error));
    a.filter_column="City"; a.filter_op=FILTER_EQUAL; a.filter_value="İstanbul"; a.sort_column="Amount"; a.descending=true;
    SourcePacket output=result(&packet,&a); CHECK(source_versions_scope_matches(&live,0,1,&a,output.data,output.bytes,&error));
    a.filter_value="Paris"; CHECK(source_versions_scope_matches(&live,0,1,&a,packet.data,packet.bytes,&error));
    CHECK(!source_versions_scope_matches(&live,0,1,&a,output.data,output.bytes,&error)); clear_error(&error);
    a=source(); a.path="/different.csv"; CHECK(!source_versions_scope_matches(&live,0,1,&a,packet.data,packet.bytes,&error)); clear_error(&error); a=source();
    CHECK(!source_versions_scope_matches(&live,0,0,&a,packet.data,packet.bytes,&error)); clear_error(&error);
    CHECK(!source_versions_scope_matches(&live,1,1,&a,packet.data,packet.bytes,&error)); clear_error(&error);
    SourceDigest changed=digest(); changed.sha256[0]^=1; SourcePacket other={0}; CHECK(source_scope_raw_build(0,1,&a,&page,&changed,"v1.5.6",&other,&error));
    CHECK(!source_versions_scope_matches(&live,0,1,&a,other.data,other.bytes,&error)); clear_error(&error);
    reject_plan(&live,&entry,false,&copy); /* Already completed, even with the same bytes. */
    entry.version=2; entry.raw=(SourcePacket){0}; entry.path="/refresh.csv";
    CHECK(source_versions_plan(&live,&entry,true,&next,&error) && next.high_water==2 && live.high_water==1); adopt(&live,&next);
    CHECK(source_versions_scope_matches(&live,0,1,&a,packet.data,packet.bytes,&error)); /* Undo can reference old immutable version. */
    CHECK(!source_versions_scope_matches(&live,0,2,&a,packet.data,packet.bytes,&error)); clear_error(&error);
    entry.raw=packet; reject_plan(&live,&entry,false,&copy); /* Proof names version 1, not pending 2. */
    SourcePacket second=raw(0,2,&page); entry.raw=second; entry.path="/wrong.csv"; reject_plan(&live,&entry,false,&copy); entry.path="/refresh.csv";
    entry.owner=1; reject_plan(&live,&entry,false,&copy); entry.owner=0; entry.raw=output; reject_plan(&live,&entry,false,&copy); entry.raw=second;
    CHECK(source_versions_plan(&live,&entry,false,&next,&error)); adopt(&live,&next); CHECK(live.high_water==2);
    entry=(SourceVersion){.owner=UINT64_MAX,.version=3,.kind=SOURCE_PARQUET,.path="retired.parquet"}; CHECK(source_versions_plan(&live,&entry,true,&next,&error)); adopt(&live,&next);
    CHECK(!source_versions_valid(&live,UINT64_MAX-1,&error)); clear_error(&error); CHECK(source_versions_valid(&live,UINT64_MAX,&error));
    SourceVersions malformed=live; malformed.high_water=2; CHECK(!source_versions_copy(&copy,&malformed,&error) && copy.count==1 && copy.high_water==1); clear_error(&error);
    malformed=live; malformed.bytes++; CHECK(!source_versions_valid(&malformed,UINT64_MAX,&error)); clear_error(&error);
    malformed=live; malformed.count=SIZE_MAX; CHECK(!source_versions_copy(&copy,&malformed,&error)); clear_error(&error);
    entry.version=2; reject_plan(&live,&entry,true,&copy); entry.version=5; reject_plan(&live,&entry,true,&copy);
    source_packet_free(&packet); source_packet_free(&second); source_packet_free(&output); source_packet_free(&other); source_versions_free(&live);
    CHECK(source_versions_valid(&copy,0,&error) && copy.count==1); a=source(); CHECK(source_versions_scope_matches(&copy,0,1,&a,copy.items[0].raw.data,copy.items[0].raw.bytes,&error)); source_versions_free(&copy);
    live=(SourceVersions){.high_water=UINT64_C(9007199254740992)}; entry=(SourceVersion){.version=UINT64_C(9007199254740993),.kind=SOURCE_CSV,.path="huge.csv"}; CHECK(source_versions_plan(&live,&entry,true,&next,&error) && next.high_water==entry.version); adopt(&live,&next); source_versions_free(&live);
    live=(SourceVersions){.high_water=UINT64_MAX-1}; entry.version=UINT64_MAX; CHECK(source_versions_plan(&live,&entry,true,&next,&error)); adopt(&live,&next); entry.version=0; reject_plan(&live,&entry,true,&copy); entry.version=UINT64_MAX; reject_plan(&live,&entry,true,&copy);
    CHECK(source_versions_copy(&copy,&live,&error) && copy.high_water==UINT64_MAX); source_versions_free(&live); source_versions_free(&copy);
}
static void ledger_limits_and_malformed_ownership(void) {
    char *error=NULL; SourceVersions live={.high_water=DX_SOURCE_VERSIONS,.count=DX_SOURCE_VERSIONS,.capacity=DX_SOURCE_VERSIONS},out={0};
    live.items=dx_alloc(live.capacity*sizeof(*live.items)); live.bytes=live.capacity*sizeof(*live.items);
    for (size_t i=0;i<live.count;i++) { live.items[i]=(SourceVersion){.version=i+1,.kind=SOURCE_CSV,.path=dx_strdup("p")}; live.bytes+=2; }
    CHECK(source_versions_valid(&live,0,&error)); SourceVersion entry={.version=DX_SOURCE_VERSIONS+1,.kind=SOURCE_CSV,.path="p"}; reject_plan(&live,&entry,true,&out);
    live.items[1].version=1; CHECK(!source_versions_valid(&live,0,&error)); clear_error(&error); live.items[1].version=2;
    live.count=SIZE_MAX; source_versions_free(&live); CHECK(!live.items && !live.capacity && !live.count); /* Actual capacity, not tampered count, governs cleanup. */
    live=(SourceVersions){.high_water=1,.count=1,.capacity=3,.bytes=3*sizeof(SourceVersion)+2}; live.items=dx_alloc(live.capacity*sizeof(*live.items)); live.items[0]=(SourceVersion){.version=1,.kind=SOURCE_CSV,.path=dx_strdup("p")};
    CHECK(source_versions_valid(&live,0,&error)); CHECK(source_versions_copy(&out,&live,&error) && out.capacity==1 && out.bytes==sizeof(SourceVersion)+2);
    live.items[2].path=dx_strdup("owned invalid slot"); CHECK(!source_versions_valid(&live,0,&error)); clear_error(&error); source_versions_free(&live); source_versions_free(&out);
    Page page=sized_schema(240u*1024u); SourceVersions next={0};
    for (uint64_t version=1;version<=4;version++) {
        SourcePacket packet=raw(0,version,&page); entry=(SourceVersion){.version=version,.kind=SOURCE_CSV,.path="p",.raw=packet}; char *path=NULL;
        if (version==4) {
            size_t path_bytes=DX_SOURCE_VERSIONS_BYTES-4*sizeof(SourceVersion)-4*packet.bytes-6; CHECK(path_bytes>1 && path_bytes<=DX_SOURCE_SCOPE_TEXT_BYTES+1);
            path=dx_alloc(path_bytes); memset(path,'p',path_bytes-1); entry.path=path;
        }
        CHECK(source_versions_plan(&live,&entry,true,&next,&error)); adopt(&live,&next); free(path); source_packet_free(&packet);
    }
    CHECK(live.bytes==DX_SOURCE_VERSIONS_BYTES && source_versions_valid(&live,0,&error)); entry=(SourceVersion){.version=5,.kind=SOURCE_CSV,.path="p"}; reject_plan(&live,&entry,true,&out);
    CHECK(source_versions_copy(&out,&live,&error) && out.bytes==DX_SOURCE_VERSIONS_BYTES);
    source_versions_free(&live); source_versions_free(&out); sized_schema_free(&page);
}
int main(void) {
    canonical_raw(); canonical_result(); exact_filter_bindings(); invalid_inputs_and_schema(); all_known_types(); adversarial_packets(); packet_budgets_and_ownership(); ledger_lifecycle(); ledger_limits_and_malformed_ownership();
    printf("PASS: %u source scope checks\n",checks); return 0;
}
