#include "page.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
enum { STATISTICS_TEXT_LIMIT=32768 };

static void fail(char **error,const char *text) {
    free(*error); *error=dx_strdup(text ? text : "DuckDB query failed");
}
void record_selection_free(RecordSelection *selection) {
    if (selection) { free(selection->ids); *selection=(RecordSelection){0}; }
}
void page_column_free(Column *c) {
    free(c->name); free(c->type_name); free(c->caption); free(c->detail);
    for (size_t i=0;i<c->category_count;i++) free(c->categories[i]);
    free(c->categories);
}

void page_relations_free(Relation *relations,size_t count) {
    for (size_t i=0;i<count;i++) { free(relations[i].schema); free(relations[i].name); free(relations[i].label); }
    free(relations);
}

void page_other_free(PageOther *owned) {
    if (!owned) return;
    for (size_t i=0;i<DX_PAGE_OTHER_VARIANTS;i++) page_free(&owned->variants[i]);
    free(owned);
}
static bool other_error(char **error,const char *text) {
    if (error) { free(*error); *error=dx_strdup(text); } return false;
}
static bool other_add(size_t *bytes,size_t extra) {
    if (extra>DX_PAGE_OTHER_BYTES || *bytes>DX_PAGE_OTHER_BYTES-extra) return false;
    *bytes+=extra; return true;
}
static bool other_text(size_t *bytes,const char *text,size_t maximum) {
    if (!text) return false;
    size_t size=0; while (size<=maximum && text[size]) size++;
    return size<=maximum && other_add(bytes,size+1);
}
static bool other_number(NumberType a,NumberType b) {
    return a.kind==b.kind && a.precision==b.precision && a.scale==b.scale;
}
bool page_other_adopt(Page *parent,PageOther **owned,char **error) {
    const char *invalid="Invalid independent chart Other readouts.";
    if (!parent || !owned || !*owned || parent->other || !parent->cells || !parent->columns || !parent->sql ||
        parent->count_pending || parent->row_count<=3 || parent->row_count>DX_PAGE_ROWS || parent->column_count>128 ||
        parent->offset>UINT64_MAX-parent->row_count || parent->offset+parent->row_count>parent->total ||
        parent->bytes>DX_PAGE_OTHER_BYTES) return other_error(error,invalid);
    PageOther *next=*owned;
    if (!next->variant_count || next->variant_count>DX_PAGE_OTHER_VARIANTS || !next->measures || next->measures>DX_PIVOT_FIELDS ||
        next->keys>DX_PIVOT_FIELDS || next->top_count!=3 || !next->value_columns || next->value_columns>DX_PAGE_OTHER_VALUE_COLUMNS ||
        next->keys>=parent->column_count || next->value_columns!=parent->column_count-next->keys || next->value_columns%next->measures)
        return other_error(error,invalid);
    bool referenced[DX_PAGE_OTHER_VARIANTS]={0};
    for (size_t m=0;m<next->measures;m++) {
        size_t v=next->measure_variant[m]; if (v==SIZE_MAX) continue;
        if (v>=next->variant_count) return other_error(error,invalid);
        referenced[v]=true;
    }
    size_t bytes=sizeof(*next);
    for (size_t v=0;v<next->variant_count;v++) {
        const Page *readout=&next->variants[v];
        if (!referenced[v] || next->rank_measure[v]>=next->measures || !readout->columns || !readout->cells ||
            readout->columns==parent->columns || readout->cells==parent->cells ||
            readout->column_count!=next->value_columns || readout->row_count!=1 || readout->offset || readout->total!=1 ||
            readout->count_pending || readout->sql || readout->totals || readout->other || readout->other_status ||
            readout->relations || readout->relation_count || readout->records.ids || readout->records.source_key ||
            readout->records.count || readout->records.stamp || readout->records.file_identity_valid)
            return other_error(error,invalid);
        for (size_t prior=0;prior<v;prior++) if (next->rank_measure[prior]==next->rank_measure[v] ||
            readout->columns==next->variants[prior].columns || readout->cells==next->variants[prior].cells) return other_error(error,invalid);
        for (size_t k=0;k<3;k++) {
            if (next->top_rows[v][k]>=parent->row_count) return other_error(error,invalid);
            for (size_t j=0;j<k;j++) if (next->top_rows[v][j]==next->top_rows[v][k]) return other_error(error,invalid);
        }
        size_t exact=readout->column_count*(sizeof(Column)+sizeof(Cell));
        for (size_t c=0;c<readout->column_count;c++) {
            const Column *actual=&readout->columns[c],*expected=&parent->columns[next->keys+c];
            const Cell *cell=&readout->cells[c]; char name[48]; snprintf(name,sizeof(name),"__dx_other_%zu",c);
            if (!actual->name || strcmp(actual->name,name) || !actual->type_name || !expected->type_name ||
                strcmp(actual->type_name,expected->type_name) || actual->type!=expected->type || actual->numeric!=expected->numeric ||
                !other_number(actual->number_type,expected->number_type) || actual->caption || actual->detail || actual->categories || actual->category_count ||
                !other_text(&exact,actual->name,47) || !other_text(&exact,actual->type_name,128) ||
                (cell->is_null ? cell->text!=NULL : !other_text(&exact,cell->text,DX_PAGE_OTHER_BYTES))) return other_error(error,invalid);
        }
        if (exact!=readout->bytes || !other_add(&bytes,exact)) return other_error(error,"Chart Other exceeds the combined 4 MiB page limit.");
    }
    for (size_t v=next->variant_count;v<DX_PAGE_OTHER_VARIANTS;v++) {
        const Page *p=&next->variants[v];
        if (p->columns || p->cells || p->sql || p->totals || p->other || p->relations || p->records.ids || p->records.source_key ||
            p->column_count || p->row_count || p->relation_count || p->bytes) return other_error(error,invalid);
    }
    size_t combined=parent->bytes; if (!other_add(&combined,bytes)) return other_error(error,"Chart Other exceeds the combined 4 MiB page limit.");
    next->parent_cells=parent->cells; next->parent_columns=parent->columns; next->parent_sql=parent->sql;
    next->offset=parent->offset; next->total=parent->total; next->loaded_rows=parent->row_count; next->parent_columns_count=parent->column_count; next->bytes=bytes;
    parent->other=next; parent->other_status=PAGE_OTHER_READY; parent->bytes=combined; *owned=NULL; return true;
}
const Page *page_other_values(const Page *parent,size_t measure) {
    if (!parent || parent->other_status!=PAGE_OTHER_READY || !parent->other) return NULL;
    const PageOther *o=parent->other;
    if (o->variant_count>DX_PAGE_OTHER_VARIANTS || o->measures>DX_PIVOT_FIELDS || measure>=o->measures ||
        o->parent_cells!=parent->cells || o->parent_columns!=parent->columns || o->parent_sql!=parent->sql ||
        o->offset!=parent->offset || o->total!=parent->total || o->loaded_rows!=parent->row_count ||
        o->parent_columns_count!=parent->column_count || o->keys>=parent->column_count ||
        o->value_columns!=parent->column_count-o->keys || parent->count_pending) return NULL;
    size_t variant=o->measure_variant[measure]; return variant<o->variant_count ? &o->variants[variant] : NULL;
}
void page_free(Page *p) {
    page_other_free(p->other);
    if (p->totals) { page_free(&p->totals->rows); page_free(&p->totals->columns); free(p->totals); }
    for (size_t i = 0; i < p->column_count; i++) page_column_free(&p->columns[i]);
    void (*release)(void *)=p->release_cell_text ? p->release_cell_text : free;
    for (size_t i=0;i<p->column_count*p->row_count;i++) if (p->cells[i].text) release(p->cells[i].text);
    page_relations_free(p->relations,p->relation_count);
    free(p->columns); free(p->cells); free(p->sql);
    free(p->records.ids); free(p->records.source_key); memset(p, 0, sizeof(*p));
}
bool page_record_id(const Page *p,uint64_t row,uint64_t *id) {
    if (!p || !id || !p->records.source_key || !p->records.ids || row<p->offset) return false;
    uint64_t index=row-p->offset;
    if (index>=p->row_count || index>=p->records.count || !p->records.ids[index]) return false;
    *id=p->records.ids[index]; return true;
}
bool page_records_changed(const Page *previous,const Page *next) {
    if (!previous || !next) return false;
    const PageRecords *a=&previous->records,*b=&next->records;
    return (a->file_identity_valid && b->file_identity_valid && !source_file_identity_same(&a->file_identity,&b->file_identity)) ||
        (a->file_identity_valid && !b->file_identity_valid && (b->stamp || b->source_key || b->ids || b->count)) ||
        (a->stamp && b->stamp && a->stamp!=b->stamp) ||
        (a->source_key && b->source_key && strcmp(a->source_key,b->source_key)!=0);
}
void distinct_free(DistinctPage *p) {
    for (size_t i=0;i<p->count;i++) { free(p->values[i].label); free(p->values[i].value); free(p->values[i].upper); free(p->values[i].key); }
    free(p->values); memset(p,0,sizeof(*p));
}

void statistics_free(SelectionStats *s) {
    free(s->sum); free(s->average); free(s->minimum); free(s->maximum); memset(s,0,sizeof(*s));
}

size_t source_snapshot_bind(SourceSnapshot *snapshot,uint64_t id,const Analysis *source,char **error) {
    if (snapshot->count>DX_MAX_OBJECTS || snapshot->bytes>8*1024*1024 || !id || !source || source->kind<SOURCE_CSV || source->kind>SOURCE_DEMO || !analysis_query_valid(source,error) || !analysis_filters_valid(source,error) || !analysis_sorts_valid(source,error)) { fail(error,"Invalid table source snapshot."); return SIZE_MAX; }
    for (size_t i=0;i<snapshot->count;i++) if (snapshot->ids[i]==id) {
        if (analysis_same_result(&snapshot->items[i],source)) return i+1;
        fail(error,"The table source changed while capturing this selection."); return SIZE_MAX;
    }
    size_t bytes=analysis_text_bytes(source);
    if (snapshot->count>=DX_MAX_OBJECTS || bytes>8*1024*1024-snapshot->bytes) { fail(error,"Selection table sources exceed the 8 MiB snapshot limit."); return SIZE_MAX; }
    snapshot->items[snapshot->count]=analysis_copy(source); snapshot->ids[snapshot->count]=id; snapshot->bytes+=bytes;
    return ++snapshot->count;
}
void source_snapshot_free(SourceSnapshot *snapshot) {
    for (size_t i=0;i<snapshot->count && i<DX_MAX_OBJECTS;i++) {
        Analysis *source=&snapshot->items[i];
        size_t filters=sizeof(source->filters)/sizeof(*source->filters),sorts=sizeof(source->sorts)/sizeof(*source->sorts);
        if (source->filter_count>filters) source->filter_count=filters;
        if (source->sort_count>sorts) source->sort_count=sorts;
        analysis_free(source);
    }
    memset(snapshot,0,sizeof(*snapshot));
}
void statistics_request_free(StatisticsRequest *request) {
    if (!request) return;
    for (size_t i=0;i<request->count && i<DX_MAX_OBJECTS+1;i++) pivot_free(&request->ranges[i].pivot);
    source_snapshot_free(&request->sources);
    /* Rejected owned jobs may have malformed counts. Capacity is the allocated
       pointer-array bound; valid helper-owned requests never exceed 32,768. */
    for (size_t i=0;request->texts && i<request->text_count && i<request->text_capacity && i<STATISTICS_TEXT_LIMIT;i++) free(request->texts[i]);
    free(request->texts); free(request);
}
bool statistics_request_text(StatisticsRequest *request,const char *text,char **error) {
    if (!text) return true;
    size_t bytes=strlen(text)+1;
    if (request->text_count>=STATISTICS_TEXT_LIMIT || bytes>8*1024*1024-request->text_bytes) { fail(error,"Selection labels exceed the 8 MiB statistics snapshot limit."); return false; }
    if (request->text_count==request->text_capacity) { request->text_capacity=request->text_capacity ? request->text_capacity*2 : 32; request->texts=dx_realloc(request->texts,request->text_capacity*sizeof(*request->texts)); }
    request->texts[request->text_count++]=dx_strdup(text); request->text_bytes+=bytes; return true;
}
