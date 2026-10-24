/* Private exact-object move/format/cache/input fixture; not in production. */
static unsigned moving_stage,moving_wait,moving_checks;
static int moving_result;
static uint64_t moving_revision,moving_id,moving_row,moving_high,moving_queries[3];
static size_t moving_col;
static Cell *moving_pages[3];
static const char *moving_path="build/tests/generated/object-move-command.dxls";
#define MOVE_GUI_CHECK(x) do { moving_checks++; if (!(x)) { fprintf(stderr,"FAIL object move GUI stage %u:%d: %s (%s)\n",moving_stage,__LINE__,#x,a->message ? a->message : ""); moving_result=-1; return; } } while (0)
static bool moving_caches(App *a) {
    ObjectStore *s=&active_sheet(a)->objects;
    for (unsigned i=0;i<3;i++) if (a->objects[i].page.cells!=moving_pages[i] || s->items[i].revision!=moving_queries[i]) return false;
    return true;
}
static void moving_destination(App *a,int index,uint64_t row,size_t col) {
    char address[48]; a->selected_object=index; a->chrome_focus=a->pivot_focus=0; a->ribbon_focus=false;
    begin_move_range(a,true); grid_format_address(row,col,a->analysis.kind!=SOURCE_NONE,address); input_set(&a->input,address); ui_note_key_press(KEY_ENTER,0);
}
static void object_move_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (moving_result || !s || a->busy || a->io_busy || ++moving_wait<24) return;
    moving_wait=0; MOVE_GUI_CHECK(a->modal!=MODAL_ERROR);
    switch (moving_stage) {
    case 0:
        remove(moving_path); grid_select_cell(&a->grid,2,1,false); MOVE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition p={0};
        MOVE_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3: {
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        for (unsigned i=0;i<3;i++) { moving_pages[i]=a->objects[i].page.cells; moving_queries[i]=store->items[i].revision; }
        SheetObject *o=store->items+2; moving_id=o->id; moving_row=o->row; moving_col=o->col; moving_high=store->next_id;
        CellFormat f=format_default(); f.flags=FORMAT_BOLD;
        MOVE_GUI_CHECK(format_apply(&s->formats,(FormatRange){o->row,o->row,o->col,o->col},FORMAT_BOLD,f,NULL)); worksheet_record(s,EDIT_FORMAT); workbook_history_note_edit(&a->book);
        moving_revision=a->book.command_revision; moving_destination(a,2,40,20); break;
    }
    case 4:
        MOVE_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].row==40 && store->items[2].col==20 && store->items[2].id==moving_id && store->next_id==moving_high && a->book.command_revision==moving_revision+1 && moving_caches(a));
        MOVE_GUI_CHECK(s->undo_actions[s->undo_actions_count-1]==EDIT_OBJECTS_FORMAT && (format_at(&s->formats,40,20).flags&FORMAT_BOLD) && !(format_at(&s->formats,moving_row,moving_col).flags&FORMAT_BOLD));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        MOVE_GUI_CHECK(store->items[2].row==moving_row && store->items[2].col==moving_col && s->redo_actions_count==1 && moving_caches(a) && (format_at(&s->formats,moving_row,moving_col).flags&FORMAT_BOLD));
        moving_destination(a,2,moving_row,moving_col); break;
    case 6:
        MOVE_GUI_CHECK(a->modal==MODAL_NONE && s->redo_actions_count==1 && a->book.command_revision==moving_revision+2 && moving_caches(a));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        MOVE_GUI_CHECK(store->items[2].row==40 && store->items[2].col==20 && a->book.command_revision==moving_revision+3 && moving_caches(a));
        moving_destination(a,2,store->items[0].row,store->items[0].col); break;
    case 8:
        MOVE_GUI_CHECK(a->modal==MODAL_MOVE_RANGE && a->range_move_error && store->items[2].row==40 && a->book.command_revision==moving_revision+3 && moving_caches(a));
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 9: {
        MOVE_GUI_CHECK(a->modal==MODAL_NONE); CellEditTarget target;
        MOVE_GUI_CHECK(workbook_prepare_cell_edit(&a->book,a->book.active,60,20,&target,NULL)); CellEditCommand edit={CELL_EDIT_COMMAND_VERSION,target,"Keep note",9};
        MOVE_GUI_CHECK(workbook_apply_cell_edit(&a->book,&edit,(CellEditPolicy){0,0,true},NULL).status==CELL_EDIT_APPLIED);
        moving_destination(a,2,60,20); break;
    }
    case 10:
        MOVE_GUI_CHECK(a->modal==MODAL_MOVE_RANGE && a->range_move_error && a->book.command_revision==moving_revision+4 && store->items[2].row==40 && moving_caches(a) && !strcmp(cells_get(&s->cells,60,20),"Keep note"));
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 11:
        MOVE_GUI_CHECK(a->modal==MODAL_NONE); moving_destination(a,0,2,36); break;
    case 12:
        MOVE_GUI_CHECK(a->modal==MODAL_NONE && store->items[0].row==2 && store->items[0].col==36 && store->items[2].source_id==store->items[0].id && a->book.command_revision==moving_revision+5 && moving_caches(a));
        MOVE_GUI_CHECK(format_at(&s->formats,40,20).flags&FORMAT_BOLD); ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 13:
        workspace_job(a,JOB_SAVE,moving_path); break;
    case 14:
        MOVE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,moving_path); break;
    case 15:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        MOVE_GUI_CHECK(store->count==3 && store->items[2].id==moving_id && store->items[2].row==40 && store->items[2].col==20 && store->next_id==moving_high && store->items[0].col==36 && store->items[2].source_id==store->items[0].id && a->book.command_revision==moving_revision+5 && !s->undo_actions_count && !a->book.dirty);
        MOVE_GUI_CHECK((format_at(&s->formats,40,20).flags&FORMAT_BOLD) && !strcmp(cells_get(&s->cells,60,20),"Keep note") && a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==3);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); moving_result=1;
        printf("PASS: native object move workflow (16 stages, %u checks)\n",moving_checks); break;
    }
    moving_stage++;
}
int app_test_object_move_command_result(void) { return moving_result; }
unsigned app_test_object_move_command_stage(void) { return moving_stage; }
#undef MOVE_GUI_CHECK
