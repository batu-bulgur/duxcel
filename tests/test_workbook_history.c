#include "workbook/history.h"
#include "workbook/cell_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL workbook history:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void add(Workbook *b,const char *name) { char *error=NULL; CHECK(workbook_add(b,name,&(Analysis){0},&error)); CHECK(!error); }
static void edit(Workbook *b,size_t sheet,const char *text) {
    char *error=NULL; CellEditTarget target;
    CHECK(workbook_prepare_cell_edit(b,sheet,1,1,&target,&error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,text,strlen(text)};
    CHECK(workbook_apply_cell_edit(b,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED); CHECK(!error);
}
static WorkbookHistoryResult step(Workbook *b,bool redo,size_t sheet) {
    char *error=NULL; uint64_t revision=b->command_revision;
    WorkbookHistoryResult target=workbook_history_peek(b,redo,&error); CHECK(target.status==WORKBOOK_HISTORY_READY && target.sheet==sheet);
    WorkbookHistoryResult result=workbook_history_step(b,redo,&error);
    CHECK(result.status==WORKBOOK_HISTORY_APPLIED && result.sheet_id==b->sheets[sheet].id && !error);
    CHECK(b->command_revision==revision+1 && b->dirty); return result;
}
static void reject(Workbook *b,bool redo,WorkbookHistoryStatus expected) {
    Workbook before=*b; Worksheet *sheets=dx_alloc(b->count*sizeof(*sheets)); memcpy(sheets,b->sheets,b->count*sizeof(*sheets)); char *error=NULL;
    CHECK(workbook_history_step(b,redo,&error).status==expected);
    CHECK(!memcmp(&before,b,sizeof(before)) && !memcmp(sheets,b->sheets,b->count*sizeof(*sheets)));
    CHECK((expected==WORKBOOK_HISTORY_EMPTY || expected==WORKBOOK_HISTORY_DELEGATE)==(!error)); free(error); free(sheets);
}
static void chronology(void) {
    Workbook b; workbook_init(&b); add(&b,"A"); add(&b,"B");
    edit(&b,0,"A1"); edit(&b,1,"B1"); edit(&b,0,"A2"); b.active=1;
    CellEditTarget draft; char *error=NULL; CHECK(workbook_prepare_cell_edit(&b,1,1,1,&draft,&error));
    step(&b,false,0); CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"A1") && b.active==1);
    CellEditCommand stale={CELL_EDIT_COMMAND_VERSION,draft,"Stale B",7};
    CHECK(workbook_apply_cell_edit(&b,&stale,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE);
    free(error); error=NULL; CHECK(!strcmp(cells_get(&b.sheets[1].cells,1,1),"B1"));
    step(&b,false,1); CHECK(!*cells_get(&b.sheets[1].cells,1,1));
    step(&b,false,0); CHECK(!*cells_get(&b.sheets[0].cells,1,1)); reject(&b,false,WORKBOOK_HISTORY_EMPTY);
    b.user_scale=1.5f; b.reduced_motion=true;
    step(&b,true,0); step(&b,true,1); step(&b,true,0);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"A2") && !strcmp(cells_get(&b.sheets[1].cells,1,1),"B1"));
    CHECK(b.user_scale==1.5f && b.reduced_motion && b.command_revision==9); reject(&b,true,WORKBOOK_HISTORY_EMPTY);
    Workbook *copy=workbook_snapshot(&b); CHECK(copy && !workbook_history_available(copy,false) && !workbook_history_available(copy,true));
    CHECK(!copy->history_branch && copy->command_revision==b.command_revision); workbook_free(copy); free(copy);
    workbook_free(&b);
}
static void branching_and_identity(void) {
    Workbook b; workbook_init(&b); add(&b,"A"); add(&b,"B"); add(&b,"C");
    edit(&b,0,"A1"); edit(&b,1,"B1"); edit(&b,0,"A2"); step(&b,false,0); step(&b,false,1);
    Workbook before=b; char *error=NULL; CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,1,1,&target,&error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"A1",2};
    CHECK(workbook_apply_cell_edit(&b,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_NOOP);
    CHECK(b.command_revision==before.command_revision && b.sheets[0].redo_actions_count==1 && b.sheets[1].redo_actions_count==1);
    command.text="Wrong"; command.bytes=5;
    CHECK(workbook_apply_cell_edit(&b,&command,(CellEditPolicy){0,0,false},&error).status==CELL_EDIT_PROTECTED); free(error); error=NULL;
    CHECK(workbook_history_available(&b,true));
    edit(&b,2,"C branch"); CHECK(!b.sheets[0].cells.redo_count && !b.sheets[1].cells.redo_count);
    reject(&b,true,WORKBOOK_HISTORY_EMPTY);
    CHECK(workbook_rename(&b,2,"Renamed",&error)); uint64_t owner=b.sheets[2].id;
    CHECK(workbook_reorder(&b,2,0,&error)); CHECK(workbook_history_peek(&b,false,&error).sheet_id==owner);
    CHECK(workbook_duplicate(&b,0,&error)); CHECK(!b.sheets[1].undo_actions_count && b.sheets[1].id!=owner);
    step(&b,false,0); step(&b,false,2); CHECK(!*cells_get(&b.sheets[2].cells,1,1));
    CHECK(!strcmp(cells_get(&b.sheets[1].cells,1,1),"C branch"));
    workbook_free(&b); free(error);
}
static void mixed_history_and_legacy_branch(void) {
    Workbook b; workbook_init(&b); add(&b,"A"); add(&b,"B"); char *error=NULL;
    edit(&b,0,"A"); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&b.sheets[1].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[1],EDIT_FORMAT);
    CHECK(workbook_history_peek(&b,false,&error).kind==EDIT_FORMAT);
    step(&b,false,1); CHECK(!format_at(&b.sheets[1].formats,1,1).flags); step(&b,false,0);
    step(&b,true,0); step(&b,true,1); CHECK(format_at(&b.sheets[1].formats,1,1).flags==FORMAT_BOLD);
    step(&b,false,1); step(&b,false,0);
    /* A legacy new record invalidates prior Redo before Undoing that record. */
    CHECK(cells_set(&b.sheets[1].cells,1,1,"Legacy branch",&error)); worksheet_record(&b.sheets[1],EDIT_CELLS);
    CHECK(!workbook_history_available(&b,true)); step(&b,false,1); step(&b,true,1); reject(&b,true,WORKBOOK_HISTORY_EMPTY);
    CHECK(!*cells_get(&b.sheets[0].cells,1,1) && !format_at(&b.sheets[1].formats,1,1).flags);
    /* A recorded analytical action must delegate, never jump to an older cell. */
    b.sheets[0].undo[b.sheets[0].undo_count++]=(Analysis){0}; worksheet_record(&b.sheets[0],EDIT_ANALYSIS);
    reject(&b,false,WORKBOOK_HISTORY_DELEGATE);
    workbook_free(&b); free(error);
}
static void composite_and_abandoned_styles(void) {
    Workbook b; workbook_init(&b); add(&b,"Values"); add(&b,"Styles"); char *error=NULL;
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(cells_set(&b.sheets[0].cells,1,1,"Composite",&error));
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&error));
    worksheet_record(&b.sheets[0],EDIT_CELLS_FORMAT); b.sheets[0].source_versions.high_water=77;
    CHECK(format_apply(&b.sheets[1].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[1],EDIT_FORMAT);
    step(&b,false,1); step(&b,false,0);
    CHECK(!*cells_get(&b.sheets[0].cells,1,1) && !format_at(&b.sheets[0].formats,1,1).flags);
    CHECK(b.sheets[0].source_versions.high_water==77);
    step(&b,true,0); CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"Composite") && format_at(&b.sheets[0].formats,1,1).flags==FORMAT_BOLD);
    step(&b,false,0); edit(&b,0,"New branch");
    CHECK(!b.sheets[0].formats.redo_count && !b.sheets[1].formats.redo_count && !b.sheets[1].redo_actions_count);
    CHECK(b.sheets[0].source_versions.high_water==77 && !workbook_history_available(&b,true));
    uint64_t revision=b.sheets[0].formats.revision;
    b.sheets[0].formats.revision=UINT64_MAX;
    /* Pure cell replay does not consume or wrap the unrelated format clock. */
    step(&b,false,0); CHECK(b.sheets[0].formats.revision==UINT64_MAX);
    b.sheets[0].formats.revision=revision;
    step(&b,true,0);
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[0],EDIT_FORMAT);
    revision=b.sheets[0].formats.revision; b.sheets[0].formats.revision=UINT64_MAX;
    reject(&b,false,WORKBOOK_HISTORY_LIMIT); b.sheets[0].formats.revision=revision;
    workbook_free(&b); free(error);
}
static void missing_future_barrier(void) {
    Workbook b; workbook_init(&b); add(&b,"A"); add(&b,"B");
    edit(&b,0,"A1"); edit(&b,0,"A2"); edit(&b,0,"A3"); edit(&b,1,"B4");
    step(&b,false,1); step(&b,false,0); step(&b,false,0);
    uint64_t missing=b.sheets[0].redo_order[0];
    /* Model a constituent byte-budget eviction of the newest future. Transfer
       the retained next action and let journal pruning discover the gap. */
    cells_trim_history(&b.sheets[0].cells,true,1);
    CHECK(cells_history(&b.sheets[0].cells,true)); worksheet_transfer_history(&b.sheets[0],true);
    CHECK(b.sheets[0].history_ceiling==missing && !b.sheets[0].history_floor);
    reject(&b,true,WORKBOOK_HISTORY_EMPTY); /* B4 cannot jump missing A3. */
    step(&b,false,0); CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"A1"));
    step(&b,true,0); CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"A2"));
    edit(&b,1,"New future"); CHECK(!b.sheets[0].history_ceiling && !b.sheets[1].redo_actions_count);
    step(&b,false,1); step(&b,true,1);
    workbook_free(&b);
}
static void bounds_and_failures(void) {
    Workbook b; workbook_init(&b); add(&b,"Old"); add(&b,"Busy"); edit(&b,0,"Old value");
    for (unsigned i=0;i<40;i++) { char value[32]; snprintf(value,sizeof(value),"Value %u",i); edit(&b,1,value); }
    CHECK(b.sheets[1].undo_actions_count==DX_HISTORY && b.sheets[1].history_floor);
    for (unsigned i=0;i<DX_HISTORY;i++) step(&b,false,1);
    CHECK(!strcmp(cells_get(&b.sheets[1].cells,1,1),"Value 7"));
    /* Old's earlier entry cannot skip over Busy's discarded interval. */
    reject(&b,false,WORKBOOK_HISTORY_EMPTY); for (unsigned i=0;i<DX_HISTORY;i++) step(&b,true,1);
    uint64_t revision=b.command_revision; b.command_revision=UINT64_MAX; reject(&b,false,WORKBOOK_HISTORY_LIMIT); b.command_revision=revision;
    revision=b.sheets[1].cells.revision; b.sheets[1].cells.revision=UINT64_MAX; reject(&b,false,WORKBOOK_HISTORY_LIMIT); b.sheets[1].cells.revision=revision;
    uint64_t order=b.sheets[1].undo_order[31]; b.sheets[1].undo_order[31]=0; reject(&b,false,WORKBOOK_HISTORY_INVALID); b.sheets[1].undo_order[31]=order;
    uint64_t floor=b.sheets[1].history_floor; b.sheets[1].history_floor=UINT64_MAX; reject(&b,false,WORKBOOK_HISTORY_INVALID); b.sheets[1].history_floor=floor;
    b.sheets[1].cells.undo_count--; reject(&b,false,WORKBOOK_HISTORY_INVALID); b.sheets[1].cells.undo_count++;
    uint64_t id=b.sheets[1].id; b.sheets[1].id=b.sheets[0].id; reject(&b,false,WORKBOOK_HISTORY_INVALID); b.sheets[1].id=id;
    workbook_free(&b);
}
int main(void) {
    chronology(); branching_and_identity(); mixed_history_and_legacy_branch(); composite_and_abandoned_styles(); missing_future_barrier(); bounds_and_failures();
    printf("PASS: %u workbook chronology checks\n",checks); return 0;
}
