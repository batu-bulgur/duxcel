#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "data/worker_archive.h"
#include "support/current_archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef DUXCEL_FEATURE_ARCHIVE_WORKER
#error Compile this test and its private archive worker adapter with DUXCEL_FEATURE_ARCHIVE_WORKER.
#endif
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL archive worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct { DxMutex mutex; DxCond condition; uint64_t id; bool named,entered,released; } gate;
static void checkpoint(QueryWorker *w,bool named,JobKind kind,WorkbookSaveProgress p) {
    dx_lock(&gate.mutex);
    if (kind==JOB_SAVE && gate.id==w->active_id && gate.named==named && p.phase==SAVE_PHASE_PUBLISH && !gate.entered) {
        gate.entered=true; dx_signal(&gate.condition);
        while (!gate.released) dx_cond_wait(&gate.condition,&gate.mutex);
    }
    dx_unlock(&gate.mutex);
}
void dx_test_archive_capture_checkpoint(QueryWorker *w,JobKind kind,WorkbookSaveProgress p) { checkpoint(w,false,kind,p); }
void dx_test_archive_publish_checkpoint(QueryWorker *w,WorkbookSaveProgress p) { checkpoint(w,true,JOB_SAVE,p); }
static void arm(uint64_t id,bool named) { dx_lock(&gate.mutex); gate.id=id; gate.named=named; gate.entered=gate.released=false; dx_unlock(&gate.mutex); }
static void wait_gate(void) {
    double deadline=dx_millis()+15000;
    for (;;) { dx_lock(&gate.mutex); bool entered=gate.entered; dx_unlock(&gate.mutex); if (entered) return; CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
}
static void release_gate(void) { dx_lock(&gate.mutex); gate.released=true; dx_signal(&gate.condition); dx_unlock(&gate.mutex); }
static QueryReply *reply(QueryWorker *w) {
    double deadline=dx_millis()+20000; QueryReply *r;
    while (!(r=worker_poll(w))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); } return r;
}
static QueryJob *job(uint64_t id,JobKind kind,const Workbook *book,const char *path) {
    QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=kind; j->path=dx_strdup(path);
    if (book) { j->workbook=workbook_snapshot(book); CHECK(j->workbook); } return j;
}
static SourceDigest digest(const char *path) { SourceDigest d={0}; CHECK(source_digest_file(path,NULL,NULL,&d,NULL)==SOURCE_DIGEST_OK); return d; }
static void unchanged(const char *path,SourceDigest before) { SourceDigest after=digest(path); CHECK(after.bytes==before.bytes && !memcmp(after.sha256,before.sha256,DX_SOURCE_DIGEST_BYTES)); }
static void saved_value(const WorkbookVersionCoreSet *set,const char *value) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(set); size_t position=0;
    while (position<index->root_count && memcmp(index->roots[position].key.bytes,index->saved.key.bytes,DX_VERSION_KEY_BYTES)) position++;
    CHECK(position<index->root_count);
    Workbook b; workbook_init(&b); char *error=NULL;
    CHECK(workbook_load_core_payload(&b,workbook_core_payload_path(workbook_version_core_set_leaf(set,position)),&error) && !error);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,0,0),value)); workbook_free(&b);
}
int main(void) {
    char directory[]="build/tests/worker-archive-XXXXXX"; CHECK(mkdtemp(directory));
    char *store=dx_printf("%s/versions",directory),*initial=dx_printf("%s/initial.dxls",directory),*path=dx_printf("%s/native.dxls",directory);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Native worker",&(Analysis){0},&error)); CHECK(cells_set(&book.sheets[0].cells,0,0,"initial R",&error)); book.command_revision=8;
    CHECK(test_current_archive_save(&book,initial,&error));
    QueryWorker worker; CHECK(worker_start(&worker)); WorkbookVersionCoreSet *base=NULL;
    dx_mutex_init(&gate.mutex); dx_cond_init(&gate.condition);
    QueryJob *j=job(1,JOB_LOAD,NULL,initial);
    CHECK(worker_archive_load_job(&worker,j,store,UINT64_C(64)*1024*1024,&error) && !error && access(store,F_OK));
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    SourceDigest original=digest(initial); worker_submit(&worker,j); QueryReply *r=reply(&worker);
    CHECK(r->kind==JOB_LOAD && !r->error && r->workbook && worker_archive_is_reply(r) && worker_archive_identity(r));
    base=worker_archive_take_versions(r); CHECK(base && workbook_version_core_set_index(base)->saved.present); saved_value(base,"initial R");
    workbook_free(&book); book=*r->workbook; workbook_init(r->workbook); reply_free(r); unchanged(initial,original);
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD}; uint64_t reserved=UINT64_C(64)*1024*1024;
    j=job(2,JOB_SAVE,&book,path); CHECK(worker_archive_save_job(&worker,j,base,owner,VERSION_USER_SAVE,store,reserved,&error));
    worker_submit(&worker,j); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_COMPLETE && !r->error && worker_archive_is_reply(r));
    CHECK(worker_archive_plan(r) && !worker_archive_plan(r)->adds_root && !worker_archive_plan(r)->updates_saved);
    CHECK(workbook_version_can_publish(workbook_version_core_set_index(base),owner,worker_archive_plan(r)));
    char *old_leaf=dx_strdup(workbook_core_payload_path(workbook_version_core_set_leaf(base,0)));
    WorkbookVersionCoreSet *next=worker_archive_take_versions(r); worker_archive_release_versions(&worker,&base); CHECK(!base);
    /* This read is processed after queued App-owner retirement. The held reply
       must still retain its old base until disposal, independent of App state. */
    QueryJob *verify=dx_alloc(sizeof(*verify)); verify->id=3; verify->kind=JOB_VERIFY_SOURCE; verify->path=dx_strdup(old_leaf); verify->expected_source_stamp=dx_file_stamp(old_leaf);
    worker_submit(&worker,verify); QueryReply *checked=reply(&worker);
    CHECK(!checked->error && checked->source_digest_status==SOURCE_DIGEST_OK && !worker_archive_is_reply(checked)); reply_free(checked);
    CHECK(!access(old_leaf,F_OK)); free(old_leaf); reply_free(r); base=next; saved_value(base,"initial R");
    CHECK(cells_set(&book.sheets[0].cells,0,0,"working R+1",&error)); book.command_revision=9; book.dirty=true;
    SourceDigest before=digest(path); arm(4,false);
    j=job(4,JOB_SAVE,&book,path); CHECK(worker_archive_save_job(&worker,j,base,owner,VERSION_USER_SAVE,store,reserved,&error));
    worker_submit(&worker,j); wait_gate(); WorkerSaveState state=worker_save_state(&worker);
    CHECK(state.active && state.cancellable && state.preparation==WORKER_SAVE_SNAPSHOT);
    CHECK(worker_save_cancel(&worker)); release_gate(); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_CANCELLED && r->error && r->workbook->command_revision==9 && !worker_archive_plan(r));
    reply_free(r); unchanged(path,before); CHECK(workbook_version_core_set_index(base)->generation==1);
    arm(5,true); j=job(5,JOB_SAVE,&book,path); CHECK(worker_archive_save_job(&worker,j,base,owner,VERSION_USER_SAVE,store,reserved,&error));
    worker_submit(&worker,j); wait_gate(); state=worker_save_state(&worker);
    CHECK(state.active && !state.cancellable && state.progress.phase==SAVE_PHASE_PUBLISH && state.preparation==WORKER_SAVE_PREPARE_NONE);
    CHECK(!worker_save_cancel(&worker));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"working R+2",&error)); book.command_revision=10; release_gate(); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_COMPLETE && !r->error && r->workbook->command_revision==9);
    CHECK(!strcmp(cells_get(&r->workbook->sheets[0].cells,0,0),"working R+1") && !strcmp(cells_get(&book.sheets[0].cells,0,0),"working R+2"));
    WorkbookVersionPlan plan=*worker_archive_plan(r);
    CHECK(plan.adds_root && plan.updates_saved && workbook_version_can_publish(workbook_version_core_set_index(base),owner,&plan));
    next=worker_archive_take_versions(r); worker_archive_release_versions(&worker,&base); base=next;
    reply_free(r); CHECK(workbook_version_core_set_index(base)->generation==2 && !workbook_version_can_publish(workbook_version_core_set_index(base),owner,&plan)); saved_value(base,"working R+1");
    j=job(6,JOB_LOAD,NULL,path); CHECK(worker_archive_load_job(&worker,j,store,reserved,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(!r->error && r->workbook->command_revision==9 && worker_archive_identity(r));
    CHECK(!strcmp(cells_get(&r->workbook->sheets[0].cells,0,0),"working R+1"));
    WorkbookVersionCoreSet *loaded=worker_archive_take_versions(r); CHECK(loaded && workbook_version_core_set_index(loaded)->root_count==2);
    reply_free(r); char label[]="Worker tag"; arm(7,true);
    j=job(7,JOB_SAVE,&book,path); CHECK(worker_archive_tag_job(&worker,j,loaded,owner,(WorkbookVersionText){label,strlen(label)},store,reserved,&error));
    memset(label,'x',strlen(label)); worker_submit(&worker,j); wait_gate();
    CHECK(!worker_save_state(&worker).cancellable && !worker_save_cancel(&worker)); release_gate(); r=reply(&worker);
    CHECK(!r->error && r->save_result==WORKBOOK_SAVE_COMPLETE && !worker_archive_plan(r));
    const WorkbookVersionTagPlan *tag=worker_archive_tag_plan(r); CHECK(tag && !strcmp(tag->label,"Worker tag"));
    CHECK(workbook_version_tag_can_publish(workbook_version_core_set_index(loaded),owner,tag));
    next=worker_archive_take_versions(r); worker_archive_release_versions(&worker,&loaded); loaded=next; reply_free(r);
    CHECK(workbook_version_core_set_index(loaded)->tag_count==1 && workbook_version_core_set_index(loaded)->root_count==3); saved_value(loaded,"working R+2");
    j=job(8,JOB_LOAD,NULL,path); CHECK(worker_archive_load_job(&worker,j,store,reserved,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(!r->error && workbook_version_core_set_index(worker_archive_versions(r))->tag_count==1);
    next=worker_archive_take_versions(r); worker_archive_release_versions(&worker,&loaded); loaded=next;
    worker_stop(&worker); reply_free(r); saved_value(loaded,"working R+2");
    worker_archive_release_versions(&worker,&base); worker_archive_release_versions(&worker,&loaded); CHECK(!base && !loaded);
    CHECK(!unlink(path));
#else
    worker_job_free(j); worker_stop(&worker);
#endif
    CHECK(!worker_archive_is_reply(NULL) && !worker_archive_plan(NULL) && !worker_archive_tag_plan(NULL) && !worker_archive_versions(NULL) && !worker_archive_identity(NULL) && !worker_archive_take_versions(NULL));
    worker_archive_release_versions(NULL,NULL);
    dx_cond_destroy(&gate.condition); dx_mutex_destroy(&gate.mutex); workbook_free(&book);
    CHECK(!error && !unlink(initial));
    if (!access(store,F_OK)) CHECK(!rmdir(store)); CHECK(!rmdir(directory)); free(store); free(initial); free(path);
    printf("PASS: %u real versioned archive worker checks\n",checks); return 0;
}
