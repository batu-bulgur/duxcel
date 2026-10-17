/* Browser-only backend, included after the common owned job/reply cleanup.
   The official engine lives in a separate Worker. Callbacks copy a bounded
   transport into C memory and publish only the current session/request. */
#include "browser_query.h"
#include "page_wire.h"
#include "distinct_page.h"
#include "../transforms/distinct.h"
#include <emscripten/emscripten.h>
#include <inttypes.h>
#include <stdio.h>

typedef enum { BROWSER_DESCRIBE, BROWSER_PAGE, BROWSER_COUNT, BROWSER_VALIDATE, BROWSER_DISTINCT_PAGE, BROWSER_DISTINCT_COUNT, BROWSER_TYPE_DESCRIBE,BROWSER_PIVOT_COUNT,BROWSER_PIVOT_TOTALS,BROWSER_PIVOT_DOMAIN,BROWSER_PIVOT_ROWS,BROWSER_PIVOT_COLUMNS,BROWSER_STATISTICS } BrowserStage;
typedef struct {
    QueryJob *job;
    QueryReply *reply;
    char *relation;
    DistinctQuery distinct;
    Page distinct_values;
    char *type_relation;
    unsigned type_slot;
    bool resolved_types[2];
    bool matrix,analytical;
    Page totals_schema;
    char *totals_sql;
    BrowserPivotPlan plan;
    Page domains[DX_PIVOT_FIELDS],rows_schema,columns_schema,row_totals;
    size_t domain_index;
    char *rows_sql,*columns_sql;
    BrowserStage stage;
    unsigned ticket;
    double started;
#ifdef DUXCEL_DEV_REVIEW
    uint64_t wire_bytes,decoded_bytes;
    unsigned callbacks;
    double decode_ms;
#endif
} BrowserQuery;
static QueryWorker *browser_owner;
static BrowserQuery browser_query;
static Page browser_schema;
static char *browser_schema_key;
/* Original input and grouped output schemas have different filter meanings.
   Keys exclude filters/sorts, and refresh/stop invalidate both bounded slots. */
static Page browser_type_schema[2];
static char *browser_type_key[2];
static unsigned browser_cookie,browser_sequence;
EM_JS(void, browser_send, (unsigned cookie,unsigned ticket,const char *sql,const char *generation,const char *object,const char *revision,const char *offset,const char *total,const char *columns,unsigned limit), {
    cookie=cookie>>>0;ticket=ticket>>>0;
    Module['dxEngine'].submit({heap:()=>HEAPU8,allocate:_malloc,release:_free,reply:_dx_browser_query_result},cookie,ticket,{requestId:String(ticket),generation:UTF8ToString(generation),documentId:String(cookie),sourceId:'0',objectId:UTF8ToString(object),revision:UTF8ToString(revision),offset:UTF8ToString(offset),knownTotal:total?UTF8ToString(total):null,rowLimit:limit,sql:UTF8ToString(sql),...(columns?{columns:JSON.parse(UTF8ToString(columns))}:{})});
})
EM_JS(void, browser_cancel, (int close), { Module['dxEngine'].cancel(Boolean(close)); })
static void browser_discard(void) {
    job_free(browser_query.job); reply_free(browser_query.reply); free(browser_query.relation); free(browser_query.type_relation);
    page_free(&browser_query.totals_schema); free(browser_query.totals_sql);
    browser_pivot_plan_free(&browser_query.plan);
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&browser_query.domains[i]);
    page_free(&browser_query.rows_schema); page_free(&browser_query.columns_schema); page_free(&browser_query.row_totals);
    free(browser_query.rows_sql); free(browser_query.columns_sql);
    distinct_query_free(&browser_query.distinct); page_free(&browser_query.distinct_values);
    browser_query=(BrowserQuery){0};
    if (browser_owner) browser_owner->running=false;
}
static void browser_publish(const char *error) {
    QueryWorker *w=browser_owner; QueryReply *r=browser_query.reply;
    if (error) { free(r->error); r->error=dx_strdup(error); }
    if (error && browser_query.matrix) page_free(&r->page);
    if (error && r->kind==JOB_STATISTICS) statistics_free(&r->statistics);
    printf("Browser worker reply: {\"id\":\"%" PRIu64 "\",\"kind\":%d,\"rows\":%zu,\"columns\":%zu,\"total\":\"%" PRIu64 "\",\"counted\":\"%" PRIu64 "\",\"pending\":%d,\"error\":%d,\"distinct_count\":%zu,\"distinct_total\":\"%" PRIu64 "\"}\n",r->id,(int)r->kind,r->page.row_count,r->page.column_count,r->page.total,r->counted_rows,r->page.count_pending,r->error!=NULL,r->distinct.count,r->distinct.total);
#ifdef DUXCEL_DEV_REVIEW
    printf("Browser worker metrics: {\"id\":\"%" PRIu64 "\",\"elapsed_ms\":%.3f,\"callbacks\":%u,\"wire_bytes\":%" PRIu64 ",\"decoded_page_bytes\":%" PRIu64 ",\"retained_page_bytes\":%zu,\"decode_ms\":%.3f}\n",r->id,dx_millis()-browser_query.started,browser_query.callbacks,browser_query.wire_bytes,browser_query.decoded_bytes,r->page.bytes,browser_query.decode_ms);
#endif
    browser_query.reply=NULL;
    reply_free(w->reply); w->reply=r;
    browser_discard();
}
static void browser_fail(const char *error) { browser_cancel(0); browser_publish(error); }
static void browser_clear_schemas(void) {
    page_free(&browser_schema); free(browser_schema_key); browser_schema_key=NULL;
    for (size_t i=0;i<2;i++) { page_free(&browser_type_schema[i]); free(browser_type_key[i]); browser_type_key[i]=NULL; }
}
static char *browser_json_string(const char *text) {
    size_t size=strlen(text); char *out=dx_alloc(size*6+3),*p=out; *p++='"';
    static const char hex[]="0123456789abcdef";
    for (const unsigned char *s=(const unsigned char *)text;*s;s++) {
        if (*s=='"' || *s=='\\') { *p++='\\'; *p++=(char)*s; }
        else if (*s<32) { *p++='\\'; *p++='u'; *p++='0'; *p++='0'; *p++=hex[*s>>4]; *p++=hex[*s&15]; }
        else *p++=(char)*s;
    }
    *p++='"'; *p=0; return out;
}
static char *browser_columns_json(const Page *schema) {
    static const char *kinds[]={"none","i8","i16","i32","i64","i128","u8","u16","u32","u64","u128","decimal","float"};
    char *json=dx_strdup("[");
    for (size_t i=0;i<schema->column_count;i++) {
        const Column *c=&schema->columns[i]; char *name=browser_json_string(c->name),*type=browser_json_string(c->type_name);
        dx_append(&json,"%s{\"name\":%s,\"typeName\":%s,\"numberKind\":\"%s\",\"precision\":%u,\"scale\":%u}",i ? "," : "",name,type,kinds[c->number_type.kind],c->number_type.precision,c->number_type.scale);
        free(name); free(type);
    }
    dx_append(&json,"]"); return json;
}
static void browser_execute(char *sql,BrowserStage stage) {
    if (!sql) { browser_fail("Could not build the browser query."); return; }
    if (strlen(sql)>DX_BROWSER_SQL_BYTES) { free(sql); browser_fail("Browser query exceeds 1 MiB. Narrow the filter selection."); return; }
    if (browser_sequence==UINT32_MAX) { free(sql); browser_fail("Browser request limit reached. Reopen the page."); return; }
    BrowserQuery *q=&browser_query; QueryJob *j=q->job;
    q->stage=stage; q->ticket=++browser_sequence;
    char id[24],object[24],revision[24],offset[24],total[24];
    snprintf(id,sizeof(id),"%" PRIu64,j->id); snprintf(object,sizeof(object),"%" PRIu64,j->object_id); snprintf(revision,sizeof(revision),"%" PRIu64,j->revision);
    snprintf(offset,sizeof(offset),"%" PRIu64,(stage==BROWSER_PAGE || stage==BROWSER_DISTINCT_PAGE || stage==BROWSER_PIVOT_ROWS) ? j->offset : 0);
    snprintf(total,sizeof(total),"%" PRIi64,j->known_total);
    bool totals=stage==BROWSER_PIVOT_TOTALS || stage==BROWSER_PIVOT_COLUMNS || stage==BROWSER_STATISTICS; if (totals) snprintf(total,sizeof(total),"1");
    if (stage==BROWSER_PIVOT_ROWS) snprintf(total,sizeof(total),"%" PRIu64,q->reply->page.total);
    const Page *schema=stage==BROWSER_PAGE ? &browser_schema : stage==BROWSER_PIVOT_TOTALS ? &q->totals_schema : stage==BROWSER_PIVOT_ROWS ? &q->rows_schema : stage==BROWSER_PIVOT_COLUMNS ? &q->columns_schema : NULL;
    char *columns=schema ? browser_columns_json(schema) : NULL;
    /* Browser counters count submitted SQL; the async host may supersede a
       request before the official engine executes it. */
    browser_owner->sql_executed++; if (stage==BROWSER_COUNT || stage==BROWSER_PIVOT_COUNT) browser_owner->counts_executed++;
    browser_send(browser_cookie,q->ticket,sql,id,object,revision,offset,totals || stage==BROWSER_PIVOT_ROWS || ((stage==BROWSER_PAGE || stage==BROWSER_DISTINCT_PAGE) && j->known_total>=0) ? total : NULL,columns,totals ? 1 : stage==BROWSER_DISTINCT_PAGE ? DX_DISTINCT_ROWS : stage==BROWSER_PIVOT_DOMAIN ? 129 : DX_PAGE_ROWS);
    free(sql); free(columns);
}
static void browser_fetch_page(void) {
    if (browser_query.matrix && browser_query.job->include_totals) {
        char *error=NULL;
        BrowserQuery *q=&browser_query;
        bool valid=q->plan.sql ? browser_pivot_readout_schema(&q->rows_schema,&browser_schema,&q->job->pivot,&q->plan,true,&error) && browser_pivot_readout_schema(&q->columns_schema,&browser_schema,&q->job->pivot,&q->plan,false,&error) : browser_pivot_totals_schema(&q->totals_schema,&browser_schema,&q->job->pivot,&error);
        if (!valid) { browser_fail(error); free(error); return; }
    }
    char *error=NULL,*sql=browser_page_sql(browser_query.relation,&browser_schema,browser_query.job->offset,&error);
    if (error) { browser_fail(error); free(error); return; }
    browser_execute(sql,BROWSER_PAGE);
}
static void browser_fetch_result(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job;
    if (j->kind!=JOB_STATISTICS) { browser_fetch_page(); return; }
    char *error=NULL,*sql=browser_statistics_sql(q->relation,&browser_schema,j->offset,j->rows,j->first_col,j->cols,&error);
    if (!sql) { browser_fail(error); free(error); return; }
    browser_execute(sql,BROWSER_STATISTICS);
}
static bool browser_comparison(FilterOp op) { return (op>=FILTER_EQUAL && op<=FILTER_LESS) || op==FILTER_BETWEEN || op==FILTER_COMPOSITE; }
static bool browser_same_column(const char *a,const char *b) {
    while (*a && *b) { unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++; if (x>='A' && x<='Z') x+=32; if (y>='A' && y<='Z') y+=32; if (x!=y) return false; }
    return *a==*b;
}
static const Column *browser_type_column(const Page *schema,const char *name) {
    for (size_t i=0;i<schema->column_count;i++) if (browser_same_column(schema->columns[i].name,name)) return &schema->columns[i];
    return NULL;
}
static bool browser_temporal(duckdb_type type) {
    return type==DUCKDB_TYPE_DATE || type==DUCKDB_TYPE_TIMESTAMP || type==DUCKDB_TYPE_TIMESTAMP_S || type==DUCKDB_TYPE_TIMESTAMP_MS || type==DUCKDB_TYPE_TIMESTAMP_NS || type==DUCKDB_TYPE_TIMESTAMP_TZ;
}
static PivotDefinition *browser_context(void) {
    QueryJob *j=browser_query.job; return browser_query.analytical || (j->kind==JOB_DISTINCT && j->object_id && !j->is_table) ? &j->pivot : NULL;
}
static bool browser_apply_types(unsigned slot,char **error) {
    const Page *schema=&browser_type_schema[slot]; Analysis *a=&browser_query.reply->analysis;
    if (!slot) {
        if (a->filter_column && browser_comparison(a->filter_op)) { const Column *c=browser_type_column(schema,a->filter_column); a->filter_number=c ? c->number_type : (NumberType){0}; }
        for (size_t i=0;i<a->filter_count;i++) if (browser_comparison(a->filters[i].op)) { const Column *c=browser_type_column(schema,a->filters[i].column); a->filters[i].number=c ? c->number_type : (NumberType){0}; }
    } else {
        PivotDefinition *pivot=browser_context();
        for (size_t i=0;i<pivot->counts[PIVOT_FILTERS];i++) { PivotField *field=&pivot->fields[PIVOT_FILTERS][i]; if (browser_comparison(field->filter)) { const Column *c=browser_type_column(schema,field->name); field->number=c ? c->number_type : (NumberType){0}; } }
        for (size_t role=0;role<2;role++) for (size_t i=0;i<pivot->counts[role];i++) if (pivot->fields[role][i].grain) {
            const Column *c=browser_type_column(schema,pivot->fields[role][i].name);
            if (!c || !browser_temporal(c->type)) { *error=dx_strdup("Date grains require a DATE or TIMESTAMP field. Cast the source field explicitly first."); return false; }
        }
    }
    browser_query.resolved_types[slot]=true; return true;
}
static void browser_copy_type_schema(unsigned slot,const char *key) {
    Page schema={.column_count=browser_schema.column_count}; schema.columns=dx_alloc(schema.column_count*sizeof(Column)); schema.bytes=schema.column_count*sizeof(Column);
    for (size_t i=0;i<schema.column_count;i++) {
        const Column *c=&browser_schema.columns[i]; schema.columns[i]=(Column){.name=dx_strdup(c->name),.type_name=dx_strdup(c->type_name),.type=c->type,.number_type=c->number_type,.numeric=c->numeric};
        schema.bytes+=strlen(c->name)+strlen(c->type_name)+2;
    }
    page_free(&browser_type_schema[slot]); browser_type_schema[slot]=schema; free(browser_type_key[slot]); browser_type_key[slot]=dx_strdup(key);
}
static void browser_prepare_query(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; Analysis *a=&q->reply->analysis; PivotDefinition *pivot=browser_context(); char *error=NULL;
    bool needed[2]={a->filter_column && browser_comparison(a->filter_op),false};
    for (size_t i=0;i<a->filter_count;i++) needed[0]|=browser_comparison(a->filters[i].op);
    if (pivot) {
        for (size_t i=0;i<pivot->counts[PIVOT_FILTERS];i++) needed[1]|=browser_comparison(pivot->fields[PIVOT_FILTERS][i].filter);
        for (size_t role=0;role<2;role++) for (size_t i=0;i<pivot->counts[role];i++) needed[1]|=pivot->fields[role][i].grain!=PIVOT_DATE_EXACT;
    }
    for (unsigned slot=0;slot<2;slot++) if (needed[slot] && !q->resolved_types[slot]) {
        Analysis shape=*a; shape.filter_column=shape.filter_value=shape.sort_column=NULL; shape.filter_count=shape.sort_count=0; shape.filter_number=(NumberType){0};
        if (!slot) shape.group_column=shape.measure_column=NULL;
        char *relation=browser_relation_sql(&shape,NULL,&error);
        if (!relation) { browser_fail(error); free(error); return; }
        if ((!browser_type_key[slot] || strcmp(browser_type_key[slot],relation)) && browser_schema_key && !strcmp(browser_schema_key,relation)) browser_copy_type_schema(slot,relation);
        if (browser_type_key[slot] && !strcmp(browser_type_key[slot],relation)) {
            free(relation); if (!browser_apply_types(slot,&error)) { browser_fail(error); free(error); return; }
        } else {
            q->type_relation=relation; q->type_slot=slot;
            char *sql=browser_describe_sql(relation,&error);
            if (error) { browser_fail(error); free(error); return; }
            browser_execute(sql,BROWSER_TYPE_DESCRIBE); return;
        }
    }
    if (j->kind==JOB_DISTINCT) {
        if ((needed[0] || needed[1]) && !distinct_query_build(&q->distinct,a,pivot,j->column,j->search,j->date_level,j->date_start,j->date_end,false,&error)) { browser_fail(error); free(error); return; }
        browser_execute(distinct_page_sql(&q->distinct,j->offset),BROWSER_DISTINCT_PAGE); return;
    }
    if (needed[0] || (q->analytical && needed[1])) {
        free(q->relation); q->relation=browser_relation_sql(a,q->analytical ? pivot : NULL,&error); if (!q->relation) { browser_fail(error); free(error); return; }
        if (q->matrix && !pivot->counts[PIVOT_COLUMNS]) { free(q->totals_sql); q->totals_sql=browser_pivot_totals_sql(a,pivot,&error); if (!q->totals_sql) { browser_fail(error); free(error); return; } }
    }
    if (q->analytical && pivot->counts[PIVOT_COLUMNS]) {
        char *sql=browser_pivot_domain_sql(a,pivot,q->domain_index,&error);
        if (!sql) { browser_fail(error); free(error); return; }
        browser_execute(sql,BROWSER_PIVOT_DOMAIN); return;
    }
    if (j->kind==JOB_COUNT) {
        char *sql=browser_count_sql(q->relation,&error);
        if (error) { browser_fail(error); free(error); return; } browser_execute(sql,BROWSER_COUNT); return;
    }
    if (j->kind!=JOB_VALIDATE_SQL && browser_schema_key && !strcmp(browser_schema_key,q->relation)) { browser_fetch_result(); return; }
    char *sql=browser_describe_sql(q->relation,&error);
    if (error) { browser_fail(error); free(error); return; }
    browser_execute(sql,j->kind==JOB_VALIDATE_SQL ? BROWSER_VALIDATE : BROWSER_DESCRIBE);
}
static bool browser_uint64(const char *text,uint64_t *value) {
    if (!text || !*text || *text<'0' || *text>'9') return false;
    uint64_t result=0;
    for (const char *p=text;*p;p++) { if (*p<'0' || *p>'9' || result>(UINT64_MAX-(unsigned)(*p-'0'))/10) return false; result=result*10+(unsigned)(*p-'0'); }
    *value=result; return true;
}
static void browser_matrix_ready(void) {
    BrowserQuery *q=&browser_query; char *error=NULL;
    if (q->plan.sql) {
        if (q->job->include_totals) { browser_execute(dx_strdup(q->rows_sql),BROWSER_PIVOT_ROWS); return; }
        if (!browser_pivot_plan_complete(&q->reply->page,NULL,NULL,&q->job->pivot,&q->plan,&error)) { browser_publish(error); free(error); return; }
        q->reply->page.query_ms=dx_millis()-q->started; browser_publish(NULL); return;
    }
    if (q->job->include_totals) { browser_execute(dx_strdup(q->totals_sql),BROWSER_PIVOT_TOTALS); return; }
    if (!browser_pivot_complete(&q->reply->page,NULL,&q->job->pivot,&error)) { browser_publish(error); free(error); return; }
    q->reply->page.query_ms=dx_millis()-q->started; browser_publish(NULL);
}
EMSCRIPTEN_KEEPALIVE void dx_browser_query_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,const char *total_text,int pending,const char *failure) {
    if (!browser_owner || !browser_query.job || cookie!=browser_cookie || ticket!=browser_query.ticket) return;
    if (failure && *failure) { browser_publish(failure); return; }
    uint64_t total=0; char *error=NULL; Page page={0};
    QueryJob *j=browser_query.job; QueryReply *r=browser_query.reply;
    uint64_t offset=(browser_query.stage==BROWSER_PAGE || browser_query.stage==BROWSER_DISTINCT_PAGE || browser_query.stage==BROWSER_PIVOT_ROWS) ? j->offset : 0;
    if (!browser_uint64(total_text,&total)) { browser_publish("Invalid browser row count."); return; }
#ifdef DUXCEL_DEV_REVIEW
    double decode_started=dx_millis();
    browser_query.callbacks++; browser_query.wire_bytes+=size;
#endif
    bool decoded=page_from_wire(&page,wire,size,offset,total,pending!=0,&error);
#ifdef DUXCEL_DEV_REVIEW
    browser_query.decode_ms+=dx_millis()-decode_started; browser_query.decoded_bytes+=page.bytes;
#endif
    if (!decoded) { browser_publish(error); free(error); return; }
    if (browser_query.stage==BROWSER_STATISTICS) {
        BrowserQuery *q=&browser_query; size_t retained=browser_schema.bytes+strlen(q->relation)+1+analysis_text_bytes(&r->analysis);
        if (q->plan.bytes>DX_PAGE_WIRE_TEXT_BYTES || retained>DX_PAGE_WIRE_TEXT_BYTES-q->plan.bytes) { page_free(&page); browser_publish("Selection statistics plan exceeds 4 MiB."); return; } retained+=q->plan.bytes;
        for (size_t role=0;role<4;role++) for (size_t field=0;field<j->pivot.counts[role];field++) {
            const PivotField *f=&j->pivot.fields[role][field]; size_t bytes=strlen(f->name)+1+(f->value ? strlen(f->value)+1 : 0);
            if (bytes>DX_PAGE_WIRE_TEXT_BYTES || retained>DX_PAGE_WIRE_TEXT_BYTES-bytes) { page_free(&page); browser_publish("Selection statistics definitions exceed 4 MiB."); return; } retained+=bytes;
        }
        for (size_t i=0;i<2;i++) {
            if (browser_type_schema[i].bytes>DX_PAGE_WIRE_TEXT_BYTES || retained>DX_PAGE_WIRE_TEXT_BYTES-browser_type_schema[i].bytes) { page_free(&page); browser_publish("Selection statistics schemas exceed 4 MiB."); return; }
            retained+=browser_type_schema[i].bytes;
        }
        bool valid=browser_statistics_from_page(&r->statistics,&page,j->rows*j->cols,retained,&error); page_free(&page);
        if (valid) r->statistics.query_ms=dx_millis()-q->started; browser_publish(valid ? NULL : error); free(error); return;
    }
    if (browser_query.stage==BROWSER_PIVOT_DOMAIN) {
        BrowserQuery *q=&browser_query; size_t keys=j->pivot.counts[PIVOT_ROWS],measures=j->pivot.counts[PIVOT_VALUES],product=1,used=page.bytes;
        if (used>DX_PAGE_WIRE_TEXT_BYTES) { page_free(&page); browser_publish("Pivot column domains exceed 4 MiB."); return; }
        for (size_t i=0;i<q->domain_index;i++) { size_t count=q->domains[i].row_count; product*=count ? count : 1; if (q->domains[i].bytes>DX_PAGE_WIRE_TEXT_BYTES-used) { page_free(&page); browser_publish("Pivot column domains exceed 4 MiB."); return; } used+=q->domains[i].bytes; }
        size_t count=page.row_count ? page.row_count : 1;
        if (page.column_count!=1 || page.row_count>128 || count>(128-keys)/(product*measures)) { page_free(&page); browser_publish("Pivot exceeds 128 result columns. Filter the column fields or use fewer measures."); return; }
        q->domains[q->domain_index++]=page;
        if (q->domain_index<j->pivot.counts[PIVOT_COLUMNS]) { char *sql=browser_pivot_domain_sql(&r->analysis,&j->pivot,q->domain_index,&error); if (!sql) { browser_publish(error); free(error); return; } browser_execute(sql,BROWSER_PIVOT_DOMAIN); return; }
        bool valid=browser_pivot_plan_build(&q->plan,&r->analysis,&j->pivot,q->domains,&error);
        for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&q->domains[i]);
        if (!valid) { browser_publish(error); free(error); return; }
        free(q->relation); q->relation=dx_strdup(q->plan.sql);
        if (q->matrix && j->include_totals) {
            q->rows_sql=browser_pivot_readout_sql(&r->analysis,&j->pivot,&q->plan,true,j->offset,&error);
            if (q->rows_sql) q->columns_sql=browser_pivot_readout_sql(&r->analysis,&j->pivot,&q->plan,false,0,&error);
            if (!q->rows_sql || !q->columns_sql) { browser_publish(error); free(error); return; }
        }
        if (browser_schema_key && !strcmp(browser_schema_key,q->relation)) browser_fetch_result();
        else { char *sql=browser_describe_sql(q->relation,&error); if (!sql) { browser_publish(error); free(error); return; } browser_execute(sql,BROWSER_DESCRIBE); }
        return;
    }
    if (browser_query.stage==BROWSER_PIVOT_ROWS) {
        BrowserQuery *q=&browser_query; size_t used=r->page.bytes;
        if (page.bytes>DX_PAGE_WIRE_TEXT_BYTES-used) { page_free(&page); browser_publish("Pivot page and source row totals exceed 4 MiB."); return; }
        q->row_totals=page; browser_execute(dx_strdup(q->columns_sql),BROWSER_PIVOT_COLUMNS); return;
    }
    if (browser_query.stage==BROWSER_PIVOT_COLUMNS) {
        BrowserQuery *q=&browser_query; bool valid=browser_pivot_plan_complete(&r->page,&q->row_totals,&page,&j->pivot,&q->plan,&error); page_free(&page);
        if (valid) r->page.query_ms=dx_millis()-q->started; browser_publish(valid ? NULL : error); free(error); return;
    }
    if (browser_query.stage==BROWSER_PIVOT_TOTALS) {
        bool valid=browser_pivot_complete(&r->page,&page,&j->pivot,&error); page_free(&page);
        if (valid) r->page.query_ms=dx_millis()-browser_query.started;
        browser_publish(valid ? NULL : error); free(error); return;
    }
    if (browser_query.stage==BROWSER_PIVOT_COUNT) {
        uint64_t count=0; bool valid=page.row_count==1 && page.column_count==1 && !page.cells[0].is_null && browser_uint64(page.cells[0].text,&count);
        page_free(&page);
        if (!valid || (r->page.row_count && (r->page.offset>UINT64_MAX-r->page.row_count || r->page.offset+r->page.row_count>count))) { browser_publish("Invalid matrix result row count."); return; }
        r->page.total=count; r->page.count_pending=false; browser_matrix_ready(); return;
    }
    if (browser_query.stage==BROWSER_TYPE_DESCRIBE) {
        unsigned slot=browser_query.type_slot; bool valid=browser_schema_from_describe(&browser_type_schema[slot],&page,&error); page_free(&page);
        if (!valid) { browser_publish(error); free(error); return; }
        free(browser_type_key[slot]); browser_type_key[slot]=browser_query.type_relation; browser_query.type_relation=NULL;
        if (!browser_apply_types(slot,&error)) { browser_publish(error); free(error); return; }
        browser_prepare_query(); return;
    }
    if (browser_query.stage==BROWSER_DESCRIBE || browser_query.stage==BROWSER_VALIDATE) {
        bool valid=browser_schema_from_describe(&browser_schema,&page,&error); page_free(&page);
        if (!valid) { browser_publish(error); free(error); return; }
        free(browser_schema_key); browser_schema_key=dx_strdup(browser_query.relation);
        if (browser_query.stage==BROWSER_VALIDATE) browser_publish(NULL); else browser_fetch_result();
        return;
    }
    if (browser_query.stage==BROWSER_DISTINCT_PAGE) {
        browser_query.distinct_values=page;
        if (j->known_total<0) { browser_execute(distinct_count_sql(&browser_query.distinct),BROWSER_DISTINCT_COUNT); return; }
        bool valid=distinct_page_from_page(&r->distinct,&browser_query.distinct_values,(uint64_t)j->known_total,&error);
        browser_publish(valid ? NULL : error); free(error); return;
    }
    if (browser_query.stage==BROWSER_DISTINCT_COUNT) {
        uint64_t count=0;
        bool valid=page.row_count==1 && page.column_count==1 && !page.cells[0].is_null && browser_uint64(page.cells[0].text,&count);
        page_free(&page);
        if (!valid) { browser_publish("Invalid available-value count result."); return; }
        valid=distinct_page_from_page(&r->distinct,&browser_query.distinct_values,count,&error);
        browser_publish(valid ? NULL : error); free(error); return;
    }
    if (browser_query.stage==BROWSER_COUNT) {
        bool valid=page.row_count==1 && page.column_count==1 && !page.cells[0].is_null && browser_uint64(page.cells[0].text,&r->counted_rows);
        page_free(&page); browser_publish(valid ? NULL : "Invalid browser count result."); return;
    }
    for (size_t c=0;c<page.column_count;c++) {
        Column *column=&page.columns[c]; column->filtered=analysis_filter_get(&r->analysis,column->name,NULL,NULL);
        analysis_filter_number(&r->analysis,column->name,column->number_type);
        for (size_t n=0;n<DX_ANALYSIS_SORTS;n++) {
            AnalysisSort sort=analysis_sort_at(&r->analysis,n); if (sort.column && !strcmp(sort.column,column->name)) { column->sort_priority=(unsigned char)(n+1); column->sort_descending=sort.descending; }
        }
    }
    page.query_ms=dx_millis()-browser_query.started;
    if (browser_query.matrix) {
        size_t bytes=strlen(browser_query.relation)+1;
        if (bytes>DX_PAGE_WIRE_TEXT_BYTES || page.bytes>DX_PAGE_WIRE_TEXT_BYTES-bytes) { page_free(&page); browser_publish("Matrix page and SQL exceed the 4 MiB memory limit."); return; }
        page.sql=dx_strdup(browser_query.relation); page.bytes+=bytes; r->page=page;
        if (j->known_total<0) {
            char *sql=browser_count_sql(browser_query.relation,&error);
            if (!sql) { browser_publish(error); free(error); return; }
            browser_execute(sql,BROWSER_PIVOT_COUNT);
        } else browser_matrix_ready();
        return;
    }
    r->page=page;
    browser_publish(NULL);
}
bool worker_start(QueryWorker *w) {
    if (browser_owner || browser_cookie==UINT32_MAX) return false;
    memset(w,0,sizeof(*w)); dx_mutex_init(&w->mutex); dx_cond_init(&w->condition);
    browser_owner=w; browser_cookie++; w->started=true; return true;
}
void worker_submit(QueryWorker *w,QueryJob *j) {
    browser_discard(); reply_free(w->reply); w->reply=NULL;
    w->latest_id=w->active_id=j->id; w->running=true; w->jobs_started++;
    BrowserQuery *q=&browser_query; q->job=j; q->started=dx_millis();
    QueryReply *r=q->reply=dx_alloc(sizeof(*r)); r->id=j->id; r->kind=j->kind;
    r->object_id=j->object_id; r->revision=j->revision; r->is_preview=j->is_preview;
    r->analysis=j->analysis; memset(&j->analysis,0,sizeof(j->analysis)); r->path=j->path; j->path=NULL;
    printf("Browser worker started: {\"id\":\"%" PRIu64 "\",\"kind\":%d}\n",r->id,(int)r->kind);
    if (j->refresh_source) browser_clear_schemas();
    q->matrix=j->kind==JOB_PIVOT && !j->is_table;
    q->analytical=q->matrix || (j->kind==JOB_STATISTICS && j->object_id && !j->is_table);
    bool invalid_pivot=false;
    for (size_t role=0;role<4;role++) invalid_pivot|=j->pivot.counts[role]>DX_PIVOT_FIELDS;
    if (invalid_pivot) { browser_fail("Matrix shelves exceed their bounded field limit."); return; }
    if (j->kind==JOB_STATISTICS) {
        bool pivot=false; for (size_t role=0;role<4;role++) pivot|=j->pivot.counts[role]!=0;
        if (j->statistics) { browser_fail("Mixed cells are not connected to browser statistics yet. Choose a direct source, table or matrix range."); return; }
        if ((j->is_table && (!j->object_id || pivot)) || (!j->object_id && pivot)) { browser_fail("Invalid statistics object identity or table definition."); return; }
        if (q->analytical && !j->pivot.counts[PIVOT_VALUES]) { browser_fail("Add a field to Values for analytical result statistics."); return; }
        if (j->expected_source_stamp) { browser_fail("Native source stamps cannot be verified by the browser engine."); return; }
        if (!j->cols || j->cols>256 || j->offset>DX_SHEET_ROWS || j->rows>DX_SHEET_ROWS-j->offset) { browser_fail("Selection statistics support up to 256 columns inside the worksheet."); return; }
    }
    if (j->kind!=JOB_DISTINCT && j->kind!=JOB_PAGE && j->kind!=JOB_COUNT && j->kind!=JOB_VALIDATE_SQL && j->kind!=JOB_STEP_PREVIEW && j->kind!=JOB_PIVOT && j->kind!=JOB_STATISTICS) {
        browser_fail("This analytical operation is not connected to the browser engine yet."); return;
    }
    char *error=NULL;
    if (j->kind==JOB_DISTINCT) {
        if (r->analysis.kind==SOURCE_NONE || r->analysis.kind==SOURCE_DATABASE) { browser_fail("Choose a CSV or Parquet source for available values."); return; }
        if (!distinct_query_build(&q->distinct,&r->analysis,j->object_id && !j->is_table ? &j->pivot : NULL,j->column,j->search,j->date_level,j->date_start,j->date_end,false,&error)) {
            browser_fail(error); free(error); return;
        }
        browser_prepare_query(); return;
    }
    if (q->matrix) {
        char *valid=j->pivot.counts[PIVOT_COLUMNS] ? browser_pivot_domain_sql(&r->analysis,&j->pivot,0,&error) : browser_pivot_totals_sql(&r->analysis,&j->pivot,&error);
        if (!valid) { browser_fail(error); free(error); return; }
        if (j->pivot.counts[PIVOT_COLUMNS]) free(valid); else q->totals_sql=valid;
    }
    q->relation=browser_relation_sql(&r->analysis,q->analytical ? &j->pivot : NULL,&error);
    if (!q->relation) { browser_fail(error); free(error); return; }
    browser_prepare_query();
}
QueryReply *worker_poll(QueryWorker *w) { QueryReply *r=w->reply; w->reply=NULL; return r; }
void worker_cancel(QueryWorker *w) { w->latest_id++; browser_discard(); reply_free(w->reply); w->reply=NULL; browser_cancel(0); }
void worker_stop(QueryWorker *w) {
    if (!w->started) return;
    worker_cancel(w); browser_cancel(1); browser_clear_schemas(); browser_owner=NULL;
    dx_cond_destroy(&w->condition); dx_mutex_destroy(&w->mutex); w->started=false;
}
