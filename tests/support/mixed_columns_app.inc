/* Private actual column context actions and complete delegated replay. */
static unsigned mixed_columns_stage,mixed_columns_wait,mixed_columns_checks;
static int mixed_columns_result;
static uint64_t mixed_columns_high,mixed_columns_base;
#define COLS_CHECK(x) do { mixed_columns_checks++; if (!(x)) { fprintf(stderr,"FAIL mixed columns GUI stage %u:%d: %s (%s)\n",mixed_columns_stage,__LINE__,#x,a->message ? a->message : ""); mixed_columns_result=-1; return; } } while (0)
static void mixed_columns_menu(App *a,size_t column,int action) {
    grid_select_cell(&a->grid,0,column,false); a->grid.selection=SELECT_COLUMNS;
    a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=action; a->context_scroll=0;
    a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0);
}
static void mixed_columns_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); char *error=NULL;
    if (mixed_columns_result || !s || a->busy || a->io_busy || ++mixed_columns_wait<24) return;
    mixed_columns_wait=0; COLS_CHECK(a->modal!=MODAL_ERROR);
    switch (mixed_columns_stage) {
    case 0: {
        COLS_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        COLS_CHECK(objects_add(&s->objects,5,2,"Columns Pivot",&error)==0); s->objects.items[0].rows=3; s->objects.items[0].columns=2; worksheet_record(s,EDIT_OBJECTS);
        COLS_CHECK(objects_add_chart(&s->objects,20,8,"Columns Chart",&(PivotDefinition){0},&error)==1); worksheet_record(s,EDIT_OBJECTS);
        COLS_CHECK(cells_set(&s->cells,1,1,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
        COLS_CHECK(cells_set(&s->cells,10,100,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; COLS_CHECK(format_apply(&s->formats,(FormatRange){1,10,1,3},FORMAT_BOLD,bold,&error)); worksheet_record(s,EDIT_FORMAT);
        grid_ensure_columns(&a->grid,16384); grid_set_width(&a->grid,1,123); grid_set_width(&a->grid,16383,333);
        COLS_CHECK(commit_grid_widths(a)); mixed_columns_base=a->book.command_revision;
        mixed_columns_high=s->objects.next_id; mixed_columns_menu(a,0,2); break;
    }
    case 1:
        COLS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==mixed_columns_base+1 && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==30 && s->objects.items[0].col==3 && s->objects.items[1].col==9);
        COLS_CHECK(!strcmp(cells_get(&s->cells,1,2),"NULL") && !strcmp(cells_get(&s->cells,10,101),"9007199254740993.0123456789") && (format_at(&s->formats,1,2).flags&FORMAT_BOLD));
        COLS_CHECK(s->view.widths[2]==123 && s->view.widths[16383]==96 && a->grid.widths[2]==123 && a->grid.widths[16383]==96);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 2:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+2 && s->objects.items[0].col==2 && !strcmp(cells_get(&s->cells,1,1),"NULL") && s->view.widths[16383]==333 && a->grid.widths[16383]==333);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 3:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+3 && s->objects.items[0].col==3 && s->view.widths[16383]==96); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+4 && s->view.widths[16383]==333); mixed_columns_menu(a,0,4); break;
    case 5:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+5 && s->objects.items[0].col==1 && s->objects.items[1].col==7 && !strcmp(cells_get(&s->cells,1,0),"NULL") && s->view.widths[0]==123 && s->view.widths[16382]==333 && s->view.widths[16383]==96);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+6 && s->objects.items[0].col==2 && s->view.widths[16383]==333 && a->grid.widths[16383]==333); mixed_columns_menu(a,3,2); break;
    case 7:
        COLS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==mixed_columns_base+6 && s->objects.items[0].col==2 && s->redo_actions_count==1); mixed_columns_menu(a,2,4); break;
    case 8:
        COLS_CHECK(a->modal==MODAL_NONE && a->book.command_revision==mixed_columns_base+6 && s->objects.items[0].col==2 && s->redo_actions_count==1); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9:
        COLS_CHECK(a->book.command_revision==mixed_columns_base+7 && s->objects.items[0].col==1 && s->objects.items[1].col==7 && s->objects.next_id==mixed_columns_high && s->view.widths[16382]==333 && a->grid.widths[16382]==333 && (format_at(&s->formats,1,0).flags&FORMAT_BOLD) && !a->busy && !a->io_busy);
        mixed_columns_result=1; printf("PASS: native source-free mixed column workflow (10 stages, %u checks)\n",mixed_columns_checks); break;
    }
    free(error); mixed_columns_stage++;
}
int app_test_mixed_columns_result(void) { return mixed_columns_result; }
unsigned app_test_mixed_columns_stage(void) { return mixed_columns_stage; }
#undef COLS_CHECK
