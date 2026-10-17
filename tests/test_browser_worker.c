/* Headless acceptance against the real official browser Worker. The runner
   supplies Module.dxEngine=createEngineHost(...), exports the worker callback,
   and polls Module.dxBrowserWorkerTestDone. No renderer/window is created. */
#include "data/worker.h"
#include <emscripten/emscripten.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DUXCEL_BROWSER_HOST
#error This driver requires the real DUXCEL_BROWSER_HOST worker backend.
#endif

extern void dx_browser_query_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,const char *total,int pending,const char *failure);
EM_JS(int, observe_host, (void), {
    const host=Module['dxEngine'];
    if(!host || typeof host.submit!=='function' || typeof host.cancel!=='function')return 0;
    const submit=host.submit.bind(host);
    host.submit=(runtime,cookie,ticket,request)=>{
        Module['dxBrowserWorkerObserved']={cookie,ticket,generation:request.generation,object:request.objectId,revision:request.revision,offset:request.offset};
        return submit(runtime,cookie,ticket,request);
    };
    return 1;
})
EM_JS(int, observed_identity, (const char *id,const char *object,const char *revision,const char *offset), {
    const value=Module['dxBrowserWorkerObserved'];
    return Boolean(value && value.generation===UTF8ToString(id) && value.object===UTF8ToString(object) && value.revision===UTF8ToString(revision) && value.offset===UTF8ToString(offset));
})
EM_JS(unsigned, observed_cookie, (void), { return Module['dxBrowserWorkerObserved'].cookie; })
EM_JS(unsigned, observed_ticket, (void), { return Module['dxBrowserWorkerObserved'].ticket; })
EM_JS(unsigned, observed_cancels, (void), {
    const count=Module['dxTestCancels'];
    return Number.isInteger(count) && count>=0 && count<4294967295 ? count>>>0 : 4294967295;
})
EM_JS(void, completed, (int ok,unsigned checks,unsigned stages), {
    Module['dxBrowserWorkerTestDone']={ok:Boolean(ok),checks,stages};
})

static const char exact_sql[]=
    "SELECT 9007199254740993::BIGINT AS wide,"
    "CAST('18446744073709551615' AS UBIGINT) AS u64,"
    "CAST('1234567890123456789012345678.1234567890' AS DECIMAL(38,10)) AS amount,"
    "CAST('-170141183460469231731687303715884105728' AS HUGEINT) AS huge_min,"
    "CAST('170141183460469231731687303715884105727' AS HUGEINT) AS huge_max,"
    "CAST('2026-10-17 12:34:56.123456789' AS TIMESTAMP_NS) AS nanosecond,"
    "NULL::VARCHAR AS missing,' ' AS space,'' AS empty,'NULL' AS literal,"
    "true AS approved,NULL::BIGINT AS null_number";
static const char range_sql[]=
    "SELECT i::BIGINT AS row_id,('row-' || i::VARCHAR)::VARCHAR AS label,"
    "(i % 100)::DECIMAL(5,2) AS amount FROM range(600) t(i) ORDER BY i";
static const char long_sql[]=
    "SELECT SUM((i % 97)::HUGEINT) AS slow_total FROM range(500000000) t(i)";
static const char latest_sql[]="SELECT 'latest wins'::VARCHAR AS result";
static const char cancel_recovery_sql[]="SELECT 'recovered after cancel'::VARCHAR AS result";
static const char rejection_recovery_sql[]="SELECT 'recovered after local rejection'::VARCHAR AS result";
static const char categories_sql[]=
    "SELECT CASE i WHEN 0 THEN NULL::VARCHAR WHEN 1 THEN '' WHEN 2 THEN 'NULL' "
    "ELSE 'v-' || lpad((i-3)::VARCHAR,3,'0') END AS category,"
    "CASE WHEN i<150 THEN 'keep' ELSE 'drop' END AS bucket FROM range(300) t(i)";
static const char decimal_filter_sql[]=
    "SELECT * FROM (VALUES (1.00::DECIMAL(4,2),'A'),"
    "(1.01::DECIMAL(4,2),'B'),(1.02::DECIMAL(4,2),'C')) t(amount,label)";
static const char dates_sql[]=
    "SELECT * FROM (VALUES (NULL::TIMESTAMP),"
    "(TIMESTAMP '2024-12-31 23:59:59.999999'),"
    "(TIMESTAMP '2025-01-01 00:00:00'),(TIMESTAMP '2025-01-01 00:00:59.999999'),"
    "(TIMESTAMP '2025-01-01 00:01:00'),(TIMESTAMP '2025-01-01 01:00:00'),"
    "(TIMESTAMP '2025-01-02 00:00:00'),(TIMESTAMP '2025-02-01 00:00:00'),"
    "(TIMESTAMP '2026-01-01 00:00:00')) t(occurred)";
static const uint64_t object_id=UINT64_C(18014398509481987),revision_id=UINT64_C(36028797018963971);
typedef enum {
    WAIT_EXACT,WAIT_PAGE0,WAIT_PAGE256,WAIT_PAGE512,WAIT_COUNT,WAIT_TABLE,
    WAIT_VALIDATE,WAIT_PREVIEW,WAIT_ERROR,WAIT_RECOVERY,
    WAIT_DISTINCT0,WAIT_DISTINCT128,WAIT_DISTINCT256,WAIT_DISTINCT_SEARCH,
    WAIT_DISTINCT_PRIMARY,WAIT_DISTINCT_SECONDARY,WAIT_DISTINCT_DECIMAL_PRIMARY,
    WAIT_DISTINCT_DECIMAL_SECONDARY,WAIT_DATE_YEAR,WAIT_DATE_MONTH,
    WAIT_DATE_DAY,WAIT_DATE_HOUR,WAIT_DATE_MINUTE,WAIT_DATE_NULL,
    WAIT_DISTINCT_ERROR,WAIT_DISTINCT_RECOVERY,WAIT_OVERSIZE_SCHEMA,RUN_OVERSIZE,
    WAIT_OVERSIZE_ERROR,WAIT_OVERSIZE_RECOVERY,WAIT_SUPERSEDE_SCHEMA,
    RUN_SUPERSEDE,WAIT_LATEST,QUIET_LATEST,WAIT_CANCEL_SCHEMA,RUN_CANCEL,
    QUIET_CANCEL,WAIT_CANCEL_RECOVERY,WAIT_STOP_SCHEMA,RUN_STOP,QUIET_STOP,FINISHED
} Stage;
static QueryWorker worker;
static QueryReply *reply;
static Page retained;
static DistinctPage retained_distinct;
static char *retained_rejection;
static Stage stage;
static double entered;
static uint64_t next_id=UINT64_C(9007199254740993),expected_id,expected_offset,expected_sql,expected_counts;
static JobKind expected_kind;
static bool expected_preview;
static unsigned checks,stages,stale_cookie,stale_ticket,late_callbacks,rejection_cancels;
static void retained_values(void);

static void die(const char *condition,int line) {
    fprintf(stderr,"FAIL browser-worker stage %d line %d: %s\n",(int)stage,line,condition);
    if (reply && reply->error) fprintf(stderr,"Worker error: %s\n",reply->error);
    reply_free(reply); reply=NULL; page_free(&retained); distinct_free(&retained_distinct); free(retained_rejection); if (worker.started) worker_stop(&worker);
    completed(0,checks,stages); emscripten_cancel_main_loop(); abort();
}
#define CHECK(condition) do { checks++; if (!(condition)) die(#condition,__LINE__); } while (0)
static void enter(Stage next) { stage=next; entered=emscripten_get_now(); }
static void pass(const char *message) { stages++; printf("PASS browser-worker: %s\n",message); }
static void counters(void) { CHECK(worker.sql_executed==expected_sql); CHECK(worker.counts_executed==expected_counts); }
static void identity(void) {
    char id[24],object[24],revision[24],offset[24];
    snprintf(id,sizeof(id),"%" PRIu64,expected_id); snprintf(object,sizeof(object),"%" PRIu64,object_id);
    snprintf(revision,sizeof(revision),"%" PRIu64,revision_id); snprintf(offset,sizeof(offset),"%" PRIu64,expected_offset);
    CHECK(observed_identity(id,object,revision,offset));
}
static void submit(JobKind kind,const char *sql,uint64_t offset,int64_t total,bool preview,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job));
    job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=kind;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(sql)};
    job->offset=offset; job->known_total=total; job->object_id=object_id; job->revision=revision_id;
    job->is_preview=expected_preview=preview; job->is_table=kind==JOB_PIVOT;
    expected_offset=kind==JOB_COUNT || kind==JOB_VALIDATE_SQL ? 0 : offset;
    expected_sql+=sql_count; if (kind==JOB_COUNT) expected_counts++;
    worker_submit(&worker,job);
}
static void submit_distinct(const char *sql,const char *column,const char *search,int level,
    const char *start,const char *end,uint64_t offset,int64_t total,int filter_order,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job));
    job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=JOB_DISTINCT;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(sql)};
    if (filter_order>=3) {
        /* A secondary numeric predicate follows an own-column filter which
           available-value enumeration must exclude, retaining the amount. */
        if (filter_order==4) CHECK(analysis_filter_set(&job->analysis,"label",FILTER_EQUAL,"A",NULL));
        CHECK(analysis_filter_set(&job->analysis,"amount",FILTER_GREATER,"1.005",NULL));
    } else if (filter_order) {
        const char *first=filter_order==1 ? "category" : "bucket",*second=filter_order==1 ? "bucket" : "category";
        CHECK(analysis_filter_set(&job->analysis,first,FILTER_EQUAL,filter_order==1 ? "v-001" : "keep",NULL));
        CHECK(analysis_filter_set(&job->analysis,second,FILTER_EQUAL,filter_order==1 ? "keep" : "v-001",NULL));
    }
    job->column=dx_strdup(column); job->search=dx_strdup(search); job->date_level=level;
    job->date_start=start ? dx_strdup(start) : NULL; job->date_end=end ? dx_strdup(end) : NULL;
    job->offset=expected_offset=offset; job->known_total=total; job->object_id=object_id; job->revision=revision_id;
    job->is_table=true; expected_preview=false; expected_sql+=sql_count;
    worker_submit(&worker,job);
}
static bool poll_received(bool error,bool sent) {
    CHECK(!reply); reply=worker_poll(&worker); if (!reply) return false;
    CHECK(reply->id==expected_id && reply->kind==expected_kind && reply->object_id==object_id && reply->revision==revision_id && reply->is_preview==expected_preview);
    CHECK((reply->error!=NULL)==error); CHECK(!worker.running); counters(); if (sent) identity(); retained_values(); return true;
}
static bool received(bool error) { return poll_received(error,true); }
static void release_reply(void) { reply_free(reply); reply=NULL; }
static void quiet(void) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply); counters(); }
static void exact_values(const Page *page) {
    static const char *names[]={"wide","u64","amount","huge_min","huge_max","nanosecond","missing","space","empty","literal","approved","null_number"};
    static const char *texts[]={"9007199254740993","18446744073709551615","1234567890123456789012345678.1234567890",
        "-170141183460469231731687303715884105728","170141183460469231731687303715884105727","2026-10-17 12:34:56.123456789",NULL," ","","NULL","true",NULL};
    CHECK(page->column_count==12 && page->row_count==1 && page->offset==0 && page->total==1 && !page->count_pending && !page->release_cell_text);
    for (size_t i=0;i<12;i++) {
        CHECK(!strcmp(page->columns[i].name,names[i]));
        CHECK(texts[i] ? !page->cells[i].is_null && page->cells[i].text && !strcmp(page->cells[i].text,texts[i]) : page->cells[i].is_null && !page->cells[i].text);
    }
    CHECK(page->columns[0].type==DUCKDB_TYPE_BIGINT && page->columns[0].number_type.kind==NUM_I64 && page->columns[0].numeric);
    CHECK(page->columns[1].type==DUCKDB_TYPE_UBIGINT && page->columns[1].number_type.kind==NUM_U64 && !strcmp(page->columns[1].type_name,"UBIGINT"));
    CHECK(page->columns[2].type==DUCKDB_TYPE_DECIMAL && page->columns[2].number_type.kind==NUM_DECIMAL && page->columns[2].number_type.precision==38 && page->columns[2].number_type.scale==10 && !strcmp(page->columns[2].type_name,"DECIMAL(38,10)"));
    CHECK(page->columns[3].type==DUCKDB_TYPE_HUGEINT && page->columns[4].type==DUCKDB_TYPE_HUGEINT && page->columns[3].number_type.kind==NUM_I128 && page->columns[4].number_type.kind==NUM_I128);
    CHECK(page->columns[5].type==DUCKDB_TYPE_TIMESTAMP_NS && !page->columns[5].numeric && !strcmp(page->columns[5].type_name,"TIMESTAMP_NS"));
    CHECK(page->columns[6].type==DUCKDB_TYPE_VARCHAR && !page->columns[6].numeric && page->cells[8].text && !*page->cells[8].text);
    CHECK(page->columns[10].type==DUCKDB_TYPE_BOOLEAN && !page->columns[10].numeric && page->columns[11].type==DUCKDB_TYPE_BIGINT && page->columns[11].number_type.kind==NUM_I64);
}
static void range_values(const Page *page,uint64_t offset,size_t rows,uint64_t total,bool pending) {
    CHECK(page->column_count==3 && page->row_count==rows && page->offset==offset && page->total==total && page->count_pending==pending);
    CHECK(!strcmp(page->columns[0].name,"row_id") && page->columns[0].number_type.kind==NUM_I64 && !strcmp(page->columns[0].type_name,"BIGINT"));
    CHECK(!strcmp(page->columns[1].name,"label") && !page->columns[1].numeric && page->columns[1].type==DUCKDB_TYPE_VARCHAR);
    CHECK(page->columns[2].number_type.kind==NUM_DECIMAL && page->columns[2].number_type.precision==5 && page->columns[2].number_type.scale==2 && !strcmp(page->columns[2].type_name,"DECIMAL(5,2)"));
    for (size_t row=0;row<rows;row++) {
        char id[24],label[32],amount[32]; uint64_t value=offset+row;
        snprintf(id,sizeof(id),"%" PRIu64,value); snprintf(label,sizeof(label),"row-%" PRIu64,value); snprintf(amount,sizeof(amount),"%" PRIu64 ".00",value%100);
        const Cell *cells=&page->cells[row*3];
        CHECK(!cells[0].is_null && !cells[1].is_null && !cells[2].is_null && !strcmp(cells[0].text,id) && !strcmp(cells[1].text,label) && !strcmp(cells[2].text,amount));
    }
    exact_values(&retained);
}
static void validation_reply(void) { CHECK(!reply->page.cells && !reply->page.columns && !reply->page.row_count && !reply->page.column_count); }
static void short_reply(const char *text) { CHECK(reply->page.row_count==1 && reply->page.column_count==1 && !reply->page.cells[0].is_null && !strcmp(reply->page.cells[0].text,text)); }
static void distinct_value(const DistinctValue *value,const char *raw,const char *label,const char *upper) {
    CHECK(value->is_null==(raw==NULL));
    CHECK(raw ? value->value && !strcmp(value->value,raw) : !value->value);
    CHECK(value->label && !strcmp(value->label,label));
    CHECK(upper ? value->upper && !strcmp(value->upper,upper) : !value->upper);
    if (!raw) CHECK(!value->key);
    else if (!upper) CHECK(value->key && !strcmp(value->key,raw));
    else {
        CHECK(value->key && !strncmp(value->key,raw,strlen(raw)) && value->key[strlen(raw)]=='\t' && !strcmp(value->key+strlen(raw)+1,upper));
    }
}
static void category_values(const DistinctPage *page,uint64_t offset,size_t rows,uint64_t total) {
    CHECK(page->offset==offset && page->count==rows && page->total==total);
    CHECK(page->values && page->bytes>=rows*sizeof(*page->values) && page->bytes<=4*1024*1024);
    for (size_t row=0;row<rows;row++) {
        uint64_t index=offset+row; char value[24];
        if (!index) distinct_value(&page->values[row],NULL,"NULL",NULL);
        else if (index==1) distinct_value(&page->values[row],"","",NULL);
        else if (index==2) distinct_value(&page->values[row],"NULL","NULL",NULL);
        else { snprintf(value,sizeof(value),"v-%03" PRIu64,index-3); distinct_value(&page->values[row],value,value,NULL); }
    }
}
static void retained_values(void) {
    if (!retained_distinct.count) return;
    category_values(&retained_distinct,0,128,300); exact_values(&retained);
}
static void distinct_reply(size_t count,uint64_t total) {
    validation_reply(); CHECK(reply->distinct.offset==0 && reply->distinct.count==count && reply->distinct.total==total);
}
static void date_value(size_t row,const char *value,const char *label,const char *upper) {
    distinct_value(&reply->distinct.values[row],value,label,upper);
}
static void decimal_distinct_values(bool secondary) {
    distinct_reply(2,2);
    distinct_value(&reply->distinct.values[0],"B","B",NULL);
    distinct_value(&reply->distinct.values[1],"C","C",NULL);
    const Analysis *analysis=&reply->analysis;
    CHECK(analysis->filter_count==(secondary ? 1u : 0u));
    CHECK(analysis->filter_column && !strcmp(analysis->filter_column,secondary ? "label" : "amount"));
    CHECK(analysis->filter_value && !strcmp(analysis->filter_value,secondary ? "A" : "1.005"));
    if (secondary) CHECK(analysis->filters[0].column && !strcmp(analysis->filters[0].column,"amount") &&
        analysis->filters[0].value && !strcmp(analysis->filters[0].value,"1.005") && analysis->filters[0].op==FILTER_GREATER);
    else CHECK(analysis->filter_op==FILTER_GREATER);
    NumberType number=secondary ? analysis->filters[0].number : analysis->filter_number;
    CHECK(number.kind==NUM_DECIMAL && number.precision==4 && number.scale==2);
}
static void inject_late(void *unused) {
    (void)unused;
    /* Matching old cookie/ticket, deliberately unreadable payload: the ownership
       fence must return before dereferencing callbacks after teardown. */
    dx_browser_query_result(stale_cookie,stale_ticket,(const void *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1,1,(const char *)(uintptr_t)1);
    late_callbacks++;
}
static void tick(void) {
    CHECK(emscripten_get_now()-entered<15000);
    switch (stage) {
    case WAIT_EXACT:
        if (!received(false)) return;
        exact_values(&reply->page); retained=reply->page; reply->page=(Page){0}; release_reply(); pass("exact owned values and uint64 identities");
        submit(JOB_PAGE,range_sql,0,-1,false,2); enter(WAIT_PAGE0); return;
    case WAIT_PAGE0:
        if (!received(false)) return;
        range_values(&reply->page,0,256,256,true); release_reply(); pass("range first page / DESCRIBE then canonical query");
        submit(JOB_PAGE,range_sql,256,-1,false,1); enter(WAIT_PAGE256); return;
    case WAIT_PAGE256:
        if (!received(false)) return;
        range_values(&reply->page,256,256,512,true); release_reply(); pass("range second page / described schema reused");
        submit(JOB_PAGE,range_sql,512,-1,false,1); enter(WAIT_PAGE512); return;
    case WAIT_PAGE512:
        if (!received(false)) return;
        range_values(&reply->page,512,88,600,false); release_reply(); pass("range final page / precise lower bound");
        submit(JOB_COUNT,range_sql,0,-1,false,1); enter(WAIT_COUNT); return;
    case WAIT_COUNT:
        if (!received(false)) return;
        CHECK(reply->counted_rows==600); validation_reply(); release_reply(); pass("real count 600");
        submit(JOB_PIVOT,range_sql,256,600,false,1); enter(WAIT_TABLE); return;
    case WAIT_TABLE:
        if (!received(false)) return;
        range_values(&reply->page,256,256,600,false); release_reply(); pass("table-scoped page and known count / no redundant DESCRIBE");
        submit(JOB_VALIDATE_SQL,range_sql,0,-1,false,1); enter(WAIT_VALIDATE); return;
    case WAIT_VALIDATE:
        if (!received(false)) return;
        validation_reply(); release_reply(); pass("real SQL validation / owned empty reply");
        submit(JOB_STEP_PREVIEW,range_sql,512,600,true,1); enter(WAIT_PREVIEW); return;
    case WAIT_PREVIEW:
        if (!received(false)) return;
        range_values(&reply->page,512,88,600,false); release_reply(); pass("historical preview page and schema reuse");
        submit(JOB_PAGE,"SELECT definitely_missing_column FROM input",0,-1,false,1); enter(WAIT_ERROR); return;
    case WAIT_ERROR:
        if (!received(true)) return;
        CHECK(strlen(reply->error)>16 && strstr(reply->error,"definitely_missing_column")); validation_reply(); release_reply(); pass("useful real engine SQL error");
        submit(JOB_PAGE,range_sql,0,600,false,1); enter(WAIT_RECOVERY); return;
    case WAIT_RECOVERY:
        if (!received(false)) return;
        range_values(&reply->page,0,256,600,false); release_reply(); pass("error recovery retains the valid described schema");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,0,-1,0,2); enter(WAIT_DISTINCT0); return;
    case WAIT_DISTINCT0:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,0,128,300);
        retained_distinct=reply->distinct; reply->distinct=(DistinctPage){0}; release_reply(); pass("300 distinct values / SQL NULL, empty and literal NULL remain separate");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,128,300,0,1); enter(WAIT_DISTINCT128); return;
    case WAIT_DISTINCT128:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,128,128,300); release_reply(); pass("distinct second page / known total needs one SQL and no DESCRIBE");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,256,300,0,1); enter(WAIT_DISTINCT256); return;
    case WAIT_DISTINCT256:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,256,44,300); release_reply(); pass("distinct final page / retained first page stays owned");
        submit_distinct(categories_sql,"category","NULL",-1,NULL,NULL,0,-1,0,2); enter(WAIT_DISTINCT_SEARCH); return;
    case WAIT_DISTINCT_SEARCH:
        if (!received(false)) return;
        distinct_reply(2,2); distinct_value(&reply->distinct.values[0],NULL,"NULL",NULL);
        distinct_value(&reply->distinct.values[1],"NULL","NULL",NULL); release_reply(); pass("distinct NULL search includes typed missing and literal NULL without empty text");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,0,-1,1,3); enter(WAIT_DISTINCT_PRIMARY); return;
    case WAIT_DISTINCT_PRIMARY:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,0,128,150);
        CHECK(reply->analysis.filter_count==1 && !strcmp(reply->analysis.filter_column,"category") && !strcmp(reply->analysis.filter_value,"v-001"));
        CHECK(!strcmp(reply->analysis.filters[0].column,"bucket") && !strcmp(reply->analysis.filters[0].value,"keep"));
        release_reply(); pass("available values ignore own primary filter and retain the secondary source filter");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,0,-1,2,2); enter(WAIT_DISTINCT_SECONDARY); return;
    case WAIT_DISTINCT_SECONDARY:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,0,128,150);
        CHECK(reply->analysis.filter_count==1 && !strcmp(reply->analysis.filter_column,"bucket") && !strcmp(reply->analysis.filter_value,"keep"));
        CHECK(!strcmp(reply->analysis.filters[0].column,"category") && !strcmp(reply->analysis.filters[0].value,"v-001"));
        release_reply(); pass("available values ignore own secondary filter and retain the primary source filter");
        submit_distinct(decimal_filter_sql,"label","",-1,NULL,NULL,0,-1,3,3); enter(WAIT_DISTINCT_DECIMAL_PRIMARY); return;
    case WAIT_DISTINCT_DECIMAL_PRIMARY:
        if (!received(false)) return;
        decimal_distinct_values(false); release_reply(); pass("distinct primary DECIMAL comparison preserves 1.01 > 1.005 without rounding the filter");
        submit_distinct(decimal_filter_sql,"label","",-1,NULL,NULL,0,-1,4,2); enter(WAIT_DISTINCT_DECIMAL_SECONDARY); return;
    case WAIT_DISTINCT_DECIMAL_SECONDARY:
        if (!received(false)) return;
        decimal_distinct_values(true); release_reply(); pass("distinct secondary DECIMAL comparison retains B and C after removing the own-column filter");
        submit_distinct(dates_sql,"occurred","",0,NULL,NULL,0,-1,0,2); enter(WAIT_DATE_YEAR); return;
    case WAIT_DATE_YEAR:
        if (!received(false)) return;
        distinct_reply(4,4); date_value(0,NULL,"NULL",NULL);
        /* Constant year/month/day truncation binds to DATE; DATE + INTERVAL
           promotes the upper endpoint back to TIMESTAMP. Keep both exact. */
        date_value(1,"2024-01-01","2024","2025-01-01 00:00:00");
        date_value(2,"2025-01-01","2025","2026-01-01 00:00:00");
        date_value(3,"2026-01-01","2026","2027-01-01 00:00:00");
        release_reply(); pass("date years keep NULL plus complete canonical lower/upper keys");
        submit_distinct(dates_sql,"occurred","",1,"2025-01-01 00:00:00","2026-01-01 00:00:00",0,-1,0,2); enter(WAIT_DATE_MONTH); return;
    case WAIT_DATE_MONTH:
        if (!received(false)) return;
        distinct_reply(2,2);
        date_value(0,"2025-01-01","January","2025-02-01 00:00:00");
        date_value(1,"2025-02-01","February","2025-03-01 00:00:00");
        release_reply(); pass("date months include the parent's lower bound and exclude its next-year endpoint");
        submit_distinct(dates_sql,"occurred","",2,"2025-01-01 00:00:00","2025-02-01 00:00:00",0,-1,0,2); enter(WAIT_DATE_DAY); return;
    case WAIT_DATE_DAY:
        if (!received(false)) return;
        distinct_reply(2,2);
        date_value(0,"2025-01-01","01","2025-01-02 00:00:00");
        date_value(1,"2025-01-02","02","2025-01-03 00:00:00");
        release_reply(); pass("date days exclude the next-month endpoint");
        submit_distinct(dates_sql,"occurred","",3,"2025-01-01 00:00:00","2025-01-02 00:00:00",0,-1,0,2); enter(WAIT_DATE_HOUR); return;
    case WAIT_DATE_HOUR:
        if (!received(false)) return;
        distinct_reply(2,2);
        date_value(0,"2025-01-01 00:00:00","00:00","2025-01-01 01:00:00");
        date_value(1,"2025-01-01 01:00:00","01:00","2025-01-01 02:00:00");
        release_reply(); pass("date hours exclude the next-day endpoint");
        submit_distinct(dates_sql,"occurred","",4,"2025-01-01 00:00:00","2025-01-01 01:00:00",0,-1,0,2); enter(WAIT_DATE_MINUTE); return;
    case WAIT_DATE_MINUTE:
        if (!received(false)) return;
        distinct_reply(2,2);
        date_value(0,"2025-01-01 00:00:00","00:00","2025-01-01 00:01:00");
        date_value(1,"2025-01-01 00:01:00","00:01","2025-01-01 00:02:00");
        release_reply(); pass("date minutes group fractional seconds and exclude the next-hour endpoint");
        submit_distinct(dates_sql,"occurred","NULL",4,NULL,NULL,0,-1,0,2); enter(WAIT_DATE_NULL); return;
    case WAIT_DATE_NULL:
        if (!received(false)) return;
        distinct_reply(1,1); date_value(0,NULL,"NULL",NULL); release_reply(); pass("date hierarchy NULL search retains an actual typed NULL key");
        submit_distinct(categories_sql,"definitely_missing_distinct_column","",-1,NULL,NULL,0,-1,0,1); enter(WAIT_DISTINCT_ERROR); return;
    case WAIT_DISTINCT_ERROR:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"definitely_missing_distinct_column")); validation_reply();
        CHECK(!reply->distinct.values && !reply->distinct.count); release_reply(); pass("distinct engine error owns no partial page");
        submit_distinct(categories_sql,"category","",-1,NULL,NULL,256,300,0,1); enter(WAIT_DISTINCT_RECOVERY); return;
    case WAIT_DISTINCT_RECOVERY:
        if (!received(false)) return;
        validation_reply(); category_values(&reply->distinct,256,44,300); release_reply(); pass("distinct error recovery preserves prior result pages and uses the known total");
        submit(JOB_VALIDATE_SQL,long_sql,0,-1,false,1); enter(WAIT_OVERSIZE_SCHEMA); return;
    case WAIT_OVERSIZE_SCHEMA:
        if (!received(false)) return;
        validation_reply(); release_reply(); submit(JOB_PAGE,long_sql,0,1,false,1); enter(RUN_OVERSIZE); return;
    case RUN_OVERSIZE: {
        if (emscripten_get_now()-entered<80) return;
        quiet(); CHECK(worker.running); identity();
        stale_cookie=observed_cookie(); stale_ticket=observed_ticket(); rejection_cancels=observed_cancels(); CHECK(rejection_cancels<UINT32_MAX);
        /* SQL escaping doubles each apostrophe: the input search fits its own
           budget but the generated statement exceeds the 1 MiB SQL limit. */
        size_t length=600*1024; char *search=dx_alloc(length+1); memset(search,'\'',length); search[length]=0;
        submit_distinct(categories_sql,"category",search,-1,NULL,NULL,0,0,0,0); free(search);
        enter(WAIT_OVERSIZE_ERROR); return;
    }
    case WAIT_OVERSIZE_ERROR:
        /* Local rejection sends no SQL: preserve the old host observation,
           while checking the newly owned C reply's full request identity. */
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"1 MiB")); validation_reply();
        CHECK(!reply->distinct.values && !reply->distinct.count && !reply->distinct.bytes && !reply->distinct.total);
        CHECK(observed_cookie()==stale_cookie && observed_ticket()==stale_ticket);
        CHECK(observed_cancels()==rejection_cancels+1);
        retained_rejection=reply->error; reply->error=NULL; release_reply(); pass("oversized local SQL rejection cancels the active engine request exactly once");
        submit(JOB_PAGE,rejection_recovery_sql,0,1,false,2); enter(WAIT_OVERSIZE_RECOVERY); return;
    case WAIT_OVERSIZE_RECOVERY:
        if (!received(false)) return;
        short_reply("recovered after local rejection"); CHECK(retained_rejection && strstr(retained_rejection,"1 MiB"));
        free(retained_rejection); retained_rejection=NULL; release_reply(); pass("recovery after local rejection preserves the owned error and prior result pages");
        submit(JOB_VALIDATE_SQL,long_sql,0,-1,false,1); enter(WAIT_SUPERSEDE_SCHEMA); return;
    case WAIT_SUPERSEDE_SCHEMA:
        if (!received(false)) return;
        validation_reply(); release_reply(); submit(JOB_PAGE,long_sql,0,1,false,1); enter(RUN_SUPERSEDE); return;
    case RUN_SUPERSEDE:
        if (emscripten_get_now()-entered<80) return;
        quiet(); CHECK(worker.running); identity(); submit(JOB_PAGE,latest_sql,0,1,false,2); enter(WAIT_LATEST); return;
    case WAIT_LATEST:
        if (!received(false)) return;
        short_reply("latest wins"); release_reply(); pass("rapid long/short latest-wins / only current reply"); enter(QUIET_LATEST); return;
    case QUIET_LATEST:
        quiet(); if (emscripten_get_now()-entered<150) return;
        CHECK(!worker.running); exact_values(&retained); submit(JOB_VALIDATE_SQL,long_sql,0,-1,false,1); enter(WAIT_CANCEL_SCHEMA); return;
    case WAIT_CANCEL_SCHEMA:
        if (!received(false)) return;
        validation_reply(); release_reply(); submit(JOB_PAGE,long_sql,0,1,false,1); enter(RUN_CANCEL); return;
    case RUN_CANCEL:
        if (emscripten_get_now()-entered<80) return;
        quiet(); CHECK(worker.running); identity();
        stale_cookie=observed_cookie(); stale_ticket=observed_ticket();
        dx_browser_query_result(stale_cookie+1,stale_ticket,NULL,0,"0",0,"wrong cookie");
        dx_browser_query_result(stale_cookie,stale_ticket+1,NULL,0,"0",0,"wrong ticket");
        CHECK(worker.running); quiet(); worker_cancel(&worker); CHECK(!worker.running && worker.started); enter(QUIET_CANCEL); return;
    case QUIET_CANCEL:
        quiet(); if (emscripten_get_now()-entered<100) return;
        submit(JOB_PAGE,cancel_recovery_sql,0,1,false,2); enter(WAIT_CANCEL_RECOVERY); return;
    case WAIT_CANCEL_RECOVERY:
        if (!received(false)) return;
        short_reply("recovered after cancel"); release_reply(); pass("explicit cancel, cookie/ticket fences and recovery");
        submit(JOB_VALIDATE_SQL,long_sql,0,-1,false,1); enter(WAIT_STOP_SCHEMA); return;
    case WAIT_STOP_SCHEMA:
        if (!received(false)) return;
        validation_reply(); release_reply(); submit(JOB_PAGE,long_sql,0,1,false,1); enter(RUN_STOP); return;
    case RUN_STOP:
        if (emscripten_get_now()-entered<80) return;
        quiet(); CHECK(worker.running); identity(); stale_cookie=observed_cookie(); stale_ticket=observed_ticket();
        worker_stop(&worker); CHECK(!worker.started && !worker.running && !worker.reply && !worker.pending);
        inject_late(NULL); emscripten_async_call(inject_late,NULL,50); enter(QUIET_STOP); return;
    case QUIET_STOP:
        quiet(); if (emscripten_get_now()-entered<150) return;
        CHECK(late_callbacks==2 && !worker.started && !worker.running && !worker.reply); worker_stop(&worker);
        retained_values(); exact_values(&retained); distinct_free(&retained_distinct); distinct_free(&retained_distinct);
        page_free(&retained); page_free(&retained); pass("early stop and scheduled stale callbacks are ignored");
        CHECK(stages==31 && worker.jobs_started==38 && expected_sql==56 && expected_counts==1);
        enter(FINISHED); completed(1,checks,stages); printf("PASS browser-worker complete: %u checks, %u stages, %" PRIu64 " SQL, %" PRIu64 " counts\n",checks,stages,worker.sql_executed,worker.counts_executed);
        emscripten_cancel_main_loop(); return;
    case FINISHED: die("main loop ran after completion",__LINE__);
    }
}
int main(void) {
    CHECK(observe_host()); CHECK(worker_start(&worker)); submit(JOB_PAGE,exact_sql,0,-1,false,2); enter(WAIT_EXACT);
    emscripten_set_main_loop(tick,0,1); return 0;
}
