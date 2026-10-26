/* Existing App editor/Save plus actual View keytips and modal input. */
static unsigned find_stage,find_wait,find_checks;
static int find_result;
static uint64_t find_document,find_cells,find_formats,find_request,find_jobs,find_sql;
static size_t find_undo,find_redo;
#define FIND_CHECK(x) do { find_checks++; if (!(x)) { fprintf(stderr,"FAIL manual Find stage %u:%d: %s\n",find_stage,__LINE__,#x); find_result=-1; return; } } while (0)
static void find_key(int key,unsigned modifiers) { ui_note_key_press(key,modifiers); }
static void find_access(int last) { find_key(KEY_LEFT_ALT,0); find_key(KEY_W,0); find_key(KEY_Z,0); find_key(last,0); }
static void find_baseline(App *a) {
    Worksheet *s=active_sheet(a); find_document=a->book.command_revision; find_cells=s->cells.revision; find_formats=s->formats.revision;
    find_request=a->request_id; find_undo=s->undo_actions_count; find_redo=s->redo_actions_count;
    dx_lock(&a->worker.mutex); find_jobs=a->worker.jobs_started; find_sql=a->worker.sql_executed; dx_unlock(&a->worker.mutex);
}
static bool find_unchanged(App *a) {
    Worksheet *s=active_sheet(a); bool same=!a->book.dirty && a->book.command_revision==find_document && s->cells.revision==find_cells && s->formats.revision==find_formats &&
        a->request_id==find_request && s->undo_actions_count==find_undo && s->redo_actions_count==find_redo && s->view.widths[0]==173 &&
        !strcmp(cells_get(&s->cells,0,0),"alpha") && !strcmp(cells_get(&s->cells,0,3),"alphabet") && !strcmp(cells_get(&s->cells,9,1),"alpha") &&
        !strcmp(cells_get(&s->cells,20000,25),"alpha") && !strcmp(cells_get(&s->cells,10,0),"NULL") && (format_at(&s->formats,0,3).flags&FORMAT_BOLD);
    dx_lock(&a->worker.mutex); same=same && a->worker.jobs_started==find_jobs && a->worker.sql_executed==find_sql; dx_unlock(&a->worker.mutex); return same;
}
static bool find_put(App *a,uint64_t row,size_t column,const char *text) {
    grid_select_cell(&a->grid,row,column,false); open_modal(a,MODAL_CELL); if (a->modal!=MODAL_CELL) return false;
    input_set(&a->input,text); return commit_cell(a);
}
static void find_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (find_result || !s || a->busy || a->io_busy || a->count_request_id || ++find_wait<24) return; find_wait=0;
    switch (find_stage) {
    case 0: a->statistics_enabled=false; statistics_cancel(a); a->document_action=DOCUMENT_NEW; finish_document_action(a); remove("build/tests/generated/manual-find.dxls"); break;
    case 1: {
        a->reduced_motion=true; a->sidebar=a->fields_visible=a->pipeline=false; a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false; set_ribbon_mode(a,0);
        ui_set_scale(&a->ui,1); SetWindowSize(960,640); a->grid.zoom=1;
        FIND_CHECK(find_put(a,0,0,"alpha") && find_put(a,0,3,"alphabet") && find_put(a,9,1,"alpha") && find_put(a,20000,25,"alpha") && find_put(a,10,0,"NULL"));
        CellFormat style=format_default(); style.flags=FORMAT_BOLD; char *error=NULL;
        bool formatted=format_apply(&s->formats,(FormatRange){0,0,3,3},FORMAT_BOLD,style,&error); free(error); FIND_CHECK(formatted); worksheet_record(s,EDIT_FORMAT);
        grid_set_width(&a->grid,0,173); FIND_CHECK(commit_grid_widths(a)); grid_select_cell(&a->grid,0,0,false); workspace_job(a,JOB_SAVE,"build/tests/generated/manual-find.dxls"); break;
    }
    case 2: FIND_CHECK(!a->book.dirty && a->modal==MODAL_NONE); find_baseline(a); find_key(KEY_LEFT_ALT,0); break;
    case 3: FIND_CHECK(a->key_tips); find_key(KEY_W,0); break;
    case 4: FIND_CHECK(a->key_tips && a->keytip_state.tab==RIBBON_VIEW); find_key(KEY_Z,0); break;
    case 5: FIND_CHECK(a->key_tips && !strcmp(a->keytip_state.prefix,"Z")); find_key(KEY_C,0); break;
    case 6: FIND_CHECK(a->modal==MODAL_FIND && !a->input.length && find_unchanged(a)); input_set(&a->input,"alpha"); find_key(KEY_ENTER,0); break;
    case 7: FIND_CHECK(a->grid.row==0 && a->grid.col==3 && strstr(a->find.message,"4 matches") && find_unchanged(a)); find_key(KEY_ENTER,0); break;
    case 8: FIND_CHECK(a->grid.row==9 && a->grid.col==1); find_key(KEY_ENTER,0); break;
    case 9: FIND_CHECK(a->grid.row==20000 && a->grid.col==25 && find_unchanged(a)); find_key(KEY_TAB,DX_MOD_SHIFT); break;
    case 10: FIND_CHECK(a->find.focus==4); find_key(KEY_TAB,DX_MOD_SHIFT); break;
    case 11: FIND_CHECK(a->find.focus==3); find_key(KEY_TAB,DX_MOD_SHIFT); break;
    case 12: FIND_CHECK(a->find.focus==2); find_key(KEY_ENTER,0); break;
    case 13: FIND_CHECK(a->grid.row==9 && a->grid.col==1); find_key(KEY_TAB,DX_MOD_SHIFT); break;
    case 14: FIND_CHECK(a->find.focus==1); find_key(KEY_ENTER,0); break;
    case 15: FIND_CHECK(a->find.whole_cell && a->grid.row==9); find_key(KEY_TAB,DX_MOD_SHIFT); break;
    case 16: FIND_CHECK(!a->find.focus); find_key(KEY_ENTER,0); break;
    case 17: FIND_CHECK(a->grid.row==20000 && a->grid.col==25 && strstr(a->find.message,"3 matches")); find_key(KEY_ENTER,0); break;
    case 18: FIND_CHECK(!a->grid.row && !a->grid.col && strstr(a->find.message,"wrapped")); input_set(&a->input,"ALPHA"); find_key(KEY_ENTER,0); break;
    case 19: FIND_CHECK(!a->grid.row && !a->grid.col && strstr(a->find.message,"No matching") && !strcmp(a->input.text,"ALPHA")); input_set(&a->input,""); find_key(KEY_ENTER,0); break;
    case 20: FIND_CHECK(a->modal==MODAL_FIND && !a->grid.row && !a->grid.col && !a->input.length); {
        char overlong[GRID_FIND_QUERY_BYTES+2]; memset(overlong,'a',sizeof(overlong)-1); overlong[sizeof(overlong)-1]=0; input_set(&a->input,overlong); find_key(KEY_ENTER,0); break;
    }
    case 21: FIND_CHECK(a->input.length==GRID_FIND_QUERY_BYTES+1 && !a->grid.row && !a->grid.col && find_unchanged(a)); input_set(&a->input,"alpha"); a->request_id++; find_key(KEY_ENTER,0); break;
    case 22: FIND_CHECK(a->modal==MODAL_FIND && !a->grid.row && !a->grid.col && !strcmp(a->input.text,"alpha")); a->request_id--; find_key(KEY_ESCAPE,0); break;
    case 23: FIND_CHECK(a->modal==MODAL_NONE && find_unchanged(a)); ui_set_scale(&a->ui,1.5f); SetWindowSize(640,480); find_access(KEY_C); break;
    case 24: FIND_CHECK(a->modal==MODAL_FIND && ui_find_layout((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},a->ui.scale).valid); input_set(&a->input,"NULL"); find_key(KEY_ENTER,0); break;
    case 25: FIND_CHECK(a->grid.row==10 && !a->grid.col && strstr(a->find.message,"1 match") && find_unchanged(a)); find_key(KEY_ESCAPE,0); break;
    case 26: FIND_CHECK(a->modal==MODAL_NONE); find_access(KEY_D); break;
    case 27: FIND_CHECK(a->modal==MODAL_GO_TO && !strcmp(a->input.text,"A11")); input_set(&a->input,"B10"); find_key(KEY_ENTER,0); break;
    case 28: FIND_CHECK(a->modal==MODAL_NONE && a->grid.row==9 && a->grid.col==1 && find_unchanged(a)); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); a->sidebar=true; a->workbook_collapsed=false; break;
    case 29: FIND_CHECK(a->sidebar && !a->workbook_collapsed && a->workspace.left.width>0 && !a->workspace.left_overlay); find_key(KEY_F6,DX_MOD_SHIFT); break;
    case 30: FIND_CHECK(a->chrome_focus==2 && !a->ribbon_focus && !a->pivot_focus && !action_enabled(a,ACTION_FIND)); input_set(&a->input,"Prior draft"); run_action(a,ACTION_FIND); break;
    case 31: FIND_CHECK(a->modal==MODAL_NONE && a->chrome_focus==2 && !strcmp(a->input.text,"Prior draft") && find_unchanged(a)); find_key(KEY_F6,0); break;
    case 32: FIND_CHECK(!a->chrome_focus && !a->ribbon_focus && !a->pivot_focus); find_key(KEY_F2,0); break;
    case 33: FIND_CHECK(a->modal==MODAL_CELL); input_set(&a->input,"Editor draft"); find_access(KEY_C); break;
    case 34: FIND_CHECK(a->modal==MODAL_CELL && !strcmp(a->input.text,"Editor draft") && find_unchanged(a)); find_key(KEY_ESCAPE,0); break;
    case 35: FIND_CHECK(a->modal==MODAL_NONE && find_unchanged(a)); find_result=1; printf("PASS: native manual Find workflow (36 stages, %u checks)\n",find_checks); break;
    }
    find_stage++;
}
int app_test_manual_find_result(void) { return find_result; }
unsigned app_test_manual_find_stage(void) { return find_stage; }
#undef FIND_CHECK
