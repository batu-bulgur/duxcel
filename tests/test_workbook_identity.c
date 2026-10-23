#include "workbook/cell_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL identity:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
int main(void) {
    Workbook book; workbook_init(&book); Analysis blank={0}; char *error=NULL;
    CHECK(workbook_identity_valid(&book)); CHECK(workbook_add(&book,"First",&blank,&error));
    WorkbookId original=book.id; CHECK(original.high || original.low);
    CHECK(book.sheets[0].id==1 && book.sheet_high_water==1);
    CHECK(workbook_add(&book,"Second",&blank,&error)); CHECK(book.sheets[1].id==2);
    CHECK(workbook_rename(&book,0,"Renamed",&error)); CHECK(workbook_reorder(&book,0,1,&error));
    CHECK(book.sheets[1].id==1 && book.sheets[0].id==2);
    CHECK(workbook_duplicate(&book,1,&error)); CHECK(book.sheets[2].id==3 && book.sheet_high_water==3);
    uint64_t ids[]={UINT64_C(9007199254740993),UINT64_C(9007199254740994),UINT64_MAX-1};
    CHECK(workbook_identity_restore(&book,(WorkbookId){UINT64_MAX,UINT64_MAX},UINT64_MAX,ids,3,&error));
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot && workbook_identity_valid(snapshot));
    CHECK(snapshot->id.high==UINT64_MAX && snapshot->id.low==UINT64_MAX && snapshot->sheet_high_water==UINT64_MAX);
    CHECK(!snapshot->edit_session_id);
    for (size_t i=0;i<3;i++) CHECK(snapshot->sheets[i].id==ids[i] && !snapshot->sheets[i].edit_session_id);
    Workbook before=book; uint64_t saved[3]; for (size_t i=0;i<3;i++) saved[i]=book.sheets[i].id;
    CHECK(!workbook_add(&book,"Exhausted",&blank,&error)); CHECK(!memcmp(&before,&book,sizeof(book))); free(error); error=NULL;
    CHECK(!workbook_duplicate(&book,0,&error)); CHECK(!memcmp(&before,&book,sizeof(book))); free(error); error=NULL;
    for (unsigned kind=0;kind<6;kind++) {
        uint64_t bad[]={ids[0],ids[1],ids[2]}; WorkbookId doc=book.id; uint64_t high=book.sheet_high_water; size_t count=3;
        if (kind==0) bad[0]=0; if (kind==1) bad[1]=bad[0]; if (kind==2) high=ids[0];
        if (kind==3) doc=(WorkbookId){0}; if (kind==4) count=2; if (kind==5) count=65;
        CHECK(!workbook_identity_restore(&book,doc,high,bad,count,&error)); CHECK(error);
        CHECK(!memcmp(&before,&book,sizeof(book))); for (size_t i=0;i<3;i++) CHECK(book.sheets[i].id==saved[i]);
        free(error); error=NULL;
    }
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,0,0,&target,&error));
    CHECK(target.workbook.high==book.id.high && target.sheet_id==book.sheets[0].id);
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"X",1};
    CHECK(workbook_apply_cell_edit(snapshot,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE); free(error); error=NULL;
    command.target.workbook.low--; CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE); free(error); error=NULL;
    command.target=target;
    CellEditResult result=workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error);
    CHECK(result.status==CELL_EDIT_APPLIED && book.command_revision==1);
    CHECK(result.before_document_revision==0 && result.after_document_revision==1);
    CHECK(workbook_prepare_cell_edit(&book,0,0,0,&target,&error));
    command.target=target; CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_NOOP);
    CHECK(book.command_revision==1);
    /* A command on another sheet fences an outstanding command globally. */
    CHECK(workbook_prepare_cell_edit(&book,1,0,0,&command.target,&error));
    CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    command.target=target; command.text="Y";
    CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE); free(error); error=NULL;
    book.command_revision=UINT64_MAX;
    CHECK(workbook_prepare_cell_edit(&book,0,0,0,&command.target,&error));
    CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_LIMIT); free(error); error=NULL;
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,0,0),"X") && book.command_revision==UINT64_MAX);
    workbook_free(snapshot); free(snapshot); workbook_free(&book);
    CHECK(workbook_add(&book,"Independent",&blank,&error)); CHECK(book.id.high!=original.high || book.id.low!=original.low);
    workbook_free(&book); free(error);
    printf("PASS: %u workbook identity checks\n",checks); return 0;
}
