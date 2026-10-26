/* Actual keyboard/address entry fixture. Compiled out of production. */
static unsigned go_to_stage,go_to_wait,go_to_checks;
static int go_to_result;
static uint64_t go_to_document,go_to_cells,go_to_formats,go_to_request,go_to_jobs,go_to_sql,go_to_session;
static size_t go_to_undo,go_to_redo;
#define GT_CHECK(x) do { go_to_checks++; if (!(x)) { fprintf(stderr,"FAIL Go To stage %u:%d: %s\n",go_to_stage,__LINE__,#x); go_to_result=-1; return; } } while (0)
static void go_to_key(int key,unsigned modifiers) { ui_note_key_press(key,modifiers); }
static void go_to_baseline(App *a) {
    Worksheet *s=active_sheet(a); go_to_document=a->book.command_revision; go_to_cells=s->cells.revision; go_to_formats=s->formats.revision;
    go_to_request=a->request_id; go_to_undo=s->undo_actions_count; go_to_redo=s->redo_actions_count;
    dx_lock(&a->worker.mutex); go_to_jobs=a->worker.jobs_started; go_to_sql=a->worker.sql_executed; dx_unlock(&a->worker.mutex);
}
static bool go_to_unchanged(App *a) {
    Worksheet *s=active_sheet(a); bool same=!a->book.dirty && a->book.command_revision==go_to_document && s->cells.revision==go_to_cells && s->formats.revision==go_to_formats &&
        a->request_id==go_to_request && s->undo_actions_count==go_to_undo && s->redo_actions_count==go_to_redo && s->view.widths[0]==173 && !strcmp(cells_get(&s->cells,8,1),"Preserved");
    dx_lock(&a->worker.mutex); same=same && a->worker.jobs_started==go_to_jobs && a->worker.sql_executed==go_to_sql; dx_unlock(&a->worker.mutex); return same;
}
static void go_to_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (go_to_result || !s || a->busy || a->io_busy || a->count_request_id || ++go_to_wait<24) return; go_to_wait=0;
    switch (go_to_stage) {
    case 0: a->statistics_enabled=false; statistics_cancel(a); a->document_action=DOCUMENT_NEW; finish_document_action(a); remove("build/tests/generated/go-to.dxls"); break;
    case 1: {
        a->reduced_motion=true; a->sidebar=a->fields_visible=a->pipeline=false; a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false; set_ribbon_mode(a,0);
        ui_set_scale(&a->ui,1); SetWindowSize(960,640); a->grid.zoom=1;
        grid_select_cell(&a->grid,8,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Preserved"); GT_CHECK(commit_cell(a));
        grid_set_width(&a->grid,0,173); GT_CHECK(commit_grid_widths(a)); workspace_job(a,JOB_SAVE,"build/tests/generated/go-to.dxls"); break;
    }
    case 2: GT_CHECK(!a->book.dirty && a->modal==MODAL_NONE); go_to_baseline(a); go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 3: GT_CHECK(a->modal==MODAL_GO_TO && !strcmp(a->input.text,"B9") && go_to_unchanged(a)); input_set(&a->input,"A0"); go_to_key(KEY_ENTER,0); break;
    case 4: GT_CHECK(a->modal==MODAL_GO_TO && !strcmp(a->input.text,"A0") && a->grid.row==8 && a->grid.col==1 && go_to_unchanged(a)); {
        char long_address[96]; memset(long_address,' ',sizeof(long_address)); memcpy(long_address,"C3",2); long_address[95]=0; input_set(&a->input,long_address); go_to_key(KEY_ENTER,0); break;
    }
    case 5: GT_CHECK(a->modal==MODAL_GO_TO && a->input.length==95 && a->grid.row==8 && a->grid.col==1 && go_to_unchanged(a)); input_set(&a->input,"XFD1000001"); go_to_key(KEY_ENTER,0); break;
    case 6: {
        GT_CHECK(a->modal==MODAL_NONE && a->grid.row==1000000 && a->grid.col==16383 && go_to_unchanged(a));
        float view=a->grid.body.width/(a->ui.scale*a->grid.zoom); GT_CHECK(a->grid.offsets[16383]>=a->grid.horizontal && a->grid.offsets[16384]-a->grid.horizontal<=view+.001f);
        go_to_key(KEY_G,DX_MOD_SUPER); break;
    }
    case 7: GT_CHECK(a->modal==MODAL_GO_TO); go_to_key(KEY_ESCAPE,0); break;
    case 8: GT_CHECK(a->modal==MODAL_NONE && go_to_unchanged(a)); { Rectangle r=a->cell_context.address; GT_CHECK(r.width>0); ui_note_pointer_gesture((DxPointerGesture){r.x+5,r.y+5,r.x+5,r.y+5,0,false}); break; }
    case 9: GT_CHECK(a->modal==MODAL_GO_TO); go_to_key(KEY_TAB,0); break;
    case 10: GT_CHECK(a->go_to.focus==1); go_to_key(KEY_ENTER,0); break;
    case 11: GT_CHECK(a->modal==MODAL_NONE && a->grid.row==1000000 && go_to_unchanged(a)); go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 12: GT_CHECK(a->modal==MODAL_GO_TO); input_set(&a->input,"$B$9"); go_to_key(KEY_TAB,0); break;
    case 13: GT_CHECK(a->go_to.focus==1); go_to_key(KEY_TAB,0); break;
    case 14: GT_CHECK(a->go_to.focus==2); go_to_key(KEY_ENTER,0); break;
    case 15: GT_CHECK(a->modal==MODAL_NONE && a->grid.row==8 && a->grid.col==1 && go_to_unchanged(a)); ui_set_scale(&a->ui,1.5f); SetWindowSize(720,480); go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 16: GT_CHECK(a->modal==MODAL_GO_TO && ui_go_to_layout((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},a->ui.scale).valid); input_set(&a->input,"Sheet1!A1"); go_to_key(KEY_ENTER,0); break;
    case 17: GT_CHECK(a->modal==MODAL_GO_TO && !strcmp(a->input.text,"Sheet1!A1") && go_to_unchanged(a)); input_set(&a->input,"A1"); a->request_id++; go_to_key(KEY_ENTER,0); break;
    case 18: GT_CHECK(a->modal==MODAL_GO_TO && a->grid.row==8 && a->grid.col==1 && !strcmp(a->input.text,"A1")); a->request_id--; go_to_key(KEY_ESCAPE,0); break;
    case 19: GT_CHECK(a->modal==MODAL_NONE && go_to_unchanged(a)); go_to_key(KEY_F2,0); break;
    case 20: GT_CHECK(a->modal==MODAL_CELL); input_set(&a->input,"Uncommitted editor draft"); go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 21: GT_CHECK(a->modal==MODAL_CELL && !strcmp(a->input.text,"Uncommitted editor draft") && go_to_unchanged(a)); go_to_key(KEY_ESCAPE,0); break;
    case 22: GT_CHECK(a->modal==MODAL_NONE); a->chrome_focus=2; go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 23: GT_CHECK(a->modal==MODAL_NONE && a->chrome_focus==2 && go_to_unchanged(a)); a->chrome_focus=0; go_to_key(KEY_G,DX_MOD_SUPER); break;
    case 24: GT_CHECK(a->modal==MODAL_GO_TO); go_to_session=a->book.edit_session_id; a->book.edit_session_id++; input_set(&a->input,"Z99"); go_to_key(KEY_ENTER,0); break;
    case 25: GT_CHECK(a->modal==MODAL_GO_TO && a->grid.row==8 && a->grid.col==1 && !strcmp(a->input.text,"Z99")); a->book.edit_session_id=go_to_session; go_to_key(KEY_ESCAPE,0); break;
    case 26: GT_CHECK(a->modal==MODAL_NONE && go_to_unchanged(a)); go_to_result=1; printf("PASS: native same-sheet Go To workflow (27 stages, %u checks)\n",go_to_checks); break;
    }
    go_to_stage++;
}
int app_test_go_to_result(void) { return go_to_result; }
unsigned app_test_go_to_stage(void) { return go_to_stage; }
#undef GT_CHECK
