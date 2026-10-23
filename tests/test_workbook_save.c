#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/save.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL controlled Save:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    WorkbookSavePhase cancel_phase;
    uint64_t cancel_at;
    WorkbookSaveProgress last;
    unsigned calls;
    bool cancel,complete;
} Progress;
static bool update(void *context,WorkbookSaveProgress progress) {
    Progress *p=context;
    CHECK(progress.total==14 && progress.completed<=progress.total);
    CHECK(!p->calls || (progress.completed>=p->last.completed && progress.phase>=p->last.phase && progress.total==p->last.total));
    CHECK((progress.completed==progress.total)==(progress.phase==SAVE_PHASE_COMPLETE));
    p->calls++; p->last=progress;
    if (progress.phase==SAVE_PHASE_COMPLETE) { p->complete=true; return false; } /* Too late to cancel a published state. */
    return !(p->cancel && progress.phase==p->cancel_phase && progress.completed>=p->cancel_at);
}
static void sql(const char *path,const char *query,bool inspect) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0};
    CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,query,&r)==DuckDBSuccess);
    if (inspect) CHECK(duckdb_row_count(&r)==1 && duckdb_value_int32(&r,0,0)==482);
    duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static void verify(const char *path,WorkbookId id,const char *text,uint64_t revision) {
    Workbook loaded; workbook_init(&loaded); char *error=NULL; CHECK(workbook_load(&loaded,path,&error));
    CHECK(!error && loaded.id.high==id.high && loaded.id.low==id.low && loaded.command_revision==revision);
    CHECK(loaded.count==2 && !strcmp(cells_get(&loaded.sheets[0].cells,0,0),text));
    CHECK(!strcmp(cells_get(&loaded.sheets[1].cells,1,1),"NULL"));
    workbook_free(&loaded);
}
int main(void) {
    char directory[]="build/tests/workbook-save-XXXXXX"; CHECK(mkdtemp(directory));
    char *path=dx_printf("%s/saved.dxls",directory),*fresh=dx_printf("%s/new.dxls",directory),*wal=dx_printf("%s.wal",fresh);
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"A",&(Analysis){0},&error)); CHECK(workbook_add(&book,"B",&(Analysis){0},&error));
    CHECK(cells_set(&book.sheets[0].cells,0,0,"9007199254740993.0123456789",&error));
    CHECK(cells_set(&book.sheets[0].cells,1,0,"İstanbul\tline\ntwo",&error));
    CHECK(cells_set(&book.sheets[1].cells,1,1,"NULL",&error));
    /* Two sheets, three cells, one authored column = six row units + eight stages. */
    SheetView *view=&book.sheets[0].view; view->column_count=1;
    view->columns=dx_alloc(sizeof(*view->columns)); view->columns[0]=dx_strdup("Field");
    view->widths=dx_alloc(sizeof(*view->widths)); view->widths[0]=120;
    book.command_revision=7; book.dirty=true;
    CHECK(workbook_save(&book,path,&error)); sql(path,"CREATE TABLE unrelated AS SELECT 482 AS value",false);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"replacement",&error)); book.command_revision=8;
    struct { WorkbookSavePhase phase; uint64_t at; } cancellations[]={
        {SAVE_PHASE_VALIDATE,0},{SAVE_PHASE_VALIDATE,1},{SAVE_PHASE_PREPARE,2},
        {SAVE_PHASE_WRITE,3},{SAVE_PHASE_WRITE,6},{SAVE_PHASE_PUBLISH,13}};
    for (size_t i=0;i<sizeof(cancellations)/sizeof(*cancellations);i++) {
        for (unsigned existing=0;existing<2;existing++) {
            Progress progress={.cancel=true,.cancel_phase=cancellations[i].phase,.cancel_at=cancellations[i].at};
            CHECK(workbook_save_controlled(&book,existing?path:fresh,(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_CANCELLED);
            CHECK(error && !progress.complete && progress.calls); free(error); error=NULL;
            CHECK(book.command_revision==8 && book.dirty && !strcmp(cells_get(&book.sheets[0].cells,0,0),"replacement"));
            CHECK(access(fresh,F_OK) && access(wal,F_OK));
            verify(path,book.id,"9007199254740993.0123456789",7);
            sql(path,"SELECT value FROM unrelated",true);
        }
    }
    Progress progress={0}; CHECK(workbook_save_controlled(&book,path,(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_COMPLETE);
    CHECK(progress.complete && progress.last.completed==14 && !error && book.dirty);
    verify(path,book.id,"replacement",8); sql(path,"SELECT value FROM unrelated",true);
    /* Failure is distinct from user cancellation, without a false completion. */
    progress=(Progress){0}; CHECK(workbook_save_controlled(&book,"/does-not-exist/workbook.dxls",(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_FAILED);
    CHECK(error && !progress.complete); free(error); error=NULL;
    sql(fresh,"CREATE TABLE unrelated AS SELECT 482 AS value",false);
    progress=(Progress){0}; CHECK(workbook_save_controlled(&book,fresh,(WorkbookSaveControl){update,&progress},&error)==WORKBOOK_SAVE_FAILED);
    CHECK(error && !progress.complete); free(error); sql(fresh,"SELECT value FROM unrelated",true);
    workbook_free(&book); CHECK(!unlink(path) && !unlink(fresh) && !rmdir(directory)); free(path); free(fresh); free(wal);
    printf("PASS: %u controlled Save checks\n",checks); return 0;
}
