/* Feature-only real App fixture. Production contains no hook or stage state. */
static unsigned identity_stage,identity_wait,identity_checks;
static int identity_result;
static WorkbookId identity_document;
static uint64_t identity_original,identity_duplicate;
static CellEditTarget identity_old_target;
#define ID_GUI_CHECK(x) do { identity_checks++; if (!(x)) { fprintf(stderr,"FAIL: identity GUI stage %u line %d: %s (%s)\n",identity_stage,__LINE__,#x,a->message ? a->message : ""); identity_result=-1; return; } } while (0)
static void workbook_identity_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *sheet=active_sheet(a);
    if (identity_result || a->busy || a->io_busy || !sheet || ++identity_wait<12) return;
    identity_wait=0;
    switch (identity_stage) {
    case 0:
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL);
        input_set(&a->input,"Current original"); ui_note_key_press(KEY_ENTER,0); break;
    case 1:
        ID_GUI_CHECK(a->modal==MODAL_NONE && a->book.count==1 && a->book.dirty && workbook_identity_valid(&a->book));
        ID_GUI_CHECK(!strcmp(cells_get(&sheet->cells,1,1),"Current original"));
        identity_document=a->book.id; identity_original=sheet->id;
        begin_sheet_rename(a,0); input_set(&a->input,"Renamed"); ui_note_key_press(KEY_ENTER,0); break;
    case 2:
        ID_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(sheet->name,"Renamed") && sheet->id==identity_original);
        a->sheet_menu_index=0; a->sheet_action=SHEET_ACTION_DUPLICATE; process_sheet_action(a); break;
    case 3:
        ID_GUI_CHECK(a->book.count==2 && a->book.active==1 && sheet->id!=identity_original);
        identity_duplicate=sheet->id; ID_GUI_CHECK(identity_duplicate>identity_original && a->book.sheet_high_water==identity_duplicate);
        a->sheet_menu_index=1; a->sheet_action=SHEET_ACTION_UP; process_sheet_action(a);
        ID_GUI_CHECK(a->book.active==0 && active_sheet(a)->id==identity_duplicate);
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Duplicate only"); ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        ID_GUI_CHECK(!strcmp(cells_get(&sheet->cells,1,1),"Duplicate only"));
        ID_GUI_CHECK(!strcmp(cells_get(&a->book.sheets[1].cells,1,1),"Current original"));
        identity_old_target=a->cell_edit_target;
        workspace_job(a,JOB_SAVE,"build/tests/generated/workbook-identity-demo.dxls"); break;
    case 5:
        ID_GUI_CHECK(!a->book.dirty && a->book.id.high==identity_document.high && a->book.id.low==identity_document.low);
        workspace_job(a,JOB_LOAD,"build/tests/generated/workbook-identity-demo.dxls"); break;
    case 6: {
        ID_GUI_CHECK(!a->book.dirty && a->book.count==2 && a->book.sheet_high_water==identity_duplicate);
        ID_GUI_CHECK(a->book.id.high==identity_document.high && a->book.id.low==identity_document.low);
        ID_GUI_CHECK(sheet->id==identity_duplicate && a->book.sheets[1].id==identity_original);
        ID_GUI_CHECK(!strcmp(sheet->name,"Renamed (2)") && !strcmp(cells_get(&sheet->cells,1,1),"Duplicate only"));
        CellEditCommand command={CELL_EDIT_COMMAND_VERSION,identity_old_target,"Stale",5}; char *error=NULL;
        ID_GUI_CHECK(workbook_apply_cell_edit(&a->book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE);
        free(error); ID_GUI_CHECK(!a->book.dirty && !sheet->undo_actions_count);
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"After reopen"); ui_note_key_press(KEY_ENTER,0); break;
    }
    case 7:
        ID_GUI_CHECK(!strcmp(cells_get(&sheet->cells,1,1),"After reopen") && sheet->undo_actions_count==1);
        ID_GUI_CHECK(!strcmp(cells_get(&a->book.sheets[1].cells,1,1),"Current original"));
        workspace_job(a,JOB_SAVE,"build/tests/generated/workbook-identity-demo.dxls"); break;
    case 8:
        ID_GUI_CHECK(!a->book.dirty && workbook_identity_valid(&a->book));
        smoke_snapshot("build/tests/results/workbook-identity/final.png"); identity_result=1;
        printf("PASS: native workbook identity workflow (9 stages, %u checks)\n",identity_checks); break;
    }
    identity_stage++;
}
int app_test_workbook_identity_result(void) { return identity_result; }
#undef ID_GUI_CHECK
