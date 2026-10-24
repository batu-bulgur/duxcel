#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/versionstate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL App versions:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static QueryReply *run(AppVersionState *state,QueryWorker *worker,uint64_t id,JobKind kind,const Workbook *book,const char *path,const char *parent) {
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=id; job->kind=kind; job->path=dx_strdup(path);
    if (book) job->workbook=workbook_snapshot(book);
    char *error=NULL; CHECK(app_versions_job(state,worker,job,parent,&error) && !error);
    worker_submit(worker,job); QueryReply *reply; double deadline=dx_millis()+20000;
    while (!(reply=worker_poll(worker))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
    CHECK(!reply->error); return reply;
}
int main(void) {
    char directory[]="build/tests/app-versions-XXXXXX"; CHECK(mkdtemp(directory));
    char *parent=dx_printf("%s/versions",directory),*path=dx_printf("%s/workbook.dxls",directory);
    char *error=NULL; Workbook book; workbook_init(&book);
    CHECK(workbook_add(&book,"Owner",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"R",&error)); CHECK(workbook_save(&book,path,&error));
    QueryWorker worker; CHECK(worker_start(&worker)); AppVersionState state={0};
#if defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    QueryReply *r=run(&state,&worker,1,JOB_LOAD,NULL,path,parent);
    CHECK(app_versions_loaded(&state,&worker,r,&error) && state.store && state.identity_valid && !error);
    CHECK(state.owner.session && !worker_archive_versions(r)); reply_free(r);
    WorkbookVersionOwner original=state.owner;
    r=run(&state,&worker,2,JOB_SAVE,&book,path,parent);
    CHECK(app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_APPLIED);
    CHECK(workbook_version_core_set_index(state.store)->root_count==1); reply_free(r);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"R+1",&error)); book.command_revision++;
    r=run(&state,&worker,3,JOB_SAVE,&book,path,parent);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"R+2",&error)); book.command_revision++; book.dirty=true;
    CHECK(app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_APPLIED);
    CHECK(book.dirty && !strcmp(cells_get(&book.sheets[0].cells,0,0),"R+2"));
    CHECK(workbook_version_core_set_index(state.store)->root_count==2);
    CHECK(app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_STALE); reply_free(r);
    r=run(&state,&worker,4,JOB_SAVE,&book,path,parent);
    app_versions_reset(&state,&worker); CHECK(!state.store && !state.owner.session && !state.identity_valid);
    CHECK(app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_STALE);
    CHECK(worker_archive_versions(r)); reply_free(r);
    r=run(&state,&worker,5,JOB_LOAD,NULL,path,parent);
    CHECK(app_versions_loaded(&state,&worker,r,&error) && state.owner.session!=original.session);
    CHECK(workbook_version_core_set_index(state.store)->root_count==3); reply_free(r);
    QueryJob *tag_job=dx_alloc(sizeof(*tag_job)); tag_job->id=6; tag_job->kind=JOB_SAVE; tag_job->path=dx_strdup(path); tag_job->workbook=workbook_snapshot(&book);
    CHECK(app_versions_tag_job(&state,&worker,tag_job,(WorkbookVersionText){"App tag",7},parent,&error));
    worker_submit(&worker,tag_job); double deadline=dx_millis()+20000;
    while (!(r=worker_poll(&worker))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); }
    CHECK(!r->error && worker_archive_tag_plan(r) && app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_APPLIED);
    CHECK(workbook_version_core_set_index(state.store)->root_count==3 && workbook_version_core_set_index(state.store)->tag_count==1);
    CHECK(!strcmp(workbook_version_core_set_index(state.store)->tags[0].label.data,"App tag"));
    CHECK(app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_STALE); reply_free(r);
    uint64_t loaded_owner=state.owner.session;
    CHECK(app_versions_recovered(&state,&worker,&book,state.store,&error));
    CHECK(state.store && state.owner.session!=loaded_owner && !state.identity_valid);
    CHECK(app_versions_recovered(&state,&worker,&book,NULL,&error));
    CHECK(!state.store && state.owner.session && !state.identity_valid);
    Workbook other; workbook_init(&other); CHECK(workbook_add(&other,"Other",&(Analysis){0},&error));
    QueryJob *j=dx_alloc(sizeof(*j)); j->kind=JOB_SAVE; j->path=dx_strdup(path); j->workbook=workbook_snapshot(&other);
    CHECK(app_versions_job(&state,&worker,j,parent,&error) && !state.store && state.owner.document_high==other.id.high && state.owner.document_low==other.id.low);
    worker_job_free(j); workbook_free(&other);
#endif
    app_versions_reset(&state,&worker); worker_stop(&worker); CHECK(!error);
    CHECK(!unlink(path)); if (!access(parent,F_OK)) CHECK(!rmdir(parent)); CHECK(!rmdir(directory));
    free(parent); free(path); workbook_free(&book);
    printf("PASS: %u native App version publication checks\n",checks); return 0;
}
