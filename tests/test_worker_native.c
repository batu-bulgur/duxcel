#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "data/worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native file worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { DxMutex mutex; unsigned ids[16],count; } Log;
/* Finite protocol fixture. It delegates real file IO to product APIs; the
   versioned adapter is tested separately, without linking it into Core. */
struct WorkbookNativeIO {
    QueryWorker *worker; Log *log; unsigned id;
    WorkerNativeRetirement retirement;
    bool block,release,entered,executed,disposed,background,cancelled,ok;
};
static bool on_worker(QueryWorker *w) {
    if (!w->started) return false;
#ifdef _WIN32
    return GetCurrentThreadId()==GetThreadId(w->thread);
#else
    return pthread_equal(pthread_self(),w->thread)!=0;
#endif
}
static void destroy(WorkbookNativeIO *c) {
    /* Reentering this mutex-owned scalar observer proves disposal is outside
       the worker mutex, including reject/cancel/stop paths. */
    (void)worker_save_state(c->worker); bool background=on_worker(c->worker);
    dx_lock(&c->log->mutex);
    c->background=background; c->disposed=true;
    if (c->log->count<16) c->log->ids[c->log->count++]=c->id; else c->ok=false;
    dx_unlock(&c->log->mutex);
}
static void retire(WorkbookNativeIO *c) {
    if (!worker_retire_native(c->worker,&c->retirement)) destroy(c);
}
static WorkbookSaveResult execute(WorkbookNativeIO *c,JobKind kind,const Workbook *snapshot,const char *path,
    WorkerNativeFileControl control,Workbook **loaded,char **error) {
    control.prepare(control.context,WORKER_SAVE_SNAPSHOT,2,65536);
    dx_lock(&c->log->mutex); c->entered=c->executed=true; dx_unlock(&c->log->mutex);
    for (;;) {
        dx_lock(&c->log->mutex); bool block=c->block && !c->release; dx_unlock(&c->log->mutex);
        if (!block || control.cancelled(control.context)) break; dx_sleep_ms(1);
    }
    if (control.cancelled(control.context)) {
        dx_lock(&c->log->mutex); c->cancelled=true; dx_unlock(&c->log->mutex);
        *error=dx_strdup("Native file task cancelled before publication."); return WORKBOOK_SAVE_CANCELLED;
    }
    if (kind==JOB_SAVE) return workbook_save_controlled(snapshot,path,control.publication,error);
    if (kind==JOB_LOAD) {
        *loaded=dx_alloc(sizeof(**loaded)); workbook_init(*loaded);
        return workbook_load(*loaded,path,error) ? WORKBOOK_SAVE_COMPLETE : WORKBOOK_SAVE_FAILED;
    }
    *error=dx_strdup("Unexpected fixture operation."); return WORKBOOK_SAVE_FAILED;
}
static void init(WorkbookNativeIO *c,QueryWorker *w,Log *log,unsigned id,bool block) {
    *c=(WorkbookNativeIO){.worker=w,.log=log,.id=id,.block=block,.ok=true};
    c->retirement=(WorkerNativeRetirement){.context=c,.dispose=destroy};
}
static QueryJob *job(WorkbookNativeIO *c,JobKind kind,const Workbook *book,const char *path) {
    QueryJob *j=dx_alloc(sizeof(*j)); j->id=c->id; j->kind=kind; j->path=dx_strdup(path);
    if (kind==JOB_SAVE) { j->workbook=workbook_snapshot(book); CHECK(j->workbook); }
    j->native_file=(WorkerNativeFileTask){c,execute,retire}; return j;
}
static void wait_flag(WorkbookNativeIO *c,bool disposed) {
    double deadline=dx_millis()+10000;
    for (;;) {
        dx_lock(&c->log->mutex); bool done=disposed ? c->disposed : c->entered; dx_unlock(&c->log->mutex);
        if (done) return; CHECK(dx_millis()<deadline); dx_sleep_ms(1);
    }
}
static QueryReply *reply(QueryWorker *w) {
    double deadline=dx_millis()+10000; QueryReply *r;
    while (!(r=worker_poll(w))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); } return r;
}
static void verify(const char *path,const char *text) {
    Workbook b; workbook_init(&b); char *error=NULL;
    CHECK(workbook_load(&b,path,&error) && !error && !strcmp(cells_get(&b.sheets[0].cells,0,0),text)); workbook_free(&b);
}
int main(void) {
    char directory[]="build/tests/worker-native-XXXXXX"; CHECK(mkdtemp(directory)); char *path=dx_printf("%s/workbook.dxls",directory);
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Native envelope",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"snapshot",&error)); book.command_revision=8;
    QueryWorker worker; CHECK(worker_start(&worker)); Log log={0}; dx_mutex_init(&log.mutex);
    WorkbookNativeIO contexts[7]; init(contexts,&worker,&log,1,false);
    worker_submit(&worker,job(contexts,JOB_SAVE,&book,path));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"newer",&error)); book.command_revision=9;
    QueryReply *r=reply(&worker);
    CHECK(r->id==1 && r->save_result==WORKBOOK_SAVE_COMPLETE && !r->error && r->workbook && r->native_file.context==contexts);
    CHECK(r->workbook->command_revision==8 && !strcmp(cells_get(&r->workbook->sheets[0].cells,0,0),"snapshot"));
    reply_free(r); wait_flag(contexts,true); CHECK(contexts[0].background); verify(path,"snapshot");
    init(contexts+1,&worker,&log,2,false); worker_submit(&worker,job(contexts+1,JOB_LOAD,NULL,path)); r=reply(&worker);
    CHECK(r->id==2 && !r->error && r->workbook && !strcmp(cells_get(&r->workbook->sheets[0].cells,0,0),"snapshot"));
    reply_free(r); worker_stop(&worker); CHECK(contexts[1].disposed && contexts[1].background && !worker.retired_head && !worker.retired_tail);
    CHECK(worker_start(&worker)); init(contexts+2,&worker,&log,3,true); init(contexts+3,&worker,&log,4,false);
    worker_submit(&worker,job(contexts+2,JOB_SAVE,&book,path)); wait_flag(contexts+2,false);
    WorkerSaveState state=worker_save_state(&worker);
    CHECK(state.active && state.cancellable && state.preparation==WORKER_SAVE_SNAPSHOT && state.preparation_root==2 && state.preparation_bytes==65536);
    worker_submit(&worker,job(contexts+3,JOB_LOAD,NULL,path)); CHECK(worker_save_state(&worker).id==3);
    CHECK(worker_save_cancel(&worker)); r=reply(&worker);
    CHECK(r->id==3 && r->save_result==WORKBOOK_SAVE_CANCELLED && r->error && r->workbook->command_revision==9);
    reply_free(r); wait_flag(contexts+2,true); wait_flag(contexts+3,true);
    CHECK(contexts[2].cancelled && contexts[2].background && contexts[3].background && !contexts[3].executed);
    dx_lock(&log.mutex); CHECK(log.count==4 && log.ids[2]==4 && log.ids[3]==3); dx_unlock(&log.mutex); verify(path,"snapshot");
    init(contexts+4,&worker,&log,5,true); init(contexts+5,&worker,&log,6,false);
    worker_submit(&worker,job(contexts+4,JOB_SAVE,&book,path)); wait_flag(contexts+4,false);
    worker_submit(&worker,job(contexts+5,JOB_LOAD,NULL,path)); worker_stop(&worker);
    CHECK(contexts[4].disposed && contexts[4].background && contexts[4].cancelled && contexts[5].disposed && contexts[5].background && !contexts[5].executed);
    CHECK(!worker.retired_head && !worker.retired_tail && !worker.pending && !worker.reply && !worker.db.connection);
    verify(path,"snapshot");
    CHECK(worker_start(&worker)); init(contexts+6,&worker,&log,7,false);
    worker_submit(&worker,job(contexts+6,JOB_SAVE,&book,path)); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_COMPLETE && !r->error); worker_stop(&worker);
    reply_free(r); CHECK(contexts[6].disposed && !contexts[6].background); verify(path,"newer");
    CHECK(!worker_retire_native(&worker,&contexts[6].retirement) && !worker_retire_native(NULL,NULL));
    dx_mutex_destroy(&log.mutex); workbook_free(&book);
    CHECK(!unlink(path) && !rmdir(directory)); free(path); CHECK(!error);
    printf("PASS: %u typed native worker ownership checks\n",checks); return 0;
}
