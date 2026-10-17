#include "page.h"
#include <stdlib.h>
#include <string.h>
enum { STATISTICS_TEXT_LIMIT=32768 };

static void fail(char **error,const char *text) {
    free(*error); *error=dx_strdup(text ? text : "DuckDB query failed");
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

void page_free(Page *p) {
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
