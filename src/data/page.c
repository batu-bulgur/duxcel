#include "page.h"
#include <stdlib.h>
#include <string.h>

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
    free(p->columns); free(p->cells); free(p->sql); memset(p, 0, sizeof(*p));
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
    for (size_t i=0;i<snapshot->count && i<DX_MAX_OBJECTS;i++) analysis_free(&snapshot->items[i]); memset(snapshot,0,sizeof(*snapshot));
}
void statistics_request_free(StatisticsRequest *request) {
    if (!request) return;
    for (size_t i=0;i<request->count && i<DX_MAX_OBJECTS+1;i++) pivot_free(&request->ranges[i].pivot);
    source_snapshot_free(&request->sources);
    for (size_t i=0;i<request->text_count;i++) free(request->texts[i]);
    free(request->texts); free(request);
}
bool statistics_request_text(StatisticsRequest *request,const char *text,char **error) {
    if (!text) return true;
    size_t bytes=strlen(text)+1;
    if (request->text_count>=32768 || bytes>8*1024*1024-request->text_bytes) { fail(error,"Selection labels exceed the 8 MiB statistics snapshot limit."); return false; }
    if (request->text_count==request->text_capacity) { request->text_capacity=request->text_capacity ? request->text_capacity*2 : 32; request->texts=dx_realloc(request->texts,request->text_capacity*sizeof(*request->texts)); }
    request->texts[request->text_count++]=dx_strdup(text); request->text_bytes+=bytes; return true;
}
