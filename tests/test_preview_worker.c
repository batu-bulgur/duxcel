#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "data/worker_archive.h"
#include "app/versionstate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef DUXCEL_FEATURE_PREVIEW_WORKER
#error Compile with the private preview worker adapter.
#endif
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL preview worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static struct { DxMutex mutex; DxCond condition; uint64_t id; bool entered,released; } gate;
void dx_test_preview_checkpoint(QueryWorker *worker,WorkbookVersionPreviewPhase phase) {
    dx_lock(&gate.mutex);
    if (worker->active_id==gate.id && phase==VERSION_PREVIEW_READY && !gate.entered) {
        gate.entered=true; dx_signal(&gate.condition);
        while (!gate.released) dx_cond_wait(&gate.condition,&gate.mutex);
    }
    dx_unlock(&gate.mutex);
}
static void arm(uint64_t id) { dx_lock(&gate.mutex); gate.id=id; gate.entered=gate.released=false; dx_unlock(&gate.mutex); }
static void release(void) { dx_lock(&gate.mutex); gate.released=true; dx_signal(&gate.condition); dx_unlock(&gate.mutex); }
static void waiting(void) {
    double deadline=dx_millis()+15000;
    for (;;) { dx_lock(&gate.mutex); bool ready=gate.entered; dx_unlock(&gate.mutex); if (ready) return; CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
}
static QueryReply *reply(QueryWorker *worker) {
    double deadline=dx_millis()+20000; QueryReply *r;
    while (!(r=worker_poll(worker))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); } return r;
}
static QueryJob *job(uint64_t id,JobKind kind,const char *path) { QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=kind; j->path=dx_strdup(path); return j; }
int main(void) {
    char parent[]="build/tests/preview-worker-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/version.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Protected root",&(Analysis){0},&error)); CHECK(cells_set(&book.sheets[0].cells,0,0,"Historical value",&error));
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD}; WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_tag_save((WorkbookVersionSaveRequest){&book,NULL,owner,VERSION_USER_SAVE,path,parent,UINT64_C(64)*1024*1024},(WorkbookVersionText){"V1",2},
        (WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    WorkbookVersionCoreSet *base=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt);
    WorkbookVersionKey root=workbook_version_core_set_index(base)->tags[0].root;
    char *old_leaf=dx_strdup(workbook_core_payload_path(workbook_version_core_set_leaf(base,0)));
    QueryWorker worker; CHECK(worker_start(&worker)); dx_mutex_init(&gate.mutex); dx_cond_init(&gate.condition);
    QueryJob *j=job(1,JOB_LOAD,"historical root, not a file path");
    CHECK(!worker_archive_preview_job(&worker,j,base,root,owner,&error) && error && !j->native_file.context); free(error); error=NULL;
    WorkbookVersionOwner reader=owner; reader.mode=VERSION_HISTORICAL_VIEW; reader.session++;
    CHECK(worker_archive_preview_job(&worker,j,base,root,reader,&error) && !error);
    workbook_version_core_set_free(base); base=NULL; arm(1); worker_submit(&worker,j); waiting();
    CHECK(!access(old_leaf,F_OK)); worker_cancel(&worker); release();
    j=job(2,JOB_VERIFY_SOURCE,path); j->expected_source_stamp=dx_file_stamp(path); worker_submit(&worker,j);
    QueryReply *r=reply(&worker); CHECK(r->id==2 && !r->error && r->source_digest_status==SOURCE_DIGEST_OK && !worker_archive_is_preview_reply(r)); reply_free(r);
    CHECK(access(old_leaf,F_OK)); free(old_leaf);
    j=job(3,JOB_LOAD,path); CHECK(worker_archive_load_job(&worker,j,parent,UINT64_C(64)*1024*1024,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(!r->error && worker_archive_is_reply(r) && !worker_archive_is_preview_reply(r) && !worker_archive_preview(r) && !worker_archive_take_preview(r));
    base=worker_archive_take_versions(r); CHECK(base); reply_free(r);
    j=job(4,JOB_LOAD,"historical root"); CHECK(worker_archive_preview_job(&worker,j,base,root,reader,&error)); arm(4); worker_submit(&worker,j); waiting(); release(); r=reply(&worker);
    CHECK(!r->error && r->save_result==WORKBOOK_SAVE_COMPLETE && r->workbook && worker_archive_is_preview_reply(r) && !worker_archive_is_reply(r));
    CHECK(!worker_archive_is_tag_reply(r) && !worker_archive_plan(r) && !worker_archive_tag_plan(r) && !worker_archive_identity(r) && !worker_archive_take_versions(r));
    CHECK(!r->workbook->path && !r->workbook->dirty && !strcmp(cells_get(&r->workbook->sheets[0].cells,0,0),"Historical value"));
    const WorkbookVersionPreview *preview=worker_archive_preview(r); CHECK(preview && !workbook_version_preview_book(preview) && workbook_version_preview_owner(preview)->session==reader.session);
    CHECK(workbook_version_core_set_index(worker_archive_versions(r))->tag_count==1);
    AppVersionState state={.owner=owner},before=state;
    CHECK(!app_versions_loaded(&state,&worker,r,&error) && error && !memcmp(&state,&before,sizeof(state))); free(error); error=NULL;
    WorkbookVersionPreview *owned=worker_archive_take_preview(r); CHECK(owned && !worker_archive_preview(r));
    Workbook *projection=r->workbook; r->workbook=NULL;
    worker_stop(&worker); reply_free(r); workbook_version_core_set_free(base); base=NULL;
    CHECK(!strcmp(cells_get(&projection->sheets[0].cells,0,0),"Historical value") && workbook_version_core_set_index(workbook_version_preview_versions(owned))->tag_count==1);
    CHECK(!worker.sql_executed && !worker_archive_is_preview_reply(NULL) && !worker_archive_preview(NULL) && !worker_archive_take_preview(NULL));
    workbook_version_preview_free(owned); workbook_free(projection); free(projection); workbook_free(&book);
    dx_cond_destroy(&gate.condition); dx_mutex_destroy(&gate.mutex);
    CHECK(!error && !unlink(path) && !rmdir(parent)); free(path);
    printf("PASS: %u protected historical worker checks\n",checks); return 0;
}
