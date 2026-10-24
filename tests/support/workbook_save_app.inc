/* Private real-App fixture; shipping objects do not contain these symbols. */
static int save_gui_stage,save_gui_result,save_gui_wait;
static unsigned save_gui_checks;
static DxMutex save_gui_mutex;
static DxCond save_gui_condition;
static bool save_gui_pan,save_gui_ready,save_gui_armed,save_gui_entered,save_gui_release;
static WorkbookSavePhase save_gui_phase;
static const char *save_gui_path="build/tests/generated/controlled-save-demo.dxls";
static const char *save_gui_bad="build/tests/generated/controlled-save-invalid.dxls";
#define SAVE_GUI_CHECK(x) do { save_gui_checks++; if (!(x)) { fprintf(stderr,"FAIL native Save stage %d:%d: %s\n",save_gui_stage,__LINE__,#x); save_gui_result=-1; return; } } while (0)
void dx_test_save_checkpoint(QueryWorker *worker,WorkbookSaveProgress progress) {
    (void)worker; if (!save_gui_ready) return;
    dx_lock(&save_gui_mutex);
    if (save_gui_armed && !save_gui_entered && progress.phase==save_gui_phase) {
        save_gui_entered=true;
        while (!save_gui_release) dx_cond_wait(&save_gui_condition,&save_gui_mutex);
    }
    dx_unlock(&save_gui_mutex);
}
static void save_gui_gate(WorkbookSavePhase phase) {
    dx_lock(&save_gui_mutex); save_gui_phase=phase; save_gui_armed=true; save_gui_entered=save_gui_release=false; dx_unlock(&save_gui_mutex);
}
static void save_gui_unblock(void) { dx_lock(&save_gui_mutex); save_gui_release=true; dx_signal(&save_gui_condition); dx_unlock(&save_gui_mutex); }
static bool save_gui_waiting(void) { dx_lock(&save_gui_mutex); bool entered=save_gui_entered; dx_unlock(&save_gui_mutex); return entered; }
static void save_gui_edit(App *a,const char *text) { a->chrome_focus=0; grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,text); ui_note_key_press(KEY_ENTER,0); }
static bool save_gui_value(const char *text) {
    WorkbookArchiveCandidate *candidate=NULL; char *error=NULL;
    bool ok=workbook_archive_read(save_gui_path,"build/tests/generated",DX_APP_VERSION_BYTES,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY;
    const Workbook *b=workbook_archive_book(candidate);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(workbook_archive_versions(candidate));
    ok=ok && !error && b && index && index->saved.present && workbook_archive_file_version(candidate)==16 &&
        !strcmp(cells_get(&b->sheets[0].cells,1,1),text);
    free(error); workbook_archive_candidate_free(candidate); return ok;
}
static void workbook_save_test_frame(AppSession *session) {
    App *a=&session->app; if (save_gui_result || !active_sheet(a) || ++save_gui_wait<12) return; save_gui_wait=0;
    WorkerSaveState state=worker_save_state(&a->worker);
    bool busy=a->busy || a->io_busy;
    switch (save_gui_stage) {
    case 0: {
        dx_mutex_init(&save_gui_mutex); dx_cond_init(&save_gui_condition); save_gui_ready=true;
        FILE *bad=fopen(save_gui_bad,"wb"); SAVE_GUI_CHECK(bad); SAVE_GUI_CHECK(fputs("not a workbook",bad)>=0 && !fclose(bad));
        char *error=NULL; SAVE_GUI_CHECK(cells_set(&active_sheet(a)->cells,9000,0,"Offscreen",&error) && !error); worksheet_record(active_sheet(a),EDIT_CELLS);
        save_gui_edit(a,"Original"); break;
    }
    case 1:
        if (busy) return; SAVE_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Original")); workspace_job(a,JOB_SAVE,save_gui_path); break;
    case 2:
        if (busy) return; SAVE_GUI_CHECK(!a->book.dirty && save_gui_value("Original")); save_gui_edit(a,"Changed"); break;
    case 3:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty); save_gui_gate(SAVE_PHASE_WRITE); workspace_job(a,JOB_SAVE,save_gui_path); break;
    case 4:
        if (!save_gui_waiting()) return;
        SAVE_GUI_CHECK(a->io_busy && state.cancellable && a->save_panel.width==360*a->ui.scale && a->save_panel.height==136*a->ui.scale);
        if (!save_gui_pan) {
            a->ui.native_pan_valid=true;
            a->ui.native_pan=(DxPanInput){.y=120,.pointer_x=a->grid.body.x+20,.pointer_y=a->grid.body.y+20,.phase=DX_PAN_ACTIVE,.event=true};
            save_gui_pan=true; return;
        }
        SAVE_GUI_CHECK(a->grid.top>0);
        smoke_snapshot("build/tests/results/workbook-save/progress.png"); ui_note_key_press(KEY_ESCAPE,0); break;
    case 5:
        SAVE_GUI_CHECK(state.cancel_requested && a->io_busy); save_gui_unblock(); break;
    case 6:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty && a->modal==MODAL_NONE && a->journal && save_gui_value("Original"));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); save_gui_gate(SAVE_PHASE_WRITE); workspace_job(a,JOB_SAVE,save_gui_path); break;
    case 7:
        if (!save_gui_waiting()) return;
        SAVE_GUI_CHECK(a->save_panel.width==540 && a->save_panel.height==204 && a->save_panel.x>=0 && a->save_panel.x+a->save_panel.width<=640);
        smoke_snapshot("build/tests/results/workbook-save/progress-150.png");
        ui_note_pointer_gesture((DxPointerGesture){a->save_cancel.x+20,a->save_cancel.y+12,a->save_cancel.x+20,a->save_cancel.y+12,0,false}); break;
    case 8:
        SAVE_GUI_CHECK(state.cancel_requested); save_gui_unblock(); break;
    case 9:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty && save_gui_value("Original"));
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); save_gui_gate(SAVE_PHASE_PUBLISH); a->document_action=DOCUMENT_QUIT; save_document_action(a); break;
    case 10: {
        if (!save_gui_waiting()) return;
        SAVE_GUI_CHECK(state.progress.phase==SAVE_PHASE_PUBLISH && !state.cancellable && !worker_save_cancel(&a->worker));
        CellEditTarget target; char *error=NULL; SAVE_GUI_CHECK(workbook_prepare_cell_edit(&a->book,0,1,1,&target,&error));
        SAVE_GUI_CHECK(workbook_journal_session_apply(a->journal,&a->book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Newer",5},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error);
        worker_cancel(&a->worker); smoke_snapshot("build/tests/results/workbook-save/publishing.png"); save_gui_unblock(); break;
    }
    case 11:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty && !a->exit_ready && a->modal==MODAL_UNSAVED && a->journal && save_gui_value("Changed"));
        SAVE_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Newer")); ui_note_key_press(KEY_ESCAPE,0); break;
    case 12:
        if (busy) return; SAVE_GUI_CHECK(a->document_action==DOCUMENT_NONE); save_gui_gate(SAVE_PHASE_COMPLETE); workspace_job(a,JOB_SAVE,save_gui_path); break;
    case 13:
        if (!save_gui_waiting()) return; SAVE_GUI_CHECK(state.progress.completed==state.progress.total && !state.cancellable && !worker_save_cancel(&a->worker)); save_gui_unblock(); break;
    case 14:
        if (busy) return; SAVE_GUI_CHECK(!a->book.dirty && !a->journal && save_gui_value("Newer")); save_gui_edit(a,"Unsaved failure"); break;
    case 15:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty); workspace_job(a,JOB_SAVE,save_gui_bad); break;
    case 16:
        if (busy) return; SAVE_GUI_CHECK(a->book.dirty && a->modal==MODAL_ERROR && a->journal && save_gui_value("Newer"));
        SAVE_GUI_CHECK(a->book.path && !strcmp(dx_basename(a->book.path),dx_basename(save_gui_path))); ui_note_key_press(KEY_ESCAPE,0); break;
    case 17:
        if (busy) return; SAVE_GUI_CHECK(a->modal==MODAL_NONE && !a->exit_ready);
        a->versions.identity_valid=false; workspace_save_as(a,save_gui_path); SAVE_GUI_CHECK(a->io_busy); break;
    case 18:
        if (busy) return; SAVE_GUI_CHECK(!a->book.dirty && a->versions.identity_valid && save_gui_value("Unsaved failure")); break;
    case 19:
        if (busy) return; SAVE_GUI_CHECK(a->modal==MODAL_NONE && !a->exit_ready);
        a->document_action=DOCUMENT_NEW; a->discard_journal=true; finish_document_action(a); SAVE_GUI_CHECK(!a->journal && !a->book.dirty && !a->versions.store && !a->versions.owner.session);
        workspace_job(a,JOB_LOAD,save_gui_path); break;
    case 20:
        if (busy) return;
        SAVE_GUI_CHECK(!a->book.dirty && a->versions.store && a->versions.identity_valid && a->versions.owner.session);
        SAVE_GUI_CHECK(workbook_version_core_set_index(a->versions.store)->root_count==4 && !strcmp(cells_get(&active_sheet(a)->cells,1,1),"Unsaved failure"));
        SAVE_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,9000,0),"Offscreen")); break;
    case 21:
        a->document_action=DOCUMENT_NEW; finish_document_action(a);
        SAVE_GUI_CHECK(!a->versions.store && !a->versions.owner.session && !a->book.dirty);
        save_gui_result=1; break;
    default: save_gui_result=-1; return;
    }
    save_gui_stage++;
}
int app_test_workbook_save_result(void) { return save_gui_result; }
void app_test_workbook_save_cleanup(void) { if (save_gui_ready) save_gui_unblock(); }
void app_test_workbook_save_finished(void) {
    if (save_gui_ready) { dx_cond_destroy(&save_gui_condition); dx_mutex_destroy(&save_gui_mutex); }
    if (save_gui_result==1) printf("PASS: native controlled Save workflow (22 stages, %u checks)\n",save_gui_checks);
}
