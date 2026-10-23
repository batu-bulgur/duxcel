#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "data/worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL Save worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static DxMutex gate_mutex;
static DxCond gate_condition;
static bool entered,released;
static WorkbookSavePhase gate_phase;
static uint64_t gate_at;
void dx_test_save_checkpoint(QueryWorker *worker,WorkbookSaveProgress progress) {
    (void)worker; dx_lock(&gate_mutex);
    if (!entered && progress.phase==gate_phase && progress.completed>=gate_at) {
        entered=true; dx_signal(&gate_condition);
        while (!released) dx_cond_wait(&gate_condition,&gate_mutex);
    }
    dx_unlock(&gate_mutex);
}
static void wait_gate(void) {
    double deadline=dx_millis()+10000;
    for (;;) { dx_lock(&gate_mutex); bool value=entered; dx_unlock(&gate_mutex); if (value) return; CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
}
static void release_gate(void) { dx_lock(&gate_mutex); released=true; dx_signal(&gate_condition); dx_unlock(&gate_mutex); }
static QueryReply *reply(QueryWorker *w) {
    double deadline=dx_millis()+10000; QueryReply *r;
    while (!(r=worker_poll(w))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
    return r;
}
static void verify(const char *path,const char *text) {
    Workbook loaded; workbook_init(&loaded); char *error=NULL; CHECK(workbook_load(&loaded,path,&error));
    CHECK(!error && !strcmp(cells_get(&loaded.sheets[0].cells,0,0),text)); workbook_free(&loaded);
}
static QueryJob *save_job(const Workbook *book,const char *path,uint64_t id) {
    QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=JOB_SAVE; j->path=dx_strdup(path); j->workbook=workbook_snapshot(book); CHECK(j->workbook); return j;
}
int main(void) {
    char directory[]="build/tests/worker-save-XXXXXX"; CHECK(mkdtemp(directory)); char *path=dx_printf("%s/book.dxls",directory);
    Workbook b; workbook_init(&b); char *error=NULL; CHECK(workbook_add(&b,"A",&(Analysis){0},&error));
    CHECK(cells_set(&b.sheets[0].cells,0,0,"original",&error)); CHECK(workbook_save(&b,path,&error));
    CHECK(cells_set(&b.sheets[0].cells,0,0,"saved",&error)); b.command_revision=8;
    dx_mutex_init(&gate_mutex); dx_cond_init(&gate_condition); QueryWorker worker; CHECK(worker_start(&worker));
    struct { WorkbookSavePhase phase; uint64_t at; bool cancelled; } cases[]={
        {SAVE_PHASE_VALIDATE,0,true},{SAVE_PHASE_WRITE,3,true},{SAVE_PHASE_WRITE,9,true},
        {SAVE_PHASE_PUBLISH,9,false},{SAVE_PHASE_COMPLETE,10,false}};
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        dx_lock(&gate_mutex); gate_phase=cases[i].phase; gate_at=cases[i].at; entered=released=false; dx_unlock(&gate_mutex);
        uint64_t id=10+i; worker_submit(&worker,save_job(&b,path,id)); wait_gate();
        WorkerSaveState state=worker_save_state(&worker); CHECK(state.active && state.id==id && state.progress.phase==cases[i].phase);
        CHECK(state.cancellable==cases[i].cancelled);
        CHECK(worker_save_cancel(&worker)==cases[i].cancelled);
        /* Legacy analytics cancellation and replacement must not erase a write receipt. */
        worker_cancel(&worker); QueryJob *read=dx_alloc(sizeof(*read)); read->id=100+i; read->kind=JOB_PAGE; worker_submit(&worker,read);
        state=worker_save_state(&worker); CHECK(state.active && state.id==id && state.cancel_requested==cases[i].cancelled);
        CHECK(cells_set(&b.sheets[0].cells,1,0,"newer",&error)); /* The immutable saved snapshot stays R. */
        release_gate(); QueryReply *r=reply(&worker);
        CHECK(r->id==id && r->kind==JOB_SAVE && r->save_result==(cases[i].cancelled?WORKBOOK_SAVE_CANCELLED:WORKBOOK_SAVE_COMPLETE));
        CHECK((r->error!=NULL)==cases[i].cancelled); reply_free(r); CHECK(!worker_save_state(&worker).active && !worker_save_cancel(&worker));
        verify(path,cases[i].cancelled?"original":"saved");
    }
    worker_stop(&worker); dx_cond_destroy(&gate_condition); dx_mutex_destroy(&gate_mutex); workbook_free(&b);
    CHECK(!unlink(path) && !rmdir(directory)); free(path); CHECK(!error);
    printf("PASS: %u ordered Save worker checks\n",checks); return 0;
}
