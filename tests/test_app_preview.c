#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/versionpreview.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL App preview:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static QueryReply *reply(QueryWorker *w) { QueryReply *r; double deadline=dx_millis()+20000; while (!(r=worker_poll(w))) { CHECK(dx_millis()<deadline); dx_sleep_ms(1); } return r; }
static QueryJob *job(uint64_t id) { QueryJob *j=dx_alloc(sizeof(*j)); j->id=id; j->kind=JOB_LOAD; j->path=dx_strdup("Historical version"); return j; }
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static WorkbookVersionCoreSet *tag(Workbook *book,const WorkbookVersionCoreSet *base,WorkbookVersionOwner owner,const char *path,const char *parent,const char *label) {
    WorkbookVersionReceipt *receipt=NULL; char *error=NULL;
    CHECK(workbook_version_tag_save((WorkbookVersionSaveRequest){book,base,owner,VERSION_USER_SAVE,path,parent,UINT64_C(128)*1024*1024},
        (WorkbookVersionText){label,strlen(label)},(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); return next;
}
static QueryReply *preview_read(AppVersionPreviewState *state,AppPreviewContext context,uint64_t id,WorkbookVersionKey root) {
    char *error=NULL; QueryJob *j=job(id); CHECK(app_preview_job(state,context,j,root,&error) && !error); worker_submit(context.worker,j); QueryReply *r=reply(context.worker); CHECK(!r->error); return r;
}
int main(void) {
    char parent[]="build/tests/app-preview-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/book.dxls",parent);
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Actual working owner",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,9000,30,"R1",&error)); worksheet_record(book.sheets,EDIT_CELLS);
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD}; WorkbookVersionCoreSet *base=tag(&book,NULL,owner,path,parent,"V1");
    CHECK(cells_set(&book.sheets[0].cells,9000,30,"R2",&error)); worksheet_record(book.sheets,EDIT_CELLS);
    WorkbookVersionCoreSet *next=tag(&book,base,owner,path,parent,"V2"); workbook_version_core_set_free(base); base=next;
    free(book.path); book.path=dx_absolute_path(path); book.dirty=true;
    AppVersionState versions={.owner=owner,.store=base}; QueryWorker worker; CHECK(worker_start(&worker));
    AppVersionPreviewState state={0}; AppPreviewContext context={&book,&versions,&worker};
    const WorkbookVersionIndex *index=workbook_version_core_set_index(base); WorkbookVersionKey first=index->tags[0].root,second=index->tags[1].root;
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,1,1,&target,&error));
    CHECK(workbook_apply_cell_edit(&book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Undo",4},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    CHECK(workbook_history_step(&book,false,&error).status==WORKBOOK_HISTORY_APPLIED);
    CHECK(workbook_prepare_cell_edit(&book,0,2,2,&target,&error));
    Workbook original=book; Worksheet sheet=book.sheets[0]; AppVersionState original_versions=versions;
    uint64_t undo=workbook_history_peek(&book,false,&error).order,redo=workbook_history_peek(&book,true,&error).order; CHECK(undo && redo);
    QueryJob *j=job(1); versions.owner.mode=VERSION_HISTORICAL_VIEW;
    CHECK(!app_preview_job(&state,context,j,first,&error) && !state.pending.request && !j->native_file.context); clear(&error); worker_job_free(j); versions.owner=owner;
    QueryReply *r=preview_read(&state,context,2,first); versions.owner.session++;
    CHECK(app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_STALE && !state.active && !state.pending.request);
    CHECK(!memcmp(&book,&original,sizeof(book)) && !memcmp(book.sheets,&sheet,sizeof(sheet)) && r->workbook && worker_archive_preview(r)); versions.owner=owner; reply_free(r);
    r=preview_read(&state,context,3,first); app_preview_cancel(&state);
    CHECK(app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_STALE && !state.active && r->workbook); reply_free(r);
    r=preview_read(&state,context,4,first); CHECK(app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_APPLIED && state.active && !state.pending.request);
    CHECK(!r->workbook->count && !worker_archive_preview(r)); reply_free(r);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,9000,30),"R1") && !book.path && !book.dirty && versions.owner.mode==VERSION_HISTORICAL_VIEW && !versions.identity_valid);
    CHECK(!memcmp(app_preview_working(&state),&original,sizeof(original)) && !memcmp(app_preview_working(&state)->sheets,&sheet,sizeof(sheet)));
    CHECK(!memcmp(app_preview_working_versions(&state),&original_versions,sizeof(original_versions)));
    CHECK(workbook_history_peek(app_preview_working(&state),false,&error).order==undo && workbook_history_peek(app_preview_working(&state),true,&error).order==redo);
    uint64_t reader=versions.owner.session; versions.owner.session++;
    CHECK(app_preview_return(&state,context,&error)==APP_PREVIEW_STALE && state.active && !strcmp(cells_get(&book.sheets[0].cells,9000,30),"R1")); versions.owner.session=reader;
    r=preview_read(&state,context,5,second); CHECK(app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_APPLIED && state.active); reply_free(r);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,9000,30),"R2") && versions.owner.session!=reader && !memcmp(app_preview_working(&state),&original,sizeof(original)));
    CHECK(app_preview_return(&state,context,&error)==APP_PREVIEW_APPLIED && !state.active && !state.preview);
    CHECK(!memcmp(&book,&original,sizeof(book)) && !memcmp(book.sheets,&sheet,sizeof(sheet)) && !memcmp(&versions,&original_versions,sizeof(versions)));
    CHECK(workbook_history_peek(&book,false,&error).order==undo && workbook_history_peek(&book,true,&error).order==redo);
    CHECK(workbook_check_cell_edit(&book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Draft retained",14},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_READY);
    /* Same document/index values in a different owning store cannot satisfy a
       pending instance fence, even before the UI rotates its runtime owner. */
    r=preview_read(&state,context,6,first); WorkbookArchiveCandidate *copy=NULL;
    CHECK(workbook_archive_read(path,parent,UINT64_C(128)*1024*1024,(WorkbookArchiveReadControl){0},&copy,&error)==ARCHIVE_READ_READY);
    WorkbookVersionCoreSet *old=versions.store; versions.store=workbook_archive_take_versions(copy); workbook_archive_candidate_free(copy);
    CHECK(versions.store!=old && app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_STALE && !state.active); reply_free(r);
    worker_archive_release_versions(&worker,&versions.store); versions.store=old;
    r=preview_read(&state,context,7,first);
    CHECK(workbook_apply_cell_edit(&book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Newer working edit",18},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    Workbook newer=book; Worksheet newer_sheet=book.sheets[0]; uint64_t newest=workbook_history_peek(&book,false,&error).order;
    CHECK(app_preview_adopt(&state,context,r,&error)==APP_PREVIEW_APPLIED); reply_free(r);
    CHECK(!memcmp(app_preview_working(&state),&newer,sizeof(newer)) && !memcmp(app_preview_working(&state)->sheets,&newer_sheet,sizeof(newer_sheet)));
    CHECK(app_preview_return(&state,context,&error)==APP_PREVIEW_APPLIED && !memcmp(&book,&newer,sizeof(book)) && !memcmp(book.sheets,&newer_sheet,sizeof(newer_sheet)));
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,2,2),"Newer working edit") && workbook_history_peek(&book,false,&error).order==newest);
    CHECK(app_preview_return(&state,context,&error)==APP_PREVIEW_EMPTY && !app_preview_working(&state) && !app_preview_working_versions(&state));
    app_preview_cancel(NULL); app_versions_reset(&versions,&worker); worker_stop(&worker); workbook_free(&book);
    CHECK(!error && !unlink(path) && !rmdir(parent)); free(path);
    printf("PASS: %u exact Working Head suspension/return checks\n",checks); return 0;
}
