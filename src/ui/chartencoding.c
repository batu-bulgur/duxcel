#include "chartencoding.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
UIEncodingBinding ui_encoding_binding(const PivotDefinition *definition,const MarkField *field) {
    if (!field || !field->name) return (UIEncodingBinding){0};
    for (int role=0;role<4;role++) if (definition->counts[role]>DX_PIVOT_FIELDS) return (UIEncodingBinding){0};
    if (field->measure) {
        for (size_t i=0;i<definition->counts[PIVOT_VALUES];i++) { const PivotField *value=&definition->fields[PIVOT_VALUES][i]; if (value->name && !strcmp(value->name,field->name) && value->aggregate==field->aggregate) return (UIEncodingBinding){UI_BIND_MEASURE,i}; }
    } else for (int role=PIVOT_ROWS;role<=PIVOT_COLUMNS;role++) for (size_t i=0;i<definition->counts[role];i++) { const PivotField *value=&definition->fields[role][i]; if (value->name && !strcmp(value->name,field->name) && value->grain==field->grain) return (UIEncodingBinding){role==PIVOT_ROWS ? UI_BIND_ROW : UI_BIND_COLUMN,i}; }
    return (UIEncodingBinding){0};
}
static UIEncodingValue cell_value(const Cell *cell,bool numeric) { return (UIEncodingValue){.raw=cell->text,.number=cell->number,.available=true,.is_null=cell->is_null,.numeric=numeric}; }
static bool top_row(const UIChartView *view,size_t row) { for (size_t i=0;i<view->series && i<3;i++) if (view->rows[i]==row) return true; return false; }
UIEncodingValue ui_encoding_value(const Page *p,const PivotDefinition *d,const UIChartView *view,UIEncodingBinding binding,size_t series,size_t category) {
    UIEncodingValue missing={0};
    if (!view->valid || !p->cells || !p->columns || p->row_count>DX_PAGE_ROWS || view->series>4 || view->categories>DX_CHART_CATEGORIES || view->keys>DX_PIVOT_FIELDS || view->measures>DX_PIVOT_FIELDS || !view->measures || view->measure>=view->measures || (unsigned)binding.kind>UI_BIND_MEASURE || series>=view->series || category>=view->categories || binding.kind==UI_BIND_NONE || view->keys+view->categories*view->measures>p->column_count) return missing;
    for (int role=0;role<4;role++) if (d->counts[role]>DX_PIVOT_FIELDS) return missing;
    if (binding.kind==UI_BIND_COLUMN) {
        if (binding.index>=d->counts[PIVOT_COLUMNS]) return missing;
        const Column *column=&p->columns[view->keys+category*view->measures+view->measure];
        if (binding.index>=column->category_count) return missing;
        return (UIEncodingValue){.raw=column->categories[binding.index],.available=true,.is_null=!column->categories[binding.index]};
    }
    size_t column=binding.kind==UI_BIND_ROW ? binding.index : view->keys+category*view->measures+binding.index;
    if ((binding.kind==UI_BIND_ROW && binding.index>=view->keys) || (binding.kind==UI_BIND_MEASURE && binding.index>=view->measures) || column>=p->column_count) return missing;
    if (series<3) { if (view->rows[series]>=p->row_count) return missing; return cell_value(&p->cells[view->rows[series]*p->column_count+column],p->columns[column].numeric); }
    /* A dimension is available for Other only when every contributing row has
       the same typed value. Mixed values are unavailable, never SQL NULL. */
    if (binding.kind==UI_BIND_ROW) {
        const Cell *first=NULL;
        for (size_t row=0;row<p->row_count;row++) if (!top_row(view,row)) {
            const Cell *cell=&p->cells[row*p->column_count+column];
            if (!first) first=cell;
            else if (first->is_null!=cell->is_null || (!cell->is_null && (first->text && cell->text ? strcmp(first->text,cell->text) : first->text!=cell->text || first->number!=cell->number))) return missing;
        }
        return first ? cell_value(first,p->columns[column].numeric) : missing;
    }
    Aggregate aggregate=d->fields[PIVOT_VALUES][binding.index].aggregate;
    if (aggregate==AGG_AVG || aggregate==AGG_DISTINCT || !p->columns[column].numeric) return missing;
    UIEncodingValue out={.available=true,.is_null=true,.numeric=p->columns[column].numeric,.approximate=true};
    for (size_t row=0;row<p->row_count;row++) if (!top_row(view,row)) {
        const Cell *cell=&p->cells[row*p->column_count+column]; if (cell->is_null) continue;
        if (!isfinite(cell->number)) return missing;
        if (out.is_null) out.number=cell->number;
        else if (aggregate==AGG_MIN) out.number=fmin(out.number,cell->number);
        else if (aggregate==AGG_MAX) out.number=fmax(out.number,cell->number);
        else out.number+=cell->number;
        out.is_null=false;
    }
    if (!out.is_null && !isfinite(out.number)) return missing; return out;
}
static uint64_t value_hash(uint64_t hash,UIEncodingValue value) {
    hash^=value.is_null ? 0 : 1; hash*=UINT64_C(1099511628211);
    if (!value.is_null && value.raw) for (const unsigned char *p=(const unsigned char *)value.raw;*p;p++) { hash^=*p; hash*=UINT64_C(1099511628211); }
    return (hash^255)*UINT64_C(1099511628211);
}
bool ui_chart_encoding(const Page *p,const PivotDefinition *d,const MarkDefinition *marks,const UIChartView *view,bool line,UIChartEncoding *out) {
    memset(out,0,sizeof(*out));
    if (!view->valid || view->series>4 || !view->series || view->categories>DX_CHART_CATEGORIES || !view->categories || !view->measures || view->measures>DX_PIVOT_FIELDS || !p->columns || !p->cells || !marks_valid(marks,NULL)) return false;
    for (int role=0;role<4;role++) if (d->counts[role]>DX_PIVOT_FIELDS) return false;
    if (view->keys!=d->counts[PIVOT_ROWS] || view->measures!=d->counts[PIVOT_VALUES] || view->keys+view->categories*view->measures>p->column_count) return false;
    for (int role=0;role<DX_MARK_ROLES;role++) {
        out->counts[role]=marks->counts[role]; out->active[role]=marks->counts[role]>0;
        for (size_t i=0;i<marks->counts[role];i++) {
            UIEncodingBinding binding=ui_encoding_binding(d,&marks->fields[role][i]); out->bindings[role][i]=binding;
            if (binding.kind==UI_BIND_NONE || (role==MARKS_COLOR && binding.kind!=UI_BIND_ROW)) out->active[role]=false;
        }
    }
    if (!line) out->active[MARKS_SHAPE]=false;
    if (out->active[MARKS_SIZE]) {
        UIEncodingBinding binding=out->bindings[MARKS_SIZE][0];
        if (binding.kind==UI_BIND_COLUMN || !p->columns[binding.kind==UI_BIND_ROW ? binding.index : view->keys+binding.index].numeric) out->active[MARKS_SIZE]=false;
    }
    for (size_t series=0;series<view->series;series++) {
        uint64_t color=UINT64_C(1469598103934665603); bool color_available=out->active[MARKS_COLOR];
        for (size_t i=0;i<marks->counts[MARKS_COLOR] && color_available;i++) { UIEncodingValue value=ui_encoding_value(p,d,view,out->bindings[MARKS_COLOR][i],series,0); if (!value.available) color_available=false; else color=value_hash(color,value); }
        out->colors[series]=color_available ? color : UINT64_MAX;
        if (out->active[MARKS_COLOR] && !color_available) out->partial[MARKS_COLOR]=true;
        for (size_t category=0;category<view->categories;category++) {
            if (out->active[MARKS_SIZE]) {
                UIEncodingValue value=ui_encoding_value(p,d,view,out->bindings[MARKS_SIZE][0],series,category);
                if (value.available && !value.is_null && value.numeric && isfinite(value.number)) { out->sizes[series][category]=fabs(value.number); out->sized[series][category]=true; out->size_max=fmax(out->size_max,fabs(value.number)); }
                else out->partial[MARKS_SIZE]=true;
            }
            if (out->active[MARKS_SHAPE]) { UIEncodingValue value=ui_encoding_value(p,d,view,out->bindings[MARKS_SHAPE][0],series,category); if (value.available && !value.approximate) out->shapes[series][category]=value_hash(UINT64_C(1469598103934665603),value); else { out->shapes[series][category]=UINT64_MAX; out->partial[MARKS_SHAPE]=true; } }
        }
    }
    return true;
}
