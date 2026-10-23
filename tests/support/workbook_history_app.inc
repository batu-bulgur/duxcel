/* Private fixture compiled only into the workbook-history feature App object. */
static unsigned history_stage,history_wait,history_checks;
static int history_result;
static bool history_style_branch;
static uint64_t history_a,history_b,history_saved_revision;
#define HIST_GUI_CHECK(x) do { history_checks++; if (!(x)) { fprintf(stderr,"FAIL: chronology GUI stage %u line %d: %s (%s)\n",history_stage,__LINE__,#x,a->message ? a->message : ""); history_result=-1; return; } } while (0)
static void history_gui_edit(App *a,const char *value) {
    grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,value); ui_note_key_press(KEY_ENTER,0);
}
static void workbook_history_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (history_result || a->busy || a->io_busy || !s || ++history_wait<12) return;
    history_wait=0;
    switch (history_stage) {
    case 0: history_a=s->id; history_gui_edit(a,"A1"); break;
    case 1:
        HIST_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"A1"));
        new_sheet(a); history_b=active_sheet(a)->id; history_gui_edit(a,"B1"); break;
    case 2:
        HIST_GUI_CHECK(s->id==history_b && !strcmp(cells_get(&s->cells,1,1),"B1"));
        activate_sheet(a,0,true); history_gui_edit(a,"A2"); break;
    case 3:
        HIST_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"A2"));
        a->sheet_menu_index=0; a->sheet_action=SHEET_ACTION_HIDE; process_sheet_action(a);
        HIST_GUI_CHECK(a->book.active==1 && a->book.sheets[0].hidden);
        HIST_GUI_CHECK(command_popup_enabled(a,RIBBON_MORE_UNDO)); history(a,false); /* Undo Hide before the cell. */ ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        HIST_GUI_CHECK(s->id==history_a && !s->hidden && !strcmp(cells_get(&s->cells,1,1),"A1"));
        HIST_GUI_CHECK(!strcmp(cells_get(&a->book.sheets[1].cells,1,1),"B1")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        HIST_GUI_CHECK(s->id==history_b && !*cells_get(&s->cells,1,1)); history(a,false); /* Undo sheet creation. */ ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        HIST_GUI_CHECK(s->id==history_a && !*cells_get(&s->cells,1,1));
        HIST_GUI_CHECK(!workbook_history_available(&a->book,false) && command_popup_enabled(a,RIBBON_MORE_REDO));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        HIST_GUI_CHECK(s->id==history_a && !strcmp(cells_get(&s->cells,1,1),"A1")); history(a,true); /* Redo sheet creation. */ ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        HIST_GUI_CHECK(s->id==history_b && !strcmp(cells_get(&s->cells,1,1),"B1")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9:
        HIST_GUI_CHECK(s->id==history_a && !strcmp(cells_get(&s->cells,1,1),"A2"));
        history(a,true); /* Redo Hide. */ HIST_GUI_CHECK(!workbook_history_available(&a->book,true)); activate_sheet(a,1,true); history(a,false); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        HIST_GUI_CHECK(s->id==history_a && !strcmp(cells_get(&s->cells,1,1),"A1")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 11:
        HIST_GUI_CHECK(s->id==history_b && !*cells_get(&s->cells,1,1)); activate_sheet(a,0,true); history_gui_edit(a,"A1"); break;
    case 12:
        if (!history_style_branch) {
            HIST_GUI_CHECK(a->book.sheets[0].redo_actions_count==1 && a->book.sheets[1].redo_actions_count==1);
            HIST_GUI_CHECK(workbook_history_available(&a->book,true));
            CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
            grid_select_cell(&a->grid,1,1,false); HIST_GUI_CHECK(apply_format(a,FORMAT_BOLD,bold));
            history_style_branch=true; return;
        }
        HIST_GUI_CHECK(!a->book.sheets[0].redo_actions_count && !a->book.sheets[1].redo_actions_count);
        HIST_GUI_CHECK(format_at(&s->formats,1,1).flags==FORMAT_BOLD);
        history(a,false);
        HIST_GUI_CHECK(!format_at(&s->formats,1,1).flags && s->formats.redo_count==1);
        activate_sheet(a,1,true); history_gui_edit(a,"Branch B"); break;
    case 13:
        HIST_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Branch B"));
        HIST_GUI_CHECK(!a->book.sheets[0].redo_actions_count && !a->book.sheets[1].redo_actions_count && !command_popup_enabled(a,RIBBON_MORE_REDO));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 14:
        HIST_GUI_CHECK(s->id==history_b && !*cells_get(&s->cells,1,1)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 15:
        HIST_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Branch B"));
        begin_sheet_rename(a,a->book.active); input_set(&a->input,"Renamed B"); ui_note_key_press(KEY_ENTER,0); break;
    case 16:
        HIST_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(s->name,"Renamed B"));
        a->sheet_menu_index=a->book.active; a->sheet_action=SHEET_ACTION_UP; process_sheet_action(a);
        HIST_GUI_CHECK(a->book.active==0 && active_sheet(a)->id==history_b); history(a,false); history(a,false); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 17:
        HIST_GUI_CHECK(s->id==history_b && !*cells_get(&s->cells,1,1));
        HIST_GUI_CHECK(!strcmp(cells_get(&a->book.sheets[0].cells,1,1),"A1")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 18:
        HIST_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Branch B")); history(a,true); history(a,true); history_saved_revision=a->book.command_revision;
        workspace_job(a,JOB_SAVE,"build/tests/generated/workbook-history-demo.dxls"); break;
    case 19:
        HIST_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,"build/tests/generated/workbook-history-demo.dxls"); break;
    case 20:
        HIST_GUI_CHECK(!a->book.dirty && a->book.command_revision==history_saved_revision);
        HIST_GUI_CHECK(s->id==history_b && a->book.sheets[1].id==history_a && !strcmp(cells_get(&s->cells,1,1),"Branch B"));
        HIST_GUI_CHECK(!strcmp(cells_get(&a->book.sheets[1].cells,1,1),"A1"));
        HIST_GUI_CHECK(!workbook_history_available(&a->book,false) && !workbook_history_available(&a->book,true));
        HIST_GUI_CHECK(!a->book.history_branch && !s->history_last && !s->history_floor);
        smoke_snapshot("build/tests/results/workbook-history/final.png"); history_result=1;
        printf("PASS: native workbook chronology workflow (21 stages, %u checks)\n",history_checks); break;
    }
    history_stage++;
}
int app_test_workbook_history_result(void) { return history_result; }
#undef HIST_GUI_CHECK
