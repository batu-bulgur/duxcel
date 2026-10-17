#define _DARWIN_C_SOURCE
#include "transforms/record_sql.h"
#include <duckdb.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL record SQL:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static const char *const fields[]={"big\"id","tie key","exact DECIMAL","nullable","Money","__duxcel_record_id","__DUXCEL_RECORD_ID_1","line\nname"};
#define FIELDS (sizeof(fields)/sizeof(*fields))
static duckdb_result execute(duckdb_connection db,const char *sql) {
    duckdb_result r={0}; if (duckdb_query(db,sql,&r)!=DuckDBSuccess) { fprintf(stderr,"DuckDB: %s\nSQL: %s\n",duckdb_result_error(&r),sql); exit(1); } return r;
}
static void command(duckdb_connection db,const char *sql) { duckdb_result r=execute(db,sql); duckdb_destroy_result(&r); }
static void text(duckdb_result *r,size_t c,size_t row,const char *expected) {
    CHECK(!duckdb_value_is_null(r,c,row)); char *value=duckdb_value_varchar(r,c,row); CHECK(value && !strcmp(value,expected)); duckdb_free(value);
}
static void reject(RecordSQL *out,const Analysis *source,const char *const *names,size_t count,bool file,RecordSQLStatus status) {
    RecordSQL old=*out; char *error=NULL;
    CHECK(record_sql_build(out,source,names,count,file,false,&error)==status);
    CHECK(error && *error && out->relation==old.relation && out->identity_name==old.identity_name && out->business_columns==old.business_columns && out->identity_column==old.identity_column && out->kind==old.kind);
    CHECK(!strcmp(out->relation,"owned retained relation") && !strcmp(out->identity_name,"owned retained alias")); free(error);
}
static void test_bounds_and_ownership(void) {
    Analysis source={.kind=SOURCE_PARQUET,.path="private.parquet"}; const char *const simple[]={"Name"}; char *error=NULL;
    RecordSQL out={dx_strdup("owned retained relation"),dx_strdup("owned retained alias"),1,1,RECORD_ID_PARQUET};
    reject(&out,NULL,simple,1,true,RECORD_SQL_INVALID);
    reject(&out,&source,NULL,1,true,RECORD_SQL_INVALID); reject(&out,&source,simple,0,true,RECORD_SQL_INVALID);
    reject(&out,&source,simple,SIZE_MAX,true,RECORD_SQL_INVALID); reject(&out,&source,simple,DX_RECORD_SQL_COLUMNS+1,true,RECORD_SQL_INVALID);
    const char *const bad[]={NULL},*const empty[]={""},*const duplicate[]={"City","CITY"},*const invalid_utf8[]={"\xed\xa0\x80"};
    reject(&out,&source,bad,1,true,RECORD_SQL_INVALID); reject(&out,&source,empty,1,true,RECORD_SQL_INVALID);
    reject(&out,&source,duplicate,2,true,RECORD_SQL_INVALID); reject(&out,&source,invalid_utf8,1,true,RECORD_SQL_INVALID);
    const char *invalid[]={"\xc0\x80","\xf4\x90\x80\x80","\xe2\x82","\x80"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) reject(&out,&source,invalid+i,1,true,RECORD_SQL_INVALID);
    const char *const metadata[]={"FiLe_RoW_NuMbEr"}; reject(&out,&source,metadata,1,true,RECORD_SQL_UNSUPPORTED);
    reject(&out,&source,simple,1,false,RECORD_SQL_UNSUPPORTED);
    for (int kind=SOURCE_NONE;kind<=SOURCE_DEMO;kind+=3) { source.kind=(SourceKind)kind; reject(&out,&source,simple,1,true,RECORD_SQL_UNSUPPORTED); }
    source.kind=(SourceKind)99; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.kind=SOURCE_PARQUET;
    source.path=NULL; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.path="private.parquet";
    source.query="SELECT * FROM input"; reject(&out,&source,simple,1,true,RECORD_SQL_UNSUPPORTED); source.query=NULL;
    source.unique_rows=true; reject(&out,&source,simple,1,true,RECORD_SQL_UNSUPPORTED); source.unique_rows=false;
    source.group_column="Name"; reject(&out,&source,simple,1,true,RECORD_SQL_UNSUPPORTED); source.group_column=NULL;
    source.measure_column="Name"; reject(&out,&source,simple,1,true,RECORD_SQL_UNSUPPORTED); source.measure_column=NULL;
    source.filter_count=SIZE_MAX; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.filter_count=1;
    reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.filter_count=0; source.sort_count=SIZE_MAX;
    reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.sort_count=0;
    source.filter_column="Missing"; source.filter_op=FILTER_EQUAL; source.filter_value="x"; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID);
    source.filter_column="Name"; source.filter_op=(FilterOp)99; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID);
    source.filter_op=FILTER_BETWEEN; source.filter_value="[\"one\"]"; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID);
    source.filter_op=FILTER_EQUAL; source.filter_value="1"; source.filter_number=(NumberType){NUM_DECIMAL,0,0}; reject(&out,&source,simple,1,true,RECORD_SQL_INVALID);
    source.filter_number=(NumberType){0}; source.filter_column=NULL; source.filter_value=NULL; source.sort_column="Missing";
    reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.sort_column="Name"; source.sort_count=1; source.sorts[0]=(AnalysisSort){"NAME",false};
    reject(&out,&source,simple,1,true,RECORD_SQL_INVALID); source.sort_column=NULL; source.sort_count=0;
    char long_name[DX_RECORD_SQL_NAME_BYTES+2]; memset(long_name,'x',sizeof(long_name)-1); long_name[sizeof(long_name)-1]=0; const char *names[]={long_name};
    reject(&out,&source,names,1,true,RECORD_SQL_UNSUPPORTED); long_name[DX_RECORD_SQL_NAME_BYTES]=0;
    CHECK(record_sql_build(&out,&source,names,1,true,false,&error)==RECORD_SQL_READY); CHECK(strlen(out.relation)<=DX_RECORD_SQL_BYTES); record_sql_free(&out);
    size_t length=DX_RECORD_SQL_INPUT_BYTES-11; char *threshold=dx_alloc(length+2); memset(threshold,'x',length); const char *const note[]={"Note"};
    source=(Analysis){.kind=SOURCE_DEMO,.filter_column="Note",.filter_value=threshold,.filter_op=FILTER_CONTAINS};
    CHECK(record_sql_build(&out,&source,note,1,false,false,&error)==RECORD_SQL_READY); CHECK(strlen(out.relation)<=DX_RECORD_SQL_BYTES);
    RecordSQL before=out; threshold[length]='x'; threshold[length+1]=0;
    CHECK(record_sql_build(&out,&source,note,1,false,false,&error)==RECORD_SQL_UNSUPPORTED && out.relation==before.relation && out.identity_name==before.identity_name); free(threshold); record_sql_free(&out);
    char (*wide_names)[5]=dx_alloc(DX_RECORD_SQL_COLUMNS*sizeof(*wide_names)); const char **wide=dx_alloc(DX_RECORD_SQL_COLUMNS*sizeof(*wide));
    for (size_t i=0;i<DX_RECORD_SQL_COLUMNS;i++) { snprintf(wide_names[i],5,"%04zx",i); wide[i]=wide_names[i]; }
    source=(Analysis){.kind=SOURCE_DEMO}; CHECK(record_sql_build(&out,&source,wide,DX_RECORD_SQL_COLUMNS,false,false,&error)==RECORD_SQL_UNSUPPORTED && !out.relation); free(wide); free(wide_names);
    char *owned_name=dx_strdup("odd \"field 🦆"),*owned_path=dx_strdup("O'Reilly.parquet"); const char *owned_names[]={owned_name};
    source=(Analysis){.kind=SOURCE_PARQUET,.path=owned_path}; CHECK(record_sql_build(&out,&source,owned_names,1,true,false,&error)==RECORD_SQL_READY);
    CHECK(out.business_columns==1 && out.identity_column==1 && out.kind==RECORD_ID_PARQUET && strstr(out.relation,"O''Reilly.parquet") && strstr(out.relation,"odd \"\"field 🦆"));
    memset(owned_name,'z',strlen(owned_name)); memset(owned_path,'z',strlen(owned_path)); free(owned_name); free(owned_path);
    CHECK(strstr(out.relation,"O''Reilly.parquet") && strstr(out.relation,"odd \"\"field 🦆")); record_sql_free(&out); record_sql_free(&out); record_sql_free(NULL);
    CHECK(!out.relation && !out.identity_name && !out.business_columns); CHECK(record_sql_build(NULL,&source,simple,1,true,false,NULL)==RECORD_SQL_INVALID); free(error);
}
static void test_normal_sql_contract(void) {
    Analysis source={.kind=SOURCE_CSV,.path="O'Reilly.csv"}; char *sql=analysis_sql(&source,false);
    CHECK(!strcmp(sql,"SELECT * FROM read_csv_auto('O''Reilly.csv',allow_quoted_nulls=false)")); free(sql);
    source.kind=SOURCE_PARQUET; sql=analysis_sql(&source,true); CHECK(!strcmp(sql,"SELECT * FROM read_parquet('O''Reilly.csv')")); free(sql);
    source=(Analysis){.kind=SOURCE_DATABASE,.schema="a\"b",.table="Odd table"}; sql=analysis_sql(&source,false);
    CHECK(!strcmp(sql,"SELECT * FROM \"source_db\".\"a\"\"b\".\"Odd table\"")); free(sql);
    source=(Analysis){.kind=SOURCE_CSV,.path="x.csv",.filter_column="A\"B",.filter_value="O'Reilly",.filter_op=FILTER_EQUAL,.sort_column="A\"B",.descending=true};
    sql=analysis_sql(&source,true); CHECK(!strcmp(sql,"SELECT * FROM (\nSELECT * FROM read_csv_auto('x.csv',allow_quoted_nulls=false)\n) AS filtered\nWHERE \"A\"\"B\" = cast_to_type($__duxcel_filter, \"A\"\"B\")\nORDER BY \"A\"\"B\" DESC NULLS LAST")); free(sql);
    source=(Analysis){.kind=SOURCE_DEMO}; char *owned=dx_strdup("SELECT 'owned input'"),*same=analysis_wrap_sql(&source,false,owned); CHECK(same==owned); free(same);
}
static void check_page(duckdb_connection db,const Analysis *source,const RecordSQL *record,size_t offset,size_t expected,bool exact) {
    char *plain=analysis_sql(source,false),*sql=dx_printf("SELECT * FROM (%s) q LIMIT 256 OFFSET %zu",record->relation,offset);
    duckdb_result r=execute(db,sql); free(sql); sql=dx_printf("SELECT * FROM (%s) q LIMIT 256 OFFSET %zu",plain,offset); duckdb_result baseline=execute(db,sql); free(sql); free(plain);
    CHECK(duckdb_row_count(&r)==expected && duckdb_row_count(&baseline)==expected && duckdb_column_count(&r)==FIELDS+1 && duckdb_column_count(&baseline)==FIELDS);
    CHECK(record->business_columns==FIELDS && record->identity_column==FIELDS && !strcmp(record->identity_name,"__duxcel_record_id_2"));
    CHECK(duckdb_column_type(&r,FIELDS)==DUCKDB_TYPE_UBIGINT && !strcmp(duckdb_column_name(&r,FIELDS),record->identity_name));
    for (size_t c=0;c<FIELDS;c++) CHECK(!strcmp(duckdb_column_name(&r,c),fields[c]) && duckdb_column_type(&r,c)==duckdb_column_type(&baseline,c));
    for (size_t row=0;row<expected;row++) {
        CHECK(duckdb_value_uint64(&r,FIELDS,row)==offset+row+1);
        for (size_t c=0;c<FIELDS;c++) {
            bool missing=duckdb_value_is_null(&baseline,c,row); CHECK(duckdb_value_is_null(&r,c,row)==missing);
            if (!missing) { char *value=duckdb_value_varchar(&baseline,c,row); text(&r,c,row,value); duckdb_free(value); }
        }
        char big[64]; snprintf(big,sizeof(big),"%" PRIu64,UINT64_C(9007199254740993)+(offset+row)/2); text(&r,0,row,big);
        if (exact) text(&r,2,row,"9999999999999999999999999999.1234567890");
        size_t kind=((offset+row)/2)%4;
        if (!kind) CHECK(duckdb_value_is_null(&r,3,row)); else text(&r,3,row,kind==1 ? "" : kind==2 ? "NULL" : "İstanbul\t\"quoted\"");
    }
    if (exact) { duckdb_logical_type type=duckdb_column_logical_type(&r,2); CHECK(duckdb_get_type_id(type)==DUCKDB_TYPE_DECIMAL && duckdb_decimal_width(type)==38 && duckdb_decimal_scale(type)==10); duckdb_destroy_logical_type(&type); }
    if (!offset) for (size_t c=0;c<FIELDS;c++) { bool missing=duckdb_value_is_null(&r,c,0); CHECK(duckdb_value_is_null(&r,c,1)==missing); if (!missing) { char *value=duckdb_value_varchar(&r,c,0); text(&r,c,1,value); duckdb_free(value); } }
    duckdb_destroy_result(&r); duckdb_destroy_result(&baseline);
}
static duckdb_result prepared(duckdb_connection db,const char *sql,const char *value) {
    duckdb_prepared_statement stmt=NULL; CHECK(duckdb_prepare(db,sql,&stmt)==DuckDBSuccess);
    idx_t index=0; if (duckdb_bind_parameter_index(stmt,&index,"__duxcel_filter")==DuckDBSuccess && index) CHECK(duckdb_bind_varchar(stmt,index,value)==DuckDBSuccess);
    duckdb_result result={0}; if (duckdb_execute_prepared(stmt,&result)!=DuckDBSuccess) { fprintf(stderr,"Prepared: %s\n",duckdb_result_error(&result)); exit(1); } duckdb_destroy_prepare(&stmt); return result;
}
static void test_filtered_sorted(duckdb_connection db,const char *path,SourceKind kind) {
    Analysis source={.kind=kind,.path=dx_strdup(path)}; char *error=NULL; RecordSQL query={0};
    CHECK(analysis_filter_set(&source,"Money",FILTER_GREATER,"1.005",&error)); source.filter_number=kind==SOURCE_PARQUET ? (NumberType){NUM_DECIMAL,4,2} : (NumberType){NUM_FLOAT,0,0};
    CHECK(analysis_filter_set(&source,"nullable",FILTER_NOT_NULL,NULL,&error));
    CHECK(analysis_sort_set(&source,"tie key",true,false,&error)); CHECK(analysis_sort_set(&source,"Money",false,true,&error));
    CHECK(record_sql_build(&query,&source,fields,FIELDS,true,true,&error)==RECORD_SQL_READY);
    CHECK(strstr(query.relation,kind==SOURCE_PARQUET ? "\"Money\" > cast_to_type('1.00',\"Money\")" : "CAST(\"Money\" AS DOUBLE) > CAST($__duxcel_filter AS DOUBLE)") && strstr(query.relation,"ORDER BY \"tie key\" DESC NULLS LAST, \"Money\" ASC NULLS LAST, \"__duxcel_record_id_2\" ASC"));
    duckdb_result r=prepared(db,query.relation,"1.005"); CHECK(duckdb_column_count(&r)==FIELDS+1 && duckdb_row_count(&r)>256);
    bool seen[700]={0}; int last_tie=3,last_money=0; uint64_t last_id=0;
    for (size_t row=0;row<duckdb_row_count(&r);row++) {
        uint64_t id=duckdb_value_uint64(&r,FIELDS,row); CHECK(id>=1 && id<=700 && !seen[id-1]); seen[id-1]=true;
        uint64_t pair=(id-1)/2; CHECK(pair%5 && pair%4);
        int tie=duckdb_value_int32(&r,1,row),money=(int)(pair%5); CHECK(tie<=last_tie);
        if (tie==last_tie) { CHECK(money>=last_money); if (money==last_money) CHECK(id>last_id); }
        last_tie=tie; last_money=money; last_id=id;
    }
    for (size_t i=0;i<700;i++) CHECK(seen[i]==((i/2)%5!=0 && (i/2)%4!=0)); duckdb_destroy_result(&r);
    if (kind==SOURCE_PARQUET) {
        char *sql=dx_printf("EXPLAIN %s",query.relation); r=execute(db,sql); free(sql); char *plan=duckdb_value_varchar(&r,1,0);
        CHECK(plan && !strstr(plan,"STREAMING_WINDOW") && strstr(plan,"READ_PARQUET") && strstr(plan,"Filters:")); duckdb_free(plan); duckdb_destroy_result(&r);
    }
    analysis_filters_clear(&source); analysis_sorts_clear(&source);
    CHECK(analysis_filter_set(&source,"nullable",FILTER_EQUAL,"NULL",&error));
    CHECK(record_sql_build(&query,&source,fields,FIELDS,true,true,&error)==RECORD_SQL_READY && strstr(query.relation,"$__duxcel_filter"));
    r=prepared(db,query.relation,"NULL"); CHECK(duckdb_row_count(&r)==174); for (size_t row=0;row<duckdb_row_count(&r);row++) { text(&r,3,row,"NULL"); CHECK(((duckdb_value_uint64(&r,FIELDS,row)-1)/2)%4==2); } duckdb_destroy_result(&r);
    if (kind==SOURCE_PARQUET) {
        analysis_filters_clear(&source); char *values[]={"1.01","1.02",NULL},*list=analysis_values_json(values,3),*packed=analysis_filter_pack(FILTER_IN,list,FILTER_GREATER,"1.005");
        CHECK(packed && analysis_filter_set(&source,"Money",FILTER_COMPOSITE,packed,&error)); source.filter_number=(NumberType){NUM_DECIMAL,4,2};
        CHECK(record_sql_build(&query,&source,fields,FIELDS,true,true,&error)==RECORD_SQL_READY && strstr(query.relation,"$__duxcel_filter"));
        r=prepared(db,query.relation,packed); CHECK(duckdb_row_count(&r)==280);
        for (size_t row=0;row<duckdb_row_count(&r);row++) { uint64_t pair=(duckdb_value_uint64(&r,FIELDS,row)-1)/2; CHECK(pair%5==1 || pair%5==2); }
        duckdb_destroy_result(&r); free(list); free(packed);
    }
    record_sql_free(&query); analysis_free(&source); free(error);
}
static void test_demo(duckdb_connection db) {
    const char *const names[]={"Transaction ID","Date","City","Amount","Approved","Note"}; Analysis source={.kind=SOURCE_DEMO}; RecordSQL record={0}; char *error=NULL;
    CHECK(record_sql_build(&record,&source,names,6,false,false,&error)==RECORD_SQL_READY && record.kind==RECORD_ID_WINDOW && !strstr(record.relation,"file_row_number"));
    for (size_t offset=0;offset<=512;offset+=256) {
        char *sql=dx_printf("SELECT * FROM (%s) q LIMIT 256 OFFSET %zu",record.relation,offset); duckdb_result r=execute(db,sql); free(sql); CHECK(duckdb_row_count(&r)==256 && duckdb_column_count(&r)==7);
        for (size_t row=0;row<256;row++) CHECK(duckdb_value_uint64(&r,6,row)==offset+row+1 && duckdb_value_int64(&r,0,row)==(int64_t)(offset+row)); duckdb_destroy_result(&r);
    }
    CHECK(analysis_filter_set(&source,"Transaction ID",FILTER_GREATER,"600.5",&error)); source.filter_number=(NumberType){NUM_I64,0,0};
    CHECK(record_sql_build(&record,&source,names,6,false,true,&error)==RECORD_SQL_READY); char *sql=dx_printf("SELECT * FROM (%s) q LIMIT 1",record.relation); duckdb_result r=prepared(db,sql,"600.5");
    CHECK(duckdb_value_uint64(&r,6,0)==602 && duckdb_value_int64(&r,0,0)==601); duckdb_destroy_result(&r); free(sql); record_sql_free(&record); analysis_free(&source); free(error);
}
static void test_fallbacks(duckdb_connection db,const char *dir,const char *parquet) {
    char *collision=dx_printf("%s/collision.parquet",dir),*quoted=dx_quote(collision,'\''); char *sql=dx_printf("COPY (SELECT 7 AS \"FiLe_RoW_NuMbEr\") TO %s (FORMAT PARQUET)",quoted); command(db,sql); free(sql); free(quoted);
    const char *const names[]={"FiLe_RoW_NuMbEr"}; Analysis source={.kind=SOURCE_PARQUET,.path=collision}; RecordSQL record={0}; char *error=NULL;
    CHECK(record_sql_build(&record,&source,names,1,true,false,&error)==RECORD_SQL_UNSUPPORTED && !record.relation);
    sql=analysis_sql(&source,false); duckdb_result r=execute(db,sql); text(&r,0,0,"7"); CHECK(duckdb_column_count(&r)==1); duckdb_destroy_result(&r); free(sql);
    source.path=(char *)parquet; source.filter_column="nullable"; source.filter_op=FILTER_CONTAINS;
    char *large=dx_alloc(DX_RECORD_SQL_INPUT_BYTES+1); memset(large,'x',DX_RECORD_SQL_INPUT_BYTES); source.filter_value=large;
    CHECK(analysis_filters_valid(&source,&error)); CHECK(record_sql_build(&record,&source,fields,FIELDS,true,false,&error)==RECORD_SQL_UNSUPPORTED);
    sql=analysis_sql(&source,false); r=execute(db,sql); CHECK(duckdb_row_count(&r)==0 && duckdb_column_count(&r)==FIELDS); duckdb_destroy_result(&r); free(sql); free(large);
    source.filter_column=NULL; source.filter_value=NULL; source.unique_rows=true;
    CHECK(record_sql_build(&record,&source,fields,FIELDS,true,false,&error)==RECORD_SQL_UNSUPPORTED);
    sql=analysis_sql(&source,false); r=execute(db,sql); CHECK(duckdb_row_count(&r)==350 && duckdb_column_count(&r)==FIELDS); duckdb_destroy_result(&r); free(sql);
    source.unique_rows=false; source.group_column="tie key"; source.measure_column="Money"; source.aggregate=AGG_SUM;
    CHECK(record_sql_build(&record,&source,fields,FIELDS,true,false,&error)==RECORD_SQL_UNSUPPORTED);
    sql=analysis_sql(&source,false); r=execute(db,sql); CHECK(duckdb_row_count(&r)==3 && duckdb_column_count(&r)==2 && !strcmp(duckdb_column_name(&r,1),"Value")); duckdb_destroy_result(&r); free(sql);
    source.group_column=source.measure_column=NULL; source.query="SELECT nullable,Money FROM input";
    CHECK(record_sql_build(&record,&source,fields,FIELDS,true,false,&error)==RECORD_SQL_UNSUPPORTED);
    sql=analysis_sql(&source,false); r=execute(db,sql); CHECK(duckdb_row_count(&r)==700 && duckdb_column_count(&r)==2); duckdb_destroy_result(&r); free(sql);
    CHECK(unlink(collision)==0); free(collision); record_sql_free(&record); free(error);
}
int main(void) {
    test_bounds_and_ownership(); test_normal_sql_contract(); char dir[]="/tmp/duxcel-record-sql-XXXXXX"; CHECK(mkdtemp(dir)!=NULL);
    char *spill=dx_printf("%s/spill",dir); CHECK(mkdir(spill,0700)==0); duckdb_config config=NULL; duckdb_database database=NULL; duckdb_connection db=NULL; char *error=NULL;
    CHECK(duckdb_create_config(&config)==DuckDBSuccess); CHECK(duckdb_set_config(config,"memory_limit","512MB")==DuckDBSuccess); CHECK(duckdb_set_config(config,"threads","2")==DuckDBSuccess);
    CHECK(duckdb_set_config(config,"preserve_insertion_order","true")==DuckDBSuccess && duckdb_set_config(config,"temp_directory",spill)==DuckDBSuccess);
    CHECK(duckdb_open_ext(NULL,&database,config,&error)==DuckDBSuccess && duckdb_connect(database,&db)==DuckDBSuccess); duckdb_destroy_config(&config); duckdb_free(error);
    command(db,"CREATE TABLE original AS SELECT (9007199254740993::BIGINT+i//2)::BIGINT AS \"big\"\"id\",((i//2)%3)::INTEGER AS \"tie key\", '9999999999999999999999999999.1234567890'::DECIMAL(38,10) AS \"exact DECIMAL\",CASE (i//2)%4 WHEN 0 THEN NULL WHEN 1 THEN '' WHEN 2 THEN 'NULL' ELSE 'İstanbul'||chr(9)||chr(34)||'quoted'||chr(34) END AS nullable,((100+(i//2)%5)/100.0)::DECIMAL(4,2) AS Money,'business' AS __duxcel_record_id,7 AS __DUXCEL_RECORD_ID_1,'odd''quoted' AS \"line\nname\" FROM range(700) t(i)");
    char *parquet=dx_printf("%s/raw 'quoted.parquet",dir),*csv=dx_printf("%s/raw 'quoted.csv",dir);
    for (int kind=0;kind<2;kind++) {
        const char *path=kind ? csv : parquet; char *quoted=dx_quote(path,'\''),*sql=dx_printf("COPY original TO %s (FORMAT %s%s)",quoted,kind ? "CSV" : "PARQUET",kind ? ",HEADER true" : ""); command(db,sql); free(sql); free(quoted);
        Analysis source={.kind=kind ? SOURCE_CSV : SOURCE_PARQUET,.path=(char *)path}; RecordSQL record={0}; error=NULL;
        CHECK(record_sql_build(&record,&source,fields,FIELDS,true,false,&error)==RECORD_SQL_READY);
        CHECK(record.kind==(kind ? RECORD_ID_WINDOW : RECORD_ID_PARQUET) && strstr(record.relation,"raw ''quoted"));
        CHECK((strstr(record.relation,"file_row_number=true")!=NULL)==!kind && (strstr(record.relation,"allow_quoted_nulls=false")!=NULL)==(kind!=0));
        for (size_t offset=0;offset<=512;offset+=256) check_page(db,&source,&record,offset,offset==512 ? 188 : 256,!kind);
        record_sql_free(&record); free(error);
    }
    test_filtered_sorted(db,parquet,SOURCE_PARQUET); test_filtered_sorted(db,csv,SOURCE_CSV); test_demo(db); test_fallbacks(db,dir,parquet);
    CHECK(unlink(parquet)==0 && unlink(csv)==0); free(parquet); free(csv); duckdb_disconnect(&db); duckdb_close(&database);
    CHECK(rmdir(spill)==0 && rmdir(dir)==0); free(spill);
    printf("PASS: %u raw-record SQL checks (private native %s; session provenance only)\n",checks,duckdb_library_version()); return 0;
}
