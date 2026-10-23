#include "workbook/lifecycle.h"
#include "workbook/cell_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL lifecycle:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void base(Workbook *b) {
    workbook_init(b); char *error=NULL;
    CHECK(workbook_add(b,"A",&(Analysis){0},&error)); CHECK(workbook_add(b,"B",&(Analysis){0},&error)); CHECK(!error);
}
static void command(Workbook *b,WorkbookSheetCommand command) {
    char *error=NULL; bool changed=false; uint64_t revision=b->command_revision;
    CHECK(workbook_sheet_command(b,command,&changed,&error)); CHECK(changed && !error && b->dirty && b->command_revision==revision+1);
}
static void edit(Workbook *b,size_t sheet,const char *text) {
    char *error=NULL; CellEditTarget target;
    CHECK(workbook_prepare_cell_edit(b,sheet,3,2,&target,&error));
    CHECK(workbook_apply_cell_edit(b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,text,strlen(text)},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED); CHECK(!error);
}
static void step(Workbook *b,bool redo,bool lifecycle) {
    char *error=NULL; uint64_t revision=b->command_revision;
    WorkbookHistoryResult r=workbook_history_step(b,redo,&error);
    CHECK(r.status==WORKBOOK_HISTORY_APPLIED && r.lifecycle==lifecycle && !error);
    CHECK(b->dirty && b->command_revision==revision+1 && b->active<b->count && !b->sheets[b->active].hidden);
}
static void deletion_and_chronology(void) {
    Workbook b; base(&b); char *error=NULL; uint64_t id=b.sheets[0].id;
    edit(&b,0,"exact 9223372036854775807"); edit(&b,1,"Other sheet");
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){3,3,2,2},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[0],EDIT_FORMAT);
    int table=objects_add_table(&b.sheets[0].objects,8,0,"Table",&(Analysis){.kind=SOURCE_CSV,.path="input.csv"},&error);
    CHECK(table>=0); worksheet_record(&b.sheets[0],EDIT_OBJECTS);
    int pivot=objects_add(&b.sheets[0].objects,8,10,"Pivot",&error); CHECK(pivot>=0); worksheet_record(&b.sheets[0],EDIT_OBJECTS);
    CHECK(objects_bind_source(&b.sheets[0].objects,(size_t)pivot,b.sheets[0].objects.items[table].id,&error)); worksheet_record(&b.sheets[0],EDIT_OBJECTS);
    b.sheets[0].source_versions.high_water=91;
    CellStore *cells=&b.sheets[0].cells; void *owned=cells->cells;
    uint64_t object=b.sheets[0].objects.items[pivot].id,binding=b.sheets[0].objects.items[pivot].source_id;
    b.active=0; command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0});
    CHECK(b.count==1 && b.sheets[0].id!=id);
    step(&b,false,true); CHECK(b.count==2 && b.sheets[0].id==id && b.active==0);
    CHECK(b.sheets[0].cells.cells==owned && !strcmp(cells_get(&b.sheets[0].cells,3,2),"exact 9223372036854775807"));
    CHECK(format_at(&b.sheets[0].formats,3,2).flags==FORMAT_BOLD);
    CHECK(b.sheets[0].objects.items[pivot].id==object && b.sheets[0].objects.items[pivot].source_id==binding && b.sheets[0].source_versions.high_water==91);
    step(&b,true,true); step(&b,false,true);
    /* Preserved analytical history remains the next global action. */
    CHECK(workbook_history_step(&b,false,&error).status==WORKBOOK_HISTORY_DELEGATE && !error);
    Workbook *snapshot=workbook_snapshot(&b); CHECK(snapshot && !snapshot->lifecycle && snapshot->sheets[0].id==id && !workbook_history_available(snapshot,false));
    workbook_free(snapshot); free(snapshot); workbook_free(&b);
}
static void metadata_and_creation(void) {
    Workbook b; base(&b); uint64_t id=b.sheets[0].id; b.active=0;
    edit(&b,0,"Original");
    command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name="Renamed"});
    command(&b,(WorkbookSheetCommand){.operation=SHEET_REORDER,.sheet=0,.destination=1});
    command(&b,(WorkbookSheetCommand){.operation=SHEET_HIDE,.sheet=1});
    CHECK(b.sheets[1].hidden && b.active==0);
    step(&b,false,true); CHECK(!b.sheets[1].hidden && b.active==1);
    step(&b,false,true); CHECK(b.sheets[0].id==id && b.active==0);
    step(&b,false,true); CHECK(!strcmp(b.sheets[0].name,"A"));
    step(&b,false,false); CHECK(!*cells_get(&b.sheets[0].cells,3,2));
    step(&b,true,false); step(&b,true,true); step(&b,true,true); step(&b,true,true);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_SHOW,.sheet=1});
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=1});
    uint64_t duplicate=b.sheets[b.active].id,high=b.sheet_high_water;
    CHECK(duplicate!=id && !strcmp(cells_get(&b.sheets[b.active].cells,3,2),"Original"));
    step(&b,false,true); CHECK(b.count==2 && b.sheet_high_water==high);
    step(&b,true,true); CHECK(b.sheets[b.active].id==duplicate && b.sheet_high_water==high);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Blank",.analysis=&(Analysis){0}});
    uint64_t created=b.sheets[b.active].id; step(&b,false,true); step(&b,true,true);
    CHECK(b.sheets[b.active].id==created && !b.sheets[b.active].cells.count);
    b.user_scale=1.75f; b.reduced_motion=true;
    step(&b,false,true); CHECK(b.user_scale==1.75f && b.reduced_motion);
    edit(&b,0,"Branch"); CHECK(!workbook_history_available(&b,true) && b.sheet_high_water==created);
    workbook_free(&b);
}
static void branch_and_failures(void) {
    Workbook b; base(&b); edit(&b,0,"One"); edit(&b,0,"Two"); step(&b,false,false);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0}); step(&b,false,true);
    CHECK(!b.sheets[0].redo_actions_count); CHECK(workbook_history_available(&b,true));
    char *error=NULL; bool changed=true; uint64_t revision=b.command_revision;
    CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name=" A "},&changed,&error));
    CHECK(!changed && b.command_revision==revision && workbook_history_available(&b,true));
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name="B"},&changed,&error));
    CHECK(!changed && error && b.command_revision==revision && workbook_history_available(&b,true)); free(error); error=NULL;
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0});
    revision=b.command_revision;
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0},&changed,&error));
    CHECK(b.count==1 && error && b.command_revision==revision); free(error); error=NULL;
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_HIDE,.sheet=0},NULL,&error)); free(error); error=NULL;
    step(&b,false,true); edit(&b,1,"New branch"); CHECK(!workbook_history_available(&b,true));
    uint64_t clock=b.command_revision; b.command_revision=UINT64_MAX;
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_REORDER,.sheet=0,.destination=1},NULL,&error));
    CHECK(error && b.sheets[0].id==1); free(error); b.command_revision=clock;
    workbook_free(&b);
}
static void hidden_limits_and_drafts(void) {
    Workbook b; base(&b); char *error=NULL; bool changed=true;
    command(&b,(WorkbookSheetCommand){.operation=SHEET_HIDE,.sheet=0});
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0});
    CHECK(b.count==1 && b.active==0); step(&b,false,true);
    CHECK(b.count==2 && b.sheets[0].hidden && b.active==1);
    step(&b,false,true); CHECK(!b.sheets[0].hidden);
    CellEditTarget draft; CHECK(workbook_prepare_cell_edit(&b,0,3,2,&draft,&error));
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0}); step(&b,false,true);
    CHECK(workbook_apply_cell_edit(&b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,draft,"Old draft",9},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE);
    free(error); error=NULL;
    uint64_t revision=b.command_revision; b.command_revision=UINT64_MAX;
    CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name=" A "},&changed,&error));
    CHECK(!changed && !error && workbook_history_available(&b,true)==false); /* Revision exhaustion blocks replay. */
    b.command_revision=revision;
    CHECK(workbook_history_available(&b,true));
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="a",.analysis=&(Analysis){0}},NULL,&error));
    CHECK(error && b.count==2 && b.command_revision==revision && workbook_history_available(&b,true)); free(error); error=NULL;
    char *name=workbook_unique_sheet_name(&b," a ",&error); CHECK(name && !strcmp(name,"a (2)")); free(name);
    name=workbook_unique_sheet_name(&b,"bad\nname",&error); CHECK(!name && error); free(error); error=NULL;
    b.sheet_high_water=UINT64_MAX;
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=0},NULL,&error));
    CHECK(error && b.count==2 && b.command_revision==revision && workbook_history_available(&b,true)); free(error); error=NULL;
    b.sheet_high_water=2;
    for (unsigned i=2;i<DX_MAX_SHEETS;i++) CHECK(workbook_add(&b,"Fixture",&(Analysis){0},&error));
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Overflow",.analysis=&(Analysis){0}},NULL,&error));
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Overflow",.analysis=&(Analysis){0}},NULL,NULL));
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=0},NULL,NULL));
    CHECK(b.count==DX_MAX_SHEETS && error && b.command_revision==revision); free(error); error=NULL;
    CHECK(!workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_REORDER,.sheet=0,.destination=DX_MAX_SHEETS},NULL,&error));
    CHECK(error && b.command_revision==revision); free(error);
    workbook_free(&b);
}
static void retained_format_budget(void) {
    Workbook b; base(&b); char *error=NULL; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&b.sheets[0].formats,(FormatRange){3,3,2,2},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[0],EDIT_FORMAT);
    WorkbookFormatUsage original,deleted,restored,duplicate,evicted;
    CHECK(workbook_format_usage(&b,NULL,0,&original)==FORMAT_BUDGET_OK && original.current>0);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0});
    CHECK(workbook_format_usage(&b,NULL,0,&deleted)==FORMAT_BUDGET_OK);
    CHECK(!deleted.current && deleted.retained==original.retained);
    Workbook *snapshot=workbook_snapshot(&b); CHECK(snapshot);
    CHECK(workbook_format_usage(snapshot,NULL,0,&restored)==FORMAT_BUDGET_OK && !restored.retained); workbook_free(snapshot); free(snapshot);
    step(&b,false,true);
    CHECK(workbook_format_usage(&b,NULL,0,&restored)==FORMAT_BUDGET_OK && restored.current==original.current && restored.retained==original.retained);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=0});
    CHECK(workbook_format_usage(&b,NULL,0,&duplicate)==FORMAT_BUDGET_OK && duplicate.current==2*original.current && duplicate.retained==original.retained+original.current);
    step(&b,false,true); edit(&b,1,"Abandon duplicate");
    CHECK(workbook_format_usage(&b,NULL,0,&restored)==FORMAT_BUDGET_OK && restored.retained==original.retained);
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0});
    for (unsigned i=0;i<DX_HISTORY;i++) { char name[32]; snprintf(name,sizeof(name),"Eviction %u",i); command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=0,.name=name}); }
    CHECK(workbook_format_usage(&b,NULL,0,&evicted)==FORMAT_BUDGET_OK && !evicted.current && !evicted.retained);
    workbook_free(&b);
}
static void retention_and_ownership(void) {
    Workbook b; base(&b); edit(&b,0,"Old data");
    for (unsigned i=0;i<40;i++) {
        char name[32]; snprintf(name,sizeof(name),"Name %u",i);
        command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=1,.name=name});
    }
    for (unsigned i=0;i<DX_HISTORY;i++) step(&b,false,true);
    CHECK(!strcmp(b.sheets[1].name,"Name 7") && !workbook_history_available(&b,false));
    for (unsigned i=0;i<DX_HISTORY;i++) step(&b,true,true);
    for (unsigned i=0;i<48;i++) {
        command(&b,(WorkbookSheetCommand){.operation=SHEET_DUPLICATE,.sheet=0});
        command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=1});
    }
    CHECK(b.count==2 && b.sheet_high_water==50);
    for (unsigned i=0;i<16;i++) { step(&b,false,true); step(&b,false,true); }
    CHECK(b.count==2 && !workbook_history_available(&b,false));
    /* Dispose a branch with owned detached sheets, then a book with both stacks. */
    edit(&b,0,"Branch"); CHECK(!workbook_history_available(&b,true));
    command(&b,(WorkbookSheetCommand){.operation=SHEET_DELETE,.sheet=0}); step(&b,false,true);
    workbook_free(&b);
}
int main(void) {
    deletion_and_chronology(); metadata_and_creation(); branch_and_failures(); hidden_limits_and_drafts(); retained_format_budget(); retention_and_ownership();
    printf("PASS: %u workbook lifecycle checks\n",checks); return 0;
}
