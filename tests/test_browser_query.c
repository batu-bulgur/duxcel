#include "data/browser_query.h"
#include "data/page_wire.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks,releases;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL browser query:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static void reject_sql(char *sql,char **error) { CHECK(!sql && *error && **error); free(*error); *error=NULL; }
static void release(void *text) { releases++; free(text); }
static Page retained(void) {
    Page page={.column_count=1,.row_count=1,.bytes=1234,.total=99,.offset=8,.count_pending=true,.release_cell_text=release};
    page.columns=dx_alloc(sizeof(Column)); page.cells=dx_alloc(sizeof(Cell));
    page.columns[0]=(Column){.name=dx_strdup("Keep"),.type_name=dx_strdup("VARCHAR")};
    page.cells[0].text=dx_strdup("Retained state"); return page;
}
static Page describe_page(size_t rows,const char *const *names,const char *const *types) {
    static const char *headers[]={"column_name","column_type","null","key","default","extra"};
    Page page={.column_count=6,.row_count=rows,.total=rows};
    page.columns=dx_alloc(6*sizeof(Column)); if (rows) page.cells=dx_alloc(rows*6*sizeof(Cell));
    for (size_t i=0;i<6;i++) page.columns[i]=(Column){.name=dx_strdup(headers[i]),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    for (size_t i=0;i<rows;i++) {
        char name[64]; snprintf(name,sizeof(name),"column_%zu",i);
        page.cells[i*6].text=dx_strdup(names ? names[i] : name);
        page.cells[i*6+1].text=dx_strdup(types ? types[i] : "BIGINT");
        page.cells[i*6+2].text=dx_strdup("YES");
        for (size_t c=3;c<6;c++) page.cells[i*6+c].is_null=true;
    }
    return page;
}
static void reject_describe(Page *out,const Page *describe) {
    Page before=*out; char *error=NULL; unsigned released=releases;
    CHECK(!browser_schema_from_describe(out,describe,&error));
    CHECK(error && *error && !memcmp(out,&before,sizeof(before)) && releases==released);
    CHECK(!strcmp(out->cells[0].text,"Retained state")); free(error);
}
static void test_relation_sql(void) {
    char *error=NULL; Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("O'Reilly.csv")};
    char *sql=browser_relation_sql(&source,NULL,&error),*expected=analysis_sql(&source,false);
    CHECK(sql && !error && !strcmp(sql,expected) && strstr(sql,"read_csv_auto('O''Reilly.csv',allow_quoted_nulls=false)"));
    free(sql); free(expected);
    source.unique_rows=true;
    CHECK(analysis_query_set(&source,"SELECT *, 1::BIGINT AS extra FROM input",&error));
    CHECK(analysis_filter_set(&source,"A\"B",FILTER_EQUAL,"9007199254740993",&error));
    analysis_filter_number(&source,"A\"B",(NumberType){NUM_I64,0,0});
    CHECK(analysis_filter_set(&source,"City",FILTER_CONTAINS,"O'Reilly",&error));
    CHECK(analysis_sort_set(&source,"A\"B",true,false,&error));
    sql=browser_relation_sql(&source,NULL,&error); expected=analysis_sql(&source,false);
    CHECK(sql && !strcmp(sql,expected) && strstr(sql,"SELECT DISTINCT") && strstr(sql,"WITH input AS"));
    CHECK(strstr(sql,"\"A\"\"B\" = cast_to_type('9007199254740993',\"A\"\"B\")") && strstr(sql,"'O''Reilly'") && !strstr(sql,"$__duxcel_filter"));
    CHECK(strstr(sql,"ORDER BY \"A\"\"B\" DESC")); free(expected);
    analysis_free(&source); CHECK(strstr(sql,"O''Reilly.csv")); free(sql);
    source=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("transactions.parquet"),.group_column=dx_strdup("City"),.measure_column=dx_strdup("Amount"),.aggregate=AGG_AVG};
    sql=browser_relation_sql(&source,NULL,&error); expected=analysis_sql(&source,false);
    CHECK(sql && !strcmp(sql,expected) && strstr(sql,"read_parquet('transactions.parquet')") && strstr(sql,"PIVOT (") && strstr(sql,"AVG(\"Amount\")"));
    free(sql); free(expected); analysis_free(&source);
    source=(Analysis){.kind=SOURCE_DEMO}; sql=browser_relation_sql(&source,NULL,&error); expected=analysis_sql(&source,false);
    CHECK(sql && !strcmp(sql,expected) && strstr(sql,"range(1000000)")); free(sql); free(expected);
    PivotDefinition pivot={0};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"Approved",AGG_SUM,&error));
    sql=browser_relation_sql(&source,&pivot,&error); expected=pivot_sql(&source,&pivot);
    CHECK(sql && !strcmp(sql,expected) && strstr(sql,"PIVOT (") && strstr(sql," ON ") && strstr(sql," USING SUM("));
    CHECK(strstr(sql,"date_trunc('month'") && strstr(sql,"GROUP BY \"City\"") && !error);
    free(sql); free(expected); pivot_free(&pivot);
}
static void test_invalid_sources(void) {
    char *error=NULL; Analysis source={0}; reject_sql(browser_relation_sql(NULL,NULL,&error),&error);
    reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.kind=SOURCE_DATABASE; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.kind=(SourceKind)99; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.kind=SOURCE_CSV; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.path=""; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO,.filter_count=SIZE_MAX}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.filter_count=0; source.sort_count=SIZE_MAX; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.sort_count=0; source.aggregate=(Aggregate)99; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.aggregate=AGG_SUM; source.query=""; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.query=NULL; source.group_column=""; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.group_column=NULL; source.filter_count=1; source.filters[0].column="City";
    reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO,.filter_column="City",.filter_op=(FilterOp)99}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.filter_op=FILTER_BETWEEN; source.filter_value="[\"only one\"]"; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source.filter_op=FILTER_EQUAL; source.filter_number=(NumberType){NUM_DECIMAL,0,0}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO,.sort_column="City",.sort_count=1,.sorts={{"CITY",false}}}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO}; PivotDefinition pivot={0}; reject_sql(browser_relation_sql(&source,&pivot,&error),&error);
    pivot.counts[PIVOT_ROWS]=1; pivot.fields[PIVOT_ROWS][0].name="City"; pivot.counts[PIVOT_COLUMNS]=SIZE_MAX;
    reject_sql(browser_relation_sql(&source,&pivot,&error),&error); /* Pre-validates cross-shelf iteration. */
    pivot.counts[PIVOT_COLUMNS]=1; pivot.counts[PIVOT_VALUES]=1; pivot.fields[PIVOT_VALUES][0].name="Amount";
    reject_sql(browser_relation_sql(&source,&pivot,&error),&error); /* NULL Columns name cannot enter pivot_valid's Rows comparison. */
    pivot=(PivotDefinition){.counts={[PIVOT_VALUES]=1},.fields={[PIVOT_VALUES]={{.name="Amount",.aggregate=(Aggregate)99}}}};
    reject_sql(browser_relation_sql(&source,&pivot,&error),&error);
    pivot.fields[PIVOT_VALUES][0].aggregate=AGG_SUM; pivot.fields[PIVOT_VALUES][0].grain=PIVOT_DATE_YEAR;
    reject_sql(browser_relation_sql(&source,&pivot,&error),&error);
    char *large=dx_alloc(DX_BROWSER_SQL_BYTES+2); memset(large,'\'',DX_BROWSER_SQL_BYTES+1);
    source=(Analysis){.kind=SOURCE_CSV,.path=large}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO,.query=large}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    source=(Analysis){.kind=SOURCE_DEMO,.filter_column="City",.filter_value=large}; reject_sql(browser_relation_sql(&source,NULL,&error),&error);
    large[600000]=0; source.filter_op=FILTER_EQUAL; reject_sql(browser_relation_sql(&source,NULL,&error),&error); /* SQL quote expansion exceeds 1 MiB. */
    free(large);
    CHECK(!browser_relation_sql(NULL,NULL,NULL));
}
static void test_sql_type_parser(void) {
    static const struct { const char *name; duckdb_type type; NumberType number; } valid[]={
        {"TINYINT",DUCKDB_TYPE_TINYINT,{NUM_I8,0,0}},{"SMALLINT",DUCKDB_TYPE_SMALLINT,{NUM_I16,0,0}},
        {"INTEGER",DUCKDB_TYPE_INTEGER,{NUM_I32,0,0}},{"BIGINT",DUCKDB_TYPE_BIGINT,{NUM_I64,0,0}},
        {"HUGEINT",DUCKDB_TYPE_HUGEINT,{NUM_I128,0,0}},{"UTINYINT",DUCKDB_TYPE_UTINYINT,{NUM_U8,0,0}},
        {"USMALLINT",DUCKDB_TYPE_USMALLINT,{NUM_U16,0,0}},{"UINTEGER",DUCKDB_TYPE_UINTEGER,{NUM_U32,0,0}},
        {"UBIGINT",DUCKDB_TYPE_UBIGINT,{NUM_U64,0,0}},{"UHUGEINT",DUCKDB_TYPE_UHUGEINT,{NUM_U128,0,0}},
        {"FLOAT",DUCKDB_TYPE_FLOAT,{NUM_FLOAT,0,0}},{"double",DUCKDB_TYPE_DOUBLE,{NUM_FLOAT,0,0}},
        {"DECIMAL(38,18)",DUCKDB_TYPE_DECIMAL,{NUM_DECIMAL,38,18}},{"numeric( 1, 0 )",DUCKDB_TYPE_DECIMAL,{NUM_DECIMAL,1,0}},
        {"DECIMAL(38,38)",DUCKDB_TYPE_DECIMAL,{NUM_DECIMAL,38,38}},{"BOOLEAN",DUCKDB_TYPE_BOOLEAN,{0}},
        {"DATE",DUCKDB_TYPE_DATE,{0}},{"TIMESTAMP",DUCKDB_TYPE_TIMESTAMP,{0}},{"TIMESTAMP_NS",DUCKDB_TYPE_TIMESTAMP_NS,{0}},
        {"TIMESTAMP_MS",DUCKDB_TYPE_TIMESTAMP_MS,{0}},{"TIMESTAMPTZ",DUCKDB_TYPE_TIMESTAMP_TZ,{0}},
        {"SQLNULL",DUCKDB_TYPE_SQLNULL,{0}},{"VARCHAR",DUCKDB_TYPE_VARCHAR,{0}},{"STRUCT(a INTEGER)",DUCKDB_TYPE_VARCHAR,{0}},
        {"DECIMAL(38,18)[]",DUCKDB_TYPE_VARCHAR,{0}}
    };
    for (size_t i=0;i<sizeof(valid)/sizeof(*valid);i++) {
        duckdb_type type=DUCKDB_TYPE_INVALID; NumberType number={NUM_U128,1,1};
        CHECK(page_type_from_sql(valid[i].name,&type,&number));
        CHECK(type==valid[i].type && number.kind==valid[i].number.kind && number.precision==valid[i].number.precision && number.scale==valid[i].number.scale);
    }
    const char *invalid[]={NULL,"","DECIMAL","NUMERIC","DECIMAL(39,0)","DECIMAL(1,2)","DECIMAL(0,0)","DECIMAL(38,-1)","DECIMAL(38,1)x","DECIMAL(38,1,2)","DECIMAL(999999999999999,1)","DECIMAL(1,999999999999)","DECIMAL(a,b)","\xc0\x80"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        duckdb_type type=DUCKDB_TYPE_UUID; NumberType number={NUM_I64,0,0};
        CHECK(!page_type_from_sql(invalid[i],&type,&number)); CHECK(type==DUCKDB_TYPE_UUID && number.kind==NUM_I64 && !number.precision && !number.scale);
    }
    char long_type[130]; memset(long_type,'x',129); long_type[129]=0; duckdb_type type; NumberType number;
    CHECK(!page_type_from_sql(long_type,&type,&number)); CHECK(!page_type_from_sql("BIGINT",NULL,&number)); CHECK(!page_type_from_sql("BIGINT",&type,NULL));
}
static void test_describe_conversion(void) {
    const char *names[]={"Missing","Empty","Literal","A\"B","Amount","Date","Approved","Nested"};
    const char *types[]={"VARCHAR","VARCHAR","VARCHAR","HUGEINT","DECIMAL(38,18)","TIMESTAMP_NS","BOOLEAN","STRUCT(a INTEGER)"};
    Page describe=describe_page(8,names,types),schema=retained(); unsigned released=releases; char *error=NULL;
    CHECK(browser_schema_from_describe(&schema,&describe,&error));
    CHECK(!error && releases==released+1 && schema.column_count==8 && !schema.row_count && !schema.total && !schema.cells && !schema.release_cell_text);
    size_t bytes=8*sizeof(Column);
    for (size_t i=0;i<8;i++) {
        CHECK(schema.columns[i].name!=describe.cells[i*6].text && !strcmp(schema.columns[i].name,names[i]) && !strcmp(schema.columns[i].type_name,types[i]));
        bytes+=strlen(names[i])+strlen(types[i])+2;
    }
    CHECK(schema.bytes==bytes && schema.columns[3].number_type.kind==NUM_I128 && schema.columns[4].number_type.kind==NUM_DECIMAL);
    CHECK(schema.columns[4].number_type.precision==38 && schema.columns[4].number_type.scale==18 && schema.columns[4].numeric);
    CHECK(schema.columns[5].type==DUCKDB_TYPE_TIMESTAMP_NS && schema.columns[6].type==DUCKDB_TYPE_BOOLEAN && !schema.columns[6].numeric && schema.columns[7].type==DUCKDB_TYPE_VARCHAR);
    memset(describe.cells[4*6+1].text,'x',strlen(describe.cells[4*6+1].text)); page_free(&describe);
    CHECK(!strcmp(schema.columns[4].type_name,"DECIMAL(38,18)")); page_free(&schema); page_free(&schema);
    describe=describe_page(3,NULL,NULL); CHECK(browser_schema_from_describe(&describe,&describe,NULL));
    CHECK(describe.column_count==3 && !describe.row_count && !strcmp(describe.columns[2].name,"column_2")); page_free(&describe);
    describe=describe_page(256,NULL,NULL); describe.count_pending=true; CHECK(browser_schema_from_describe(&schema,&describe,NULL));
    CHECK(schema.column_count==256 && !strcmp(schema.columns[255].name,"column_255"));
    page_free(&schema); page_free(&describe);
    describe=describe_page(0,NULL,NULL); CHECK(browser_schema_from_describe(&schema,&describe,NULL));
    CHECK(!schema.columns && !schema.column_count && !schema.cells && !schema.bytes); page_free(&schema); page_free(&describe);
}
static void test_bad_describe(void) {
    Page out=retained(),describe=describe_page(2,NULL,NULL); reject_describe(&out,NULL);
    Page malformed=describe; malformed.column_count=5; reject_describe(&out,&malformed);
    malformed=describe; malformed.row_count=SIZE_MAX; reject_describe(&out,&malformed);
    malformed=describe; malformed.cells=NULL; reject_describe(&out,&malformed);
    malformed=describe; malformed.columns=NULL; reject_describe(&out,&malformed);
    malformed=describe; malformed.offset=1; reject_describe(&out,&malformed);
    malformed=describe; malformed.total=3; reject_describe(&out,&malformed);
    char *name=describe.columns[5].name; describe.columns[5].name="unknown"; reject_describe(&out,&describe); describe.columns[5].name=name;
    name=describe.cells[0].text; describe.cells[0].text=NULL; reject_describe(&out,&describe); describe.cells[0].text=""; reject_describe(&out,&describe); describe.cells[0].text=name;
    describe.cells[0].is_null=true; reject_describe(&out,&describe); describe.cells[0].is_null=false;
    describe.cells[1].is_null=true; reject_describe(&out,&describe); describe.cells[1].is_null=false;
    name=describe.cells[1].text; describe.cells[1].text="DECIMAL(76,1)"; reject_describe(&out,&describe); describe.cells[1].text=name;
    name=describe.cells[6].text; describe.cells[6].text="COLUMN_0"; reject_describe(&out,&describe); describe.cells[6].text=name;
    CHECK(!browser_schema_from_describe(NULL,&describe,NULL)); CHECK(!browser_schema_from_describe(&out,NULL,NULL));
    page_free(&describe); describe=describe_page(257,NULL,NULL); reject_describe(&out,&describe); page_free(&describe); page_free(&out);
}
static void test_page_and_count_sql(void) {
    const char *names[]={"A\"B","NULL","Amount"},*types[]={"HUGEINT","VARCHAR","DECIMAL(38,18)"};
    Page describe=describe_page(3,names,types),schema={0}; char *error=NULL;
    CHECK(browser_schema_from_describe(&schema,&describe,&error));
    const char *relation="SELECT 170141183460469231731687303715884105727::HUGEINT AS \"A\"\"B\", NULL::VARCHAR AS \"NULL\", 1.25::DECIMAL(38,18) AS Amount";
    char *sql=browser_page_sql(relation,&schema,UINT64_C(9007199254740993),&error);
    CHECK(sql && strstr(sql,"SELECT CAST(\"A\"\"B\" AS VARCHAR) AS \"A\"\"B\",CAST(\"NULL\" AS VARCHAR) AS \"NULL\",CAST(\"Amount\" AS VARCHAR) AS \"Amount\" FROM ("));
    CHECK(strstr(sql,relation) && strstr(sql,"LIMIT 256 OFFSET 9007199254740993") && !strstr(sql,"COALESCE") && !error); free(sql);
    sql=browser_page_sql(relation,&schema,UINT64_MAX-256,&error); CHECK(sql && strstr(sql,"OFFSET 18446744073709551359")); free(sql);
    reject_sql(browser_page_sql(relation,&schema,UINT64_MAX-255,&error),&error);
    sql=browser_describe_sql(relation,&error); char *expected=dx_printf("DESCRIBE SELECT * FROM (%s) AS _duxcel_relation",relation);
    CHECK(sql && !strcmp(sql,expected)); free(sql); free(expected);
    sql=browser_count_sql(relation,&error); expected=dx_printf("SELECT COUNT(*)::UBIGINT AS \"__duxcel_count\" FROM (%s) AS _duxcel_relation",relation);
    CHECK(sql && !strcmp(sql,expected)); free(sql); free(expected);
    char *name=schema.columns[2].name; schema.columns[2].name="a\"b"; reject_sql(browser_page_sql(relation,&schema,0,&error),&error); schema.columns[2].name=name;
    Page invalid={0}; reject_sql(browser_page_sql(relation,&invalid,0,&error),&error);
    invalid=schema; invalid.column_count=SIZE_MAX; reject_sql(browser_page_sql(relation,&invalid,0,&error),&error);
    invalid=schema; invalid.columns=NULL; reject_sql(browser_page_sql(relation,&invalid,0,&error),&error);
    const char *bad[]={NULL,""," \t\n"};
    for (size_t i=0;i<3;i++) { reject_sql(browser_describe_sql(bad[i],&error),&error); reject_sql(browser_count_sql(bad[i],&error),&error); reject_sql(browser_page_sql(bad[i],&schema,0,&error),&error); }
    char *large=dx_alloc(DX_BROWSER_SQL_BYTES+2); memset(large,'x',DX_BROWSER_SQL_BYTES+1);
    reject_sql(browser_describe_sql(large,&error),&error); reject_sql(browser_count_sql(large,&error),&error); reject_sql(browser_page_sql(large,&schema,0,&error),&error);
    large[DX_BROWSER_SQL_BYTES]=0; reject_sql(browser_describe_sql(large,&error),&error); reject_sql(browser_count_sql(large,&error),&error); reject_sql(browser_page_sql(large,&schema,0,&error),&error); free(large);
    page_free(&schema); page_free(&describe);
    describe=describe_page(256,NULL,NULL); CHECK(browser_schema_from_describe(&schema,&describe,NULL));
    sql=browser_page_sql("SELECT * FROM input",&schema,0,NULL); CHECK(sql && strstr(sql,"CAST(\"column_255\" AS VARCHAR) AS \"column_255\"")); free(sql);
    for (size_t i=0;i<256;i++) { free(schema.columns[i].name); schema.columns[i].name=dx_alloc(4097); memset(schema.columns[i].name,'x',4096); snprintf(schema.columns[i].name,32,"column_%03zu",i); size_t length=strlen(schema.columns[i].name); memset(schema.columns[i].name+length,'x',4096-length); }
    reject_sql(browser_page_sql("SELECT * FROM input",&schema,0,&error),&error);
    page_free(&schema); page_free(&describe);
}
int main(void) {
    test_relation_sql(); test_invalid_sources(); test_sql_type_parser(); test_describe_conversion(); test_bad_describe(); test_page_and_count_sql();
    printf("PASS: %u browser query checks\n",checks); return 0;
}
