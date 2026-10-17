/* Headless acceptance against the real official browser Worker. The runner
   supplies Module.dxEngine=createEngineHost(...), exports the worker callback,
   and polls Module.dxBrowserWorkerTestDone. No renderer/window is created. */
#include "data/worker.h"
#include <emscripten/emscripten.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DUXCEL_BROWSER_HOST
#error This driver requires the real DUXCEL_BROWSER_HOST worker backend.
#endif

extern void dx_browser_query_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,const char *total,int pending,const char *failure);
extern int dx_browser_export_result(unsigned cookie,unsigned ticket,const void *wire,size_t size,unsigned char *owned_data,size_t data_size,const char *failure);
EM_JS(int, observe_query_host, (void), {
    const host=Module['dxEngine'];
    if(!host || typeof host.submit!=='function' || typeof host.cancel!=='function')return 0;
    const submit=host.submit.bind(host);
    host.submit=(runtime,cookie,ticket,request)=>{
        Module['dxBrowserWorkerObserved']={cookie,ticket,generation:request.generation,object:request.objectId,revision:request.revision,offset:request.offset};
        const hold=Module['dxHoldMatrixReply'];
        if((hold===1 && request.sql.includes('__duxcel_count')) || (hold===2 && request.sql.includes('_duxcel_matrix_totals')) || (hold===3 && request.sql.includes('_duxcel_domain_input')) || (hold===4 && request.sql.includes('_duxcel_matrix_rows')) || (hold===5 && request.sql.includes('_duxcel_matrix_columns')) || (hold===6 && request.sql.includes(' AS distinct_values')) || (hold===7 && request.purpose==='copy' && request.sql.includes(' ON '))){
            Module['dxHoldMatrixReply']=0;
            const original=runtime;
            runtime={...original,reply:(...args)=>{
                const [replyCookie,replyTicket,pointer,size,total,pending,error]=args;
                const data=pointer?original.heap().slice(pointer,pointer+size):null;
                const totalText=total?UTF8ToString(total):null,errorText=error?UTF8ToString(error):null;
                Module['dxHeldMatrixReply']=()=>{
                    const owned=[];
                    const bytes=value=>{const pointer=original.allocate(value.byteLength||1);if(!pointer)throw new Error('Test reply allocation failed');owned.push(pointer);original.heap().set(value,pointer);return pointer;};
                    const string=value=>value===null?0:bytes(new TextEncoder().encode(value+'\0'));
                    try{original.reply(replyCookie,replyTicket,data?bytes(data):0,size,string(totalText),pending,string(errorText));}
                    finally{for(const pointer of owned)original.release(pointer);Module['dxHeldMatrixReply']=null;}
                };
            }};
        }
        return submit(runtime,cookie,ticket,request);
    };

    return 1;
})
EM_JS(int, observe_export_host, (void), {
    const host=Module['dxEngine'];
    if(typeof host.exportParquet==='function'){
        const exporting=host.exportParquet.bind(host);
        host.exportParquet=(runtime,cookie,ticket,request)=>{
            Module['dxBrowserWorkerObserved']={cookie,ticket,generation:request.generation,object:request.objectId,revision:request.revision,offset:request.offset};
            const fault=Module['dxExportFault']||0,hold=Module['dxHoldExportReply'];Module['dxExportFault']=0;Module['dxHoldExportReply']=0;
            if(fault||hold){const original=runtime;runtime={...original,reply:(cookie,ticket,wire,size,data,bytes,error)=>{
                if(error||!wire||!data)return original.reply(cookie,ticket,wire,size,data,bytes,error);
                if(hold){
                    const count=wire?original.heap().slice(wire,wire+size):null,binary=data?original.heap().slice(data,data+bytes):null;
                    const failure=error?UTF8ToString(error):null;Module['dxHeldExportBytes']=binary?.length||0;
                    Module['dxHeldExportReply']=()=>{
                        const owned=[];let binaryPointer=0;
                        const allocate=value=>{const pointer=original.allocate(value.length||1);if(!pointer)throw new Error('Held export test malloc failed');owned.push(pointer);original.heap().set(value,pointer);return pointer;};
                        try{const countPointer=count?allocate(count):0;binaryPointer=binary?allocate(binary):0;
                            const accepted=original.reply(cookie,ticket,countPointer,size,binaryPointer,bytes,failure?allocate(new TextEncoder().encode(failure+'\0')):0);
                            if(accepted===1)owned.splice(owned.indexOf(binaryPointer),1);return accepted;
                        }finally{for(const pointer of owned)original.release(pointer);Module['dxHeldExportReply']=null;Module['dxHeldExportBytes']=0;}
                    };return 0;
                }
                // Deliberate transport faults after a real C-built COPY. This
                // is a DXP1 fixture, not replacement analytical computation.
                const countWire=(text,isNull=false,type='BIGINT',name='Count')=>{
                    const enc=new TextEncoder(),parts=[enc.encode(name),enc.encode(type),enc.encode(type==='VARCHAR'?'none':'i64')],value=enc.encode(text);
                    const out=new Uint8Array(20+20+parts.reduce((n,p)=>n+p.length,0)+8+1+value.length),view=new DataView(out.buffer);let at=0;
                    const u32=n=>{view.setUint32(at,n,true);at+=4;};for(const n of [0x31505844,1,1,1,value.length,...parts.map(p=>p.length),0,0])u32(n);
                    for(const part of parts){out.set(part,at);at+=part.length;}u32(0);u32(value.length);out[at++]=isNull?1:0;out.set(value,at);return out;
                };
                const owned=[];const allocate=value=>{const pointer=original.allocate(value.length||1);if(!pointer)throw new Error('Fault fixture malloc failed');owned.push(pointer);original.heap().set(value,pointer);return pointer;};
                try{
                    if(fault===1)data=0;
                    else if(fault===2){data=1;bytes=64*1024*1024+1;}
                    else if(fault===3)original.heap()[data]=0;
                    else if(fault===4)original.heap().fill(255,data+bytes-8,data+bytes-4);
                    else if(fault>=5&&fault<=9){const count=fault===5?countWire("",true):fault===6?countWire('-1'):fault===7?countWire('18446744073709551616'):fault===8?countWire('0600'):countWire('600',false,'VARCHAR');wire=allocate(count);size=count.length;}
                    else if(fault===10)original.heap()[wire]=0;
                    else if(fault===11)size--;
                    else if(fault===12){const count=countWire('600',false,'BIGINT','Other');wire=allocate(count);size=count.length;}
                    else if(fault===13)error=allocate(new TextEncoder().encode('Injected export transport failure\0'));
                    return original.reply(cookie,ticket,wire,size,data,bytes,error);
                }finally{for(const pointer of owned)original.release(pointer);}
            }};}
            return exporting(runtime,cookie,ticket,request);
        };
    }
    return typeof host.exportParquet==='function'?1:0;
})
static int observe_host(void) { return observe_query_host() && observe_export_host(); }
EM_JS(int, reset_test_engine, (void), {
    if(typeof Module['dxResetTestEngine']!=='function')return 0;Module['dxExportResetReady']=0;
    Promise.resolve().then(()=>Module['dxResetTestEngine']()).then(()=>Module['dxExportResetReady']=1,error=>{Module['dxExportResetReady']=-1;console.error(error);});return 1;
})
EM_JS(int, export_reset_ready, (void), { return Module['dxExportResetReady']||0; })
EM_JS(void, export_fault, (int kind), { Module['dxExportFault']=kind; })
EM_JS(void, hold_export_reply, (void), { Module['dxHoldExportReply']=1; })
EM_JS(unsigned, held_export_bytes, (void), { return Module['dxHeldExportBytes']||0; })
EM_JS(int, release_export_reply, (void), { return Module['dxHeldExportReply'](); })
EM_JS(void, register_export_bytes, (const char *name,const unsigned char *data,size_t size), {
    const bytes=HEAPU8.slice(data,data+size),file=UTF8ToString(name);Module['dxExportRegisterReady']=0;
    Promise.resolve().then(()=>Module['dxTestAdapter'].registerFile(file,bytes)).then(()=>Module['dxExportRegisterReady']=1,error=>{Module['dxExportRegisterReady']=-1;console.error(error);});
})
EM_JS(void, register_export_source, (const char *text), {
    const bytes=new TextEncoder().encode(UTF8ToString(text));Module['dxExportSourceOriginal']=bytes.slice();Module['dxExportRegisterReady']=0;
    Promise.resolve().then(()=>Module['dxTestAdapter'].registerFile('worker-export-source.csv',bytes)).then(()=>Module['dxExportRegisterReady']=1,error=>{Module['dxExportRegisterReady']=-1;console.error(error);});
})
EM_JS(int, export_registration_ready, (void), { return Module['dxExportRegisterReady']||0; })
EM_JS(void, verify_export_source, (void), {
    Module['dxExportSourceVerified']=0;
    Promise.resolve().then(()=>Module['dxTestAdapter'].database.copyFileToBuffer('worker-export-source.csv')).then(bytes=>{
        const original=Module['dxExportSourceOriginal'];Module['dxExportSourceVerified']=bytes.length===original.length&&bytes.every((value,index)=>value===original[index])?1:-1;
    },error=>{Module['dxExportSourceVerified']=-1;console.error(error);});
})
EM_JS(int, export_source_verified, (void), { return Module['dxExportSourceVerified']||0; })
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
EM_JS(void, hold_matrix_reply, (int kind), { Module['dxHoldMatrixReply']=kind; })
EM_JS(int, matrix_reply_held, (void), { return typeof Module['dxHeldMatrixReply']==='function'; })
EM_JS(void, release_matrix_reply, (void), { Module['dxHeldMatrixReply'](); })
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
static const char matrix_exact_sql[]=
    "SELECT * FROM (VALUES (NULL::VARCHAR,9007199254740993::BIGINT,10.00::DECIMAL(38,2),1.01::DECIMAL(4,2)),"
    "(NULL,9007199254740994,20.00,1.01),('',9007199254740993,30.00,1.01),"
    "('NULL',NULL,NULL,1.01),('excluded',17,999.00,1.00)) t(category,wide,amount,gate)";
static const char matrix_large_sql[]="SELECT i::BIGINT AS row_id,repeat('x',900000)::VARCHAR AS payload FROM range(4) t(i)";
static const char columns_exact_sql[]=
    "SELECT * FROM (VALUES (NULL::VARCHAR,NULL::VARCHAR,10::DECIMAL(38,2),9007199254740993::BIGINT),"
    "('A','x',1,9007199254740993),('A','x',3,9007199254740994),('A','y',9,9007199254740993),('A','y',NULL,NULL),"
    "('B','x',2,9007199254740993),('B','y',2,9007199254740993),('B','z',NULL,NULL),"
    "('','',100,9007199254740993),('NULL','NULL',200,9007199254740995),"
    "('A','quote_\"''_SUM',5,9007199254740994),('B','FOO',7,9007199254740993),('B','foo',11,9007199254740993)) t(r,c,v,id)";
static const char columns_range_sql[]="SELECT i::BIGINT AS row_id,(i%3)::INTEGER AS category,(i%100)::DECIMAL(5,2) AS amount FROM range(600) t(i)";
static const char columns_bound_sql[]="SELECT i::BIGINT AS Category,i::DECIMAL(5,2) AS amount FROM range(128) t(i)";
static const char columns_wide_sql[]="SELECT * FROM (VALUES (2::DECIMAL(38,2),9007199254740993::BIGINT),(10,9007199254740994),(9007199254740993,9007199254740993),(9007199254740994,9007199254740994)) t(Category,amount)";
static const char columns_product_sql[]="SELECT i::BIGINT AS row_id,(i%16)::BIGINT AS c1,(i%8)::BIGINT AS c2,i::BIGINT AS amount FROM range(128) t(i)";
static const char columns_large_sql[]="SELECT i::BIGINT AS row_id,'A' AS category,repeat('x',900000) AS payload FROM range(4) t(i)";
static const char columns_sql_limit[]="SELECT repeat('''',600000) AS category,1::BIGINT AS amount";
static const char statistics_exact_sql[]="SELECT big::HUGEINT AS big,tiny::DECIMAL(38,38) AS tiny,label FROM (VALUES ('170141183460469231731687303715884105727','0.00000000000000000000000000000000000001','NULL'),('170141183460469231731687303715884105727','-0.00000000000000000000000000000000000002',''),(NULL,NULL,NULL)) t(big,tiny,label)";
static const char statistics_text_sql[]="SELECT * FROM (VALUES (NULL::VARCHAR),(''),('NULL'),(' '),('NULL')) t(label)";
static const char statistics_filter_sql[]="SELECT * FROM (VALUES (1.00::DECIMAL(4,2),'A'),(1.01::DECIMAL(4,2),'B'),(1.02::DECIMAL(4,2),'C')) t(amount,label) WHERE true";
static const char statistics_overflow_sql[]="SELECT x::DOUBLE AS value FROM (VALUES ('1e308'),('1e308')) t(x)";
static const char statistics_nonfinite_sql[]="SELECT x::DOUBLE AS value FROM (VALUES ('nan'),('inf'),('-inf'),(NULL)) t(x)";
static const char statistics_zero_sql[]="SELECT * FROM (VALUES (-0.0::DOUBLE),(0.0::DOUBLE)) t(value)";
static const uint64_t object_id=UINT64_C(18014398509481987),revision_id=UINT64_C(36028797018963971);
typedef enum {
    WAIT_EXACT,WAIT_PAGE0,WAIT_PAGE256,WAIT_PAGE512,WAIT_COUNT,WAIT_TABLE,
    WAIT_VALIDATE,WAIT_PREVIEW,WAIT_ERROR,WAIT_RECOVERY,
    WAIT_DISTINCT0,WAIT_DISTINCT128,WAIT_DISTINCT256,WAIT_DISTINCT_SEARCH,
    WAIT_DISTINCT_PRIMARY,WAIT_DISTINCT_SECONDARY,WAIT_DISTINCT_DECIMAL_PRIMARY,
    WAIT_DISTINCT_DECIMAL_SECONDARY,WAIT_DECIMAL_PAGE,WAIT_DECIMAL_COUNT,WAIT_DATE_YEAR,WAIT_DATE_MONTH,
    WAIT_DATE_DAY,WAIT_DATE_HOUR,WAIT_DATE_MINUTE,WAIT_DATE_NULL,
    WAIT_DISTINCT_ERROR,WAIT_DISTINCT_RECOVERY,WAIT_MATRIX0,WAIT_MATRIX256,WAIT_MATRIX512,WAIT_MATRIX_EXACT,
    WAIT_MATRIX_GRAND,WAIT_MATRIX_EMPTY,WAIT_MATRIX_HUGE,WAIT_MATRIX_DATE,WAIT_MATRIX_NO_TOTALS,WAIT_MATRIX_COLUMNS,WAIT_MATRIX_INVALID,WAIT_MATRIX_LARGE,
    WAIT_MATRIX_RECOVERY,WAIT_MATRIX_CANCEL_COUNT,WAIT_MATRIX_CANCEL_GRAND,WAIT_MATRIX_FINAL,
    WAIT_COLUMNS_EXACT,WAIT_COLUMNS_GRAND,WAIT_COLUMNS_EMPTY,WAIT_COLUMNS_DATE,WAIT_COLUMNS0,WAIT_COLUMNS256,WAIT_COLUMNS512,WAIT_COLUMNS_NO_TOTALS,
    WAIT_COLUMNS_WIDE,WAIT_COLUMNS_BOUND,WAIT_COLUMNS_PRODUCT,WAIT_COLUMNS_SQL_LIMIT,WAIT_COLUMNS_LARGE,WAIT_COLUMNS_CANCEL_DOMAIN,WAIT_COLUMNS_CANCEL_ROWS,WAIT_COLUMNS_CANCEL_FOOTER,WAIT_COLUMNS_FINAL,
    WAIT_MATRIX_COUNT,WAIT_MATRIX_COUNT_EMPTY,
    WAIT_STATS_RANGE,WAIT_STATS_OFFSET,WAIT_STATS_EXACT,WAIT_STATS_TINY,WAIT_STATS_TEXT,WAIT_STATS_FILTERED,WAIT_STATS_OVERFLOW,WAIT_STATS_NONFINITE,WAIT_STATS_ZERO,WAIT_STATS_EMPTY,
    WAIT_STATS_OBJECT,WAIT_STATS_PIVOT,WAIT_STATS_MIXED,WAIT_STATS_STAMP,WAIT_STATS_BOUNDS,WAIT_STATS_COLUMN,WAIT_STATS_CANCEL,WAIT_STATS_FINAL,
    WAIT_STATS_TABLE,WAIT_STATS_ROWS,WAIT_STATS_ROWS_AVG,WAIT_STATS_COLUMNS_AVG,WAIT_STATS_COLUMNS_SUBSET,WAIT_STATS_COLUMNS_DATE,WAIT_STATS_GRAND,WAIT_STATS_TABLE_BAD,WAIT_STATS_ANALYSIS_CANCEL,WAIT_STATS_ANALYSIS_FINAL,
    WAIT_MIX_BLANK,WAIT_MIX_MANUAL,WAIT_MIX_TEXT,WAIT_MIX_EXACT,WAIT_MIX_OBJECTS,WAIT_MIX_DATE,WAIT_MIX_INVALID,WAIT_MIX_STAMP,WAIT_MIX_PRIMARY_BAD,WAIT_MIX_SOURCE_BAD,WAIT_MIX_BUDGET,WAIT_MIX_SQL,WAIT_MIX_CUMULATIVE,WAIT_MIX_ENGINE,WAIT_MIX_CANCEL_DOMAIN,WAIT_MIX_CANCEL_SCALAR,WAIT_MIX_FINAL,
    WAIT_COPY_CASES,WAIT_COPY_CANCEL,WAIT_COPY_STALE,WAIT_COPY_RECOVERY,
    WAIT_OVERSIZE_SCHEMA,RUN_OVERSIZE,
    WAIT_OVERSIZE_ERROR,WAIT_OVERSIZE_RECOVERY,WAIT_SUPERSEDE_SCHEMA,
    RUN_SUPERSEDE,WAIT_LATEST,QUIET_LATEST,WAIT_CANCEL_SCHEMA,RUN_CANCEL,
    QUIET_CANCEL,WAIT_CANCEL_RECOVERY,WAIT_STOP_SCHEMA,RUN_STOP,QUIET_STOP,
    WAIT_EXPORT_RESET,WAIT_EXPORT_CASES,WAIT_EXPORT_REGISTER,WAIT_EXPORT_READBACK,WAIT_EXPORT_LAST,WAIT_EXPORT_SOURCE_REGISTER,
    WAIT_EXPORT_CANCEL,WAIT_EXPORT_STALE,WAIT_EXPORT_RECOVERY,WAIT_EXPORT_SOURCE_VERIFY,QUIET_EXPORT_STOP,FINISHED
} Stage;
static QueryWorker worker;
static QueryReply *reply;
static Page retained;
static DistinctPage retained_distinct;
static SelectionStats retained_statistics;
static char *retained_rejection;
static char *retained_clipboard;
static unsigned char *retained_export;
static size_t retained_export_size;
static uint64_t baseline_jobs,baseline_sql,baseline_counts,export_object,export_revision;
static unsigned export_late_callbacks;
static Stage stage;
static double entered;
static uint64_t next_id=UINT64_C(9007199254740993),expected_id,expected_offset,expected_sql,expected_counts;
static uint64_t statistics_object,statistics_revision;
static JobKind expected_kind;
static bool expected_preview;
static unsigned checks,stages,stale_cookie,stale_ticket,late_callbacks,rejection_cancels;
static void retained_values(void);

static void die(const char *condition,int line) {
    fprintf(stderr,"FAIL browser-worker stage %d line %d: %s\n",(int)stage,line,condition);
    if (reply && reply->error) fprintf(stderr,"Worker error: %s\n",reply->error);
    reply_free(reply); reply=NULL; page_free(&retained); distinct_free(&retained_distinct); statistics_free(&retained_statistics); free(retained_rejection); free(retained_clipboard); free(retained_export); if (worker.started) worker_stop(&worker);
    completed(0,checks,stages); emscripten_cancel_main_loop(); abort();
}
#define CHECK(condition) do { checks++; if (!(condition)) die(#condition,__LINE__); } while (0)
static void enter(Stage next) { stage=next; entered=emscripten_get_now(); }
static void pass(const char *message) { stages++; printf("PASS browser-worker: %s\n",message); }
static void counters(void) { CHECK(worker.sql_executed==expected_sql); CHECK(worker.counts_executed==expected_counts); }
static void identity(void) {
    char id[24],object[24],revision[24],offset[24];
    snprintf(id,sizeof(id),"%" PRIu64,expected_id); snprintf(object,sizeof(object),"%" PRIu64,expected_kind==JOB_EXPORT ? export_object : expected_kind==JOB_STATISTICS ? statistics_object : expected_kind==JOB_RANGE_COPY ? 0 : object_id);
    snprintf(revision,sizeof(revision),"%" PRIu64,expected_kind==JOB_EXPORT ? export_revision : expected_kind==JOB_STATISTICS ? statistics_revision : expected_kind==JOB_RANGE_COPY ? 0 : revision_id); snprintf(offset,sizeof(offset),"%" PRIu64,expected_offset);
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
static void submit_decimal(JobKind kind,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job));
    job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=kind;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(decimal_filter_sql),.sort_column=dx_strdup("label")};
    CHECK(analysis_filter_set(&job->analysis,"amount",FILTER_GREATER,"1.005",NULL));
    CHECK(job->analysis.filter_number.kind==NUM_NONE); /* The backend must resolve this from the source. */
    job->known_total=-1; job->object_id=object_id; job->revision=revision_id;
    expected_offset=0; expected_preview=false; expected_sql+=sql_count; if (kind==JOB_COUNT) expected_counts++;
    worker_submit(&worker,job);
}
typedef enum { MATRIX_RANGE,MATRIX_EXACT,MATRIX_GRAND,MATRIX_EMPTY,MATRIX_HUGE,MATRIX_DATE,MATRIX_COLUMNS,MATRIX_INVALID,MATRIX_LARGE,MATRIX_COUNT,MATRIX_COUNT_EMPTY } MatrixCase;
static void submit_matrix(MatrixCase kind,uint64_t offset,int64_t total,bool totals,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job));
    job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=JOB_PIVOT; expected_preview=false;
    bool exact=kind==MATRIX_EXACT || kind==MATRIX_GRAND || kind==MATRIX_EMPTY || kind==MATRIX_HUGE || kind==MATRIX_COUNT || kind==MATRIX_COUNT_EMPTY;
    const char *source=exact ? matrix_exact_sql : kind==MATRIX_DATE ? dates_sql : kind==MATRIX_LARGE ? matrix_large_sql : range_sql;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(source)}; job->offset=offset; job->known_total=total; job->include_totals=totals;
    job->object_id=object_id; job->revision=revision_id; expected_offset=totals || total<0 ? 0 : offset;
    if (exact) {
        if (kind==MATRIX_EXACT) CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"category",AGG_SUM,NULL));
        if (kind==MATRIX_HUGE) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"wide",AGG_SUM,NULL));
        else if (kind==MATRIX_COUNT || kind==MATRIX_COUNT_EMPTY) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"wide",AGG_COUNT,NULL));
        else {
            CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_AVG,NULL));
            CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"wide",AGG_MAX,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"wide",AGG_DISTINCT,NULL));
        }
        CHECK(pivot_assign(&job->pivot,PIVOT_FILTERS,"gate",AGG_SUM,NULL)); job->pivot.fields[PIVOT_FILTERS][0].filter=FILTER_GREATER; job->pivot.fields[PIVOT_FILTERS][0].value=dx_strdup(kind==MATRIX_EMPTY || kind==MATRIX_COUNT_EMPTY ? "2.00" : "1.005");
    } else if (kind==MATRIX_DATE) {
        CHECK(pivot_assign_grain(&job->pivot,PIVOT_ROWS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"occurred",AGG_COUNT,NULL));
    } else {
        CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,kind==MATRIX_LARGE ? "payload" : "amount",kind==MATRIX_LARGE ? AGG_MIN : AGG_SUM,NULL));
        if (kind==MATRIX_COLUMNS) CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"label",AGG_SUM,NULL));
        if (kind==MATRIX_INVALID) job->pivot.counts[PIVOT_VALUES]=SIZE_MAX;
    }
    expected_sql+=sql_count; if (sql_count && total<0 && kind!=MATRIX_COLUMNS) expected_counts++;
    worker_submit(&worker,job);
}
typedef enum { COLUMNS_EXACT,COLUMNS_GRAND,COLUMNS_EMPTY,COLUMNS_DATE,COLUMNS_RANGE,COLUMNS_WIDE,COLUMNS_BOUND,COLUMNS_PRODUCT,COLUMNS_SQL_LIMIT,COLUMNS_LARGE } ColumnsCase;
static void submit_columns(ColumnsCase kind,uint64_t offset,int64_t total,bool totals,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=JOB_PIVOT; expected_preview=false;
    const char *source=kind<=COLUMNS_EMPTY ? columns_exact_sql : kind==COLUMNS_DATE ? dates_sql : kind==COLUMNS_WIDE ? columns_wide_sql : kind==COLUMNS_BOUND ? columns_bound_sql : kind==COLUMNS_PRODUCT ? columns_product_sql : kind==COLUMNS_SQL_LIMIT ? columns_sql_limit : kind==COLUMNS_LARGE ? columns_large_sql : columns_range_sql;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=kind==COLUMNS_EMPTY ? dx_printf("SELECT * FROM (%s) empty_source WHERE false",source) : dx_strdup(source)};
    job->offset=offset; job->known_total=total; job->include_totals=totals; job->object_id=object_id; job->revision=revision_id; expected_offset=totals || total<0 ? 0 : offset;
    if (kind<=COLUMNS_EMPTY) {
        if (kind!=COLUMNS_GRAND) CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"r",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"c",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"v",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"v",AGG_AVG,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"id",AGG_DISTINCT,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"id",AGG_MAX,NULL));
    } else if (kind==COLUMNS_DATE) {
        CHECK(pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"occurred",AGG_COUNT,NULL));
    } else {
        if (kind==COLUMNS_RANGE || kind==COLUMNS_PRODUCT || kind==COLUMNS_LARGE) CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,kind==COLUMNS_BOUND || kind==COLUMNS_WIDE ? "Category" : kind==COLUMNS_PRODUCT ? "c1" : "category",AGG_SUM,NULL));
        if (kind==COLUMNS_PRODUCT) CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"c2",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,kind==COLUMNS_LARGE ? "payload" : "amount",kind==COLUMNS_LARGE ? AGG_MIN : AGG_SUM,NULL));
    }
    expected_sql+=sql_count; if (total<0 && kind!=COLUMNS_PRODUCT && kind!=COLUMNS_SQL_LIMIT) expected_counts++;
    worker_submit(&worker,job);
}
typedef enum { STAT_RANGE,STAT_OFFSET,STAT_EXACT,STAT_TINY,STAT_TEXT,STAT_FILTERED,STAT_OVERFLOW,STAT_NONFINITE,STAT_ZERO,STAT_EMPTY,STAT_OBJECT,STAT_PIVOT,STAT_MIXED,STAT_STAMP,STAT_BOUNDS,STAT_COLUMN,STAT_FINAL,STAT_TABLE,STAT_ROWS,STAT_ROWS_AVG,STAT_COLUMNS_AVG,STAT_COLUMNS_SUBSET,STAT_COLUMNS_DATE,STAT_GRAND,STAT_TABLE_BAD,STAT_COLUMNS_NEW_REVISION } StatisticsCase;
static void submit_statistics(StatisticsCase kind,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=JOB_STATISTICS; expected_preview=false; expected_offset=0; statistics_object=0; statistics_revision=revision_id;
    const char *sql=kind==STAT_EXACT || kind==STAT_TINY ? statistics_exact_sql : kind==STAT_TEXT ? statistics_text_sql : kind==STAT_FILTERED ? statistics_filter_sql : kind==STAT_OVERFLOW ? statistics_overflow_sql : kind==STAT_NONFINITE ? statistics_nonfinite_sql : kind>=STAT_ZERO && kind<=STAT_COLUMN ? statistics_zero_sql : kind>=STAT_ROWS_AVG && kind<=STAT_COLUMNS_NEW_REVISION ? kind==STAT_COLUMNS_DATE ? dates_sql : columns_exact_sql : range_sql;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(sql)}; job->revision=statistics_revision; job->rows=600; job->first_col=2; job->cols=1;
    if (kind==STAT_OFFSET) { job->offset=256; job->rows=400; job->first_col=0; job->cols=3; }
    if (kind==STAT_EXACT || kind==STAT_TINY) { job->rows=3; job->first_col=kind==STAT_TINY ? 1 : 0; job->cols=kind==STAT_TINY ? 1 : 3; }
    if (kind>=STAT_TEXT && kind<=STAT_COLUMN) { job->rows=kind==STAT_TEXT ? 5 : kind==STAT_NONFINITE ? 4 : 2; job->first_col=0; }
    if (kind==STAT_FILTERED) { job->rows=100; CHECK(analysis_filter_set(&job->analysis,"amount",FILTER_GREATER,"1.005",NULL)); }
    if (kind==STAT_EMPTY) job->rows=0;
    if (kind==STAT_OBJECT) job->object_id=statistics_object=object_id;
    if (kind==STAT_PIVOT) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"value",AGG_SUM,NULL));
    if (kind==STAT_MIXED) { job->statistics=dx_alloc(sizeof(StatisticsRequest)); CHECK(statistics_request_text(job->statistics,"Owned mixed-selection text",NULL)); }
    if (kind==STAT_STAMP) job->expected_source_stamp=1;
    if (kind==STAT_BOUNDS) { job->offset=DX_SHEET_ROWS; job->rows=1; }
    if (kind==STAT_COLUMN) job->first_col=SIZE_MAX;
    if (kind==STAT_FINAL) { job->offset=10; job->rows=100; }
    if (kind>=STAT_TABLE) {
        job->object_id=statistics_object=object_id;
        if (kind==STAT_TABLE) job->is_table=true;
        else if (kind==STAT_TABLE_BAD) { job->is_table=true; job->object_id=statistics_object=0; }
        else if (kind==STAT_ROWS) { CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_SUM,NULL)); job->first_col=1; }
        else if (kind==STAT_COLUMNS_DATE) {
            CHECK(pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"occurred",AGG_COUNT,NULL)); job->rows=1; job->first_col=0; job->cols=15;
        } else if (kind==STAT_GRAND) { CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"id",AGG_COUNT,NULL)); job->rows=1; job->first_col=0; }
        else {
            CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"r",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"v",AGG_AVG,NULL)); job->rows=5; job->first_col=1;
            if (kind!=STAT_ROWS_AVG) { CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"c",AGG_SUM,NULL)); job->cols=9; }
            if (kind==STAT_COLUMNS_SUBSET || kind==STAT_COLUMNS_NEW_REVISION) { job->offset=2; job->rows=2; }
            if (kind==STAT_COLUMNS_NEW_REVISION) job->revision=statistics_revision=revision_id+4;
        }
    }
    expected_sql+=sql_count; worker_submit(&worker,job);
}
typedef enum { MIX_BLANK,MIX_MANUAL,MIX_TEXT,MIX_EXACT,MIX_OBJECTS,MIX_DATE,MIX_INVALID,MIX_STAMP,MIX_PRIMARY_BAD,MIX_SOURCE_BAD,MIX_BUDGET,MIX_SQL,MIX_CUMULATIVE,MIX_ENGINE } MixedCase;
static size_t mixed_source(StatisticsRequest *request,uint64_t id,const char *sql) {
    Analysis source={.kind=SOURCE_DEMO,.query=(char *)sql}; size_t index=source_snapshot_bind(&request->sources,id,&source,NULL); CHECK(index!=SIZE_MAX); return index;
}
static void submit_mixed(MixedCase kind,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->id=expected_id=next_id; next_id+=4; job->kind=expected_kind=JOB_STATISTICS; expected_preview=false; expected_offset=0; statistics_object=0; job->revision=statistics_revision=revision_id;
    StatisticsRequest *request=job->statistics=dx_alloc(sizeof(*request)); request->cells=20;
    if (kind==MIX_BLANK) request->cells=DX_SHEET_ROWS*UINT64_C(16384);
    else if (kind==MIX_MANUAL) {
        CHECK(statistics_request_text(request,"",NULL) && statistics_request_text(request,"NULL",NULL) && statistics_request_text(request,"1",NULL) && statistics_request_text(request,"NULL",NULL) && statistics_request_text(request,"O'Reilly",NULL));
    } else if (kind==MIX_TEXT || kind==MIX_EXACT) {
        const char *sql=kind==MIX_TEXT ? statistics_text_sql : statistics_exact_sql;
        job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(sql)}; CHECK(mixed_source(request,object_id,sql)==1);
        request->count=2;
        request->ranges[0]=(StatisticsRange){.rows=kind==MIX_TEXT ? 5 : 3,.columns=kind==MIX_TEXT ? 1 : 3};
        request->ranges[1]=(StatisticsRange){.source=1,.rows=kind==MIX_TEXT ? 5 : 2,.columns=kind==MIX_TEXT ? 1 : 2};
        CHECK(statistics_request_text(request,"",NULL) && statistics_request_text(request,"NULL",NULL) && statistics_request_text(request,"1",NULL));
    } else if (kind==MIX_OBJECTS) {
        job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(range_sql)}; CHECK(mixed_source(request,object_id,columns_exact_sql)==1);
        request->cells=700; request->count=4;
        request->ranges[0]=(StatisticsRange){.rows=600,.column=2,.columns=1};
        request->ranges[1]=(StatisticsRange){.source=1,.rows=13,.column=2,.columns=1};
        for (size_t i=2;i<4;i++) {
            StatisticsRange *range=&request->ranges[i]; *range=(StatisticsRange){.is_pivot=true,.source=1,.rows=5,.column=1,.columns=i==3 ? 9 : 1};
            CHECK(pivot_assign(&range->pivot,PIVOT_ROWS,"r",AGG_SUM,NULL) && pivot_assign(&range->pivot,PIVOT_VALUES,"v",AGG_AVG,NULL));
            if (i==3) CHECK(pivot_assign(&range->pivot,PIVOT_COLUMNS,"c",AGG_SUM,NULL));
        }
    } else if (kind==MIX_DATE) {
        job->refresh_source=true; request->cells=32; request->count=2; CHECK(mixed_source(request,object_id,dates_sql)==1);
        Analysis filtered={.kind=SOURCE_DEMO,.query=(char *)statistics_filter_sql}; CHECK(analysis_filter_set(&filtered,"amount",FILTER_GREATER,"1.005",NULL));
        CHECK(source_snapshot_bind(&request->sources,object_id+4,&filtered,NULL)==2); free(filtered.filter_column); free(filtered.filter_value);
        StatisticsRange *range=&request->ranges[0]; *range=(StatisticsRange){.is_pivot=true,.source=1,.rows=1,.columns=15};
        CHECK(pivot_assign_grain(&range->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign_grain(&range->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,NULL) && pivot_assign(&range->pivot,PIVOT_VALUES,"occurred",AGG_COUNT,NULL));
        request->ranges[1]=(StatisticsRange){.source=2,.rows=2,.columns=1};
        CHECK(statistics_request_text(request,"",NULL) && statistics_request_text(request,"NULL",NULL));
    } else if (kind==MIX_INVALID || kind==MIX_STAMP) {
        job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(range_sql)}; request->count=2;
        request->ranges[0]=(StatisticsRange){.rows=1,.columns=1}; request->ranges[1]=(StatisticsRange){.rows=1,.columns=1,.source=kind==MIX_INVALID ? 1 : 0,.expected_source_stamp=kind==MIX_STAMP ? 1 : 0};
    } else if (kind==MIX_PRIMARY_BAD || kind==MIX_SOURCE_BAD) {
        Analysis *source=&job->analysis;
        if (kind==MIX_SOURCE_BAD) { request->sources.count=1; request->sources.ids[0]=object_id; source=&request->sources.items[0]; }
        *source=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup("SELECT 1"),.filter_count=SIZE_MAX,.sort_count=SIZE_MAX};
        for (size_t i=0;i<sizeof(source->filters)/sizeof(*source->filters);i++) source->filters[i]=(AnalysisFilter){.column=dx_strdup("Owned invalid filter"),.value=dx_strdup("")};
        for (size_t i=0;i<sizeof(source->sorts)/sizeof(*source->sorts);i++) source->sorts[i]=(AnalysisSort){.column=dx_strdup("Owned invalid sort")};
    } else if (kind==MIX_BUDGET) {
        char *value=dx_alloc(700001); memset(value,'a',700000);
        Analysis source={.kind=SOURCE_DEMO,.filter_column="label",.filter_op=FILTER_EQUAL,.filter_value=value};
        for (uint64_t i=1;i<=6;i++) CHECK(source_snapshot_bind(&request->sources,i,&source,NULL)==i); free(value);
    } else if (kind==MIX_SQL) {
        char *value=dx_alloc(600001); memset(value,'\'',600000); CHECK(statistics_request_text(request,value,NULL)); free(value);
    } else if (kind==MIX_CUMULATIVE) {
        char *value=dx_alloc(60001); memset(value,'a',60000);
        job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_printf("SELECT '%s'::VARCHAR AS value",value)}; free(value);
        request->count=20; for (size_t i=0;i<request->count;i++) request->ranges[i]=(StatisticsRange){.rows=1,.columns=1};
    } else if (kind==MIX_ENGINE) {
        job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(range_sql)}; CHECK(mixed_source(request,object_id,"SELECT absent FROM missing_mixed_table")==1);
        request->count=2; request->ranges[0]=(StatisticsRange){.rows=1,.columns=1}; request->ranges[1]=(StatisticsRange){.source=1,.rows=1,.columns=1};
    }
    expected_sql+=sql_count; worker_submit(&worker,job);
}
typedef enum { COPY_EXACT,COPY_SPECIAL,COPY_DEEP,COPY_OFFSET,COPY_RAW_TABLE,COPY_ROWS,COPY_COLUMNS,COPY_HEADER,
    COPY_MIXED,COPY_EOF,COPY_MANUAL,COPY_STAMP,COPY_INVALID,COPY_DIMENSIONS,COPY_BYTES,COPY_OVERSIZE,
    COPY_ENGINE,COPY_WIRE_LIMIT,COPY_TSV_LIMIT,COPY_DOMAIN_LIMIT,COPY_SQL_LIMIT,COPY_MAXIMUM,COPY_EMPTY,COPY_CASES } CopyCase;
static const unsigned copy_sql_counts[COPY_CASES]={2,2,2,2,3,2,3,2,9,2,0,0,0,0,0,0,3,2,0,2,1,2,2};
static const char *copy_case_names[COPY_CASES]={"exact typed clipboard text","apostrophe, tab, quote, newline and NULL TSV",
    "one query copies all600 source rows","source EOF trims a direct selection","raw table typed filter/sort identity",
    "full Rows PIVOT with human headers","full true Columns PIVOT and sparse categories","header-only PIVOT",
    "mixed primary/table/Rows/Columns plus manual snapshot","range EOF pads cached cells with SQL NULL","manual-only owned snapshot",
    "native stamps reject before SQL","invalid later patch rejects before first query","malformed dimensions clean owned slots",
    "source byte accounting rejects before SQL","20,001 cells reject before SQL","later engine error discards earlier staged cells",
    "oversized transport publishes no clipboard","TSV quote expansion publishes no clipboard","full column product rejects before data",
    "domain-generated SQL cap discards plans","exact20,000-cell copy envelope","empty direct interval is empty text"};
static const char copy_exact_text[]="9007199254740993\t18446744073709551615\t1234567890123456789012345678.1234567890\t-170141183460469231731687303715884105728\t170141183460469231731687303715884105727\t2026-10-17 12:34:56.123456789\t\t \t\"\"\tNULL\ttrue\t";
static const char copy_special_sql[]="SELECT * FROM (VALUES ('O''Reilly'),('a'||chr(9)||'\"quote\"'||chr(10)||'next'),(''),(NULL::VARCHAR)) t(value)";
static const char copy_columns_sql[]="SELECT * FROM (VALUES ('A','x',1::DECIMAL(38,2)),('A','y',2),('B','x',3)) t(r,c,v)";
static CopyCase copy_case;
static size_t copy_source(CopyRequest *request,uint64_t id,const char *sql) {
    Analysis source={.kind=SOURCE_DEMO,.query=dx_strdup(sql)};
    size_t index=source_snapshot_bind(&request->sources,id,&source,NULL); analysis_free(&source); CHECK(index!=SIZE_MAX); return index;
}
static void submit_copy(CopyCase kind) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->id=expected_id=next_id; next_id+=4;
    job->kind=JOB_COPY; job->object_id=object_id; job->revision=revision_id; job->refresh_source=true;
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(exact_sql)}; job->rows=1; job->cols=12;
    expected_offset=0; expected_preview=kind==COPY_EXACT; job->is_preview=expected_preview;
    if (kind==COPY_SPECIAL) { free(job->analysis.query); job->analysis.query=dx_strdup(copy_special_sql); job->rows=4; job->cols=1; }
    else if (kind==COPY_DEEP || kind==COPY_OFFSET || kind==COPY_EMPTY) {
        free(job->analysis.query); job->analysis.query=dx_strdup(range_sql); job->cols=kind==COPY_EMPTY ? 1 : 3;
        job->rows=kind==COPY_DEEP ? 600 : kind==COPY_OFFSET ? 1000 : 3;
        job->offset=expected_offset=kind==COPY_DEEP ? 0 : kind==COPY_OFFSET ? 500 : 999;
    } else if (kind==COPY_RAW_TABLE) {
        analysis_free(&job->analysis); job->analysis=(Analysis){.kind=SOURCE_DEMO}; job->rows=10; job->cols=1; job->is_table=true;
        CHECK(analysis_filter_set(&job->analysis,"Transaction ID",FILTER_BETWEEN,"[\"10\",\"12\"]",NULL));
        CHECK(analysis_sort_set(&job->analysis,"Transaction ID",true,false,NULL));
    } else if (kind==COPY_ROWS || kind==COPY_COLUMNS || kind==COPY_HEADER || kind==COPY_DOMAIN_LIMIT || kind==COPY_SQL_LIMIT) {
        job->kind=JOB_PIVOT_COPY; free(job->analysis.query);
        job->analysis.query=dx_strdup(kind==COPY_ROWS ? range_sql : kind==COPY_DOMAIN_LIMIT ? columns_product_sql : kind==COPY_SQL_LIMIT ? columns_sql_limit : columns_range_sql);
        job->include_header=kind<=COPY_HEADER; job->rows=kind==COPY_HEADER ? 0 : kind<=COPY_COLUMNS ? 600 : 1;
        job->cols=kind==COPY_ROWS ? 2 : kind<=COPY_HEADER ? 4 : 1;
        if (kind!=COPY_SQL_LIMIT) CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL));
        if (kind!=COPY_ROWS) CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,kind==COPY_DOMAIN_LIMIT ? "c1" : "category",AGG_SUM,NULL));
        if (kind==COPY_DOMAIN_LIMIT) CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"c2",AGG_SUM,NULL));
        CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_SUM,NULL));
        if (kind==COPY_DOMAIN_LIMIT) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_COUNT,NULL));
    } else if (kind==COPY_MIXED || kind==COPY_EOF || kind==COPY_MANUAL || kind==COPY_INVALID || kind==COPY_DIMENSIONS || kind==COPY_BYTES || kind==COPY_ENGINE || kind==COPY_TSV_LIMIT) {
        job->kind=JOB_RANGE_COPY; job->object_id=job->revision=0; job->rows=job->cols=0; analysis_free(&job->analysis);
        CopyRequest *request=job->copy=dx_alloc(sizeof(*request));
        CHECK(clipboard_init(&request->cells,kind==COPY_MIXED ? 5 : kind==COPY_EOF ? 4 : kind==COPY_MANUAL ? 2 : 1,kind==COPY_MIXED ? 8 : kind==COPY_MANUAL ? 3 : kind==COPY_TSV_LIMIT ? 1 : 2,NULL));
        if (kind==COPY_MANUAL) {
            CHECK(clipboard_set(&request->cells,0,1,"",NULL) && clipboard_set(&request->cells,0,2,"NULL",NULL) && clipboard_set(&request->cells,1,0,"O'Reilly",NULL));
        } else if (kind==COPY_TSV_LIMIT) {
            char *quotes=dx_alloc(4u*1024u*1024u+1u); memset(quotes,'"',4u*1024u*1024u); CHECK(clipboard_set(&request->cells,0,0,quotes,NULL)); free(quotes);
        } else {
            job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=dx_strdup(range_sql)};
            request->count=1; request->patches[0]=(CopyPatch){.rows=1,.columns=1};
            if (kind==COPY_MIXED) {
                CHECK(clipboard_set(&request->cells,0,7,"Before async",NULL));
                size_t table=copy_source(request,object_id,statistics_text_sql),columns=copy_source(request,object_id+1,copy_columns_sql);
                request->count=4;
                request->patches[0]=(CopyPatch){.first=500,.rows=2,.columns=2,.target_row=1};
                request->patches[1]=(CopyPatch){.source=table,.rows=5,.columns=1,.target_column=3};
                request->patches[2]=(CopyPatch){.is_pivot=true,.first=598,.rows=2,.columns=2,.target_row=3};
                CHECK(pivot_assign(&request->patches[2].pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL) && pivot_assign(&request->patches[2].pivot,PIVOT_VALUES,"amount",AGG_SUM,NULL));
                request->patches[3]=(CopyPatch){.source=columns,.is_pivot=true,.rows=2,.columns=3,.target_row=3,.target_column=4};
                CHECK(pivot_assign(&request->patches[3].pivot,PIVOT_ROWS,"r",AGG_SUM,NULL) && pivot_assign(&request->patches[3].pivot,PIVOT_COLUMNS,"c",AGG_SUM,NULL) && pivot_assign(&request->patches[3].pivot,PIVOT_VALUES,"v",AGG_SUM,NULL));
            } else if (kind==COPY_EOF) {
                request->patches[0]=(CopyPatch){.first=599,.rows=4,.columns=2}; expected_offset=599;
                CHECK(clipboard_set(&request->cells,3,1,"Cached value must become NULL",NULL));
            } else if (kind==COPY_INVALID) {
                request->count=2; request->patches[1]=(CopyPatch){.source=1,.rows=1,.columns=1,.target_column=1};
            } else if (kind==COPY_DIMENSIONS) {
                CHECK(clipboard_set(&request->cells,0,1,"Owned even after malformed dimensions",NULL)); request->cells.rows=SIZE_MAX;
            } else if (kind==COPY_BYTES) {
                CHECK(copy_source(request,object_id,range_sql)==1); request->sources.bytes++;
            } else if (kind==COPY_ENGINE) {
                size_t source=copy_source(request,object_id,"SELECT absent FROM missing_copy_table");
                request->count=2; request->patches[1]=(CopyPatch){.source=source,.rows=1,.columns=1,.target_column=1};
            }
        }
    } else if (kind==COPY_STAMP) job->expected_source_stamp=UINT64_MAX;
    else if (kind==COPY_OVERSIZE) { job->rows=20001; job->cols=1; }
    else if (kind==COPY_WIRE_LIMIT) { free(job->analysis.query); job->analysis.query=dx_strdup("SELECT repeat('x',1000000)::VARCHAR AS payload FROM range(5)"); job->rows=5; job->cols=1; }
    else if (kind==COPY_MAXIMUM) { free(job->analysis.query); job->analysis.query=dx_strdup("SELECT i::BIGINT AS copied_id FROM range(20000) t(i) ORDER BY i"); job->rows=20000; job->cols=1; }
    expected_kind=job->kind; expected_sql+=copy_sql_counts[kind]; worker_submit(&worker,job);
}
static bool copy_case_error(CopyCase kind) { return kind>=COPY_STAMP && kind<=COPY_SQL_LIMIT; }
static bool poll_received(bool error,bool sent) {
    CHECK(!reply); reply=worker_poll(&worker); if (!reply) return false;
    CHECK(reply->id==expected_id && reply->kind==expected_kind && reply->object_id==(expected_kind==JOB_EXPORT ? export_object : expected_kind==JOB_STATISTICS ? statistics_object : expected_kind==JOB_RANGE_COPY ? 0 : object_id) && reply->revision==(expected_kind==JOB_EXPORT ? export_revision : expected_kind==JOB_STATISTICS ? statistics_revision : expected_kind==JOB_RANGE_COPY ? 0 : revision_id) && reply->is_preview==expected_preview);
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
    if (retained_clipboard) CHECK(!strcmp(retained_clipboard,copy_exact_text));
    if (retained_statistics.sum) CHECK(retained_statistics.cells==600 && retained_statistics.numeric==600 && !retained_statistics.approximate && !strcmp(retained_statistics.sum,"29700") && !strcmp(retained_statistics.average,"49.5"));
    if (!retained_distinct.count) return;
    category_values(&retained_distinct,0,128,300); exact_values(&retained);
}
static void copy_values(CopyCase kind) {
    CHECK(reply->clipboard && !reply->error); validation_reply();
    CHECK(!reply->page.totals && !reply->statistics.sum && !reply->statistics.cells && !reply->distinct.values);
    const char *text=reply->clipboard;
    if (kind==COPY_EXACT) CHECK(!strcmp(text,copy_exact_text));
    else if (kind==COPY_SPECIAL) CHECK(!strcmp(text,"O'Reilly\n\"a\t\"\"quote\"\"\nnext\"\n\"\"\n"));
    else if (kind==COPY_RAW_TABLE) CHECK(!strcmp(text,"12\n11\n10") && reply->analysis.filter_number.kind==NUM_I64);
    else if (kind==COPY_HEADER) CHECK(!strcmp(text,"row_id\t0_SUM(amount)\t1_SUM(amount)\t2_SUM(amount)"));
    else if (kind==COPY_MIXED) CHECK(!strcmp(text,"\t\t\t\t\t\t\tBefore async\n500\trow-500\t\t\"\"\t\t\t\t\n501\trow-501\t\tNULL\t\t\t\t\n598\t98.00\t\t \tA\t1.00\t2.00\t\n599\t99.00\t\tNULL\tB\t3.00\t\t"));
    else if (kind==COPY_EOF) CHECK(!strcmp(text,"599\trow-599\n\t\n\t\n\t"));
    else if (kind==COPY_MANUAL) CHECK(!strcmp(text,"\t\"\"\tNULL\nO'Reilly\t\t"));
    else if (kind==COPY_EMPTY) CHECK(!*text);
    else if (kind==COPY_MAXIMUM) {
        for (uint64_t i=0;i<20000;i++) { char *end=NULL; CHECK(strtoull(text,&end,10)==i && end>text && *end==(i==19999 ? 0 : '\n')); text=end+(i==19999 ? 0 : 1); }
        CHECK(!*text);
    } else {
        bool columns=kind==COPY_COLUMNS,rows=kind==COPY_ROWS; size_t first=kind==COPY_OFFSET ? 500 : 0,last=600;
        const char *header=columns ? "row_id\t0_SUM(amount)\t1_SUM(amount)\t2_SUM(amount)\n" : rows ? "row_id\tSUM(amount)\n" : "";
        CHECK(!strncmp(text,header,strlen(header))); text+=strlen(header);
        for (size_t i=first;i<last;i++) {
            char line[128],amount[32]; snprintf(amount,sizeof(amount),"%zu.00",i%100);
            if (columns) { const char *slots[3]={"","",""}; slots[i%3]=amount; snprintf(line,sizeof(line),"%zu\t%s\t%s\t%s",i,slots[0],slots[1],slots[2]); }
            else if (rows) snprintf(line,sizeof(line),"%zu\t%s",i,amount);
            else snprintf(line,sizeof(line),"%zu\trow-%zu\t%s",i,i,amount);
            size_t length=strlen(line); CHECK(!strncmp(text,line,length)); text+=length;
            CHECK(*text==(i+1==last ? 0 : '\n')); if (*text) text++;
        }
        CHECK(!*text);
    }
}
typedef enum { EXPORT_PRIMARY,EXPORT_TABLE,EXPORT_ROWS,EXPORT_COLUMNS,EXPORT_EMPTY,EXPORT_GRAND,EXPORT_DATE,
    EXPORT_STAMP,EXPORT_OWNER,EXPORT_PAYLOAD,EXPORT_PATH,EXPORT_COUNTS,EXPORT_NONE,EXPORT_DATABASE,
    EXPORT_HUGE,EXPORT_NESTED,EXPORT_DOMAIN,EXPORT_SQL,EXPORT_ENGINE,
    EXPORT_NULL_DATA,EXPORT_OVERSIZE_DATA,EXPORT_HEADER,EXPORT_FOOTER,EXPORT_NULL_COUNT,EXPORT_NEGATIVE_COUNT,
    EXPORT_OVERFLOW_COUNT,EXPORT_NONCANONICAL_COUNT,EXPORT_COUNT_TYPE,EXPORT_WIRE,EXPORT_TRUNCATED_WIRE,EXPORT_COUNT_NAME,EXPORT_TRANSPORT,EXPORT_CASES } ExportCase;
static ExportCase export_case;
static const unsigned export_sql_counts[EXPORT_CASES]={2,3,2,3,2,2,5,0,0,0,0,0,0,0,1,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2};
static const uint64_t export_row_counts[]={600,4,5,5,0,1,1};
static const char *export_names[EXPORT_CASES]={"full typed primary Parquet bytes","registered filtered/sorted table Parquet bytes","Rows AVG/distinct native export aliases","real Columns typed-category export aliases",
    "empty typed Rows export","empty global COUNT export","two date-grain Columns COUNT export","native stamp rejection","missing table owner rejection","unrelated owned payload rejection","OS path rejection","malformed owned shelves cleanup","missing source rejection","native database source rejection",
    "128-bit precision rejection before COPY","nested unknown type rejection before COPY","bounded column-product rejection before COPY","escaped-domain SQL limit rejection before COPY","engine schema error without partial bytes",
    "NULL binary rejection","oversized binary rejection before dereference","PAR1 corruption rejection","footer corruption rejection","SQL NULL Count rejection","negative Count rejection","Count overflow rejection","noncanonical Count rejection","nonnumeric Count rejection","malformed Count wire rejection","truncated Count wire rejection","wrong Count name rejection","explicit export transport failure"};
static const char export_exact_sql[]="SELECT i::BIGINT AS row_id,9007199254740993::BIGINT AS wide,'18446744073709551615'::UBIGINT AS u64,'9007199254740993.1234567890'::DECIMAL(38,10) AS amount,CASE i%3 WHEN 0 THEN NULL::VARCHAR WHEN 1 THEN '' ELSE 'NULL' END AS label,'O''Reilly\t\"quoted\"\nnext' AS special FROM range(600) t(i) ORDER BY i";
static const char export_csv[]="id,City,Amount\n10,A,1.25\n11,B,2.50\n12,A,3.75\n13,NULL,\n14,,0.00\n";
static void submit_export(ExportCase kind) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->kind=expected_kind=JOB_EXPORT; job->id=expected_id=next_id; next_id+=4;
    job->object_id=export_object=kind==EXPORT_PRIMARY || kind==EXPORT_OWNER ? 0 : object_id;
    job->revision=export_revision=job->object_id ? revision_id : 0; expected_preview=false; expected_offset=0;
    bool analytical=kind==EXPORT_ROWS || kind==EXPORT_COLUMNS || kind==EXPORT_EMPTY || kind==EXPORT_GRAND || kind==EXPORT_DATE || kind==EXPORT_DOMAIN || kind==EXPORT_SQL;
    job->is_table=!analytical && kind!=EXPORT_PRIMARY; job->refresh_source=true; job->path=dx_strdup("Proof export.parquet");
    const char *source=analytical ? columns_exact_sql : export_exact_sql;
    if (kind==EXPORT_DATE) source=dates_sql;
    if (kind==EXPORT_DOMAIN) source=columns_product_sql;
    if (kind==EXPORT_SQL) source=columns_sql_limit;
    if (kind==EXPORT_HUGE) source=exact_sql;
    if (kind==EXPORT_NESTED) source="SELECT {'value':1::HUGEINT} AS nested";
    if (kind==EXPORT_ENGINE) source="SELECT absent FROM missing_export_table";
    job->analysis=(Analysis){.kind=SOURCE_DEMO,.query=kind==EXPORT_EMPTY || kind==EXPORT_GRAND ? dx_printf("SELECT * FROM (%s) empty_source WHERE false",source) : dx_strdup(source)};
    if (kind==EXPORT_TABLE) {
        analysis_free(&job->analysis); job->analysis=(Analysis){.kind=SOURCE_CSV,.path=dx_strdup("worker-export-source.csv"),.query=dx_strdup("SELECT id::BIGINT AS id,City::VARCHAR AS City,Amount::DECIMAL(38,10) AS Amount FROM input")};
        CHECK(analysis_filter_set(&job->analysis,"id",FILTER_GREATER,"10",NULL) && analysis_sort_set(&job->analysis,"id",true,false,NULL));
    }
    if (analytical) {
        if (kind==EXPORT_DATE) {
            CHECK(pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_MONTH,NULL) && pivot_assign_grain(&job->pivot,PIVOT_COLUMNS,"occurred",AGG_SUM,PIVOT_DATE_QUARTER,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"occurred",AGG_COUNT,NULL));
        } else if (kind==EXPORT_DOMAIN) {
            CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"row_id",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_COLUMNS,"c1",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_COLUMNS,"c2",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_AVG,NULL));
        } else if (kind==EXPORT_SQL) {
            CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"category",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"amount",AGG_AVG,NULL));
        } else if (kind==EXPORT_GRAND) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"id",AGG_COUNT,NULL));
        else {
            CHECK(pivot_assign(&job->pivot,PIVOT_ROWS,"r",AGG_SUM,NULL) && pivot_assign(&job->pivot,PIVOT_VALUES,"v",AGG_AVG,NULL));
            if (kind!=EXPORT_EMPTY) CHECK(pivot_assign(&job->pivot,PIVOT_VALUES,"id",AGG_DISTINCT,NULL));
            if (kind==EXPORT_COLUMNS) CHECK(pivot_assign(&job->pivot,PIVOT_COLUMNS,"c",AGG_SUM,NULL));
        }
    }
    if (kind==EXPORT_STAMP) job->expected_source_stamp=UINT64_MAX;
    if (kind==EXPORT_PAYLOAD) job->statistics=dx_alloc(sizeof(StatisticsRequest));
    if (kind==EXPORT_PATH) { free(job->path); job->path=dx_strdup("/tmp/forbidden.parquet"); }
    if (kind==EXPORT_COUNTS) { job->pivot.counts[PIVOT_VALUES]=SIZE_MAX; for (size_t i=0;i<DX_PIVOT_FIELDS;i++) { job->pivot.fields[PIVOT_VALUES][i].name=dx_strdup("Owned invalid field"); job->pivot.fields[PIVOT_VALUES][i].value=dx_strdup(""); } }
    if (kind==EXPORT_NONE || kind==EXPORT_DATABASE) { analysis_free(&job->analysis); job->analysis.kind=kind==EXPORT_NONE ? SOURCE_NONE : SOURCE_DATABASE; }
    export_fault(kind>=EXPORT_NULL_DATA ? (int)(kind-EXPORT_NULL_DATA+1) : 0);
    expected_sql+=export_sql_counts[kind]; worker_submit(&worker,job);
}
static void export_result_values(ExportCase kind) {
    validation_reply(); CHECK(!reply->clipboard && !reply->statistics.sum && !reply->distinct.values);
    if (kind>=EXPORT_STAMP) { CHECK(reply->error && !reply->export_data && !reply->export_bytes && !reply->exported_rows); return; }
    CHECK(!reply->error && reply->export_data && reply->export_bytes>=12 && reply->export_bytes<=64u*1024u*1024u && reply->exported_rows==export_row_counts[kind]);
    CHECK(!memcmp(reply->export_data,"PAR1",4) && !memcmp(reply->export_data+reply->export_bytes-4,"PAR1",4));
    CHECK(reply->path && !strcmp(reply->path,"Proof export.parquet"));
    if (retained_export) CHECK(reply->export_data!=retained_export);
}
static void submit_export_readback(uint64_t offset,unsigned sql_count) {
    CHECK(!reply); QueryJob *job=dx_alloc(sizeof(*job)); job->kind=expected_kind=JOB_PAGE; job->id=expected_id=next_id; next_id+=4;
    job->analysis=(Analysis){.kind=SOURCE_PARQUET,.path=dx_printf("worker-export-reopen-%u.parquet",(unsigned)export_case)};
    job->object_id=object_id; job->revision=revision_id; job->is_table=true; job->offset=expected_offset=offset; job->known_total=(int64_t)export_row_counts[export_case];
    expected_preview=false; expected_sql+=sql_count; worker_submit(&worker,job);
}
static size_t exported_column(const Page *page,const char *name) {
    for (size_t i=0;i<page->column_count;i++) if (!strcmp(page->columns[i].name,name)) return i;
    CHECK(!"exported column is missing"); return 0;
}
static void exported_double_value(const Page *page,size_t row,size_t column,double expected) {
    const Column *type=&page->columns[column]; const Cell *cell=&page->cells[row*page->column_count+column];
    CHECK(type->type==DUCKDB_TYPE_DOUBLE && type->number_type.kind==NUM_FLOAT && !cell->is_null && cell->text);
    char *end=NULL; double value=strtod(cell->text,&end);
    if (!isfinite(value) || end==cell->text || *end || value!=expected) fprintf(stderr,"Exported DOUBLE row %zu column %s: '%s', expected %.17g\n",row,type->name,cell->text,expected);
    CHECK(isfinite(value) && end!=cell->text && !*end && value==expected);
}
static void export_readback_values(ExportCase kind) {
    Page *page=&reply->page; CHECK(!reply->error && !reply->export_data && !reply->export_bytes && !page->count_pending && page->total==export_row_counts[kind]);
    if (kind==EXPORT_PRIMARY) {
        size_t rows=page->offset==257 ? 256 : 88; CHECK(page->column_count==6 && page->row_count==rows && (page->offset==257 || page->offset==512));
        CHECK(page->columns[1].type==DUCKDB_TYPE_BIGINT && page->columns[2].type==DUCKDB_TYPE_UBIGINT && !strcmp(page->columns[3].type_name,"DECIMAL(38,10)"));
        for (size_t r=0;r<rows;r++) {
            const Cell *cells=&page->cells[r*6]; uint64_t id=page->offset+r; char number[24]; snprintf(number,sizeof(number),"%" PRIu64,id);
            CHECK(!strcmp(cells[0].text,number) && !strcmp(cells[1].text,"9007199254740993") && !strcmp(cells[2].text,"18446744073709551615") && !strcmp(cells[3].text,"9007199254740993.1234567890"));
            CHECK(id%3 ? !cells[4].is_null && !strcmp(cells[4].text,id%3==1 ? "" : "NULL") : cells[4].is_null && !cells[4].text);
            CHECK(!strcmp(cells[5].text,"O'Reilly\t\"quoted\"\nnext"));
        }
    } else if (kind==EXPORT_TABLE) {
        CHECK(page->column_count==3 && page->row_count==4 && !strcmp(page->columns[2].type_name,"DECIMAL(38,10)"));
        CHECK(!strcmp(page->cells[0].text,"14") && !strcmp(page->cells[2].text,"0.0000000000") && !strcmp(page->cells[3].text,"13") && page->cells[5].is_null);
        CHECK(!strcmp(page->cells[6].text,"12") && !strcmp(page->cells[8].text,"3.7500000000") && !strcmp(page->cells[9].text,"11") && !strcmp(page->cells[11].text,"2.5000000000"));
    } else if (kind==EXPORT_ROWS || kind==EXPORT_COLUMNS) {
        CHECK(page->row_count==5 && page->column_count==(kind==EXPORT_ROWS ? 3u : 19u));
        CHECK(page->cells[0].is_null && !*page->cells[page->column_count].text && !strcmp(page->cells[4*page->column_count].text,"NULL"));
        if (kind==EXPORT_ROWS) {
            CHECK(!strcmp(page->columns[1].name,"AVG(v)") && !strcmp(page->columns[2].name,"COUNT DISTINCT(id)"));
            CHECK(!strcmp(page->cells[7].text,"4.5") && !strcmp(page->cells[8].text,"2") && !strcmp(page->cells[10].text,"5.5"));
        } else {
            size_t missing=exported_column(page,"c=NULL · AVG(v)"),empty=exported_column(page,"c='' · AVG(v)"),literal=exported_column(page,"c='NULL' · AVG(v)");
            size_t upper=exported_column(page,"c='FOO' · AVG(v)"),lower=exported_column(page,"c='foo' · AVG(v) · 2");
            /* AVG remains DOUBLE through Parquet. Its canonical text may be
               10.0 rather than integer spelling; verify the full typed value. */
            exported_double_value(page,0,missing,10); exported_double_value(page,1,empty,100); exported_double_value(page,4,literal,200);
            exported_double_value(page,3,upper,7); exported_double_value(page,3,lower,11);
            CHECK(exported_column(page,"c='quote_\"''_SUM' · AVG(v)")<page->column_count);
        }
    } else if (kind==EXPORT_EMPTY) CHECK(!page->row_count && page->column_count==2 && !strcmp(page->columns[1].name,"AVG(v)"));
    else if (kind==EXPORT_GRAND) CHECK(page->row_count==1 && page->column_count==1 && !strcmp(page->columns[0].name,"COUNT(*) [id]") && !strcmp(page->cells[0].text,"0"));
    else if (kind==EXPORT_DATE) {
        CHECK(page->row_count==1 && page->column_count==15); uint64_t sum=0;
        for (size_t c=0;c<15;c++) { CHECK(page->columns[c].type==DUCKDB_TYPE_BIGINT && !page->cells[c].is_null); sum+=strtoull(page->cells[c].text,NULL,10); }
        CHECK(sum==9 && !strcmp(page->cells[exported_column(page,"occurred='2025-01-01' · occurred='Q1' · COUNT(*) [occurred]")].text,"5"));
    }
}
static void submit_next_export(void) {
    if (++export_case==EXPORT_TABLE) { register_export_source(export_csv); enter(WAIT_EXPORT_SOURCE_REGISTER); }
    else if (export_case<EXPORT_CASES) { submit_export(export_case); enter(WAIT_EXPORT_CASES); }
    else { hold_export_reply(); submit_export(EXPORT_COLUMNS); enter(WAIT_EXPORT_CANCEL); }
}
static void inject_late_export(void *unused) {
    (void)unused; CHECK(!dx_browser_export_result(stale_cookie,stale_ticket,(const void *)(uintptr_t)1,SIZE_MAX,(unsigned char *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1)); export_late_callbacks++;
}
static void distinct_reply(size_t count,uint64_t total) {
    validation_reply(); CHECK(reply->distinct.offset==0 && reply->distinct.count==count && reply->distinct.total==total);
}
static void date_value(size_t row,const char *value,const char *label,const char *upper) {
    distinct_value(&reply->distinct.values[row],value,label,upper);
}
static void decimal_analysis(bool secondary) {
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
static void decimal_distinct_values(bool secondary) {
    distinct_reply(2,2);
    distinct_value(&reply->distinct.values[0],"B","B",NULL);
    distinct_value(&reply->distinct.values[1],"C","C",NULL);
    decimal_analysis(secondary);
}
static void matrix_range_values(uint64_t offset,size_t rows,bool totals) {
    Page *page=&reply->page; CHECK(page->offset==offset && page->row_count==rows && page->total==600 && !page->count_pending && page->column_count==2);
    CHECK(page->sql && strstr(page->sql,"PIVOT (") && !strcmp(page->columns[1].caption,"SUM(amount)") && page->columns[1].number_type.kind==NUM_DECIMAL);
    CHECK(page->bytes<=4*1024*1024 && (page->totals!=NULL)==totals);
    for (size_t r=0;r<rows;r++) {
        char id[32],amount[32]; snprintf(id,sizeof(id),"%" PRIu64,offset+r); snprintf(amount,sizeof(amount),"%" PRIu64 ".00",(offset+r)%100);
        CHECK(!strcmp(page->cells[r*2].text,id) && !strcmp(page->cells[r*2+1].text,amount));
        if (totals) CHECK(!strcmp(page->totals->rows.cells[r].text,amount) && page->totals->rows.cells[r].text!=page->cells[r*2+1].text);
    }
    if (totals) CHECK(!page->totals->has_columns && page->totals->rows.offset==offset && page->totals->rows.total==600 && page->totals->columns.row_count==1 && !strcmp(page->totals->columns.cells[0].text,"29700.00"));
}
static void columns_range_values(uint64_t offset,size_t rows,bool totals) {
    Page *p=&reply->page; CHECK(p->row_count==rows && p->column_count==4 && p->offset==offset && p->total==600 && !p->count_pending && p->sql && strstr(p->sql,"PIVOT (") && strstr(p->sql," ON ") && strstr(p->sql," IN (") && p->bytes<=4*1024*1024);
    CHECK((p->totals!=NULL)==totals);
    for (size_t c=0;c<3;c++) { char category[16]; snprintf(category,sizeof(category),"%zu",c); CHECK(p->columns[c+1].category_count==1 && !strcmp(p->columns[c+1].categories[0],category) && !strcmp(p->columns[c+1].caption,category)); }
    for (size_t r=0;r<rows;r++) {
        char key[32],amount[32]; snprintf(key,sizeof(key),"%" PRIu64,offset+r); snprintf(amount,sizeof(amount),"%" PRIu64 ".00",(offset+r)%100);
        CHECK(!strcmp(p->cells[r*4].text,key));
        for (size_t c=0;c<3;c++) CHECK(c==(offset+r)%3 ? !p->cells[r*4+c+1].is_null && !strcmp(p->cells[r*4+c+1].text,amount) : p->cells[r*4+c+1].is_null && !p->cells[r*4+c+1].text);
        if (totals) CHECK(!strcmp(p->totals->rows.cells[r].text,amount));
    }
    if (totals) { CHECK(p->totals->has_columns && p->totals->rows.offset==offset && p->totals->rows.total==600 && p->totals->columns.column_count==4); for (size_t i=0;i<3;i++) CHECK(!strcmp(p->totals->columns.cells[i].text,"9900.00")); CHECK(!strcmp(p->totals->columns.cells[3].text,"29700.00")); }
}
static void statistics_values(uint64_t cells,uint64_t filled,uint64_t numeric,uint64_t distinct) {
    validation_reply(); SelectionStats *s=&reply->statistics;
    CHECK(s->cells==cells && s->filled==filled && s->numeric==numeric && s->distinct==distinct && isfinite(s->query_ms) && s->query_ms>=0 && !reply->distinct.values && !reply->clipboard);
}
static void statistics_error(void) {
    validation_reply(); SelectionStats empty={0}; CHECK(!memcmp(&reply->statistics,&empty,sizeof(empty)));
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
        submit_decimal(JOB_PAGE,2); enter(WAIT_DECIMAL_PAGE); return;
    case WAIT_DECIMAL_PAGE: {
        if (!received(false)) return;
        const Page *page=&reply->page; decimal_analysis(false);
        CHECK(page->row_count==2 && page->column_count==2 && page->offset==0 && page->total==2 && !page->count_pending && !page->release_cell_text);
        CHECK(!strcmp(page->columns[0].name,"amount") && !strcmp(page->columns[0].type_name,"DECIMAL(4,2)") && page->columns[0].type==DUCKDB_TYPE_DECIMAL);
        CHECK(page->columns[0].numeric && page->columns[0].number_type.kind==NUM_DECIMAL && page->columns[0].number_type.precision==4 && page->columns[0].number_type.scale==2);
        CHECK(!strcmp(page->columns[1].name,"label") && page->columns[1].type==DUCKDB_TYPE_VARCHAR && !page->columns[1].numeric);
        static const char *values[]={"1.01","B","1.02","C"};
        for (size_t i=0;i<4;i++) CHECK(!page->cells[i].is_null && page->cells[i].text && !strcmp(page->cells[i].text,values[i]));
        release_reply(); pass("typed DECIMAL PAGE keeps exact 1.01/B and 1.02/C above the unrounded 1.005 threshold");
        submit_decimal(JOB_COUNT,1); enter(WAIT_DECIMAL_COUNT); return;
    }
    case WAIT_DECIMAL_COUNT:
        if (!received(false)) return;
        decimal_analysis(false); validation_reply(); CHECK(reply->counted_rows==2);
        release_reply(); pass("typed DECIMAL COUNT reuses source metadata and counts both rows above 1.005");
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
        submit_matrix(MATRIX_RANGE,0,-1,true,4); enter(WAIT_MATRIX0); return;
    case WAIT_MATRIX0:
        if (!received(false)) return;
        matrix_range_values(0,256,true); release_reply(); pass("real Rows/Values PIVOT, exact 600-group count and source-derived grand total");
        submit_matrix(MATRIX_RANGE,256,600,true,2); enter(WAIT_MATRIX256); return;
    case WAIT_MATRIX256:
        if (!received(false)) return;
        matrix_range_values(256,256,true); release_reply(); pass("matrix second page reuses typed schema and exact count");
        submit_matrix(MATRIX_RANGE,512,600,true,2); enter(WAIT_MATRIX512); return;
    case WAIT_MATRIX512:
        if (!received(false)) return;
        matrix_range_values(512,88,true); release_reply(); pass("matrix last page preserves independent exact measure totals");
        submit_matrix(MATRIX_EXACT,0,-1,true,5); enter(WAIT_MATRIX_EXACT); return;
    case WAIT_MATRIX_EXACT: {
        if (!received(false)) return;
        Page *page=&reply->page; CHECK(page->row_count==3 && page->column_count==5 && page->total==3 && !page->count_pending && page->totals && page->bytes<=4*1024*1024);
        CHECK(page->cells[0].is_null && !page->cells[5].is_null && !*page->cells[5].text && !strcmp(page->cells[10].text,"NULL"));
        CHECK(!strcmp(page->cells[1].text,"30.00") && !strcmp(page->cells[6].text,"30.00") && page->cells[11].is_null && !strcmp(page->cells[3].text,"9007199254740994") && !strcmp(page->cells[8].text,"9007199254740993"));
        CHECK(page->columns[1].number_type.kind==NUM_DECIMAL && page->columns[1].number_type.precision==38 && page->columns[1].number_type.scale==2 && page->columns[2].type==DUCKDB_TYPE_DOUBLE);
        CHECK(strtod(page->cells[2].text,NULL)==15 && strtod(page->cells[7].text,NULL)==30 && page->cells[12].is_null);
        CHECK(!strcmp(page->cells[4].text,"2") && !strcmp(page->cells[9].text,"1") && !strcmp(page->cells[14].text,"0"));
        Page *grand=&page->totals->columns; CHECK(grand->column_count==4 && grand->row_count==1 && !strcmp(grand->cells[0].text,"60.00") && strtod(grand->cells[1].text,NULL)==20 && !strcmp(grand->cells[2].text,"9007199254740994") && !strcmp(grand->cells[3].text,"2"));
        CHECK(page->totals->rows.cells[8].is_null && !strcmp(page->totals->rows.cells[11].text,"0")); release_reply(); pass("matrix typed NULL/empty/literal NULL, DECIMAL, wide BIGINT, exact predicate, AVG and distinct grand semantics");
        submit_matrix(MATRIX_GRAND,0,-1,true,4); enter(WAIT_MATRIX_GRAND); return;
    }
    case WAIT_MATRIX_GRAND:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==4 && reply->page.total==1 && reply->page.totals && !strcmp(reply->page.cells[0].text,"60.00") && strtod(reply->page.cells[1].text,NULL)==20 && !strcmp(reply->page.cells[2].text,"9007199254740994") && !strcmp(reply->page.cells[3].text,"2"));
        CHECK(!strcmp(reply->page.totals->columns.cells[0].text,"60.00") && !strcmp(reply->page.totals->columns.cells[3].text,"2")); release_reply(); pass("matrix without Rows returns one real aggregate and source grand row");
        submit_matrix(MATRIX_EMPTY,0,-1,true,4); enter(WAIT_MATRIX_EMPTY); return;
    case WAIT_MATRIX_EMPTY:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==4 && reply->page.cells[0].is_null && reply->page.cells[1].is_null && reply->page.cells[2].is_null && !strcmp(reply->page.cells[3].text,"0"));
        CHECK(reply->page.totals && reply->page.totals->columns.cells[0].is_null && reply->page.totals->columns.cells[1].is_null && reply->page.totals->columns.cells[2].is_null && !strcmp(reply->page.totals->columns.cells[3].text,"0")); release_reply(); pass("empty filtered matrix aggregate keeps SUM/AVG/MAX NULL and distinct count zero");
        submit_matrix(MATRIX_HUGE,0,-1,true,4); enter(WAIT_MATRIX_HUGE); return;
    case WAIT_MATRIX_HUGE:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==1 && reply->page.columns[0].type==DUCKDB_TYPE_HUGEINT && reply->page.columns[0].number_type.kind==NUM_I128 && !strcmp(reply->page.cells[0].text,"27021597764222980"));
        CHECK(reply->page.totals && reply->page.totals->columns.columns[0].type==DUCKDB_TYPE_HUGEINT && !strcmp(reply->page.totals->columns.cells[0].text,"27021597764222980")); release_reply(); pass("matrix SUM(BIGINT) keeps an exact typed HUGEINT through body and grand footer");
        submit_matrix(MATRIX_DATE,0,-1,true,5); enter(WAIT_MATRIX_DATE); return;
    case WAIT_MATRIX_DATE:
        if (!received(false)) return;
        CHECK(reply->page.row_count==5 && reply->page.column_count==2 && reply->page.columns[0].type==DUCKDB_TYPE_DATE && reply->page.columns[0].date_grain==PIVOT_DATE_MONTH && !strcmp(reply->page.columns[0].name,"MONTH(occurred)"));
        CHECK(reply->page.cells[0].is_null && !strcmp(reply->page.cells[4].text,"2025-01-01") && !strcmp(reply->page.cells[5].text,"5") && reply->page.totals && !strcmp(reply->page.totals->columns.cells[0].text,"9")); release_reply(); pass("matrix Month Rows resolve temporal source types and retain NULL plus COUNT(*) semantics");
        submit_matrix(MATRIX_RANGE,0,-1,false,3); enter(WAIT_MATRIX_NO_TOTALS); return;
    case WAIT_MATRIX_NO_TOTALS:
        if (!received(false)) return;
        matrix_range_values(0,256,false); release_reply(); pass("matrix include_totals=false retains header metadata without a footer query");
        submit_matrix(MATRIX_COLUMNS,0,-1,true,1); enter(WAIT_MATRIX_COLUMNS); return;
    case WAIT_MATRIX_COLUMNS:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"128 result columns")); validation_reply(); release_reply(); pass("the 129th typed column category rejects before result DESCRIBE or data SQL");
        submit_matrix(MATRIX_INVALID,0,-1,true,0); enter(WAIT_MATRIX_INVALID); return;
    case WAIT_MATRIX_INVALID:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"bounded field limit")); validation_reply(); release_reply(); pass("malformed owned matrix counts reject before SQL and free within shelf capacity");
        submit_matrix(MATRIX_LARGE,0,-1,true,4); enter(WAIT_MATRIX_LARGE); return;
    case WAIT_MATRIX_LARGE:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"4 MiB")); validation_reply(); release_reply(); pass("combined matrix and totals memory cap rejects without publishing a partial page");
        submit_matrix(MATRIX_RANGE,0,600,true,3); enter(WAIT_MATRIX_RECOVERY); return;
    case WAIT_MATRIX_RECOVERY:
        if (!received(false)) return;
        matrix_range_values(0,256,true); release_reply(); pass("matrix recovery after local/schema/memory errors keeps previous owned results intact");
        hold_matrix_reply(1); submit_matrix(MATRIX_RANGE,0,-1,true,2); enter(WAIT_MATRIX_CANCEL_COUNT); return;
    case WAIT_MATRIX_CANCEL_COUNT:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        CHECK(worker.running); counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); pass("cancel after the real matrix count discards staged data and ignores its delayed callback");
        hold_matrix_reply(2); submit_matrix(MATRIX_RANGE,256,600,true,2); enter(WAIT_MATRIX_CANCEL_GRAND); return;
    case WAIT_MATRIX_CANCEL_GRAND:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        CHECK(worker.running); counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); pass("cancel after real source grand totals frees staged matrix ownership and ignores delayed totals");
        submit_matrix(MATRIX_RANGE,512,600,true,2); enter(WAIT_MATRIX_FINAL); return;
    case WAIT_MATRIX_FINAL:
        if (!received(false)) return;
        matrix_range_values(512,88,true); release_reply(); pass("matrix cancellation recovery publishes only a complete current page and footer");
        submit_columns(COLUMNS_EXACT,0,-1,true,6); enter(WAIT_COLUMNS_EXACT); return;
    case WAIT_COLUMNS_EXACT: {
        if (!received(false)) return;
        Page *p=&reply->page; CHECK(p->row_count==5 && p->column_count==37 && p->total==5 && p->totals && p->totals->has_columns && p->totals->rows.column_count==4 && p->totals->columns.column_count==40);
        CHECK(p->cells[0].is_null && !*p->cells[37].text && !strcmp(p->cells[74].text,"A") && !strcmp(p->cells[111].text,"B") && !strcmp(p->cells[148].text,"NULL"));
        CHECK(!p->columns[1].categories[0] && p->columns[5].categories[0] && !*p->columns[5].categories[0] && !strcmp(p->columns[13].categories[0],"NULL"));
        CHECK(!strcmp(p->columns[9].categories[0],"FOO") && !strcmp(p->columns[17].categories[0],"foo") && !strcmp(p->columns[21].categories[0],"quote_\"'_SUM") && !strcmp(p->columns[22].detail,"AVG(v)"));
        CHECK(!strcmp(p->cells[74+25].text,"4.00") && strtod(p->cells[74+26].text,NULL)==2 && !strcmp(p->cells[74+27].text,"2") && !strcmp(p->cells[74+28].text,"9007199254740994"));
        CHECK(p->columns[25].number_type.kind==NUM_DECIMAL && p->columns[25].number_type.scale==2 && p->columns[28].type==DUCKDB_TYPE_BIGINT);
        Page *r=&p->totals->rows,*c=&p->totals->columns;
        CHECK(!strcmp(r->cells[8].text,"18.00") && strtod(r->cells[9].text,NULL)==4.5 && !strcmp(r->cells[10].text,"2") && !strcmp(r->cells[12].text,"22.00") && strtod(r->cells[13].text,NULL)==5.5);
        CHECK(!strcmp(c->cells[24].text,"6.00") && strtod(c->cells[25].text,NULL)==2 && !strcmp(c->cells[26].text,"2") && !strcmp(c->cells[36].text,"350.00") && strtod(c->cells[37].text,NULL)>31.81 && strtod(c->cells[37].text,NULL)<31.82 && !strcmp(c->cells[38].text,"3") && !strcmp(c->cells[39].text,"9007199254740995"));
        CHECK(p->sql && strstr(p->sql,"PIVOT (") && strstr(p->sql,"NULL AS \"category_0\"") && strstr(p->sql,"'quote_\"''_SUM' AS \"category_5\"") && p->bytes<=4*1024*1024);
        release_reply(); pass("real Columns PIVOT preserves typed NULL/empty/literal/case/quote categories, DECIMAL/wide integers and raw AVG/distinct totals");
        submit_columns(COLUMNS_GRAND,0,-1,true,6); enter(WAIT_COLUMNS_GRAND); return;
    }
    case WAIT_COLUMNS_GRAND:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==36 && reply->page.total==1 && reply->page.totals && !strcmp(reply->page.totals->rows.cells[0].text,"350.00") && !strcmp(reply->page.totals->rows.cells[2].text,"3") && !strcmp(reply->page.totals->columns.cells[36].text,"350.00")); release_reply(); pass("Columns without Rows uses one real PIVOT row and independent source grand measures");
        submit_columns(COLUMNS_EMPTY,0,-1,true,6); enter(WAIT_COLUMNS_EMPTY); return;
    case WAIT_COLUMNS_EMPTY:
        if (!received(false)) return;
        CHECK(!reply->page.row_count && reply->page.column_count==5 && !reply->page.total && reply->page.totals && !reply->page.totals->has_columns && !reply->page.columns[1].category_count && !strcmp(reply->page.columns[1].caption,"SUM(v)") && reply->page.totals->columns.column_count==4 && reply->page.totals->columns.cells[0].is_null && !strcmp(reply->page.totals->columns.cells[2].text,"0")); release_reply(); pass("empty Columns domains use explicit NULL ordinals and preserve empty source aggregate semantics");
        submit_columns(COLUMNS_DATE,0,-1,true,7); enter(WAIT_COLUMNS_DATE); return;
    case WAIT_COLUMNS_DATE:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==15 && reply->page.totals && reply->page.totals->columns.column_count==16);
        CHECK(reply->page.columns[7].category_count==2 && reply->page.columns[7].category_grains[0]==PIVOT_DATE_MONTH && reply->page.columns[7].category_grains[1]==PIVOT_DATE_QUARTER && !strcmp(reply->page.columns[7].categories[0],"2025-01-01") && !strcmp(reply->page.columns[7].categories[1],"Q1") && !strcmp(reply->page.cells[7].text,"5") && !strcmp(reply->page.totals->columns.cells[15].text,"9")); release_reply(); pass("two date-grain Columns use Cartesian ordinal metadata and real COUNT(*) categories");
        submit_columns(COLUMNS_RANGE,0,-1,true,6); enter(WAIT_COLUMNS0); return;
    case WAIT_COLUMNS0:
        if (!received(false)) return;
        columns_range_values(0,256,true); release_reply(); pass("600-row Columns PIVOT pages and source row/category/grand readouts publish atomically");
        submit_columns(COLUMNS_RANGE,256,600,true,4); enter(WAIT_COLUMNS256); return;
    case WAIT_COLUMNS256:
        if (!received(false)) return;
        columns_range_values(256,256,true); release_reply(); pass("Columns second page has no redundant count or DESCRIBE");
        submit_columns(COLUMNS_RANGE,512,600,true,4); enter(WAIT_COLUMNS512); return;
    case WAIT_COLUMNS512:
        if (!received(false)) return;
        columns_range_values(512,88,true); release_reply(); pass("Columns final interval retains correctly aligned source row totals");
        submit_columns(COLUMNS_RANGE,256,600,false,2); enter(WAIT_COLUMNS_NO_TOTALS); return;
    case WAIT_COLUMNS_NO_TOTALS:
        if (!received(false)) return;
        columns_range_values(256,256,false); release_reply(); pass("Columns without totals retains typed headers and issues no readout SQL");
        submit_columns(COLUMNS_WIDE,0,-1,true,6); enter(WAIT_COLUMNS_WIDE); return;
    case WAIT_COLUMNS_WIDE: {
        if (!received(false)) return;
        const char *keys[]={"2.00","10.00","9007199254740993.00","9007199254740994.00"},*values[]={"9007199254740993","9007199254740994","9007199254740993","9007199254740994"};
        CHECK(reply->page.row_count==1 && reply->page.column_count==4 && reply->page.totals);
        for (size_t c=0;c<4;c++) CHECK(!strcmp(reply->page.columns[c].categories[0],keys[c]) && !strcmp(reply->page.cells[c].text,values[c]) && reply->page.columns[c].type==DUCKDB_TYPE_HUGEINT);
        CHECK(!strcmp(reply->page.totals->rows.cells[0].text,"36028797018963974") && !strcmp(reply->page.totals->columns.cells[4].text,"36028797018963974"));
        release_reply(); pass("DECIMAL ON values above 2^53 retain exact typed domains and SUM(BIGINT) remains HUGEINT");
        submit_columns(COLUMNS_BOUND,0,-1,true,6); enter(WAIT_COLUMNS_BOUND); return;
    }
    case WAIT_COLUMNS_BOUND:
        if (!received(false)) return;
        CHECK(reply->page.column_count==128 && reply->page.row_count==1 && reply->page.totals && reply->page.totals->columns.column_count==129 && !strcmp(reply->page.totals->columns.cells[128].text,"8128.00"));
        for (size_t c=0;c<128;c++) { char key[32],amount[32]; snprintf(key,sizeof(key),"%zu",c); snprintf(amount,sizeof(amount),"%zu.00",c); CHECK(reply->page.columns[c].category_count==1 && !strcmp(reply->page.columns[c].categories[0],key) && !strcmp(reply->page.cells[c].text,amount)); }
        release_reply(); pass("exactly 128 numeric categories preserve typed ordering and ordinal aliases beyond category_9");
        submit_columns(COLUMNS_PRODUCT,0,-1,true,2); enter(WAIT_COLUMNS_PRODUCT); return;
    case WAIT_COLUMNS_PRODUCT:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"128 result columns")); validation_reply(); release_reply(); pass("multi-field Cartesian product rejects before result or readout SQL");
        submit_columns(COLUMNS_SQL_LIMIT,0,-1,true,1); enter(WAIT_COLUMNS_SQL_LIMIT); return;
    case WAIT_COLUMNS_SQL_LIMIT:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"1 MiB")); validation_reply(); release_reply(); pass("escaped domain SQL cap rejects before any data page is staged");
        submit_columns(COLUMNS_LARGE,0,-1,true,5); enter(WAIT_COLUMNS_LARGE); return;
    case WAIT_COLUMNS_LARGE:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"4 MiB")); validation_reply(); release_reply(); pass("combined Columns page and source row readouts reject without partial publication");
        hold_matrix_reply(3); submit_columns(COLUMNS_RANGE,0,600,true,1); enter(WAIT_COLUMNS_CANCEL_DOMAIN); return;
    case WAIT_COLUMNS_CANCEL_DOMAIN:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); pass("cancelled real domain callback cannot resurrect its plan or publish a page");
        hold_matrix_reply(4); submit_columns(COLUMNS_RANGE,256,600,true,4); enter(WAIT_COLUMNS_CANCEL_ROWS); return;
    case WAIT_COLUMNS_CANCEL_ROWS:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); pass("cancel during source row readouts discards the staged Columns page");
        hold_matrix_reply(5); submit_columns(COLUMNS_RANGE,512,600,true,4); enter(WAIT_COLUMNS_CANCEL_FOOTER); return;
    case WAIT_COLUMNS_CANCEL_FOOTER:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); pass("cancel after category/grand engine output frees staged row readouts and rejects the late footer");
        submit_columns(COLUMNS_RANGE,512,600,true,4); enter(WAIT_COLUMNS_FINAL); return;
    case WAIT_COLUMNS_FINAL:
        if (!received(false)) return;
        columns_range_values(512,88,true); release_reply(); pass("Columns cancellation/memory-error recovery publishes only the current fully owned result");
        submit_matrix(MATRIX_COUNT,0,-1,true,5); enter(WAIT_MATRIX_COUNT); return;
    case WAIT_MATRIX_COUNT:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==1 && reply->page.total==1 && !strcmp(reply->page.cells[0].text,"4") && reply->page.columns[0].type==DUCKDB_TYPE_BIGINT && reply->page.totals && !strcmp(reply->page.totals->columns.cells[0].text,"4") && reply->page.sql && !strstr(reply->page.sql,"PIVOT (")); release_reply(); pass("global COUNT(*) counts nullable records without an implicit grouping column");
        submit_matrix(MATRIX_COUNT_EMPTY,0,-1,true,4); enter(WAIT_MATRIX_COUNT_EMPTY); return;
    case WAIT_MATRIX_COUNT_EMPTY:
        if (!received(false)) return;
        CHECK(reply->page.row_count==1 && reply->page.column_count==1 && reply->page.total==1 && !strcmp(reply->page.cells[0].text,"0") && reply->page.totals && !strcmp(reply->page.totals->columns.cells[0].text,"0")); release_reply(); pass("empty global COUNT(*) produces one typed zero row and a source zero footer");
        submit_statistics(STAT_RANGE,2); enter(WAIT_STATS_RANGE); return;
    case WAIT_STATS_RANGE:
        if (!received(false)) return;
        statistics_values(600,600,600,100); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"29700") && !strcmp(reply->statistics.average,"49.5") && !strcmp(reply->statistics.minimum,"0") && !strcmp(reply->statistics.maximum,"99"));
        retained_statistics=reply->statistics; reply->statistics=(SelectionStats){0}; release_reply(); pass("direct statistics aggregate all 600 selected source rows with an independently owned exact result");
        submit_statistics(STAT_OFFSET,1); enter(WAIT_STATS_OFFSET); return;
    case WAIT_STATS_OFFSET:
        if (!received(false)) return;
        statistics_values(1032,1032,688,788); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"165320") && !strcmp(reply->statistics.minimum,"0") && !strcmp(reply->statistics.maximum,"599")); release_reply(); pass("statistics OFFSET spans beyond the loaded page, clips to source extent and preserves typed distinct counts");
        submit_statistics(STAT_EXACT,2); enter(WAIT_STATS_EXACT); return;
    case WAIT_STATS_EXACT:
        if (!received(false)) return;
        statistics_values(9,6,4,5); CHECK(!reply->statistics.approximate && !reply->statistics.non_finite && !strcmp(reply->statistics.sum,"340282366920938463463374607431768211453.99999999999999999999999999999999999999") && !strcmp(reply->statistics.maximum,"170141183460469231731687303715884105727")); release_reply(); pass("scale38 BIGNUM statistics preserve sums beyond128 bits and SQL NULL versus empty/literal text");
        submit_statistics(STAT_TINY,1); enter(WAIT_STATS_TINY); return;
    case WAIT_STATS_TINY:
        if (!received(false)) return;
        statistics_values(3,2,2,2); CHECK(!strcmp(reply->statistics.sum,"-0.00000000000000000000000000000000000001") && !strcmp(reply->statistics.minimum,"-0.00000000000000000000000000000000000002") && !strcmp(reply->statistics.maximum,"0.00000000000000000000000000000000000001")); release_reply(); pass("38-digit fractional statistics use exact text rather than a double round trip");
        submit_statistics(STAT_TEXT,2); enter(WAIT_STATS_TEXT); return;
    case WAIT_STATS_TEXT:
        if (!received(false)) return;
        statistics_values(5,4,0,3); CHECK(!reply->statistics.sum && !reply->statistics.average && !reply->statistics.minimum && !reply->statistics.approximate); release_reply(); pass("text-only statistics count empty and literal NULL as present independently from SQL NULL");
        submit_statistics(STAT_FILTERED,3); enter(WAIT_STATS_FILTERED); return;
    case WAIT_STATS_FILTERED:
        if (!received(false)) return;
        statistics_values(2,2,2,2); CHECK(reply->analysis.filter_number.kind==NUM_DECIMAL && reply->analysis.filter_number.scale==2 && !reply->statistics.approximate && !strcmp(reply->statistics.sum,"2.03") && !strcmp(reply->statistics.average,"1.015") && !strcmp(reply->statistics.minimum,"1.01") && !strcmp(reply->statistics.maximum,"1.02")); release_reply(); pass("statistics resolve exact decimal predicates before selecting and aggregating source records");
        submit_statistics(STAT_OVERFLOW,2); enter(WAIT_STATS_OVERFLOW); return;
    case WAIT_STATS_OVERFLOW:
        if (!received(false)) return;
        statistics_values(2,2,2,1); CHECK(reply->statistics.approximate && !reply->statistics.non_finite && reply->statistics.sum_overflow && reply->statistics.average_overflow && !reply->statistics.sum && !reply->statistics.average && !strcmp(reply->statistics.minimum,"1e+308")); release_reply(); pass("finite-input sum/average overflow has separate flags and retains finite extrema");
        submit_statistics(STAT_NONFINITE,2); enter(WAIT_STATS_NONFINITE); return;
    case WAIT_STATS_NONFINITE:
        if (!received(false)) return;
        statistics_values(4,3,3,3); CHECK(reply->statistics.approximate && reply->statistics.non_finite && !reply->statistics.sum && !reply->statistics.average && !reply->statistics.minimum && !reply->statistics.maximum && !reply->statistics.sum_overflow); release_reply(); pass("NaN and infinities mark nonfinite input and suppress numeric readouts without overflow spoofing");
        submit_statistics(STAT_ZERO,2); enter(WAIT_STATS_ZERO); return;
    case WAIT_STATS_ZERO:
        if (!received(false)) return;
        statistics_values(2,2,2,1); CHECK(reply->statistics.approximate && !strcmp(reply->statistics.sum,"0") && !strcmp(reply->statistics.average,"0")); release_reply(); pass("floating negative zero and positive zero share one canonical typed distinct value");
        submit_statistics(STAT_EMPTY,1); enter(WAIT_STATS_EMPTY); return;
    case WAIT_STATS_EMPTY:
        if (!received(false)) return;
        statistics_values(0,0,0,0); CHECK(!reply->statistics.sum && !reply->statistics.average && !reply->statistics.approximate); release_reply(); pass("empty selections return exact zero counts with NULL aggregates");
        submit_statistics(STAT_OBJECT,0); enter(WAIT_STATS_OBJECT); return;
    case WAIT_STATS_OBJECT:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"field to Values")); statistics_error(); release_reply(); pass("analytical statistics reject an incomplete object definition before SQL");
        submit_statistics(STAT_PIVOT,0); enter(WAIT_STATS_PIVOT); return;
    case WAIT_STATS_PIVOT:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"object identity")); statistics_error(); release_reply(); pass("pivot statistics require a real object identity before SQL");
        submit_statistics(STAT_MIXED,0); enter(WAIT_STATS_MIXED); return;
    case WAIT_STATS_MIXED:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"snapshot bounds")); statistics_error(); release_reply(); pass("malformed owned mixed-selection snapshots reject and release before any browser SQL");
        submit_statistics(STAT_STAMP,0); enter(WAIT_STATS_STAMP); return;
    case WAIT_STATS_STAMP:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"Native source stamps")); statistics_error(); release_reply(); pass("unverifiable native source stamps fail closed before browser statistics SQL");
        submit_statistics(STAT_BOUNDS,0); enter(WAIT_STATS_BOUNDS); return;
    case WAIT_STATS_BOUNDS:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"inside the worksheet")); statistics_error(); release_reply(); pass("overflowing worksheet statistics extents reject before SQL");
        submit_statistics(STAT_COLUMN,0); enter(WAIT_STATS_COLUMN); return;
    case WAIT_STATS_COLUMN:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"described columns")); statistics_error(); release_reply(); pass("cached-schema field overflow rejects before an aggregate query or output mutation");
        hold_matrix_reply(6); submit_statistics(STAT_RANGE,2); enter(WAIT_STATS_CANCEL); return;
    case WAIT_STATS_CANCEL:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); retained_values(); pass("cancelled real statistics callbacks cannot publish obsolete scalars or corrupt prior owned results");
        submit_statistics(STAT_FINAL,1); enter(WAIT_STATS_FINAL); return;
    case WAIT_STATS_FINAL:
        if (!received(false)) return;
        statistics_values(100,100,100,100); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"4950") && !strcmp(reply->statistics.average,"49.5")); release_reply(); pass("statistics cancellation recovery reuses schema and publishes only the current full interval");
        submit_statistics(STAT_TABLE,1); enter(WAIT_STATS_TABLE); return;
    case WAIT_STATS_TABLE:
        if (!received(false)) return;
        statistics_values(600,600,600,100); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"29700") && !strcmp(reply->statistics.average,"49.5")); release_reply(); pass("direct table statistics reuse its full typed relation and preserve bound object/revision identity");
        submit_statistics(STAT_ROWS,2); enter(WAIT_STATS_ROWS); return;
    case WAIT_STATS_ROWS:
        if (!received(false)) return;
        statistics_values(600,600,600,100); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"29700") && !strcmp(reply->statistics.average,"49.5")); release_reply(); pass("Rows result statistics aggregate600 actual pivot cells without a Page/count/totals submission");
        submit_statistics(STAT_ROWS_AVG,2); enter(WAIT_STATS_ROWS_AVG); return;
    case WAIT_STATS_ROWS_AVG:
        if (!received(false)) return;
        statistics_values(5,5,5,5); CHECK(reply->statistics.approximate && !strcmp(reply->statistics.sum,"320") && !strcmp(reply->statistics.average,"64") && !strcmp(reply->statistics.minimum,"4.5") && !strcmp(reply->statistics.maximum,"200")); release_reply(); pass("statistics over AVG result cells average selected group averages rather than raw source values");
        submit_statistics(STAT_COLUMNS_AVG,3); enter(WAIT_STATS_COLUMNS_AVG); return;
    case WAIT_STATS_COLUMNS_AVG:
        if (!received(false)) return;
        statistics_values(45,10,10,8); CHECK(reply->statistics.approximate && !strcmp(reply->statistics.sum,"348") && !strcmp(reply->statistics.average,"34.8") && !strcmp(reply->statistics.minimum,"2") && !strcmp(reply->statistics.maximum,"200")); release_reply(); pass("bounded real Columns PIVOT statistics preserve sparse NULL intersections and result-cell semantics");
        submit_statistics(STAT_COLUMNS_SUBSET,2); enter(WAIT_STATS_COLUMNS_SUBSET); return;
    case WAIT_STATS_COLUMNS_SUBSET:
        if (!received(false)) return;
        statistics_values(18,7,7,5); CHECK(reply->statistics.approximate && !strcmp(reply->statistics.sum,"38") && strtod(reply->statistics.average,NULL)>5.42 && strtod(reply->statistics.average,NULL)<5.43 && !strcmp(reply->statistics.minimum,"2") && !strcmp(reply->statistics.maximum,"11")); release_reply(); pass("Columns result OFFSET statistics recompute the selected full interval without cached-page sampling");
        submit_statistics(STAT_COLUMNS_DATE,5); enter(WAIT_STATS_COLUMNS_DATE); return;
    case WAIT_STATS_COLUMNS_DATE:
        if (!received(false)) return;
        statistics_values(15,15,15,3); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"9") && !strcmp(reply->statistics.average,"0.6") && !strcmp(reply->statistics.minimum,"0") && !strcmp(reply->statistics.maximum,"5")); release_reply(); pass("date-domain COUNT(*) result statistics retain zero intersections and typed result descriptors");
        submit_statistics(STAT_GRAND,2); enter(WAIT_STATS_GRAND); return;
    case WAIT_STATS_GRAND:
        if (!received(false)) return;
        statistics_values(1,1,1,1); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"13") && !strcmp(reply->statistics.average,"13")); release_reply(); pass("scalar analytical-result statistics measure the aggregate result row");
        submit_statistics(STAT_TABLE_BAD,0); enter(WAIT_STATS_TABLE_BAD); return;
    case WAIT_STATS_TABLE_BAD:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"object identity")); statistics_error(); release_reply(); pass("a table statistics flag without an owner rejects before SQL");
        hold_matrix_reply(6); submit_statistics(STAT_COLUMNS_AVG,3); enter(WAIT_STATS_ANALYSIS_CANCEL); return;
    case WAIT_STATS_ANALYSIS_CANCEL:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); submit_statistics(STAT_COLUMNS_NEW_REVISION,2); release_matrix_reply(); CHECK(worker.running && !worker_poll(&worker)); pass("late analytical statistics from an old object revision cannot replace the active newer request"); enter(WAIT_STATS_ANALYSIS_FINAL); return;
    case WAIT_STATS_ANALYSIS_FINAL:
        if (!received(false)) return;
        statistics_values(18,7,7,5); CHECK(reply->revision==revision_id+4 && !strcmp(reply->statistics.sum,"38")); release_reply(); pass("analytical statistics recovery publishes the current object revision with independently owned results");
        submit_mixed(MIX_BLANK,1); enter(WAIT_MIX_BLANK); return;
    case WAIT_MIX_BLANK:
        if (!received(false)) return;
        statistics_values(DX_SHEET_ROWS*UINT64_C(16384),0,0,0); CHECK(!reply->statistics.sum && !reply->statistics.average); release_reply(); pass("blank-only statistics preserve the exact full worksheet logical area without enumerating sparse NULL cells");
        submit_mixed(MIX_MANUAL,1); enter(WAIT_MIX_MANUAL); return;
    case WAIT_MIX_MANUAL:
        if (!received(false)) return;
        statistics_values(20,5,0,4); CHECK(!reply->statistics.sum && !reply->statistics.approximate); release_reply(); pass("manual-only labels keep empty, literal NULL and numeric-looking VARCHAR distinct from sparse missing cells");
        submit_mixed(MIX_TEXT,2); enter(WAIT_MIX_TEXT); return;
    case WAIT_MIX_TEXT:
        if (!received(false)) return;
        statistics_values(20,11,0,4); CHECK(!reply->statistics.sum); release_reply(); pass("one global typed DISTINCT deduplicates equal text across primary, table and manual owners");
        submit_mixed(MIX_EXACT,2); enter(WAIT_MIX_EXACT); return;
    case WAIT_MIX_EXACT:
        if (!received(false)) return;
        statistics_values(20,13,8,6); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"680564733841876926926749214863536422907.99999999999999999999999999999999999998") && !strcmp(reply->statistics.minimum,"-0.00000000000000000000000000000000000002") && !strcmp(reply->statistics.maximum,"170141183460469231731687303715884105727")); release_reply(); pass("mixed HUGEINT and scale38 DECIMAL stay exact and deduplicate typed values across owners");
        submit_mixed(MIX_OBJECTS,6); enter(WAIT_MIX_OBJECTS); return;
    case WAIT_MIX_OBJECTS:
        if (!received(false)) return;
        statistics_values(700,626,626,120); CHECK(reply->statistics.approximate && !strcmp(reply->statistics.sum,"30718") && !strcmp(reply->statistics.minimum,"0") && !strcmp(reply->statistics.maximum,"200") && strtod(reply->statistics.average,NULL)>49.07 && strtod(reply->statistics.average,NULL)<49.08); release_reply(); pass("mixed full600-row source, table, Rows and actual Columns PIVOT cells share one global numeric aggregate");
        submit_mixed(MIX_DATE,7); enter(WAIT_MIX_DATE); return;
    case WAIT_MIX_DATE:
        if (!received(false)) return;
        statistics_values(32,19,17,7); CHECK(!reply->statistics.approximate && !strcmp(reply->statistics.sum,"11.03") && !strcmp(reply->statistics.minimum,"0") && !strcmp(reply->statistics.maximum,"5")); release_reply(); pass("mixed date-grain COUNT intersections and exact filtered table cells retain their independent typed schemas");
        submit_mixed(MIX_INVALID,0); enter(WAIT_MIX_INVALID); return;
    case WAIT_MIX_INVALID:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"range inside the worksheet")); statistics_error(); release_reply(); pass("an invalid later source reference rejects the whole snapshot before even its first range SQL");
        submit_mixed(MIX_STAMP,0); enter(WAIT_MIX_STAMP); return;
    case WAIT_MIX_STAMP:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"Native source stamps")); statistics_error(); release_reply(); pass("unverifiable range source stamps fail before mixed browser SQL");
        submit_mixed(MIX_PRIMARY_BAD,0); enter(WAIT_MIX_PRIMARY_BAD); return;
    case WAIT_MIX_PRIMARY_BAD:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"Invalid browser source definition")); statistics_error(); release_reply(); pass("owned primary Analysis with malformed nested counts rejects and cleans every allocated fixed-array slot");
        submit_mixed(MIX_SOURCE_BAD,0); enter(WAIT_MIX_SOURCE_BAD); return;
    case WAIT_MIX_SOURCE_BAD:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"Invalid browser source definition")); statistics_error(); release_reply(); pass("owned source snapshots with malformed nested counts reject without SQL or unsafe cleanup");
        submit_mixed(MIX_BUDGET,0); enter(WAIT_MIX_BUDGET); return;
    case WAIT_MIX_BUDGET:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"4 MiB")); statistics_error(); release_reply(); pass("actual owned source-definition bytes reject oversized mixed snapshots before SQL");
        submit_mixed(MIX_SQL,0); enter(WAIT_MIX_SQL); return;
    case WAIT_MIX_SQL:
        if (!poll_received(true,false)) return;
        CHECK(strstr(reply->error,"1 MiB")); statistics_error(); release_reply(); pass("escaped manual-label SQL respects the1MiB cap before an engine query");
        submit_mixed(MIX_CUMULATIVE,1); enter(WAIT_MIX_CUMULATIVE); return;
    case WAIT_MIX_CUMULATIVE:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"1 MiB")); statistics_error(); release_reply(); pass("cumulative typed range SQL rejects atomically after schema planning without publishing partial statistics");
        submit_mixed(MIX_ENGINE,2); enter(WAIT_MIX_ENGINE); return;
    case WAIT_MIX_ENGINE:
        if (!received(true)) return;
        CHECK(strstr(reply->error,"missing_mixed_table")); statistics_error(); release_reply(); pass("a later range engine error discards all earlier staged cell SQL and emits no partial aggregate");
        hold_matrix_reply(3); submit_mixed(MIX_OBJECTS,3); enter(WAIT_MIX_CANCEL_DOMAIN); return;
    case WAIT_MIX_CANCEL_DOMAIN:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); retained_values(); pass("cancelled mixed Columns domain callbacks cannot publish or retain staged earlier ranges");
        hold_matrix_reply(6); submit_mixed(MIX_OBJECTS,6); enter(WAIT_MIX_CANCEL_SCALAR); return;
    case WAIT_MIX_CANCEL_SCALAR:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); submit_mixed(MIX_MANUAL,1); release_matrix_reply(); CHECK(worker.running && !worker_poll(&worker)); pass("a late mixed aggregate callback is ignored while a newer owned snapshot is active"); enter(WAIT_MIX_FINAL); return;
    case WAIT_MIX_FINAL:
        if (!received(false)) return;
        statistics_values(20,5,0,4); CHECK(!reply->statistics.sum); release_reply(); pass("mixed cancellation and local/engine errors recover with an independent current scalar");
        copy_case=COPY_EXACT; submit_copy(copy_case); enter(WAIT_COPY_CASES); return;
    case WAIT_COPY_CASES: {
        bool error=copy_case_error(copy_case);
        if (!poll_received(error,copy_sql_counts[copy_case]!=0)) return;
        if (error) { CHECK(reply->error && !reply->clipboard); validation_reply(); CHECK(!reply->statistics.sum); }
        else copy_values(copy_case);
        if (copy_case==COPY_EXACT) { retained_clipboard=reply->clipboard; reply->clipboard=NULL; }
        release_reply(); pass(copy_case_names[copy_case]);
        if (++copy_case<COPY_CASES) { submit_copy(copy_case); return; }
        hold_matrix_reply(7); submit_copy(COPY_MIXED); enter(WAIT_COPY_CANCEL); return;
    }
    case WAIT_COPY_CANCEL:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); release_matrix_reply(); quiet(); retained_values();
        pass("cancel after earlier real copy patches discards every staged cell and the late Columns payload");
        hold_matrix_reply(7); submit_copy(COPY_MIXED); enter(WAIT_COPY_STALE); return;
    case WAIT_COPY_STALE:
        if (!matrix_reply_held()) { CHECK(!reply); reply=worker_poll(&worker); CHECK(!reply && worker.running); return; }
        counters(); worker_cancel(&worker); CHECK(!worker.running); submit_copy(COPY_EXACT); release_matrix_reply();
        CHECK(worker.running && !worker_poll(&worker)); retained_values();
        pass("obsolete copy callbacks cannot publish over a newer owned request or retained clipboard"); enter(WAIT_COPY_RECOVERY); return;
    case WAIT_COPY_RECOVERY:
        if (!received(false)) return;
        copy_values(COPY_EXACT); CHECK(reply->clipboard!=retained_clipboard); release_reply();
        pass("copy cancellation and late failures recover with one independently owned exact TSV");
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
        page_free(&retained); page_free(&retained); statistics_free(&retained_statistics); statistics_free(&retained_statistics); pass("early stop and scheduled stale callbacks are ignored");
        CHECK(stages==139 && worker.jobs_started==146 && expected_sql==325 && expected_counts==21);
        free(retained_clipboard); retained_clipboard=NULL;
        baseline_jobs=worker.jobs_started; baseline_sql=worker.sql_executed; baseline_counts=worker.counts_executed;
        CHECK(reset_test_engine()); enter(WAIT_EXPORT_RESET); return;
    case WAIT_EXPORT_RESET: {
        int ready=export_reset_ready(); CHECK(ready>=0); if (!ready) return;
        CHECK(!worker.started && !worker.running && worker_start(&worker) && observe_host());
        expected_sql=expected_counts=0; export_case=EXPORT_PRIMARY; submit_export(export_case); enter(WAIT_EXPORT_CASES); return;
    }
    case WAIT_EXPORT_CASES: {
        if (!poll_received(export_case>=EXPORT_STAMP,export_sql_counts[export_case]!=0)) return;
        export_result_values(export_case);
        if (export_case<EXPORT_STAMP) {
            char name[64]; snprintf(name,sizeof(name),"worker-export-reopen-%u.parquet",(unsigned)export_case);
            register_export_bytes(name,reply->export_data,reply->export_bytes);
            if (export_case==EXPORT_PRIMARY) { retained_export=reply->export_data; retained_export_size=reply->export_bytes; reply->export_data=NULL; reply->export_bytes=0; }
            release_reply(); pass(export_names[export_case]); enter(WAIT_EXPORT_REGISTER); return;
        }
        release_reply(); pass(export_names[export_case]); submit_next_export(); return;
    }
    case WAIT_EXPORT_REGISTER: {
        int ready=export_registration_ready(); CHECK(ready>=0); if (!ready) { quiet(); CHECK(!worker.running); return; }
        submit_export_readback(export_case==EXPORT_PRIMARY ? 257 : 0,2); enter(WAIT_EXPORT_READBACK); return;
    }
    case WAIT_EXPORT_READBACK:
        if (!received(false)) return;
        export_readback_values(export_case); release_reply(); pass("real shared-C reimport preserves the exported Parquet schema, complete relation and exact values");
        if (export_case==EXPORT_PRIMARY) { submit_export_readback(512,1); enter(WAIT_EXPORT_LAST); return; }
        submit_next_export(); return;
    case WAIT_EXPORT_LAST:
        if (!received(false)) return;
        export_readback_values(EXPORT_PRIMARY); release_reply(); pass("exported source includes the final off-page rows and retains independent original bytes"); submit_next_export(); return;
    case WAIT_EXPORT_SOURCE_REGISTER: {
        int ready=export_registration_ready(); CHECK(ready>=0); if (!ready) { quiet(); CHECK(!worker.running); return; }
        submit_export(export_case); enter(WAIT_EXPORT_CASES); return;
    }
    case WAIT_EXPORT_CANCEL:
        if (!held_export_bytes()) { CHECK(!reply && worker.running); quiet(); return; }
        CHECK(held_export_bytes()>=12); counters(); stale_cookie=observed_cookie(); stale_ticket=observed_ticket();
        CHECK(!dx_browser_export_result(stale_cookie+1,stale_ticket,(const void *)(uintptr_t)1,SIZE_MAX,(unsigned char *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1));
        CHECK(!dx_browser_export_result(stale_cookie,stale_ticket+1,(const void *)(uintptr_t)1,SIZE_MAX,(unsigned char *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1));
        dx_browser_query_result(stale_cookie,stale_ticket,(const void *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1,1,(const char *)(uintptr_t)1);
        CHECK(worker.running); quiet(); worker_cancel(&worker); CHECK(!worker.running); CHECK(!release_export_reply()); quiet();
        CHECK(retained_export_size>=12 && !memcmp(retained_export,"PAR1",4)); pass("cancelled export and wrong cookie/ticket/callback cannot adopt or publish late binary allocations");
        hold_export_reply(); submit_export(EXPORT_COLUMNS); enter(WAIT_EXPORT_STALE); return;
    case WAIT_EXPORT_STALE:
        if (!held_export_bytes()) { CHECK(!reply && worker.running); quiet(); return; }
        counters(); worker_cancel(&worker); submit(JOB_PAGE,rejection_recovery_sql,0,1,false,2);
        CHECK(!dx_browser_export_result(observed_cookie(),observed_ticket(),(const void *)(uintptr_t)1,SIZE_MAX,(unsigned char *)(uintptr_t)1,SIZE_MAX,(const char *)(uintptr_t)1));
        CHECK(!release_export_reply() && worker.running && !worker_poll(&worker)); pass("obsolete export bytes cannot enter a newer non-export stage or replace retained independent data"); enter(WAIT_EXPORT_RECOVERY); return;
    case WAIT_EXPORT_RECOVERY:
        if (!received(false)) return;
        short_reply("recovered after local rejection"); CHECK(!reply->export_data && !reply->export_bytes && retained_export_size>=12 && !memcmp(retained_export,"PAR1",4));
        release_reply(); pass("binary faults and cancellation recover through ordinary current shared-C queries"); verify_export_source(); enter(WAIT_EXPORT_SOURCE_VERIFY); return;
    case WAIT_EXPORT_SOURCE_VERIFY: {
        int verified=export_source_verified(); CHECK(verified>=0); if (!verified) { quiet(); CHECK(!worker.running); return; }
        CHECK(!memcmp(retained_export,"PAR1",4) && !memcmp(retained_export+retained_export_size-4,"PAR1",4)); pass("registered CSV source bytes remain unchanged after filtered exports, readback and binary failures");
        stale_cookie=observed_cookie(); stale_ticket=observed_ticket(); worker_stop(&worker); inject_late_export(NULL); emscripten_async_call(inject_late_export,NULL,50); enter(QUIET_EXPORT_STOP); return;
    }
    case QUIET_EXPORT_STOP:
        quiet(); if (emscripten_get_now()-entered<150) return;
        CHECK(export_late_callbacks==2 && !worker.started && !worker.running && !worker.reply); counters();
        pass("second sequential Worker teardown rejects scheduled stale binary callbacks and frees owned export state");
        CHECK(stages==184 && worker.jobs_started==43 && expected_sql==74 && expected_counts==0);
        free(retained_export); retained_export=NULL; retained_export_size=0;
        printf("Browser worker workloads: {\"baseline\":{\"jobs\":%" PRIu64 ",\"sql\":%" PRIu64 ",\"counts\":%" PRIu64 "},\"export\":{\"jobs\":%" PRIu64 ",\"sql\":%" PRIu64 ",\"counts\":%" PRIu64 "},\"sequential_workers\":2}\n",baseline_jobs,baseline_sql,baseline_counts,worker.jobs_started,worker.sql_executed,worker.counts_executed);
        enter(FINISHED); completed(1,checks,stages); printf("PASS browser-worker complete: %u checks, %u stages, %" PRIu64 " SQL, %" PRIu64 " counts\n",checks,stages,baseline_sql+worker.sql_executed,baseline_counts+worker.counts_executed);
        emscripten_cancel_main_loop(); return;
    case FINISHED: die("main loop ran after completion",__LINE__);
    }
}
int main(void) {
    CHECK(observe_host()); CHECK(worker_start(&worker)); submit(JOB_PAGE,exact_sql,0,-1,false,2); enter(WAIT_EXACT);
    emscripten_set_main_loop(tick,0,1); return 0;
}
