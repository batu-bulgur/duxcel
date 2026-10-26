/* Actual manual navigation input only; analytical/typed editor activation has
   separate owners. Setup uses the existing App editor and genuine Save path. */
static unsigned data_boundary_stage,data_boundary_wait,data_boundary_checks;
static int data_boundary_result;
static uint64_t data_boundary_document,data_boundary_cells,data_boundary_formats,data_boundary_request,data_boundary_jobs,data_boundary_sql;
static size_t data_boundary_undo,data_boundary_redo,data_boundary_width_count;
#define DB_CHECK(x) do { data_boundary_checks++; if (!(x)) { fprintf(stderr,"FAIL data boundary stage %u:%d: %s (%s)\n",data_boundary_stage,__LINE__,#x,a->message ? a->message : ""); data_boundary_result=-1; return; } } while (0)
static void data_boundary_key(int key,bool shift) { ui_note_key_press(key,DX_MOD_SUPER|(shift ? DX_MOD_SHIFT : 0)); }
static bool data_boundary_put(App *a,uint64_t row,size_t column,const char *text) {
    grid_select_cell(&a->grid,row,column,false); open_modal(a,MODAL_CELL); if (a->modal!=MODAL_CELL) return false;
    input_set(&a->input,text); return commit_cell(a);
}
static void data_boundary_baseline(App *a) {
    Worksheet *s=active_sheet(a); data_boundary_document=a->book.command_revision; data_boundary_cells=s->cells.revision; data_boundary_formats=s->formats.revision;
    data_boundary_request=a->request_id; data_boundary_undo=s->undo_actions_count; data_boundary_redo=s->redo_actions_count; data_boundary_width_count=s->view.column_count;
    dx_lock(&a->worker.mutex); data_boundary_jobs=a->worker.jobs_started; data_boundary_sql=a->worker.sql_executed; dx_unlock(&a->worker.mutex);
}
static bool data_boundary_unchanged(App *a) {
    Worksheet *s=active_sheet(a); bool same=!a->book.dirty && a->book.command_revision==data_boundary_document && s->cells.revision==data_boundary_cells && s->formats.revision==data_boundary_formats &&
        a->request_id==data_boundary_request && s->undo_actions_count==data_boundary_undo && s->redo_actions_count==data_boundary_redo && s->view.column_count==data_boundary_width_count &&
        s->view.widths[0]==171 && !strcmp(cells_get(&s->cells,20000,25),"Far island");
    for (size_t i=0;same && i<s->view.column_count;i++) same=(i==0 || s->view.widths[i]==96) && (!s->view.columns[i] || !*s->view.columns[i]);
    dx_lock(&a->worker.mutex); same=same && a->worker.jobs_started==data_boundary_jobs && a->worker.sql_executed==data_boundary_sql; dx_unlock(&a->worker.mutex); return same;
}
static void data_boundary_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); if (data_boundary_result || !s || a->busy || a->io_busy || a->count_request_id) return;
    if (++data_boundary_wait<24) return; data_boundary_wait=0;
    switch (data_boundary_stage) {
    case 0: a->statistics_enabled=false; statistics_cancel(a); a->document_action=DOCUMENT_NEW; finish_document_action(a); remove("build/tests/generated/data-boundary.dxls"); break;
    case 1: {
        a->reduced_motion=true; a->sidebar=a->fields_visible=a->pipeline=false; a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false; set_ribbon_mode(a,0);
        ui_set_scale(&a->ui,1); SetWindowSize(960,640); a->grid.zoom=1;
        DB_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count);
        for (uint64_t row=5;row<=7;row++) DB_CHECK(data_boundary_put(a,row,2,"Contiguous"));
        for (uint64_t row=20;row<=21;row++) DB_CHECK(data_boundary_put(a,row,2,"Next island"));
        DB_CHECK(data_boundary_put(a,30,0,"Left") && data_boundary_put(a,30,1,"Adjacent") && data_boundary_put(a,30,4,"Gap target") && data_boundary_put(a,20000,25,"Far island"));
        CellFormat style=format_default(); style.flags=FORMAT_BOLD; char *error=NULL;
        bool formatted=format_apply(&s->formats,(FormatRange){30,30,3,3},FORMAT_BOLD,style,&error); free(error); DB_CHECK(formatted); worksheet_record(s,EDIT_FORMAT);
        grid_set_width(&a->grid,0,171); DB_CHECK(commit_grid_widths(a)); grid_select_cell(&a->grid,5,2,false); a->grid.top=0; a->grid.fraction=0; a->grid.horizontal=0;
        workspace_job(a,JOB_SAVE,"build/tests/generated/data-boundary.dxls"); break;
    }
    case 2: DB_CHECK(!a->book.dirty && s->view.column_count && s->view.widths[0]==171); data_boundary_baseline(a); grid_select_cell(&a->grid,5,2,false); data_boundary_key(KEY_DOWN,false); break;
    case 3: DB_CHECK(a->grid.row==7 && a->grid.anchor_row==7 && a->grid.end_row==7); data_boundary_key(KEY_DOWN,false); break;
    case 4: DB_CHECK(a->grid.row==20); data_boundary_key(KEY_DOWN,true); break;
    case 5: DB_CHECK(a->grid.row==20 && a->grid.anchor_row==20 && a->grid.end_row==21); data_boundary_key(KEY_UP,true); break;
    case 6: DB_CHECK(a->grid.anchor_row==20 && a->grid.end_row==20); data_boundary_key(KEY_UP,true); break;
    case 7: DB_CHECK(a->grid.row==20 && a->grid.anchor_row==20 && a->grid.end_row==7); data_boundary_key(KEY_UP,true); break;
    case 8: DB_CHECK(a->grid.anchor_row==20 && a->grid.end_row==5); data_boundary_key(KEY_DOWN,true); break;
    case 9: DB_CHECK(a->grid.anchor_row==20 && a->grid.end_row==7); grid_select_cell(&a->grid,30,0,false); data_boundary_key(KEY_RIGHT,false); break;
    case 10: DB_CHECK(a->grid.col==1); data_boundary_key(KEY_RIGHT,false); break;
    case 11: DB_CHECK(a->grid.col==4); data_boundary_key(KEY_RIGHT,false); break;
    case 12: DB_CHECK(a->grid.col==GRID_BOUNDARY_COLUMNS-1 && a->grid.width_count==GRID_BOUNDARY_COLUMNS && data_boundary_unchanged(a)); data_boundary_key(KEY_LEFT,true); break;
    case 13: DB_CHECK(a->grid.col==GRID_BOUNDARY_COLUMNS-1 && a->grid.anchor_col==GRID_BOUNDARY_COLUMNS-1 && a->grid.end_col==4); data_boundary_key(KEY_RIGHT,true); break;
    case 14: DB_CHECK(a->grid.anchor_col==GRID_BOUNDARY_COLUMNS-1 && a->grid.end_col==GRID_BOUNDARY_COLUMNS-1); data_boundary_key(KEY_LEFT,false); break;
    case 15: DB_CHECK(a->grid.col==4 && a->grid.anchor_col==4 && a->grid.end_col==4); data_boundary_key(KEY_LEFT,false); break;
    case 16: DB_CHECK(a->grid.col==1); data_boundary_key(KEY_LEFT,false); break;
    case 17: DB_CHECK(!a->grid.col && data_boundary_unchanged(a)); data_boundary_key(KEY_LEFT,false); ui_set_scale(&a->ui,1.5f); SetWindowSize(720,480); break;
    case 18: DB_CHECK(!a->grid.col && data_boundary_unchanged(a)); grid_select_cell(&a->grid,5,2,false); data_boundary_key(KEY_DOWN,false); break;
    case 19: DB_CHECK(a->grid.row==7); data_boundary_key(KEY_DOWN,false); break;
    case 20: DB_CHECK(a->grid.row==20); data_boundary_key(KEY_UP,false); break;
    case 21: DB_CHECK(a->grid.row==7); data_boundary_key(KEY_UP,false); break;
    case 22: DB_CHECK(a->grid.row==5); ui_note_key_press(KEY_F2,0); break;
    case 23: DB_CHECK(a->modal==MODAL_CELL && a->grid.row==5 && a->grid.col==2); data_boundary_key(KEY_DOWN,false); break;
    case 24: DB_CHECK(a->modal==MODAL_CELL && a->grid.row==5 && a->grid.col==2 && data_boundary_unchanged(a)); ui_note_key_press(KEY_ESCAPE,0); break;
    case 25: {
        DB_CHECK(a->modal==MODAL_NONE && !a->grid.manual_occupancy_active && !a->grid.manual_occupancy.context);
        AppGridBoundary target; app_grid_boundary_begin(a,&target); GridBoundaryCell cell={77,77,false}; DB_CHECK(a->grid.manual_occupancy_active && app_grid_boundary_read(&target,0,&cell));
        uint64_t request=target.request; target.request++; cell.row=77; DB_CHECK(!app_grid_boundary_read(&target,0,&cell) && cell.row==77); target.request=request;
        Workbook *copy=workbook_snapshot(&a->book); DB_CHECK(copy); target.entries=copy->sheets[copy->active].cells.cells;
        bool rejected=!app_grid_boundary_read(&target,0,&cell); workbook_free(copy); free(copy); DB_CHECK(rejected); app_grid_boundary_end(a);
        grid_select_cell(&a->grid,19999,25,false); data_boundary_key(KEY_DOWN,false); break;
    }
    case 26: DB_CHECK(a->grid.row==20000); data_boundary_key(KEY_DOWN,false); break;
    case 27: DB_CHECK(a->grid.row==DX_SHEET_ROWS-1 && data_boundary_unchanged(a)); data_boundary_key(KEY_DOWN,false); break;
    case 28: {
        DB_CHECK(a->grid.row==DX_SHEET_ROWS-1 && data_boundary_unchanged(a));
        AppGridBoundary target; a->busy=true; app_grid_boundary_begin(a,&target); bool guarded=!a->grid.manual_occupancy_active; a->busy=false; DB_CHECK(guarded);
        SourceKind kind=a->analysis.kind; a->analysis.kind=SOURCE_CSV; app_grid_boundary_begin(a,&target); guarded=!a->grid.manual_occupancy_active; a->analysis.kind=kind; DB_CHECK(guarded); app_grid_boundary_end(a);
        workspace_job(a,JOB_LOAD,"build/tests/generated/data-boundary.dxls"); break;
    }
    case 29: DB_CHECK(!a->book.dirty && !strcmp(cells_get(&s->cells,20000,25),"Far island") && (format_at(&s->formats,30,3).flags&FORMAT_BOLD)); data_boundary_baseline(a); grid_select_cell(&a->grid,5,2,false); data_boundary_key(KEY_DOWN,false); break;
    case 30: DB_CHECK(a->grid.row==7 && data_boundary_unchanged(a) && !a->grid.manual_occupancy_active && !a->grid.manual_occupancy.context); data_boundary_result=1; printf("PASS: native manual data-boundary workflow (31 stages, %u checks)\n",data_boundary_checks); break;
    }
    data_boundary_stage++;
}
int app_test_data_boundary_result(void) { return data_boundary_result; }
unsigned app_test_data_boundary_stage(void) { return data_boundary_stage; }
#undef DB_CHECK
