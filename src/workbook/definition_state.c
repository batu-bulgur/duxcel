#include "definition_state.h"
#include <string.h>
static bool text(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
bool definition_analysis_equal(const Analysis *a,const Analysis *b) {
    if (!a || !b || a->filter_count>=DX_ANALYSIS_FILTERS || b->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || b->sort_count>=DX_ANALYSIS_SORTS ||
        a->kind!=b->kind || a->source_version!=b->source_version || a->unique_rows!=b->unique_rows || a->filter_op!=b->filter_op || a->aggregate!=b->aggregate ||
        a->descending!=b->descending || a->filter_count!=b->filter_count || a->sort_count!=b->sort_count) return false;
    const char *x[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    const char *y[]={b->path,b->schema,b->table,b->query,b->filter_column,b->filter_value,b->group_column,b->measure_column,b->sort_column};
    for (size_t i=0;i<9;i++) if (!text(x[i],y[i])) return false;
    for (size_t i=0;i<a->filter_count;i++) if (a->filters[i].op!=b->filters[i].op || !text(a->filters[i].column,b->filters[i].column) || !text(a->filters[i].value,b->filters[i].value)) return false;
    for (size_t i=0;i<a->sort_count;i++) if (a->sorts[i].descending!=b->sorts[i].descending || !text(a->sorts[i].column,b->sorts[i].column)) return false;
    return true;
}
bool definition_pivot_equal(const PivotDefinition *a,const PivotDefinition *b) {
    if (!a || !b) return false;
    for (unsigned role=0;role<4;role++) {
        if (a->counts[role]>DX_PIVOT_FIELDS || b->counts[role]>DX_PIVOT_FIELDS || a->counts[role]!=b->counts[role]) return false;
        for (size_t i=0;i<a->counts[role];i++) {
            const PivotField *x=&a->fields[role][i],*y=&b->fields[role][i];
            if (x->aggregate!=y->aggregate || x->filter!=y->filter || x->grain!=y->grain || !text(x->name,y->name) || !text(x->value,y->value)) return false;
        }
    }
    return true;
}
bool definition_marks_equal(const MarkDefinition *a,const MarkDefinition *b) {
    if (!a || !b) return false;
    for (unsigned role=0;role<DX_MARK_ROLES;role++) {
        size_t limit=role==MARKS_SIZE || role==MARKS_SHAPE ? 1 : DX_MARK_FIELDS;
        if (a->counts[role]>limit || b->counts[role]>limit || a->counts[role]!=b->counts[role]) return false;
        for (size_t i=0;i<a->counts[role];i++) {
            const MarkField *x=&a->fields[role][i],*y=&b->fields[role][i];
            if (x->aggregate!=y->aggregate || x->grain!=y->grain || x->measure!=y->measure || !text(x->name,y->name)) return false;
        }
    }
    return true;
}
bool definition_objects_equal(const ObjectSnapshot *a,const ObjectSnapshot *b) {
    if (!a || !b || a->count>DX_MAX_OBJECTS || b->count>DX_MAX_OBJECTS || a->count!=b->count || (a->count && (!a->items || !b->items))) return false;
    for (size_t i=0;i<a->count;i++) {
        const SheetObject *x=a->items+i,*y=b->items+i;
        if (x->id!=y->id || x->row!=y->row || x->col!=y->col || x->kind!=y->kind || x->mark!=y->mark || x->chart_measure!=y->chart_measure || x->source_id!=y->source_id ||
            (x->kind==OBJECT_CHART && (x->rows!=y->rows || x->columns!=y->columns)) || !text(x->name,y->name) ||
            !definition_analysis_equal(&x->source,&y->source) || !definition_pivot_equal(&x->pivot,&y->pivot) || !definition_marks_equal(&x->marks,&y->marks)) return false;
    }
    return true;
}
