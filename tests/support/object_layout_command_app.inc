/* Private layout/cache/input fixture; excluded from production App. */
static unsigned layout_stage,layout_wait,layout_checks;
static int layout_result;
static uint64_t layout_revision,layout_request,layout_jobs,layout_titles;
static Cell *layout_pages[3];
static const char *layout_path="build/tests/generated/object-layout-command.dxls";
#define PRESENT_GUI_CHECK(x) do { layout_checks++; if (!(x)) { fprintf(stderr,"FAIL presentation GUI stage %u:%d: %s (%s)\n",layout_stage,__LINE__,#x,a->message ? a->message : ""); layout_result=-1; return; } } while (0)
static bool layout_caches(const App *a) {
    return a->objects[0].page.cells==layout_pages[0] && a->objects[1].page.cells==layout_pages[1] && a->objects[2].page.cells==layout_pages[2] &&
        a->request_id==layout_request && a->worker.jobs_started==layout_jobs;
}
static void object_layout_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (layout_result || !s || a->busy || a->io_busy || ++layout_wait<24) return;
    layout_wait=0; PRESENT_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (layout_stage) {
    case 0:
        remove(layout_path); grid_select_cell(&a->grid,2,1,false); PRESENT_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition pivot={0}; char *error=NULL;
        PRESENT_GUI_CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
        set_pivot_definition(a,&pivot); free(error); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        for (unsigned i=0;i<3;i++) layout_pages[i]=a->objects[i].page.cells;
        layout_revision=a->book.command_revision; layout_request=a->request_id; layout_jobs=a->worker.jobs_started; layout_titles=store->title_revision;
        a->selected_object=2;
        { uint64_t id=a->objects[2].id; a->objects[2].id=UINT64_MAX;
          PRESENT_GUI_CHECK(!set_representation(a,0) && store->items[2].kind==OBJECT_CHART && a->book.command_revision==layout_revision && layout_caches(a));
          a->objects[2].id=id; }
        a->pivot_focus=6; a->chrome_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        PRESENT_GUI_CHECK(a->modal==MODAL_REPRESENTATION); ui_note_key_press(KEY_HOME,0); break;
    case 5:
        PRESENT_GUI_CHECK(a->context_focus==0); ui_note_key_press(KEY_ENTER,0); break;
    case 6:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].kind==OBJECT_PIVOT && store->items[2].rows==5 && a->book.command_revision==layout_revision+1 && layout_caches(a));
        history(a,false);
        PRESENT_GUI_CHECK(store->items[2].kind==OBJECT_CHART && store->items[2].rows==12 && layout_caches(a));
        history(a,true);
        PRESENT_GUI_CHECK(store->items[2].kind==OBJECT_PIVOT && store->items[2].rows==5 && a->book.command_revision==layout_revision+3 && layout_caches(a));
        ui_note_key_press(KEY_ENTER,0); break;
    case 7:
        PRESENT_GUI_CHECK(a->modal==MODAL_REPRESENTATION); ui_note_key_press(KEY_DOWN,0); break;
    case 8:
        PRESENT_GUI_CHECK(a->context_focus==1); ui_note_key_press(KEY_ENTER,0); break;
    case 9:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].kind==OBJECT_CHART && store->items[2].rows==12 && store->items[2].columns==6 && a->book.command_revision==layout_revision+4 && layout_caches(a));
        matrix_select_frame(a,store->items+2); begin_chart_size(a); input_set(&a->chart_columns,"8"); input_set(&a->input,"14"); ui_note_key_press(KEY_ENTER,0); break;
    case 10:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].rows==14 && store->items[2].columns==8 && a->book.command_revision==layout_revision+5 && layout_caches(a));
        PRESENT_GUI_CHECK(a->grid.end_row==store->items[2].row+13 && a->grid.end_col==store->items[2].col+7);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 11:
        PRESENT_GUI_CHECK(store->items[2].rows==12 && store->items[2].columns==6 && s->redo_actions_count==1 && layout_caches(a));
        begin_chart_size(a); ui_note_key_press(KEY_ENTER,0); break;
    case 12:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && s->redo_actions_count==1 && a->book.command_revision==layout_revision+6 && layout_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 13:
        PRESENT_GUI_CHECK(store->items[2].rows==14 && store->items[2].columns==8 && a->book.command_revision==layout_revision+7 && layout_caches(a));
        close_workspace(a); set_ribbon_mode(a,3); a->selected_object=2; a->pivot_reveal_object=true;
        grid_zoom_at(&a->grid,&a->page,a->ui.scale,.5f,(Vector2){a->grid.body.x,a->grid.body.y}); break;
    case 14: {
        SheetObject *o=store->items+2; Rectangle old=object_rect(a,o),next=grid_range_rect(&a->grid,a->ui.scale*a->grid.zoom,o->row,o->col,16,9);
        a->consume_grid_input=false; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        ui_note_pointer_gesture((DxPointerGesture){old.x+old.width-1,old.y+old.height-1,next.x+next.width-1,next.y+next.height-1,0,false}); chart_resize_input(a);
        PRESENT_GUI_CHECK(store->items[2].rows==16 && store->items[2].columns==9 && !a->chart_resizing && a->book.command_revision==layout_revision+8 && layout_caches(a));
        o=store->items+2; CellEditTarget target;
        PRESENT_GUI_CHECK(workbook_prepare_cell_edit(&a->book,a->book.active,o->row+16,o->col+9,&target,NULL));
        CellEditCommand edit={CELL_EDIT_COMMAND_VERSION,target,"Keep note",9};
        PRESENT_GUI_CHECK(workbook_apply_cell_edit(&a->book,&edit,(CellEditPolicy){0,0,true},NULL).status==CELL_EDIT_APPLIED);
        PRESENT_GUI_CHECK(!resize_chart(a,17,10) && a->chart_resize_error && store->items[2].rows==16 && store->items[2].columns==9 && a->book.command_revision==layout_revision+9 && layout_caches(a));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    }
    case 15:
        PRESENT_GUI_CHECK(layout_caches(a) && store->title_revision==layout_titles); workspace_job(a,JOB_SAVE,layout_path); break;
    case 16:
        PRESENT_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,layout_path); break;
    case 17:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        PRESENT_GUI_CHECK(store->items[2].kind==OBJECT_CHART && store->items[2].rows==16 && store->items[2].columns==9 && store->items[2].source_id==store->items[0].id &&
            a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==3 && a->book.command_revision==layout_revision+9 && !s->undo_actions_count && !a->book.dirty);
        PRESENT_GUI_CHECK(cells_get(&s->cells,store->items[2].row+16,store->items[2].col+9) && !strcmp(cells_get(&s->cells,store->items[2].row+16,store->items[2].col+9),"Keep note"));
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); layout_result=1;
        printf("PASS: native object layout workflow (18 stages, %u checks)\n",layout_checks); break;
    }
    layout_stage++;
}
int app_test_object_layout_command_result(void) { return layout_result; }
#undef PRESENT_GUI_CHECK
