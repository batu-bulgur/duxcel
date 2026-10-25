/* Private actual context-menu clear/Undo fixture; excluded from production. */
static unsigned clear_stage,clear_wait,clear_checks;
static int clear_result;
#define CLEAR_CHECK(x) do { clear_checks++; if (!(x)) { fprintf(stderr,"FAIL clear GUI stage %u:%d: %s (%s)\n",clear_stage,__LINE__,#x,a->message ? a->message : ""); clear_result=-1; return; } } while (0)
static void clear_menu(App *a) { a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=6; a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0); }
static void clear_range_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (clear_result || !s || a->busy || a->io_busy || ++clear_wait<24) return;
    clear_wait=0; CLEAR_CHECK(a->modal!=MODAL_ERROR);
    switch (clear_stage) {
    case 0: {
        CLEAR_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        CLEAR_CHECK(cells_set(&s->cells,1,1,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
        CLEAR_CHECK(cells_set(&s->cells,3,3,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        CLEAR_CHECK(cells_set(&s->cells,10,10,"Outside",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CLEAR_CHECK(format_apply(&s->formats,(FormatRange){1,3,1,3},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
        a->book.dirty=false; grid_select_cell(&a->grid,1,1,false); a->grid.end_row=a->grid.end_col=3; break;
    }
    case 1: clear_menu(a); break;
    case 2:
        CLEAR_CHECK(a->modal==MODAL_NONE && a->book.command_revision==1 && s->cells.count==1 && !*cells_get(&s->cells,1,1) && !*cells_get(&s->cells,3,3) && !strcmp(cells_get(&s->cells,10,10),"Outside"));
        CLEAR_CHECK((format_at(&s->formats,1,1).flags&FORMAT_BOLD) && a->grid.anchor_row==1 && a->grid.anchor_col==1 && a->grid.end_row==3 && a->grid.end_col==3 && !s->width_revision && s->undo_actions[s->undo_actions_count-1]==EDIT_CELLS);
        clear_menu(a); break;
    case 3:
        CLEAR_CHECK(a->book.command_revision==1); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        CLEAR_CHECK(a->book.command_revision==2 && s->redo_actions_count==1 && !strcmp(cells_get(&s->cells,1,1),"NULL") && !strcmp(cells_get(&s->cells,3,3),"9007199254740993.0123456789"));
        grid_select_cell(&a->grid,100,100,false); a->grid.end_row=a->grid.end_col=200; clear_menu(a); break;
    case 5:
        CLEAR_CHECK(a->book.command_revision==2 && s->redo_actions_count==1 && !strcmp(cells_get(&s->cells,1,1),"NULL")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 6:
        CLEAR_CHECK(a->book.command_revision==3 && !*cells_get(&s->cells,1,1) && !strcmp(cells_get(&s->cells,10,10),"Outside"));
        grid_select_cell(&a->grid,0,1,false); a->grid.selection=SELECT_COLUMNS; a->grid.end_col=3; clear_menu(a); break;
    case 7:
        CLEAR_CHECK(a->modal==MODAL_NONE && a->book.command_revision==3 && s->cells.count==1 && (format_at(&s->formats,3,3).flags&FORMAT_BOLD) && !s->width_revision && !s->objects.count);
        clear_result=1; printf("PASS: native manual clear workflow (8 stages, %u checks)\n",clear_checks); break;
    }
    free(error); clear_stage++;
}
int app_test_clear_range_result(void) { return clear_result; }
unsigned app_test_clear_range_stage(void) { return clear_stage; }
#undef CLEAR_CHECK
