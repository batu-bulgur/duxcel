#include "data/worker.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#test); exit(1); } } while (0)
static void execute(Database *db,const char *sql) {
    duckdb_result r; char *error=NULL;
    if (!database_execute(db,sql,&r,&error)) { fprintf(stderr,"%s\n",error); exit(1); }
    duckdb_destroy_result(&r); free(error);
}
static Page fetch(Database *db,Analysis *a,uint64_t offset,int64_t total) {
    Page p; char *error=NULL;
    if (!database_fetch(db,a,offset,total,&p,&error)) { fprintf(stderr,"Fetch failed: %s\n",error); exit(1); }
    free(error); return p;
}
static size_t find_col(const Page *p,const char *name) {
    for (size_t i=0;i<p->column_count;i++) if (!strcmp(p->columns[i].name,name)) return i;
    return p->column_count;
}
static void fixtures(Database *db) {
    remove("build/tests/generated/fixture.duckdb");
    execute(db,"CREATE TABLE fixture AS SELECT * FROM (VALUES "
        "(1, 'İstanbul', 12.50::DECIMAL(12,2), DATE '2026-01-01', true, 'first'),"
        "(2, 'Ankara', 30.00, DATE '2026-01-02', false, NULL),"
        "(3, 'İstanbul', 7.25, DATE '2026-01-03', true, 'tab' || chr(9) || 'line' || chr(10) || 'quote' || chr(34)),"
        "(4, 'İzmir', 100.00, DATE '2026-01-04', false, repeat('long ',10000)),"
        "(5, 'Bursa', -4.50, DATE '2026-01-05', true, 'last')) "
        "t(\"Transaction ID\", \"City\", \"Amount\", \"Date\", \"Approved\", \"odd ""name\")");
    execute(db,"COPY fixture TO 'build/tests/generated/odd''name.parquet' (FORMAT PARQUET)");
    execute(db,"COPY (SELECT * FROM (VALUES (TIMESTAMP '2026-01-02 03:04:05'), (TIMESTAMP '2026-01-02 03:05:00'), (TIMESTAMP '2026-02-01 04:00:00'), (NULL::TIMESTAMP)) t(Stamp)) TO 'build/tests/generated/timestamps.parquet' (FORMAT PARQUET)");
    execute(db,"COPY (SELECT i::BIGINT AS ID, (i%10)::INTEGER AS category, (i%1000)/10.0 AS amount "
        "FROM range(1000000) t(i)) TO 'build/tests/generated/million.parquet' (FORMAT PARQUET)");
    execute(db,"ATTACH 'build/tests/generated/fixture.duckdb' AS fixture_db; "
        "CREATE TABLE fixture_db.\"table with spaces\" AS SELECT * FROM fixture; "
        "USE fixture_db; CREATE VIEW \"a view\" AS SELECT * FROM \"table with spaces\"; USE memory; DETACH fixture_db");
}
static void test_csv(Database *db) {
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    Page p=fetch(db,&a,0,-1); CHECK(p.total==5 && p.row_count==5 && p.column_count==6);
    CHECK(p.columns[0].numeric); CHECK(p.columns[3].type==DUCKDB_TYPE_DATE);
    CHECK(p.columns[4].type==DUCKDB_TYPE_BOOLEAN); CHECK(p.cells[1].text && !strcmp(p.cells[1].text,"İstanbul"));
    CHECK(p.cells[11].is_null); page_free(&p); analysis_free(&a);
}
static void test_typed_query(Database *db) {
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet")};
    Page p=fetch(db,&a,0,-1);
    CHECK(p.total==5); CHECK(p.columns[2].type==DUCKDB_TYPE_DECIMAL); CHECK(!strcmp(p.columns[2].type_name,"DECIMAL(12,2)"));
    CHECK(p.cells[11].is_null); CHECK(strlen(p.cells[23].text)==50000);
    char *odd=dx_strdup(p.columns[5].name); page_free(&p);
    a.sort_column=dx_strdup("Amount"); a.descending=true; p=fetch(db,&a,0,-1);
    CHECK(!strcmp(p.cells[0].text,"4")); CHECK(fabs(p.cells[2].number-100)<1e-9); page_free(&p);
    a.filter_column=dx_strdup("Amount"); a.filter_value=dx_strdup("20"); a.filter_op=FILTER_GREATER;
    p=fetch(db,&a,0,-1); CHECK(p.total==2 && p.row_count==2); page_free(&p);
    free(a.filter_value); a.filter_value=dx_strdup("not numeric"); char *error=NULL;
    CHECK(!database_fetch(db,&a,0,-1,&p,&error)); CHECK(error && strstr(error,"Enter a number")); free(error); page_free(&p);
    free(a.filter_value); a.filter_value=dx_strdup("99999999"); p=fetch(db,&a,0,-1);
    CHECK(p.total==0 && p.row_count==0 && p.column_count==6); page_free(&p);
    free(a.filter_column); a.filter_column=odd; a.filter_op=FILTER_NULL;
    p=fetch(db,&a,0,-1); CHECK(p.total==1 && p.cells[5].is_null); page_free(&p);
    a.filter_op=FILTER_NOT_NULL; p=fetch(db,&a,0,-1); CHECK(p.total==4); page_free(&p);
    free(a.filter_column); a.filter_column=dx_strdup("City"); a.filter_op=FILTER_EQUAL;
    free(a.filter_value); a.filter_value=dx_strdup("İstanbul"); p=fetch(db,&a,0,-1); CHECK(p.total==2); page_free(&p);
    free(a.filter_value); a.filter_value=dx_strdup("' OR 1=1; DROP TABLE fixture; --");
    p=fetch(db,&a,0,-1); CHECK(p.total==0); page_free(&p);
    a.filter_op=FILTER_CONTAINS; free(a.filter_value); a.filter_value=dx_strdup("stan");
    p=fetch(db,&a,0,-1); CHECK(p.total==2); page_free(&p);
    char *sql=analysis_sql(&a,false); CHECK(strstr(sql,"contains(CAST(\"City\" AS VARCHAR), 'stan')")); free(sql);
    free(a.filter_column); a.filter_column=NULL; free(a.sort_column); a.sort_column=dx_strdup("Transaction ID"); a.descending=false;
    char *tsv=NULL; error=NULL; CHECK(database_copy(db,&a,1,2,4,2,&tsv,&error));
    CHECK(strstr(tsv,"false\t\ntrue\t\"tab\tline\nquote\"\"\"")!=NULL); free(tsv);
    CHECK(!database_copy(db,&a,0,20001,0,1,&tsv,&error)); CHECK(error!=NULL); free(error);
    analysis_free(&a);
}
static void test_aggregates(Database *db) {
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet"),
        .group_column=dx_strdup("City"),.measure_column=dx_strdup("Amount"),.sort_column=dx_strdup("City")};
    for (int agg=AGG_SUM;agg<=AGG_MAX;agg++) {
        a.aggregate=(Aggregate)agg; Page p=fetch(db,&a,0,-1); CHECK(p.total==4 && p.column_count==2);
        bool found=false;
        for (size_t r=0;r<p.row_count;r++) if (!strcmp(p.cells[r*2].text,"İstanbul")) {
            double expected=agg==AGG_SUM ? 19.75 : agg==AGG_COUNT || agg==AGG_DISTINCT ? 2 : agg==AGG_AVG ? 9.875 : agg==AGG_MIN ? 7.25 : 12.5;
            CHECK(fabs(p.cells[r*2+1].number-expected)<1e-8); found=true;
        }
        CHECK(found); page_free(&p);
    }
    analysis_free(&a);
}
static void test_database(Database *db) {
    Analysis a={.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/fixture.duckdb")};
    Page p=fetch(db,&a,0,-1); CHECK(p.relation_count==2 && p.total==5);
    CHECK(a.table && !strcmp(a.table,"a view")); page_free(&p);
    uint64_t queries=db->query_count,counts=db->count_queries;
    free(a.table); a.table=dx_strdup("table with spaces"); p=fetch(db,&a,0,5); CHECK(p.total==5);
    CHECK(db->query_count==queries+1 && db->count_queries==counts);
    CHECK(p.relation_count==2 && !strcmp(p.relations[0].label,"main.a view"));
    page_free(&p); analysis_free(&a);
}
static void test_pivot(Database *db) {
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    PivotDefinition p={0}; char *error=NULL; Page page={0};
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_COLUMNS,"Approved",AGG_SUM,&error));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    char *sql=pivot_sql(&a,&p); CHECK(strstr(sql,"PIVOT (") && strstr(sql," ON \"Approved\"") && strstr(sql," GROUP BY \"City\"")); free(sql);
    CHECK(database_pivot(db,&a,&p,0,-1,&page,&error)); CHECK(page.total==4 && page.column_count==3);
    bool found=false;
    for (size_t r=0;r<page.row_count;r++) if (!strcmp(page.cells[r*3].text,"İstanbul")) {
        CHECK(page.cells[r*3+1].is_null && fabs(page.cells[r*3+2].number-19.75)<1e-8); found=true;
    }
    CHECK(found); CHECK(!strcmp(page.columns[1].caption,"false") && !strcmp(page.columns[2].caption,"true"));
    char *clipboard=NULL; CHECK(database_pivot_copy(db,&a,&p,0,4,0,3,true,&clipboard,&error));
    CHECK(strstr(clipboard,"City\tfalse_SUM(Amount)\ttrue_SUM(Amount)\n") && strstr(clipboard,"İstanbul\t\t19.75")); free(clipboard);
    uint64_t queries=db->query_count; page_free(&page);
    CHECK(database_pivot(db,&a,&p,1,4,&page,&error)); CHECK(page.offset==1 && page.row_count==3 && db->query_count==queries+1); page_free(&page);
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_AVG,&error));
    CHECK(database_pivot(db,&a,&p,0,-1,&page,&error)); CHECK(page.column_count==5 && page.total==4);
    CHECK(page.columns[1].detail && !strcmp(page.columns[1].caption,"false") && !strcmp(page.columns[1].detail,"SUM(Amount)")); page_free(&page);
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Amount",AGG_SUM,&error));
    DistinctPage distinct={0}; CHECK(database_pivot_distinct(db,&a,&p,"City","",-1,NULL,NULL,0,-1,&distinct,&error));
    CHECK(distinct.total==4); distinct_free(&distinct);
    p.fields[PIVOT_FILTERS][0].filter=FILTER_GREATER; free(p.fields[PIVOT_FILTERS][0].value); p.fields[PIVOT_FILTERS][0].value=dx_strdup("20");
    CHECK(database_pivot(db,&a,&p,0,-1,&page,&error)); CHECK(page.total==2); page_free(&page);
    CHECK(database_pivot_distinct(db,&a,&p,"City","",-1,NULL,NULL,0,-1,&distinct,&error)); CHECK(distinct.total==2); distinct_free(&distinct);
    CHECK(!pivot_assign(&p,(PivotRole)-1,"City",AGG_SUM,&error)); free(error); error=NULL;
    CHECK(!pivot_move(&p,(PivotRole)-1,0,PIVOT_ROWS,0,&error)); free(error); error=NULL;
    CHECK(pivot_assign(&p,PIVOT_ROWS,"Approved",AGG_SUM,&error)); CHECK(p.counts[PIVOT_COLUMNS]==0 && p.counts[PIVOT_ROWS]==2);
    CHECK(pivot_move(&p,PIVOT_ROWS,1,PIVOT_COLUMNS,0,&error)); CHECK(p.counts[PIVOT_ROWS]==1 && p.counts[PIVOT_COLUMNS]==1);
    PivotDefinition copy=pivot_copy(&p); pivot_remove(&p,PIVOT_VALUES,0); CHECK(copy.counts[PIVOT_VALUES]==2 && p.counts[PIVOT_VALUES]==1);
    pivot_free(&copy); pivot_free(&p); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")};
    CHECK(pivot_assign(&p,PIVOT_COLUMNS,"ID",AGG_SUM,&error)); CHECK(pivot_assign(&p,PIVOT_VALUES,"amount",AGG_SUM,&error));
    CHECK(!database_pivot(db,&a,&p,0,-1,&page,&error)); CHECK(error!=NULL); free(error); error=NULL; page_free(&page);
    pivot_remove(&p,PIVOT_COLUMNS,0); CHECK(pivot_assign(&p,PIVOT_ROWS,"category",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&p,0,-1,&page,&error)); CHECK(page.total==10 && page.column_count==2); page_free(&page);
    pivot_free(&p); analysis_free(&a);
}
static void test_composable_filters(Database *db) {
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")}; char *error=NULL;
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(analysis_filter_set(&a,"Amount",FILTER_GREATER,"10",&error));
    CHECK(analysis_filter_set(&a,"Approved",FILTER_EQUAL,"true",&error) && a.filter_count==2);
    Analysis aliases=analysis_copy(&a);
    CHECK(analysis_filter_set(&aliases,"CITY",FILTER_EQUAL,"Ankara",&error) && aliases.filter_count==2);
    const char *alias_value=NULL; CHECK(analysis_filter_get(&aliases,"city",NULL,&alias_value) && !strcmp(alias_value,"Ankara"));
    Page alias_page=fetch(db,&aliases,0,-1); CHECK(!alias_page.total); page_free(&alias_page);
    CHECK(analysis_filter_set(&aliases,"cItY",FILTER_EQUAL,"İstanbul",&error) && analysis_same_result(&aliases,&a));
    alias_page=fetch(db,&aliases,0,-1); CHECK(alias_page.total==1); page_free(&alias_page);
    analysis_filter_remove(&aliases,"CiTy"); CHECK(aliases.filter_count==1 && !analysis_filter_get(&aliases,"City",NULL,NULL)); analysis_free(&aliases);
    aliases=analysis_copy(&a); free(aliases.filters[0].column); aliases.filters[0].column=dx_strdup("CITY");
    CHECK(!analysis_filters_valid(&aliases,&error)); free(error); error=NULL; analysis_free(&aliases);
    Page p=fetch(db,&a,0,-1); CHECK(p.total==1 && !strcmp(p.cells[0].text,"1")); page_free(&p);
    DistinctPage d={0}; CHECK(database_distinct(db,&a,"City","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==1); distinct_free(&d);
    CHECK(database_distinct(db,&a,"Amount","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==2); distinct_free(&d);
    Analysis previous=analysis_copy(&a);
    CHECK(analysis_filter_move(&a,2,0) && !strcmp(a.filter_column,"Approved"));
    CHECK(analysis_same_result(&a,&previous));
    CHECK(analysis_filter_set(&previous,"Amount",FILTER_GREATER,"11",&error)); CHECK(!analysis_same_result(&a,&previous));
    CHECK(analysis_filter_set(&previous,"Amount",FILTER_GREATER,"10",&error)); CHECK(analysis_same_result(&a,&previous));
    CHECK(analysis_sort_set(&previous,"City",false,false,&error)); CHECK(!analysis_same_result(&a,&previous));
    analysis_sorts_clear(&previous); CHECK(analysis_same_result(&a,&previous));
    previous.kind=SOURCE_PARQUET; CHECK(!analysis_same_result(&a,&previous)); previous.kind=a.kind;
    free(previous.path); previous.path=dx_strdup("other.csv"); CHECK(!analysis_same_result(&a,&previous));
    free(previous.path); previous.path=dx_strdup(a.path); CHECK(analysis_same_result(&a,&previous));
    previous.group_column=dx_strdup("City"); previous.measure_column=dx_strdup("Amount"); CHECK(!analysis_same_result(&a,&previous));
    analysis_free(&previous);
    p=fetch(db,&a,0,-1); CHECK(p.total==1); page_free(&p);
    Analysis copy=analysis_copy(&a); analysis_filter_remove(&a,"Amount");
    CHECK(a.filter_count==1 && copy.filter_count==2 && analysis_filter_get(&copy,"Amount",NULL,NULL));
    p=fetch(db,&a,0,-1); CHECK(p.total==2); page_free(&p);
    analysis_filter_remove(&a,"Approved"); CHECK(a.filter_count==0 && !strcmp(a.filter_column,"City"));
    analysis_filters_clear(&a); CHECK(!a.filter_column && !a.filter_count);
    CHECK(analysis_filter_set(&a,"Amount",FILTER_GREATER,"0",&error));
    CHECK(analysis_filter_set(&a,"Note",FILTER_EQUAL,"' OR true --",&error)); p=fetch(db,&a,0,-1); CHECK(p.total==0); page_free(&p);
    CHECK(analysis_filter_set(&a,"Note",FILTER_EQUAL,"A \"quote\"",&error)); p=fetch(db,&a,0,-1); CHECK(p.total==1 && !strcmp(p.cells[0].text,"4")); page_free(&p);
    analysis_free(&a); analysis_free(&copy);
    for (int i=0;i<DX_ANALYSIS_FILTERS;i++) { char name[32]; snprintf(name,sizeof(name),"Field %d",i); CHECK(analysis_filter_set(&a,name,FILTER_NOT_NULL,NULL,&error)); }
    CHECK(analysis_filters_valid(&a,&error)); CHECK(!analysis_filter_set(&a,"Too many",FILTER_NULL,NULL,&error)); free(error); error=NULL;
    CHECK(a.filter_count==15); analysis_free(&a);
}
static void test_pivot_null_columns(Database *db) {
    char *error=NULL; duckdb_result result={0};
    CHECK(database_execute(db,"COPY (SELECT city AS \"City\",category AS \"Category\",year AS \"Year\",amount AS \"Amount\",DATE '2026-01-01'+year AS \"Date\" FROM (VALUES (NULL::VARCHAR,'foo',1,10.0),(NULL,NULL,1,20.0),('X','FOO',2,30.0),('X','(blank)',NULL,40.0),('X','quote_\"''_SUM',10,NULL)) t(city,category,year,amount)) TO 'build/tests/generated/pivot-nulls.parquet' (FORMAT PARQUET)",&result,&error)); duckdb_destroy_result(&result);
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/pivot-nulls.parquet")}; PivotDefinition definition={0}; Page page={0};
    CHECK(pivot_assign(&definition,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&definition,PIVOT_COLUMNS,"Category",AGG_SUM,&error));
    CHECK(pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
    CHECK(database_pivot(db,&a,&definition,0,-1,&page,&error)); CHECK(page.total==2 && page.column_count==11 && page.cells[0].is_null);
    bool blank=false,real_blank=false,upper=false,lower=false; double sum=0,rows=0;
    for (size_t c=1;c<page.column_count;c++) {
        const char *caption=page.columns[c].caption; blank|=page.columns[c].category_count==1 && page.columns[c].categories[0]==NULL; real_blank|=!strcmp(caption,"(blank)") && page.columns[c].categories[0]!=NULL; upper|=!strcmp(caption,"FOO"); lower|=!strcmp(caption,"foo");
        for (size_t r=0;r<page.row_count;r++) if (!page.cells[r*page.column_count+c].is_null) { double value=page.cells[r*page.column_count+c].number; if (strstr(page.columns[c].detail,"SUM")) sum+=value; else rows+=value; }
    }
    CHECK(blank && real_blank && upper && lower); CHECK(fabs(sum-100)<1e-8 && rows==5);
    CHECK(page.sql && strstr(page.sql,"NULL AS \"category_0\"") && strstr(page.sql,"'quote_\"''_SUM'"));
    CHECK(database_execute(db,page.sql,&result,&error)); CHECK(duckdb_row_count(&result)==2 && duckdb_column_count(&result)==11); duckdb_destroy_result(&result); page_free(&page);
    CHECK(pivot_assign(&definition,PIVOT_COLUMNS,"Year",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&definition,0,-1,&page,&error)); CHECK(page.total==2 && page.column_count==41);
    sum=0; for (size_t c=1;c<page.column_count;c++) if (strstr(page.columns[c].detail,"SUM")) for (size_t r=0;r<page.row_count;r++) if (!page.cells[r*page.column_count+c].is_null) sum+=page.cells[r*page.column_count+c].number;
    CHECK(fabs(sum-100)<1e-8); CHECK(!strcmp(page.columns[1].caption,"NULL · NULL") && !strcmp(page.columns[7].caption,"NULL · 10")); page_free(&page);
    pivot_remove(&definition,PIVOT_COLUMNS,0); pivot_remove(&definition,PIVOT_COLUMNS,0); CHECK(pivot_assign(&definition,PIVOT_COLUMNS,"Date",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&definition,0,-1,&page,&error)); CHECK(page.column_count==9 && page.columns[1].category_count==1 && !page.columns[1].categories[0]);
    sum=0; for (size_t c=1;c<page.column_count;c++) if (strstr(page.columns[c].detail,"SUM")) for (size_t r=0;r<page.row_count;r++) if (!page.cells[r*page.column_count+c].is_null) sum+=page.cells[r*page.column_count+c].number;
    CHECK(fabs(sum-100)<1e-8); page_free(&page);
    CHECK(analysis_filter_set(&a,"Amount",FILTER_GREATER,"99",&error)); CHECK(database_pivot(db,&a,&definition,0,-1,&page,&error)); CHECK(page.total==0 && page.column_count==3 && !page.columns[1].category_count && !strcmp(page.columns[1].caption,"SUM(Amount)")); page_free(&page);
    pivot_free(&definition); analysis_free(&a);
}
static void test_pivot_date_grains(Database *db) {
    execute(db,"COPY (SELECT city AS City,day AS Date,day::TIMESTAMP AS Stamp,amount::DECIMAL(12,2) AS Amount,10 AS __DUXCEL_DATE_1_0_0 FROM (VALUES ('A',DATE '2025-12-31',1),('A',DATE '2026-01-01',2),('A',DATE '2026-01-15',3),('B',DATE '2026-04-01',4),('B',NULL,5)) t(city,day,amount)) TO 'build/tests/generated/pivot-dates.parquet' (FORMAT PARQUET)");
    Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/pivot-dates.parquet")}; PivotDefinition p={0}; Page page={0}; char *error=NULL;
    CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_YEAR,&error));
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_QUARTER,&error));
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
    CHECK(pivot_valid(&p,&error)); CHECK(database_pivot(db,&source,&p,0,-1,&page,&error));
    CHECK(page.total==2 && page.column_count==97 && page.sql && strstr(page.sql,"PIVOT") && strstr(page.sql,"date_trunc('month'") && strstr(page.sql,"quarter("));
    double sum=0,count=0; bool january=false,missing=false;
    for (size_t c=1;c<page.column_count;c++) {
        Column *col=&page.columns[c]; CHECK(col->category_count==3 && col->category_grains[0]==PIVOT_DATE_YEAR && col->category_grains[2]==PIVOT_DATE_MONTH);
        january|=strstr(col->caption,"Jan 2026")!=NULL; missing|=!col->categories[0] && !col->categories[1] && !col->categories[2];
        for (size_t r=0;r<page.row_count;r++) if (!page.cells[r*page.column_count+c].is_null) {
            if ((c-1)%2) count+=page.cells[r*page.column_count+c].number; else sum+=page.cells[r*page.column_count+c].number;
        }
    }
    CHECK(january && missing && sum==15 && count==5); page_free(&page);
    /* Grain edits are atomic, duplicates are rejected and moves retain grain. */
    CHECK(!pivot_set_grain(&p,PIVOT_COLUMNS,0,PIVOT_DATE_MONTH,&error)); CHECK(p.fields[PIVOT_COLUMNS][0].grain==PIVOT_DATE_YEAR); free(error); error=NULL;
    CHECK(pivot_move(&p,PIVOT_COLUMNS,2,PIVOT_ROWS,0,&error)); CHECK(p.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_MONTH && !strcmp(p.fields[PIVOT_ROWS][0].name,"Date"));
    CHECK(database_pivot(db,&source,&p,0,-1,&page,&error)); CHECK(!strcmp(page.columns[0].name,"MONTH(Date)") && page.columns[0].type==DUCKDB_TYPE_DATE && page.columns[0].date_grain==PIVOT_DATE_MONTH); page_free(&page);
    CHECK(pivot_move(&p,PIVOT_ROWS,0,PIVOT_VALUES,0,&error)); CHECK(p.fields[PIVOT_VALUES][0].grain==PIVOT_DATE_EXACT); pivot_remove(&p,PIVOT_VALUES,0);
    /* Derived aliases cannot shadow an unusually named raw source field. */
    CHECK(pivot_assign(&p,PIVOT_VALUES,"__DUXCEL_DATE_1_0_0",AGG_SUM,&error)); char *key=pivot_field_key(&p,PIVOT_COLUMNS,0);
    CHECK(!strcmp(key,"__duxcel_date_1_0_1")); free(key); CHECK(database_pivot(db,&source,&p,0,-1,&page,&error)); page_free(&page);
    pivot_free(&p); CHECK(pivot_assign_grain(&p,PIVOT_ROWS,"Stamp",AGG_SUM,PIVOT_DATE_MONTH,&error)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(database_pivot(db,&source,&p,0,-1,&page,&error)); CHECK(page.total==4 && page.cells[0].is_null); page_free(&page);
    char *tsv=NULL; CHECK(database_pivot_copy(db,&source,&p,0,4,0,2,true,&tsv,&error)); CHECK(strstr(tsv,"MONTH(Stamp)") && !strstr(tsv,"__duxcel_date")); free(tsv);
    CHECK(pivot_assign_grain(&p,PIVOT_ROWS,"City",AGG_SUM,PIVOT_DATE_YEAR,&error)); CHECK(!database_pivot(db,&source,&p,0,-1,&page,&error) && error && strstr(error,"DATE or TIMESTAMP")); page_free(&page); free(error); error=NULL;
    CHECK(!pivot_assign_grain(&p,PIVOT_FILTERS,"Date",AGG_SUM,PIVOT_DATE_YEAR,&error)); free(error); error=NULL;
    CHECK(!pivot_set_grain(&p,PIVOT_VALUES,0,PIVOT_DATE_MONTH,&error)); free(error);
    char display[64]; pivot_date_label(PIVOT_DATE_MONTH,"2026-01-01",display); CHECK(!strcmp(display,"Jan 2026")); pivot_date_label(PIVOT_DATE_EXACT,"2026-01-01",display); CHECK(!strcmp(display,"2026-01-01"));
    pivot_free(&p); analysis_free(&source);
}
static void test_null_text(Database *db) {
    execute(db,"COPY (SELECT id AS ID,value AS Value,10 AS Amount FROM (VALUES (1,NULL::VARCHAR),(2,'NULL'),(3,''),(4,'\"\"'),(5,'(blank)'),(6,'null')) t(id,value)) TO 'build/tests/generated/null-text.parquet' (FORMAT PARQUET)");
    Analysis csv={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")}; Page imported=fetch(db,&csv,0,-1);
    CHECK(imported.total==6 && imported.cells[1].is_null);
    CHECK(!imported.cells[4].is_null && !strcmp(imported.cells[4].text,"NULL"));
    CHECK(!imported.cells[7].is_null && !*imported.cells[7].text);
    CHECK(!imported.cells[10].is_null && !strcmp(imported.cells[10].text,"\"\"")); page_free(&imported);
    char *csv_sql=analysis_sql(&csv,false),*convert=dx_printf("COPY (%s) TO 'build/tests/generated/null-text-csv.parquet' (FORMAT PARQUET)",csv_sql); execute(db,convert); free(convert); free(csv_sql); analysis_free(&csv);
    Analysis roundtrip={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/null-text-csv.parquet")}; imported=fetch(db,&roundtrip,0,-1);
    CHECK(imported.cells[1].is_null && !imported.cells[7].is_null && !*imported.cells[7].text && !strcmp(imported.cells[10].text,"\"\"")); page_free(&imported); analysis_free(&roundtrip);
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/null-text.parquet")}; Page p=fetch(db,&a,0,-1);
    CHECK(p.total==6 && p.cells[1].is_null && p.cells[1].text==NULL);
    CHECK(!p.cells[4].is_null && !strcmp(p.cells[4].text,"NULL"));
    CHECK(!p.cells[7].is_null && !strcmp(p.cells[7].text,""));
    CHECK(!p.cells[10].is_null && !strcmp(p.cells[10].text,"\"\"")); page_free(&p);
    DistinctPage choices={0}; char *error=NULL;
    CHECK(database_distinct(db,&a,"Value","",-1,NULL,NULL,0,-1,&choices,&error)); CHECK(choices.count==6 && choices.values[0].is_null);
    bool literal=false,empty=false,quotes=false;
    for (size_t i=1;i<choices.count;i++) { CHECK(!choices.values[i].is_null); literal|=!strcmp(choices.values[i].value,"NULL"); empty|=!*choices.values[i].value; quotes|=!strcmp(choices.values[i].value,"\"\""); }
    CHECK(literal && empty && quotes); distinct_free(&choices);
    PivotDefinition d={0}; CHECK(pivot_assign(&d,PIVOT_COLUMNS,"Value",AGG_SUM,&error)); CHECK(pivot_assign(&d,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&d,0,-1,&p,&error)); CHECK(p.total==1 && p.column_count==6);
    unsigned nulls=0,literals=0,empties=0;
    for (size_t c=0;c<p.column_count;c++) {
        Column *col=&p.columns[c]; CHECK(col->category_count==1 && p.cells[c].number==10);
        if (!col->categories[0]) nulls++; else if (!*col->categories[0]) empties++; else if (!strcmp(col->categories[0],"NULL")) literals++;
    }
    CHECK(nulls==1 && empties==1 && literals==1); page_free(&p);
    CHECK(analysis_filter_set(&a,"Value",FILTER_IN,"[null]",&error)); p=fetch(db,&a,0,-1); CHECK(p.total==1 && p.cells[1].is_null); page_free(&p);
    CHECK(analysis_filter_set(&a,"Value",FILTER_IN,"[\"NULL\"]",&error)); p=fetch(db,&a,0,-1); CHECK(p.total==1 && !p.cells[1].is_null && !strcmp(p.cells[1].text,"NULL")); page_free(&p);
    CHECK(analysis_filter_set(&a,"Value",FILTER_IN,"[\"\"]",&error)); p=fetch(db,&a,0,-1); CHECK(p.total==1 && !p.cells[1].is_null && !*p.cells[1].text); page_free(&p);
    pivot_free(&d); analysis_free(&a); free(error);
}
static bool cancel_export(void *context) { unsigned *calls=context; return ++*calls>=5; }
static void test_export(Database *db) {
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")}; char *error=NULL; uint64_t rows=0;
    CHECK(analysis_filter_set(&a,"Value",FILTER_IN,"[null,\"NULL\",\"\"]",&error));
    uint64_t queries=db->query_count,counts=db->count_queries;
    CHECK(database_export_parquet(db,&a,"build/tests/generated/export-null.parquet",&rows,&error)); CHECK(rows==3 && db->query_count==queries+1 && db->count_queries==counts);
    Analysis exported={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/export-null.parquet")}; Page p=fetch(db,&exported,0,-1);
    CHECK(p.total==3 && p.cells[1].is_null && !p.cells[4].is_null && !strcmp(p.cells[4].text,"NULL") && !p.cells[7].is_null && !*p.cells[7].text); page_free(&p);
    CHECK(!database_export_parquet(db,&a,a.path,&rows,&error)); free(error); error=NULL;
#ifndef _WIN32
    remove("build/tests/generated/export-source-link.parquet"); CHECK(link(a.path,"build/tests/generated/export-source-link.parquet")==0);
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/export-source-link.parquet",&rows,&error)); free(error); error=NULL; remove("build/tests/generated/export-source-link.parquet");
#endif
    CHECK(analysis_filter_set(&a,"Missing field",FILTER_EQUAL,"x",&error));
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/export-null.parquet",&rows,&error)); CHECK(!rows && error); free(error); error=NULL;
    analysis_filter_remove(&a,"Missing field"); p=fetch(db,&exported,0,-1); CHECK(p.total==3 && p.cells[1].is_null); page_free(&p);
    unsigned calls=0; db->cancelled=cancel_export; db->cancel_context=&calls;
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/export-null.parquet",&rows,&error)); CHECK(!rows && calls>=5); free(error); error=NULL;
    db->cancelled=NULL; db->cancel_context=NULL; p=fetch(db,&exported,0,-1); CHECK(p.total==3 && !*p.cells[7].text); page_free(&p);
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/missing-parent/export.parquet",&rows,&error)); free(error); error=NULL;
    analysis_free(&a); analysis_free(&exported);
    Analysis million={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")};
    double start=dx_millis(); CHECK(database_export_parquet(db,&million,"build/tests/generated/export-million.parquet",&rows,&error)); CHECK(rows==1000000);
    printf("Streamed 1M rows to Parquet: %.2f ms\n",dx_millis()-start); analysis_free(&million); free(error);
}
static void test_pivot_export(Database *db) {
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    PivotDefinition pivot={0}; Page page={0}; char *error=NULL; uint64_t rows=0;
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"Approved",AGG_SUM,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&pivot,0,-1,&page,&error) && page.total==1 && strstr(page.sql,"PIVOT")); page_free(&page);
    uint64_t queries=db->query_count,counts=db->count_queries;
    CHECK(database_export_pivot_parquet(db,&a,&pivot,"build/tests/generated/export-pivot.parquet",&rows,&error));
    CHECK(rows==1 && db->query_count==queries+1 && db->count_queries==counts);
    Analysis output={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/export-pivot.parquet")}; page=fetch(db,&output,0,-1);
    CHECK(page.total==1 && page.column_count==2 && !strcmp(page.cells[0].text,"İstanbul") && page.cells[1].number==19.75);
    CHECK(!strcmp(page.columns[0].name,"City") && !strcmp(page.columns[1].name,"Approved='true' · SUM(Amount)")); page_free(&page);
    CHECK(!database_export_pivot_parquet(db,&a,&pivot,a.path,&rows,&error) && !rows); free(error); error=NULL;
    CHECK(!database_export_pivot_parquet(db,&a,NULL,output.path,&rows,&error) && !rows); free(error); error=NULL;
    analysis_free(&a); pivot_free(&pivot);
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"Value",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_MAX,&error));
    CHECK(database_export_pivot_parquet(db,&a,&pivot,output.path,&rows,&error) && rows==6);
    page=fetch(db,&output,0,-1); unsigned nulls=0,literals=0,empty=0,quotes=0;
    for (size_t r=0;r<page.row_count;r++) {
        Cell *cell=&page.cells[r*page.column_count];
        if (cell->is_null) nulls++; else if (!*cell->text) empty++; else if (!strcmp(cell->text,"NULL")) literals++; else if (!strcmp(cell->text,"\"\"")) quotes++;
    }
    CHECK(page.total==6 && nulls==1 && literals==1 && empty==1 && quotes==1); page_free(&page);
    pivot_free(&pivot); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"Value",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_MAX,&error));
    CHECK(database_export_pivot_parquet(db,&a,&pivot,output.path,&rows,&error) && rows==1);
    page=fetch(db,&output,0,-1);
    const char *names[]={"Value=NULL · MAX(Amount)","Value='NULL' · MAX(Amount)","Value='' · MAX(Amount)","Value='\"\"' · MAX(Amount)"};
    CHECK(page.column_count==6 && page.total==1);
    for (size_t i=0;i<sizeof(names)/sizeof(*names);i++) { size_t c=find_col(&page,names[i]); CHECK(c<page.column_count && !page.cells[c].is_null && page.cells[c].number==10); }
    page_free(&page);
    pivot_free(&pivot); analysis_free(&a);
    remove("build/tests/generated/export-pivots.duckdb");
    execute(db,"ATTACH 'build/tests/generated/export-pivots.duckdb' AS export_fixture; CREATE TABLE export_fixture.series AS SELECT i AS id,(i/100.0)::DECIMAL(18,2) AS amount FROM range(600) t(i); CREATE TABLE export_fixture.big AS SELECT '170141183460469231731687303715884105727'::HUGEINT AS amount; DETACH export_fixture");
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/export-pivots.duckdb"),.table=dx_strdup("series")};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"id",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"amount",AGG_SUM,&error));
    CHECK(database_pivot(db,&a,&pivot,0,-1,&page,&error) && page.total==600 && page.row_count==256); page_free(&page);
    queries=db->query_count; counts=db->count_queries;
    CHECK(database_export_pivot_parquet(db,&a,&pivot,output.path,&rows,&error) && rows==600);
    CHECK(db->query_count==queries+1 && db->count_queries==counts);
    page=fetch(db,&output,599,-1); CHECK(page.total==600 && page.row_count==1 && !strcmp(page.cells[0].text,"599") && !strcmp(page.cells[1].text,"5.99") && page.columns[1].type==DUCKDB_TYPE_DECIMAL); page_free(&page);
    unsigned calls=0; db->cancelled=cancel_export; db->cancel_context=&calls;
    CHECK(!database_export_pivot_parquet(db,&a,&pivot,output.path,&rows,&error) && !rows && calls>=5); free(error); error=NULL;
    db->cancelled=NULL; db->cancel_context=NULL;
    page=fetch(db,&output,599,-1); CHECK(page.total==600 && !strcmp(page.cells[1].text,"5.99")); page_free(&page);
    CHECK(!database_export_pivot_parquet(db,&a,&pivot,"build/tests/generated/missing-parent/export.parquet",&rows,&error)); free(error); error=NULL;
    free(a.table); a.table=dx_strdup("big"); pivot_free(&pivot); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"amount",AGG_MAX,&error));
    CHECK(!database_export_pivot_parquet(db,&a,&pivot,output.path,&rows,&error) && !rows && error && strstr(error,"128-bit")); free(error); error=NULL;
    page=fetch(db,&output,599,-1); CHECK(page.total==600 && !strcmp(page.cells[1].text,"5.99")); page_free(&page);
    CHECK(database_export_parquet(db,&output,"build/tests/generated/export-pivot-recovery.parquet",&rows,&error) && rows==600);
    pivot_free(&pivot); analysis_free(&a); analysis_free(&output); free(error);
}
static void test_window_counts(Database *db) {
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")}; Page p={0}; char *error=NULL;
    uint64_t queries=db->query_count,counts=db->count_queries,total=0;
    CHECK(database_fetch_window(db,&a,0,-1,&p,&error)); CHECK(p.count_pending && p.total==DX_PAGE_ROWS && p.row_count==DX_PAGE_ROWS);
    CHECK(db->query_count==queries+1 && db->count_queries==counts); page_free(&p);
    CHECK(database_count(db,&a,&total,&error)); CHECK(total==1000000 && db->query_count==queries+2 && db->count_queries==counts+1);
    CHECK(database_fetch_window(db,&a,999900,-1,&p,&error)); CHECK(!p.count_pending && p.total==1000000 && p.row_count==100); page_free(&p);
    CHECK(database_fetch_window(db,&a,1001000,-1,&p,&error)); CHECK(p.count_pending && !p.total && !p.row_count); page_free(&p);
    CHECK(database_fetch_window(db,&a,1001000,1000000,&p,&error)); CHECK(!p.count_pending && p.total==1000000 && !p.row_count && db->count_queries==counts+1); page_free(&p);
    analysis_free(&a); a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")};
    CHECK(database_fetch_window(db,&a,0,-1,&p,&error)); CHECK(!p.count_pending && p.total==6 && p.cells[1].is_null && !p.cells[7].is_null); page_free(&p);
    analysis_free(&a); free(error);
}
static void numeric_count(Database *db,Analysis *a,const char *column,FilterOp op,const char *value,uint64_t expected) {
    char *error=NULL; CHECK(analysis_filter_set(a,column,op,value,&error)); Page page={0};
    if (!database_fetch(db,a,0,-1,&page,&error)) { fprintf(stderr,"Numeric %s %s %s: %s\n",column,filter_label(op),value,error); exit(1); }
    if (page.total!=expected) fprintf(stderr,"Numeric %s %s %s: expected %" PRIu64 ", got %" PRIu64 "\n",column,filter_label(op),value,expected,page.total);
    CHECK(page.total==expected); page_free(&page); free(error);
}
static void test_numeric_filters(Database *db) {
    execute(db,"COPY (SELECT tiny::TINYINT AS Tiny,money::DECIMAL(3,1) AS Money FROM (VALUES (-128,-99.9),(-2,-1.2),(-1,-0.1),(0,0.0),(1,0.1),(2,1.2),(127,99.9),(NULL,NULL)) t(tiny,money)) TO 'build/tests/generated/numeric.parquet' (FORMAT PARQUET)");
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric.parquet")};
    numeric_count(db,&a,"tiny",FILTER_GREATER,"1000",0); numeric_count(db,&a,"tiny",FILTER_LESS,"1000",7);
    numeric_count(db,&a,"tiny",FILTER_GREATER,"-1000",7); numeric_count(db,&a,"tiny",FILTER_LESS,"-1000",0);
    numeric_count(db,&a,"tiny",FILTER_GREATER,"1.5",2); numeric_count(db,&a,"tiny",FILTER_LESS,"1.5",5); numeric_count(db,&a,"tiny",FILTER_EQUAL,"1.5",0);
    numeric_count(db,&a,"tiny",FILTER_GREATER,"-1.5",5); numeric_count(db,&a,"tiny",FILTER_LESS,"-1.5",2);
    numeric_count(db,&a,"tiny",FILTER_GREATER,"1e-100000",3); numeric_count(db,&a,"tiny",FILTER_LESS,"1e-100000",4);
    numeric_count(db,&a,"tiny",FILTER_GREATER,"-1e-100000",4); numeric_count(db,&a,"tiny",FILTER_LESS,"-1e-100000",3);
    numeric_count(db,&a,"tiny",FILTER_EQUAL," -0e999999999999999999999999 ",1); numeric_count(db,&a,"tiny",FILTER_LESS,"1e99999999999999",7);
    analysis_filters_clear(&a);
    numeric_count(db,&a,"Money",FILTER_GREATER,"1000.0",0); numeric_count(db,&a,"Money",FILTER_LESS,"1000.0",7);
    numeric_count(db,&a,"Money",FILTER_LESS,"99.95",7); numeric_count(db,&a,"Money",FILTER_GREATER,"-99.95",7);
    numeric_count(db,&a,"Money",FILTER_GREATER,"0.005",3); numeric_count(db,&a,"Money",FILTER_LESS,"0.005",4); numeric_count(db,&a,"Money",FILTER_EQUAL,"0.005",0);
    numeric_count(db,&a,"Money",FILTER_GREATER,"-0.005",4); numeric_count(db,&a,"Money",FILTER_LESS,"-0.005",3);
    numeric_count(db,&a,"Money",FILTER_GREATER,"1.21",1); numeric_count(db,&a,"Money",FILTER_LESS,"1.21",6);
    numeric_count(db,&a,"Money",FILTER_LESS,"1.19",5); numeric_count(db,&a,"Money",FILTER_GREATER,"1.19",2);
    numeric_count(db,&a,"Money",FILTER_GREATER,"-1.21",6); numeric_count(db,&a,"Money",FILTER_LESS,"-1.21",1);
    numeric_count(db,&a,"Money",FILTER_LESS,"-1.19",2); numeric_count(db,&a,"Money",FILTER_GREATER,"-1.19",5);
    numeric_count(db,&a,"Money",FILTER_EQUAL,"+00.10000",1);
    numeric_count(db,&a,"Money",FILTER_LESS,"1.21",6);
    char *clipboard=NULL,*error=NULL; uint64_t exported_rows=0;
    CHECK(database_copy(db,&a,0,6,1,1,&clipboard,&error)); CHECK(!strcmp(clipboard,"-99.9\n-1.2\n-0.1\n0.0\n0.1\n1.2")); free(clipboard);
    CHECK(database_export_parquet(db,&a,"build/tests/generated/numeric-filtered.parquet",&exported_rows,&error)); CHECK(exported_rows==6);
    DistinctPage choices={0}; CHECK(database_distinct(db,&a,"Tiny","",-1,NULL,NULL,0,-1,&choices,&error)); CHECK(choices.total==6); distinct_free(&choices);
    CHECK(analysis_filter_set(&a,"tiny",FILTER_GREATER,"1.5",&error)); Page p=fetch(db,&a,0,-1); CHECK(p.total==1 && a.filter_number.kind==NUM_DECIMAL && a.filters[0].number.kind==NUM_I8); page_free(&p);
    Analysis copy=analysis_copy(&a); CHECK(analysis_filter_move(&a,1,0) && a.filter_number.kind==NUM_I8 && a.filters[0].number.kind==NUM_DECIMAL);
    analysis_filter_remove(&a,"tiny"); CHECK(a.filter_number.kind==NUM_DECIMAL); analysis_free(&copy);
    analysis_filters_clear(&a); CHECK(analysis_filter_set(&a,"Money",FILTER_EQUAL,"0); DROP TABLE fixture;--",&error));
    CHECK(!database_fetch(db,&a,0,-1,&p,&error)); CHECK(error && strstr(error,"Enter a number")); page_free(&p); free(error); error=NULL;
    analysis_filters_clear(&a);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"Tiny",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Money",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"Money",AGG_SUM,&error));
    pivot.fields[PIVOT_FILTERS][0].filter=FILTER_LESS; free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup("1.21");
    CHECK(database_pivot(db,&a,&pivot,0,-1,&p,&error)); CHECK(p.total==6 && p.sql && strstr(p.sql,"'1.3'")); page_free(&p);
    DistinctPage d={0}; CHECK(database_pivot_distinct(db,&a,&pivot,"Tiny","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==6); distinct_free(&d); pivot_free(&pivot);
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Money",AGG_COUNT,&error)); CHECK(database_pivot(db,&a,&pivot,0,-1,&p,&error));
    CHECK(p.cells[0].number==8 && !strcmp(p.columns[0].caption,"COUNT(*) [Money]") && strstr(p.sql,"COUNT(*)")); page_free(&p); pivot_free(&pivot); analysis_free(&a);
    remove("build/tests/generated/numeric.duckdb");
    execute(db,"ATTACH 'build/tests/generated/numeric.duckdb' AS numeric_fixture; CREATE TABLE numeric_fixture.huge AS SELECT value::HUGEINT AS Value FROM (VALUES ('-170141183460469231731687303715884105728'),('-170141183460469231731687303715884105727'),('-1'),('0'),('1'),('170141183460469231731687303715884105726'),('170141183460469231731687303715884105727'),(NULL)) t(value); CREATE TABLE numeric_fixture.unsigned_values AS SELECT value::UHUGEINT AS Value FROM (VALUES ('0'),('1'),('340282366920938463463374607431768211454'),('340282366920938463463374607431768211455'),(NULL)) t(value); CREATE TABLE numeric_fixture.nested AS SELECT {'items':[170141183460469231731687303715884105727::HUGEINT]} AS detail; DETACH numeric_fixture");
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/numeric.duckdb"),.table=dx_strdup("huge")};
    numeric_count(db,&a,"Value",FILTER_EQUAL,"170141183460469231731687303715884105727",1);
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/numeric-filtered.parquet",&exported_rows,&error)); CHECK(!exported_rows && error && strstr(error,"128-bit")); free(error); error=NULL;
    numeric_count(db,&a,"Value",FILTER_LESS,"170141183460469231731687303715884105728",7);
    numeric_count(db,&a,"Value",FILTER_GREATER,"170141183460469231731687303715884105728",0);
    numeric_count(db,&a,"Value",FILTER_GREATER,"-170141183460469231731687303715884105729",7);
    numeric_count(db,&a,"Value",FILTER_GREATER,"170141183460469231731687303715884105726.75",1);
    numeric_count(db,&a,"Value",FILTER_LESS,"170141183460469231731687303715884105726.75",6);
    numeric_count(db,&a,"Value",FILTER_GREATER,"1.5",2); numeric_count(db,&a,"Value",FILTER_LESS,"1.5",5); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/numeric.duckdb"),.table=dx_strdup("unsigned_values")};
    numeric_count(db,&a,"Value",FILTER_GREATER,"340282366920938463463374607431768211454",1);
    numeric_count(db,&a,"Value",FILTER_LESS,"340282366920938463463374607431768211456",4);
    numeric_count(db,&a,"Value",FILTER_GREATER,"-0.0001",4); numeric_count(db,&a,"Value",FILTER_LESS,"1.5",2); numeric_count(db,&a,"Value",FILTER_EQUAL,"1.5",0); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/numeric.duckdb"),.table=dx_strdup("nested")};
    CHECK(!database_export_parquet(db,&a,"build/tests/generated/numeric-filtered.parquet",&exported_rows,&error)); CHECK(error && strstr(error,"detail")); free(error); error=NULL; analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric-filtered.parquet")}; p=fetch(db,&a,0,-1); CHECK(p.total==6 && p.columns[1].type==DUCKDB_TYPE_DECIMAL); page_free(&p); analysis_free(&a);
    execute(db,"COPY (SELECT value::DECIMAL(38,38) AS Value FROM (VALUES ('-0.99999999999999999999999999999999999999'),('-0.5'),('0'),('0.00000000000000000000000000000000000001'),('0.5'),('0.99999999999999999999999999999999999999'),(NULL)) t(value)) TO 'build/tests/generated/numeric-precise.parquet' (FORMAT PARQUET)");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric-precise.parquet")};
    numeric_count(db,&a,"Value",FILTER_LESS,"0.999999999999999999999999999999999999999",6);
    numeric_count(db,&a,"Value",FILTER_EQUAL,"0.999999999999999999999999999999999999999",0);
    numeric_count(db,&a,"Value",FILTER_GREATER,"-0.999999999999999999999999999999999999999",6);
    numeric_count(db,&a,"Value",FILTER_GREATER,"1e-39",3); numeric_count(db,&a,"Value",FILTER_LESS,"1e-39",3); analysis_free(&a);
    execute(db,"COPY (SELECT value::FLOAT AS Value FROM (VALUES (1.0),(1.00000012),(NULL)) t(value)) TO 'build/tests/generated/numeric-float.parquet' (FORMAT PARQUET)");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric-float.parquet")};
    numeric_count(db,&a,"Value",FILTER_LESS,"1.00000001",1); numeric_count(db,&a,"Value",FILTER_GREATER,"1.00000001",1); analysis_free(&a);
    /* Independent wide-decimal reference: compare every predicate around source quanta. */
    execute(db,"COPY (SELECT (i*7/1000.0)::DECIMAL(6,3) AS Value FROM range(-20,21) t(i) UNION ALL SELECT NULL) TO 'build/tests/generated/numeric-matrix.parquet' (FORMAT PARQUET)");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric-matrix.parquet")};
    for (int threshold=-30;threshold<=30;threshold++) for (int op=FILTER_EQUAL;op<=FILTER_LESS;op++) {
        char value[40]; snprintf(value,sizeof(value),"%.4f",threshold*13/10000.0);
        char *sql=dx_printf("SELECT count(*) FROM read_parquet('build/tests/generated/numeric-matrix.parquet') WHERE CAST(Value AS DECIMAL(38,9)) %s CAST('%s' AS DECIMAL(38,9))",op==FILTER_EQUAL ? "=" : op==FILTER_GREATER ? ">" : "<",value);
        duckdb_result result={0}; CHECK(database_execute(db,sql,&result,&error)); free(sql); uint64_t expected=duckdb_value_uint64(&result,0,0); duckdb_destroy_result(&result);
        numeric_count(db,&a,"Value",(FilterOp)op,value,expected);
    }
    analysis_free(&a);
    /* A replaced file must invalidate bound schema even inside one second. */
    execute(db,"COPY (SELECT 1::TINYINT AS Value) TO 'build/tests/generated/numeric-replaced.parquet' (FORMAT PARQUET)");
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric-replaced.parquet")};
    numeric_count(db,&a,"Value",FILTER_EQUAL,"1.5",0); CHECK(a.filter_number.kind==NUM_I8);
    char *old_key=dx_strdup(db->schema_key[0]);
    execute(db,"COPY (SELECT '1.5' AS Value) TO 'build/tests/generated/numeric-replaced.parquet' (FORMAT PARQUET)");
    numeric_count(db,&a,"Value",FILTER_EQUAL,"1.5",1); CHECK(a.filter_number.kind==NUM_NONE && strcmp(old_key,db->schema_key[0])); free(old_key); analysis_free(&a);
    /* Grouped input and local pivot filters use different source/output shapes. */
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric.parquet"),.group_column=dx_strdup("Tiny"),.measure_column=dx_strdup("Money"),.aggregate=AGG_SUM};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"Tiny",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Value",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"Value",AGG_SUM,&error));
    pivot.fields[PIVOT_FILTERS][0].filter=FILTER_LESS; free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup("1.21");
    CHECK(database_pivot(db,&a,&pivot,0,-1,&p,&error)); CHECK(p.total==6); page_free(&p); pivot_free(&pivot); analysis_free(&a); free(error);
}
static void test_million(Database *db) {
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet"),.sort_column=dx_strdup("ID")};
    double start=dx_millis(); Page p=fetch(db,&a,0,-1); double first=dx_millis()-start;
    CHECK(p.total==1000000 && p.row_count==DX_PAGE_ROWS); CHECK(p.bytes<100000);
    page_free(&p); start=dx_millis(); p=fetch(db,&a,999744,1000000);
    CHECK(p.row_count==DX_PAGE_ROWS); CHECK(!strcmp(p.cells[0].text,"999744")); CHECK(!strcmp(p.cells[255*3].text,"999999"));
    printf("Generated 1M rows: first %.2f ms, last page %.2f ms, page %zu bytes\n",first,dx_millis()-start,p.bytes);
    page_free(&p); analysis_free(&a);
}
static void test_sort_order(Database *db) {
    execute(db,"COPY (SELECT * FROM (VALUES (7,'a',2),(2,'a',2),(5,'a',NULL),(4,'b',3),(1,'b',1),(3,NULL,4),(6,NULL,NULL)) t(ID,GroupKey,Amount)) TO 'build/tests/generated/sort-keys.parquet' (FORMAT PARQUET)");
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/sort-keys.parquet")}; char *error=NULL;
    CHECK(analysis_sort_set(&a,"GroupKey",false,true,&error)); CHECK(analysis_sort_set(&a,"Amount",true,true,&error)); CHECK(analysis_sort_set(&a,"ID",false,true,&error)); CHECK(analysis_sorts_valid(&a,&error));
    char *sql=analysis_sql(&a,true); CHECK(strstr(sql,"ORDER BY \"GroupKey\" ASC NULLS LAST, \"Amount\" DESC NULLS LAST, \"ID\" ASC NULLS LAST")); free(sql);
    Page p=fetch(db,&a,0,-1); int expected[]={2,7,5,4,1,3,6}; for (size_t i=0;i<7;i++) CHECK(p.cells[i*3].number==expected[i]);
    CHECK(p.columns[0].sort_priority==3 && p.columns[1].sort_priority==1 && p.columns[2].sort_priority==2 && p.columns[2].sort_descending); page_free(&p);
    char *text=NULL; CHECK(database_copy(db,&a,0,7,0,1,&text,&error)); CHECK(!strcmp(text,"2\n7\n5\n4\n1\n3\n6")); free(text);
    Analysis copy=analysis_copy(&a); CHECK(analysis_sorts_equal(&a,&copy));
    CHECK(analysis_sort_set(&a,"groupkey",true,true,&error)); CHECK(a.sort_count==2 && a.descending && !analysis_sorts_equal(&a,&copy));
    p=fetch(db,&a,0,-1); CHECK(p.cells[0].number==4 && p.cells[3].number==1 && p.cells[18].number==6); page_free(&p);
    CHECK(analysis_sort_move(&a,1,0)); p=fetch(db,&a,0,-1); CHECK(p.cells[0].number==3 && p.cells[3].number==4 && p.cells[6].number==2); page_free(&p);
    analysis_sort_remove(&a,0); CHECK(!strcmp(a.sort_column,"GroupKey") && a.sort_count==1 && !strcmp(a.sorts[0].column,"ID"));
    CHECK(!analysis_sort_move(&a,3,0)); analysis_sort_remove(&a,7); CHECK(a.sort_count==1);
    CHECK(analysis_sort_set(&a,"Amount",false,false,&error)); CHECK(!a.sort_count && !strcmp(a.sort_column,"Amount") && !strcmp(copy.sort_column,"GroupKey"));
    DistinctPage choices={0}; CHECK(database_distinct(db,&copy,"GroupKey","",-1,NULL,NULL,0,-1,&choices,&error)); CHECK(choices.total==3 && choices.values[0].is_null); distinct_free(&choices);
    uint64_t rows=0; CHECK(database_export_parquet(db,&copy,"build/tests/generated/sorted-export.parquet",&rows,&error) && rows==7);
    Analysis exported={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/sorted-export.parquet")}; p=fetch(db,&exported,0,-1); for (size_t i=0;i<7;i++) CHECK(p.cells[i*3].number==expected[i]); page_free(&p); analysis_free(&exported);
    analysis_free(&a); analysis_free(&copy);
    for (size_t i=0;i<DX_ANALYSIS_SORTS;i++) { char name[8]; snprintf(name,sizeof(name),"field%zu",i); CHECK(analysis_sort_set(&a,name,false,true,&error)); }
    CHECK(!analysis_sort_set(&a,"overflow",false,true,&error)); free(error); error=NULL; CHECK(analysis_sorts_valid(&a,&error));
    analysis_sort_remove(&a,0); CHECK(!strcmp(a.sort_column,"field1") && a.sort_count==6); analysis_sorts_clear(&a); CHECK(!a.sort_column && !a.sort_count); analysis_free(&a); free(error);
}
static bool cancel_page(void *context) { unsigned *calls=context; return ++*calls>=4; }
static void test_prepared_pages(Database *db) {
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet"),.sort_column=dx_strdup("ID")}; char *error=NULL;
    uint64_t prepares=db->page_prepares; Page page=fetch(db,&a,0,1000000); CHECK(db->page_prepares==prepares+1); page_free(&page);
    page=fetch(db,&a,256,1000000); CHECK(db->page_prepares==prepares+1 && page.offset==256 && !strcmp(page.cells[0].text,"256")); page_free(&page);
    page=fetch(db,&a,900000,1000000); CHECK(db->page_prepares==prepares+1 && !strcmp(page.cells[0].text,"900000")); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error)); page=fetch(db,&a,0,-1); CHECK(page.total==2); page_free(&page); prepares=db->page_prepares;
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"Ankara",&error)); page=fetch(db,&a,0,-1); CHECK(db->page_prepares==prepares && page.total==1 && !strcmp(page.cells[0].text,"2")); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/timestamps.parquet")};
    CHECK(analysis_filter_set(&a,"Stamp",FILTER_GREATER,"invalid timestamp",&error)); CHECK(!database_fetch(db,&a,0,-1,&page,&error)); CHECK(error); free(error); error=NULL; page_free(&page); prepares=db->page_prepares;
    CHECK(analysis_filter_set(&a,"Stamp",FILTER_GREATER,"2026-01-02 03:04:06",&error)); page=fetch(db,&a,0,-1); CHECK(db->page_prepares==prepares && page.total==2); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DEMO,.sort_column=dx_strdup("Amount")}; page=fetch(db,&a,0,1000000); page_free(&page); prepares=db->page_prepares;
    unsigned calls=0; db->cancelled=cancel_page; db->cancel_context=&calls;
    CHECK(!database_fetch_window(db,&a,256,1000000,&page,&error)); CHECK(error && strstr(error,"cancelled") && calls>=4); free(error); error=NULL; page_free(&page);
    db->cancelled=NULL; db->cancel_context=NULL; page=fetch(db,&a,256,1000000); CHECK(page.row_count==256 && db->page_prepares==prepares); page_free(&page);
    CHECK(database_refresh(db,&error)); CHECK(!db->page_statement && !db->schema_statement[0] && !db->schema_statement[1] && !db->pivot_key);
    page=fetch(db,&a,0,1000000); CHECK(db->page_prepares==prepares+1); page_free(&page); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/fixture.duckdb")}; page=fetch(db,&a,0,-1); CHECK(page.relation_count==2); page_free(&page);
    CHECK(database_refresh(db,&error)); CHECK(!db->attached_path && !db->relation_count); page=fetch(db,&a,0,-1); CHECK(page.total==5 && page.relation_count==2); page_free(&page); analysis_free(&a);
    Analysis invalid={.sort_count=DX_ANALYSIS_SORTS}; CHECK(!database_fetch_window(db,&invalid,0,-1,&page,&error)); CHECK(error && strstr(error,"Invalid sort")); free(error); page_free(&page);
}
static QueryReply *wait_reply(QueryWorker *w) {
    double until=dx_millis()+10000;
    while (dx_millis()<until) { QueryReply *r=worker_poll(w); if (r) return r; dx_sleep_ms(1); }
    CHECK(!"Worker timed out"); return NULL;
}
static QueryJob *job(uint64_t id,SourceKind kind,const char *path) {
    QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=JOB_PAGE; j->analysis.kind=kind;
    j->analysis.path=dx_strdup(path); j->known_total=-1; return j;
}
static void test_clipboard_ownership(void) {
    char *error=NULL; CellClipboard copy={0}; clipboard_free(NULL); clipboard_free(&copy);
    CHECK(clipboard_init(&copy,2,2,&error) && copy.capacity==4);
    for (size_t i=0;i<4;i++) CHECK(clipboard_set(&copy,i/2,i%2,i==1 ? "" : "Owned slot",&error));
    copy.rows=SIZE_MAX; copy.columns=SIZE_MAX;
    CHECK(!clipboard_set(&copy,0,0,"Cannot index malformed dimensions",&error)); free(error); error=NULL;
    CHECK(!clipboard_tsv(&copy,&error)); free(error); error=NULL;
    clipboard_free(&copy); CHECK(!copy.values && !copy.rows && !copy.columns && !copy.bytes && !copy.capacity);
    CHECK(clipboard_init(&copy,1,4,&error)); CHECK(clipboard_set(&copy,0,3,"Owned beyond narrowed logical shape",&error));
    copy.columns=1; char *text=clipboard_tsv(&copy,&error); CHECK(text && !*text); free(text); clipboard_free(&copy);
    CopyRequest *request=dx_alloc(sizeof(*request)); CHECK(clipboard_init(&request->cells,1,3,&error));
    for (size_t i=0;i<3;i++) CHECK(clipboard_set(&request->cells,0,i,"Nested owned copy text",&error));
    request->cells.rows=SIZE_MAX; request->cells.columns=0; request->count=SIZE_MAX; request->guard_count=SIZE_MAX;
    copy_request_free(request);
    copy=(CellClipboard){.rows=SIZE_MAX,.columns=SIZE_MAX,.capacity=SIZE_MAX};
    CHECK(!clipboard_set(&copy,0,0,"No backing slots",&error)); free(error); error=NULL;
    CHECK(!clipboard_tsv(&copy,&error)); free(error); clipboard_free(&copy);
}
static void test_clipboard(Database *db) {
    test_clipboard_ownership();
    CellClipboard copy={0}; char *error=NULL,*text=NULL;
    CellStore manual={0}; ObjectStore objects={0};
    CHECK(cells_set(&manual,1000,5,"Far note",&error)); CHECK(objects_add(&objects,10,8,"Pivot",&error)>=0);
    uint64_t r0=0,r1=UINT64_MAX; size_t c0=0,c1=0;
    CHECK(clipboard_bounds(&manual,&objects,5,3,&r0,&r1,&c0,&c1,&error) && r1==4 && c1==0);
    r1=UINT64_MAX; c0=c1=5; CHECK(clipboard_bounds(&manual,&objects,5,3,&r0,&r1,&c0,&c1,&error) && r1==1000);
    r1=UINT64_MAX; c0=c1=8; CHECK(clipboard_bounds(&manual,&objects,5,3,&r0,&r1,&c0,&c1,&error) && r1==14);
    r1=2; c0=0; c1=SIZE_MAX; CHECK(clipboard_bounds(&manual,&objects,5,3,&r0,&r1,&c0,&c1,&error) && c1==2 && r1==2);
    r1=UINT64_MAX; c1=SIZE_MAX; CHECK(clipboard_bounds(&manual,&objects,5,3,&r0,&r1,&c0,&c1,&error) && r1==1000 && c1==11);
    r1=UINT64_MAX; c1=0; CHECK(!clipboard_bounds(&manual,&objects,UINT64_MAX,3,&r0,&r1,&c0,&c1,&error)); free(error); error=NULL;
    cells_free(&manual); objects_free(&objects);
    CHECK(clipboard_init(&copy,2,4,&error));
    CHECK(clipboard_set(&copy,0,1,"NULL",&error) && clipboard_set(&copy,0,2,"",&error) && clipboard_set(&copy,0,3,"\"\"",&error));
    CHECK(clipboard_set(&copy,1,0,"İstanbul\tα\nquoted \"value\"",&error));
    text=clipboard_tsv(&copy,&error); CHECK(text && !strcmp(text,"\tNULL\t\"\"\t\"\"\"\"\"\"\n\"İstanbul\tα\nquoted \"\"value\"\"\"\t\t\t")); free(text);
    CHECK(clipboard_set(&copy,0,1,NULL,&error)); CHECK(!copy.values[1] && copy.values[2] && !*copy.values[2]); clipboard_free(&copy);
    CHECK(!clipboard_init(&copy,20001,1,&error)); free(error); error=NULL; clipboard_free(&copy);
    CHECK(clipboard_init(&copy,1,2,&error)); char *large=dx_alloc(DX_CLIPBOARD_BYTES/2+1); memset(large,'"',DX_CLIPBOARD_BYTES/2);
    CHECK(clipboard_set(&copy,0,0,large,&error)); CHECK(!clipboard_tsv(&copy,&error)); free(error); error=NULL;
    CHECK(!clipboard_set(&copy,0,1,large,&error)); free(error); error=NULL; free(large); clipboard_free(&copy);
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")}; Page page={0};
    uint64_t counts=db->count_queries; CHECK(database_copy_page(db,&a,NULL,0,6,1,1,&page,&error));
    CHECK(page.column_count==1 && page.row_count==6 && page.cells[0].is_null && !strcmp(page.cells[1].text,"NULL") && !page.cells[2].is_null && !*page.cells[2].text && db->count_queries==counts); page_free(&page);
    CHECK(database_copy(db,&a,0,3,1,1,&text,&error)); CHECK(!strcmp(text,"\nNULL\n\"\"")); free(text);
    CHECK(database_copy_page(db,&a,NULL,4,5,0,2,&page,&error)); CHECK(page.row_count==2 && page.column_count==2); page_free(&page);
    CHECK(!database_copy_page(db,&a,NULL,0,20001,0,1,&page,&error)); free(error); error=NULL;
    CHECK(!database_copy_page(db,&a,NULL,0,2,90,1,&page,&error)); free(error); error=NULL;
    analysis_free(&a);
}
static void test_worker(void) {
    QueryWorker w; CHECK(worker_start(&w));
    QueryJob *j=job(1,SOURCE_DEMO,""); j->analysis.sort_column=dx_strdup("Amount"); worker_submit(&w,j);
    dx_sleep_ms(2); worker_submit(&w,job(2,SOURCE_CSV,"tests/fixtures/transactions.csv"));
    QueryReply *r=wait_reply(&w); CHECK(r->id==2 && !r->error && r->page.total==5); reply_free(r);
    worker_submit(&w,job(3,SOURCE_PARQUET,"build/tests/generated/missing.parquet"));
    r=wait_reply(&w); CHECK(r->error && r->id==3); reply_free(r);
    worker_submit(&w,job(4,SOURCE_DEMO,"")); dx_sleep_ms(1); worker_cancel(&w);
    worker_submit(&w,job(6,SOURCE_CSV,"tests/fixtures/transactions.csv"));
    r=wait_reply(&w); CHECK(r->id==6 && !r->error && r->page.total==5); reply_free(r);
    worker_submit(&w,job(7,SOURCE_PARQUET,"build/tests/generated/million.parquet"));
    r=wait_reply(&w); CHECK(r->id==7 && !r->error && r->page.count_pending && r->page.total==DX_PAGE_ROWS); reply_free(r);
    j=job(8,SOURCE_PARQUET,"build/tests/generated/million.parquet"); j->kind=JOB_COUNT; worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==8 && !r->error && r->counted_rows==1000000 && !r->page.column_count); reply_free(r);
    j=job(9,SOURCE_DEMO,""); j->kind=JOB_COUNT; worker_submit(&w,j); worker_submit(&w,job(10,SOURCE_CSV,"tests/fixtures/transactions.csv"));
    r=wait_reply(&w); CHECK(r->id==10 && !r->error && r->page.total==5 && !r->page.count_pending); reply_free(r);
    j=job(11,SOURCE_CSV,"tests/fixtures/null-text.csv"); j->kind=JOB_RANGE_COPY; j->copy=dx_alloc(sizeof(*j->copy));
    char *error=NULL; CHECK(clipboard_init(&j->copy->cells,8,4,&error));
    CHECK(clipboard_set(&j->copy->cells,0,0,"Snapshot title",&error)); CHECK(clipboard_set(&j->copy->cells,1,3,"Adjacent note",&error));
    j->copy->patches[0]=(CopyPatch){.first=0,.rows=6,.column=1,.columns=1,.target_row=1,.target_column=1};
    j->copy->patches[1]=(CopyPatch){.is_pivot=true,.first=0,.rows=6,.column=0,.columns=1,.target_row=1,.target_column=2}; j->copy->count=2;
    CHECK(pivot_assign(&j->copy->patches[1].pivot,PIVOT_ROWS,"ID",AGG_SUM,&error)); CHECK(pivot_assign(&j->copy->patches[1].pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    worker_submit(&w,j); r=wait_reply(&w); CHECK(r->id==11 && !r->error && r->clipboard);
    CHECK(strstr(r->clipboard,"Snapshot title\t\t\t\n\t\t1\tAdjacent note\n\tNULL\t2\t\n\t\"\"\t3\t")); reply_free(r);
    j=job(12,SOURCE_PARQUET,"build/tests/generated/missing.parquet"); j->kind=JOB_RANGE_COPY; j->copy=dx_alloc(sizeof(*j->copy)); CHECK(clipboard_init(&j->copy->cells,1,1,&error));
    j->copy->patches[0]=(CopyPatch){.rows=1,.columns=1}; j->copy->count=1; worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==12 && r->error && !r->clipboard); reply_free(r); free(error); error=NULL;
    j=job(13,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_STATISTICS; j->rows=5; j->first_col=2; j->cols=1; worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==13 && !r->error && r->statistics.numeric==5 && !strcmp(r->statistics.sum,"145.25") && !r->page.column_count); reply_free(r);
    j=job(14,SOURCE_DEMO,""); j->kind=JOB_STATISTICS; j->rows=1000000; j->cols=6; worker_submit(&w,j); dx_sleep_ms(2);
    worker_submit(&w,job(15,SOURCE_CSV,"tests/fixtures/transactions.csv"));
    r=wait_reply(&w); CHECK(r->id==15 && !r->error && r->page.total==5 && !r->statistics.sum); reply_free(r);
    j=job(16,SOURCE_PARQUET,"build/tests/generated/missing.parquet"); j->kind=JOB_STATISTICS; j->rows=5; j->cols=1; worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==16 && r->error && !r->statistics.sum); reply_free(r);
    j=job(17,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_STATISTICS; j->rows=64; j->first_col=1; j->cols=1; j->object_id=99; j->revision=5;
    CHECK(pivot_assign(&j->pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&j->pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==17 && !r->error && r->object_id==99 && r->revision==5 && r->statistics.cells==4 && !strcmp(r->statistics.sum,"145.25")); reply_free(r); free(error);
    j=job(18,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_STATISTICS; j->statistics=dx_alloc(sizeof(*j->statistics));
    j->statistics->cells=10; j->statistics->count=1; j->statistics->ranges[0]=(StatisticsRange){.rows=5,.column=2,.columns=1};
    error=NULL; CHECK(statistics_request_text(j->statistics,"12.5",&error)); worker_submit(&w,j);
    r=wait_reply(&w); CHECK(r->id==18 && !r->error && r->statistics.cells==10 && r->statistics.filled==6 && r->statistics.numeric==5 && r->statistics.distinct==6 && !strcmp(r->statistics.sum,"145.25")); reply_free(r); free(error);
    error=NULL; j=job(19,SOURCE_PARQUET,"build/tests/generated/million.parquet"); j->kind=JOB_PIVOT; j->is_table=true; j->object_id=73; j->revision=9; j->offset=999744; j->known_total=1000000;
    worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==19 && !r->error && r->object_id==73 && r->revision==9 && r->page.total==1000000 && !r->page.count_pending && r->page.offset==999744 && r->page.row_count==256 && r->page.bytes<=4*1024*1024); reply_free(r);
    j=job(20,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_DISTINCT; j->is_table=true; j->object_id=73; j->column=dx_strdup("City"); j->search=dx_strdup("stan"); j->date_level=-1;
    worker_submit(&w,j); r=wait_reply(&w); CHECK(r->id==20 && !r->error && r->distinct.total==1 && !strcmp(r->distinct.values[0].value,"İstanbul")); reply_free(r);
    j=job(21,SOURCE_PARQUET,"build/tests/generated/missing.parquet"); j->kind=JOB_RANGE_COPY; j->copy=dx_alloc(sizeof(*j->copy)); CHECK(clipboard_init(&j->copy->cells,2,2,&error));
    j->copy->count=2;
    Analysis table_csv={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")},table_parquet={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/null-text.parquet")};
    CHECK(analysis_filter_set(&table_csv,"City",FILTER_EQUAL,"İstanbul",&error));
    size_t csv_source=source_snapshot_bind(&j->copy->sources,1,&table_csv,&error),parquet_source=source_snapshot_bind(&j->copy->sources,2,&table_parquet,&error);
    CHECK(csv_source==1 && parquet_source==2);
    j->copy->patches[0]=(CopyPatch){.source=csv_source,.rows=2,.column=2,.columns=1};
    j->copy->patches[1]=(CopyPatch){.source=parquet_source,.rows=2,.column=1,.columns=1,.target_column=1};
    worker_submit(&w,j); r=wait_reply(&w); CHECK(r->id==21 && !r->error && !strcmp(r->clipboard,"12.5\t\n7.25\tNULL")); reply_free(r);
    j=job(22,SOURCE_PARQUET,"build/tests/generated/missing.parquet"); j->kind=JOB_STATISTICS; j->statistics=dx_alloc(sizeof(*j->statistics)); j->statistics->cells=100; j->statistics->count=3;
    csv_source=source_snapshot_bind(&j->statistics->sources,1,&table_csv,&error); parquet_source=source_snapshot_bind(&j->statistics->sources,2,&table_parquet,&error);
    CHECK(csv_source==1 && parquet_source==2 && source_snapshot_bind(&j->statistics->sources,1,&table_csv,&error)==csv_source && j->statistics->sources.count==2);
    j->statistics->ranges[0]=(StatisticsRange){.source=csv_source,.rows=5,.column=2,.columns=1};
    j->statistics->ranges[1]=(StatisticsRange){.source=parquet_source,.rows=6,.column=2,.columns=1};
    j->statistics->ranges[2]=(StatisticsRange){.is_pivot=true,.source=csv_source,.rows=64,.column=1,.columns=1};
    analysis_free(&table_csv); analysis_free(&table_parquet); /* Worker owns independent definitions. */
    CHECK(pivot_assign(&j->statistics->ranges[2].pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&j->statistics->ranges[2].pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(statistics_request_text(j->statistics,"NULL",&error));
    worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==22 && !r->error && r->statistics.cells==100 && r->statistics.filled==10 && r->statistics.numeric==9 && r->statistics.sum && fabs(strtod(r->statistics.sum,NULL)-99.5)<1e-12); reply_free(r);
    j=job(23,SOURCE_PARQUET,"build/tests/generated/null-text.parquet"); j->kind=JOB_STATISTICS; j->is_table=true; j->object_id=73; j->rows=6; j->first_col=2; j->cols=1;
    worker_submit(&w,j); r=wait_reply(&w); CHECK(r->id==23 && !r->error && r->statistics.numeric==6 && !strcmp(r->statistics.sum,"60")); reply_free(r); free(error);
    j=job(24,SOURCE_PARQUET,"build/tests/generated/million.parquet"); j->kind=JOB_PIVOT; j->is_table=true; j->object_id=73; j->revision=9;
    worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==24 && !r->error && r->page.count_pending && r->page.total==256 && r->page.row_count==256 && r->page.bytes<=4*1024*1024); reply_free(r);
    j=job(25,SOURCE_PARQUET,"build/tests/generated/million.parquet"); j->kind=JOB_COUNT; j->is_table=true; j->object_id=73; j->revision=9;
    worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==25 && !r->error && r->object_id==73 && r->revision==9 && r->counted_rows==1000000 && !r->page.column_count); reply_free(r);
    error=NULL; j=job(26,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_EXPORT; j->is_table=true; j->object_id=73; j->revision=10; j->path=dx_strdup("build/tests/generated/worker-table-export.parquet");
    CHECK(analysis_filter_set(&j->analysis,"City",FILTER_EQUAL,"İstanbul",&error)); worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==26 && !r->error && r->object_id==73 && r->revision==10 && r->exported_rows==2 && !strcmp(r->path,"build/tests/generated/worker-table-export.parquet")); reply_free(r);
    j=job(27,SOURCE_CSV,"tests/fixtures/transactions.csv"); j->kind=JOB_EXPORT; j->object_id=98; j->revision=11; j->path=dx_strdup("build/tests/generated/worker-pivot-export.parquet");
    CHECK(pivot_assign(&j->pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&j->pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); worker_submit(&w,j); r=wait_reply(&w);
    CHECK(r->id==27 && !r->error && r->object_id==98 && r->revision==11 && r->exported_rows==4); reply_free(r); free(error);
    worker_stop(&w);
}
static void guard_fixture(const char *path,const char *text) {
    FILE *file=fopen(path,"wb"); size_t length=strlen(text); CHECK(file && fwrite(text,1,length,file)==length && !fclose(file));
}
typedef struct { Database *db; const char *path; uint64_t queries,stamp; bool changed; } WorkerSourceTouch;
static bool touch_worker_source(void *context) {
    WorkerSourceTouch *touch=context;
    if (!touch->changed && touch->db->query_count>touch->queries) {
        FILE *file=fopen(touch->path,"ab"); CHECK(file && fputc('\n',file)!=EOF && !fclose(file));
        CHECK(dx_file_stamp(touch->path)!=touch->stamp); touch->changed=true;
    }
    return false;
}
static void test_worker_source_guards(void) {
    char *directory=dx_temp_directory(); CHECK(directory); char *source=dx_printf("%s/source.csv",directory),*other=dx_printf("%s/table.csv",directory),*destination=dx_printf("%s/export.parquet",directory),*error=NULL;
    guard_fixture(source,"id,value\n1,original\n2,original\n3,original\n"); guard_fixture(other,"id,value\n7,table\n8,table\n"); guard_fixture(destination,"Preserve destination");
    QueryWorker worker; CHECK(worker_start(&worker)); worker_submit(&worker,job(1,SOURCE_CSV,source)); QueryReply *reply=wait_reply(&worker);
    CHECK(!reply->error && reply->page.records.stamp && reply->page.records.ids && !strcmp(reply->page.cells[0].text,"1"));
    uint64_t old_stamp=reply->page.records.stamp; reply_free(reply);
    guard_fixture(source,"id,value\n100,replacement\n101,replacement\n"); CHECK(dx_file_stamp(source)!=old_stamp);
    for (unsigned operation=0;operation<4;operation++) {
        QueryJob *request=job(2+operation,SOURCE_CSV,source); request->expected_source_stamp=old_stamp; request->rows=2; request->cols=1;
        if (operation==0) request->kind=JOB_COPY;
        else if (operation==1) request->kind=JOB_STATISTICS;
        else if (operation==2) { request->kind=JOB_EXPORT; request->path=dx_strdup(destination); }
        else { request->kind=JOB_STATISTICS; request->statistics=dx_alloc(sizeof(*request->statistics)); request->statistics->count=1; request->statistics->cells=2; request->statistics->ranges[0]=(StatisticsRange){.rows=2,.columns=1,.expected_source_stamp=old_stamp}; }
        worker_submit(&worker,request); reply=wait_reply(&worker);
        CHECK(reply->id==2+operation && reply->error && strstr(reply->error,"source changed") && !reply->clipboard && !reply->statistics.sum && !reply->exported_rows); reply_free(reply);
    }
    /* The stale owner contributes cached cells only; another owner needs SQL. */
    QueryJob *request=job(6,SOURCE_CSV,source); request->kind=JOB_RANGE_COPY; request->copy=dx_alloc(sizeof(*request->copy));
    CHECK(clipboard_init(&request->copy->cells,1,2,&error) && clipboard_set(&request->copy->cells,0,0,"1",&error));
    Analysis table={.kind=SOURCE_CSV,.path=other}; size_t binding=source_snapshot_bind(&request->copy->sources,9,&table,&error); CHECK(binding==1);
    request->copy->count=1; request->copy->patches[0]=(CopyPatch){.source=binding,.rows=1,.columns=1,.target_column=1};
    request->copy->guard_count=2; request->copy->guards[0]=(CopySourceGuard){0,old_stamp}; request->copy->guards[1]=(CopySourceGuard){binding,dx_file_stamp(other)};
    uint64_t queries=worker.db.query_count; worker_submit(&worker,request); reply=wait_reply(&worker);
    CHECK(reply->id==6 && reply->error && !reply->clipboard && worker.db.query_count==queries); reply_free(reply);
    /* Invalid count/index/duplicate/target overflow fail without engine work. */
    for (unsigned invalid=0;invalid<4;invalid++) {
        request=job(7+invalid,SOURCE_CSV,source); request->kind=JOB_RANGE_COPY; request->copy=dx_alloc(sizeof(*request->copy)); CHECK(clipboard_init(&request->copy->cells,1,1,&error));
        request->copy->guard_count=1; request->copy->guards[0]=(CopySourceGuard){0,dx_file_stamp(source)};
        if (invalid==0) request->copy->guard_count=DX_MAX_OBJECTS+2;
        else if (invalid==1) request->copy->guards[0].source=1;
        else if (invalid==2) { request->copy->guard_count=2; request->copy->guards[1]=request->copy->guards[0]; }
        else { request->copy->count=1; request->copy->patches[0]=(CopyPatch){.rows=1,.columns=1,.target_row=SIZE_MAX}; }
        queries=worker.db.query_count; worker_submit(&worker,request); reply=wait_reply(&worker);
        CHECK(reply->id==7+invalid && reply->error && strstr(reply->error,"Invalid clipboard") && !reply->clipboard && worker.db.query_count==queries); reply_free(reply);
    }
    request=job(11,SOURCE_CSV,source); request->refresh_source=true; worker_submit(&worker,request); reply=wait_reply(&worker);
    CHECK(!reply->error && reply->page.records.stamp!=old_stamp && !strcmp(reply->page.cells[0].text,"100")); uint64_t refreshed=reply->page.records.stamp; reply_free(reply);
    request=job(12,SOURCE_CSV,source); request->kind=JOB_COPY; request->rows=2; request->cols=1; request->expected_source_stamp=refreshed;
    worker_submit(&worker,request); reply=wait_reply(&worker); CHECK(!reply->error && !strcmp(reply->clipboard,"100\n101")); reply_free(reply);
    request=job(13,SOURCE_CSV,source); request->kind=JOB_STATISTICS; request->rows=2; request->cols=1; request->expected_source_stamp=refreshed;
    worker_submit(&worker,request); reply=wait_reply(&worker); CHECK(!reply->error && !strcmp(reply->statistics.sum,"201")); reply_free(reply);
    request=job(14,SOURCE_CSV,source); request->kind=JOB_RANGE_COPY; request->copy=dx_alloc(sizeof(*request->copy)); CHECK(clipboard_init(&request->copy->cells,1,2,&error) && clipboard_set(&request->copy->cells,0,0,"100",&error));
    binding=source_snapshot_bind(&request->copy->sources,9,&table,&error); CHECK(binding==1); request->copy->count=1; request->copy->patches[0]=(CopyPatch){.source=binding,.rows=1,.columns=1,.target_column=1};
    request->copy->guard_count=2; request->copy->guards[0]=(CopySourceGuard){0,refreshed}; request->copy->guards[1]=(CopySourceGuard){binding,dx_file_stamp(other)};
    worker_submit(&worker,request); reply=wait_reply(&worker); CHECK(!reply->error && !strcmp(reply->clipboard,"100\t7")); reply_free(reply);
    request=job(15,SOURCE_CSV,source); request->kind=JOB_COPY; request->rows=2; request->cols=1; CHECK(analysis_query_set(&request->analysis,"SELECT * FROM input",&error));
    worker_submit(&worker,request); reply=wait_reply(&worker); CHECK(!reply->error && !strcmp(reply->clipboard,"100\n101")); reply_free(reply);
    for (unsigned fresh=0;fresh<2;fresh++) {
        request=job(16+fresh,SOURCE_CSV,source); request->kind=JOB_PIVOT_COPY; request->rows=64; request->cols=2; request->expected_source_stamp=fresh ? refreshed : old_stamp;
        CHECK(pivot_assign(&request->pivot,PIVOT_ROWS,"value",AGG_SUM,&error) && pivot_assign(&request->pivot,PIVOT_VALUES,"id",AGG_MAX,&error));
        worker_submit(&worker,request); reply=wait_reply(&worker);
        CHECK(fresh ? !reply->error && reply->clipboard && !strcmp(reply->clipboard,"replacement\t101") : reply->error && strstr(reply->error,"source changed") && !reply->clipboard); reply_free(reply);
    }
    for (unsigned operation=0;operation<4;operation++) {
        bool fresh=operation>=2,distinct=operation%2;
        request=job(18+operation,SOURCE_CSV,source); request->kind=distinct ? JOB_DISTINCT : JOB_COUNT; request->expected_source_stamp=fresh ? refreshed : old_stamp;
        if (distinct) { request->column=dx_strdup("value"); request->search=dx_strdup(""); request->date_level=-1; }
        queries=worker.db.query_count; worker_submit(&worker,request); reply=wait_reply(&worker);
        if (!fresh) CHECK(reply->error && strstr(reply->error,"source changed") && !reply->counted_rows && !reply->distinct.count && !reply->distinct.values && worker.db.query_count==queries);
        else CHECK(!reply->error && (distinct ? reply->distinct.count==1 && !strcmp(reply->distinct.values[0].value,"replacement") : reply->counted_rows==2));
        reply_free(reply);
    }
    /* Override the idle connection's existing cancellation callback only in
       this private test. A false callback appends a harmless blank line after
       execution starts, proving the final check discards staged results. */
    for (unsigned distinct=0;distinct<2;distinct++) {
        WorkerSourceTouch touch={&worker.db,source,worker.db.query_count,dx_file_stamp(source),false};
        dx_lock(&worker.mutex); bool (*original)(void *)=worker.db.cancelled; void *original_context=worker.db.cancel_context;
        worker.db.cancelled=touch_worker_source; worker.db.cancel_context=&touch; dx_unlock(&worker.mutex);
        request=job(22+distinct,SOURCE_CSV,source); request->kind=distinct ? JOB_DISTINCT : JOB_COUNT; request->expected_source_stamp=touch.stamp;
        if (distinct) { request->column=dx_strdup("value"); request->search=dx_strdup(""); request->date_level=-1; }
        worker_submit(&worker,request); reply=wait_reply(&worker);
        dx_lock(&worker.mutex); worker.db.cancelled=original; worker.db.cancel_context=original_context; dx_unlock(&worker.mutex);
        CHECK(touch.changed && worker.db.query_count>touch.queries && reply->error && strstr(reply->error,"source changed") && !reply->counted_rows && !reply->distinct.count && !reply->distinct.values); reply_free(reply);
    }
    worker_stop(&worker);
    char preserved[32]={0}; FILE *file=fopen(destination,"rb"); CHECK(file && fread(preserved,1,sizeof(preserved)-1,file)==20 && fgetc(file)==EOF && !fclose(file) && !strcmp(preserved,"Preserve destination"));
    remove(source); remove(other); remove(destination); free(source); free(other); free(destination); dx_remove_temp_directory(directory); free(directory); free(error);
}
static void test_source_snapshots(void) {
    SourceSnapshot snapshot={0}; char *error=NULL;
    Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("one.csv")}; char *large=dx_alloc(3*1024*1024+1); memset(large,'x',3*1024*1024);
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,large,&error)); free(large);
    CHECK(source_snapshot_bind(&snapshot,1,&source,&error)==1); size_t bytes=snapshot.bytes; char *owned=snapshot.items[0].filter_value;
    CHECK(owned!=source.filter_value && source_snapshot_bind(&snapshot,1,&source,&error)==1 && snapshot.bytes==bytes && snapshot.items[0].filter_value==owned);
    free(source.path); source.path=dx_strdup("two.csv");
    CHECK(source_snapshot_bind(&snapshot,1,&source,&error)==SIZE_MAX && snapshot.count==1 && snapshot.bytes==bytes); free(error); error=NULL;
    CHECK(source_snapshot_bind(&snapshot,2,&source,&error)==2 && snapshot.count==2);
    CHECK(source_snapshot_bind(&snapshot,3,&source,&error)==SIZE_MAX && snapshot.count==2 && snapshot.bytes<8*1024*1024); free(error); error=NULL;
    analysis_free(&source); CHECK(!strcmp(snapshot.items[0].path,"one.csv") && !strcmp(snapshot.items[1].path,"two.csv"));
    source_snapshot_free(&snapshot); CHECK(!snapshot.count && !snapshot.bytes && !snapshot.items[0].filter_value);
    source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("small.csv")};
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) CHECK(source_snapshot_bind(&snapshot,i+1,&source,&error)==i+1);
    CHECK(source_snapshot_bind(&snapshot,100,&source,&error)==SIZE_MAX && snapshot.count==DX_MAX_OBJECTS); free(error); error=NULL;
    source_snapshot_free(&snapshot); analysis_free(&source); free(error);
}
static void test_table_windows(Database *db) {
    char *error=NULL; Page page={0}; Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")};
    CHECK(database_fetch_table(db,&source,999744,-1,&page,&error) && page.total==1000000 && !page.count_pending && page.row_count==256 && page.bytes<=4*1024*1024); page_free(&page);
    uint64_t counts=db->count_queries,queries=db->query_count;
    CHECK(database_fetch_table(db,&source,999999,1000000,&page,&error) && page.row_count==1 && !strcmp(page.cells[0].text,"999999")); page_free(&page);
    CHECK(db->count_queries==counts && db->query_count==queries+1);
    counts=db->count_queries; queries=db->query_count;
    CHECK(database_fetch_table_window(db,&source,0,-1,&page,&error));
    CHECK(page.count_pending && page.total==256 && page.row_count==256 && page.bytes<=4*1024*1024);
    CHECK(db->count_queries==counts && db->query_count==queries+1); page_free(&page);
    uint64_t total=0; CHECK(database_count(db,&source,&total,&error) && total==1000000 && db->count_queries==counts+1);
    CHECK(database_fetch_table_window(db,&source,999900,-1,&page,&error) && !page.count_pending && page.row_count==100 && page.total==1000000); page_free(&page);
    CHECK(database_fetch_table_window(db,&source,1001000,-1,&page,&error) && page.count_pending && !page.row_count && !page.total); page_free(&page);
    CHECK(database_fetch_table_window(db,&source,1001000,1000000,&page,&error) && !page.count_pending && !page.row_count && page.total==1000000 && db->count_queries==counts+1); page_free(&page);
    execute(db,"COPY (SELECT repeat('x',20000) AS value FROM range(256)) TO 'build/tests/generated/table-budget.parquet' (FORMAT PARQUET)");
    free(source.path); source.path=dx_strdup("build/tests/generated/table-budget.parquet");
    CHECK(!database_fetch_table(db,&source,0,-1,&page,&error) && error); page_free(&page); free(error); error=NULL;
    FILE *wide=fopen("build/tests/generated/table-wide.csv","wb"); CHECK(wide);
    for (int i=0;i<257;i++) fprintf(wide,"%sField%d",i ? "," : "",i); fputc('\n',wide);
    for (int i=0;i<257;i++) fprintf(wide,"%s%d",i ? "," : "",i); fputc('\n',wide); CHECK(!fclose(wide));
    analysis_free(&source); source=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("build/tests/generated/table-wide.csv")};
    CHECK(!database_fetch_table(db,&source,0,-1,&page,&error) && error && strstr(error,"256 columns")); page_free(&page); free(error); error=NULL;
    analysis_free(&source); source=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")};
    CHECK(database_fetch_table(db,&source,0,1000000,&page,&error) && page.row_count==256 && !page.count_pending); page_free(&page);
    analysis_free(&source); free(error);
}
static void test_available_values(Database *db) {
    char *error=NULL; Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet")}; DistinctPage d;
    CHECK(database_distinct(db,&a,"City","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==4 && d.count==4); distinct_free(&d);
    CHECK(database_distinct(db,&a,"City","stan",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==1 && !strcmp(d.values[0].value,"İstanbul")); distinct_free(&d);
    char **branches=NULL; size_t branch_count=0; bool mixed=false;
    const char *year="2026-01-01\t2027-01-01",*month="2026-02-01\t2026-03-01";
    CHECK(analysis_toggle_value(&branches,&branch_count,true,true,year));
    CHECK(!analysis_value_checked(branches,branch_count,true,true,month,&mixed) && !mixed);
    CHECK(analysis_toggle_value(&branches,&branch_count,true,true,month));
    CHECK(analysis_value_checked(branches,branch_count,true,true,month,&mixed));
    CHECK(analysis_value_checked(branches,branch_count,true,true,year,&mixed) && mixed);
    CHECK(analysis_toggle_value(&branches,&branch_count,true,true,year));
    CHECK(!analysis_value_checked(branches,branch_count,true,true,month,&mixed));
    for (size_t i=0;i<branch_count;i++) free(branches[i]); free(branches);
    branches=NULL; branch_count=0;
    CHECK(analysis_toggle_value(&branches,&branch_count,true,false,"literal\tvalue"));
    CHECK(!analysis_value_checked(branches,branch_count,true,false,"literal\tvalue",&mixed) && !mixed);
    CHECK(analysis_value_checked(branches,branch_count,true,false,"literal\tother",&mixed));
    CHECK(analysis_toggle_value(&branches,&branch_count,true,false,"literal\tvalue") && branch_count==0); free(branches);
    char *choices[]={"İstanbul",NULL,"a,b\nquote\"'\\"}; char *json=analysis_values_json(choices,3); char **parsed=NULL; size_t n=0;
    CHECK(analysis_parse_values(json,&parsed,&n)); CHECK(n==3 && !strcmp(parsed[0],choices[0]) && parsed[1]==NULL && !strcmp(parsed[2],choices[2]));
    for (size_t i=0;i<n;i++) free(parsed[i]); free(parsed);
    CHECK(analysis_parse_values(" [ \"\\u0130stanbul\", \"\\ud83d\\ude00\\n\", null ] ",&parsed,&n));
    CHECK(n==3 && !strcmp(parsed[0],"İstanbul") && !strcmp(parsed[1],"😀\n") && !parsed[2]);
    for (size_t i=0;i<n;i++) free(parsed[i]); free(parsed);
    CHECK(!analysis_parse_values("[\"x\",]",&parsed,&n) && !parsed && !n);
    CHECK(!analysis_parse_values("[\"\\u0000\"]",&parsed,&n));
    CHECK(!analysis_parse_values("[\"\\ud800\"]",&parsed,&n));
    CHECK(!analysis_parse_values("[\"\\u0zz0\"]",&parsed,&n));
    a.filter_column=dx_strdup("City"); a.filter_op=FILTER_IN; a.filter_value=json; Page p=fetch(db,&a,0,-1); CHECK(p.total==2); page_free(&p);
    a.filter_op=FILTER_NOT_IN; p=fetch(db,&a,0,-1); CHECK(p.total==3); page_free(&p);
    /* Enumerating the column itself ignores its existing filter. */
    CHECK(database_distinct(db,&a,"City","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==4); distinct_free(&d);
    free(a.filter_column); a.filter_column=NULL;
    CHECK(database_distinct(db,&a,"Date","",0,NULL,NULL,0,-1,&d,&error));
    CHECK(d.total==1 && !strcmp(d.values[0].label,"2026") && strstr(d.values[0].upper,"2027-01-01"));
    free(a.filter_value); a.filter_value=analysis_values_json(&d.values[0].key,1); a.filter_column=dx_strdup("Date"); a.filter_op=FILTER_DATE_IN;
    p=fetch(db,&a,0,-1); CHECK(p.total==5); page_free(&p); distinct_free(&d);
    CHECK(database_distinct(db,&a,"Date","",2,"2026-01-01","2026-02-01",0,-1,&d,&error)); CHECK(d.total==5); distinct_free(&d);
    analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/timestamps.parquet")};
    for (int level=0;level<5;level++) {
        CHECK(database_distinct(db,&a,"Stamp","",level,NULL,NULL,0,-1,&d,&error));
        CHECK(d.total==(uint64_t)(level==0 ? 2 : level==4 ? 4 : 3) && d.values[0].is_null);
        if (level==4) {
            a.filter_column=dx_strdup("Stamp"); a.filter_op=FILTER_DATE_IN; a.filter_value=analysis_values_json(&d.values[1].key,1);
            p=fetch(db,&a,0,-1); CHECK(p.total==1 && strstr(p.cells[0].text,"03:04:05")); page_free(&p);
            free(a.filter_value); char *null_choice=NULL; a.filter_value=analysis_values_json(&null_choice,1);
            p=fetch(db,&a,0,-1); CHECK(p.total==1 && p.cells[0].is_null); page_free(&p);
        }
        distinct_free(&d);
    }
    analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/million.parquet")};
    CHECK(database_distinct(db,&a,"ID","",-1,NULL,NULL,0,-1,&d,&error)); CHECK(d.total==1000000 && d.count==128 && d.bytes<20000); distinct_free(&d);
    uint64_t queries=db->query_count;
    CHECK(database_distinct(db,&a,"ID","",-1,NULL,NULL,128,1000000,&d,&error)); CHECK(d.offset==128 && d.count==128 && db->query_count==queries+1); distinct_free(&d);
    CHECK(!database_distinct(db,&a,"missing\"; DROP TABLE fixture;--","",-1,NULL,NULL,0,-1,&d,&error)); free(error); error=NULL;
    analysis_free(&a);
}
static int benchmark(const char *path) {
    Database db; char *error=NULL; CHECK(database_open(&db,&error));
    Analysis a={.kind=source_kind(path),.path=dx_strdup(path)}; CHECK(a.kind!=SOURCE_NONE);
    double start=dx_millis(); Page p=fetch(&db,&a,0,-1);
    printf("DATA %s\nrows=%" PRIu64 " columns=%zu first_page_ms=%.2f cache_rows=%zu cache_bytes=%zu\n",path,p.total,p.column_count,dx_millis()-start,p.row_count,p.bytes);
    uint64_t total=p.total;
    size_t amount=find_col(&p,"total_amount"); if (amount==p.column_count) amount=find_col(&p,"total");
    size_t category=find_col(&p,"payment_type"); if (category==p.column_count) category=find_col(&p,"color");
    char *amount_name=amount<p.column_count ? dx_strdup(p.columns[amount].name) : NULL;
    char *category_name=category<p.column_count ? dx_strdup(p.columns[category].name) : NULL;
    page_free(&p); start=dx_millis(); p=fetch(&db,&a,total>256 ? total-256 : 0,(int64_t)total);
    printf("last_page_ms=%.2f cache_rows=%zu cache_bytes=%zu\n",dx_millis()-start,p.row_count,p.bytes); page_free(&p);
    if (amount_name) {
        a.sort_column=dx_strdup(amount_name); a.descending=true; start=dx_millis(); p=fetch(&db,&a,0,(int64_t)total);
        CHECK(p.row_count<=256); printf("sort_ms=%.2f top_value=%s\n",dx_millis()-start,p.cells[amount].text); page_free(&p);
        a.filter_column=dx_strdup(amount_name); a.filter_op=FILTER_GREATER; a.filter_value=dx_strdup("50");
        start=dx_millis(); p=fetch(&db,&a,0,-1);
        printf("filter_and_sort_ms=%.2f filtered_rows=%" PRIu64 "\n",dx_millis()-start,p.total); page_free(&p);
        if (category_name) {
            free(a.sort_column); a.sort_column=NULL; a.group_column=dx_strdup(category_name); a.measure_column=dx_strdup(amount_name); a.aggregate=AGG_SUM;
            start=dx_millis(); p=fetch(&db,&a,0,-1);
            printf("group_ms=%.2f groups=%" PRIu64 "\n",dx_millis()-start,p.total); CHECK(p.total<256); page_free(&p);
        }
    }
    free(amount_name); free(category_name); analysis_free(&a); database_close(&db); free(error); return 0;
}
static int table_latency(const char *path) {
    uint64_t expected=0;
    for (int sample=0;sample<4;sample++) for (int order=0;order<2;order++) {
        bool deferred=(sample%2==0) ? order==0 : order!=0;
        Database db; char *error=NULL; CHECK(database_open(&db,&error));
        Analysis a={.kind=source_kind(path),.path=dx_absolute_path(path)}; Page page={0};
        double start=dx_millis();
        bool ok=deferred ? database_fetch_table_window(&db,&a,0,-1,&page,&error) : database_fetch_table(&db,&a,0,-1,&page,&error);
        if (!ok) { fprintf(stderr,"%s\n",error); page_free(&page); analysis_free(&a); database_close(&db); free(error); return 1; }
        double visible=dx_millis()-start; uint64_t total=page.total; bool pending=page.count_pending;
        if (pending) CHECK(database_count(&db,&a,&total,&error));
        double complete=dx_millis()-start;
        if (!expected) expected=total;
        CHECK(total==expected && page.row_count<=256 && page.column_count<=256 && page.bytes<=4*1024*1024);
        printf("table_sample=%d mode=%s first_viewport_ms=%.3f complete_ms=%.3f rows=%" PRIu64 " cached=%zu bytes=%zu count_pending_at_publication=%d SQL=%" PRIu64 " counts=%" PRIu64 "\n",sample,deferred ? "deferred" : "exact",visible,complete,total,page.row_count,page.bytes,pending,db.query_count,db.count_queries);
        page_free(&page); analysis_free(&a); database_close(&db); free(error);
    }
    return 0;
}
static int page_reuse_latency(const char *path) {
    const uint64_t offsets[]={0,256,8192,65536,524288};
    uint64_t hashes[5]={0};
    for (int pass=0;pass<4;pass++) for (int order=0;order<2;order++) {
        bool reuse=(pass%2==0) ? order==0 : order!=0;
        Database db; char *error=NULL; CHECK(database_open(&db,&error));
        Analysis a={.kind=source_kind(path),.path=dx_strdup(path)};
        for (size_t i=0;i<sizeof(offsets)/sizeof(*offsets);i++) {
            if (!reuse) CHECK(database_refresh(&db,&error));
            Page page={0}; double start=dx_millis();
            if (!database_fetch_window(&db,&a,offsets[i],-1,&page,&error)) { fprintf(stderr,"%s\n",error); return 1; }
            double elapsed=dx_millis()-start; CHECK(page.row_count==DX_PAGE_ROWS);
            uint64_t hash=UINT64_C(1469598103934665603);
            for (size_t c=0;c<page.column_count*page.row_count;c++) {
                hash^=page.cells[c].is_null ? 0 : 1; hash*=UINT64_C(1099511628211);
                const unsigned char *p=(const unsigned char *)page.cells[c].text;
                for (;p && *p;p++) { hash^=*p; hash*=UINT64_C(1099511628211); }
                hash^=255; hash*=UINT64_C(1099511628211);
            }
            if (!pass && !order) hashes[i]=hash; else CHECK(hashes[i]==hash);
            printf("pass=%d mode=%s offset=%" PRIu64 " page_ms=%.2f prepares=%" PRIu64 " bytes=%zu\n",pass+1,reuse ? "reuse" : "reprepare",offsets[i],elapsed,db.page_prepares,page.bytes); fflush(stdout); page_free(&page);
        }
        analysis_free(&a); database_close(&db); free(error);
    }
    return 0;
}
static void test_statistics(Database *db) {
    char *error=NULL; SelectionStats stats={0};
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/null-text.csv")};
    CHECK(database_statistics(db,&a,NULL,0,6,1,1,&stats,&error));
    CHECK(stats.cells==6 && stats.filled==5 && stats.numeric==0 && stats.distinct==5 && !stats.sum && !stats.average);
    statistics_free(&stats); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    CHECK(database_statistics(db,&a,NULL,0,5,2,1,&stats,&error));
    CHECK(stats.cells==5 && stats.filled==5 && stats.numeric==5 && stats.distinct==5 && stats.approximate && !stats.non_finite);
    CHECK(!strcmp(stats.sum,"145.25") && !strcmp(stats.average,"29.05") && !strcmp(stats.minimum,"-4.5") && !strcmp(stats.maximum,"100"));
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(database_statistics(db,&a,NULL,0,5,2,1,&stats,&error) && stats.cells==2 && !strcmp(stats.sum,"19.75"));
    CHECK(analysis_filter_set(&a,"City",FILTER_IN,"[\"İstanbul\"]",&error));
    CHECK(database_statistics(db,&a,NULL,0,5,2,1,&stats,&error) && stats.cells==2 && stats.numeric==2 && !strcmp(stats.sum,"19.75"));
    char *filtered_copy=NULL; CHECK(database_copy(db,&a,0,2,2,1,&filtered_copy,&error)); CHECK(!strcmp(filtered_copy,"12.5\n7.25")); free(filtered_copy);
    CHECK(analysis_filter_set(&a,"City",FILTER_NOT_IN,"[\"İstanbul\"]",&error));
    CHECK(database_statistics(db,&a,NULL,0,5,2,1,&stats,&error) && stats.cells==3 && !strcmp(stats.sum,"125.5"));
    char *composite=analysis_filter_pack(FILTER_IN,"[\"İstanbul\",\"Ankara\"]",FILTER_CONTAINS,"stanbul"); CHECK(composite);
    CHECK(analysis_filter_set(&a,"City",FILTER_COMPOSITE,composite,&error)); free(composite);
    CHECK(database_statistics(db,&a,NULL,0,5,2,1,&stats,&error) && stats.cells==2 && !strcmp(stats.sum,"19.75"));
    CHECK(database_copy(db,&a,0,2,2,1,&filtered_copy,&error) && !strcmp(filtered_copy,"12.5\n7.25")); free(filtered_copy);
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error));
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(database_statistics(db,&a,&pivot,0,5,1,1,&stats,&error) && stats.cells==1 && !strcmp(stats.sum,"19.75"));
    pivot_free(&pivot); analysis_free(&a); statistics_free(&stats);
    remove("build/tests/generated/statistics.duckdb");
    execute(db,"ATTACH 'build/tests/generated/statistics.duckdb' AS stats_fixture; CREATE TABLE stats_fixture.exact_values AS SELECT big::HUGEINT AS big,tiny::DECIMAL(38,38) AS tiny,label FROM (VALUES ('170141183460469231731687303715884105727','0.00000000000000000000000000000000000001','NULL'),('170141183460469231731687303715884105727','-0.00000000000000000000000000000000000002',''),(NULL,NULL,NULL)) t(big,tiny,label); CREATE TABLE stats_fixture.floats AS SELECT x::DOUBLE AS x FROM (VALUES ('1e308'),('1e308')) t(x); CREATE TABLE stats_fixture.nonfinite AS SELECT x::DOUBLE AS x FROM (VALUES ('nan'),('inf'),('-inf'),(NULL)) t(x); DETACH stats_fixture");
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/statistics.duckdb"),.table=dx_strdup("exact_values")};
    CHECK(database_statistics(db,&a,NULL,0,3,0,1,&stats,&error));
    CHECK(!stats.approximate && !stats.non_finite && stats.cells==3 && stats.numeric==2 && stats.distinct==1);
    CHECK(!strcmp(stats.sum,"340282366920938463463374607431768211454") && !strcmp(stats.minimum,"170141183460469231731687303715884105727") && !strcmp(stats.maximum,stats.minimum));
    CHECK(database_statistics(db,&a,NULL,0,3,1,1,&stats,&error));
    CHECK(!strcmp(stats.sum,"-0.00000000000000000000000000000000000001") && !strcmp(stats.minimum,"-0.00000000000000000000000000000000000002") && !strcmp(stats.maximum,"0.00000000000000000000000000000000000001"));
    CHECK(database_statistics(db,&a,NULL,0,3,0,3,&stats,&error));
    CHECK(stats.cells==9 && stats.filled==6 && stats.numeric==4 && stats.distinct==5 && !stats.approximate);
    CHECK(!strcmp(stats.sum,"340282366920938463463374607431768211453.99999999999999999999999999999999999999"));
    char *owned=stats.sum;
    CHECK(!database_statistics(db,&a,NULL,0,3,3,1,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    CHECK(!database_statistics(db,&a,NULL,0,3,0,257,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    CHECK(!database_statistics(db,&a,NULL,DX_SHEET_ROWS,1,0,1,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    CHECK(database_statistics(db,&a,NULL,3,2,0,3,&stats,&error));
    CHECK(!stats.cells && !stats.filled && !stats.numeric && !stats.distinct && !stats.sum && !stats.average);
    CHECK(database_statistics(db,&a,NULL,0,0,0,1,&stats,&error) && !stats.cells);
    free(a.table); a.table=dx_strdup("floats");
    CHECK(database_statistics(db,&a,NULL,0,2,0,1,&stats,&error));
    CHECK(stats.numeric==2 && stats.approximate && stats.sum_overflow && stats.average_overflow && !stats.sum && !stats.average && stats.minimum && !stats.non_finite);
    free(a.table); a.table=dx_strdup("nonfinite");
    CHECK(database_statistics(db,&a,NULL,0,4,0,1,&stats,&error));
    CHECK(stats.cells==4 && stats.filled==3 && stats.numeric==3 && stats.distinct==3 && stats.non_finite && !stats.sum && !stats.minimum);
    statistics_free(&stats); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DEMO,.path=dx_strdup("")};
    CHECK(database_statistics(db,&a,NULL,0,1000000,0,1,&stats,&error));
    CHECK(stats.cells==1000000 && stats.numeric==1000000 && stats.distinct==1000000 && !stats.approximate && !strcmp(stats.sum,"499999500000"));
    printf("Selection statistics 1M rows: %.2f ms; exact sum %s\n",stats.query_ms,stats.sum);
    owned=stats.sum; unsigned calls=0; db->cancelled=cancel_page; db->cancel_context=&calls;
    CHECK(!database_statistics(db,&a,NULL,0,1000000,0,2,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    db->cancelled=NULL; db->cancel_context=NULL;
    CHECK(database_statistics(db,&a,NULL,9,2,0,1,&stats,&error) && stats.cells==2 && !strcmp(stats.sum,"19"));
    statistics_free(&stats); analysis_free(&a); free(error);
}
static void test_mixed_statistics(Database *db) {
    char *error=NULL; SelectionStats stats={0}; Analysis empty={0};
    StatisticsRequest *request=dx_alloc(sizeof(*request)); request->cells=DX_SHEET_ROWS*256;
    CHECK(database_statistics_ranges(db,&empty,request,&stats,&error));
    CHECK(stats.cells==DX_SHEET_ROWS*256 && !stats.filled && !stats.numeric && !stats.distinct && !stats.sum);
    CHECK(statistics_request_text(request,NULL,&error) && !request->text_count);
    const char *labels[]={"NULL","","12.5","NULL","a'quote","İstanbul"};
    for (size_t i=0;i<6;i++) CHECK(statistics_request_text(request,labels[i],&error));
    CHECK(database_statistics_ranges(db,&empty,request,&stats,&error));
    CHECK(stats.filled==6 && stats.distinct==5 && !stats.numeric && !stats.sum);
    Analysis a={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    request->cells=100; request->count=3;
    request->ranges[0]=(StatisticsRange){.rows=5,.column=2,.columns=1};
    request->ranges[1]=(StatisticsRange){.is_pivot=true,.rows=99,.column=1,.columns=1};
    CHECK(pivot_assign(&request->ranges[1].pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&request->ranges[1].pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    request->ranges[2]=(StatisticsRange){.is_pivot=true,.rows=99,.column=1,.columns=1};
    CHECK(pivot_assign(&request->ranges[2].pivot,PIVOT_ROWS,"City",AGG_SUM,&error));
    CHECK(pivot_assign(&request->ranges[2].pivot,PIVOT_VALUES,"Amount",AGG_AVG,&error));
    CHECK(database_statistics_ranges(db,&a,request,&stats,&error));
    CHECK(stats.cells==100 && stats.filled==19 && stats.numeric==13 && stats.distinct==12 && stats.approximate);
    CHECK(!strcmp(stats.sum,"425.875") && !strcmp(stats.minimum,"-4.5") && !strcmp(stats.maximum,"100"));
    char *owned=stats.sum;
    request->ranges[0].columns=257;
    CHECK(!database_statistics_ranges(db,&a,request,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    request->ranges[0].columns=1;
    unsigned calls=0; db->cancelled=cancel_page; db->cancel_context=&calls;
    CHECK(!database_statistics_ranges(db,&a,request,&stats,&error) && stats.sum==owned); free(error); error=NULL;
    db->cancelled=NULL; db->cancel_context=NULL;
    CHECK(database_statistics_ranges(db,&a,request,&stats,&error) && !strcmp(stats.sum,"425.875"));
    CHECK(analysis_filter_set(&a,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(database_statistics_ranges(db,&a,request,&stats,&error));
    CHECK(stats.numeric==4 && stats.filled==10 && stats.distinct==9 && !strcmp(stats.sum,"49.375"));
    statistics_request_free(request); statistics_free(&stats); analysis_free(&a);
    request=dx_alloc(sizeof(*request)); request->cells=9; request->count=1; request->ranges[0]=(StatisticsRange){.rows=3,.columns=3};
    a=(Analysis){.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/statistics.duckdb"),.table=dx_strdup("exact_values")};
    CHECK(database_statistics_ranges(db,&a,request,&stats,&error));
    CHECK(!stats.approximate && !strcmp(stats.sum,"340282366920938463463374607431768211453.99999999999999999999999999999999999999"));
    request->text_bytes=8*1024*1024; CHECK(!statistics_request_text(request,"overflow",&error)); free(error); error=NULL; request->text_bytes=0;
    request->count=DX_MAX_OBJECTS+2; owned=stats.sum;
    CHECK(!database_statistics_ranges(db,&a,request,&stats,&error) && stats.sum==owned); free(error); error=NULL; request->count=1;
    statistics_request_free(request); statistics_free(&stats); analysis_free(&a); free(error);
}
static int page_latency(const char *path) {
    for (int pass=0;pass<4;pass++) for (int order=0;order<2;order++) {
        bool window=(pass%2==0) ? order==0 : order!=0;
        Database db; char *error=NULL; CHECK(database_open(&db,&error));
        Analysis a={.kind=source_kind(path),.path=dx_strdup(path)}; Page page={0};
        double start=dx_millis();
        bool ok=window ? database_fetch_window(&db,&a,0,-1,&page,&error) : database_fetch(&db,&a,0,-1,&page,&error);
        if (!ok) { fprintf(stderr,"%s\n",error); return 1; }
        double first=dx_millis()-start,count_ms=0; uint64_t total=page.total;
        CHECK(page.row_count==DX_PAGE_ROWS);
        if (window && page.count_pending) { start=dx_millis(); CHECK(database_count(&db,&a,&total,&error)); count_ms=dx_millis()-start; }
        printf("pass=%d mode=%s visible_ms=%.2f deferred_count_ms=%.2f rows=%" PRIu64 " bytes=%zu\n",pass+1,window ? "window" : "exact",first,count_ms,total,page.bytes); fflush(stdout);
        page_free(&page); analysis_free(&a); database_close(&db); free(error);
    }
    return 0;
}
static int spill_probe(void) {
#ifndef _WIN32
    if (chdir("/")!=0) { perror("Spill probe read-only working directory"); return 1; }
#endif
    Database db; char *error=NULL; duckdb_result result={0};
    if (!database_open(&db,&error)) { fprintf(stderr,"Spill probe open: %s\n",error ? error : ""); free(error); return 1; }
    char *directory=dx_strdup(db.temp_directory);
    bool ok=database_execute(&db,"SELECT current_setting('temp_directory')",&result,&error);
    if (ok) {
        char *path=duckdb_value_varchar(&result,0,0);
        printf("Query temporary directory: %s\n",path);
        ok=path && !strcmp(path,directory);
#ifndef _WIN32
        ok=ok && *path=='/';
#endif
        duckdb_free(path);
    }
    duckdb_destroy_result(&result);
    if (ok) ok=database_execute(&db,"SET memory_limit='32MB'; SET threads=1; SET preserve_insertion_order=false; CREATE TEMP TABLE spill_probe AS SELECT i,md5(i::VARCHAR)||repeat('x',80) AS payload FROM range(750000) r(i)",&result,&error);
    duckdb_destroy_result(&result);
    if (ok) ok=database_execute(&db,"SELECT sum(length(payload)) FROM spill_probe",&result,&error);
    if (ok) ok=duckdb_value_uint64(&result,0,0)==84000000; duckdb_destroy_result(&result);
    if (ok) ok=database_execute(&db,"SELECT coalesce(sum(size),0) FROM duckdb_temporary_files()",&result,&error);
    uint64_t spilled=ok ? duckdb_value_uint64(&result,0,0) : 0; duckdb_destroy_result(&result);
    if (!ok) fprintf(stderr,"Spill probe query: %s\n",error ? error : "incorrect result");
    database_close(&db); free(error);
    struct stat remaining;
    bool cleaned=stat(directory,&remaining)!=0 && errno==ENOENT;
    printf("Private temporary directory removed after close: %s\n",cleaned ? "yes" : "no");
    free(directory);
    printf("%s: 750,000 rows / 84,000,000 payload characters with 32 MB memory limit; temporary files %" PRIu64 " bytes\n",ok && spilled && cleaned ? "PASS" : "FAIL",spilled);
    return ok && spilled && cleaned ? 0 : 1;
}
static void test_temporary_storage(Database *first) {
    Database second; char *error=NULL; duckdb_result result={0}; struct stat info;
    CHECK(first->temp_directory && database_open(&second,&error));
    CHECK(second.temp_directory && strcmp(first->temp_directory,second.temp_directory));
    CHECK(stat(first->temp_directory,&info)==0);
#ifndef _WIN32
    CHECK(S_ISDIR(info.st_mode) && (info.st_mode&0777)==0700);
    CHECK(*first->temp_directory=='/' && *second.temp_directory=='/');
#endif
    CHECK(database_execute(&second,"SELECT current_setting('temp_directory')",&result,&error));
    char *configured=duckdb_value_varchar(&result,0,0);
    CHECK(configured && !strcmp(configured,second.temp_directory));
    duckdb_free(configured); duckdb_destroy_result(&result);
    char *directory=dx_strdup(second.temp_directory);
    database_close(&second);
    CHECK(!second.temp_directory && stat(directory,&info)!=0 && errno==ENOENT);
    CHECK(stat(first->temp_directory,&info)==0);
    /* Independent in-memory engines may attach the same external data file. */
    CHECK(database_open(&second,&error));
    Analysis source={.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/fixture.duckdb")};
    Analysis another=analysis_copy(&source);
    Page a=fetch(first,&source,0,-1),b=fetch(&second,&another,0,-1);
    CHECK(a.total==5 && b.total==5 && a.column_count==b.column_count);
    page_free(&a); page_free(&b); analysis_free(&source); analysis_free(&another);
    database_close(&second); free(directory); free(error);
}
static bool cancel_relation(void *context) { (void)context; return true; }
static void test_source_query(Database *db) {
    char *error=NULL; Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    CHECK(analysis_query_set(&source," \nWITH cities AS (SELECT City,Amount::DECIMAL(12,2) AS Money,Approved FROM input)\nSELECT * FROM cities WHERE Money>10; \n",&error));
    CHECK(source.query && !strchr(source.query,';'));
    Analysis owned=analysis_copy(&source); CHECK(owned.query!=source.query && analysis_same_result(&owned,&source));
    size_t bytes=analysis_text_bytes(&source); CHECK(bytes>strlen(source.query));
    uint64_t sql=db->query_count,count=db->count_queries,prepares=db->relation_prepares;
    CHECK(database_validate_relation(db,&source,&error)); CHECK(db->query_count==sql && db->count_queries==count && db->relation_prepares==prepares+1);
    CHECK(database_validate_relation(db,&source,&error) && db->relation_prepares==prepares+1);
    Page page={0}; CHECK(database_fetch_table(db,&source,0,-1,&page,&error));
    CHECK(page.total==3 && page.column_count==3 && !strcmp(page.columns[1].name,"Money") && page.columns[1].number_type.kind==NUM_DECIMAL);
    CHECK(db->relation_prepares==prepares+1); page_free(&page);
    CHECK(analysis_filter_set(&source,"Money",FILTER_GREATER,"20",&error)); CHECK(analysis_sort_set(&source,"Money",true,false,&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==2 && !strcmp(page.cells[1].text,"100.00")); page_free(&page);
    DistinctPage distinct={0}; CHECK(database_distinct(db,&source,"City","",-1,NULL,NULL,0,-1,&distinct,&error) && distinct.total==2); distinct_free(&distinct);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"Approved",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Money",AGG_SUM,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error) && page.total==2 && page.sql && strstr(page.sql,"PIVOT") && strstr(page.sql,"WITH input")); page_free(&page); pivot_free(&pivot);
    CHECK(!analysis_same_result(&owned,&source)); analysis_free(&source); source=analysis_copy(&owned);
    CHECK(analysis_query_set(&source,source.query+1,&error) && !analysis_same_result(&source,&owned)); /* Aliased edits own their text. */
    const char *bad[]={"SELECT missing FROM input","SELECT * FROM input; CREATE TABLE query_side_effect(i INTEGER)","CREATE TABLE query_side_effect(i INTEGER)","DELETE FROM input","SELECT ? FROM input","SELECT * FROM input) ; DROP TABLE query_side_effect; --","-- comment only"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        CHECK(analysis_query_set(&source,bad[i],&error));
        CHECK(!database_validate_relation(db,&source,&error) && error && *error); free(error); error=NULL;
        CHECK(!database_fetch_table(db,&source,0,-1,&page,&error)); page_free(&page); free(error); error=NULL;
        CHECK(database_fetch_table(db,&owned,0,-1,&page,&error) && page.total==3); page_free(&page);
    }
    CHECK(analysis_query_set(&source,"\nSELECT\nmissing FROM input",&error));
    CHECK(!database_validate_relation(db,&source,&error) && error && strstr(error,"LINE 3:")); free(error); error=NULL;
    CHECK(analysis_query_set(&source,"SELECT \"LINE 999:\" FROM input",&error));
    CHECK(!database_validate_relation(db,&source,&error) && error && strstr(error,"\"LINE 999:\"") && strstr(error,"LINE 1:")); free(error); error=NULL;
    Analysis demo={.kind=SOURCE_DEMO}; CHECK(analysis_query_set(&demo,"SELECT\nmissing FROM input",&error));
    CHECK(!database_validate_relation(db,&demo,&error) && error && strstr(error,"LINE 2:")); free(error); error=NULL; analysis_free(&demo);
    duckdb_result result={0}; CHECK(database_execute(db,"SELECT count(*) FROM information_schema.tables WHERE table_name='query_side_effect'",&result,&error)); CHECK(!duckdb_value_uint64(&result,0,0)); duckdb_destroy_result(&result);
    CHECK(analysis_query_set(&source,"SELECT '; CREATE TABLE x(i INT)' AS text, NULL::VARCHAR AS missing, '' AS empty, 'NULL' AS literal FROM input LIMIT 1;",&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.column_count==4 && page.row_count==1);
    CHECK(page.cells[1].is_null && !page.cells[2].is_null && !*page.cells[2].text && !strcmp(page.cells[3].text,"NULL")); page_free(&page);
    db->cancelled=cancel_relation; CHECK(!database_validate_relation(db,&owned,&error)); free(error); error=NULL; db->cancelled=NULL;
    CHECK(database_validate_relation(db,&owned,&error));
    char *large=dx_alloc(DX_QUERY_BYTES+2); memset(large,'x',DX_QUERY_BYTES+1); large[DX_QUERY_BYTES+1]=0;
    CHECK(!analysis_query_set(&owned,large,&error) && strstr(owned.query,"WITH cities")); free(large); free(error); error=NULL;
    CHECK(analysis_query_set(&source," \n ",&error) && !source.query); CHECK(database_validate_relation(db,&source,&error));
    Analysis blank={0}; CHECK(!analysis_query_set(&blank,"SELECT 1",&error) && !blank.query); free(error); error=NULL;
    QueryWorker worker; CHECK(worker_start(&worker)); QueryJob *request=job(1,source.kind,source.path); request->kind=JOB_VALIDATE_SQL;
    CHECK(analysis_query_set(&request->analysis,owned.query,&error)); worker_submit(&worker,request);
    QueryReply *reply=wait_reply(&worker); CHECK(!reply->error && reply->kind==JOB_VALIDATE_SQL && reply->analysis.query && !reply->page.cells && !reply->page.columns); reply_free(reply); worker_stop(&worker);
    CHECK(!worker.sql_executed && !worker.counts_executed);
    analysis_free(&source); analysis_free(&owned); free(error);
}
static void test_unique_rows(Database *db) {
    char *error=NULL; Page page={0}; Analysis source={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/duplicates.csv")};
    CHECK(analysis_query_set(&source,"SELECT City, Amount::DECIMAL(12,2) AS Amount, Value FROM input",&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==8); page_free(&page);
    Analysis original=analysis_copy(&source); source.unique_rows=true; CHECK(!analysis_same_result(&source,&original));
    Analysis copy=analysis_copy(&source); CHECK(copy.unique_rows && analysis_same_result(&source,&copy)); analysis_free(&copy);
    CHECK(analysis_sort_set(&source,"City",false,false,&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==4 && page.column_count==3 && page.columns[1].number_type.kind==NUM_DECIMAL);
    size_t nulls=0,empty=0,literal=0;
    for (size_t i=0;i<page.row_count;i++) { Cell *value=&page.cells[i*3+2]; nulls+=value->is_null; empty+=!value->is_null && !*value->text; literal+=!value->is_null && !strcmp(value->text,"NULL"); }
    CHECK(nulls==1 && empty==2 && literal==1); page_free(&page);
    uint64_t total=0; CHECK(database_count(db,&source,&total,&error) && total==4);
    DistinctPage distinct={0}; CHECK(database_distinct(db,&source,"Value","",-1,NULL,NULL,0,-1,&distinct,&error) && distinct.total==3); distinct_free(&distinct);
    SelectionStats stats={0}; CHECK(database_statistics(db,&source,NULL,0,4,1,1,&stats,&error) && stats.numeric==4 && !stats.approximate && !strcmp(stats.sum,"50.5")); statistics_free(&stats);
    CHECK(database_copy_page(db,&source,NULL,1,2,0,3,&page,&error) && page.row_count==2 && page.column_count==3); page_free(&page);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"Value",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error) && page.total==3 && strstr(page.sql,"PIVOT") && strstr(page.sql,"SELECT DISTINCT")); page_free(&page);
    CHECK(database_statistics(db,&source,&pivot,0,3,1,3,&stats,&error) && !strcmp(stats.sum,"50.5")); statistics_free(&stats); pivot_free(&pivot);
    CHECK(analysis_filter_set(&source,"City",FILTER_EQUAL,"İstanbul",&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==2); page_free(&page);
    source.unique_rows=false;
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==4); page_free(&page);
    source.unique_rows=true; analysis_filters_clear(&source);
    CHECK(database_export_parquet(db,&source,"build/tests/generated/unique-rows.parquet",&total,&error) && total==4);
    Analysis exported={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/unique-rows.parquet")};
    CHECK(database_fetch_table(db,&exported,0,-1,&page,&error) && page.total==4 && page.columns[1].number_type.kind==NUM_DECIMAL); page_free(&page); analysis_free(&exported);
    CHECK(database_fetch_table(db,&original,0,-1,&page,&error) && page.total==8); page_free(&page); /* External CSV retains duplicates. */
    analysis_sorts_clear(&source); CHECK(analysis_query_set(&source,"SELECT City FROM input",&error));
    CHECK(database_fetch_table(db,&source,0,-1,&page,&error) && page.total==3 && page.column_count==1); page_free(&page);
    CHECK(analysis_query_set(&source,"SELECT\nmissing FROM input",&error));
    CHECK(!database_validate_relation(db,&source,&error) && error && strstr(error,"LINE 2:")); free(error); error=NULL;
    Analysis blank={.unique_rows=true}; CHECK(!analysis_query_valid(&blank,&error));
    CHECK(!database_fetch_table(db,&blank,0,-1,&page,&error)); page_free(&page);
    free(error); error=NULL;
    Analysis relations[]={
        {.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet"),.unique_rows=true},
        {.kind=SOURCE_DATABASE,.path=dx_strdup("build/tests/generated/fixture.duckdb"),.table=dx_strdup("table with spaces"),.unique_rows=true}
    };
    for (size_t i=0;i<sizeof(relations)/sizeof(*relations);i++) {
        CHECK(analysis_query_set(&relations[i],"SELECT City FROM input",&error));
        CHECK(database_fetch_table(db,&relations[i],0,-1,&page,&error) && page.total==4 && page.column_count==1); page_free(&page);
        relations[i].unique_rows=false;
        CHECK(database_fetch_table(db,&relations[i],0,-1,&page,&error) && page.total==5); page_free(&page); analysis_free(&relations[i]);
    }
    analysis_free(&source); analysis_free(&original); free(error);
}
static void test_combined_filter(Database *db) {
    char **selected=NULL; size_t selected_count=0; bool partial=false;
    char *found[]={"İstanbul",NULL,"","NULL"};
    CHECK(analysis_toggle_values(&selected,&selected_count,false,false,found,4)); CHECK(selected_count==4);
    CHECK(analysis_value_checked(selected,selected_count,false,false,NULL,&partial) && !partial);
    CHECK(analysis_toggle_values(&selected,&selected_count,false,false,found,4)); CHECK(selected_count==0);
    CHECK(analysis_toggle_value(&selected,&selected_count,false,false,"Outside search"));
    CHECK(analysis_toggle_value(&selected,&selected_count,false,false,"İstanbul"));
    CHECK(analysis_toggle_values(&selected,&selected_count,false,false,found,4)); CHECK(selected_count==5);
    CHECK(analysis_toggle_values(&selected,&selected_count,false,false,found,4)); CHECK(selected_count==1 && !strcmp(selected[0],"Outside search"));
    for (size_t i=0;i<selected_count;i++) free(selected[i]); free(selected); selected=NULL; selected_count=0;
    CHECK(analysis_toggle_values(&selected,&selected_count,true,false,found,4)); CHECK(selected_count==4);
    CHECK(!analysis_value_checked(selected,selected_count,true,false,"NULL",&partial) && !partial);
    CHECK(analysis_toggle_values(&selected,&selected_count,true,false,found,4)); CHECK(selected_count==0);
    char *month[]={"2026-01-01\t2026-02-01"};
    CHECK(analysis_toggle_value(&selected,&selected_count,true,true,"2026-01-10\t2026-01-11"));
    CHECK(analysis_toggle_values(&selected,&selected_count,true,true,month,1)); CHECK(selected_count==0);
    CHECK(analysis_toggle_values(&selected,&selected_count,true,true,month,1)); CHECK(selected_count==1 && !strcmp(selected[0],month[0]));
    CHECK(!analysis_toggle_values(&selected,&selected_count,true,true,month,4097)); CHECK(selected_count==1 && !strcmp(selected[0],month[0]));
    for (size_t i=0;i<selected_count;i++) free(selected[i]); free(selected);
    char *error=NULL,*lo=NULL,*co=NULL; FilterOp lop,cop;
    char *bounds_values[]={"7.25","30.00"}; char *bounds=analysis_values_json(bounds_values,2);
    CHECK(analysis_filter_value_valid(FILTER_BETWEEN,bounds));
    const char *bad[]={NULL,"[]","[null,\"1\"]","[\"1\"]","[\"1\",\"2\",\"3\"]","[\"1\",]"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) CHECK(!analysis_filter_value_valid(FILTER_BETWEEN,bad[i]));
    char *payload=analysis_filter_pack(FILTER_IN,"[\"12.50\",\"30.00\",\"100.00\"]",FILTER_BETWEEN,bounds);
    CHECK(payload && analysis_filter_unpack(payload,&lop,&lo,&cop,&co));
    CHECK(lop==FILTER_IN && cop==FILTER_BETWEEN && !strcmp(lo,"[\"12.50\",\"30.00\",\"100.00\"]") && !strcmp(co,bounds)); free(lo); free(co);
    CHECK(!analysis_filter_pack(FILTER_CONTAINS,"[]",FILTER_EQUAL,"x"));
    CHECK(!analysis_filter_pack(FILTER_IN,"[]",FILTER_COMPOSITE,payload));
    CHECK(!analysis_filter_value_valid(FILTER_COMPOSITE,"[\"6\",\"[null]\",\"10\",\"[]\"]"));
    Analysis a={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet")};
    CHECK(analysis_filter_set(&a,"Amount",FILTER_BETWEEN,bounds,&error)); Page page=fetch(db,&a,0,-1); CHECK(page.total==3); page_free(&page);
    CHECK(analysis_filter_set(&a,"Amount",FILTER_COMPOSITE,payload,&error)); page=fetch(db,&a,0,-1); CHECK(page.total==2); page_free(&page);
    Analysis copy=analysis_copy(&a); CHECK(copy.filter_value!=a.filter_value && analysis_same_result(&a,&copy)); analysis_free(&copy);
    DistinctPage values={0}; CHECK(database_distinct(db,&a,"Amount","",-1,NULL,NULL,0,-1,&values,&error)); CHECK(values.total==5); distinct_free(&values);
    CHECK(!analysis_filter_set(&a,"Amount",FILTER_COMPOSITE,"[]",&error)); CHECK(!strcmp(a.filter_value,payload)); free(error); error=NULL;
    analysis_filters_clear(&a);
    PivotDefinition pivot={0}; CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_FILTERS,"Amount",AGG_SUM,&error));
    pivot.fields[PIVOT_FILTERS][0].filter=FILTER_COMPOSITE; free(pivot.fields[PIVOT_FILTERS][0].value); pivot.fields[PIVOT_FILTERS][0].value=dx_strdup(payload);
    CHECK(database_pivot(db,&a,&pivot,0,-1,&page,&error)); CHECK(page.total==2); page_free(&page); pivot_free(&pivot);
    free(payload); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/numeric.parquet")};
    numeric_count(db,&a,"Tiny",FILTER_BETWEEN,"[\"-1.5\",\"1.5\"]",3);
    numeric_count(db,&a,"Tiny",FILTER_BETWEEN,"[\"-1\",\"1\"]",3);
    numeric_count(db,&a,"Tiny",FILTER_BETWEEN,"[\"-1000\",\"1000\"]",7);
    numeric_count(db,&a,"Tiny",FILTER_BETWEEN,"[\"2\",\"-2\"]",0);
    analysis_filters_clear(&a);
    numeric_count(db,&a,"Money",FILTER_BETWEEN,"[\"-0.005\",\"0.005\"]",1);
    numeric_count(db,&a,"Money",FILTER_BETWEEN,"[\"-0.1\",\"0.1\"]",3);
    payload=analysis_filter_pack(FILTER_NOT_IN,"[\"0.0\"]",FILTER_BETWEEN,"[\"-0.1\",\"0.1\"]");
    numeric_count(db,&a,"Money",FILTER_COMPOSITE,payload,2); free(payload); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/null-text.parquet")};
    /* Missing values and the literal NULL remain independent under conjunction. */
    payload=analysis_filter_pack(FILTER_IN,"[null,\"NULL\"]",FILTER_NOT_NULL,"");
    CHECK(analysis_filter_set(&a,"Value",FILTER_COMPOSITE,payload,&error)); page=fetch(db,&a,0,-1); CHECK(page.total==1 && !page.cells[1].is_null && !strcmp(page.cells[1].text,"NULL")); page_free(&page); free(payload); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/odd'name.parquet")};
    payload=analysis_filter_pack(FILTER_DATE_IN,"[\"2026-01-01\\t2026-02-01\"]",FILTER_BETWEEN,"[\"2026-01-02\",\"2026-01-04\"]");
    CHECK(analysis_filter_set(&a,"Date",FILTER_COMPOSITE,payload,&error)); page=fetch(db,&a,0,-1); CHECK(page.total==3); page_free(&page); free(payload); analysis_free(&a); free(bounds); free(error);
}
static int unique_large(const char *path) {
    Database db; char *error=NULL; CHECK(database_open(&db,&error));
    Analysis source={.kind=source_kind(path),.path=dx_absolute_path(path)}; Page page={0};
    uint64_t original=0,unique=0; bool ok=database_count(&db,&source,&original,&error);
    uint64_t counts=db.count_queries; source.unique_rows=true; double start=dx_millis();
    if (ok) ok=database_fetch_table_window(&db,&source,0,-1,&page,&error);
    if (!ok) { fprintf(stderr,"Large DISTINCT failed: %s\n",error ? error : "unknown error"); goto done; }
    CHECK(page.row_count==256 && page.column_count==20 && page.bytes<=4*1024*1024 && page.count_pending && db.count_queries==counts);
    double first=dx_millis()-start; page_free(&page);
    ok=database_count(&db,&source,&unique,&error); if (!ok) { fprintf(stderr,"Large DISTINCT count failed: %s\n",error); goto done; }
    CHECK(unique==3475226 && unique==original);
    double counted=dx_millis()-start;
    ok=database_fetch_table_window(&db,&source,256,(int64_t)unique,&page,&error);
    if (!ok) { fprintf(stderr,"Large DISTINCT second page failed: %s\n",error); goto done; }
    CHECK(page.offset==256 && page.row_count==256 && !page.count_pending && page.total==unique && page.bytes<=4*1024*1024 && db.count_queries==counts+1);
    printf("PASS: public NYC DISTINCT %" PRIu64 " rows / %zu columns, 512 MB budget, page %.2f ms, page+count %.2f ms, complete %.2f ms, bounded %zu-byte page, no redundant count\n",unique,page.column_count,first,counted,dx_millis()-start,page.bytes);
done:
    page_free(&page); analysis_free(&source); database_close(&db); free(error); return ok ? 0 : 1;
}
static bool totals_cancelled(void *context) { return *(bool *)context; }
static void test_pivot_count_projection(Database *db) {
    Analysis source={.kind=SOURCE_DEMO,.query=dx_strdup("SELECT * FROM (VALUES (NULL::BIGINT),(1),(3)) t(Nullable)")}; PivotDefinition pivot={0}; Page page={0}; char *error=NULL;
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Nullable",AGG_COUNT,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error));
    CHECK(page.row_count==1 && page.column_count==1 && page.total==1 && !strcmp(page.cells[0].text,"3") && page.columns[0].type==DUCKDB_TYPE_BIGINT && page.sql && !strstr(page.sql,"PIVOT ("));
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error) && !strcmp(page.totals->columns.cells[0].text,"3")); page_free(&page);
    /* A preceding COUNT(*) must not hide the projection a later SUM needs. */
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"Nullable",AGG_SUM,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error) && page.row_count==1 && page.column_count==2 && !strcmp(page.cells[0].text,"3") && !strcmp(page.cells[1].text,"4"));
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error) && !strcmp(page.totals->columns.cells[0].text,"3") && !strcmp(page.totals->columns.cells[1].text,"4")); page_free(&page);
    CHECK(analysis_filter_set(&source,"Nullable",FILTER_GREATER,"999",&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error) && page.row_count==1 && page.column_count==2 && page.total==1 && !strcmp(page.cells[0].text,"0") && page.cells[1].is_null);
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error) && !strcmp(page.totals->columns.cells[0].text,"0") && page.totals->columns.cells[1].is_null); page_free(&page); pivot_free(&pivot); analysis_free(&source);
    source=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup("SELECT * FROM (VALUES (NULL::TIMESTAMP),(TIMESTAMP '2025-01-01'),(TIMESTAMP '2025-01-02'),(TIMESTAMP '2025-04-01')) t(occurred)")};
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign_grain(&pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,&error));
    CHECK(pivot_assign(&pivot,PIVOT_VALUES,"occurred",AGG_COUNT,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error) && page.row_count==1 && page.column_count==9 && page.total==1 && page.sql && strstr(page.sql,"PIVOT (") && strstr(page.sql," ON "));
    bool january=false,missing=false;
    for (size_t c=0;c<page.column_count;c++) {
        const Column *header=&page.columns[c]; CHECK(header->category_count==2 && header->category_grains[0]==PIVOT_DATE_MONTH && header->category_grains[1]==PIVOT_DATE_QUARTER);
        if (!header->categories[0] && !header->categories[1]) { CHECK(!strcmp(page.cells[c].text,"1")); missing=true; }
        if (header->categories[0] && !strcmp(header->categories[0],"2025-01-01") && header->categories[1] && !strcmp(header->categories[1],"Q1")) { CHECK(!strcmp(page.cells[c].text,"2")); january=true; }
    }
    CHECK(january && missing);
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error) && page.totals->columns.column_count==10 && !strcmp(page.totals->rows.cells[0].text,"4") && !strcmp(page.totals->columns.cells[9].text,"4"));
    page_free(&page); pivot_free(&pivot); analysis_free(&source); free(error);
}
static void test_pivot_totals(Database *db) {
    execute(db,"COPY (SELECT * FROM (VALUES (NULL::VARCHAR,NULL::VARCHAR,10::DECIMAL(38,2)),('A','x',1),('A','x',3),('A','y',9),('A','y',NULL),('B','x',2),('B','y',2),('B','z',NULL),('','',100),('NULL','NULL',200)) t(r,c,v)) TO 'build/tests/generated/pivot-totals.parquet' (FORMAT PARQUET)");
    Analysis source={.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/pivot-totals.parquet")}; PivotDefinition pivot={0}; Page page={0}; char *error=NULL;
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"r",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"c",AGG_SUM,&error));
    for (int agg=AGG_SUM;agg<=AGG_MAX;agg++) CHECK(pivot_assign(&pivot,PIVOT_VALUES,"v",(Aggregate)agg,&error));
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error)); Cell *original=page.cells;
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error)); CHECK(page.cells==original && page.total==5 && page.column_count==37);
    PivotTotals *totals=page.totals; CHECK(totals && totals->has_columns && totals->rows.row_count==5 && totals->rows.column_count==6 && totals->columns.column_count==42);
    /* NULL, empty VARCHAR and the literal NULL are independent keys, ordered
       exactly like the main PIVOT rows. Averages/distinct counts use raw rows. */
    CHECK(page.cells[0].is_null); CHECK(!strcmp(page.cells[37].text,"")); CHECK(!strcmp(page.cells[74].text,"A")); CHECK(!strcmp(page.cells[148].text,"NULL"));
    CHECK(!strcmp(totals->rows.cells[12].text,"13.00")); CHECK(fabs(totals->rows.cells[15].number-13.0/3)<1e-9);
    CHECK(!strcmp(totals->rows.cells[13].text,"4")); CHECK(!strcmp(totals->rows.cells[14].text,"3"));
    CHECK(!strcmp(totals->rows.cells[16].text,"1.00")); CHECK(!strcmp(totals->rows.cells[17].text,"9.00"));
    CHECK(!strcmp(totals->columns.cells[36].text,"327.00")); CHECK(fabs(totals->columns.cells[39].number-327.0/8)<1e-9);
    CHECK(!strcmp(totals->columns.cells[37].text,"10")); CHECK(!strcmp(totals->columns.cells[38].text,"7"));
    CHECK(!strcmp(totals->columns.cells[40].text,"1.00")); CHECK(!strcmp(totals->columns.cells[41].text,"200.00"));
    uint64_t queries=db->query_count; CHECK(database_pivot_totals(db,&source,&pivot,&page,&error)); CHECK(db->query_count==queries && page.totals==totals);
    page_free(&page); CHECK(!page.totals && !page.cells); pivot_remove(&pivot,PIVOT_COLUMNS,0);
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error)); CHECK(database_pivot_totals(db,&source,&pivot,&page,&error));
    CHECK(!page.totals->has_columns && page.totals->columns.column_count==6); CHECK(!strcmp(page.totals->columns.cells[0].text,"327.00")); page_free(&page);
    CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"c",AGG_SUM,&error)); pivot_remove(&pivot,PIVOT_ROWS,0);
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error)); CHECK(database_pivot_totals(db,&source,&pivot,&page,&error));
    CHECK(page.row_count==1 && page.totals->rows.row_count==1 && !strcmp(page.totals->rows.cells[0].text,"327.00")); page_free(&page);
    pivot_remove(&pivot,PIVOT_COLUMNS,0); CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error)); CHECK(database_pivot_totals(db,&source,&pivot,&page,&error));
    CHECK(!page.totals->has_columns && !strcmp(page.totals->columns.cells[1].text,"10")); page_free(&page);
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"r",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"c",AGG_SUM,&error));
    CHECK(analysis_filter_set(&source,"v",FILTER_GREATER,"999999",&error)); CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error));
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error)); CHECK(!page.row_count && !page.totals->rows.row_count && !page.totals->has_columns);
    CHECK(page.totals->columns.cells[0].is_null && !strcmp(page.totals->columns.cells[1].text,"0") && !strcmp(page.totals->columns.cells[2].text,"0")); page_free(&page);
    analysis_filter_remove(&source,"v");
    CHECK(database_pivot(db,&source,&pivot,0,-1,&page,&error)); original=page.cells; bool cancel=true;
    db->cancelled=totals_cancelled; db->cancel_context=&cancel; CHECK(!database_pivot_totals(db,&source,&pivot,&page,&error));
    CHECK(error && strstr(error,"cancelled") && !page.totals && page.cells==original); db->cancelled=NULL; db->cancel_context=NULL; free(error); error=NULL;
    CHECK(database_pivot_totals(db,&source,&pivot,&page,&error)); page_free(&page); pivot_free(&pivot); analysis_free(&source);
    /* DuckDB's Parquet writer maps HUGEINT to DOUBLE; use an exact DECIMAL
       source so this fixture tests aggregate precision rather than that loss. */
    execute(db,"COPY (SELECT i::BIGINT AS r,i%2 AS c,100000000000000000000::DECIMAL(38,0)+i AS v FROM range(600) t(i)) TO 'build/tests/generated/pivot-totals-large.parquet' (FORMAT PARQUET)");
    source=(Analysis){.kind=SOURCE_PARQUET,.path=dx_strdup("build/tests/generated/pivot-totals-large.parquet")};
    CHECK(pivot_assign(&pivot,PIVOT_ROWS,"r",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_COLUMNS,"c",AGG_SUM,&error)); CHECK(pivot_assign(&pivot,PIVOT_VALUES,"v",AGG_SUM,&error));
    CHECK(database_pivot(db,&source,&pivot,300,-1,&page,&error)); CHECK(database_pivot_totals(db,&source,&pivot,&page,&error));
    CHECK(page.offset==300 && page.row_count==256 && page.total==600 && page.totals->rows.offset==300);
    CHECK(!strcmp(page.totals->rows.cells[0].text,"100000000000000000300")); CHECK(!strcmp(page.totals->columns.cells[2].text,"60000000000000000179700"));
    CHECK(page.bytes<=4*1024*1024); page_free(&page); pivot_free(&pivot); analysis_free(&source); free(error);
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--clipboard-ownership-only")) { test_clipboard_ownership(); printf("PASS: %u focused clipboard ownership checks\n",checks); return 0; }
    if (argc==2 && !strcmp(argv[1],"--source-guards-only")) { test_worker_source_guards(); printf("PASS: %u focused worker source-guard checks\n",checks); return 0; }
    if (argc==2 && !strcmp(argv[1],"--count-projection-only")) { Database db; char *error=NULL; CHECK(database_open(&db,&error)); test_pivot_count_projection(&db); database_close(&db); free(error); printf("PASS: %u focused COUNT projection checks\n",checks); return 0; }
    if (argc==3 && !strcmp(argv[1],"--unique-large")) return unique_large(argv[2]);
    if (argc==2 && !strcmp(argv[1],"--spill-probe")) return spill_probe();
    if (argc==3 && !strcmp(argv[1],"--page-reuse-latency")) return page_reuse_latency(argv[2]);
    if (argc==3 && !strcmp(argv[1],"--table-latency")) return table_latency(argv[2]);
    if (argc==3 && !strcmp(argv[1],"--page-latency")) return page_latency(argv[2]);
    if (argc==3 && !strcmp(argv[1],"--benchmark")) return benchmark(argv[2]);
    Database db; char *error=NULL; CHECK(database_open(&db,&error));
    fixtures(&db); test_temporary_storage(&db); test_csv(&db); test_typed_query(&db); test_aggregates(&db); test_database(&db); test_million(&db); test_available_values(&db); test_pivot(&db); test_composable_filters(&db); test_pivot_null_columns(&db); test_pivot_date_grains(&db); test_null_text(&db); test_export(&db); test_pivot_export(&db); test_window_counts(&db); test_numeric_filters(&db); test_combined_filter(&db); test_clipboard(&db); test_sort_order(&db); test_prepared_pages(&db); test_statistics(&db); test_mixed_statistics(&db);
    test_source_snapshots(); test_table_windows(&db); test_source_query(&db); test_unique_rows(&db); test_pivot_totals(&db); test_pivot_count_projection(&db); database_close(&db); test_worker(); test_worker_source_guards(); free(error);
    printf("PASS: %u checks (CSV, Parquet, typed values, safe SQL, filters, aggregates, clipboard, database, million-row paging, worker recovery)\n",checks);
    return 0;
}
