#define _DARWIN_C_SOURCE
#include "data/worker.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks;
static uint64_t sequence;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL record selection worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static QueryJob *request(const char *path,SourceKind kind,uint64_t first,uint64_t rows,uint64_t object) {
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=++sequence; job->kind=JOB_COLLECT_RECORDS;
    job->analysis=(Analysis){.kind=kind,.path=dx_strdup(path),.source_version=UINT64_C(9007199254740993)};
    job->offset=first; job->rows=rows; job->object_id=object; job->is_table=object!=0; job->revision=UINT64_C(9007199254740995);
    job->expected_source_stamp=dx_file_stamp(path); CHECK(job->expected_source_stamp);
    CHECK(source_file_identity(path,&job->expected_file_identity,NULL)==SOURCE_DIGEST_OK); job->expected_file_identity_valid=true; return job;
}
static QueryReply *wait_reply(QueryWorker *worker) {
    double deadline=dx_millis()+10000;
    while (dx_millis()<deadline) { QueryReply *reply=worker_poll(worker); if (reply) return reply; dx_sleep_ms(1); }
    CHECK(!"record collection worker timeout"); return NULL;
}
static void wait_idle(QueryWorker *worker) {
    double deadline=dx_millis()+10000;
    while (dx_millis()<deadline) {
        dx_lock(&worker->mutex); bool idle=!worker->running && !worker->pending; dx_unlock(&worker->mutex);
        if (idle) return; dx_sleep_ms(1);
    }
    CHECK(!"record collection cancellation timeout");
}
static void succeeded(QueryReply *reply,uint64_t id,size_t count,uint64_t first,uint64_t object) {
    CHECK(reply->id==id && reply->kind==JOB_COLLECT_RECORDS && !reply->error && reply->object_id==object && reply->revision==UINT64_C(9007199254740995));
    CHECK(reply->analysis.source_version==UINT64_C(9007199254740993) && reply->records.count==count && reply->records.bytes==count*sizeof(uint64_t));
    CHECK(!reply->page.cells && !reply->page.columns && !reply->page.row_count && !reply->statistics.sum && !reply->clipboard);
    for (size_t i=0;i<count;i++) CHECK(reply->records.ids[i]==first+i);
}
static void failed(QueryReply *reply,uint64_t id) {
    CHECK(reply->id==id && reply->kind==JOB_COLLECT_RECORDS && reply->error && *reply->error);
    CHECK(!reply->records.ids && !reply->records.count && !reply->records.bytes && !reply->records.stamp && !reply->page.cells && !reply->page.columns);
}
static void create_sources(const char *csv,const char *parquet) {
    Database db={0}; char *error=NULL; CHECK(database_open(&db,&error)); const char *paths[]={csv,parquet};
    for (size_t i=0;i<2;i++) {
        char *path=dx_quote(paths[i],'\''),*sql=dx_printf("COPY (SELECT i//2 AS physical_duplicate FROM range(20001) t(i)) TO %s (FORMAT %s%s)",path,i ? "PARQUET" : "CSV",i ? "" : ",HEADER true"); duckdb_result result={0};
        CHECK(database_execute(&db,sql,&result,&error)); duckdb_destroy_result(&result); free(path); free(sql);
    }
    database_close(&db); free(error); /* Never overlap this fixture engine with the worker. */
}
int main(void) {
    char directory[]="/tmp/duxcel-record-worker-XXXXXX"; CHECK(mkdtemp(directory)); char *csv=dx_printf("%s/source.csv",directory),*parquet=dx_printf("%s/source.parquet",directory);
    create_sources(csv,parquet); QueryWorker worker; CHECK(worker_start(&worker));
    QueryJob *job=request(csv,SOURCE_CSV,0,4,0); uint64_t id=job->id; job->expected_file_identity_valid=false;
    worker_submit(&worker,job); QueryReply *reply=wait_reply(&worker); failed(reply,id); reply_free(reply);
    dx_lock(&worker.mutex); CHECK(!worker.db.connection && !worker.db.query_count); dx_unlock(&worker.mutex);
    job=request(csv,SOURCE_CSV,0,4,77); id=job->id; job->is_table=false; worker_submit(&worker,job); reply=wait_reply(&worker); failed(reply,id); reply_free(reply);
    job=request(csv,SOURCE_CSV,240,350,0); id=job->id; const char *owned_path=job->analysis.path;
    worker_submit(&worker,job); QueryReply *retained=wait_reply(&worker); succeeded(retained,id,350,241,0); CHECK(retained->analysis.path==owned_path);
    /* A polled vector stays caller-owned across later jobs and worker close. */
    job=request(parquet,SOURCE_PARQUET,5,3,UINT64_C(9007199254740997)); id=job->id;
    worker_submit(&worker,job); reply=wait_reply(&worker); succeeded(reply,id,3,6,UINT64_C(9007199254740997)); reply_free(reply);
    CHECK(retained->records.ids[0]==241 && retained->records.ids[349]==590);
    job=request(csv,SOURCE_CSV,0,20001,0); id=job->id; worker_submit(&worker,job); reply=wait_reply(&worker); failed(reply,id); CHECK(strstr(reply->error,"20,000")); reply_free(reply);
    job=request(csv,SOURCE_CSV,0,4,0); id=job->id; uint64_t stamp=job->expected_source_stamp; struct stat file;
    CHECK(stat(csv,&file)==0 && chmod(csv,(file.st_mode&0777)^S_IXUSR)==0 && dx_file_stamp(csv)==stamp);
    worker_submit(&worker,job); reply=wait_reply(&worker); failed(reply,id); CHECK(strstr(reply->error,"exact native file")); reply_free(reply);
    job=request(csv,SOURCE_CSV,0,4,0); id=job->id; worker_submit(&worker,job); reply=wait_reply(&worker); succeeded(reply,id,4,1,0); reply_free(reply);
    /* Ordinary scheduling controls cover cancellation of pending/running work,
       superseded requests, and rejection of late publication, without sleeps
       or claiming a deterministic conversion-phase interruption. */
    worker_submit(&worker,request(csv,SOURCE_CSV,0,20000,0)); worker_cancel(&worker); CHECK(!worker_poll(&worker));
    worker_submit(&worker,request(csv,SOURCE_CSV,0,20000,0)); worker_submit(&worker,request(parquet,SOURCE_PARQUET,0,20000,77));
    job=request(csv,SOURCE_CSV,7,2,0); id=job->id; worker_submit(&worker,job); reply=wait_reply(&worker); succeeded(reply,id,2,8,0); reply_free(reply);
    wait_idle(&worker); CHECK(!worker_poll(&worker));
    worker_submit(&worker,request(parquet,SOURCE_PARQUET,0,20000,77)); worker_stop(&worker);
    CHECK(!worker.started && !worker.pending && !worker.reply && !worker.db.connection);
    CHECK(retained->records.count==350 && retained->records.ids[0]==241 && retained->records.ids[349]==590); reply_free(retained);
    CHECK(unlink(csv)==0 && unlink(parquet)==0 && rmdir(directory)==0); free(csv); free(parquet);
    printf("PASS: %u owned native worker record-collection checks\n",checks); return 0;
}
