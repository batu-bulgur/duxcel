/* Private real-App workflow. Only the existing native destination dialog is
   stubbed; shared input, Save worker, cancellation and disk IO remain real. */
#ifdef __APPLE__
extern bool dx_test_session_native_versions(void);
#endif
static unsigned version_gui_stage,version_gui_wait,version_gui_checks,version_gui_destinations;
static int version_gui_result;
static bool version_gui_destination,version_gui_gate_ready,version_gui_armed,version_gui_entered,version_gui_release;
static DxMutex version_gui_mutex; static DxCond version_gui_condition;
static const char *version_gui_path="build/tests/generated/version-controls.dxls",*version_gui_bad="build/tests/generated/version-controls-invalid.dxls";
#define VERSION_GUI_CHECK(x) do { version_gui_checks++; if (!(x)) { fprintf(stderr,"FAIL version GUI stage %u:%d: %s (%s)\n",version_gui_stage,__LINE__,#x,a->message ? a->message : ""); version_gui_result=-1; return; } } while (0)
char *dx_test_version_destination(void) { version_gui_destinations++; return version_gui_destination ? dx_strdup(version_gui_path) : NULL; }
void dx_test_save_checkpoint(QueryWorker *worker,WorkbookSaveProgress progress) {
    (void)worker; if (!version_gui_gate_ready) return;
    dx_lock(&version_gui_mutex);
    if (version_gui_armed && progress.phase==SAVE_PHASE_WRITE && !version_gui_entered) {
        version_gui_entered=true; while (!version_gui_release) dx_cond_wait(&version_gui_condition,&version_gui_mutex);
    }
    dx_unlock(&version_gui_mutex);
}
static void version_gui_click(Rectangle r) { ui_note_pointer_gesture((DxPointerGesture){r.x+10,r.y+10,r.x+10,r.y+10,0,false}); }
static bool version_gui_old(App *a) {
    const WorkbookVersionIndex *index=versions_index(a); if (!index || !index->tag_count) return false;
    size_t i=0; while (i<index->root_count && memcmp(index->roots[i].key.bytes,index->tags[0].root.bytes,DX_VERSION_KEY_BYTES)) i++;
    if (i==index->root_count) return false;
    Workbook b; workbook_init(&b); char *error=NULL;
    bool ok=workbook_load_core_payload(&b,workbook_core_payload_path(workbook_version_core_set_leaf(a->versions.store,i)),&error);
    ok=ok && !error && !strcmp(cells_get(&b.sheets[0].cells,9000,30),"R1"); free(error); workbook_free(&b); return ok;
}
static void version_gui_name(App *a,const char *name) { input_set(&a->input,name); a->version_view.focus=0; ui_note_key_press(KEY_ENTER,0); }
static void version_controls_test_frame(AppSession *session) {
    App *a=&session->app; if (version_gui_result || !active_sheet(a) || ++version_gui_wait<12) return; version_gui_wait=0;
    if ((a->busy || a->io_busy) && version_gui_stage!=22 && version_gui_stage!=23) return;
    const WorkbookVersionIndex *index=versions_index(a); const UIVersionLayout *l=&a->version_view.layout;
    switch (version_gui_stage) {
    case 0: {
        remove(version_gui_path); dx_mutex_init(&version_gui_mutex); dx_cond_init(&version_gui_condition); version_gui_gate_ready=true;
        char *error=NULL; VERSION_GUI_CHECK(cells_set(&active_sheet(a)->cells,9000,30,"R1",&error) && !error); worksheet_record(active_sheet(a),EDIT_CELLS);
        a->file_menu=true; a->file_menu_frame=a->frames-1;
        UIChoiceListLayout menu=file_menu_layout(a);
        VERSION_GUI_CHECK(menu.frame.valid && menu.visible==UI_KEYTIP_FILE_COUNT && menu.frame.count==UI_KEYTIP_FILE_COUNT);
        version_gui_click(menu.frame.rows[UI_KEYTIP_FILE_VERSIONS]); break;
    }
    case 1:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && !a->book.path && !index && l->valid && l->panel.width==480 && l->panel.height==300);
        version_gui_click(l->create); break;
    case 2:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && a->version_message[0] && !version_gui_destinations && !a->io_busy);
        input_set(&a->input,"V1"); version_gui_click(l->create); break;
    case 3:
        VERSION_GUI_CHECK(version_gui_destinations==1 && !a->book.path && !index && !a->io_busy && a->modal==MODAL_VERSIONS);
        version_gui_destination=true; version_gui_click(l->create); break;
    case 4:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && a->book.path && !a->book.dirty && index && index->saved.present && index->tag_count==1 && index->root_count==1);
        VERSION_GUI_CHECK(version_gui_destinations==2 && version_gui_old(a)); smoke_snapshot("build/tests/results/version-controls/first.png");
        input_set(&a->input,"V1"); version_gui_click(l->create); break;
    case 5:
        VERSION_GUI_CHECK(index->tag_count==1 && !a->io_busy && a->version_message[0] && version_gui_destinations==2);
        version_gui_name(a,"V2"); break;
    case 6:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && index->tag_count==2 && index->root_count==1 && !a->book.dirty);
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 7: {
        VERSION_GUI_CHECK(a->modal==MODAL_NONE); char *error=NULL;
        VERSION_GUI_CHECK(cells_set(&active_sheet(a)->cells,9000,30,"R2",&error) && !error); worksheet_record(active_sheet(a),EDIT_CELLS); a->book.dirty=true;
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Visible edit"); ui_note_key_press(KEY_ENTER,0); break;
    }
    case 8:
        VERSION_GUI_CHECK(a->book.dirty && a->modal==MODAL_NONE); file_action(a,UI_KEYTIP_FILE_VERSIONS); version_gui_name(a,"V3"); break;
    case 9:
        VERSION_GUI_CHECK(index->tag_count==3 && index->root_count==2 && !a->book.dirty && version_gui_old(a));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); break;
    case 10:
        VERSION_GUI_CHECK(l->valid && l->panel.x>=0 && l->panel.y>=0 && l->panel.x+l->panel.width<=640 && l->panel.y+l->panel.height<=640);
        input_set(&a->input,"V4"); ui_note_key_press(KEY_TAB,0); break;
    case 11:
        VERSION_GUI_CHECK(a->version_view.focus==0); ui_note_key_press(KEY_ENTER,0); break;
    /* The changed display scale belongs to the captured supported state, so
       V4 correctly adds one root; V5/V6 then reuse that complete state. */
    case 12:
        VERSION_GUI_CHECK(index->tag_count==4 && index->root_count==3); version_gui_name(a,"V5"); break;
    case 13:
        VERSION_GUI_CHECK(index->tag_count==5 && index->root_count==3); version_gui_name(a,"V6"); break;
    case 14:
        VERSION_GUI_CHECK(index->tag_count==6 && index->root_count==3 && a->version_view.selected==5 && a->version_view.scroll>0 && l->scrollbar.width>0);
        ui_note_key_press(KEY_HOME,0); break;
    case 15:
        VERSION_GUI_CHECK(a->version_view.selected==SIZE_MAX && !a->version_view.scroll); ui_note_key_press(KEY_END,0); break;
    case 16:
        VERSION_GUI_CHECK(a->version_view.selected==5 && a->version_view.scroll>0 && l->scrollbar.width>0); smoke_snapshot("build/tests/results/version-controls/list-150.png"); ui_note_key_press(KEY_ESCAPE,0); break;
    case 17:
        VERSION_GUI_CHECK(a->modal==MODAL_NONE); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); workspace_job(a,JOB_LOAD,version_gui_path); break;
    case 18:
        VERSION_GUI_CHECK(index && index->tag_count==6 && index->root_count==3 && index->saved.present && !a->book.dirty && version_gui_old(a));
        VERSION_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,9000,30),"R2"));
#ifdef __APPLE__
        VERSION_GUI_CHECK(dx_test_session_native_versions());
#else
        file_action(a,UI_KEYTIP_FILE_VERSIONS);
#endif
        break;
    case 19: {
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS);
        FILE *bad=fopen(version_gui_bad,"wb"); VERSION_GUI_CHECK(bad && fputs("invalid workbook",bad)>=0 && !fclose(bad));
        free(a->book.path); a->book.path=dx_absolute_path(version_gui_bad); version_gui_name(a,"Failed"); break;
    }
    case 20:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && index->tag_count==6 && a->version_message[0] && !strcmp(a->input.text,"Failed"));
        free(a->book.path); a->book.path=dx_absolute_path(version_gui_path);
        { uint64_t request=a->request_id; a->versions.identity_valid=false; versions_create(a);
          VERSION_GUI_CHECK(!a->io_busy && a->request_id==request && a->modal==MODAL_VERSIONS && !strcmp(a->input.text,"Failed"));
          a->versions.identity_valid=true; }
        dx_lock(&version_gui_mutex); version_gui_armed=true; dx_unlock(&version_gui_mutex); break;
    case 21: version_gui_name(a,"Cancelled"); break;
    case 22: {
        dx_lock(&version_gui_mutex); bool entered=version_gui_entered; dx_unlock(&version_gui_mutex); if (!entered) return;
        VERSION_GUI_CHECK(a->io_busy && worker_save_state(&a->worker).cancellable); ui_note_key_press(KEY_ESCAPE,0); break;
    }
    case 23:
        VERSION_GUI_CHECK(worker_save_state(&a->worker).cancel_requested);
        dx_lock(&version_gui_mutex); version_gui_release=true; dx_signal(&version_gui_condition); dx_unlock(&version_gui_mutex); break;
    case 24:
        VERSION_GUI_CHECK(a->modal==MODAL_VERSIONS && index->tag_count==6 && !strcmp(a->input.text,"Cancelled") && version_gui_old(a));
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 25:
        VERSION_GUI_CHECK(a->modal==MODAL_NONE && !a->book.dirty); file_action(a,UI_KEYTIP_FILE_NEW);
        VERSION_GUI_CHECK(!a->versions.store && !a->book.dirty); version_gui_result=1; break;
    default: version_gui_result=-1; return;
    }
    version_gui_stage++;
}
int app_test_version_controls_result(void) { return version_gui_result; }
void app_test_version_controls_cleanup(void) {
    if (version_gui_gate_ready) { dx_lock(&version_gui_mutex); version_gui_release=true; dx_signal(&version_gui_condition); dx_unlock(&version_gui_mutex); }
}
void app_test_version_controls_finished(void) {
    if (version_gui_gate_ready) { dx_cond_destroy(&version_gui_condition); dx_mutex_destroy(&version_gui_mutex); }
    if (version_gui_result==1) printf("PASS: native version controls workflow (26 stages, %u checks)\n",version_gui_checks);
}
