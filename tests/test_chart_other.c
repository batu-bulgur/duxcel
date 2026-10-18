#include "transforms/chart_other.h"
#include "data/database.h"
#include "ui/chartview.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL chart Other:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void clear_error(char **error) { CHECK(*error && **error); free(*error); *error=NULL; }
static Analysis source(const char *query) { return (Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(query)}; }
static void assign(PivotDefinition *d,PivotRole role,const char *name,Aggregate aggregate) {
    char *error=NULL; CHECK(pivot_assign(d,role,name,aggregate,&error)); CHECK(!error);
}
static void fetch(Database *db,Analysis *a,PivotDefinition *d,uint64_t offset,Page *p) {
    char *error=NULL; CHECK(database_pivot(db,a,d,offset,-1,p,&error)); CHECK(!error && !p->count_pending);
}
static void execute(Database *db,const ChartOtherPlan *plan,duckdb_result *result) {
    char *error=NULL; uint64_t queries=db->query_count;
    if (!database_execute(db,plan->sql,result,&error)) { fprintf(stderr,"Other SQL: %s\n%s\n",error,plan->sql); exit(1); }
    CHECK(!error && db->query_count==queries+1 && duckdb_row_count(result)==1);
    CHECK(duckdb_column_count(result)==plan->rank.categories*plan->rank.measures);
}
static void equal_number(duckdb_result *result,size_t column,double value) {
    CHECK(!duckdb_value_is_null(result,column,0)); CHECK(fabs(duckdb_value_double(result,column,0)-value)<1e-10);
}
static void equal_text(duckdb_result *result,size_t column,const char *value) {
    char *actual=duckdb_value_varchar(result,column,0); CHECK(actual && !strcmp(actual,value)); duckdb_free(actual);
}
static void plan(Database *db,Analysis *a,PivotDefinition *d,Page *p,size_t measure,ChartOtherPlan *out,duckdb_result *result) {
    char *error=NULL; uint64_t queries=db->query_count; Cell *cells=p->cells; Column *columns=p->columns;
    /* These fixtures' original Column fields are VARCHAR or DATE. The date
       grains below project them independently; no type query is needed. */
    duckdb_type types[DX_PIVOT_FIELDS]; for (size_t i=0;i<d->counts[PIVOT_COLUMNS];i++) types[i]=d->fields[PIVOT_COLUMNS][i].grain ? DUCKDB_TYPE_DATE : DUCKDB_TYPE_VARCHAR;
    CHECK(chart_other_plan(a,d,p,measure,types,d->counts[PIVOT_COLUMNS],DX_CHART_OTHER_SQL_BYTES,out,&error));
    CHECK(!error && db->query_count==queries && p->cells==cells && p->columns==columns);
    CHECK(out->rank.has_other && out->sql && out->sql_bytes==strlen(out->sql)+1 && out->sql_bytes<=DX_CHART_OTHER_SQL_BYTES);
    UIChartView old={0}; CHECK(ui_chart_view(p,d,measure,&old));
    for (size_t i=0;i<out->rank.top_count;i++) CHECK(out->rank.top_rows[i]==old.rows[i]);
    CHECK(out->rank.truncated==old.truncated);
    execute(db,out,result);
}
static void reject_types(Analysis *a,PivotDefinition *d,Page *p,const duckdb_type *types,size_t count,ChartOtherPlan *out) {
    unsigned char before[sizeof(*out)]; memcpy(before,out,sizeof(before)); char *copy=dx_strdup(out->sql),*error=NULL;
    CHECK(!chart_other_plan(a,d,p,0,types,count,DX_CHART_OTHER_SQL_BYTES,out,&error)); clear_error(&error);
    CHECK(!memcmp(before,out,sizeof(before)) && !strcmp(copy,out->sql)); free(copy);
}
static void test_weighted(Database *db) {
    Analysis a=source("SELECT * FROM (VALUES ('A',100,100,'a'),('B',90,100,'b'),('C',80,100,'c'),('D',2,0,'x'),('E',1,10,'x'),('E',1,10,'y'),('E',1,10,'z')) t(r,rank,v,customer)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0};
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"rank",AGG_SUM);
    assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT);
    assign(&d,PIVOT_VALUES,"v",AGG_COUNT); assign(&d,PIVOT_VALUES,"v",AGG_MIN); assign(&d,PIVOT_VALUES,"v",AGG_MAX);
    fetch(db,&a,&d,0,&p); CHECK(p.row_count==5 && p.column_count==7);
    plan(db,&a,&d,&p,1,&out,&result); CHECK(out.rank.rank_measure==0 && out.rank.top_rows[0]==0 && out.rank.top_rows[2]==2);
    equal_number(&result,0,5); equal_number(&result,1,7.5); equal_number(&result,2,3); equal_number(&result,3,4); equal_number(&result,4,0); equal_number(&result,5,10);
    CHECK(p.cells[3*7+2].number==0 && p.cells[4*7+2].number==10); CHECK((p.cells[3*7+2].number+p.cells[4*7+2].number)/2==5);
    CHECK(p.cells[3*7+3].number+p.cells[4*7+3].number==4);
    duckdb_destroy_result(&result);
    /* With no SUM, a distinct-count Plot changes the top tuple set. Other still
       returns every assigned measure, so switching Plot needs no new contract. */
    pivot_remove(&d,PIVOT_VALUES,0); fetch(db,&a,&d,0,&p); plan(db,&a,&d,&p,1,&out,&result);
    CHECK(out.rank.rank_measure==1 && out.rank.top_rows[0]==4 && out.rank.top_rows[1]==0 && out.rank.top_rows[2]==1);
    equal_number(&result,0,50); equal_number(&result,1,2); duckdb_destroy_result(&result);
    pivot_remove(&d,PIVOT_VALUES,1); /* Return the original SUM ranking below. */
    assign(&d,PIVOT_VALUES,"rank",AGG_SUM); fetch(db,&a,&d,0,&p); plan(db,&a,&d,&p,0,&out,&result);
    CHECK(out.rank.rank_measure==4); equal_number(&result,0,7.5); duckdb_destroy_result(&result);
    /* The completed plan owns all SQL, even after every borrowed definition and
       business-value Page has been released. */
    page_free(&p); pivot_free(&d); analysis_free(&a); execute(db,&out,&result); equal_number(&result,0,7.5);
    duckdb_destroy_result(&result); chart_other_plan_free(&out); chart_other_plan_free(&out); CHECK(!out.sql && !out.sql_bytes);
}
static void test_null_text(Database *db) {
    Analysis a=source("SELECT r,c,rank,v,customer FROM (VALUES ('A',100,100,'a'),('B',90,100,'b'),('C',80,100,'c'),(NULL,1,NULL,NULL),('',1,NULL,''),('NULL',1,NULL,'NULL'),('O''İ',1,NULL,'')) t(r,rank,v,customer) CROSS JOIN (VALUES (NULL::VARCHAR),(''),('NULL'),('q''\"İ')) categories(c)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0};
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_COLUMNS,"c",AGG_SUM);
    assign(&d,PIVOT_VALUES,"rank",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT); assign(&d,PIVOT_VALUES,"v",AGG_COUNT);
    assign(&d,PIVOT_VALUES,"v",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_MIN); assign(&d,PIVOT_VALUES,"v",AGG_MAX);
    fetch(db,&a,&d,0,&p); CHECK(p.row_count==7 && p.column_count==29);
    plan(db,&a,&d,&p,1,&out,&result); CHECK(out.rank.categories==4 && out.rank.top_rows[0]==2 && out.rank.top_rows[2]==4);
    bool missing=false,empty=false,literal=false,quoted=false;
    for (size_t c=0;c<4;c++) {
        const char *category=p.columns[1+c*7].categories[0];
        missing|=!category; empty|=category && !*category; literal|=category && !strcmp(category,"NULL"); quoted|=category && !strcmp(category,"q'\"İ");
        equal_number(&result,c*7,4); CHECK(duckdb_value_is_null(&result,c*7+1,0)); equal_number(&result,c*7+2,2); equal_number(&result,c*7+3,4);
        CHECK(duckdb_value_is_null(&result,c*7+4,0) && duckdb_value_is_null(&result,c*7+5,0) && duckdb_value_is_null(&result,c*7+6,0));
    }
    CHECK(missing && empty && literal && quoted); CHECK(strstr(out.sql,"IS NOT DISTINCT FROM NULL") && strstr(out.sql,"cast_to_type(''") && strstr(out.sql,"cast_to_type('NULL'"));
    duckdb_type types[]={DUCKDB_TYPE_LIST,DUCKDB_TYPE_VARCHAR};
    reject_types(&a,&d,&p,NULL,1,&out); reject_types(&a,&d,&p,types,1,&out); reject_types(&a,&d,&p,types,2,&out);
    PivotDateGrain grain=p.columns[0].date_grain; p.columns[0].date_grain=PIVOT_DATE_YEAR; types[0]=DUCKDB_TYPE_VARCHAR;
    reject_types(&a,&d,&p,types,1,&out); p.columns[0].date_grain=grain;
    duckdb_destroy_result(&result); chart_other_plan_free(&out); page_free(&p); pivot_free(&d); analysis_free(&a);
}
static void test_exact_typed(Database *db) {
    Analysis a=source("SELECT (9007199254740993+i)::BIGINT AS \"r'İ\"\"key\", (DATE '2025-01-01'+(i*365)::INTEGER) AS occurred, (10-i)::DECIMAL(38,4) AS v FROM range(5) t(i)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0}; char *error=NULL;
    assign(&d,PIVOT_ROWS,"r'İ\"key",AGG_SUM); CHECK(pivot_assign_grain(&d,PIVOT_ROWS,"occurred",AGG_SUM,PIVOT_DATE_YEAR,&error));
    CHECK(pivot_assign_grain(&d,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign_grain(&d,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,&error));
    assign(&d,PIVOT_VALUES,"v",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    fetch(db,&a,&d,0,&p); CHECK(p.row_count==5 && p.columns[0].type==DUCKDB_TYPE_BIGINT && p.columns[1].date_grain==PIVOT_DATE_YEAR);
    plan(db,&a,&d,&p,1,&out,&result); CHECK(strstr(out.sql,"9007199254740996") && strstr(out.sql,"9007199254740997"));
    double sum=0,average_sum=0; size_t present=0;
    for (size_t c=0;c<out.rank.categories;c++) {
        if (!duckdb_value_is_null(&result,c*2,0)) { sum+=duckdb_value_double(&result,c*2,0); average_sum+=duckdb_value_double(&result,c*2+1,0); present++; }
    }
    CHECK(present==2 && sum==13 && average_sum==13);
    duckdb_type types[]={DUCKDB_TYPE_VARCHAR,DUCKDB_TYPE_DATE}; reject_types(&a,&d,&p,types,2,&out);
    types[0]=DUCKDB_TYPE_DATE; types[1]=DUCKDB_TYPE_STRUCT; reject_types(&a,&d,&p,types,2,&out);
    duckdb_destroy_result(&result); chart_other_plan_free(&out); page_free(&p); pivot_free(&d); analysis_free(&a); free(error);
}
static void test_loaded_window(Database *db) {
    Analysis a=source("SELECT i AS r,1000000-i AS rank,i::DOUBLE AS v,CASE WHEN i<300 THEN 'outside' ELSE 'loaded-'||(i%3)::VARCHAR END AS customer FROM range(600) t(i)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0};
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"rank",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"customer",AGG_DISTINCT);
    fetch(db,&a,&d,300,&p); CHECK(p.row_count==256 && p.offset==300 && p.total==600);
    plan(db,&a,&d,&p,1,&out,&result); CHECK(out.rank.truncated && out.rank.top_rows[0]==0 && out.rank.top_rows[2]==2);
    equal_number(&result,1,429); equal_number(&result,2,3); /* only303..555, not0..299 or556..599 */
    double expected=0; for (size_t r=303;r<=555;r++) expected+=1000000-r; equal_number(&result,0,expected);
    duckdb_destroy_result(&result); chart_other_plan_free(&out); page_free(&p); pivot_free(&d); analysis_free(&a);
}
static void test_filters_precision(Database *db) {
    Analysis a=source("SELECT i AS r,12345678901234567890.1250::DECIMAL(38,4) AS v,i<5 AS keep FROM range(6) t(i)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0}; char *error=NULL;
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG);
    CHECK(analysis_filter_set(&a,"keep",FILTER_EQUAL,"true",&error));
    assign(&d,PIVOT_FILTERS,"v",AGG_SUM); d.fields[PIVOT_FILTERS][0].filter=FILTER_NOT_NULL;
    CHECK(pivot_valid(&d,&error)); fetch(db,&a,&d,0,&p); CHECK(p.total==5);
    plan(db,&a,&d,&p,1,&out,&result); CHECK(out.rank.top_rows[0]==0 && out.rank.top_rows[1]==1 && out.rank.top_rows[2]==2);
    CHECK(duckdb_column_type(&result,0)==DUCKDB_TYPE_DECIMAL); equal_text(&result,0,"24691357802469135780.2500");
    duckdb_destroy_result(&result); chart_other_plan_free(&out); page_free(&p); pivot_free(&d); analysis_free(&a); free(error);
}
static void test_rank(void) {
    PivotDefinition d={0}; assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_AVG); assign(&d,PIVOT_VALUES,"w",AGG_MAX);
    Cell cells[6*5]={0}; Column columns[5]={0}; Page p={.columns=columns,.cells=cells,.row_count=6,.column_count=5,.offset=17,.total=100};
    for (size_t c=1;c<5;c++) columns[c].numeric=true;
    const double first[]={-DBL_MAX,DBL_MAX,DBL_MAX,NAN,INFINITY,0},second[]={DBL_MAX,-DBL_MAX,DBL_MAX,0,0,0};
    for (size_t r=0;r<6;r++) { cells[r*5+1].number=first[r]; cells[r*5+3].number=second[r]; cells[r*5+2].number=6-r; cells[r*5+4].number=r; }
    for (size_t measure=0;measure<2;measure++) {
        ChartOtherRank rank={0}; UIChartView old={0}; char *error=NULL;
        CHECK(chart_other_rank(&p,&d,measure,&rank,&error) && !error && ui_chart_view(&p,&d,measure,&old));
        CHECK(rank.rank_measure==measure && rank.truncated && rank.has_other);
        for (size_t i=0;i<3;i++) CHECK(rank.top_rows[i]==old.rows[i]);
    }
    ChartOtherRank rank={0}; char *error=NULL; CHECK(chart_other_rank(&p,&d,0,&rank,&error)); CHECK(rank.top_rows[0]==2 && rank.top_rows[1]==0 && rank.top_rows[2]==1);
    d.fields[PIVOT_VALUES][1].aggregate=AGG_SUM; CHECK(chart_other_rank(&p,&d,0,&rank,&error) && rank.rank_measure==1 && rank.top_rows[0]==0);
    unsigned char before[sizeof(rank)]; memcpy(before,&rank,sizeof(before)); CHECK(!chart_other_rank(&p,&d,2,&rank,&error) && !memcmp(before,&rank,sizeof(rank))); clear_error(&error);
    columns[1].numeric=false; CHECK(!chart_other_rank(&p,&d,0,&rank,&error) && !memcmp(before,&rank,sizeof(rank))); clear_error(&error);
    d.counts[PIVOT_FILTERS]=DX_PIVOT_FIELDS+1; CHECK(!chart_other_rank(&p,&d,0,&rank,&error)); clear_error(&error); d.counts[PIVOT_FILTERS]=0;
    pivot_free(&d);
}
static void rejected(Analysis *a,PivotDefinition *d,Page *p,size_t measure,size_t budget,ChartOtherPlan *out) {
    unsigned char before[sizeof(*out)]; memcpy(before,out,sizeof(before)); char *copy=dx_strdup(out->sql),*error=NULL;
    CHECK(!chart_other_plan(a,d,p,measure,NULL,0,budget,out,&error)); clear_error(&error);
    CHECK(!memcmp(before,out,sizeof(before)) && !strcmp(copy,out->sql)); free(copy);
}
static void test_limits(Database *db) {
    Analysis a=source("SELECT i AS r,i+1 AS v FROM range(5) t(i)"); PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result result={0}; char *error=NULL;
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_SUM); fetch(db,&a,&d,0,&p); plan(db,&a,&d,&p,0,&out,&result); equal_text(&result,0,"3"); duckdb_destroy_result(&result);
    uint64_t queries=db->query_count; rejected(&a,&d,&p,0,1,&out); rejected(&a,&d,&p,0,strlen(out.sql),&out);
    p.count_pending=true; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.count_pending=false;
    duckdb_type type=p.columns[0].type; p.columns[0].type=DUCKDB_TYPE_LIST; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.columns[0].type=type;
    char *type_name=p.columns[0].type_name; bool numeric=p.columns[0].numeric;
    p.columns[0].type=DUCKDB_TYPE_VARCHAR; p.columns[0].numeric=false; p.columns[0].type_name="INTEGER[]";
    rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.columns[0].type_name="STRUCT(a INTEGER)";
    rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.columns[0].type_name=NULL;
    rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.columns[0].type=type; p.columns[0].type_name=type_name; p.columns[0].numeric=numeric;
    char *text=p.cells[0].text; p.cells[0].text=p.cells[2].text; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.cells[0].text=text;
    size_t count=p.column_count; p.column_count=1; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); p.column_count=count;
    SourceKind kind=a.kind; a.kind=SOURCE_NONE; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); a.kind=kind;
    size_t arity=d.counts[PIVOT_VALUES]; d.counts[PIVOT_VALUES]=DX_PIVOT_FIELDS+1; rejected(&a,&d,&p,0,DX_CHART_OTHER_SQL_BYTES,&out); d.counts[PIVOT_VALUES]=arity;
    CHECK(db->query_count==queries); /* validation/planning never executes SQL */
    p.row_count=3; CHECK(chart_other_plan(&a,&d,&p,0,NULL,0,DX_CHART_OTHER_SQL_BYTES,&out,&error)); CHECK(!error && !out.sql && !out.sql_bytes && !out.rank.has_other);
    p.row_count=5; page_free(&p); chart_other_plan_free(&out); pivot_free(&d); analysis_free(&a);
}
/* Deliberately violates the planner's key-proof precondition. Keep this opt-in
   natural counterexample distinct from accepted component business behavior. */
static void unproved_nul_counterexample(Database *db) {
    Analysis a=source("SELECT * FROM (VALUES ('A',100,1),('B',90,2),('C',80,3),('a'||chr(0)||'b',2,41),('z',1,1)) t(r,rank,v)");
    PivotDefinition d={0}; Page p={0}; ChartOtherPlan out={0}; duckdb_result raw={0},result={0}; char *error=NULL;
    assign(&d,PIVOT_ROWS,"r",AGG_SUM); assign(&d,PIVOT_VALUES,"rank",AGG_SUM); assign(&d,PIVOT_VALUES,"v",AGG_SUM);
    fetch(db,&a,&d,0,&p); CHECK(p.row_count==5 && p.column_count==3);
    CHECK(database_execute(db,p.sql,&raw,&error) && !error && duckdb_row_count(&raw)==p.row_count);
    duckdb_data_chunk chunk=duckdb_result_get_chunk(raw,0); CHECK(chunk && duckdb_data_chunk_get_size(chunk)==p.row_count);
    duckdb_vector vector=duckdb_data_chunk_get_vector(chunk,0); duckdb_string_t *values=duckdb_vector_get_data(vector); CHECK(values);
    size_t nul_row=SIZE_MAX; double expected=0; ChartOtherRank rank={0}; CHECK(chart_other_rank(&p,&d,1,&rank,&error));
    for (size_t row=0;row<p.row_count;row++) {
        size_t bytes=duckdb_string_t_length(values[row]); const char *data=duckdb_string_t_data(&values[row]);
        if (bytes==3 && !memcmp(data,"a\0b",3)) { nul_row=row; CHECK(strlen(p.cells[row*3].text)==1 && bytes!=strlen(p.cells[row*3].text)); }
        bool top=false; for (size_t i=0;i<rank.top_count;i++) top|=rank.top_rows[i]==row;
        if (!top) expected+=p.cells[row*3+2].number;
    }
    CHECK(nul_row!=SIZE_MAX && expected==42);
    duckdb_destroy_data_chunk(&chunk); duckdb_destroy_result(&raw);
    /* Legacy value accessors are kept on a separate result: mixing them with
       chunk retrieval is outside the SDK contract. This accessor loses length
       too, so its size cannot serve as the integration's independent proof. */
    CHECK(database_execute(db,p.sql,&raw,&error) && !error); duckdb_string accessor=duckdb_value_string(&raw,0,nul_row);
    CHECK(accessor.size==1 && !strcmp(accessor.data,"a")); duckdb_free(accessor.data);
    CHECK(chart_other_plan(&a,&d,&p,1,NULL,0,DX_CHART_OTHER_SQL_BYTES,&out,&error) && !error);
    execute(db,&out,&result); equal_number(&result,1,1); CHECK(strstr(out.sql,"cast_to_type('a',\"r\")"));
    puts("COUNTEREXAMPLE: intentionally unproved NUL key: chunk bytes=3, legacy accessor bytes=1, C-string bytes=1, expected Other=42, unproved plan Other=1; no producer enabled");
    duckdb_destroy_result(&raw); duckdb_destroy_result(&result); chart_other_plan_free(&out); page_free(&p); pivot_free(&d); analysis_free(&a); free(error);
}
int main(int argc,char **argv) {
    Database db={0}; char *error=NULL; CHECK(database_open(&db,&error)); CHECK(!error);
    if (argc==2 && !strcmp(argv[1],"--unproved-nul-counterexample")) {
        unproved_nul_counterexample(&db); database_close(&db); free(error); return 0;
    }
    CHECK(argc==1);
    test_rank(); test_weighted(&db); test_null_text(&db); test_exact_typed(&db); test_loaded_window(&db); test_filters_precision(&db); test_limits(&db);
    database_close(&db); free(error); printf("PASS: %u Chart Other component checks; real source AVG/distinct, no App/UI activation\n",checks); return 0;
}
