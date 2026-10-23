#include "save_fence.h"
#include <string.h>
static bool text(const char *a,const char *b) { return a==b || (a && b && !strcmp(a,b)); }
static bool analysis(const Analysis *a,const Analysis *b) {
    if (!analysis_same_result(a,b) || a->source_version!=b->source_version || a->filter_count!=b->filter_count || a->sort_count!=b->sort_count) return false;
    if (!text(a->path,b->path) || !text(a->schema,b->schema) || !text(a->table,b->table) || !text(a->query,b->query) ||
        !text(a->filter_column,b->filter_column) || !text(a->filter_value,b->filter_value) || a->filter_op!=b->filter_op ||
        !text(a->group_column,b->group_column) || !text(a->measure_column,b->measure_column) || a->aggregate!=b->aggregate ||
        !text(a->sort_column,b->sort_column) || a->descending!=b->descending || a->unique_rows!=b->unique_rows) return false;
    for (size_t i=0;i<a->filter_count;i++) if (!text(a->filters[i].column,b->filters[i].column) || !text(a->filters[i].value,b->filters[i].value) || a->filters[i].op!=b->filters[i].op) return false;
    for (size_t i=0;i<a->sort_count;i++) if (!text(a->sorts[i].column,b->sorts[i].column) || a->sorts[i].descending!=b->sorts[i].descending) return false;
    return true;
}
static bool view(const SheetView *a,const SheetView *b) {
    if (a->row!=b->row || a->anchor_row!=b->anchor_row || a->end_row!=b->end_row || a->top!=b->top || a->col!=b->col ||
        a->anchor_col!=b->anchor_col || a->end_col!=b->end_col || a->selection!=b->selection || a->horizontal!=b->horizontal ||
        a->fraction!=b->fraction || a->canvas_zoom!=b->canvas_zoom || a->column_count!=b->column_count) return false;
    for (size_t i=0;i<a->column_count;i++) if (!text(a->columns[i],b->columns[i]) || a->widths[i]!=b->widths[i]) return false;
    return true;
}
bool workbook_save_matches(const Workbook *a,const Workbook *b) {
    if (!a || !b || !workbook_identity_valid(a) || !workbook_identity_valid(b) || a->id.high!=b->id.high || a->id.low!=b->id.low ||
        a->command_revision!=b->command_revision || a->sheet_high_water!=b->sheet_high_water || a->count!=b->count || a->active!=b->active ||
        a->user_scale!=b->user_scale || a->reduced_motion!=b->reduced_motion) return false;
    for (size_t i=0;i<a->count;i++) {
        const Worksheet *x=a->sheets+i,*y=b->sheets+i;
        if (x->id!=y->id || x->hidden!=y->hidden || !text(x->name,y->name) || !analysis(&x->analysis,&y->analysis) || !view(&x->view,&y->view) ||
            x->cells.count!=y->cells.count || x->cells.bytes!=y->cells.bytes || x->formats.revision!=y->formats.revision ||
            x->source_versions.high_water!=y->source_versions.high_water || x->source_versions.count!=y->source_versions.count || x->source_versions.bytes!=y->source_versions.bytes ||
            x->objects.count!=y->objects.count || x->objects.next_id!=y->objects.next_id || x->objects.title_revision!=y->objects.title_revision || x->objects.marks_revision!=y->objects.marks_revision) return false;
        for (size_t n=0;n<x->cells.count;n++) { const SheetCell *p=x->cells.cells+n,*q=y->cells.cells+n;
            if (p->row!=q->row || p->col!=q->col || !text(p->text,q->text)) return false;
        }
        for (size_t n=0;n<x->objects.count;n++) { const SheetObject *p=x->objects.items+n,*q=y->objects.items+n;
            if (p->id!=q->id || p->revision!=q->revision || p->kind!=q->kind || p->row!=q->row || p->col!=q->col || p->rows!=q->rows || p->columns!=q->columns ||
                p->source_id!=q->source_id || !text(p->name,q->name) || !analysis(&p->source,&q->source)) return false;
        }
    }
    return true;
}
