/* Private actual dialog/confirmation/Undo fixture; excluded from production. */
static unsigned range_stage,range_wait,range_checks;
static int range_result;
static uint64_t range_request;
static const char *range_path="build/tests/generated/range-command.dxls";
#define RANGE_GUI_CHECK(x) do { range_checks++; if (!(x)) { fprintf(stderr,"FAIL range GUI stage %u:%d: %s (%s)\n",range_stage,__LINE__,#x,a->message ? a->message : ""); range_result=-1; return; } } while (0)
static void range_gui_select(App *a) { grid_select_cell(&a->grid,1,1,false); a->grid.end_row=a->grid.end_col=3; }
static void range_gui_move(App *a,const char *address) { begin_move_range(a,false); input_set(&a->input,address); a->range_move_focus=1; ui_note_key_press(KEY_ENTER,0); }
static void range_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (range_result || !s || a->busy || a->io_busy || ++range_wait<24) return;
    range_wait=0; RANGE_GUI_CHECK(a->modal!=MODAL_ERROR);
    switch (range_stage) {
    case 0: {
        remove(range_path);
        const struct { uint64_t row; size_t column; const char *text; } cells[]={{1,1,"NULL"},{3,3,"12345678901234567890.123456789"},{10,10,"Occupied"},{11,11,"Destination hole"}};
        for (size_t i=0;i<4;i++) { RANGE_GUI_CHECK(cells_set(&s->cells,cells[i].row,cells[i].column,cells[i].text,&error)); worksheet_record(s,EDIT_CELLS); }
        CellFormat style=format_default(); style.flags=1; RANGE_GUI_CHECK(format_apply(&s->formats,(FormatRange){1,3,1,3},FORMAT_BOLD,style,&error)); worksheet_record(s,EDIT_FORMAT);
        style.flags=0; style.color=2; RANGE_GUI_CHECK(format_apply(&s->formats,(FormatRange){10,12,10,12},FORMAT_COLOR,style,&error)); worksheet_record(s,EDIT_FORMAT);
        a->book.dirty=false; range_request=a->request_id; range_gui_select(a); break;
    }
    case 1: range_gui_move(a,"K11"); break;
    case 2:
        RANGE_GUI_CHECK(a->modal==MODAL_REPLACE_RANGE && a->replace_manual_target && !a->book.command_revision && !a->book.dirty);
        ui_note_key_press(KEY_ENTER,0); break;
    case 3:
        RANGE_GUI_CHECK(a->modal==MODAL_NONE && !a->replace_manual_target && !strcmp(cells_get(&s->cells,10,10),"Occupied") && !a->book.dirty);
        range_gui_move(a,"K11"); break;
    case 4:
        RANGE_GUI_CHECK(a->modal==MODAL_REPLACE_RANGE); a->replace_focus=1; ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        RANGE_GUI_CHECK(a->modal==MODAL_NONE && !a->replace_manual_target && a->book.command_revision==1 && !strcmp(cells_get(&s->cells,10,10),"NULL") && !*cells_get(&s->cells,11,11) && !*cells_get(&s->cells,1,1) &&
            (format_at(&s->formats,10,10).flags&1) && !format_at(&s->formats,10,10).color && s->undo_actions[s->undo_actions_count-1]==EDIT_CELLS_FORMAT && a->request_id==range_request);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        RANGE_GUI_CHECK(a->book.command_revision==2 && !strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,10,10),"Occupied") && format_at(&s->formats,10,10).color==2); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        RANGE_GUI_CHECK(a->book.command_revision==3 && !strcmp(cells_get(&s->cells,12,12),"12345678901234567890.123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        RANGE_GUI_CHECK(a->book.command_revision==4 && s->redo_actions_count==1); range_gui_select(a); range_gui_move(a,"K11"); break;
    case 9:
        RANGE_GUI_CHECK(a->modal==MODAL_REPLACE_RANGE && a->replace_manual_target); a->request_id++;
        RANGE_GUI_CHECK(!move_manual_range(a,(RangeCommand){.range={50,52,50,52},.dr=10,.dc=10},a->replace_manual_target,&error) && a->book.command_revision==4 && s->redo_actions_count==1 && a->grid.anchor_row==1 && a->grid.end_row==3);
        free(error); error=NULL; a->replace_focus=1; ui_note_key_press(KEY_ENTER,0); break;
    case 10:
        RANGE_GUI_CHECK(a->modal==MODAL_NONE && !a->replace_manual_target && a->book.command_revision==4 && s->redo_actions_count==1 && !strcmp(cells_get(&s->cells,10,10),"Occupied"));
        range_gui_select(a); range_gui_move(a,"E5"); break;
    case 11:
        RANGE_GUI_CHECK(a->book.command_revision==5 && !s->redo_actions_count && !strcmp(cells_get(&s->cells,4,4),"NULL") && !strcmp(cells_get(&s->cells,6,6),"12345678901234567890.123456789") && !*cells_get(&s->cells,1,1));
        range_gui_move(a,"E5"); break;
    case 12:
        RANGE_GUI_CHECK(a->book.command_revision==5 && !strcmp(cells_get(&s->cells,4,4),"NULL")); ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 13:
        grid_select_cell(&a->grid,4,4,false); a->grid.selection=SELECT_ROWS; a->grid.end_row=5;
        a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0); break;
    case 14:
        RANGE_GUI_CHECK(a->book.command_revision==6 && !strcmp(cells_get(&s->cells,6,4),"NULL") && !strcmp(cells_get(&s->cells,8,6),"12345678901234567890.123456789") && (format_at(&s->formats,6,4).flags&1));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 15:
        RANGE_GUI_CHECK(a->book.command_revision==7 && !strcmp(cells_get(&s->cells,4,4),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 16:
        RANGE_GUI_CHECK(a->book.command_revision==8 && !strcmp(cells_get(&s->cells,6,4),"NULL"));
        grid_select_cell(&a->grid,6,4,false); a->grid.selection=SELECT_ROWS; a->grid.end_row=7;
        a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=2; a->context_scroll=0; ui_note_key_press(KEY_ENTER,0); break;
    case 17:
        RANGE_GUI_CHECK(a->book.command_revision==9 && !*cells_get(&s->cells,6,4) && !strcmp(cells_get(&s->cells,6,6),"12345678901234567890.123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 18:
        RANGE_GUI_CHECK(a->book.command_revision==10 && !strcmp(cells_get(&s->cells,6,4),"NULL") && !strcmp(cells_get(&s->cells,8,6),"12345678901234567890.123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 19:
        RANGE_GUI_CHECK(a->book.command_revision==11 && !*cells_get(&s->cells,6,4)); grid_select_cell(&a->grid,50000,4,false); column_action(a,ROW_ABOVE);
        RANGE_GUI_CHECK(a->book.command_revision==11); break;
    case 20: workspace_job(a,JOB_SAVE,range_path); break;
    case 21:
        RANGE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,range_path); range_stage++; return;
    case 22:
        RANGE_GUI_CHECK(!a->book.dirty && !s->undo_actions_count && !*cells_get(&s->cells,6,4) && !strcmp(cells_get(&s->cells,6,6),"12345678901234567890.123456789") &&
            !strcmp(cells_get(&s->cells,10,10),"Occupied") && (format_at(&s->formats,6,6).flags&1) && format_at(&s->formats,10,10).color==2 && a->book.command_revision==11);
        range_result=1; ui_set_scale(&a->ui,1); SetWindowSize(1280,820); printf("PASS: native manual range workflow (23 stages, %u checks)\n",range_checks); break;
    }
    free(error); range_stage++;
}
int app_test_range_command_result(void) { return range_result; }
unsigned app_test_range_command_stage(void) { return range_stage; }
#undef RANGE_GUI_CHECK
