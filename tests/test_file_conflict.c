#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/versionstate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL file conflict:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static QueryReply *reply(QueryWorker *worker) { QueryReply *r; double deadline=dx_millis()+20000; while (!(r=worker_poll(worker))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); } return r; }
typedef struct { const char *path; bool touched; } Touch;
static bool touch_after_precheck(void *context,WorkbookSaveProgress progress) {
    Touch *touch=context;
    if (!touch->touched && progress.phase==SAVE_PHASE_VALIDATE) {
        struct timeval times[2]; CHECK(!gettimeofday(&times[0],NULL)); times[1]=times[0]; times[1].tv_sec+=3;
        CHECK(!utimes(touch->path,times)); touch->touched=true;
    }
    return true;
}
static QueryJob *job(uint64_t id,JobKind kind,const char *path,const Workbook *book) { QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=kind; j->path=dx_strdup(path); if (book) j->workbook=workbook_snapshot(book); return j; }
int main(void) {
    char parent[]="build/tests/file-conflict-XXXXXX"; CHECK(mkdtemp(parent)); char *requested=dx_printf("%s/book.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Two sessions",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Opened state",&error)); worksheet_record(book.sheets,EDIT_CELLS); CHECK(workbook_save(&book,requested,&error));
    char *path=dx_absolute_path(requested); free(requested); QueryWorker worker; CHECK(worker_start(&worker)); AppVersionState state={0};
    QueryJob *j=job(1,JOB_LOAD,path,NULL); CHECK(app_versions_job(&state,&worker,j,parent,&error)); worker_submit(&worker,j); QueryReply *r=reply(&worker);
    CHECK(!r->error && app_versions_loaded(&state,&worker,r,&error)); workbook_free(&book); book=*r->workbook; workbook_init(r->workbook); reply_free(r);
    CHECK(state.identity_valid && book.path && !strcmp(book.path,path));
    Workbook *other_book=workbook_snapshot(&book);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Unsaved current edit",&error)); worksheet_record(book.sheets,EDIT_CELLS); book.command_revision++; book.dirty=true;
    j=job(2,JOB_SAVE,path,&book); CHECK(app_versions_job(&state,&worker,j,parent,&error));
    WorkbookVersionOwner other={book.id.high,book.id.low,999,VERSION_WORKING_HEAD}; WorkbookVersionReceipt *receipt=NULL;
    CHECK(workbook_version_tag_save((WorkbookVersionSaveRequest){other_book,state.store,other,VERSION_USER_SAVE,path,parent,UINT64_C(64)*1024*1024},
        (WorkbookVersionText){"Other session",13},(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error); CHECK(workbook_version_core_set_index(workbook_archive_receipt_versions(workbook_version_receipt_archive(receipt)))->tag_count==1); workbook_version_receipt_free(receipt); workbook_free(other_book); free(other_book);
    SourceDigest after_other={0}; CHECK(source_digest_file(path,NULL,NULL,&after_other,NULL)==SOURCE_DIGEST_OK);
    SourceFileIdentity initial_identity=state.identity; state.identity=after_other.identity;
    worker_submit(&worker,j); r=reply(&worker);
    state.identity=initial_identity;
    CHECK(r->save_result==WORKBOOK_SAVE_FAILED && r->error && book.dirty);
    SourceDigest preserved={0}; CHECK(source_digest_file(path,NULL,NULL,&preserved,NULL)==SOURCE_DIGEST_OK && preserved.bytes==after_other.bytes && !memcmp(preserved.sha256,after_other.sha256,DX_SOURCE_DIGEST_BYTES)); reply_free(r);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,0,0),"Unsaved current edit") && workbook_version_core_set_index(state.store)->tag_count==0);
    j=job(3,JOB_SAVE,path,&book); CHECK(app_versions_tag_job(&state,&worker,j,(WorkbookVersionText){"Current tag",11},parent,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_FAILED && r->error && strstr(r->error,"Save As") && worker_archive_is_tag_reply(r)); reply_free(r);
    CHECK(source_digest_file(path,NULL,NULL,&preserved,NULL)==SOURCE_DIGEST_OK && !memcmp(preserved.sha256,after_other.sha256,DX_SOURCE_DIGEST_BYTES));
    SourceFileIdentity old=state.identity; state.identity=after_other.identity;
    j=job(4,JOB_SAVE,path,&book); CHECK(app_versions_job(&state,&worker,j,parent,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(r->save_result==WORKBOOK_SAVE_FAILED && r->error && strstr(r->error,"index changed")); reply_free(r);
    CHECK(source_digest_file(path,NULL,NULL,&preserved,NULL)==SOURCE_DIGEST_OK && !memcmp(preserved.sha256,after_other.sha256,DX_SOURCE_DIGEST_BYTES));
    state.identity=old; state.identity_valid=false;
    j=job(5,JOB_SAVE,path,&book); CHECK(!app_versions_job(&state,&worker,j,parent,&error) && error); free(error); error=NULL; worker_job_free(j);
    state.identity=old; state.identity_valid=true;
    j=job(6,JOB_SAVE,path,&book); CHECK(app_versions_replace_job(&state,&worker,j,parent,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(!r->error && r->save_result==WORKBOOK_SAVE_COMPLETE && app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_APPLIED && state.identity_valid); reply_free(r);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Next admitted save",&error)); worksheet_record(book.sheets,EDIT_CELLS); book.command_revision++;
    j=job(7,JOB_SAVE,path,&book); CHECK(app_versions_job(&state,&worker,j,parent,&error)); worker_submit(&worker,j); r=reply(&worker);
    CHECK(!r->error && r->save_result==WORKBOOK_SAVE_COMPLETE && app_versions_saved(&state,&worker,&book,r,&error)==APP_VERSION_APPLIED); reply_free(r);
    Touch touch={.path=path}; receipt=NULL;
    WorkbookVersionSaveControl control={.archive={.core={touch_after_precheck,&touch},.expected_target=&state.identity}};
    CHECK(workbook_version_save((WorkbookVersionSaveRequest){&book,state.store,state.owner,VERSION_USER_SAVE,path,parent,UINT64_C(64)*1024*1024},
        control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && error && touch.touched && strstr(error,"changed")); free(error); error=NULL;
    app_versions_reset(&state,&worker); worker_stop(&worker); workbook_free(&book); CHECK(!error && !unlink(path) && !rmdir(parent)); free(path);
    printf("PASS: %u changed-workbook Save conflict checks\n",checks); return 0;
}
