#include "chartencoding.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL encoding:%d: %s\n",__LINE__,#c); exit(1); } } while (0)
int main(void) {
    PivotDefinition definition={0}; char *error=NULL;
    CHECK(pivot_assign(&definition,PIVOT_ROWS,"Account",AGG_SUM,&error));
    CHECK(pivot_assign(&definition,PIVOT_ROWS,"Category",AGG_SUM,&error));
    CHECK(pivot_assign_grain(&definition,PIVOT_COLUMNS,"Date",AGG_SUM,PIVOT_DATE_MONTH,&error));
    CHECK(pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_SUM,&error));
    CHECK(pivot_assign(&definition,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
    Cell cells[30]={0}; Column columns[6]={0};
    char *july[]={"2026-07-01"},*august[]={"2026-08-01"};
    for (size_t c=2;c<6;c++) { columns[c].numeric=true; columns[c].categories=c<4 ? july : august; columns[c].category_count=1; columns[c].category_grains[0]=PIVOT_DATE_MONTH; }
    const char *accounts[]={"Operating","Operating","NULL","",NULL},*categories[]={"one","two","three","four","five"};
    char numeric[5][4][24];
    for (size_t r=0;r<5;r++) {
        cells[r*6]=(Cell){.text=(char *)accounts[r],.is_null=!accounts[r]}; cells[r*6+1].text=(char *)categories[r];
        for (size_t c=0;c<4;c++) { double value=c%2 ? r+1+c/2 : (r+1)*10+c/2*10; snprintf(numeric[r][c],24,"%.0f",value); cells[r*6+c+2]=(Cell){.text=numeric[r][c],.number=value}; }
    }
    Page page={.cells=cells,.columns=columns,.column_count=6,.row_count=5,.total=5}; UIChartView view;
    CHECK(ui_chart_view(&page,&definition,0,&view)); CHECK(view.rows[0]==4 && view.rows[1]==3 && view.rows[2]==2 && view.other);
    MarkField account={.name="Account"},category={.name="Category"},amount={.name="Amount",.measure=true,.aggregate=AGG_SUM},count={.name="Amount",.measure=true,.aggregate=AGG_COUNT},month={.name="Date",.grain=PIVOT_DATE_MONTH};
    UIEncodingBinding row=ui_encoding_binding(&definition,&account),value=ui_encoding_binding(&definition,&count),column=ui_encoding_binding(&definition,&month);
    CHECK(row.kind==UI_BIND_ROW && row.index==0); CHECK(value.kind==UI_BIND_MEASURE && value.index==1); CHECK(column.kind==UI_BIND_COLUMN && column.index==0);
    UIEncodingValue missing=ui_encoding_value(&page,&definition,&view,row,0,0),empty=ui_encoding_value(&page,&definition,&view,row,1,0),literal=ui_encoding_value(&page,&definition,&view,row,2,0);
    CHECK(missing.available && missing.is_null && !missing.raw); CHECK(empty.available && !empty.is_null && empty.raw && !*empty.raw); CHECK(literal.available && !literal.is_null && !strcmp(literal.raw,"NULL"));
    UIEncodingValue other=ui_encoding_value(&page,&definition,&view,row,3,0); CHECK(other.available && !other.is_null && !strcmp(other.raw,"Operating"));
    other=ui_encoding_value(&page,&definition,&view,(UIEncodingBinding){UI_BIND_ROW,1},3,0); CHECK(!other.available && !other.is_null);
    other=ui_encoding_value(&page,&definition,&view,value,3,0); CHECK(other.available && other.numeric && other.approximate && other.number==3 && !other.raw);
    UIEncodingValue month_value=ui_encoding_value(&page,&definition,&view,column,0,1); CHECK(month_value.available && !month_value.is_null && !strcmp(month_value.raw,"2026-08-01"));
    MarkDefinition marks={0}; CHECK(marks_assign(&marks,MARKS_COLOR,&account,&error)); CHECK(marks_assign(&marks,MARKS_SIZE,&count,&error)); CHECK(marks_assign(&marks,MARKS_LABEL,&amount,&error)); CHECK(marks_assign(&marks,MARKS_DETAIL,&category,&error)); CHECK(marks_assign(&marks,MARKS_TOOLTIP,&month,&error)); CHECK(marks_assign(&marks,MARKS_SHAPE,&month,&error));
    UIChartEncoding encoding; CHECK(ui_chart_encoding(&page,&definition,&marks,&view,true,&encoding));
    for (int role=0;role<DX_MARK_ROLES;role++) CHECK(encoding.active[role]);
    CHECK(encoding.colors[0]!=encoding.colors[1] && encoding.colors[1]!=encoding.colors[2] && encoding.colors[0]!=encoding.colors[2]);
    CHECK(encoding.sized[0][1] && encoding.sizes[0][1]==6 && encoding.size_max==6 && encoding.sized[3][0] && encoding.sizes[3][0]==3);
    CHECK(encoding.shapes[0][0]!=encoding.shapes[0][1]); CHECK(ui_chart_encoding(&page,&definition,&marks,&view,false,&encoding) && !encoding.active[MARKS_SHAPE]);
    CHECK(marks_assign(&marks,MARKS_COLOR,&category,&error)); CHECK(ui_chart_encoding(&page,&definition,&marks,&view,true,&encoding)); CHECK(encoding.active[MARKS_COLOR] && encoding.partial[MARKS_COLOR] && encoding.colors[3]==UINT64_MAX);
    MarkField unknown={.name="Not grouped"}; CHECK(marks_assign(&marks,MARKS_DETAIL,&unknown,&error)); CHECK(ui_chart_encoding(&page,&definition,&marks,&view,true,&encoding) && !encoding.active[MARKS_DETAIL]);
    CHECK(marks_assign(&marks,MARKS_SIZE,&account,&error)); CHECK(ui_chart_encoding(&page,&definition,&marks,&view,true,&encoding) && !encoding.active[MARKS_SIZE]);
    definition.fields[PIVOT_VALUES][1].aggregate=AGG_AVG; other=ui_encoding_value(&page,&definition,&view,value,3,0); CHECK(!other.available && !other.is_null); definition.fields[PIVOT_VALUES][1].aggregate=AGG_DISTINCT; other=ui_encoding_value(&page,&definition,&view,value,3,0); CHECK(!other.available); definition.fields[PIVOT_VALUES][1].aggregate=AGG_COUNT;
    UIChartView invalid=view; invalid.series=5; CHECK(!ui_chart_encoding(&page,&definition,&marks,&invalid,true,&encoding)); invalid=view; invalid.categories=129; CHECK(!ui_chart_encoding(&page,&definition,&marks,&invalid,true,&encoding));
    CHECK(!ui_encoding_value(&page,&definition,&view,value,4,0).available); CHECK(!ui_encoding_value(&page,&definition,&view,value,0,2).available); CHECK(!ui_encoding_value(&page,&definition,&view,(UIEncodingBinding){UI_BIND_MEASURE,9},0,0).available);
    CHECK(!ui_encoding_value(&page,&definition,&view,(UIEncodingBinding){(UIEncodingKind)9,SIZE_MAX},0,0).available);
    invalid=view; invalid.measure=9; CHECK(!ui_encoding_value(&page,&definition,&invalid,column,0,0).available);
    cells[0].is_null=cells[6].is_null=true;
    other=ui_encoding_value(&page,&definition,&view,row,3,0); CHECK(other.available && other.is_null);
    cells[0].is_null=cells[6].is_null=false;
    definition.fields[PIVOT_VALUES][1].aggregate=AGG_MIN; other=ui_encoding_value(&page,&definition,&view,value,3,0); CHECK(other.available && other.approximate && other.number==1);
    definition.fields[PIVOT_VALUES][1].aggregate=AGG_MAX; other=ui_encoding_value(&page,&definition,&view,value,3,0); CHECK(other.available && other.approximate && other.number==2);
    marks_free(&marks); pivot_free(&definition); free(error); printf("PASS: %u cached encoding checks (field/aggregate/grain binding, typed NULL/empty/literal values, mixed Other, finite sizes, shapes, inactive mappings and bounds)\n",checks); return 0;
}
