#include "chartencoding.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"FAIL encoding:%d: %s\n",__LINE__,#c); exit(1); } } while (0)
static unsigned parent_frees,other_frees;
static void parent_text_free(void *text) { parent_frees++; free(text); }
static void other_text_free(void *text) { other_frees++; free(text); }
enum { SOURCE_RANK,SOURCE_AVERAGE,SOURCE_DISTINCT,SOURCE_EXACT };
static Page owned_matrix(PivotDefinition *d,const unsigned *kinds,size_t measures) {
    const char *names[]={"rank","v","customer","exact"}; Aggregate aggs[]={AGG_SUM,AGG_AVG,AGG_DISTINCT,AGG_SUM};
    const char *keys[]={"A","B","C","D","E"};
    const char *raw[5][4]={{"100","100","1","100"},{"90","100","1","90"},{"80","100","1","80"},{"2","0","1","12345678901234567890.1250"},{"3","10","3","12345678901234567890.1250"}};
    char *error=NULL; CHECK(pivot_assign(d,PIVOT_ROWS,"r",AGG_SUM,&error) && !error);
    Page p={.row_count=5,.total=5,.column_count=1+measures,.sql=dx_strdup("SELECT * FROM captured_weighted_pivot"),.release_cell_text=parent_text_free};
    p.columns=calloc(p.column_count,sizeof(*p.columns)); p.cells=calloc(p.row_count*p.column_count,sizeof(*p.cells)); CHECK(p.columns && p.cells);
    p.columns[0]=(Column){.name=dx_strdup("r"),.type_name=dx_strdup("VARCHAR"),.type=DUCKDB_TYPE_VARCHAR};
    for (size_t m=0;m<measures;m++) {
        unsigned k=kinds[m]; CHECK(k<=SOURCE_EXACT && pivot_assign(d,PIVOT_VALUES,names[k],aggs[k],&error) && !error);
        p.columns[1+m]=(Column){.name=dx_strdup(names[k]),.numeric=true}; Column *c=&p.columns[1+m];
        if (k==SOURCE_DISTINCT) { c->type_name=dx_strdup("BIGINT"); c->type=DUCKDB_TYPE_BIGINT; c->number_type.kind=NUM_I64; }
        else if (k==SOURCE_EXACT) { c->type_name=dx_strdup("DECIMAL(38,4)"); c->type=DUCKDB_TYPE_DECIMAL; c->number_type=(NumberType){NUM_DECIMAL,38,4}; }
        else { c->type_name=dx_strdup("DOUBLE"); c->type=DUCKDB_TYPE_DOUBLE; c->number_type.kind=NUM_FLOAT; }
        for (size_t r=0;r<5;r++) p.cells[r*p.column_count+1+m]=(Cell){.text=dx_strdup(raw[r][k]),.number=strtod(raw[r][k],NULL)};
    }
    for (size_t r=0;r<5;r++) p.cells[r*p.column_count].text=dx_strdup(keys[r]);
    p.bytes=p.column_count*sizeof(Column)+p.row_count*p.column_count*sizeof(Cell)+strlen(p.sql)+1;
    for (size_t c=0;c<p.column_count;c++) p.bytes+=strlen(p.columns[c].name)+strlen(p.columns[c].type_name)+2;
    for (size_t i=0;i<p.row_count*p.column_count;i++) p.bytes+=strlen(p.cells[i].text)+1;
    return p;
}
static PageOther *owned_readouts(const Page *p,size_t variants,const size_t rank[2],const size_t top[2][3],const char *const values[2][4]) {
    PageOther *o=calloc(1,sizeof(*o)); CHECK(o); o->keys=1; o->measures=o->value_columns=p->column_count-1; o->top_count=3; o->variant_count=variants;
    for (size_t m=0;m<DX_PIVOT_FIELDS;m++) o->measure_variant[m]=SIZE_MAX;
    for (size_t m=0;m<o->measures;m++) o->measure_variant[m]=variants==1 ? 0 : m;
    for (size_t v=0;v<variants;v++) {
        o->rank_measure[v]=rank[v]; memcpy(o->top_rows[v],top[v],sizeof(o->top_rows[v]));
        Page *q=&o->variants[v]; q->row_count=q->total=1; q->column_count=o->value_columns; q->release_cell_text=other_text_free;
        q->columns=calloc(q->column_count,sizeof(*q->columns)); q->cells=calloc(q->column_count,sizeof(*q->cells)); CHECK(q->columns && q->cells);
        q->bytes=q->column_count*(sizeof(Column)+sizeof(Cell));
        for (size_t c=0;c<q->column_count;c++) {
            const Column *expected=&p->columns[c+1]; q->columns[c]=(Column){.name=dx_printf("__dx_other_%zu",c),.type_name=dx_strdup(expected->type_name),.numeric=true,.type=expected->type,.number_type=expected->number_type};
            q->cells[c]=(Cell){.text=values[v][c] ? dx_strdup(values[v][c]) : NULL,.is_null=!values[v][c],.number=values[v][c] ? strtod(values[v][c],NULL) : 0};
            q->bytes+=strlen(q->columns[c].name)+strlen(q->columns[c].type_name)+2+(values[v][c] ? strlen(values[v][c])+1 : 0);
        }
    }
    return o;
}
static void adopt(Page *p,PageOther **o) {
    PageOther *owned=*o; char *error=NULL; CHECK(page_other_adopt(p,o,&error) && !error && !*o && p->other==owned && p->other_status==PAGE_OTHER_READY);
}
static void test_typed_other_encodings(void) {
    /* Source D=(0,x), E=(10,x),(10,y),(10,z): source AVG7.5/distinct3
       cannot be reconstructed by folding the two displayed aggregate rows. */
    const unsigned kinds[]={SOURCE_RANK,SOURCE_AVERAGE,SOURCE_DISTINCT,SOURCE_EXACT};
    PivotDefinition d={0}; Page p=owned_matrix(&d,kinds,4); UIChartView view;
    const size_t rank[2]={0},top[2][3]={{0,1,2}};
    const char *exact="24691357802469135780.2500",*values[2][4]={{"5","7.5","3",exact}};
    PageOther *o=owned_readouts(&p,1,rank,top,values); adopt(&p,&o); CHECK(ui_chart_view(&p,&d,0,&view) && view.other);
    MarkField avg={.name="v",.measure=true,.aggregate=AGG_AVG},distinct={.name="customer",.measure=true,.aggregate=AGG_DISTINCT},decimal={.name="exact",.measure=true,.aggregate=AGG_SUM};
    MarkDefinition marks={0}; char *error=NULL;
    CHECK(marks_assign(&marks,MARKS_SIZE,&avg,&error) && marks_assign(&marks,MARKS_LABEL,&avg,&error));
    CHECK(marks_assign(&marks,MARKS_DETAIL,&distinct,&error) && marks_assign(&marks,MARKS_TOOLTIP,&decimal,&error) && !error);
    UIEncodingBinding a=ui_encoding_binding(&d,&avg),n=ui_encoding_binding(&d,&distinct),e=ui_encoding_binding(&d,&decimal);
    CHECK(a.kind==UI_BIND_MEASURE && a.index==1 && n.index==2 && e.index==3);
    Page before=p; PageOther cache=*p.other; Cell parent_cells[25]; memcpy(parent_cells,p.cells,sizeof(parent_cells));
    for (size_t plot=0;plot<4;plot++) {
        CHECK(ui_chart_view(&p,&d,plot,&view) && view.other_values==&p.other->variants[0]);
        UIEncodingValue average=ui_encoding_value(&p,&d,&view,a,3,0),count=ui_encoding_value(&p,&d,&view,n,3,0),large=ui_encoding_value(&p,&d,&view,e,3,0);
        CHECK(average.available && !average.is_null && average.numeric && !average.approximate && average.number==7.5 && !strcmp(average.raw,"7.5"));
        CHECK(count.available && !count.is_null && count.numeric && !count.approximate && count.number==3 && !strcmp(count.raw,"3"));
        CHECK(average.number!=(p.cells[3*5+2].number+p.cells[4*5+2].number)/2 && count.number!=p.cells[3*5+3].number+p.cells[4*5+3].number);
        CHECK(large.available && !large.is_null && large.numeric && !large.approximate && !strcmp(large.raw,exact) && large.raw==p.other->variants[0].cells[3].text);
        CHECK(strtod(large.raw,NULL)==strtod("24691357802469135780.2501",NULL)); /* double cannot retain this distinction; raw text must. */
        UIChartEncoding encoding; CHECK(ui_chart_encoding(&p,&d,&marks,&view,true,&encoding));
        CHECK(encoding.active[MARKS_SIZE] && encoding.sized[3][0] && encoding.sizes[3][0]==7.5 && !encoding.partial[MARKS_SIZE]);
        CHECK(encoding.active[MARKS_LABEL] && encoding.bindings[MARKS_LABEL][0].index==1 && encoding.active[MARKS_DETAIL] && encoding.bindings[MARKS_DETAIL][0].index==2);
        CHECK(encoding.active[MARKS_TOOLTIP] && encoding.bindings[MARKS_TOOLTIP][0].index==3);
    }
    CHECK(!memcmp(&p,&before,sizeof(p)) && !memcmp(p.other,&cache,sizeof(cache)) && !memcmp(p.cells,parent_cells,sizeof(parent_cells)));
    /* An otherwise identical borrowed prefix must not leak the cached exact
       source readout. Legacy SUM folding stays explicitly approximate. */
    Page prefix=p; prefix.row_count=4; CHECK(ui_chart_view(&prefix,&d,0,&view) && !view.other_values);
    UIEncodingValue missing=ui_encoding_value(&prefix,&d,&view,a,3,0),fallback=ui_encoding_value(&prefix,&d,&view,e,3,0);
    CHECK(!missing.available && !missing.is_null && !missing.raw);
    CHECK(!ui_encoding_value(&prefix,&d,&view,n,3,0).available);
    CHECK(fallback.available && !fallback.is_null && fallback.approximate && !fallback.raw);
    unsigned parents=parent_frees,others=other_frees; page_free(&p); CHECK(parent_frees==parents+25 && other_frees==others+4 && !p.other && !p.cells);
    page_free(&p); CHECK(parent_frees==parents+25 && other_frees==others+4); marks_free(&marks); pivot_free(&d); free(error);
}
static void test_other_variants_and_nulls(void) {
    const unsigned kinds[]={SOURCE_AVERAGE,SOURCE_DISTINCT}; PivotDefinition d={0}; Page p=owned_matrix(&d,kinds,2); UIChartView view;
    const size_t ranks[2]={0,1},tops[2][3]={{0,1,2},{4,0,1}}; const char *values[2][4]={{"7.5","3"},{"50","2"}};
    PageOther *o=owned_readouts(&p,2,ranks,tops,values); adopt(&p,&o);
    for (size_t plot=0;plot<2;plot++) {
        CHECK(ui_chart_view(&p,&d,plot,&view) && view.other_values==&p.other->variants[plot]);
        UIEncodingValue avg=ui_encoding_value(&p,&d,&view,(UIEncodingBinding){UI_BIND_MEASURE,0},3,0),count=ui_encoding_value(&p,&d,&view,(UIEncodingBinding){UI_BIND_MEASURE,1},3,0);
        CHECK(avg.available && !avg.approximate && avg.number==(plot ? 50 : 7.5) && !strcmp(avg.raw,values[plot][0]));
        CHECK(count.available && !count.approximate && count.number==(plot ? 2 : 3) && !strcmp(count.raw,values[plot][1]));
    }
    unsigned parents=parent_frees,others=other_frees; page_free(&p); CHECK(parent_frees==parents+15 && other_frees==others+4); pivot_free(&d);
    const unsigned with_sum[]={SOURCE_RANK,SOURCE_AVERAGE}; const char *typed[]={NULL,"0","nan","inf"};
    for (size_t i=0;i<4;i++) {
        p=owned_matrix(&d,with_sum,2);
        for (size_t row=3;row<5;row++) { Cell *c=&p.cells[row*3+2]; p.bytes-=strlen(c->text)+1; free(c->text); *c=(Cell){.text=typed[i] ? dx_strdup(typed[i]) : NULL,.is_null=!typed[i],.number=typed[i] ? strtod(typed[i],NULL) : 0}; if (c->text) p.bytes+=strlen(c->text)+1; }
        const char *readout[2][4]={{"5",typed[i]}}; o=owned_readouts(&p,1,ranks,tops,readout); adopt(&p,&o);
        CHECK(ui_chart_view(&p,&d,0,&view) && view.other_values);
        UIEncodingValue v=ui_encoding_value(&p,&d,&view,(UIEncodingBinding){UI_BIND_MEASURE,1},3,0);
        CHECK(v.available && v.numeric && !v.approximate && v.is_null==(i==0));
        if (!i) CHECK(!v.raw); else CHECK(v.raw && !strcmp(v.raw,typed[i]));
        if (i==1) CHECK(v.number==0); else if (i>=2) CHECK(!isfinite(v.number));
        MarkDefinition marks={0}; char *error=NULL; MarkField avg={.name="v",.measure=true,.aggregate=AGG_AVG}; CHECK(marks_assign(&marks,MARKS_SIZE,&avg,&error) && !error);
        UIChartEncoding encoding; CHECK(ui_chart_encoding(&p,&d,&marks,&view,true,&encoding));
        CHECK(encoding.active[MARKS_SIZE] && encoding.sized[3][0]==(i==1) && encoding.partial[MARKS_SIZE]==(i!=1));
        if (i==1) CHECK(encoding.sizes[3][0]==0);
        parents=parent_frees; others=other_frees; page_free(&p); CHECK(parent_frees==parents+(i ? 15 : 13) && other_frees==others+(i ? 2 : 1));
        pivot_free(&d); marks_free(&marks); free(error);
    }
}
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
    marks_free(&marks); pivot_free(&definition); free(error); test_typed_other_encodings(); test_other_variants_and_nulls(); printf("PASS: %u cached encoding checks (field/aggregate/grain binding, typed NULL/empty/literal values, mixed Other, finite sizes, shapes, inactive mappings and bounds)\n",checks); return 0;
}
