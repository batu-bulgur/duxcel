/* Private native lifecycle fixture; production contains no hook or export. */
static unsigned lifecycle_stage,lifecycle_wait,lifecycle_checks;
static int lifecycle_result;
static uint64_t lifecycle_request,lifecycle_cell_revision,lifecycle_analysis_revision;
static uint64_t lifecycle_a,lifecycle_b,lifecycle_copy,lifecycle_object,lifecycle_high,lifecycle_revision;
#define LIFE_GUI_CHECK(x) do { lifecycle_checks++; if (!(x)) { fprintf(stderr,"FAIL: lifecycle GUI stage %u line %d: %s (%s)\n",lifecycle_stage,__LINE__,#x,a->message ? a->message : ""); lifecycle_result=-1; return; } } while (0)
static void lifecycle_edit(App *a,const char *value) {
    grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,value); ui_note_key_press(KEY_ENTER,0);
}
static void lifecycle_action(App *a,size_t index,SheetAction action) {
    a->sheet_menu_index=index; a->sheet_action=action; process_sheet_action(a);
}
static void workbook_lifecycle_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (lifecycle_result || a->busy || a->io_busy || !s || ++lifecycle_wait<12) return;
    lifecycle_wait=0;
    switch (lifecycle_stage) {
    case 0:
        LIFE_GUI_CHECK(!a->book.edit_session_id && !s->edit_session_id && !s->undo_actions_count && !a->book.lifecycle);
        lifecycle_request=a->request_id; lifecycle_cell_revision=s->cells.revision; lifecycle_analysis_revision=a->analysis_revision;
        lifecycle_revision=a->book.command_revision;
        begin_sheet_rename(a,a->book.active); input_set(&a->input,"First metadata"); ui_note_key_press(KEY_ENTER,0); break;
    case 1:
        LIFE_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(s->name,"First metadata") && a->book.edit_session_id && s->edit_session_id);
        LIFE_GUI_CHECK(a->book.command_revision==lifecycle_revision+1 && a->book.lifecycle && !s->undo_actions_count && !s->cells.undo_count);
        LIFE_GUI_CHECK(a->request_id==lifecycle_request && a->analysis_revision==lifecycle_analysis_revision && s->cells.revision==lifecycle_cell_revision);
        lifecycle_revision=a->book.command_revision;
        begin_sheet_rename(a,a->book.active); input_set(&a->input,"  First metadata  "); ui_note_key_press(KEY_ENTER,0); break;
    case 2:
        LIFE_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(s->name,"First metadata") && a->book.command_revision==lifecycle_revision);
        LIFE_GUI_CHECK(a->request_id==lifecycle_request && a->analysis_revision==lifecycle_analysis_revision && !s->undo_actions_count);
        break;
    case 3: lifecycle_a=s->id; lifecycle_edit(a,"Keep A"); break;
    case 4: {
        LIFE_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Keep A"));
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; grid_select_cell(&a->grid,1,1,false);
        LIFE_GUI_CHECK(apply_format(a,FORMAT_BOLD,bold));
        char *error=NULL; int object=objects_add(&s->objects,8,8,"Retained pivot",&error);
        LIFE_GUI_CHECK(object>=0 && !error); worksheet_record(s,EDIT_OBJECTS); lifecycle_object=s->objects.items[object].id; break;
    }
    case 5: new_sheet(a); lifecycle_b=active_sheet(a)->id; lifecycle_edit(a,"Keep B"); break;
    case 6:
        LIFE_GUI_CHECK(s->id==lifecycle_b && !strcmp(cells_get(&s->cells,1,1),"Keep B"));
        begin_sheet_rename(a,a->book.active); input_set(&a->input,"Renamed B"); ui_note_key_press(KEY_ENTER,0); break;
    case 7:
        LIFE_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(s->name,"Renamed B"));
        lifecycle_request=a->request_id; lifecycle_analysis_revision=a->analysis_revision;
        lifecycle_action(a,a->book.active,SHEET_ACTION_UP); break;
    case 8:
        LIFE_GUI_CHECK(a->book.active==0 && s->id==lifecycle_b); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        LIFE_GUI_CHECK(a->book.active==1 && s->id==lifecycle_b);
        LIFE_GUI_CHECK(a->request_id==lifecycle_request && a->analysis_revision==lifecycle_analysis_revision); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        LIFE_GUI_CHECK(!strcmp(s->name,"Sheet 2"));
        LIFE_GUI_CHECK(a->request_id==lifecycle_request && a->analysis_revision==lifecycle_analysis_revision); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 11:
        LIFE_GUI_CHECK(!strcmp(s->name,"Renamed B")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 12:
        if (a->modal!=MODAL_SHEET) {
            LIFE_GUI_CHECK(a->book.active==0 && s->id==lifecycle_b);
            activate_sheet(a,1,true); begin_sheet_menu(a,1,(Vector2){300,300}); a->sheet_menu_focus=6; return;
        }
        ui_note_key_press(KEY_ENTER,0); break;
    case 13:
        LIFE_GUI_CHECK(a->book.count==1 && s->id==lifecycle_b && a->modal==MODAL_NONE);
        LIFE_GUI_CHECK(command_popup_enabled(a,RIBBON_MORE_UNDO)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 14:
        LIFE_GUI_CHECK(a->book.count==2 && s->id==lifecycle_a && a->book.active==1);
        LIFE_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Keep A") && format_at(&s->formats,1,1).flags==FORMAT_BOLD);
        LIFE_GUI_CHECK(s->objects.count==1 && s->objects.items[0].id==lifecycle_object);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 15:
        LIFE_GUI_CHECK(a->book.count==1 && s->id==lifecycle_b); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 16:
        LIFE_GUI_CHECK(s->id==lifecycle_a && s->objects.count==1);
        lifecycle_action(a,a->book.active,SHEET_ACTION_DUPLICATE); lifecycle_copy=active_sheet(a)->id; break;
    case 17:
        LIFE_GUI_CHECK(a->book.count==3 && s->id==lifecycle_copy && s->id!=lifecycle_a && s->objects.items[0].id==lifecycle_object);
        LIFE_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Keep A") && format_at(&s->formats,1,1).flags==FORMAT_BOLD);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 18:
        LIFE_GUI_CHECK(a->book.count==2 && s->id==lifecycle_a); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 19:
        LIFE_GUI_CHECK(a->book.count==3 && s->id==lifecycle_copy);
        lifecycle_action(a,a->book.active,SHEET_ACTION_HIDE); break;
    case 20:
        LIFE_GUI_CHECK(a->book.sheets[2].hidden && s->id==lifecycle_b); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 21:
        LIFE_GUI_CHECK(!a->book.sheets[2].hidden && s->id==lifecycle_b); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 22:
        LIFE_GUI_CHECK(a->book.count==2 && s->id==lifecycle_a);
        lifecycle_action(a,a->book.active,SHEET_ACTION_DELETE); lifecycle_high=a->book.sheet_high_water; lifecycle_revision=a->book.command_revision;
        workspace_job(a,JOB_SAVE,"build/tests/generated/workbook-lifecycle-demo.dxls"); break;
    case 23:
        LIFE_GUI_CHECK(!a->book.dirty && a->book.count==1); workspace_job(a,JOB_LOAD,"build/tests/generated/workbook-lifecycle-demo.dxls"); break;
    case 24:
        LIFE_GUI_CHECK(!a->book.dirty && a->book.count==1 && s->id==lifecycle_b && !strcmp(cells_get(&s->cells,1,1),"Keep B"));
        LIFE_GUI_CHECK(!a->book.lifecycle && !workbook_history_available(&a->book,false) && a->book.command_revision==lifecycle_revision);
        LIFE_GUI_CHECK(!a->book.edit_session_id && !s->edit_session_id);
        lifecycle_revision=a->book.command_revision; lifecycle_request=a->request_id;
        begin_sheet_rename(a,a->book.active); input_set(&a->input,"Reopened B"); ui_note_key_press(KEY_ENTER,0); break;
    case 25:
        LIFE_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(s->name,"Reopened B") && a->book.command_revision==lifecycle_revision+1);
        LIFE_GUI_CHECK(a->book.edit_session_id && s->edit_session_id && !s->undo_actions_count && a->request_id==lifecycle_request);
        new_sheet(a); break;
    case 26:
        LIFE_GUI_CHECK(a->book.count==2 && s->id>lifecycle_high); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 27:
        LIFE_GUI_CHECK(a->book.count==1 && s->id==lifecycle_b && a->book.sheet_high_water>lifecycle_high);
        smoke_snapshot("build/tests/results/workbook-lifecycle/final.png"); lifecycle_result=1;
        printf("PASS: native workbook lifecycle workflow (28 stages, %u checks)\n",lifecycle_checks); break;
    }
    lifecycle_stage++;
}
int app_test_workbook_lifecycle_result(void) { return lifecycle_result; }
#undef LIFE_GUI_CHECK
