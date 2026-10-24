#include "workbook/definition_state.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL definition state:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void analysis(void) {
    Analysis a={.kind=SOURCE_PARQUET,.source_version=UINT64_MAX,.path="/source",.schema="",.table="NULL",.query="SELECT * FROM input",.unique_rows=true,
        .filter_column="A",.filter_value="9007199254740993.0001",.filter_op=FILTER_EQUAL,.filter_count=1,.sort_column="A",.sort_count=1,.aggregate=AGG_AVG,.group_column="Group",.measure_column="Amount"};
    a.filters[0]=(AnalysisFilter){.column="B",.value="",.op=FILTER_CONTAINS}; a.sorts[0]=(AnalysisSort){"B",true};
    Analysis b=a; CHECK(definition_analysis_equal(&a,&b)); b.filter_number=(NumberType){NUM_DECIMAL,38,4}; b.filters[0].number=(NumberType){NUM_I128,0,0}; CHECK(definition_analysis_equal(&a,&b));
#define DIFFER(field,value) do { b=a; b.field=(value); CHECK(!definition_analysis_equal(&a,&b)); } while (0)
    DIFFER(kind,SOURCE_CSV); DIFFER(source_version,0); DIFFER(path,"/other"); DIFFER(schema,NULL); DIFFER(table,"Other"); DIFFER(query,"SELECT Id FROM input");
    DIFFER(unique_rows,false); DIFFER(filter_column,"Other"); DIFFER(filter_value,"9007199254740993.0002"); DIFFER(filter_op,FILTER_GREATER); DIFFER(filter_count,0);
    DIFFER(group_column,"Other"); DIFFER(measure_column,"Other"); DIFFER(aggregate,AGG_SUM); DIFFER(sort_column,"Other"); DIFFER(descending,true); DIFFER(sort_count,0);
    DIFFER(filters[0].column,"Other"); DIFFER(filters[0].value,NULL); DIFFER(filters[0].op,FILTER_EQUAL); DIFFER(sorts[0].column,"Other"); DIFFER(sorts[0].descending,false);
    DIFFER(filter_count,DX_ANALYSIS_FILTERS); DIFFER(sort_count,DX_ANALYSIS_SORTS);
#undef DIFFER
    b=a; b.filter_column="B"; b.filter_value=""; b.filter_op=FILTER_CONTAINS; b.filters[0]=(AnalysisFilter){.column="A",.value=a.filter_value,.op=FILTER_EQUAL};
    CHECK(!definition_analysis_equal(&a,&b)); /* Same result predicates, different authored display order. */
    a=(Analysis){.kind=SOURCE_DATABASE,.path="/data.duckdb"}; b=a; b.schema="main"; b.table="First";
    CHECK(!definition_analysis_equal(&a,&b)); /* Worker completion needs a baseline owner, never an equality waiver. */
    a=(Analysis){.kind=SOURCE_CSV,.filter_column="A",.filter_op=FILTER_EQUAL}; b=a; b.filter_value=""; CHECK(!definition_analysis_equal(&a,&b)); a.filter_value="NULL"; CHECK(!definition_analysis_equal(&a,&b));
    CHECK(!definition_analysis_equal(NULL,&a) && !definition_analysis_equal(&a,NULL));
}
static void pivot_and_marks(void) {
    PivotDefinition a={0},b;
    a.counts[PIVOT_ROWS]=2; a.fields[PIVOT_ROWS][0]=(PivotField){.name="Date",.grain=PIVOT_DATE_YEAR}; a.fields[PIVOT_ROWS][1]=(PivotField){.name="Date",.grain=PIVOT_DATE_MONTH};
    a.counts[PIVOT_VALUES]=1; a.fields[PIVOT_VALUES][0]=(PivotField){.name="Amount",.aggregate=AGG_AVG};
    b=a; b.fields[PIVOT_ROWS][0].number=(NumberType){NUM_DECIMAL,38,4}; CHECK(definition_pivot_equal(&a,&b));
    b=a; PivotField swap=b.fields[PIVOT_ROWS][0]; b.fields[PIVOT_ROWS][0]=b.fields[PIVOT_ROWS][1]; b.fields[PIVOT_ROWS][1]=swap; CHECK(!definition_pivot_equal(&a,&b));
#define DIFFER(field,value) do { b=a; b.field=(value); CHECK(!definition_pivot_equal(&a,&b)); } while (0)
    DIFFER(counts[PIVOT_ROWS],1); DIFFER(fields[PIVOT_ROWS][0].name,"Other"); DIFFER(fields[PIVOT_ROWS][0].grain,PIVOT_DATE_QUARTER);
    DIFFER(fields[PIVOT_VALUES][0].aggregate,AGG_SUM); DIFFER(fields[PIVOT_VALUES][0].filter,FILTER_EQUAL); DIFFER(fields[PIVOT_VALUES][0].value,""); DIFFER(counts[PIVOT_ROWS],9);
#undef DIFFER
    CHECK(!definition_pivot_equal(NULL,&a));
    MarkDefinition x={0},y; x.counts[MARKS_COLOR]=1; x.fields[MARKS_COLOR][0]=(MarkField){.name="Region",.aggregate=AGG_AVG}; y=x; CHECK(definition_marks_equal(&x,&y));
#define DIFFER(field,value) do { y=x; y.field=(value); CHECK(!definition_marks_equal(&x,&y)); } while (0)
    DIFFER(fields[MARKS_COLOR][0].name,"Other"); DIFFER(fields[MARKS_COLOR][0].aggregate,AGG_SUM); DIFFER(fields[MARKS_COLOR][0].grain,PIVOT_DATE_MONTH);
    DIFFER(fields[MARKS_COLOR][0].measure,true); DIFFER(counts[MARKS_COLOR],0); DIFFER(counts[MARKS_COLOR],9); DIFFER(counts[MARKS_SIZE],2);
#undef DIFFER
    CHECK(!definition_marks_equal(&x,NULL));
}
static void objects(void) {
    SheetObject a[3]={{.id=1,.name="Raw",.kind=OBJECT_TABLE,.row=0,.rows=5,.columns=4,.source={.kind=SOURCE_CSV,.path="/source"}},
        {.id=2,.name="Pivot",.kind=OBJECT_PIVOT,.row=10,.rows=5,.columns=4,.source_id=1},
        {.id=3,.name="Chart",.kind=OBJECT_CHART,.row=20,.rows=12,.columns=6,.mark=MARK_LINE,.source_id=1}},b[3];
    for (unsigned i=0;i<3;i++) b[i]=a[i];
    ObjectSnapshot left={a,3},right={b,3}; CHECK(definition_objects_equal(&left,&right));
    b[0].rows=1000002; b[0].columns=100; b[1].rows=42; b[1].columns=10;
    for (unsigned i=0;i<3;i++) b[i].revision=UINT64_MAX;
    CHECK(definition_objects_equal(&left,&right)); /* Async result extents/epochs are not authored edits. */
#define DIFFER(index,field,value) do { for (unsigned k=0;k<3;k++) b[k]=a[k]; b[index].field=(value); CHECK(!definition_objects_equal(&left,&right)); } while (0)
    DIFFER(0,id,4); DIFFER(0,name,"Other"); DIFFER(0,row,1); DIFFER(0,col,1); DIFFER(1,kind,OBJECT_CHART); DIFFER(1,source_id,0);
    DIFFER(2,rows,13); DIFFER(2,columns,7); DIFFER(2,mark,MARK_BAR); DIFFER(2,chart_measure,1);
    DIFFER(0,source.source_version,1); DIFFER(0,source.path,"/other"); DIFFER(0,source.schema,"main"); DIFFER(0,source.table,"First");
    DIFFER(1,pivot.counts[PIVOT_VALUES],1); DIFFER(2,marks.counts[MARKS_COLOR],1);
#undef DIFFER
    for (unsigned i=0;i<3;i++) b[i]=a[i]; SheetObject swap=b[1]; b[1]=b[2]; b[2]=swap; CHECK(!definition_objects_equal(&left,&right));
    right.count=2; CHECK(!definition_objects_equal(&left,&right)); right.count=33; CHECK(!definition_objects_equal(&left,&right));
    right=(ObjectSnapshot){NULL,3}; CHECK(!definition_objects_equal(&left,&right));
    CHECK(!definition_objects_equal(NULL,&right)); left=(ObjectSnapshot){0}; right=(ObjectSnapshot){0}; CHECK(definition_objects_equal(&left,&right));
}
int main(void) { analysis(); pivot_and_marks(); objects(); printf("PASS: %u authored definition/preimage projection checks\n",checks); return 0; }
