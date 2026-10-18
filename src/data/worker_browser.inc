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

#define BROWSER_EXPORT_BYTES (64u*1024u*1024u)
#define BROWSER_BINDINGS_JSON_BYTES (64u*1024u)
typedef struct {
    BrowserSourceGuard guards[DX_BROWSER_SOURCE_BINDINGS];
    size_t count;
    char *json; /* Exact names/order, independent of retired job definitions. */
} BrowserSourceCacheKey;
typedef enum { BROWSER_DESCRIBE, BROWSER_PAGE, BROWSER_COUNT, BROWSER_VALIDATE, BROWSER_DISTINCT_PAGE, BROWSER_DISTINCT_COUNT, BROWSER_TYPE_DESCRIBE,BROWSER_PIVOT_COUNT,BROWSER_PIVOT_TOTALS,BROWSER_PIVOT_DOMAIN,BROWSER_PIVOT_ROWS,BROWSER_PIVOT_COLUMNS,BROWSER_STATISTICS,BROWSER_COPY_PAGE,BROWSER_EXPORT } BrowserStage;
typedef struct {
    QueryJob *job;
    QueryReply *reply;
    char *relation;
    char *sources_json; /* Built once from frozen job bindings, never refreshed. */
    DistinctQuery distinct;
    Page distinct_values;
    char *type_relation;
    unsigned type_slot;
    bool resolved_types[2];
    bool matrix,analytical,mixed;
    size_t statistics_range,statistics_definitions;
    char *statistics_cells;
    Page totals_schema;
    char *totals_sql;
    BrowserPivotPlan plan;
    Page domains[DX_PIVOT_FIELDS],rows_schema,columns_schema,row_totals;
    size_t domain_index;
    char *rows_sql,*columns_sql;
    bool copying;
    size_t copy_index,copy_definitions;
    CopyPatch direct_copy; /* Scalar geometry only; the job owns its pivot. */
    CellClipboard copy_cells;
    Page copy_schema;
    size_t export_definitions;
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
static BrowserSourceCacheKey browser_schema_sources,browser_type_sources[2];
static unsigned browser_cookie,browser_sequence;
EM_JS(void, browser_send, (unsigned cookie,unsigned ticket,const char *sql,const char *generation,const char *object,const char *revision,const char *offset,const char *total,const char *columns,unsigned limit,int copying,const char *sources), {
    cookie=cookie>>>0;ticket=ticket>>>0;
    Module['dxEngine'].submit({heap:()=>HEAPU8,allocate:_malloc,release:_free,reply:_dx_browser_query_result},cookie,ticket,{requestId:String(ticket),generation:UTF8ToString(generation),documentId:String(cookie),sourceId:'0',objectId:UTF8ToString(object),revision:UTF8ToString(revision),offset:UTF8ToString(offset),knownTotal:total?UTF8ToString(total):null,rowLimit:limit,sql:UTF8ToString(sql),...(columns?{columns:JSON.parse(UTF8ToString(columns))}:{}),...(copying?{purpose:'copy'}:{}),...(sources?{sourceBindings:JSON.parse(UTF8ToString(sources))}:{})});
})
EM_JS(void, browser_export_send, (unsigned cookie,unsigned ticket,const char *sql,const char *name,const char *generation,const char *object,const char *revision,const char *sources), {
    cookie=cookie>>>0;ticket=ticket>>>0;
    Module['dxEngine'].exportParquet({heap:()=>HEAPU8,allocate:_malloc,release:_free,reply:_dx_browser_export_result},cookie,ticket,{requestId:String(ticket),generation:UTF8ToString(generation),documentId:String(cookie),sourceId:'0',objectId:UTF8ToString(object),revision:UTF8ToString(revision),offset:'0',knownTotal:'1',rowLimit:1,sql:UTF8ToString(sql),fileName:UTF8ToString(name),...(sources?{sourceBindings:JSON.parse(UTF8ToString(sources))}:{})});
})
EM_JS(void, browser_cancel, (int close), { Module['dxEngine'].cancel(Boolean(close)); })
static void browser_discard(void) {
    job_free(browser_query.job); reply_free(browser_query.reply); free(browser_query.relation); free(browser_query.type_relation); free(browser_query.sources_json);
    page_free(&browser_query.totals_schema); free(browser_query.totals_sql);
    browser_pivot_plan_free(&browser_query.plan);
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&browser_query.domains[i]);
    page_free(&browser_query.rows_schema); page_free(&browser_query.columns_schema); page_free(&browser_query.row_totals);
    free(browser_query.rows_sql); free(browser_query.columns_sql); free(browser_query.statistics_cells);
    clipboard_free(&browser_query.copy_cells); page_free(&browser_query.copy_schema);
    distinct_query_free(&browser_query.distinct); page_free(&browser_query.distinct_values);
    browser_query=(BrowserQuery){0};
    if (browser_owner) browser_owner->running=false;
}
static void browser_publish(const char *error) {
    QueryWorker *w=browser_owner; QueryReply *r=browser_query.reply;
    if (error) { free(r->error); r->error=dx_strdup(error); }
    if (error && browser_query.matrix) page_free(&r->page);
    if (error && r->kind==JOB_STATISTICS) statistics_free(&r->statistics);
    if (error && browser_query.copying) { free(r->clipboard); r->clipboard=NULL; }
    if (error && r->kind==JOB_EXPORT) { free(r->export_data); r->export_data=NULL; r->export_bytes=0; r->exported_rows=0; }
    printf("Browser worker reply: {\"id\":\"%" PRIu64 "\",\"kind\":%d,\"rows\":%zu,\"columns\":%zu,\"total\":\"%" PRIu64 "\",\"counted\":\"%" PRIu64 "\",\"pending\":%d,\"error\":%d,\"distinct_count\":%zu,\"distinct_total\":\"%" PRIu64 "\"}\n",r->id,(int)r->kind,r->page.row_count,r->page.column_count,r->page.total,r->counted_rows,r->page.count_pending,r->error!=NULL,r->distinct.count,r->distinct.total);
#ifdef DUXCEL_DEV_REVIEW
    printf("Browser worker metrics: {\"id\":\"%" PRIu64 "\",\"elapsed_ms\":%.3f,\"callbacks\":%u,\"wire_bytes\":%" PRIu64 ",\"decoded_page_bytes\":%" PRIu64 ",\"retained_page_bytes\":%zu,\"decode_ms\":%.3f}\n",r->id,dx_millis()-browser_query.started,browser_query.callbacks,browser_query.wire_bytes,browser_query.decoded_bytes,r->page.bytes,browser_query.decode_ms);
#endif
    browser_query.reply=NULL;
    reply_free(w->reply); w->reply=r;
    browser_discard();
}
static void browser_fail(const char *error) { browser_cancel(0); browser_publish(error); }
static void browser_source_cache_free(BrowserSourceCacheKey *key) { free(key->json); *key=(BrowserSourceCacheKey){0}; }
static void browser_clear_schemas(void) {
    page_free(&browser_schema); free(browser_schema_key); browser_schema_key=NULL;
    browser_source_cache_free(&browser_schema_sources);
    for (size_t i=0;i<2;i++) { page_free(&browser_type_schema[i]); free(browser_type_key[i]); browser_type_key[i]=NULL; browser_source_cache_free(&browser_type_sources[i]); }
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
static const SourceSnapshot *browser_source_snapshots(void) {
    QueryJob *j=browser_query.job;
    return j->copy ? &j->copy->sources : j->statistics ? &j->statistics->sources : NULL;
}
static const Analysis *browser_source_at(size_t source) {
    if (!source) return &browser_query.reply->analysis; /* Job's primary was moved. */
    const SourceSnapshot *extra=browser_source_snapshots();
    return extra && extra->count<=DX_MAX_OBJECTS && source<=extra->count ? &extra->items[source-1] : NULL;
}
static bool browser_source_name(const char *name) {
    if (!name) return false;
    size_t size=0; while (size<=255 && name[size]) size++;
    if (!size || size>255 || !strcmp(name,".") || !strcmp(name,"..")) return false;
    for (size_t i=0;i<size;) {
        unsigned char first=(unsigned char)name[i++]; if (first=='/' || first=='\\') return false;
        if (first<0x80) continue;
        unsigned count; uint32_t value,minimum;
        if (first>=0xc2 && first<=0xdf) { count=1; value=first&31; minimum=0x80; }
        else if (first>=0xe0 && first<=0xef) { count=2; value=first&15; minimum=0x800; }
        else if (first>=0xf0 && first<=0xf4) { count=3; value=first&7; minimum=0x10000; }
        else return false;
        if (count>size-i) return false;
        while (count--) { unsigned char next=(unsigned char)name[i++]; if ((next&0xc0)!=0x80) return false; value=(value<<6)|(next&63); }
        if (value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
static void browser_source_name_json(const char *name,char out[255*6+3]) {
    static const char hex[]="0123456789abcdef"; char *p=out; *p++='"';
    for (const unsigned char *s=(const unsigned char *)name;*s;s++) {
        if (*s=='"' || *s=='\\') { *p++='\\'; *p++=(char)*s; }
        else if (*s<32) { memcpy(p,"\\u00",4); p+=4; *p++=hex[*s>>4]; *p++=hex[*s&15]; }
        else *p++=(char)*s;
    }
    *p++='"'; *p=0;
}
static bool browser_sources_prepare(char **error) {
    QueryJob *j=browser_query.job; const SourceSnapshot *extra=browser_source_snapshots();
    const char *failure="Invalid or unavailable captured browser source bindings.";
    if (j->browser_sources_invalid || j->browser_source_count>DX_BROWSER_SOURCE_BINDINGS) goto invalid;
    if (!j->browser_source_count) return true; /* Explicit legacy/unbound contract. */
    if ((j->copy && j->statistics) || (extra && extra->count>DX_MAX_OBJECTS)) goto invalid;
    for (size_t i=0;i<j->browser_source_count;i++) {
        const BrowserSourceGuard *g=&j->browser_sources[i]; const Analysis *a=browser_source_at(g->source);
        if (!a || (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET) || !browser_source_name(a->path) ||
            !g->binding.engine_epoch || !g->binding.registration_id) goto invalid;
        for (size_t k=0;k<i;k++) if (j->browser_sources[k].source==g->source ||
            !strcmp(browser_source_at(j->browser_sources[k].source)->path,a->path)) goto invalid;
    }
    /* A nonempty vector is complete for all declared file snapshots, including
       cached contributors that need no data query. It does not infer SQL's
       external/remote dependencies or certify arbitrary database writes. */
    for (size_t source=0;source<=(extra ? extra->count : 0);source++) {
        const Analysis *a=browser_source_at(source); if (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET) continue;
        if (!browser_source_name(a->path)) goto invalid;
        bool found=false; for (size_t i=0;i<j->browser_source_count;i++)
            found|=!strcmp(browser_source_at(j->browser_sources[i].source)->path,a->path);
        if (!found) goto invalid;
    }
    /* Exact two-pass sizing: only one retained JSON allocation. Escaping uses
       bounded stack storage; no per-SQL rebuild or borrowed job string. */
    const char *format="%s{\"name\":%s,\"engineEpoch\":\"%" PRIu64 "\",\"registrationId\":\"%" PRIu64 "\"}";
    size_t bytes=3; char name[255*6+3];
    for (size_t i=0;i<j->browser_source_count;i++) {
        const BrowserSourceGuard *g=&j->browser_sources[i]; browser_source_name_json(browser_source_at(g->source)->path,name);
        int n=snprintf(NULL,0,format,i ? "," : "",name,g->binding.engine_epoch,g->binding.registration_id);
        if (n<0 || (size_t)n>BROWSER_BINDINGS_JSON_BYTES-bytes) goto invalid;
        bytes+=(size_t)n;
    }
    char *json=dx_alloc(bytes),*p=json; *p++='[';
    for (size_t i=0;i<j->browser_source_count;i++) {
        const BrowserSourceGuard *g=&j->browser_sources[i]; browser_source_name_json(browser_source_at(g->source)->path,name);
        int n=snprintf(p,bytes-(size_t)(p-json),format,i ? "," : "",name,g->binding.engine_epoch,g->binding.registration_id); p+=(size_t)n;
    }
    *p++=']'; *p=0; browser_query.sources_json=json; return true;
invalid:
    *error=dx_strdup(failure); return false;
}
static bool browser_sources_match(const BrowserSourceCacheKey *key) {
    const QueryJob *j=browser_query.job;
    if (key->count!=j->browser_source_count) return false;
    for (size_t i=0;i<key->count;i++) {
        const BrowserSourceGuard *a=&key->guards[i],*b=&j->browser_sources[i];
        if (a->source!=b->source || a->binding.engine_epoch!=b->binding.engine_epoch || a->binding.registration_id!=b->binding.registration_id) return false;
    }
    return !key->count || (key->json && browser_query.sources_json && !strcmp(key->json,browser_query.sources_json));
}
static void browser_sources_cache(BrowserSourceCacheKey *key) {
    browser_source_cache_free(key); key->count=browser_query.job->browser_source_count;
    for (size_t i=0;i<key->count;i++) key->guards[i]=browser_query.job->browser_sources[i];
    if (key->count) key->json=dx_strdup(browser_query.sources_json);
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
static CopyPatch *browser_copy_patch(void) {
    BrowserQuery *q=&browser_query;
    return q->job->kind==JOB_RANGE_COPY ? &q->job->copy->patches[q->copy_index] : &q->direct_copy;
}
static CellClipboard *browser_copy_cells(void) {
    return browser_query.job->kind==JOB_RANGE_COPY ? &browser_query.job->copy->cells : &browser_query.copy_cells;
}
static Analysis *browser_active_analysis(void) {
    BrowserQuery *q=&browser_query;
    if (q->copying && q->job->kind==JOB_RANGE_COPY && q->copy_index<q->job->copy->count) {
        size_t source=q->job->copy->patches[q->copy_index].source;
        if (source) return &q->job->copy->sources.items[source-1];
    }
    if (q->mixed && q->statistics_range<q->job->statistics->count) {
        size_t source=q->job->statistics->ranges[q->statistics_range].source;
        if (source) return &q->job->statistics->sources.items[source-1];
    }
    return &q->reply->analysis;
}
static PivotDefinition *browser_context(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job;
    if (q->copying) {
        CopyPatch *patch=browser_copy_patch();
        return patch->is_pivot ? j->kind==JOB_RANGE_COPY ? &patch->pivot : &j->pivot : NULL;
    }
    if (q->mixed) { StatisticsRange *range=&j->statistics->ranges[q->statistics_range]; return range->is_pivot ? &range->pivot : NULL; }
    return q->analytical || (j->kind==JOB_DISTINCT && j->object_id && !j->is_table) ? &j->pivot : NULL;
}
static bool browser_copy_add(size_t *used,size_t bytes,char **error) {
    if (bytes>DX_BROWSER_COPY_RETAINED_BYTES || *used>DX_BROWSER_COPY_RETAINED_BYTES-bytes) {
        if (error) { free(*error); *error=dx_strdup("Clipboard staging exceeds its combined 24 MiB C memory limit."); } return false;
    }
    *used+=bytes; return true;
}
static bool browser_copy_source_valid(const Analysis *source,bool optional,size_t *used,char **error) {
    if (optional && source->kind==SOURCE_NONE) {
        if ((source->path && *source->path) || source->schema || source->table || source->query || source->filter_column || source->filter_value || source->group_column || source->measure_column || source->sort_column || source->filter_count || source->sort_count || source->unique_rows) { *error=dx_strdup("Invalid empty clipboard source snapshot."); return false; }
    } else {
        char *sql=browser_relation_sql(source,NULL,error); if (!sql) return false; free(sql);
    }
    return browser_copy_add(used,analysis_text_bytes(source),error);
}
static bool browser_copy_pivot_valid(const Analysis *source,const PivotDefinition *pivot,bool analytical,size_t *used,char **error) {
    bool fields=false;
    for (size_t role=0;role<4;role++) { if (pivot->counts[role]>DX_PIVOT_FIELDS) { *error=dx_strdup("Clipboard shelves exceed their bounded field limit."); return false; } fields|=pivot->counts[role]!=0; }
    if (!analytical && fields) { *error=dx_strdup("A copied table cannot contain matrix shelves."); return false; }
    if (!analytical) return true;
    char *sql=browser_relation_sql(source,pivot,error); if (!sql) return false; free(sql);
    for (size_t role=0;role<4;role++) for (size_t i=0;i<pivot->counts[role];i++) {
        const PivotField *field=&pivot->fields[role][i];
        if (!browser_copy_add(used,strlen(field->name)+1+(field->value ? strlen(field->value)+1 : 0),error)) return false;
    }
    return true;
}
static bool browser_copy_preflight(char **error) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; size_t used=sizeof(*j)+sizeof(*q->reply),owned=0;
    if (j->expected_source_stamp || j->statistics) { *error=dx_strdup("Native source stamps or statistics snapshots cannot be used for browser copy."); return false; }
    if (j->workbook || q->reply->path || j->column || j->search || j->date_start || j->date_end) { *error=dx_strdup("Clipboard jobs cannot contain unrelated file or query payloads."); return false; }
    if (!browser_copy_source_valid(&q->reply->analysis,j->kind==JOB_RANGE_COPY,&used,error)) return false;
    if (j->kind==JOB_RANGE_COPY) {
        CopyRequest *copy=j->copy;
        if (!copy || copy->count>DX_MAX_OBJECTS+1 || copy->guard_count>DX_MAX_OBJECTS+1 || copy->sources.count>DX_MAX_OBJECTS || j->object_id || j->is_table) { *error=dx_strdup("Invalid clipboard source snapshot bounds."); return false; }
        for (size_t role=0;role<4;role++) if (j->pivot.counts[role]) { *error=dx_strdup("Mixed clipboard shelves belong to their individual patches."); return false; }
        if (!browser_copy_add(&used,sizeof(*copy),error) || !browser_copy_owned_bytes(&copy->cells,&owned,error)) return false;
        size_t source_bytes=0;
        for (size_t i=0;i<copy->sources.count;i++) {
            if (!copy->sources.ids[i]) { *error=dx_strdup("Invalid clipboard source identity."); return false; }
            for (size_t k=0;k<i;k++) if (copy->sources.ids[k]==copy->sources.ids[i]) { *error=dx_strdup("Duplicate clipboard source identity."); return false; }
            size_t before=used;
            if (!browser_copy_source_valid(&copy->sources.items[i],false,&used,error)) return false;
            source_bytes+=used-before;
        }
        if (copy->sources.bytes!=source_bytes || source_bytes>8u*1024u*1024u) { *error=dx_strdup("Clipboard source byte accounting changed."); return false; }
        for (size_t i=0;i<copy->guard_count;i++) {
            const CopySourceGuard *guard=&copy->guards[i];
            if (guard->source>copy->sources.count || guard->expected_source_stamp) { *error=dx_strdup("Invalid clipboard source guard or unsupported native source stamp."); return false; }
            for (size_t k=0;k<i;k++) if (copy->guards[k].source==guard->source) { *error=dx_strdup("Duplicate clipboard source guard."); return false; }
        }
        for (size_t i=0;i<copy->count;i++) {
            CopyPatch *patch=&copy->patches[i];
            if (patch->source>copy->sources.count || !patch->rows || !patch->columns || patch->column>=256 || patch->columns>256-patch->column || patch->first>DX_SHEET_ROWS || patch->rows>DX_SHEET_ROWS-patch->first || patch->target_row>copy->cells.rows || patch->rows>copy->cells.rows-patch->target_row || patch->target_column>copy->cells.columns || patch->columns>copy->cells.columns-patch->target_column) { *error=dx_strdup("Invalid clipboard patch bounds."); return false; }
            Analysis *source=patch->source ? &copy->sources.items[patch->source-1] : &q->reply->analysis;
            if (source->kind==SOURCE_NONE) { *error=dx_strdup("A clipboard patch requires a real source."); return false; }
            if (!browser_copy_pivot_valid(source,&patch->pivot,patch->is_pivot,&used,error)) return false;
        }
    } else {
        bool header=j->kind==JOB_PIVOT_COPY && j->include_header;
        if (j->copy || (!j->rows && !header) || !j->cols || j->cols>256 || j->first_col>=256 || j->cols>256-j->first_col || j->rows>DX_CLIPBOARD_CELLS/j->cols-(header ? 1u : 0u) || j->offset>DX_SHEET_ROWS || j->rows>DX_SHEET_ROWS-j->offset || (j->kind==JOB_PIVOT_COPY && j->is_table)) { *error=dx_strdup("Copy up to 20,000 cells inside the worksheet."); return false; }
        if (!browser_copy_pivot_valid(&q->reply->analysis,&j->pivot,j->kind==JOB_PIVOT_COPY,&used,error)) return false;
        if (!clipboard_init(&q->copy_cells,(size_t)j->rows+(header ? 1u : 0u),j->cols,error)) return false;
        owned=q->copy_cells.capacity*sizeof(char *);
        q->direct_copy=(CopyPatch){.is_pivot=j->kind==JOB_PIVOT_COPY,.first=j->offset,.rows=(size_t)j->rows,.column=j->first_col,.columns=j->cols,.target_row=header ? 1u : 0u};
    }
    q->copy_definitions=used;
    return browser_copy_add(&used,owned,error) && browser_copy_add(&used,DX_CLIPBOARD_BYTES+1u,error);
}
static bool browser_copy_retained(size_t extra,size_t *retained,char **error) {
    BrowserQuery *q=&browser_query; size_t used=q->copy_definitions,owned;
    if (!browser_copy_owned_bytes(browser_copy_cells(),&owned,error) || !browser_copy_add(&used,owned,error)) return false;
    const char *texts[]={q->relation,q->type_relation,q->sources_json,browser_schema_key,browser_type_key[0],browser_type_key[1],browser_schema_sources.json,browser_type_sources[0].json,browser_type_sources[1].json};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (texts[i] && !browser_copy_add(&used,strlen(texts[i])+1,error)) return false;
    if (!browser_copy_add(&used,browser_schema.bytes,error) || !browser_copy_add(&used,q->copy_schema.bytes,error) || !browser_copy_add(&used,q->plan.bytes,error)) return false;
    for (size_t i=0;i<2;i++) if (!browser_copy_add(&used,browser_type_schema[i].bytes,error)) return false;
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) if (!browser_copy_add(&used,q->domains[i].bytes,error)) return false;
    if (!browser_copy_add(&used,extra,error)) return false;
    *retained=used; return true;
}
/* Nonbinary definitions/plans share the existing 24 MiB retained-data policy.
   A separate <=64 MiB malloc allocation is adopted only after final validation;
   Worker/host transport and allocator/process peaks are separate. */
static bool browser_export_add(size_t *used,size_t bytes,char **error) {
    if (bytes>DX_BROWSER_COPY_RETAINED_BYTES || *used>DX_BROWSER_COPY_RETAINED_BYTES-bytes) { *error=dx_strdup("Parquet export definitions exceed their combined 24 MiB C memory limit."); return false; }
    *used+=bytes; return true;
}
static bool browser_export_preflight(char **error) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; size_t used=sizeof(*j)+sizeof(*q->reply)+sizeof(*q);
    if (j->expected_source_stamp) { *error=dx_strdup("Native source stamps cannot be verified by browser Parquet export."); return false; }
    if (j->workbook || j->copy || j->statistics || j->column || j->search || j->date_start || j->date_end || j->offset || j->rows || j->first_col || j->cols || j->include_header || j->include_totals || j->is_preview) { *error=dx_strdup("Parquet export requires one complete source or analytical result without unrelated payloads."); return false; }
    if (j->is_table && !j->object_id) { *error=dx_strdup("A table export requires its object identity."); return false; }
    if (q->reply->path) {
        const char *name=q->reply->path; size_t length=0;
        while (length<=255 && name[length] && (unsigned char)name[length]>=32 && (unsigned char)name[length]!=127 && name[length]!='/' && name[length]!='\\') length++;
        if (!length || length>255 || name[length] || !strcmp(name,".") || !strcmp(name,"..")) { *error=dx_strdup("Browser Parquet export accepts a suggested basename, never an OS path."); return false; }
        if (!browser_export_add(&used,length+1,error)) return false;
    }
    char *valid=browser_relation_sql(&q->reply->analysis,q->analytical ? &j->pivot : NULL,error); if (!valid) return false; free(valid);
    if (!browser_export_add(&used,analysis_text_bytes(&q->reply->analysis),error)) return false;
    for (size_t role=0;role<4;role++) for (size_t i=0;i<j->pivot.counts[role];i++) {
        if (!q->analytical) { *error=dx_strdup("A direct Parquet export cannot contain analytical shelves."); return false; }
        const PivotField *field=&j->pivot.fields[role][i];
        if (!browser_export_add(&used,strlen(field->name)+1+(field->value ? strlen(field->value)+1 : 0),error)) return false;
    }
    q->export_definitions=used; return true;
}
static bool browser_export_retained(size_t extra,size_t *retained,char **error) {
    BrowserQuery *q=&browser_query; size_t used=q->export_definitions;
    const char *texts[]={q->relation,q->type_relation,q->sources_json,browser_schema_key,browser_type_key[0],browser_type_key[1],browser_schema_sources.json,browser_type_sources[0].json,browser_type_sources[1].json};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (texts[i] && !browser_export_add(&used,strlen(texts[i])+1,error)) return false;
    if (!browser_export_add(&used,browser_schema.bytes,error) || !browser_export_add(&used,q->plan.bytes,error)) return false;
    for (size_t i=0;i<2;i++) if (!browser_export_add(&used,browser_type_schema[i].bytes,error)) return false;
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) if (!browser_export_add(&used,q->domains[i].bytes,error)) return false;
    if (!browser_export_add(&used,extra,error)) return false;
    *retained=used; return true;
}
static bool browser_statistics_add_bytes(size_t *used,size_t bytes,char **error) {
    if (bytes>DX_PAGE_WIRE_TEXT_BYTES || *used>DX_PAGE_WIRE_TEXT_BYTES-bytes) { *error=dx_strdup("Selection statistics exceed their combined 4 MiB memory limit."); return false; }
    *used+=bytes; return true;
}
static bool browser_statistics_retained(size_t extra,size_t *retained,char **error) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; size_t used=q->statistics_definitions;
    if (!q->mixed) {
        used=analysis_text_bytes(&q->reply->analysis);
        for (size_t role=0;role<4;role++) for (size_t i=0;i<j->pivot.counts[role];i++) {
            const PivotField *f=&j->pivot.fields[role][i];
            if (!browser_statistics_add_bytes(&used,strlen(f->name)+1+(f->value ? strlen(f->value)+1 : 0),error)) return false;
        }
    }
    const char *texts[]={q->relation,q->type_relation,q->statistics_cells,q->sources_json,browser_schema_key,browser_type_key[0],browser_type_key[1],browser_schema_sources.json,browser_type_sources[0].json,browser_type_sources[1].json};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (texts[i] && !browser_statistics_add_bytes(&used,strlen(texts[i])+1,error)) return false;
    if (!browser_statistics_add_bytes(&used,browser_schema.bytes,error) || !browser_statistics_add_bytes(&used,q->plan.bytes,error)) return false;
    for (size_t i=0;i<2;i++) if (!browser_statistics_add_bytes(&used,browser_type_schema[i].bytes,error)) return false;
    for (size_t i=0;i<DX_PIVOT_FIELDS;i++) if (!browser_statistics_add_bytes(&used,q->domains[i].bytes,error)) return false;
    if (!browser_statistics_add_bytes(&used,extra,error)) return false;
    *retained=used; return true;
}
static bool browser_source_cache_admit(size_t extra,char **error) {
    BrowserQuery *q=&browser_query; size_t retained,bytes=q->sources_json ? strlen(q->sources_json)+1 : 0;
    if (bytes>SIZE_MAX-extra) { *error=dx_strdup("Browser source-cache staging overflows its memory bound."); return false; }
    extra+=bytes;
    if (q->job->kind==JOB_EXPORT) return browser_export_retained(extra,&retained,error);
    if (q->copying) return browser_copy_retained(extra,&retained,error);
    if (q->job->kind==JOB_STATISTICS) return browser_statistics_retained(extra,&retained,error);
    return true;
}
/* Admit replacement before changing either schema or its bound cache key. The
   incoming DESCRIBE, old cache and new schema/JSON can coexist. Ordinary
   unbound copy/statistics retain their existing admission behavior. */
static bool browser_describe_retained(const Page *describe,char **error) {
    if (browser_query.job->kind!=JOB_EXPORT && !browser_query.sources_json) return true;
    if (describe->column_count!=6 || (describe->row_count && !describe->cells)) return true;
    size_t extra=describe->bytes;
    if (!browser_export_add(&extra,describe->row_count*sizeof(Column),error)) return false;
    for (size_t i=0;i<describe->row_count;i++) for (size_t c=0;c<2;c++) {
        const Cell *cell=&describe->cells[i*6+c];
        if (!cell->is_null && cell->text && !browser_export_add(&extra,strlen(cell->text)+1,error)) return false;
    }
    return browser_source_cache_admit(extra,error);
}
static void browser_execute(char *sql,BrowserStage stage) {
    if (!sql) { browser_fail("Could not build the browser query."); return; }
    if (strlen(sql)>DX_BROWSER_SQL_BYTES) { free(sql); browser_fail("Browser query exceeds 1 MiB. Narrow the filter selection."); return; }
    if (browser_sequence==UINT32_MAX) { free(sql); browser_fail("Browser request limit reached. Reopen the page."); return; }
    BrowserQuery *q=&browser_query; QueryJob *j=q->job;
    if (j->kind==JOB_STATISTICS) {
        char *error=NULL; size_t retained;
        if (!browser_statistics_retained(strlen(sql)+1,&retained,&error)) { free(sql); browser_fail(error); free(error); return; }
    }
    if (q->copying) {
        char *error=NULL; size_t retained,extra=strlen(sql)+1;
        if (stage==BROWSER_COPY_PAGE) {
            CopyPatch *patch=browser_copy_patch();
            extra+=q->copy_schema.bytes+patch->rows*patch->columns*(sizeof(Cell)+1u)+DX_PAGE_WIRE_TEXT_BYTES;
        }
        if (!browser_copy_retained(extra,&retained,&error)) { free(sql); browser_fail(error); free(error); return; }
    }
    if (j->kind==JOB_EXPORT) {
        char *error=NULL; size_t retained;
        if (!browser_export_retained(strlen(sql)+1,&retained,&error)) { free(sql); browser_fail(error); free(error); return; }
    }
    q->stage=stage; q->ticket=++browser_sequence;
    char id[24],object[24],revision[24],offset[24],total[24];
    snprintf(id,sizeof(id),"%" PRIu64,j->id); snprintf(object,sizeof(object),"%" PRIu64,j->object_id); snprintf(revision,sizeof(revision),"%" PRIu64,j->revision);
    snprintf(offset,sizeof(offset),"%" PRIu64,(stage==BROWSER_PAGE || stage==BROWSER_DISTINCT_PAGE || stage==BROWSER_PIVOT_ROWS) ? j->offset : 0);
    if (stage==BROWSER_COPY_PAGE) snprintf(offset,sizeof(offset),"%" PRIu64,browser_copy_patch()->first);
    snprintf(total,sizeof(total),"%" PRIi64,j->known_total);
    bool totals=stage==BROWSER_PIVOT_TOTALS || stage==BROWSER_PIVOT_COLUMNS || stage==BROWSER_STATISTICS; if (totals) snprintf(total,sizeof(total),"1");
    if (stage==BROWSER_PIVOT_ROWS) snprintf(total,sizeof(total),"%" PRIu64,q->reply->page.total);
    const Page *schema=stage==BROWSER_COPY_PAGE ? &q->copy_schema : stage==BROWSER_PAGE ? &browser_schema : stage==BROWSER_PIVOT_TOTALS ? &q->totals_schema : stage==BROWSER_PIVOT_ROWS ? &q->rows_schema : stage==BROWSER_PIVOT_COLUMNS ? &q->columns_schema : NULL;
    char *columns=schema ? browser_columns_json(schema) : NULL;
    /* Browser counters count submitted SQL; the async host may supersede a
       request before the official engine executes it. */
    browser_owner->sql_executed++; if (stage==BROWSER_COUNT || stage==BROWSER_PIVOT_COUNT) browser_owner->counts_executed++;
    browser_send(browser_cookie,q->ticket,sql,id,object,revision,offset,totals || stage==BROWSER_PIVOT_ROWS || ((stage==BROWSER_PAGE || stage==BROWSER_DISTINCT_PAGE) && j->known_total>=0) ? total : NULL,columns,stage==BROWSER_COPY_PAGE ? (unsigned)browser_copy_patch()->rows : totals ? 1 : stage==BROWSER_DISTINCT_PAGE ? DX_DISTINCT_ROWS : stage==BROWSER_PIVOT_DOMAIN ? 129 : DX_PAGE_ROWS,stage==BROWSER_COPY_PAGE,q->sources_json);
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
static void browser_prepare_query(void);
static void browser_copy_release_relation(void) {
    BrowserQuery *q=&browser_query;
    free(q->relation); q->relation=NULL; free(q->type_relation); q->type_relation=NULL;
    browser_pivot_plan_free(&q->plan); for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&q->domains[i]);
    page_free(&q->copy_schema); q->domain_index=0; q->resolved_types[0]=q->resolved_types[1]=false;
}
static void browser_copy_finish(bool empty) {
    BrowserQuery *q=&browser_query; char *error=NULL; size_t retained;
    browser_copy_release_relation();
    if (!browser_copy_retained(DX_CLIPBOARD_BYTES+1u,&retained,&error)) { browser_fail(error); free(error); return; }
    char *text=empty ? dx_strdup("") : clipboard_tsv(browser_copy_cells(),&error);
    if (!text) { browser_fail(error); free(error); return; }
    q->reply->clipboard=text; browser_publish(NULL);
}
static void browser_copy_next_patch(void) {
    BrowserQuery *q=&browser_query; char *error=NULL;
    browser_copy_release_relation();
    if (q->job->kind==JOB_RANGE_COPY && q->copy_index==q->job->copy->count) { browser_copy_finish(false); return; }
    q->analytical=browser_copy_patch()->is_pivot;
    q->relation=browser_relation_sql(browser_active_analysis(),browser_context(),&error);
    if (!q->relation) { browser_fail(error); free(error); return; }
    browser_prepare_query();
}
static void browser_fetch_copy(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; CopyPatch *patch=browser_copy_patch(); char *error=NULL;
    PivotDefinition *pivot=browser_context();
    if ((pivot && !browser_copy_pivot_schema_valid(&browser_schema,pivot,&q->plan,&error)) ||
        !browser_copy_schema(&q->copy_schema,&browser_schema,patch->column,patch->columns,&error)) { browser_fail(error); free(error); return; }
    if (j->kind==JOB_PIVOT_COPY && j->include_header) {
        size_t keys=pivot->counts[PIVOT_ROWS];
        for (size_t c=0;c<patch->columns;c++) {
            size_t index=patch->column+c; char *label;
            if (index<keys) label=pivot_field_label(&pivot->fields[PIVOT_ROWS][index]);
            else {
                size_t measure=(index-keys)%pivot->counts[PIVOT_VALUES];
                const PivotField *field=&pivot->fields[PIVOT_VALUES][measure]; char *aggregate=aggregate_measure_label(field->aggregate,field->name);
                label=pivot->counts[PIVOT_COLUMNS] ? dx_printf("%s_%s",q->plan.headers[index-keys].caption,aggregate) : dx_strdup(aggregate); free(aggregate);
            }
            bool valid=clipboard_set(browser_copy_cells(),0,c,label,&error); free(label);
            if (!valid) { browser_fail(error); free(error); return; }
        }
    }
    if (!patch->rows) { browser_copy_finish(false); return; }
    char *sql=browser_copy_sql(q->relation,&browser_schema,patch->first,patch->rows,patch->column,patch->columns,&error);
    if (!sql) { browser_fail(error); free(error); return; }
    browser_execute(sql,BROWSER_COPY_PAGE);
}
static void browser_statistics_next_range(void) {
    BrowserQuery *q=&browser_query; StatisticsRequest *request=q->job->statistics; char *error=NULL;
    free(q->relation); q->relation=NULL; free(q->type_relation); q->type_relation=NULL;
    browser_pivot_plan_free(&q->plan); for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&q->domains[i]);
    q->domain_index=0; q->resolved_types[0]=q->resolved_types[1]=false;
    if (q->statistics_range==request->count) {
        char *sql=browser_statistics_aggregate_sql(q->statistics_cells,&request->cells,&error);
        if (!sql) { browser_fail(error); free(error); return; }
        browser_execute(sql,BROWSER_STATISTICS); return;
    }
    q->analytical=request->ranges[q->statistics_range].is_pivot;
    q->relation=browser_relation_sql(browser_active_analysis(),browser_context(),&error);
    if (!q->relation) { browser_fail(error); free(error); return; }
    browser_prepare_query();
}
static void browser_fetch_result(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job;
    if (q->copying) { browser_fetch_copy(); return; }
    if (j->kind==JOB_EXPORT) {
        if (browser_sequence==UINT32_MAX) { browser_fail("Browser request limit reached. Reopen the page."); return; }
        char name[64],generation[24],object[24],revision[24],*error=NULL; size_t retained;
        snprintf(name,sizeof(name),"export-%u-%u.parquet",browser_cookie,browser_sequence+1);
        char *sql=browser_export_sql(q->relation,&browser_schema,q->analytical ? &j->pivot : NULL,&q->plan,name,&error);
        if (!sql || !browser_export_retained(strlen(sql)+1,&retained,&error)) { free(sql); browser_fail(error); free(error); return; }
        q->stage=BROWSER_EXPORT; q->ticket=++browser_sequence; browser_owner->sql_executed++;
        snprintf(generation,sizeof(generation),"%" PRIu64,j->id); snprintf(object,sizeof(object),"%" PRIu64,j->object_id); snprintf(revision,sizeof(revision),"%" PRIu64,j->revision);
        browser_export_send(browser_cookie,q->ticket,sql,name,generation,object,revision,q->sources_json); free(sql); return;
    }
    if (j->kind!=JOB_STATISTICS) { browser_fetch_page(); return; }
    char *error=NULL,*sql=NULL;
    if (q->mixed) {
        StatisticsRange *range=&j->statistics->ranges[q->statistics_range];
        sql=browser_statistics_cells_sql(q->relation,&browser_schema,range->first,range->rows,range->column,range->columns,&error);
        size_t retained;
        bool valid=sql && browser_statistics_retained(strlen(sql)+1,&retained,&error) && browser_statistics_append_cells(&q->statistics_cells,sql,&error); free(sql);
        if (!valid || !browser_statistics_retained(0,&retained,&error)) { browser_fail(error); free(error); return; }
        q->statistics_range++; browser_statistics_next_range(); return;
    }
    sql=browser_statistics_sql(q->relation,&browser_schema,j->offset,j->rows,j->first_col,j->cols,&error);
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
static bool browser_apply_types(unsigned slot,char **error) {
    const Page *schema=&browser_type_schema[slot]; Analysis *a=browser_active_analysis();
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
    browser_sources_cache(&browser_type_sources[slot]);
}
static void browser_prepare_query(void) {
    BrowserQuery *q=&browser_query; QueryJob *j=q->job; Analysis *a=browser_active_analysis(); PivotDefinition *pivot=browser_context(); char *error=NULL;
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
        if ((!browser_type_key[slot] || strcmp(browser_type_key[slot],relation) || !browser_sources_match(&browser_type_sources[slot])) && browser_schema_key && !strcmp(browser_schema_key,relation) && browser_sources_match(&browser_schema_sources)) {
            size_t retained;
            if (j->kind==JOB_EXPORT && !browser_export_retained(browser_schema.bytes+2*(strlen(relation)+1),&retained,&error)) { free(relation); browser_fail(error); free(error); return; }
            if (q->sources_json && !browser_source_cache_admit(browser_schema.bytes+2*(strlen(relation)+1),&error)) { free(relation); browser_fail(error); free(error); return; }
            browser_copy_type_schema(slot,relation);
        }
        if (browser_type_key[slot] && !strcmp(browser_type_key[slot],relation) && browser_sources_match(&browser_type_sources[slot])) {
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
    if (j->kind!=JOB_VALIDATE_SQL && browser_schema_key && !strcmp(browser_schema_key,q->relation) && browser_sources_match(&browser_schema_sources)) { browser_fetch_result(); return; }
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
    if (browser_query.stage==BROWSER_EXPORT) return; /* Only the binary callback owns this phase. */
    if (failure && *failure) { browser_publish(failure); return; }
    uint64_t total=0; char *error=NULL; Page page={0};
    QueryJob *j=browser_query.job; QueryReply *r=browser_query.reply;
    uint64_t offset=(browser_query.stage==BROWSER_PAGE || browser_query.stage==BROWSER_DISTINCT_PAGE || browser_query.stage==BROWSER_PIVOT_ROWS) ? j->offset : 0;
    if (browser_query.stage==BROWSER_COPY_PAGE) offset=browser_copy_patch()->first;
    if (!browser_uint64(total_text,&total)) { browser_publish("Invalid browser row count."); return; }
#ifdef DUXCEL_DEV_REVIEW
    double decode_started=dx_millis();
    browser_query.callbacks++; browser_query.wire_bytes+=size;
#endif
    size_t available=SIZE_MAX;
    if (browser_query.copying) {
        size_t retained;
        if (!browser_copy_retained(0,&retained,&error)) { browser_publish(error); free(error); return; }
        available=DX_BROWSER_COPY_RETAINED_BYTES-retained;
    }
    if (j->kind==JOB_EXPORT) {
        size_t retained;
        if (!browser_export_retained(0,&retained,&error)) { browser_publish(error); free(error); return; }
        available=DX_BROWSER_COPY_RETAINED_BYTES-retained;
    }
    bool decoded=browser_query.copying ? browser_query.stage==BROWSER_COPY_PAGE ?
        page_from_wire_copy_owned(&page,wire,size,offset,total,pending!=0,available,&error) :
        page_from_wire_owned(&page,wire,size,offset,total,pending!=0,available,&error) :
        j->kind==JOB_EXPORT ? page_from_wire_owned(&page,wire,size,offset,total,pending!=0,available,&error) : page_from_wire(&page,wire,size,offset,total,pending!=0,&error);
#ifdef DUXCEL_DEV_REVIEW
    browser_query.decode_ms+=dx_millis()-decode_started; browser_query.decoded_bytes+=page.bytes;
#endif
    if (!decoded) { browser_publish(error); free(error); return; }
    if (browser_query.copying) {
        size_t retained;
        if (!browser_copy_retained(page.bytes,&retained,&error)) { page_free(&page); browser_publish(error); free(error); return; }
    }
    if (j->kind==JOB_EXPORT) {
        size_t retained;
        if (!browser_export_retained(page.bytes,&retained,&error)) { page_free(&page); browser_publish(error); free(error); return; }
    }
    if (browser_query.stage==BROWSER_COPY_PAGE) {
        BrowserQuery *q=&browser_query; CopyPatch *patch=browser_copy_patch(); bool valid=page.column_count==q->copy_schema.column_count;
        for (size_t c=0;valid && c<page.column_count;c++) {
            const Column *actual=&page.columns[c],*expected=&q->copy_schema.columns[c];
            valid=!strcmp(actual->name,expected->name) && !strcmp(actual->type_name,expected->type_name) && actual->type==expected->type && actual->number_type.kind==expected->number_type.kind && actual->number_type.precision==expected->number_type.precision && actual->number_type.scale==expected->number_type.scale;
        }
        size_t retained=0,owned=0,actual_rows=page.row_count;
        if (valid) valid=browser_copy_retained(0,&retained,&error) && browser_copy_owned_bytes(browser_copy_cells(),&owned,&error) &&
            browser_copy_apply(browser_copy_cells(),&page,patch->target_row,patch->target_column,patch->rows,patch->columns,retained-owned,&error);
        page_free(&page);
        if (!valid) { browser_publish(error ? error : "Copied result no longer matches its typed projection."); free(error); return; }
        if (j->kind==JOB_RANGE_COPY) { q->copy_index++; browser_copy_next_patch(); return; }
        bool header=j->kind==JOB_PIVOT_COPY && j->include_header;
        if (actual_rows || header) q->copy_cells.rows=actual_rows+(header ? 1u : 0u);
        browser_copy_finish(!actual_rows && !header); return;
    }
    if (browser_query.stage==BROWSER_STATISTICS) {
        BrowserQuery *q=&browser_query; size_t retained=0;
        bool valid=browser_statistics_retained(0,&retained,&error) && browser_statistics_from_page(&r->statistics,&page,q->mixed ? j->statistics->cells : j->rows*j->cols,retained,&error); page_free(&page);
        if (valid) r->statistics.query_ms=dx_millis()-q->started; browser_publish(valid ? NULL : error); free(error); return;
    }
    if (j->kind==JOB_STATISTICS) {
        size_t retained;
        if (!browser_statistics_retained(page.bytes,&retained,&error)) { page_free(&page); browser_publish(error); free(error); return; }
    }
    if (browser_query.stage==BROWSER_PIVOT_DOMAIN) {
        BrowserQuery *q=&browser_query; PivotDefinition *pivot=browser_context(); Analysis *source=browser_active_analysis(); size_t keys=pivot->counts[PIVOT_ROWS],measures=pivot->counts[PIVOT_VALUES],product=1,used=page.bytes;
        if (used>DX_PAGE_WIRE_TEXT_BYTES) { page_free(&page); browser_publish("Pivot column domains exceed 4 MiB."); return; }
        for (size_t i=0;i<q->domain_index;i++) { size_t count=q->domains[i].row_count; product*=count ? count : 1; if (q->domains[i].bytes>DX_PAGE_WIRE_TEXT_BYTES-used) { page_free(&page); browser_publish("Pivot column domains exceed 4 MiB."); return; } used+=q->domains[i].bytes; }
        size_t count=page.row_count ? page.row_count : 1;
        if (page.column_count!=1 || page.row_count>128 || count>(128-keys)/(product*measures)) { page_free(&page); browser_publish("Pivot exceeds 128 result columns. Filter the column fields or use fewer measures."); return; }
        q->domains[q->domain_index++]=page;
        if (q->domain_index<pivot->counts[PIVOT_COLUMNS]) { char *sql=browser_pivot_domain_sql(source,pivot,q->domain_index,&error); if (!sql) { browser_publish(error); free(error); return; } browser_execute(sql,BROWSER_PIVOT_DOMAIN); return; }
        size_t retained=0; bool valid=true;
        if (j->kind==JOB_STATISTICS) {
            valid=browser_statistics_retained(0,&retained,&error);
            if (valid) for (size_t i=0;i<q->domain_index;i++) retained-=q->domains[i].bytes;
        }
        /* The planner independently bounds its new owned plan to 4 MiB;
           reserve that amount while all typed input domains still live. */
        if (valid && j->kind==JOB_EXPORT) { size_t combined; valid=browser_export_retained(DX_PAGE_WIRE_TEXT_BYTES,&combined,&error); }
        if (valid) valid=browser_pivot_plan_build_retained(&q->plan,source,pivot,q->domains,retained,&error);
        for (size_t i=0;i<DX_PIVOT_FIELDS;i++) page_free(&q->domains[i]);
        if (!valid) { browser_publish(error); free(error); return; }
        free(q->relation); q->relation=NULL;
        if (j->kind==JOB_EXPORT) { size_t combined; if (!browser_export_retained(strlen(q->plan.sql)+1,&combined,&error)) { browser_publish(error); free(error); return; } }
        q->relation=dx_strdup(q->plan.sql);
        if (q->matrix && j->include_totals) {
            q->rows_sql=browser_pivot_readout_sql(source,pivot,&q->plan,true,j->offset,&error);
            if (q->rows_sql) q->columns_sql=browser_pivot_readout_sql(source,pivot,&q->plan,false,0,&error);
            if (!q->rows_sql || !q->columns_sql) { browser_publish(error); free(error); return; }
        }
        if (browser_schema_key && !strcmp(browser_schema_key,q->relation) && browser_sources_match(&browser_schema_sources)) browser_fetch_result();
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
        if (!browser_describe_retained(&page,&error)) { page_free(&page); browser_publish(error); free(error); return; }
        unsigned slot=browser_query.type_slot; bool valid=browser_schema_from_describe(&browser_type_schema[slot],&page,&error); page_free(&page);
        if (!valid) { browser_publish(error); free(error); return; }
        free(browser_type_key[slot]); browser_type_key[slot]=browser_query.type_relation; browser_query.type_relation=NULL;
        browser_sources_cache(&browser_type_sources[slot]);
        if (!browser_apply_types(slot,&error)) { browser_publish(error); free(error); return; }
        browser_prepare_query(); return;
    }
    if (browser_query.stage==BROWSER_DESCRIBE || browser_query.stage==BROWSER_VALIDATE) {
        if (!browser_describe_retained(&page,&error)) { page_free(&page); browser_publish(error); free(error); return; }
        bool valid=browser_schema_from_describe(&browser_schema,&page,&error); page_free(&page);
        if (!valid) { browser_publish(error); free(error); return; }
        free(browser_schema_key); browser_schema_key=NULL;
        if (j->kind==JOB_EXPORT) { size_t retained; if (!browser_export_retained(strlen(browser_query.relation)+1,&retained,&error)) { browser_publish(error); free(error); return; } }
        browser_schema_key=dx_strdup(browser_query.relation);
        browser_sources_cache(&browser_schema_sources);
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
EMSCRIPTEN_KEEPALIVE int dx_browser_export_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,unsigned char *owned_data,size_t data_size,const char *failure) {
    BrowserQuery *q=&browser_query;
    if (!browser_owner || !q->job || !q->reply || q->job->kind!=JOB_EXPORT || q->stage!=BROWSER_EXPORT || cookie!=browser_cookie || ticket!=q->ticket) return 0;
    if (failure && *failure) { browser_publish(failure); return 0; }
    if (!owned_data || data_size<12 || data_size>BROWSER_EXPORT_BYTES) { browser_publish("Invalid bounded Parquet export bytes."); return 0; }
    uint32_t footer=0; for (unsigned i=0;i<4;i++) footer|=(uint32_t)owned_data[data_size-8+i]<<(i*8);
    if (memcmp(owned_data,"PAR1",4) || memcmp(owned_data+data_size-4,"PAR1",4) || !footer || footer>data_size-12) { browser_publish("Invalid Parquet export header/footer."); return 0; }
    char *error=NULL; size_t retained; Page count={0};
    if (!browser_export_retained(0,&retained,&error) || !page_from_wire_owned(&count,wire,size,0,1,false,DX_BROWSER_COPY_RETAINED_BYTES-retained,&error)) { browser_publish(error); free(error); return 0; }
    uint64_t rows=0; bool valid=count.row_count==1 && count.column_count==1 && count.cells && !count.cells[0].is_null && count.cells[0].text &&
        !strcmp(count.columns[0].name,"Count") && count.columns[0].numeric &&
        ((count.columns[0].type==DUCKDB_TYPE_BIGINT && !strcmp(count.columns[0].type_name,"BIGINT") && count.columns[0].number_type.kind==NUM_I64) ||
         (count.columns[0].type==DUCKDB_TYPE_UBIGINT && !strcmp(count.columns[0].type_name,"UBIGINT") && count.columns[0].number_type.kind==NUM_U64)) &&
        !(count.cells[0].text[0]=='0' && count.cells[0].text[1]) && browser_uint64(count.cells[0].text,&rows) &&
        (count.columns[0].type!=DUCKDB_TYPE_BIGINT || rows<=INT64_MAX);
#ifdef DUXCEL_DEV_REVIEW
    q->callbacks++; q->wire_bytes+=size+data_size; q->decoded_bytes+=count.bytes;
#endif
    page_free(&count);
    if (!valid) { browser_publish("Invalid exact COPY row count."); return 0; }
    /* No failure path after adoption: the host releases its temporary Count
       wire, while poll/reply_free now owns this one UI-heap malloc allocation. */
    q->reply->export_data=owned_data; q->reply->export_bytes=data_size; q->reply->exported_rows=rows;
    browser_publish(NULL); return 1;
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
    if (j->kind==JOB_VERIFY_SOURCE) {
        r->source_digest_status=SOURCE_DIGEST_UNSUPPORTED;
        browser_fail("Source-content verification is unavailable in the browser engine; no native path or verification job was submitted."); return;
    }
    if (j->kind==JOB_COLLECT_RECORDS) {
        browser_fail("Native record collection is unavailable in the browser engine; no identity query was submitted."); return;
    }
    char *source_error=NULL;
    if (!browser_sources_prepare(&source_error)) { browser_fail(source_error); free(source_error); return; }
    if (j->refresh_source) browser_clear_schemas();
    q->copying=j->kind==JOB_COPY || j->kind==JOB_PIVOT_COPY || j->kind==JOB_RANGE_COPY;
    q->matrix=j->kind==JOB_PIVOT && !j->is_table;
    q->analytical=q->matrix || ((j->kind==JOB_STATISTICS || j->kind==JOB_EXPORT) && j->object_id && !j->is_table);
    bool invalid_pivot=false;
    for (size_t role=0;role<4;role++) invalid_pivot|=j->pivot.counts[role]>DX_PIVOT_FIELDS;
    if (invalid_pivot) { browser_fail("Matrix shelves exceed their bounded field limit."); return; }
    if (j->kind==JOB_EXPORT) {
        char *error=NULL;
        if (!browser_export_preflight(&error)) { browser_fail(error ? error : "Invalid Parquet export snapshot."); free(error); return; }
    }
    if (q->copying) {
        char *error=NULL;
        if (!browser_copy_preflight(&error)) { browser_fail(error ? error : "Invalid clipboard source snapshot."); free(error); return; }
        browser_copy_next_patch(); return;
    }
    if (j->kind==JOB_STATISTICS) {
        bool pivot=false; for (size_t role=0;role<4;role++) pivot|=j->pivot.counts[role]!=0;
        if (j->statistics) {
            char *error=NULL; q->mixed=true;
            if (j->object_id || j->is_table || pivot || j->expected_source_stamp || !browser_statistics_request_valid(&r->analysis,j->statistics,&q->statistics_definitions,&error)) { browser_fail(error ? error : "Invalid mixed statistics object identity or native source stamp."); free(error); return; }
            q->statistics_cells=browser_statistics_manual_sql(j->statistics->texts,j->statistics->text_count,&error);
            if (!q->statistics_cells) { browser_fail(error); free(error); return; }
            browser_statistics_next_range(); return;
        }
        if ((j->is_table && (!j->object_id || pivot)) || (!j->object_id && pivot)) { browser_fail("Invalid statistics object identity or table definition."); return; }
        if (q->analytical && !j->pivot.counts[PIVOT_VALUES]) { browser_fail("Add a field to Values for analytical result statistics."); return; }
        if (j->expected_source_stamp) { browser_fail("Native source stamps cannot be verified by the browser engine."); return; }
        if (!j->cols || j->cols>256 || j->offset>DX_SHEET_ROWS || j->rows>DX_SHEET_ROWS-j->offset) { browser_fail("Selection statistics support up to 256 columns inside the worksheet."); return; }
    }
    if (j->kind!=JOB_DISTINCT && j->kind!=JOB_PAGE && j->kind!=JOB_COUNT && j->kind!=JOB_VALIDATE_SQL && j->kind!=JOB_STEP_PREVIEW && j->kind!=JOB_PIVOT && j->kind!=JOB_STATISTICS && j->kind!=JOB_EXPORT) {
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
WorkerFormatUsage worker_format_usage(QueryWorker *w) {
    (void)w; return (WorkerFormatUsage){0}; /* Native format jobs reject synchronously; no native reservation is admitted. */
}
void worker_cancel(QueryWorker *w) { w->latest_id++; browser_discard(); reply_free(w->reply); w->reply=NULL; browser_cancel(0); }
void worker_stop(QueryWorker *w) {
    if (!w->started) return;
    worker_cancel(w); browser_cancel(1); browser_clear_schemas(); browser_owner=NULL;
    dx_cond_destroy(&w->condition); dx_mutex_destroy(&w->mutex); w->started=false;
}
