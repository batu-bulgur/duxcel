/* Private actual CSV-primary/Table clipboard and sparse Clear workflow. */
static unsigned source_cells_stage,source_cells_wait,source_cells_checks;
static int source_cells_result;
static uint64_t source_cells_base,source_cells_request,source_cells_high;
static Cell *source_cells_primary_page,*source_cells_table_page;
static char *source_cells_clipboard;
#define SC_CHECK(x) do { source_cells_checks++; if (!(x)) { fprintf(stderr,"FAIL source cells GUI stage %u:%d: %s (%s)\n",source_cells_stage,__LINE__,#x,a->message ? a->message : ""); source_cells_result=-1; return; } } while (0)
static void source_cells_paste(App *a,uint64_t row,size_t column,const char *text) {
    grid_select_cell(&a->grid,row,column,false); a->chrome_focus=a->pivot_focus=0; SetClipboardText(text); ui_note_key_press(KEY_V,DX_MOD_SUPER);
}
static void source_cells_clear(App *a) {
    grid_select_cell(&a->grid,0,20,false); a->grid.end_row=40; a->grid.end_col=0;
    a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=6; a->context_scroll=0;
    a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0);
}
static void source_cells_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (source_cells_result || !s || a->busy || a->io_busy || ++source_cells_wait<24) return;
    source_cells_wait=0; SC_CHECK(a->modal!=MODAL_ERROR);
    switch (source_cells_stage) {
    case 0:
        source_cells_clipboard=dx_strdup(GetClipboardText()); open_source(a,"tests/fixtures/null-text.csv",false); break;
    case 1: {
        if (a->page.count_pending) return;
        SC_CHECK(a->analysis.kind==SOURCE_CSV && !a->page_stale && !a->fetch_failed && a->page.total==6 && a->page.column_count==3);
        grid_select_cell(&a->grid,0,4,false); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; SC_CHECK(apply_format(a,FORMAT_BOLD,bold));
        grid_select_cell(&a->grid,20,8,false); SC_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    }
    case 2:
        if (!object_ready(a,0) || a->page.count_pending) return;
        SC_CHECK(s->objects.count==1 && s->objects.items[0].kind==OBJECT_TABLE && s->formats.source_aware);
        source_cells_base=a->book.command_revision; source_cells_request=a->request_id; source_cells_high=s->objects.next_id;
        source_cells_primary_page=a->page.cells; source_cells_table_page=a->objects[0].page.cells;
        source_cells_paste(a,0,0,"Blocked primary"); break;
    case 3:
        SC_CHECK(a->book.command_revision==source_cells_base && !s->cells.count && a->page.cells==source_cells_primary_page);
        source_cells_paste(a,20,8,""); break;
    case 4:
        SC_CHECK(a->book.command_revision==source_cells_base && !s->cells.count && a->objects[0].page.cells==source_cells_table_page);
        source_cells_paste(a,18,7,"A\tB\tC\nD\nE"); break;
    case 5:
        SC_CHECK(a->book.command_revision==source_cells_base && !s->cells.count);
        source_cells_paste(a,0,4,"NULL\t\"\"\n\"two\tparts\"\t9007199254740993.0123456789"); break;
    case 6:
        SC_CHECK(a->book.command_revision==source_cells_base+1 && s->undo_actions[s->undo_actions_count-1]==EDIT_CELLS && !strcmp(cells_get(&s->cells,0,4),"NULL") && !*cells_get(&s->cells,0,5) && !strcmp(cells_get(&s->cells,1,4),"two\tparts") && !strcmp(cells_get(&s->cells,1,5),"9007199254740993.0123456789"));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7:
        SC_CHECK(a->book.command_revision==source_cells_base+2 && !s->cells.count && (format_at(&s->formats,0,4).flags&FORMAT_BOLD)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        SC_CHECK(a->book.command_revision==source_cells_base+3 && !strcmp(cells_get(&s->cells,1,5),"9007199254740993.0123456789")); source_cells_clear(a); break;
    case 9:
        SC_CHECK(a->book.command_revision==source_cells_base+4 && !s->cells.count && s->objects.items[0].row==20 && s->objects.items[0].col==8 && (format_at(&s->formats,0,4).flags&FORMAT_BOLD)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        SC_CHECK(a->book.command_revision==source_cells_base+5 && !strcmp(cells_get(&s->cells,0,4),"NULL")); source_cells_clear(a); break;
    case 11:
        SC_CHECK(a->book.command_revision==source_cells_base+6 && !s->cells.count);
        column_action(a,COLUMN_CLEAR);
        SC_CHECK(a->book.command_revision==source_cells_base+6 && !s->cells.count && s->objects.next_id==source_cells_high && a->page.cells==source_cells_primary_page && a->objects[0].page.cells==source_cells_table_page && a->request_id==source_cells_request && (format_at(&s->formats,0,4).flags&FORMAT_BOLD) && !a->busy && !a->io_busy);
        source_cells_result=1; printf("PASS: native source manual cell workflow (12 stages, %u checks)\n",source_cells_checks); break;
    }
    source_cells_stage++;
}
int app_test_source_cells_result(void) {
    if (source_cells_result && source_cells_clipboard) { SetClipboardText(source_cells_clipboard); free(source_cells_clipboard); source_cells_clipboard=NULL; }
    return source_cells_result;
}
unsigned app_test_source_cells_stage(void) { return source_cells_stage; }
#undef SC_CHECK
