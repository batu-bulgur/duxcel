#include "transforms/distinct.h"
#ifdef DUXCEL_DISTINCT_ENGINE_TEST
#include "data/database.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL distinct query:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static void test_exact_sql(void) {
    Analysis source={.kind=SOURCE_CSV,.path="odd'name.csv"}; DistinctQuery query={0}; char *error=NULL;
    const char *expected="SELECT DISTINCT CAST(\"A\"\"B\" AS VARCHAR) AS value,CAST(\"A\"\"B\" AS VARCHAR) AS label,NULL::VARCHAR AS upper_bound FROM (SELECT * FROM read_csv_auto('odd''name.csv',allow_quoted_nulls=false)) available WHERE contains(COALESCE(CAST(\"A\"\"B\" AS VARCHAR), 'NULL'), 'O''Reilly')";
    CHECK(distinct_query_build(&query,&source,NULL,"A\"B","O'Reilly",-1,NULL,NULL,true,&error));
    CHECK(!error && !query.keep_primary_filter && !strcmp(query.relation,expected));
    char *page=distinct_page_sql(&query,UINT64_MAX),*want=dx_printf("SELECT * FROM (%s) distinct_values ORDER BY value NULLS FIRST LIMIT 128 OFFSET 18446744073709551615",expected);
    CHECK(!strcmp(page,want)); free(page); free(want);
    char *count=distinct_count_sql(&query); want=dx_printf("SELECT count(*) FROM (%s) counted",expected);
    CHECK(!strcmp(count,want)); free(count); free(want);
    CHECK(distinct_query_build(&query,&source,NULL,"Date","",1,"2026-01-01","2027-01-01",false,&error));
    expected="SELECT DISTINCT CAST(date_trunc('month',\"Date\") AS VARCHAR) AS value,strftime(date_trunc('month',\"Date\"),'%B') AS label,CAST(date_trunc('month',\"Date\")+INTERVAL '1 month' AS VARCHAR) AS upper_bound FROM (SELECT * FROM read_csv_auto('odd''name.csv',allow_quoted_nulls=false)) available WHERE contains(COALESCE(CAST(\"Date\" AS VARCHAR), 'NULL'), '') AND \"Date\" >= cast_to_type('2026-01-01',\"Date\") AND \"Date\" < cast_to_type('2027-01-01',\"Date\")";
    CHECK(!strcmp(query.relation,expected));
    const char *labels[]={"strftime(date_trunc('year',\"Date\"),'%Y')","strftime(date_trunc('month',\"Date\"),'%B')","strftime(date_trunc('day',\"Date\"),'%d')","strftime(date_trunc('hour',\"Date\"),'%H:00')","strftime(date_trunc('minute',\"Date\"),'%H:%M')"};
    for (int level=0;level<5;level++) { CHECK(distinct_query_build(&query,&source,NULL,"Date",NULL,level,NULL,NULL,true,NULL)); CHECK(strstr(query.relation,labels[level])); }
    distinct_query_free(&query); distinct_query_free(&query); distinct_query_free(NULL);
    CHECK(!query.relation && !query.keep_primary_filter && !distinct_page_sql(NULL,0) && !distinct_count_sql(&query));
}
static void test_filter_scope_and_ownership(void) {
    Analysis source={.kind=SOURCE_DEMO,.group_column=dx_strdup("City"),.measure_column=dx_strdup("Amount"),.aggregate=AGG_SUM}; char *error=NULL;
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"Ankara",&error));
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"100",&error));
    CHECK(analysis_sort_set(&source,"City",true,false,&error));
    Analysis before=source; DistinctQuery query={0};
    CHECK(distinct_query_build(&query,&source,NULL,"Amount","",-1,NULL,NULL,true,&error));
    CHECK(query.keep_primary_filter && strstr(query.relation,"$__duxcel_filter") && !strstr(query.relation,"cast_to_type('100'") && !strstr(query.relation,"ORDER BY") && !strstr(query.relation,"PIVOT ("));
    CHECK(!memcmp(&before,&source,sizeof(source)));
    CHECK(distinct_query_build(&query,&source,NULL,"City","",-1,NULL,NULL,true,&error));
    CHECK(!query.keep_primary_filter && !strstr(query.relation,"$__duxcel_filter") && strstr(query.relation,"cast_to_type('100'"));
    CHECK(distinct_query_build(&query,&source,NULL,"Other","",-1,NULL,NULL,false,&error));
    CHECK(query.keep_primary_filter && !strstr(query.relation,"$__duxcel_filter") && strstr(query.relation,"'Ankara'") && strstr(query.relation,"'100'"));
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"Amount",AGG_SUM,&error));
    free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup("self-filter-ignored"); pivot.fields[PIVOT_FILTERS][0].filter=FILTER_EQUAL;
    free(pivot.fields[PIVOT_FILTERS][1].value); pivot.fields[PIVOT_FILTERS][1].value=dx_strdup("other-pivot-retained"); pivot.fields[PIVOT_FILTERS][1].filter=FILTER_EQUAL;
    CHECK(distinct_query_build(&query,&source,&pivot,"City","",-1,NULL,NULL,true,&error));
    CHECK(query.keep_primary_filter && strstr(query.relation,"$__duxcel_filter") && strstr(query.relation,"PIVOT (") && strstr(query.relation,"'other-pivot-retained'") && !strstr(query.relation,"'self-filter-ignored'"));
    analysis_free(&source); pivot_free(&pivot); CHECK(strstr(query.relation,"other-pivot-retained")); distinct_query_free(&query); free(error);
}
static void test_invalid_preserves_plan(void) {
    Analysis source={.kind=SOURCE_DEMO}; DistinctQuery query={dx_strdup("retained SQL"),true}; char *error=NULL;
    for (int level=-2;level<6;level+=7) { CHECK(!distinct_query_build(&query,&source,NULL,"Date","",level,NULL,NULL,false,&error)); CHECK(error && !strcmp(query.relation,"retained SQL") && query.keep_primary_filter); }
    CHECK(!distinct_query_build(&query,NULL,NULL,"City","",-1,NULL,NULL,false,&error));
    CHECK(!distinct_query_build(&query,&source,NULL,NULL,"",-1,NULL,NULL,false,&error));
    CHECK(!distinct_query_build(&query,&source,NULL,"","",-1,NULL,NULL,false,&error));
    source.filter_count=SIZE_MAX; CHECK(!distinct_query_build(&query,&source,NULL,"City","",-1,NULL,NULL,false,&error)); source.filter_count=0;
    PivotDefinition pivot={.counts={[PIVOT_ROWS]=1,[PIVOT_COLUMNS]=SIZE_MAX},.fields={[PIVOT_ROWS]={{.name="City"}}}};
    CHECK(!distinct_query_build(&query,&source,&pivot,"City","",-1,NULL,NULL,false,&error));
    pivot.counts[PIVOT_COLUMNS]=1; CHECK(!distinct_query_build(&query,&source,&pivot,"City","",-1,NULL,NULL,false,&error));
    char name[4098]; memset(name,'x',sizeof(name)-1); name[4097]=0;
    CHECK(!distinct_query_build(&query,&source,NULL,name,"",-1,NULL,NULL,false,&error));
    CHECK(!strcmp(query.relation,"retained SQL") && query.keep_primary_filter); distinct_query_free(&query); free(error);
}
#ifdef DUXCEL_DISTINCT_ENGINE_TEST
static void check_literal_query(Database *db,const Analysis *source,const char *column,int level,const char *lo,const char *hi,uint64_t expected) {
    DistinctQuery query={0}; char *error=NULL; duckdb_result result={0};
    CHECK(distinct_query_build(&query,source,NULL,column,"",level,lo,hi,false,&error));
    CHECK(!strstr(query.relation,"$__duxcel_filter")); char *sql=distinct_page_sql(&query,0);
    CHECK(database_execute(db,sql,&result,&error)); CHECK(duckdb_column_count(&result)==3 && duckdb_row_count(&result)==expected);
    duckdb_destroy_result(&result); free(sql); sql=distinct_count_sql(&query);
    CHECK(database_execute(db,sql,&result,&error)); CHECK(duckdb_value_uint64(&result,0,0)==expected);
    duckdb_destroy_result(&result); free(sql); distinct_query_free(&query); free(error);
}
static void test_native_binding_and_dates(void) {
    Database db={0}; char *error=NULL; CHECK(database_open(&db,&error));
    Analysis source={.kind=SOURCE_DEMO,.query=dx_strdup("SELECT * FROM (VALUES ('A',10, TIMESTAMP '2026-01-02 03:04:05'),('A',20,TIMESTAMP '2026-01-02 03:05:06'),('B',30,TIMESTAMP '2027-02-03 04:06:07'),('B',40,NULL)) t(City,Amount,Stamp)")};
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"A",&error)); DistinctPage values={0};
    CHECK(database_distinct(&db,&source,"Amount","",-1,NULL,NULL,0,-1,&values,&error));
    CHECK(values.count==2 && values.total==2 && !strcmp(values.values[0].value,"10") && !strcmp(values.values[1].value,"20")); distinct_free(&values);
    CHECK(database_distinct(&db,&source,"City","",-1,NULL,NULL,0,-1,&values,&error)); CHECK(values.total==2 && values.count==2); distinct_free(&values);
    check_literal_query(&db,&source,"Amount",-1,NULL,NULL,2); check_literal_query(&db,&source,"City",-1,NULL,NULL,2);
    CHECK(analysis_filter_set(&source,"Amount",FILTER_GREATER,"15",&error));
    CHECK(database_distinct(&db,&source,"Amount","",-1,NULL,NULL,0,-1,&values,&error)); CHECK(values.total==2); distinct_free(&values);
    CHECK(database_distinct(&db,&source,"City","",-1,NULL,NULL,0,-1,&values,&error)); CHECK(values.total==2); distinct_free(&values);
    check_literal_query(&db,&source,"Amount",-1,NULL,NULL,2); check_literal_query(&db,&source,"City",-1,NULL,NULL,2);
    uint64_t queries=db.query_count;
    CHECK(database_distinct(&db,&source,"Amount","",-1,NULL,NULL,0,2,&values,&error)); CHECK(db.query_count==queries+1 && values.total==2); distinct_free(&values);
    analysis_filters_clear(&source);
    for (int level=0;level<5;level++) {
        CHECK(database_distinct(&db,&source,"Stamp","",level,NULL,NULL,0,-1,&values,&error));
        CHECK(values.total==(uint64_t)(level==4 ? 4 : 3) && values.values[0].is_null && !values.values[0].key && !strcmp(values.values[0].label,"NULL"));
        CHECK(values.values[1].upper && strchr(values.values[1].key,'\t')); distinct_free(&values);
    }
    CHECK(database_distinct(&db,&source,"Stamp","",4,"2026-01-02 03:00:00","2026-01-02 04:00:00",0,-1,&values,&error));
    CHECK(values.count==2 && !strcmp(values.values[0].value,"2026-01-02 03:04:00") && !strcmp(values.values[0].label,"03:04")); distinct_free(&values);
    check_literal_query(&db,&source,"Stamp",4,"2026-01-02 03:00:00","2026-01-02 04:00:00",2);
    analysis_free(&source); database_close(&db); free(error);
}
#endif
int main(void) {
    test_exact_sql(); test_filter_scope_and_ownership(); test_invalid_preserves_plan();
#ifdef DUXCEL_DISTINCT_ENGINE_TEST
    test_native_binding_and_dates();
#endif
    printf("PASS: %u distinct query checks\n",checks); return 0;
}
