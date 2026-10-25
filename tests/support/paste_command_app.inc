/* Private clipboard/input/Undo fixture; the runner owns the desktop lease. */
static unsigned paste_stage,paste_wait,paste_checks;
static int paste_result;
static uint64_t paste_revision;
static char *paste_original_clipboard;
static const char *paste_path="build/tests/generated/paste-command.dxls";
static const char *paste_text="NULL\t12345678901234567890.123456789\n\tplain";
#define PASTE_CHECK(x) do { paste_checks++; if (!(x)) { fprintf(stderr,"FAIL paste GUI stage %u:%d: %s (%s)\n",paste_stage,__LINE__,#x,a->message ? a->message : ""); paste_result=-1; return; } } while (0)
static void paste_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (paste_result || !s || a->busy || a->io_busy || ++paste_wait<24) return;
    paste_wait=0; PASTE_CHECK(a->modal!=MODAL_ERROR);
    switch (paste_stage) {
    case 0: {
        remove(paste_path); paste_original_clipboard=dx_strdup(GetClipboardText());
        PASTE_CHECK(cells_set(&s->cells,1,1,"Before",&error)); worksheet_record(s,EDIT_CELLS);
        PASTE_CHECK(cells_set(&s->cells,2,1,"Clear",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat format=format_default(); format.flags=FORMAT_BOLD;
        PASTE_CHECK(format_apply(&s->formats,(FormatRange){1,2,1,2},FORMAT_BOLD,format,&error)); worksheet_record(s,EDIT_FORMAT);
        grid_select_cell(&a->grid,1,1,false); paste_revision=a->book.command_revision;
        SetClipboardText(paste_text); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    }
    case 1:
        PASTE_CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,1,2),"12345678901234567890.123456789") &&
            !*cells_get(&s->cells,2,1) && !strcmp(cells_get(&s->cells,2,2),"plain") && (format_at(&s->formats,2,1).flags&FORMAT_BOLD) &&
            a->grid.end_row==2 && a->grid.end_col==2 && a->book.command_revision==paste_revision+1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 2:
        PASTE_CHECK(!strcmp(cells_get(&s->cells,1,1),"Before") && !strcmp(cells_get(&s->cells,2,1),"Clear") && s->redo_actions_count==1);
        paste_revision=a->book.command_revision; SetClipboardText("\"unterminated"); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    case 3:
        PASTE_CHECK(a->book.command_revision==paste_revision && s->redo_actions_count==1 && !strcmp(cells_get(&s->cells,1,1),"Before"));
        SetClipboardText(paste_text); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    case 4:
        PASTE_CHECK(!s->redo_actions_count && !strcmp(cells_get(&s->cells,1,1),"NULL"));
        paste_revision=a->book.command_revision; a->book.dirty=false; grid_select_cell(&a->grid,1,1,false); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    case 5:
        PASTE_CHECK(a->book.command_revision==paste_revision && !a->book.dirty && a->grid.end_row==2 && a->grid.end_col==2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        PASTE_CHECK(!strcmp(cells_get(&s->cells,1,1),"Before") && (format_at(&s->formats,1,1).flags&FORMAT_BOLD));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        PASTE_CHECK(!strcmp(cells_get(&s->cells,1,1),"NULL") && !*cells_get(&s->cells,2,1)); workspace_job(a,JOB_SAVE,paste_path); break;
    case 8:
        PASTE_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,paste_path); break;
    case 9:
        PASTE_CHECK(!a->book.dirty && !s->undo_actions_count && !strcmp(cells_get(&s->cells,1,2),"12345678901234567890.123456789") && (format_at(&s->formats,1,1).flags&FORMAT_BOLD));
        paste_revision=a->book.command_revision; grid_select_cell(&a->grid,DX_SHEET_ROWS-1,16383,false);
        SetClipboardText("A\tB"); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    case 10:
        PASTE_CHECK(a->book.command_revision==paste_revision && !a->book.dirty && !s->undo_actions_count);
        grid_select_cell(&a->grid,3,3,false); SetClipboardText("\"İstanbul\tquoted\nline\"\t\"a\"\"b\""); ui_note_key_press(KEY_V,DX_MOD_SUPER); break;
    case 11:
        PASTE_CHECK(!strcmp(cells_get(&s->cells,3,3),"İstanbul\tquoted\nline") && !strcmp(cells_get(&s->cells,3,4),"a\"b") && a->book.command_revision==paste_revision+1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 12:
        PASTE_CHECK(!*cells_get(&s->cells,3,3) && !*cells_get(&s->cells,3,4));
        paste_result=1; printf("PASS: native manual paste workflow (13 stages, %u checks)\n",paste_checks); break;
    }
    free(error); paste_stage++;
}
int app_test_paste_command_result(void) {
    if (paste_result && paste_original_clipboard) { SetClipboardText(paste_original_clipboard); free(paste_original_clipboard); paste_original_clipboard=NULL; }
    return paste_result;
}
unsigned app_test_paste_command_stage(void) { return paste_stage; }
#undef PASTE_CHECK
