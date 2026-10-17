#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#include "data/worker.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef DUXCEL_TEST_SOURCE_DIGEST
#error Compile this focused test and its private worker object with DUXCEL_TEST_SOURCE_DIGEST.
#endif
static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { fprintf(stderr,"FAIL worker source digest:%d: %s\n",__LINE__,#condition); exit(1); } } while (0)
/* The verifier's real cancellation callback runs AFTER this isolated barrier.
   The hook never changes latest_id, returns a fake digest or touches DuckDB. */
static struct {
    DxMutex mutex; DxCond condition; QueryWorker *worker;
    uint64_t id,threshold,progress; unsigned visits;
    bool armed,entered,released;
} checkpoint;
void dx_test_source_digest_checkpoint(QueryWorker *worker,uint64_t bytes) {
    dx_lock(&checkpoint.mutex);
    if (checkpoint.worker==worker && checkpoint.id==worker->active_id) {
        checkpoint.visits++;
        if (checkpoint.armed && !checkpoint.entered && bytes>=checkpoint.threshold) {
            checkpoint.entered=true; checkpoint.progress=bytes; dx_signal(&checkpoint.condition);
            while (!checkpoint.released) dx_cond_wait(&checkpoint.condition,&checkpoint.mutex);
            checkpoint.armed=false;
        }
    }
    dx_unlock(&checkpoint.mutex);
}
static void arm(QueryWorker *worker,uint64_t id) {
    dx_lock(&checkpoint.mutex); checkpoint.worker=worker; checkpoint.id=id; checkpoint.threshold=DX_SOURCE_DIGEST_BUFFER_BYTES;
    checkpoint.progress=0; checkpoint.visits=0; checkpoint.armed=true; checkpoint.entered=checkpoint.released=false; dx_unlock(&checkpoint.mutex);
}
static void wait_checkpoint(void) {
    struct timespec deadline; CHECK(clock_gettime(CLOCK_REALTIME,&deadline)==0); deadline.tv_sec+=5; int status=0;
    dx_lock(&checkpoint.mutex); while (!checkpoint.entered && !status) status=pthread_cond_timedwait(&checkpoint.condition,&checkpoint.mutex,&deadline);
    bool entered=checkpoint.entered; uint64_t bytes=checkpoint.progress; dx_unlock(&checkpoint.mutex);
    CHECK(!status && entered && bytes==DX_SOURCE_DIGEST_BUFFER_BYTES);
}
static void release_checkpoint(void) { dx_lock(&checkpoint.mutex); checkpoint.released=true; dx_signal(&checkpoint.condition); dx_unlock(&checkpoint.mutex); }
static unsigned checkpoint_visits(void) { dx_lock(&checkpoint.mutex); unsigned visits=checkpoint.visits; dx_unlock(&checkpoint.mutex); return visits; }
static QueryReply *wait_reply(QueryWorker *worker) {
    double until=dx_millis()+5000;
    while (dx_millis()<until) { QueryReply *reply=worker_poll(worker); if (reply) return reply; dx_sleep_ms(1); }
    CHECK(!"verification reply timed out"); return NULL;
}
static void wait_idle(QueryWorker *worker) {
    double until=dx_millis()+5000;
    while (dx_millis()<until) { dx_lock(&worker->mutex); bool idle=!worker->running && !worker->pending; dx_unlock(&worker->mutex); if (idle) return; dx_sleep_ms(1); }
    CHECK(!"verification cancellation timed out");
}
static uint64_t jobs_started(QueryWorker *worker) { dx_lock(&worker->mutex); uint64_t jobs=worker->jobs_started; dx_unlock(&worker->mutex); return jobs; }
static void no_sql(QueryWorker *worker) {
    dx_lock(&worker->mutex); bool clean=!worker->db.connection && !worker->db.query_count && !worker->db.count_queries && !worker->sql_executed && !worker->counts_executed; dx_unlock(&worker->mutex);
    CHECK(clean);
}
static void write_file(const char *path,bool large) {
    FILE *file=fopen(path,"wb"); CHECK(file);
    if (large) { unsigned char block[4096]; memset(block,'a',sizeof(block)); for (size_t offset=0;offset<1000000;) { size_t n=1000000-offset; if (n>sizeof(block)) n=sizeof(block); CHECK(fwrite(block,1,n,file)==n); offset+=n; } }
    else CHECK(fwrite("abc",1,3,file)==3);
    CHECK(fclose(file)==0);
}
static const uint64_t owner=UINT64_C(18014398509481987),revision=UINT64_C(36028797018963971);
static uint64_t next_id=UINT64_C(9007199254740993);
static QueryJob *request(const char *path,uint64_t stamp) {
    QueryJob *job=dx_alloc(sizeof(*job)); job->kind=JOB_VERIFY_SOURCE; job->id=next_id++;
    job->path=path ? dx_strdup(path) : NULL; job->expected_source_stamp=stamp; job->object_id=owner; job->revision=revision; return job;
}
static void only_digest(const QueryReply *reply,uint64_t id,const char *path) {
    CHECK(reply->id==id && reply->kind==JOB_VERIFY_SOURCE && reply->object_id==owner && reply->revision==revision && !reply->is_preview);
    CHECK(path ? reply->path && !strcmp(reply->path,path) : !reply->path);
    CHECK(!reply->page.cells && !reply->page.columns && !reply->clipboard && !reply->workbook && !reply->distinct.values && !reply->statistics.sum && !reply->export_data && !reply->export_bytes && !reply->exported_rows && !reply->counted_rows);
}
static void success(QueryReply *reply,uint64_t id,const char *path,bool large) {
    only_digest(reply,id,path); CHECK(!reply->error && reply->source_digest_status==SOURCE_DIGEST_OK);
    CHECK(reply->source_digest.bytes==(large ? UINT64_C(1000000) : 3) && reply->source_digest.identity.size==reply->source_digest.bytes && reply->source_digest.observed_stamp==dx_file_stamp(path));
    const char *expected=large ? "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" : "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    char hex[65]; static const char digits[]="0123456789abcdef";
    for (size_t i=0;i<DX_SOURCE_DIGEST_BYTES;i++) { hex[i*2]=digits[reply->source_digest.sha256[i]>>4]; hex[i*2+1]=digits[reply->source_digest.sha256[i]&15]; } hex[64]=0; CHECK(!strcmp(hex,expected));
}
static void failed(QueryReply *reply,uint64_t id,const char *path,SourceDigestStatus status) {
    only_digest(reply,id,path); SourceDigest empty; memset(&empty,0,sizeof(empty));
    CHECK(reply->error && *reply->error && strlen(reply->error)<DX_SOURCE_DIGEST_ERROR_BYTES && reply->source_digest_status==status && !memcmp(&reply->source_digest,&empty,sizeof(empty)));
}
static void basic_and_payloads(QueryWorker *worker,const char *path,const char *missing) {
    for (unsigned unbound=0;unbound<2;unbound++) {
        QueryJob *job=request(path,unbound ? 0 : dx_file_stamp(path)); uint64_t id=job->id; job->known_total=unbound ? -1 : 0;
        worker_submit(worker,job); QueryReply *reply=wait_reply(worker); success(reply,id,path,true); reply_free(reply); no_sql(worker);
    }
    for (unsigned kind=0;kind<19;kind++) {
        QueryJob *job=request(kind ? path : NULL,0); uint64_t id=job->id;
        if (kind==1) { free(job->path); job->path=dx_strdup(missing); }
        if (kind==2) { free(job->path); job->path=dx_alloc(65538); memset(job->path,'x',65537); job->path[65537]=0; }
        if (kind==3) job->analysis.query=dx_strdup("Owned unrelated query");
        if (kind==4) { job->analysis.filter_count=SIZE_MAX; for (size_t i=0;i<sizeof(job->analysis.filters)/sizeof(*job->analysis.filters);i++) { job->analysis.filters[i].column=dx_strdup("Owned field"); job->analysis.filters[i].value=dx_strdup(""); } }
        if (kind==5) { job->pivot.counts[PIVOT_VALUES]=SIZE_MAX; for (size_t i=0;i<DX_PIVOT_FIELDS;i++) { job->pivot.fields[PIVOT_VALUES][i].name=dx_strdup("Owned measure"); job->pivot.fields[PIVOT_VALUES][i].value=dx_strdup(""); } }
        if (kind==6) { job->copy=dx_alloc(sizeof(*job->copy)); char *error=NULL; CHECK(clipboard_init(&job->copy->cells,1,1,&error) && clipboard_set(&job->copy->cells,0,0,"Owned copy",&error)); free(error); }
        if (kind==7) { job->statistics=dx_alloc(sizeof(*job->statistics)); char *error=NULL; CHECK(statistics_request_text(job->statistics,"Owned statistics",&error)); free(error); }
        if (kind==8) { job->workbook=dx_alloc(sizeof(*job->workbook)); workbook_init(job->workbook); }
        if (kind==9) { job->column=dx_strdup("Owned column"); job->search=dx_strdup("Owned search"); job->date_start=dx_strdup("Owned start"); job->date_end=dx_strdup("Owned end"); }
        if (kind==10) job->rows=1;
        if (kind==11) job->include_totals=true;
        if (kind==12) job->refresh_source=true;
        if (kind==13) job->known_total=1;
        if (kind==14) job->analysis.filter_number=(NumberType){NUM_I64,0,0};
        if (kind==15) job->is_table=true;
        if (kind==16) job->date_level=1;
        if (kind==17) {
            size_t last=sizeof(job->analysis.filters)/sizeof(*job->analysis.filters)-1;
            job->analysis.filters[0]=(AnalysisFilter){.column=dx_strdup("Owned zero-count first filter"),.value=dx_strdup("")};
            job->analysis.filters[last]=(AnalysisFilter){.column=dx_strdup("Owned zero-count last filter"),.value=dx_strdup("NULL")};
            CHECK(!job->analysis.filter_count);
        }
        if (kind==18) {
            size_t last=sizeof(job->analysis.sorts)/sizeof(*job->analysis.sorts)-1;
            job->analysis.sorts[0]=(AnalysisSort){.column=dx_strdup("Owned zero-count first sort")};
            job->analysis.sorts[last]=(AnalysisSort){.column=dx_strdup("Owned zero-count last sort"),.descending=true};
            CHECK(!job->analysis.sort_count);
        }
        char *owned_path=job->path ? dx_strdup(job->path) : NULL;
        arm(worker,id); worker_submit(worker,job); QueryReply *reply=wait_reply(worker); failed(reply,id,owned_path,SOURCE_DIGEST_ERROR);
        CHECK(!checkpoint_visits());
        if (kind==17 || kind==18) {
            CHECK(!reply->analysis.filter_count && !reply->analysis.sort_count);
            for (size_t i=0;i<sizeof(reply->analysis.filters)/sizeof(*reply->analysis.filters);i++) CHECK(!reply->analysis.filters[i].column && !reply->analysis.filters[i].value);
            for (size_t i=0;i<sizeof(reply->analysis.sorts)/sizeof(*reply->analysis.sorts);i++) CHECK(!reply->analysis.sorts[i].column);
        }
        reply_free(reply); free(owned_path); no_sql(worker);
    }
}
static void abandoned_analysis(QueryJob *job) {
    size_t filter=sizeof(job->analysis.filters)/sizeof(*job->analysis.filters)-1,sort=sizeof(job->analysis.sorts)/sizeof(*job->analysis.sorts)-1;
    job->analysis.filters[filter]=(AnalysisFilter){.column=dx_strdup("Owned abandoned filter"),.value=dx_strdup("")};
    job->analysis.sorts[sort]=(AnalysisSort){.column=dx_strdup("Owned abandoned sort")}; CHECK(!job->analysis.filter_count && !job->analysis.sort_count);
}
static void cancellation_and_latest(QueryWorker *worker,const char *large,const char *small,const char *missing) {
    QueryJob *job=request(large,dx_file_stamp(large)); uint64_t id=job->id; arm(worker,id); worker_submit(worker,job); wait_checkpoint();
    /* The helper is blocked on its own test condition, never the UI mutex. */
    CHECK(!worker_poll(worker)); no_sql(worker);
    QueryJob *pending=request(missing,0); abandoned_analysis(pending); worker_submit(worker,pending);
    worker_cancel(worker); CHECK(!worker_poll(worker)); release_checkpoint(); wait_idle(worker); CHECK(!worker_poll(worker));
    job=request(small,0); id=job->id; worker_submit(worker,job); QueryReply *reply=wait_reply(worker); success(reply,id,small,false); reply_free(reply); no_sql(worker);
    uint64_t started=jobs_started(worker); job=request(large,0); arm(worker,job->id); worker_submit(worker,job); wait_checkpoint();
    QueryJob *superseded=request(missing,0),*latest=request(small,dx_file_stamp(small)); id=latest->id; abandoned_analysis(superseded);
    worker_submit(worker,superseded); worker_submit(worker,latest); CHECK(!worker_poll(worker)); release_checkpoint();
    reply=wait_reply(worker); success(reply,id,small,false); CHECK(jobs_started(worker)==started+2); reply_free(reply); no_sql(worker);
}
static void changed_input(QueryWorker *worker,const char *path,const char *replacement) {
    uint64_t stamp=dx_file_stamp(path); FILE *file=fopen(path,"ab"); CHECK(file && fwrite("x",1,1,file)==1 && fclose(file)==0); CHECK(dx_file_stamp(path)!=stamp);
    QueryJob *job=request(path,stamp); uint64_t id=job->id; arm(worker,id); worker_submit(worker,job); QueryReply *reply=wait_reply(worker);
    failed(reply,id,path,SOURCE_DIGEST_CHANGED); CHECK(!checkpoint_visits()); reply_free(reply); no_sql(worker); write_file(path,true);
    for (unsigned unbound=0;unbound<2;unbound++) {
        write_file(replacement,false); job=request(path,unbound ? 0 : dx_file_stamp(path)); id=job->id; arm(worker,id); worker_submit(worker,job); wait_checkpoint();
        CHECK(rename(replacement,path)==0); release_checkpoint(); reply=wait_reply(worker); failed(reply,id,path,SOURCE_DIGEST_CHANGED); reply_free(reply); no_sql(worker); write_file(path,true);
    }
    job=request(path,dx_file_stamp(path)); id=job->id; worker_submit(worker,job); reply=wait_reply(worker); success(reply,id,path,true);
    SourceDigest retained=reply->source_digest; reply_free(reply); CHECK(retained.bytes==1000000 && retained.sha256[0]==0xcd); no_sql(worker);
}
static void *stop_thread(void *context) { worker_stop(context); return NULL; }
static void stop_while_verifying(QueryWorker *worker,const char *path) {
    QueryJob *job=request(path,0); arm(worker,job->id); worker_submit(worker,job); wait_checkpoint();
    DxThread stopper; CHECK(dx_thread_start(&stopper,stop_thread,worker)); double until=dx_millis()+5000; bool stopped=false;
    while (dx_millis()<until) { dx_lock(&worker->mutex); stopped=worker->stop; dx_unlock(&worker->mutex); if (stopped) break; dx_sleep_ms(1); }
    CHECK(stopped); release_checkpoint(); dx_thread_join(stopper);
    CHECK(!worker->started && !worker->running && !worker->pending && !worker->reply && !worker->db.connection && !worker->sql_executed && !worker->counts_executed);
}
int main(void) {
    CHECK(JOB_PAGE==0 && JOB_STEP_PREVIEW==12 && JOB_VERIFY_SOURCE==13);
    char directory[]="/tmp/duxcel-worker-digest-XXXXXX"; CHECK(mkdtemp(directory));
    char large[1024],small[1024],missing[1024],replacement[1024]; snprintf(large,sizeof(large),"%s/large",directory); snprintf(small,sizeof(small),"%s/small",directory); snprintf(missing,sizeof(missing),"%s/missing",directory); snprintf(replacement,sizeof(replacement),"%s/replacement",directory);
    write_file(large,true); write_file(small,false); dx_mutex_init(&checkpoint.mutex); dx_cond_init(&checkpoint.condition);
    QueryWorker worker; CHECK(worker_start(&worker)); basic_and_payloads(&worker,large,missing); cancellation_and_latest(&worker,large,small,missing); changed_input(&worker,large,replacement); stop_while_verifying(&worker,large);
    dx_cond_destroy(&checkpoint.condition); dx_mutex_destroy(&checkpoint.mutex); CHECK(unlink(large)==0 && unlink(small)==0 && rmdir(directory)==0);
    printf("PASS worker-source-digest: %u checks; zero SQL and no DuckDB connection\n",checks); return 0;
}
