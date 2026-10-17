#include "ui/chartview.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL chartview:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
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
    puts("PASS: bounded cached matrix-to-chart mapping, Column X categories, ranked Row series, selected measure, Other aggregates, NULL/non-finite/partial results and readable finite ticks"); printf("%u chart checks\n",checks); return 0;
}
