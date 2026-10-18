#include "stepview.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void label(char *out,size_t capacity,const char *format,...) {
    va_list args; va_start(args,format); int length=vsnprintf(out,capacity,format,args); va_end(args);
    if (length<0) { *out=0; return; }
    if ((size_t)length>=capacity) {
        size_t end=capacity-1,start=end;
        while (start && ((unsigned char)out[start-1]&0xc0)==0x80) start--;
        if (start) { unsigned char lead=(unsigned char)out[start-1]; size_t need=lead<0x80 ? 1 : lead<0xe0 ? 2 : lead<0xf0 ? 3 : 4; if (end-start+1<need) out[start-1]=0; }
    }
    for (char *p=out;*p;p++) if ((unsigned char)*p<32) *p=' ';
}
static void filter_step(UIStep *step,const char *column,FilterOp op,const char *value) {
    label(step->title,sizeof(step->title),"Filter %s",column);
    label(step->detail,sizeof(step->detail),"%s · %s",column,filter_label(op));
    if (op==FILTER_IN || op==FILTER_NOT_IN) {
        char **values=NULL; size_t count=0;
        if (!analysis_parse_values(value,&values,&count)) return;
        if (count==1) {
            if (!values[0]) { label(step->title,sizeof(step->title),"Filter %s IS %sNULL",column,op==FILTER_NOT_IN ? "NOT " : ""); label(step->detail,sizeof(step->detail),"%s is %sSQL NULL",column,op==FILTER_NOT_IN ? "not " : ""); }
            else { label(step->title,sizeof(step->title),"Filter %s %s %s",column,op==FILTER_IN ? "=" : "≠",*values[0] ? values[0] : "\"\""); label(step->detail,sizeof(step->detail),"%s %s '%s'",column,op==FILTER_IN ? "equals" : "excludes",values[0]); }
        } else label(step->detail,sizeof(step->detail),"%zu %s values",count,op==FILTER_IN ? "selected" : "excluded");
        for (size_t i=0;i<count;i++) free(values[i]); free(values);
    } else if (op==FILTER_EQUAL) {
        label(step->title,sizeof(step->title),"Filter %s = %s",column,value ? value : ""); label(step->detail,sizeof(step->detail),"%s equals '%s'",column,value ? value : "");
    } else if (op==FILTER_NULL || op==FILTER_NOT_NULL) {
        label(step->title,sizeof(step->title),"Filter %s IS %sNULL",column,op==FILTER_NOT_NULL ? "NOT " : "");
    } else if (op!=FILTER_COMPOSITE && op!=FILTER_BETWEEN && op!=FILTER_DATE_IN && op!=FILTER_DATE_NOT_IN) label(step->detail,sizeof(step->detail),"%s · %s",filter_label(op),value ? value : "");
}
size_t ui_source_steps_named(const Analysis *source,size_t fields,const char *display_filename,UIStep steps[DX_UI_STEPS]) {
    if (!source || source->filter_count>=DX_ANALYSIS_FILTERS || source->sort_count>=DX_ANALYSIS_SORTS || (unsigned)source->kind>SOURCE_DEMO) return 0;
    memset(steps,0,sizeof(*steps)*DX_UI_STEPS); size_t count=1; UIStep *first=&steps[0]; first->kind=UI_STEP_SOURCE;
    const char *name=display_filename;
    if (!name && source->kind!=SOURCE_NONE && (source->query || source->kind!=SOURCE_DEMO)) name=dx_basename(source->path ? source->path : "source");
    if (source->kind==SOURCE_NONE) { label(first->title,sizeof(first->title),"Blank worksheet"); label(first->detail,sizeof(first->detail),"Enter cells or open a data file"); }
    else if (source->query) { label(first->title,sizeof(first->title),"Source query"); label(first->detail,sizeof(first->detail),"SELECT over %s",name); }
    else { label(first->title,sizeof(first->title),"%s %s",source->kind==SOURCE_DATABASE ? "Connect" : "Import",source->kind==SOURCE_DEMO ? "sample data" : name);
        if (source->kind==SOURCE_DATABASE) label(first->detail,sizeof(first->detail),"%s.%s · %zu fields",source->schema ? source->schema : "main",source->table ? source->table : "Choose table",fields);
        else label(first->detail,sizeof(first->detail),"%s · %zu fields",source->kind==SOURCE_PARQUET ? "Parquet" : source->kind==SOURCE_CSV ? "Delimited text" : "Generated data",fields);
    }
    if (source->unique_rows) { UIStep *step=&steps[count++]; step->kind=UI_STEP_UNIQUE; label(step->title,sizeof(step->title),"Unique rows"); label(step->detail,sizeof(step->detail),"Exact matches across all source columns"); }
    for (size_t i=0;i<source->filter_count+(source->filter_column ? 1 : 0);i++) {
        const char *column=i ? source->filters[i-1].column : source->filter_column,*value=i ? source->filters[i-1].value : source->filter_value; FilterOp op=i ? source->filters[i-1].op : source->filter_op;
        if (!column) return 0; UIStep *step=&steps[count++]; step->kind=UI_STEP_FILTER; step->index=i; step->column=column; filter_step(step,column,op,value);
    }
    if (source->group_column) { UIStep *step=&steps[count++]; step->kind=UI_STEP_GROUP; step->column=source->group_column; label(step->title,sizeof(step->title),"Group %s",source->group_column); label(step->detail,sizeof(step->detail),"%s · %s",aggregate_caption(source->aggregate),source->measure_column ? source->measure_column : ""); }
    for (size_t i=0;i<source->sort_count+(source->sort_column ? 1 : 0);i++) {
        AnalysisSort sort=analysis_sort_at(source,i); if (!sort.column) return 0;
        UIStep *step=&steps[count++]; step->kind=UI_STEP_SORT; step->index=i; step->column=sort.column;
        label(step->title,sizeof(step->title),"Sort %s %s",sort.column,sort.descending ? "descending" : "ascending"); label(step->detail,sizeof(step->detail),"%s · NULLs last",sort.descending ? "Descending" : "Ascending");
    }
    return count;
}
size_t ui_source_steps(const Analysis *source,size_t fields,UIStep steps[DX_UI_STEPS]) {
    return ui_source_steps_named(source,fields,NULL,steps);
}
bool ui_step_source(const Analysis *source,size_t index,Analysis *out) {
    UIStep steps[DX_UI_STEPS]; size_t count=ui_source_steps(source,0,steps); if (index>=count || !out) return false;
    const UIStep *step=&steps[index]; Analysis copy=analysis_copy(source);
    if (step->kind==UI_STEP_SOURCE) copy.unique_rows=false;
    size_t filters=step->kind==UI_STEP_FILTER ? step->index+1 : step->kind==UI_STEP_GROUP || step->kind==UI_STEP_SORT ? source->filter_count+(source->filter_column ? 1 : 0) : 0;
    while (copy.filter_count+(copy.filter_column ? 1 : 0)>filters) analysis_filter_remove(&copy,copy.filter_count ? copy.filters[copy.filter_count-1].column : copy.filter_column);
    if (step->kind!=UI_STEP_GROUP && step->kind!=UI_STEP_SORT) { free(copy.group_column); free(copy.measure_column); copy.group_column=copy.measure_column=NULL; }
    size_t sorts=step->kind==UI_STEP_SORT ? step->index+1 : 0;
    while (copy.sort_count+(copy.sort_column ? 1 : 0)>sorts) analysis_sort_remove(&copy,copy.sort_count);
    analysis_free(out); *out=copy; return true;
}
