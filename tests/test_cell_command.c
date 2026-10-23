#include "workbook/cell_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell command:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const CellEditPolicy manual={0,0,true};
static CellEditTarget prepare(Workbook *book,size_t sheet,uint64_t row,size_t col) {
    CellEditTarget target={0}; char *error=NULL;
    CHECK(workbook_prepare_cell_edit(book,sheet,row,col,&target,&error)); CHECK(!error);
    return target;
}
static CellEditResult apply(Workbook *book,CellEditTarget target,const char *text,CellEditStatus expected) {
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,text,text ? strlen(text) : 0}; char *error=NULL;
    CellEditResult result=workbook_apply_cell_edit(book,&command,manual,&error);
    CHECK(result.status==expected); CHECK((expected<=CELL_EDIT_NOOP)==(error==NULL)); free(error); return result;
}
static void reject(Workbook *book,CellEditCommand command,CellEditPolicy policy,CellEditStatus expected) {
    Workbook before=*book; Worksheet *copy=dx_alloc(book->count*sizeof(*copy));
    memcpy(copy,book->sheets,book->count*sizeof(*copy)); char *error=NULL;
    CellEditResult result=workbook_apply_cell_edit(book,&command,policy,&error);
    CHECK(result.status==expected); CHECK(error!=NULL);
    CHECK(!memcmp(&before,book,sizeof(before))); CHECK(!memcmp(copy,book->sheets,book->count*sizeof(*copy)));
    free(error); free(copy);
}
static void basic_and_history(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"First",&blank,&error)); book.dirty=false;
    CellEditTarget target=prepare(&book,0,1,1); CHECK(!book.dirty && target.document && target.sheet);
    char value[]="12345678901234567890.0123456789";
    CellEditResult result=apply(&book,target,value,CELL_EDIT_APPLIED); value[0]='x';
    Worksheet *s=&book.sheets[0]; CHECK(result.before_revision==0 && result.after_revision==1);
    CHECK(book.dirty && s->undo_actions_count==1 && s->cells.undo_count==1);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123456789"));
    target=prepare(&book,0,1,1); book.dirty=false;
    result=apply(&book,target,cells_get(&s->cells,1,1),CELL_EDIT_NOOP);
    CHECK(!book.dirty && s->undo_actions_count==1 && s->cells.undo_count==1);
    CHECK(result.before_revision==result.after_revision);
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false);
    CHECK(s->redo_actions_count==1 && !*cells_get(&s->cells,1,1));
    target=prepare(&book,0,1,1); apply(&book,target,NULL,CELL_EDIT_NOOP);
    CHECK(!book.dirty && s->redo_actions_count==1 && s->cells.redo_count==1);
    CHECK(cells_history(&s->cells,true)); worksheet_transfer_history(s,true);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123456789"));
    target=prepare(&book,0,1,1); apply(&book,target,"NULL",CELL_EDIT_APPLIED);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL"));
    /* All cell-store changes fence a draft, including Undo and unrelated cells. */
    CellEditTarget stale=target; CellEditCommand command={CELL_EDIT_COMMAND_VERSION,stale,"NULL",4};
    reject(&book,command,manual,CELL_EDIT_STALE);
    target=prepare(&book,0,1,1); apply(&book,prepare(&book,0,2,2),"Other",CELL_EDIT_APPLIED);
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    target=prepare(&book,0,1,1); apply(&book,target,"",CELL_EDIT_APPLIED);
    CHECK(!*cells_get(&s->cells,1,1));
    CHECK(cells_history(&s->cells,false)); worksheet_transfer_history(s,false);
    CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL"));
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    workbook_free(&book); free(error);
}
static void identity_and_validation(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"First",&blank,&error));
    CellEditTarget target=prepare(&book,0,1,1);
    CHECK(workbook_add(&book,"Second",&blank,&error));
    CellEditTarget other=prepare(&book,1,1,1); CHECK(target.sheet!=other.sheet);
    CHECK(workbook_rename(&book,0,"Renamed",&error)); CHECK(workbook_reorder(&book,0,1,&error));
    apply(&book,target,"Original owner",CELL_EDIT_APPLIED);
    CHECK(!strcmp(cells_get(&book.sheets[1].cells,1,1),"Original owner"));
    CHECK(!*cells_get(&book.sheets[0].cells,1,1));
    CHECK(workbook_duplicate(&book,1,&error));
    CellEditTarget duplicate=prepare(&book,book.active,1,1); CHECK(duplicate.sheet!=target.sheet);
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot!=NULL);
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,duplicate,"No",2};
    reject(snapshot,command,manual,CELL_EDIT_STALE);
    CellEditTarget copied=prepare(snapshot,snapshot->active,1,1); CHECK(copied.document!=duplicate.document);
    reject(snapshot,command,manual,CELL_EDIT_STALE); workbook_free(snapshot); free(snapshot);
    command.target=prepare(&book,1,3,3);
    command.version=0; reject(&book,command,manual,CELL_EDIT_INVALID); command.version=CELL_EDIT_COMMAND_VERSION;
    command.text=NULL; reject(&book,command,manual,CELL_EDIT_INVALID);
    char embedded[]={'A',0,'B'}; command.text=embedded; command.bytes=3; reject(&book,command,manual,CELL_EDIT_INVALID);
    command.text="X"; command.bytes=65537; reject(&book,command,manual,CELL_EDIT_INVALID);
    command.bytes=1; CellEditPolicy blocked={0,0,false}; reject(&book,command,blocked,CELL_EDIT_PROTECTED);
    blocked=(CellEditPolicy){5,5,true}; reject(&book,command,blocked,CELL_EDIT_PROTECTED);
    uint64_t original_row=command.target.row; command.target.row=DX_SHEET_ROWS; reject(&book,command,manual,CELL_EDIT_INVALID); command.target.row=original_row;
    command.target.column=16384; reject(&book,command,manual,CELL_EDIT_INVALID); command.target.column=3;
    Worksheet *s=&book.sheets[1]; s->cells.revision=UINT64_MAX;
    command.target=prepare(&book,1,3,3); reject(&book,command,manual,CELL_EDIT_LIMIT);
    apply(&book,command.target,"",CELL_EDIT_NOOP);
    CellEditTarget unchanged={0}; CHECK(!workbook_prepare_cell_edit(&book,99,0,0,&unchanged,&error)); CHECK(!unchanged.document); free(error); error=NULL;
    uint64_t doc=target.document; workbook_free(&book); workbook_init(&book);
    CHECK(workbook_add(&book,"New document",&blank,&error));
    command.target=target; reject(&book,command,manual,CELL_EDIT_STALE);
    CHECK(prepare(&book,0,1,1).document!=doc); workbook_free(&book); free(error);
}
static void object_and_capacity(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_add(&book,"Bounded",&blank,&error)); Worksheet *s=&book.sheets[0];
    s->objects.items=dx_alloc(sizeof(*s->objects.items)); s->objects.count=1; s->objects.next_id=1;
    s->objects.items[0]=(SheetObject){.id=1,.row=10,.col=10,.rows=5,.columns=5,.kind=OBJECT_PIVOT,.name=dx_strdup("Pivot")};
    CHECK(objects_valid(&s->objects,&error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,prepare(&book,0,11,11),"X",1};
    reject(&book,command,manual,CELL_EDIT_PROTECTED);
    command.target=prepare(&book,0,9,9); apply(&book,command.target,"Outside",CELL_EDIT_APPLIED);
    workbook_free(&book); workbook_init(&book); CHECK(workbook_add(&book,"Full",&blank,&error)); s=&book.sheets[0];
    s->cells.cells=dx_alloc(DX_MAX_EDIT_CELLS*sizeof(*s->cells.cells)); s->cells.count=s->cells.capacity=DX_MAX_EDIT_CELLS;
    for (size_t i=0;i<s->cells.count;i++) s->cells.cells[i]=(SheetCell){i,0,dx_strdup("X")};
    s->cells.bytes=2*s->cells.count;
    command.target=prepare(&book,0,DX_MAX_EDIT_CELLS,0); reject(&book,command,manual,CELL_EDIT_LIMIT);
    /* A byte span is copied before mutation; its source need not terminate. */
    char span[]={'O','K'}; command=(CellEditCommand){CELL_EDIT_COMMAND_VERSION,prepare(&book,0,0,0),span,2};
    CHECK(workbook_apply_cell_edit(&book,&command,manual,&error).status==CELL_EDIT_APPLIED);
    span[0]='N'; CHECK(!strcmp(cells_get(&s->cells,0,0),"OK"));
    workbook_free(&book); free(error);
}
static void pure_admission(void) {
    Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Pure",&(Analysis){0},&error));
    CellEditCommand c={CELL_EDIT_COMMAND_VERSION,prepare(&book,0,1,1),"New",3};
    Workbook before=book; Worksheet sheet=book.sheets[0];
    CellEditResult ready=workbook_check_cell_edit(&book,&c,manual,&error);
    CHECK(ready.status==CELL_EDIT_READY && !error && ready.before_revision==ready.after_revision && ready.before_document_revision==ready.after_document_revision);
    CHECK(!memcmp(&before,&book,sizeof(book)) && !memcmp(&sheet,book.sheets,sizeof(sheet)));
    CHECK(workbook_apply_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_APPLIED);
    before=book; sheet=book.sheets[0];
    CHECK(workbook_check_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_STALE && error); free(error); error=NULL;
    CHECK(!memcmp(&before,&book,sizeof(book)) && !memcmp(&sheet,book.sheets,sizeof(sheet)));
    c.target=prepare(&book,0,1,1);
    CHECK(workbook_check_cell_edit(&book,&c,manual,&error).status==CELL_EDIT_NOOP && !error);
    workbook_free(&book);
}
int main(void) {
    basic_and_history(); identity_and_validation(); object_and_capacity(); pure_admission();
    printf("PASS: %u cell command checks\n",checks); return 0;
}
