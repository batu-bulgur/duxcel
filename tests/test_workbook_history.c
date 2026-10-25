#include "workbook/history.h"
#include "workbook/lifecycle.h"
#include "workbook/cell_command.h"
#include "workbook/workbook_width_command.h"
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
static WorkbookWidthResult width(Workbook *b,size_t sheet,float value) {
    uint64_t request=17; WorkbookWidthContext c={.book=b,.request_epoch=&request,.reserved=SIZE_MAX,.ready=true};
    WorkbookWidthTarget target; CHECK(workbook_prepare_width_command(&c,sheet,&target,NULL));
    ColumnWidthEntry entry={16383,column_width_bits(value)};
    return workbook_apply_width_command_recorded(&c,&target,(ColumnWidthView){&entry,1},NULL,NULL);
}
static bool denied(void *context,size_t staging,size_t retained,char **error) { (void)context; (void)error; CHECK(staging && retained); return false; }
static bool relocate(void *context,size_t staging,size_t retained,char **error) {
    (void)staging; (void)retained; (void)error; Workbook *b=context; Worksheet *s=dx_alloc(b->count*sizeof(*s)); memcpy(s,b->sheets,b->count*sizeof(*s)); free(b->sheets); b->sheets=s; return true;
}
static unsigned cancelled;
static bool reserve_relocate(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    CHECK(record->view.effects==16 && column_width_valid(record->view.widths.before) && column_width_valid(record->view.widths.after));
    relocate(context,0,0,error); *ticket=dx_alloc(1); return true;
}
static void publish_unexpected(void *context,void *ticket) { (void)context; free(ticket); CHECK(false); }
static void cancel_relocated(void *context,void *ticket) { (void)context; free(ticket); cancelled++; }
static void widths(void) {
    Workbook b; workbook_init(&b); add(&b,"Physical"); add(&b,"Other");
    CHECK(width(&b,0,1200).status==COLUMN_WIDTH_OK && b.sheets[0].width_revision==1);
    CHECK(b.sheets[0].view.column_count==16384 && b.sheets[0].view.widths[16383]==1200);
    edit(&b,1,"Other edit"); CHECK(width(&b,0,1199).status==COLUMN_WIDTH_OK);
    CHECK(workbook_history_peek(&b,false,NULL).kind==EDIT_WIDTHS);
    step(&b,false,0); CHECK(b.sheets[0].view.widths[16383]==1200 && b.sheets[0].width_revision==3);
    step(&b,false,1); step(&b,false,0); CHECK(b.sheets[0].view.widths[16383]==96 && b.sheets[0].width_revision==4);
    step(&b,true,0); step(&b,true,1); step(&b,true,0);
    step(&b,false,0); uint64_t revision=b.command_revision;
    CHECK(width(&b,0,1200).status==COLUMN_WIDTH_NOOP && b.command_revision==revision && column_width_history_info(b.sheets[0].width_history).redo==1);
    edit(&b,1,"Branch"); CHECK(!column_width_history_info(b.sheets[0].width_history).redo && workbook_history_peek(&b,true,NULL).status==WORKBOOK_HISTORY_EMPTY);
    WorkbookFormatUsage usage; CHECK(workbook_format_usage(&b,NULL,0,&usage)==FORMAT_BUDGET_OK && usage.retained>=column_width_history_allocation()+column_width_history_info(b.sheets[0].width_history).owned_bytes);
    Workbook *copy=workbook_snapshot(&b); CHECK(copy && !copy->sheets[0].width_history && !copy->sheets[0].width_revision && copy->sheets[0].view.widths!=b.sheets[0].view.widths && copy->sheets[0].view.widths[16383]==1200); workbook_free(copy); free(copy);
    uint64_t request=17; WorkbookWidthContext c={.book=&b,.request_epoch=&request,.reserved=SIZE_MAX,.ready=true,.admit=denied};
    WorkbookWidthTarget target; CHECK(workbook_prepare_width_command(&c,0,&target,NULL));
    ColumnWidthEntry entry={16383,column_width_bits(1000)}; ColumnWidthView next={&entry,1};
    ColumnWidthHistoryInfo before=column_width_history_info(b.sheets[0].width_history); revision=b.command_revision;
    CHECK(workbook_apply_width_command_recorded(&c,&target,next,NULL,NULL).status==COLUMN_WIDTH_LIMIT && b.command_revision==revision && column_width_history_info(b.sheets[0].width_history).generation==before.generation);
    c.admit=relocate; c.admission_context=&b;
    CHECK(workbook_apply_width_command_recorded(&c,&target,next,NULL,NULL).status==COLUMN_WIDTH_OK && b.sheets[0].view.widths[16383]==1000);
    c.admit=NULL; CHECK(workbook_prepare_width_command(&c,0,&target,NULL));
    entry.bits=column_width_bits(999); revision=b.command_revision;
    WorkbookHistoryRangeRecorder recorder={reserve_relocate,publish_unexpected,cancel_relocated,&b};
    CHECK(workbook_apply_width_command_recorded(&c,&target,next,&recorder,NULL).status==COLUMN_WIDTH_STALE && cancelled==1 && b.command_revision==revision && b.sheets[0].view.widths[16383]==1000);
    CHECK(workbook_prepare_width_command(&c,0,&target,NULL)); ColumnWidthHistory *old=b.sheets[0].width_history,*replacement=NULL;
    CHECK(column_width_history_new((ColumnWidthHistoryOptions){32,DX_COLUMN_WIDTH_HISTORY_BYTES},column_width_history_allocation(),&replacement)==COLUMN_WIDTH_OK);
    b.sheets[0].width_history=replacement; CHECK(workbook_apply_width_command_recorded(&c,&target,next,NULL,NULL).status==COLUMN_WIDTH_STALE); b.sheets[0].width_history=old; column_width_history_free(&replacement);
    for (unsigned i=0;i<40;i++) CHECK(width(&b,0,100+i).status==COLUMN_WIDTH_OK);
    CHECK(b.sheets[0].history_floor && column_width_history_info(b.sheets[0].width_history).undo==32);
    for (unsigned i=0;i<32;i++) step(&b,false,0);
    CHECK(workbook_history_peek(&b,false,NULL).status==WORKBOOK_HISTORY_EMPTY);
    workbook_free(&b);
    workbook_init(&b); add(&b,"Columns");
    CHECK(width(&b,0,1200).status==COLUMN_WIDTH_OK);
    CHECK(worksheet_column(b.sheets,0,true,0,NULL)); workbook_history_note_edit(&b);
    CHECK(b.sheets[0].view.widths[16383]==96 && !b.sheets[0].undo_columns[b.sheets[0].undo_actions_count-1].active);
    CHECK(workbook_history_peek(&b,false,NULL).kind==EDIT_CELLS_WIDTHS);
    step(&b,false,0); CHECK(b.sheets[0].view.widths[16383]==1200);
    step(&b,true,0); CHECK(b.sheets[0].view.widths[16383]==96);
    CHECK(worksheet_column(b.sheets,0,false,0,NULL)); workbook_history_note_edit(&b);
    CHECK(workbook_history_peek(&b,false,NULL).status==WORKBOOK_HISTORY_READY);
    workbook_free(&b);
}
/* Public-producer regression: newer constituent Redo belongs to a detached
   duplicate while its lifecycle action is undone. It must keep the branch alive. */
static void detached_duplicate_chronology(void) {
    Workbook b; workbook_init(&b); Analysis source={.kind=SOURCE_CSV,.path="/reference.csv"};
    CHECK(workbook_add(&b,"Source",&source,NULL)); edit(&b,0,"Original");
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,NULL));
    worksheet_record(&b.sheets[0],EDIT_FORMAT); workbook_history_note_edit(&b);
    CHECK(width(&b,0,190).status==COLUMN_WIDTH_OK);
    b.sheets[0].source_versions.high_water=37;
    uint64_t original=b.sheets[0].id,revision=b.command_revision;
    CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=0},NULL,NULL));
    CHECK(b.count==2 && b.active==1 && b.command_revision==revision+1);
    uint64_t duplicate=b.sheets[1].id,high=b.sheet_high_water;
    CHECK(duplicate!=original && high==duplicate && b.sheets[1].analysis.path!=b.sheets[0].analysis.path);
    CHECK(!strcmp(b.sheets[1].analysis.path,"/reference.csv") && b.sheets[1].source_versions.high_water==37);
    CHECK(format_at(&b.sheets[1].formats,1,1).flags==FORMAT_BOLD && b.sheets[1].view.widths[16383]==190);
    edit(&b,1,"Edited duplicate"); CHECK(width(&b,1,321).status==COLUMN_WIDTH_OK);
    uint64_t order=worksheet_history_high_water(),branch=b.history_branch;
    CHECK(workbook_history_peek(&b,false,NULL).kind==EDIT_WIDTHS);
    step(&b,false,1); CHECK(b.sheets[1].view.widths[16383]==190);
    step(&b,false,1); CHECK(!strcmp(cells_get(&b.sheets[1].cells,1,1),"Original"));
    CHECK(b.sheets[1].redo_actions_count==2 && column_width_history_info(b.sheets[1].width_history).redo==1);
    revision=b.command_revision; WorkbookHistoryResult target=workbook_history_peek(&b,false,NULL);
    CHECK(target.status==WORKBOOK_HISTORY_READY && target.lifecycle && target.sheet_id==duplicate);
    CHECK(workbook_history_step(&b,false,NULL).status==WORKBOOK_HISTORY_APPLIED && b.command_revision==revision+1);
    CHECK(b.count==1 && b.sheets[0].id==original && b.sheet_high_water==high);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"Original") && b.sheets[0].view.widths[16383]==190 && format_at(&b.sheets[0].formats,1,1).flags==FORMAT_BOLD);
    target=workbook_history_peek(&b,true,NULL);
    CHECK(target.status==WORKBOOK_HISTORY_READY && target.lifecycle && target.sheet_id==duplicate);
    CHECK(b.history_branch==branch && worksheet_history_high_water()==order);
    revision=b.command_revision;
    CHECK(workbook_history_step(&b,true,NULL).status==WORKBOOK_HISTORY_APPLIED && b.command_revision==revision+1);
    CHECK(b.count==2 && b.sheets[1].id==duplicate && b.sheets[0].id==original && b.sheet_high_water==high);
    CHECK(b.sheets[1].redo_actions_count==2 && !strcmp(cells_get(&b.sheets[1].cells,1,1),"Original") && b.sheets[1].view.widths[16383]==190);
    CHECK(b.sheets[1].analysis.kind==SOURCE_CSV && !strcmp(b.sheets[1].analysis.path,"/reference.csv") && b.sheets[1].source_versions.high_water==37);
    CHECK(format_at(&b.sheets[1].formats,1,1).flags==FORMAT_BOLD);
    step(&b,true,1); CHECK(!strcmp(cells_get(&b.sheets[1].cells,1,1),"Edited duplicate"));
    step(&b,true,1); CHECK(b.sheets[1].view.widths[16383]==321);
    CHECK(!workbook_history_available(&b,true) && !b.sheets[1].redo_actions_count && !column_width_history_info(b.sheets[1].width_history).redo);
    CHECK(b.sheet_high_water==high && b.sheets[1].source_versions.high_water==37 && b.history_branch==branch && worksheet_history_high_water()==order);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,1),"Original") && b.sheets[0].view.widths[16383]==190 && format_at(&b.sheets[1].formats,1,1).flags==FORMAT_BOLD);
    workbook_free(&b);
}
int main(void) {
    detached_duplicate_chronology(); widths(); chronology(); branching_and_identity(); mixed_history_and_legacy_branch(); composite_and_abandoned_styles(); missing_future_barrier(); bounds_and_failures();
    printf("PASS: %u workbook chronology checks\n",checks); return 0;
}
