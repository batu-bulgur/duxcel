#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_preview.h"
#include "workbook/version_save.h"
#include "workbook/cell_command.h"
#include "workbook/history.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version preview:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { WorkbookVersionPreviewPhase phase; uint64_t minimum; unsigned calls; } Stop;
static bool stop(void *context,WorkbookVersionPreviewPhase phase,uint64_t bytes) { Stop *s=context; s->calls++; return phase==s->phase && bytes>=s->minimum; }
static void clear(char **error) { CHECK(*error); free(*error); *error=NULL; }
static WorkbookVersionCoreSet *tag(Workbook *book,const WorkbookVersionCoreSet *base,WorkbookVersionOwner owner,const char *path,const char *parent,const char *label) {
    WorkbookVersionReceipt *receipt=NULL; char *error=NULL;
    CHECK(workbook_version_tag_save((WorkbookVersionSaveRequest){book,base,owner,VERSION_USER_SAVE,path,parent,UINT64_C(128)*1024*1024},
        (WorkbookVersionText){label,strlen(label)},(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); return next;
}
int main(void) {
    char parent[]="build/tests/version-preview-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/versions.dxls",parent);
    Workbook working; workbook_init(&working); char *error=NULL;
    CHECK(workbook_add(&working,"Manual",&(Analysis){0},&error));
    CHECK(cells_set(&working.sheets[0].cells,9000,30,"Old offscreen state",&error)); worksheet_record(working.sheets,EDIT_CELLS);
    Analysis offline={.kind=SOURCE_PARQUET,.path="/offline/preview-source-does-not-exist.parquet"};
    CHECK(workbook_add(&working,"Offline definitions",&offline,&error)); working.active=0;
    WorkbookVersionOwner writer={working.id.high,working.id.low,482,VERSION_WORKING_HEAD};
    WorkbookVersionCoreSet *base=tag(&working,NULL,writer,path,parent,"V1");
    CHECK(cells_set(&working.sheets[0].cells,9000,30,"Working state",&error)); worksheet_record(working.sheets,EDIT_CELLS);
    WorkbookVersionCoreSet *next=tag(&working,base,writer,path,parent,"V2"); workbook_version_core_set_free(base); base=next;
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&working,0,1,1,&target,&error));
    CHECK(workbook_apply_cell_edit(&working,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Undo me",7},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    CHECK(workbook_history_step(&working,false,&error).status==WORKBOOK_HISTORY_APPLIED);
    CHECK(workbook_history_available(&working,false) && workbook_history_available(&working,true));
    CHECK(workbook_prepare_cell_edit(&working,0,2,2,&target,&error));
    Workbook original=working; Worksheet sheets[2]={working.sheets[0],working.sheets[1]};
    WorkbookHistoryResult undo=workbook_history_peek(&working,false,&error),redo=workbook_history_peek(&working,true,&error);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(base); CHECK(index->root_count==2 && index->tag_count==2);
    WorkbookVersionKey root=index->tags[0].root; WorkbookVersionOwner reader=writer; reader.mode=VERSION_HISTORICAL_VIEW; reader.session++;
    WorkbookVersionPreview *preview=NULL; WorkbookVersionOwner invalid=reader; invalid.session=0;
    CHECK(workbook_version_preview_prepare(base,root,invalid,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_FAILED && !preview); clear(&error);
    invalid=reader; invalid.mode=VERSION_WORKING_HEAD;
    CHECK(workbook_version_preview_prepare(base,root,invalid,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_FAILED && !preview); clear(&error);
    invalid=reader; invalid.document_low++;
    CHECK(workbook_version_preview_prepare(base,root,invalid,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_FAILED && !preview); clear(&error);
    WorkbookVersionKey missing=root; missing.bytes[0]^=0xff;
    CHECK(workbook_version_preview_prepare(base,missing,reader,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_FAILED && !preview); clear(&error);
    for (unsigned phase=VERSION_PREVIEW_VERIFY;phase<=VERSION_PREVIEW_READY;phase++) {
        Stop state={.phase=(WorkbookVersionPreviewPhase)phase};
        CHECK(workbook_version_preview_prepare(base,root,reader,(WorkbookVersionPreviewControl){stop,&state},&preview,&error)==VERSION_PREVIEW_CANCELLED && !preview && state.calls); clear(&error);
        CHECK(!memcmp(&working,&original,sizeof(working)) && !memcmp(working.sheets,sheets,sizeof(sheets)));
    }
    Stop hash={.phase=VERSION_PREVIEW_VERIFY,.minimum=1};
    CHECK(workbook_version_preview_prepare(base,root,reader,(WorkbookVersionPreviewControl){stop,&hash},&preview,&error)==VERSION_PREVIEW_CANCELLED && !preview && hash.calls>1); clear(&error);
    CHECK(!unlink(path)); /* Private admitted roots no longer need the named file. */
    CHECK(workbook_version_preview_prepare(base,root,reader,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_PREPARED && preview && !error);
    const Workbook *book=workbook_version_preview_book(preview);
    CHECK(book && !book->path && !book->dirty && !book->edit_session_id && !book->sheets[0].edit_session_id);
    CHECK(book->id.high==working.id.high && book->id.low==working.id.low && book->sheets[0].id==working.sheets[0].id);
    CHECK(!strcmp(cells_get(&book->sheets[0].cells,9000,30),"Old offscreen state") && !book->sheets[0].cells.undo_count && !book->sheets[0].cells.redo_count);
    CHECK(book->sheets[1].analysis.kind==SOURCE_PARQUET && !strcmp(book->sheets[1].analysis.path,offline.path));
    CHECK(workbook_version_preview_owner(preview)->mode==VERSION_HISTORICAL_VIEW && workbook_version_preview_owner(preview)->session==reader.session);
    CHECK(!memcmp(workbook_version_preview_root(preview)->bytes,root.bytes,DX_VERSION_KEY_BYTES));
    CHECK(!memcmp(&working,&original,sizeof(working)) && !memcmp(working.sheets,sheets,sizeof(sheets)));
    CHECK(workbook_history_peek(&working,false,&error).order==undo.order && workbook_history_peek(&working,true,&error).order==redo.order);
    CHECK(!strcmp(cells_get(&working.sheets[0].cells,9000,30),"Working state") && working.edit_session_id==target.document);
    CHECK(workbook_version_preview_prepare(base,root,reader,(WorkbookVersionPreviewControl){0},&preview,&error)==VERSION_PREVIEW_FAILED && preview); clear(&error);
    workbook_version_core_set_free(base); base=NULL;
    Workbook *taken=workbook_version_preview_take_book(preview); CHECK(taken && !workbook_version_preview_book(preview));
    CHECK(workbook_version_core_set_index(workbook_version_preview_versions(preview))->tag_count==2);
    CHECK(cells_set(&taken->sheets[0].cells,9000,30,"Private projection",&error));
    WorkbookVersionPreview *second=NULL; reader.session++;
    CHECK(workbook_version_preview_prepare(workbook_version_preview_versions(preview),root,reader,(WorkbookVersionPreviewControl){0},&second,&error)==VERSION_PREVIEW_PREPARED && !error);
    CHECK(!strcmp(cells_get(&workbook_version_preview_book(second)->sheets[0].cells,9000,30),"Old offscreen state"));
    workbook_version_preview_free(preview); workbook_free(taken); free(taken); workbook_version_preview_free(second);
    CHECK(!workbook_version_preview_book(NULL) && !workbook_version_preview_owner(NULL) && !workbook_version_preview_root(NULL) && !workbook_version_preview_versions(NULL) && !workbook_version_preview_take_book(NULL));
    workbook_version_preview_free(NULL); workbook_free(&working); CHECK(!error && !rmdir(parent)); free(path);
    printf("PASS: %u isolated historical projection checks\n",checks); return 0;
}
