#include "ui/chartview.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL chartview:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
enum { VALUE_RANK,VALUE_AVERAGE,VALUE_DISTINCT };
/* Independent raw fixture: A/B/C each contribute one value100; D contributes
   (0,"x"), E contributes (10,"x"),(10,"y"),(10,"z"). The excluded union
   therefore has AVG7.5/distinct3, unlike averaging0/10 or summing1/3. These
   tests supply owned typed results, not a source/key proof or an SQL engine. */
static Page owned_matrix(PivotDefinition *d,const unsigned *kinds,size_t measures) {
    const char *names[]={"rank","v","customer"}; Aggregate aggregates[]={AGG_SUM,AGG_AVG,AGG_DISTINCT};
    const char *raw[5][3]={{"100","100","1"},{"90","100","1"},{"80","100","1"},{"2","0","1"},{"3","10","3"}};
    const char *keys[]={"A","B","C","D","E"}; char *error=NULL;
    CHECK(pivot_assign(d,PIVOT_ROWS,"r",AGG_SUM,&error) && !error);
    Page p={.row_count=5,.total=5,.column_count=1+measures,.sql=dx_strdup("SELECT * FROM captured_weighted_pivot")};
    p.columns=calloc(p.column_count,sizeof(*p.columns)); p.cells=calloc(p.row_count*p.column_count,sizeof(*p.cells)); CHECK(p.columns && p.cells);
    p.columns[0]=(Column){.name=dx_strdup("r"),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    for (size_t m=0;m<measures;m++) {
        unsigned k=kinds[m]; CHECK(k<=VALUE_DISTINCT && pivot_assign(d,PIVOT_VALUES,names[k],aggregates[k],&error) && !error);
        p.columns[1+m]=(Column){.name=dx_strdup(names[k]),.type_name=dx_strdup(k==VALUE_DISTINCT ? "BIGINT" : "DOUBLE"),.type=k==VALUE_DISTINCT ? DUCKDB_TYPE_BIGINT : DUCKDB_TYPE_DOUBLE,.numeric=true,.number_type={.kind=k==VALUE_DISTINCT ? NUM_I64 : NUM_FLOAT}};
        for (size_t r=0;r<5;r++) p.cells[r*p.column_count+1+m]=(Cell){.text=dx_strdup(raw[r][k]),.number=strtod(raw[r][k],NULL)};
    }
    for (size_t r=0;r<5;r++) p.cells[r*p.column_count].text=dx_strdup(keys[r]);
    p.bytes=p.column_count*sizeof(Column)+p.row_count*p.column_count*sizeof(Cell)+strlen(p.sql)+1;
    for (size_t c=0;c<p.column_count;c++) p.bytes+=strlen(p.columns[c].name)+strlen(p.columns[c].type_name)+2;
    for (size_t i=0;i<p.row_count*p.column_count;i++) p.bytes+=strlen(p.cells[i].text)+1;
    return p;
}
static PageOther *owned_other(const Page *p,size_t variants,const size_t *rank,const size_t top[2][3],const char *const values[2][3]) {
    PageOther *o=calloc(1,sizeof(*o)); CHECK(o);
    o->variant_count=variants; o->keys=1; o->measures=p->column_count-1; o->value_columns=o->measures; o->top_count=3;
    for (size_t m=0;m<DX_PIVOT_FIELDS;m++) o->measure_variant[m]=SIZE_MAX;
    for (size_t m=0;m<o->measures;m++) o->measure_variant[m]=variants==1 ? 0 : m;
    for (size_t v=0;v<variants;v++) {
        o->rank_measure[v]=rank[v]; memcpy(o->top_rows[v],top[v],sizeof(o->top_rows[v]));
        Page *q=&o->variants[v]; q->row_count=q->total=1; q->column_count=o->value_columns;
        q->columns=calloc(q->column_count,sizeof(*q->columns)); q->cells=calloc(q->column_count,sizeof(*q->cells)); CHECK(q->columns && q->cells);
        q->bytes=q->column_count*(sizeof(Column)+sizeof(Cell));
        for (size_t c=0;c<q->column_count;c++) {
            const Column *parent=&p->columns[1+c]; q->columns[c]=(Column){.name=dx_printf("__dx_other_%zu",c),.type_name=dx_strdup(parent->type_name),.type=parent->type,.number_type=parent->number_type,.numeric=true};
            q->cells[c]=(Cell){.text=values[v][c] ? dx_strdup(values[v][c]) : NULL,.is_null=!values[v][c],.number=values[v][c] ? strtod(values[v][c],NULL) : 0};
            q->bytes+=strlen(q->columns[c].name)+strlen(q->columns[c].type_name)+2+(values[v][c] ? strlen(values[v][c])+1 : 0);
        }
    }
    return o;
}
static void adopt(Page *p,PageOther **o) {
    PageOther *owned=*o; size_t before=p->bytes; char *error=NULL;
    CHECK(page_other_adopt(p,o,&error) && !error && !*o && p->other==owned && p->other_status==PAGE_OTHER_READY);
    CHECK(p->bytes==before+owned->bytes && page_other_values(p,0)==&owned->variants[owned->measure_variant[0]]);
}
static void free_fixture(Page *p,PivotDefinition *d) {
    page_free(p); CHECK(!p->columns && !p->cells && !p->other && !p->bytes && p->other_status==PAGE_OTHER_NONE);
    page_free(p); pivot_free(d);
}
static void expect_fallback(const Page *p,const PivotDefinition *d) {
    UIChartView plot; CHECK(!page_other_values(p,1));
    CHECK(ui_chart_view(p,d,1,&plot) && !plot.other_values && !plot.other && plot.non_additive && plot.series==3);
    CHECK(ui_chart_view(p,d,0,&plot) && !plot.other_values && plot.other && !plot.non_additive && plot.series==4);
}
static void test_owned_source_other(void) {
    const unsigned kinds[]={VALUE_RANK,VALUE_AVERAGE,VALUE_DISTINCT}; PivotDefinition d={0}; Page p=owned_matrix(&d,kinds,3); UIChartView plot;
    CHECK(ui_chart_view(&p,&d,1,&plot) && !plot.other && plot.non_additive);
    p.other_status=PAGE_OTHER_UNAVAILABLE; expect_fallback(&p,&d); p.other_status=PAGE_OTHER_NONE;
    const size_t rank[]={0},top[2][3]={{0,1,2}}; const char *values[2][3]={{"5","7.5","3"}};
    PageOther *o=owned_other(&p,1,rank,top,values); adopt(&p,&o);
    CHECK(ui_chart_view(&p,&d,1,&plot) && plot.other && !plot.non_additive && plot.series==4 && plot.other_values==&p.other->variants[0]);
    CHECK(plot.present[3][0] && plot.values[3][0]==7.5 && plot.values[3][0]!=(p.cells[3*4+2].number+p.cells[4*4+2].number)/2);
    CHECK(ui_chart_view(&p,&d,2,&plot) && plot.other && plot.values[3][0]==3 && plot.values[3][0]!=p.cells[3*4+3].number+p.cells[4*4+3].number);
    CHECK(ui_chart_view(&p,&d,0,&plot) && plot.values[3][0]==5 && plot.other_values==&p.other->variants[0]);
    /* Borrowed prefixes/reshaped owners do not own these pointers and are
       never freed. Every change must reject the source-derived cache. */
    Page borrowed=p; borrowed.row_count=4; expect_fallback(&borrowed,&d);
    borrowed=p; borrowed.total++; expect_fallback(&borrowed,&d);
    borrowed=p; borrowed.offset=1; borrowed.total++; expect_fallback(&borrowed,&d);
    borrowed=p; borrowed.count_pending=true; expect_fallback(&borrowed,&d);
    Cell cells[20]; memcpy(cells,p.cells,sizeof(cells)); borrowed=p; borrowed.cells=cells; expect_fallback(&borrowed,&d);
    Column columns[4]; memcpy(columns,p.columns,sizeof(columns)); borrowed=p; borrowed.columns=columns; expect_fallback(&borrowed,&d);
    char *same_sql=dx_strdup(p.sql); borrowed=p; borrowed.sql=same_sql; expect_fallback(&borrowed,&d); free(same_sql);
    free_fixture(&p,&d);
    /* These are contained owned graphs with deliberately obsolete ranking
       metadata; adoption is not a definition proof. The UI validates rank. */
    for (unsigned mismatch=0;mismatch<3;mismatch++) {
        p=owned_matrix(&d,kinds,3); o=owned_other(&p,1,rank,top,values);
        if (!mismatch) o->top_rows[0][0]=3; else if (mismatch==1) o->rank_measure[0]=1; else o->top_count=2;
        if (mismatch==2) {
            char *error=NULL; PageOther *before=o; size_t bytes=p.bytes;
            CHECK(!page_other_adopt(&p,&o,&error) && error && o==before && !p.other && p.bytes==bytes);
            free(error); page_other_free(o); o=NULL;
        } else {
            adopt(&p,&o); CHECK(page_other_values(&p,1));
            CHECK(ui_chart_view(&p,&d,1,&plot) && !plot.other_values && !plot.other && plot.non_additive);
            CHECK(ui_chart_view(&p,&d,0,&plot) && !plot.other_values && plot.other && plot.values[3][0]==5);
        }
        free_fixture(&p,&d);
    }
}
static void test_rank_variants_and_typed_values(void) {
    const unsigned kinds[]={VALUE_AVERAGE,VALUE_DISTINCT}; PivotDefinition d={0}; Page p=owned_matrix(&d,kinds,2); UIChartView plot;
    const size_t rank[]={0,1},top[2][3]={{0,1,2},{4,0,1}}; const char *values[2][3]={{"7.5","3"},{"50","2"}};
    PageOther *o=owned_other(&p,2,rank,top,values); adopt(&p,&o); Page before=p; PageOther cache=*p.other;
    for (unsigned cycle=0;cycle<8;cycle++) {
        CHECK(ui_chart_view(&p,&d,0,&plot) && plot.other_values==&p.other->variants[0] && plot.values[3][0]==7.5 && !plot.non_additive);
        CHECK(plot.rows[0]==0 && plot.rows[1]==1 && plot.rows[2]==2);
        CHECK(ui_chart_view(&p,&d,1,&plot) && plot.other_values==&p.other->variants[1] && plot.values[3][0]==2 && !plot.non_additive);
        CHECK(plot.rows[0]==4 && plot.rows[1]==0 && plot.rows[2]==1);
    }
    CHECK(!memcmp(&before,&p,sizeof(p)) && !memcmp(&cache,p.other,sizeof(cache))); free_fixture(&p,&d);
    const unsigned with_sum[]={VALUE_RANK,VALUE_AVERAGE}; const char *typed[]={NULL,"0","nan","inf"};
    for (size_t i=0;i<4;i++) {
        p=owned_matrix(&d,with_sum,2);
        for (size_t r=3;r<5;r++) { Cell *c=&p.cells[r*3+2]; p.bytes-=strlen(c->text)+1; free(c->text); *c=(Cell){.text=typed[i] ? dx_strdup(typed[i]) : NULL,.is_null=!typed[i],.number=typed[i] ? strtod(typed[i],NULL) : 0}; if (c->text) p.bytes+=strlen(c->text)+1; }
        const char *readout[2][3]={{"5",typed[i]}}; o=owned_other(&p,1,rank,top,readout); adopt(&p,&o);
        CHECK(ui_chart_view(&p,&d,1,&plot) && plot.other && !plot.non_additive && plot.other_values);
        CHECK(plot.present[3][0]==(i==1) && plot.non_finite==(i>=2));
        if (i==1) CHECK(plot.values[3][0]==0);
        CHECK(p.other->variants[0].cells[1].is_null==(i==0)); free_fixture(&p,&d);
    }
}
int main(void) {
    PivotDefinition d={.counts={[PIVOT_ROWS]=1,[PIVOT_VALUES]=2},.fields={[PIVOT_VALUES]={{.name="Amount",.aggregate=AGG_SUM},{.name="Amount",.aggregate=AGG_COUNT}}}};
    Column columns[7]={0}; for (size_t c=1;c<7;c++) columns[c].numeric=true;
    Cell cells[35]={0}; const double values[5][6]={{100,1,200,2,300,3},{-80,4,-90,5,-100,6},{50,7,60,8,70,9},{20,10,30,11,40,12},{10,13,20,14,30,15}};
    for (size_t r=0;r<5;r++) for (size_t c=0;c<6;c++) cells[r*7+c+1].number=values[r][c];
    Page p={.columns=columns,.cells=cells,.row_count=5,.total=5,.column_count=7}; UIChartView chart;
    CHECK(ui_chart_view(&p,&d,0,&chart)); CHECK(chart.valid && chart.categories==3 && chart.series==4 && chart.other && !chart.truncated);
    CHECK(chart.rows[0]==0 && chart.rows[1]==1 && chart.rows[2]==2 && chart.values[3][0]==30 && chart.values[3][1]==50 && chart.values[3][2]==70 && chart.low==-100 && chart.high==300);
    CHECK(ui_chart_view(&p,&d,1,&chart)); CHECK(chart.rows[0]==0 && chart.rows[1]==1 && chart.rows[2]==2 && chart.values[0][0]==1 && chart.values[3][0]==23 && chart.values[3][2]==27 && chart.low==0);
    cells[2*7+1].is_null=true; CHECK(ui_chart_view(&p,&d,0,&chart) && !chart.present[2][0] && chart.present[3][0]); cells[2*7+1].is_null=false;
    d.fields[PIVOT_VALUES][0].aggregate=AGG_AVG; CHECK(ui_chart_view(&p,&d,0,&chart) && chart.series==3 && !chart.other && chart.non_additive); d.fields[PIVOT_VALUES][0].aggregate=AGG_SUM;
    p.total=500; CHECK(ui_chart_view(&p,&d,0,&chart) && chart.truncated); p.total=5;
    cells[0*7+1].number=INFINITY; CHECK(ui_chart_view(&p,&d,0,&chart) && chart.non_finite && !chart.present[0][0]); cells[1].number=100;
    columns[1].numeric=false; CHECK(!ui_chart_view(&p,&d,0,&chart)); columns[1].numeric=true;
    CHECK(!ui_chart_view(&p,&d,2,&chart)); p.row_count=DX_PAGE_ROWS+1; CHECK(!ui_chart_view(&p,&d,0,&chart)); p.row_count=5;
    p.column_count=6; CHECK(!ui_chart_view(&p,&d,0,&chart)); p.column_count=7;
    double ticks[5]; ui_chart_ticks(-100,300,ticks); CHECK(ticks[0]==-100 && ticks[4]==300 && ticks[1]==0);
    ui_chart_ticks(-427000,335281.6,ticks); CHECK(ticks[0]==-500000 && ticks[4]==500000);
    ui_chart_ticks(-DBL_MAX,DBL_MAX,ticks); for (size_t i=0;i<5;i++) CHECK(isfinite(ticks[i])); CHECK(ticks[0]==-DBL_MAX && ticks[2]==0 && ticks[4]==DBL_MAX);
    test_owned_source_other(); test_rank_variants_and_typed_values();
    puts("PASS: bounded cached matrix-to-chart mapping, Column X categories, ranked Row series, selected measure, Other aggregates, NULL/non-finite/partial results and readable finite ticks"); printf("%u chart checks\n",checks); return 0;
}
