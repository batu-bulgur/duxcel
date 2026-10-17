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
static Page matrix_fixture(const PivotDefinition *pivot,size_t count) {
    size_t rows=pivot->counts[PIVOT_ROWS],measures=pivot->counts[PIVOT_VALUES];
    char *names[2*DX_PIVOT_FIELDS]={0}; const char *types[2*DX_PIVOT_FIELDS]={0};
    for (size_t c=0;c<rows;c++) { names[c]=pivot_field_key(pivot,PIVOT_ROWS,c); types[c]=pivot->fields[PIVOT_ROWS][c].grain ? "DATE" : "VARCHAR"; }
    for (size_t m=0;m<measures;m++) { const PivotField *field=&pivot->fields[PIVOT_VALUES][m]; names[rows+m]=aggregate_measure_label(field->aggregate,field->name); types[rows+m]=field->aggregate==AGG_AVG ? "DOUBLE" : field->aggregate==AGG_DISTINCT || field->aggregate==AGG_COUNT ? "BIGINT" : !strcmp(field->name,"Text") ? "VARCHAR" : "DECIMAL(38,2)"; }
    Page describe=describe_page(rows+measures,(const char *const *)names,types),page={0}; CHECK(browser_schema_from_describe(&page,&describe,NULL)); page_free(&describe);
    for (size_t c=0;c<rows+measures;c++) free(names[c]);
    page.row_count=count; page.total=count; page.cells=dx_alloc(count*page.column_count*sizeof(Cell)); page.bytes+=count*page.column_count*sizeof(Cell);
    for (size_t i=0;i<count*page.column_count;i++) { page.cells[i].text=dx_strdup("1"); page.bytes+=2; }
    return page;
}
static void matrix_cell(Page *page,size_t row,size_t col,const char *text) {
    Cell *cell=&page->cells[row*page->column_count+col]; if (cell->text) page->bytes-=strlen(cell->text)+1; free(cell->text);
    *cell=(Cell){.is_null=!text,.text=text ? dx_strdup(text) : NULL}; if (text) page->bytes+=strlen(text)+1;
}
static Page matrix_grand(const Page *page,const PivotDefinition *pivot) {
    Page grand={0}; CHECK(browser_pivot_totals_schema(&grand,page,pivot,NULL)); grand.row_count=grand.total=1;
    grand.cells=dx_alloc(grand.column_count*sizeof(Cell)); grand.bytes+=grand.column_count*sizeof(Cell);
    for (size_t i=0;i<grand.column_count;i++) { grand.cells[i].text=dx_strdup("1"); grand.bytes+=2; } return grand;
}
static void reject_matrix(Page *page,Page *grand,const PivotDefinition *pivot) {
    Page before=*page,total_before=*grand; char *error=NULL;
    CHECK(!browser_pivot_complete(page,grand,pivot,&error) && error && *error);
    CHECK(!memcmp(page,&before,sizeof(before)) && !memcmp(grand,&total_before,sizeof(total_before))); free(error);
}
static void test_matrix_completion(void) {
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("O'Reilly.csv")}; PivotDefinition pivot={0}; char *error=NULL;
    CHECK(pivot_assign_grain(&pivot,PIVOT_ROWS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_AVG,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"ID",AGG_DISTINCT,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Text",AGG_MIN,&error));
    char *sql=browser_pivot_totals_sql(&source,&pivot,&error);
    CHECK(sql && strstr(sql,"GROUP BY GROUPING SETS") && strstr(sql,"WHERE mask=1 LIMIT 1") && strstr(sql,"COUNT(DISTINCT v2)") && strstr(sql,"CAST(m0 AS VARCHAR) AS total_0") && strstr(sql,"O''Reilly.csv")); free(sql);
    Page page=matrix_fixture(&pivot,2); page.offset=256; page.total=600; page.sql=dx_strdup("SELECT * FROM a_real_pivot"); page.bytes+=strlen(page.sql)+1;
    matrix_cell(&page,0,0,"2025-01-01"); matrix_cell(&page,1,0,NULL);
    matrix_cell(&page,0,1,"1234567890123456789012345678.12"); matrix_cell(&page,1,1,NULL);
    matrix_cell(&page,0,2,"1.5"); matrix_cell(&page,1,2,NULL); matrix_cell(&page,0,3,"9007199254740993"); matrix_cell(&page,1,3,"0"); matrix_cell(&page,0,4,""); matrix_cell(&page,1,4,"NULL");
    Page grand=matrix_grand(&page,&pivot); matrix_cell(&grand,0,0,"1234567890123456789012345678.12"); matrix_cell(&grand,0,1,"1.1"); matrix_cell(&grand,0,2,"9007199254740994"); matrix_cell(&grand,0,3,"");
    char *grand_text=grand.cells[1].text,*original=page.cells[1].text; size_t original_bytes=page.bytes;
    CHECK(browser_pivot_complete(&page,&grand,&pivot,&error) && !error && !grand.cells && !grand.columns && !grand.bytes);
    CHECK(page.totals && !page.totals->has_columns && page.totals->rows.row_count==2 && page.totals->rows.column_count==4 && page.totals->rows.offset==256 && page.totals->rows.total==600);
    CHECK(page.totals->rows.cells[0].text!=original && !strcmp(page.totals->rows.cells[0].text,original) && page.totals->rows.cells[4].is_null && !page.totals->rows.cells[4].text);
    CHECK(!strcmp(page.totals->rows.cells[2].text,"9007199254740993") && !*page.totals->rows.cells[3].text && !strcmp(page.totals->rows.cells[7].text,"NULL"));
    CHECK(page.totals->columns.cells[1].text==grand_text && !strcmp(grand_text,"1.1") && !strcmp(page.totals->columns.cells[2].text,"9007199254740994"));
    CHECK(page.columns[0].date_grain==PIVOT_DATE_MONTH && !strcmp(page.columns[0].name,"MONTH(Date)") && !strcmp(page.columns[1].caption,"SUM(Amount)"));
    CHECK(page.bytes>original_bytes && page.bytes<=DX_PAGE_WIRE_TEXT_BYTES); page_free(&page); page_free(&grand);
    page=matrix_fixture(&pivot,1); grand=matrix_grand(&page,&pivot);
    Page malformed=grand; malformed.total=2; reject_matrix(&page,&malformed,&pivot);
    malformed=grand; malformed.row_count=0; reject_matrix(&page,&malformed,&pivot);
    malformed=page; malformed.count_pending=true; reject_matrix(&malformed,&grand,&pivot);
    malformed=page; malformed.bytes=SIZE_MAX; reject_matrix(&malformed,&grand,&pivot);
    malformed=page; malformed.bytes=0; reject_matrix(&malformed,&grand,&pivot);
    malformed=page; malformed.bytes=DX_PAGE_WIRE_TEXT_BYTES-1; reject_matrix(&malformed,&grand,&pivot);
    char *name=grand.columns[0].name; grand.columns[0].name="wrong"; reject_matrix(&page,&grand,&pivot); grand.columns[0].name=name;
    NumberType number=grand.columns[0].number_type; grand.columns[0].number_type=(NumberType){NUM_FLOAT,0,0}; reject_matrix(&page,&grand,&pivot); grand.columns[0].number_type=number;
    PivotDefinition invalid=pivot; invalid.counts[PIVOT_VALUES]=SIZE_MAX; reject_matrix(&page,&grand,&invalid);
    invalid=pivot; invalid.counts[PIVOT_COLUMNS]=1; reject_matrix(&page,&grand,&invalid); reject_sql(browser_pivot_totals_sql(&source,&invalid,&error),&error);
    Page schema=retained(),before=schema; CHECK(!browser_pivot_totals_schema(&schema,&page,&invalid,&error) && error && !memcmp(&schema,&before,sizeof(before))); free(error); error=NULL; page_free(&schema);
    page_free(&page); page_free(&grand); pivot_free(&pivot); analysis_free(&source);
    /* An aggregate without Rows still has one typed row and one source grand
       row. NULL measures are retained rather than changed to numeric zero. */
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); source.kind=SOURCE_DEMO;
    sql=browser_pivot_totals_sql(&source,&pivot,&error); CHECK(sql && strstr(sql,"WHERE mask=0 LIMIT 1") && !strstr(sql,"GROUP BY GROUPING SETS")); free(sql);
    page=matrix_fixture(&pivot,1); grand=matrix_grand(&page,&pivot); matrix_cell(&page,0,0,NULL); matrix_cell(&grand,0,0,NULL);
    CHECK(browser_pivot_complete(&page,&grand,&pivot,&error) && page.totals->rows.cells[0].is_null && page.totals->columns.cells[0].is_null); page_free(&page); pivot_free(&pivot); free(error);
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,NULL)); page=matrix_fixture(&pivot,1);
    CHECK(browser_pivot_complete(&page,NULL,&pivot,NULL) && !page.totals && !strcmp(page.columns[0].caption,"SUM(Amount)")); page_free(&page); pivot_free(&pivot);
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Text",AGG_MIN,NULL)); page=matrix_fixture(&pivot,1); grand=matrix_grand(&page,&pivot);
    CHECK(browser_pivot_complete(&page,&grand,&pivot,NULL)); size_t remaining=DX_PAGE_WIRE_TEXT_BYTES-page.bytes; page_free(&page);
    size_t length=1+remaining/2; char *large=dx_alloc(length+1); memset(large,'x',length); large[length]=0;
    page=matrix_fixture(&pivot,1); grand=matrix_grand(&page,&pivot); matrix_cell(&page,0,0,large); matrix_cell(&grand,0,0,remaining%2 ? "11" : "1");
    CHECK(browser_pivot_complete(&page,&grand,&pivot,NULL) && page.bytes==DX_PAGE_WIRE_TEXT_BYTES); page_free(&page);
    page=matrix_fixture(&pivot,1); grand=matrix_grand(&page,&pivot); matrix_cell(&page,0,0,large); matrix_cell(&grand,0,0,remaining%2 ? "111" : "11");
    reject_matrix(&page,&grand,&pivot); page_free(&page); page_free(&grand); free(large); pivot_free(&pivot);
    /* A rejected owned job can still contain all eight allocated shelf slots.
       Destruction must not walk its malformed counts outside those arrays. */
    for (size_t role=0;role<4;role++) {
        pivot.counts[role]=role%2 ? SIZE_MAX : DX_PIVOT_FIELDS+1;
        for (size_t field=0;field<DX_PIVOT_FIELDS;field++) { pivot.fields[role][field].name=dx_strdup("Owned field"); pivot.fields[role][field].value=dx_strdup("Owned filter value"); }
    }
    pivot_free(&pivot); PivotDefinition empty={0}; CHECK(!memcmp(&pivot,&empty,sizeof(pivot))); pivot_free(&pivot); CHECK(!memcmp(&pivot,&empty,sizeof(pivot)));
}
static Page domain_fixture(size_t count,const char *const *values) {
    const char *names[]={"category"},*types[]={"VARCHAR"}; Page describe=describe_page(1,names,types),page={0};
    CHECK(browser_schema_from_describe(&page,&describe,NULL)); page_free(&describe);
    page.row_count=page.total=count; if (count) page.cells=dx_alloc(count*sizeof(Cell)); page.bytes+=count*sizeof(Cell);
    for (size_t i=0;i<count;i++) { char value[32]; snprintf(value,sizeof(value),"v%03zu",i); const char *text=values ? values[i] : value; page.cells[i]=(Cell){.is_null=!text,.text=text ? dx_strdup(text) : NULL}; if (text) page.bytes+=strlen(text)+1; }
    return page;
}
static Page planned_fixture(const PivotDefinition *p,const BrowserPivotPlan *plan,size_t rows) {
    const char *names[128]={0},*types[128]={0}; char *keys[DX_PIVOT_FIELDS]={0}; size_t count=p->counts[PIVOT_ROWS]+plan->count;
    for (size_t i=0;i<p->counts[PIVOT_ROWS];i++) { keys[i]=pivot_field_key(p,PIVOT_ROWS,i); names[i]=keys[i]; types[i]="VARCHAR"; }
    for (size_t c=0;c<plan->count;c++) { names[p->counts[PIVOT_ROWS]+c]=plan->headers[c].name; const PivotField *m=&p->fields[PIVOT_VALUES][c%p->counts[PIVOT_VALUES]]; types[p->counts[PIVOT_ROWS]+c]=m->aggregate==AGG_AVG ? "DOUBLE" : m->aggregate==AGG_DISTINCT || m->aggregate==AGG_COUNT ? "BIGINT" : !strcmp(m->name,"Text") ? "VARCHAR" : "DECIMAL(38,2)"; }
    Page describe=describe_page(count,names,types),page={0}; CHECK(browser_schema_from_describe(&page,&describe,NULL)); page_free(&describe);
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) free(keys[i]);
    page.row_count=page.total=rows; if (rows) page.cells=dx_alloc(rows*count*sizeof(Cell)); page.bytes+=rows*count*sizeof(Cell);
    for (size_t i=0;i<rows*count;i++) { page.cells[i].text=dx_strdup("1"); page.bytes+=2; } return page;
}
static Page planned_readout(const Page *body,const PivotDefinition *p,const BrowserPivotPlan *plan,bool rows) {
    Page page={0}; CHECK(browser_pivot_readout_schema(&page,body,p,plan,rows,NULL)); page.row_count=rows ? body->row_count : 1; page.total=rows ? body->total : 1; page.offset=rows ? body->offset : 0;
    if (page.row_count) page.cells=dx_alloc(page.row_count*page.column_count*sizeof(Cell)); page.bytes+=page.row_count*page.column_count*sizeof(Cell);
    for (size_t i=0;i<page.row_count*page.column_count;i++) { page.cells[i].text=dx_strdup("1"); page.bytes+=2; } return page;
}
static void reject_plan(BrowserPivotPlan *out,const Analysis *source,const PivotDefinition *p,const Page *domains) {
    BrowserPivotPlan before=*out; char *error=NULL; CHECK(!browser_pivot_plan_build(out,source,p,domains,&error) && error && *error); CHECK(!memcmp(out,&before,sizeof(before))); free(error);
}
static void reject_planned_page(Page *body,Page *rows,Page *columns,const PivotDefinition *p,const BrowserPivotPlan *plan) {
    Page b=*body,r=rows ? *rows : (Page){0},c=columns ? *columns : (Page){0}; char *error=NULL;
    CHECK(!browser_pivot_plan_complete(body,rows,columns,p,plan,&error) && error && *error);
    CHECK(!memcmp(body,&b,sizeof(b)) && (!rows || !memcmp(rows,&r,sizeof(r))) && (!columns || !memcmp(columns,&c,sizeof(c)))); free(error);
}
static void test_column_plans(void) {
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("O'Reilly.csv")}; PivotDefinition p={0}; BrowserPivotPlan plan={0}; Page domains[DX_PIVOT_FIELDS]={0}; char *error=NULL;
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL)); CHECK(pivot_assign(&p,PIVOT_COLUMNS,"Category",AGG_SUM,NULL));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_AVG,NULL) && pivot_assign(&p,PIVOT_VALUES,"ID",AGG_DISTINCT,NULL));
    const char *categories[]={NULL,"","NULL","FOO","foo","quote_\"'_SUM"}; domains[0]=domain_fixture(6,categories);
    char *sql=browser_pivot_domain_sql(&source,&p,0,&error);
    CHECK(sql && strstr(sql,"CAST(_duxcel_domain_value AS VARCHAR) AS category") && strstr(sql,"SELECT DISTINCT \"Category\" AS _duxcel_domain_value") && strstr(sql,"ORDER BY _duxcel_domain_value NULLS FIRST LIMIT 129") && strstr(sql,"O''Reilly.csv")); free(sql);
    CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,&error) && !error && plan.count==12 && !plan.empty_input && plan.bytes<=DX_PAGE_WIRE_TEXT_BYTES);
    CHECK(strstr(plan.sql,"PIVOT (") && strstr(plan.sql,"NULL AS \"category_0\"") && strstr(plan.sql,"'' AS \"category_1\"") && strstr(plan.sql,"'NULL' AS \"category_2\"") && strstr(plan.sql,"'quote_\"''_SUM' AS \"category_5\""));
    CHECK(plan.headers[0].category_count==1 && !plan.headers[0].categories[0] && !strcmp(plan.headers[0].caption,"NULL"));
    CHECK(plan.headers[2].categories[0] && !*plan.headers[2].categories[0] && !*plan.headers[2].caption && !strcmp(plan.headers[4].categories[0],"NULL"));
    CHECK(!strcmp(plan.headers[8].categories[0],"foo") && !strcmp(plan.headers[11].detail,"COUNT DISTINCT(ID)") && strstr(plan.sql,"__duxcel_pivot_value_11_0"));
    sql=browser_pivot_readout_sql(&source,&p,&plan,true,256,&error);
    CHECK(sql && strstr(sql,"GROUP BY GROUPING SETS") && strstr(sql,"WHERE mask=1 ORDER BY d0 NULLS FIRST LIMIT 256 OFFSET 256") && strstr(sql,"COUNT(DISTINCT v1)")); free(sql);
    sql=browser_pivot_readout_sql(&source,&p,&plan,false,0,&error);
    CHECK(sql && strstr(sql,"mask=2 AND d1 IS NOT DISTINCT FROM NULL") && strstr(sql,"d1 IS NOT DISTINCT FROM ''") && strstr(sql,"d1 IS NOT DISTINCT FROM 'NULL'") && strstr(sql,"mask=3") && strstr(sql,"AS total_13") && strstr(sql,"CAST(coalesce(max(m1)")); free(sql);
    Page body=planned_fixture(&p,&plan,2); body.offset=256; body.total=600;
    Page rows=planned_readout(&body,&p,&plan,true),columns=planned_readout(&body,&p,&plan,false);
    matrix_cell(&body,0,1,"15"); matrix_cell(&body,0,3,"30"); matrix_cell(&rows,0,0,"20"); matrix_cell(&rows,0,1,"2"); matrix_cell(&columns,0,12,"25");
    char *row_text=rows.cells[0].text,*footer_text=columns.cells[12].text;
    CHECK(browser_pivot_plan_complete(&body,&rows,&columns,&p,&plan,&error) && !rows.cells && !columns.cells);
    CHECK(body.totals && body.totals->has_columns && body.totals->rows.cells[0].text==row_text && !strcmp(row_text,"20") && body.totals->columns.cells[12].text==footer_text && !strcmp(footer_text,"25"));
    CHECK(!body.columns[1].categories[0] && body.columns[3].categories[0] && !*body.columns[3].categories[0] && body.columns[5].categories[0] && !strcmp(body.columns[5].categories[0],"NULL"));
    CHECK(body.columns[3].categories[0]!=plan.headers[2].categories[0] && body.bytes<=DX_PAGE_WIRE_TEXT_BYTES); page_free(&body);
    body=planned_fixture(&p,&plan,1); rows=planned_readout(&body,&p,&plan,true); columns=planned_readout(&body,&p,&plan,false);
    Page invalid=rows; invalid.total=5; reject_planned_page(&body,&invalid,&columns,&p,&plan);
    invalid=columns; invalid.column_count--; reject_planned_page(&body,&rows,&invalid,&p,&plan);
    invalid=body; invalid.count_pending=true; reject_planned_page(&invalid,&rows,&columns,&p,&plan);
    invalid=body; invalid.bytes=DX_PAGE_WIRE_TEXT_BYTES-1; reject_planned_page(&invalid,&rows,&columns,&p,&plan);
    reject_planned_page(&body,&rows,NULL,&p,&plan);
    char *saved=columns.columns[0].type_name; columns.columns[0].type_name="VARCHAR"; reject_planned_page(&body,&rows,&columns,&p,&plan); columns.columns[0].type_name=saved;
    CHECK(browser_pivot_plan_complete(&body,NULL,NULL,&p,&plan,NULL) && !body.totals && body.columns[1].category_count==1); page_free(&body); page_free(&rows); page_free(&columns);
    Page malformed=domains[0]; malformed.row_count=129; reject_plan(&plan,&source,&p,&malformed);
    malformed=domains[0]; malformed.bytes=0; reject_plan(&plan,&source,&p,&malformed);
    matrix_cell(&domains[0],4,0,"FOO"); reject_plan(&plan,&source,&p,domains); matrix_cell(&domains[0],4,0,"foo");
    PivotDefinition invalid_p=p; invalid_p.counts[PIVOT_COLUMNS]=SIZE_MAX; reject_plan(&plan,&source,&invalid_p,domains);
    reject_sql(browser_pivot_domain_sql(&source,&p,SIZE_MAX,&error),&error); reject_sql(browser_pivot_readout_sql(&source,&p,&plan,true,UINT64_MAX-255,&error),&error);
    page_free(&domains[0]); browser_pivot_plan_free(&plan); pivot_free(&p);
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign_grain(&p,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign(&p,PIVOT_COLUMNS,"Category",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    const char *dates[]={NULL,"2026-01-01"},*labels[]={"","NULL"}; domains[0]=domain_fixture(2,dates); domains[1]=domain_fixture(2,labels);
    CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,NULL) && plan.count==4 && plan.headers[2].category_count==2 && plan.headers[2].category_grains[0]==PIVOT_DATE_MONTH && !strcmp(plan.headers[2].caption,"Jan 2026 · ") && !strcmp(plan.headers[3].categories[1],"NULL"));
    sql=browser_pivot_domain_sql(&source,&p,0,NULL); CHECK(sql && strstr(sql,"date_trunc('month'") && strstr(sql,"__duxcel_date_1_0_0")); free(sql);
    sql=browser_pivot_readout_sql(&source,&p,&plan,false,0,NULL); CHECK(sql && strstr(sql,"mask=4") && strstr(sql,"mask=7") && strstr(sql,"d1 IS NOT DISTINCT FROM '2026-01-01' AND d2 IS NOT DISTINCT FROM ''")); free(sql);
    page_free(&domains[0]); page_free(&domains[1]); browser_pivot_plan_free(&plan); pivot_free(&p);
    CHECK(pivot_assign(&p,PIVOT_COLUMNS,"Category",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL));
    domains[0]=domain_fixture(128,NULL); CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,NULL) && plan.count==128); page_free(&domains[0]);
    domains[0]=domain_fixture(129,NULL); reject_plan(&plan,&source,&p,domains); page_free(&domains[0]);
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL)); domains[0]=domain_fixture(128,NULL); reject_plan(&plan,&source,&p,domains); page_free(&domains[0]);
    domains[0]=domain_fixture(127,NULL); CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,NULL) && plan.count==127); page_free(&domains[0]);
    CHECK(pivot_assign(&p,PIVOT_COLUMNS,"Second",AGG_SUM,NULL)); domains[0]=domain_fixture(16,NULL); domains[1]=domain_fixture(8,NULL); reject_plan(&plan,&source,&p,domains); page_free(&domains[0]); page_free(&domains[1]); browser_pivot_plan_free(&plan); pivot_free(&p);
    CHECK(pivot_assign(&p,PIVOT_ROWS,"__DUXCEL_PIVOT_VALUE_0_0",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_COLUMNS,"Category",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Text",AGG_MIN,NULL));
    domains[0]=domain_fixture(0,NULL); CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,NULL) && plan.empty_input && plan.count==1 && !plan.headers[0].category_count && !strcmp(plan.headers[0].caption,"MIN(Text)") && !strcmp(plan.headers[0].name,"__duxcel_pivot_value_0_1"));
    body=planned_fixture(&p,&plan,0); rows=planned_readout(&body,&p,&plan,true); columns=planned_readout(&body,&p,&plan,false); matrix_cell(&columns,0,0,NULL);
    CHECK(browser_pivot_plan_complete(&body,&rows,&columns,&p,&plan,NULL) && !body.row_count && !body.totals->has_columns && body.totals->columns.cells[0].is_null); page_free(&body); page_free(&domains[0]); browser_pivot_plan_free(&plan);
    const char *one[]={"x"}; domains[0]=domain_fixture(1,one); CHECK(browser_pivot_plan_build(&plan,&source,&p,domains,NULL));
    body=planned_fixture(&p,&plan,1); rows=planned_readout(&body,&p,&plan,true); columns=planned_readout(&body,&p,&plan,false); CHECK(browser_pivot_plan_complete(&body,&rows,&columns,&p,&plan,NULL)); size_t extra=DX_PAGE_WIRE_TEXT_BYTES-body.bytes; page_free(&body);
    char *large=dx_alloc(extra+3); memset(large,'x',extra+2); large[extra+1]=0;
    body=planned_fixture(&p,&plan,1); rows=planned_readout(&body,&p,&plan,true); columns=planned_readout(&body,&p,&plan,false); matrix_cell(&body,0,1,large);
    CHECK(browser_pivot_plan_complete(&body,&rows,&columns,&p,&plan,NULL) && body.bytes==DX_PAGE_WIRE_TEXT_BYTES); page_free(&body);
    large[extra+1]='x'; large[extra+2]=0; body=planned_fixture(&p,&plan,1); rows=planned_readout(&body,&p,&plan,true); columns=planned_readout(&body,&p,&plan,false); matrix_cell(&body,0,1,large);
    reject_planned_page(&body,&rows,&columns,&p,&plan); page_free(&body); page_free(&rows); page_free(&columns); free(large);
    page_free(&domains[0]); browser_pivot_plan_free(&plan); browser_pivot_plan_free(&plan); pivot_free(&p); analysis_free(&source); free(error);
}
static void test_count_projection(void) {
    Analysis source={.kind=SOURCE_DEMO}; PivotDefinition p={0};
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL));
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,NULL));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"occurred",AGG_COUNT,NULL));
    char *input=pivot_input_sql(&source,&p); CHECK(strstr(input,"date_trunc('month'") && strstr(input,"quarter(\"occurred\")") && !strstr(input,",\"occurred\" FROM")); free(input);
    char *sql=pivot_sql(&source,&p); CHECK(strstr(sql,"PIVOT (") && strstr(sql,"COUNT(*)") && !strstr(sql,",\"occurred\" FROM")); free(sql);
    CHECK(pivot_assign(&p,PIVOT_VALUES,"occurred",AGG_MAX,NULL));
    input=pivot_input_sql(&source,&p); CHECK(strstr(input,",\"occurred\" FROM")); free(input); pivot_free(&p);
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Nullable",AGG_COUNT,NULL));
    input=pivot_input_sql(&source,&p); CHECK(!strstr(input,"\"Nullable\"")); free(input);
    sql=pivot_sql(&source,&p); CHECK(strstr(sql,"SELECT ") && strstr(sql,"COUNT(*)") && !strstr(sql,"PIVOT (") && !strstr(sql,"GROUP BY")); free(sql);
    sql=pivot_totals_sql(&source,&p); CHECK(strstr(sql,"1 AS v0") && !strstr(sql,"\"Nullable\" AS v0")); free(sql);
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Nullable",AGG_SUM,NULL));
    input=pivot_input_sql(&source,&p); CHECK(strstr(input,"SELECT \"Nullable\" FROM")); free(input);
    sql=pivot_sql(&source,&p); CHECK(strstr(sql,"COUNT(*)") && strstr(sql,"SUM(\"Nullable\")") && !strstr(sql,"PIVOT (")); free(sql); pivot_free(&p);
}
int main(void) {
    test_relation_sql(); test_invalid_sources(); test_sql_type_parser(); test_describe_conversion(); test_bad_describe(); test_page_and_count_sql(); test_matrix_completion(); test_column_plans(); test_count_projection();
    printf("PASS: %u browser query checks\n",checks); return 0;
}
