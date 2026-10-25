/* Native App adapter: this owns presentation/request epochs and the current
   recovered parent. Reader controllers never receive an App or live model. */
static bool recovery_reader(App *a) {
    if (a->recovery_worker) return true;
    char *error=NULL;
    if (a->recovery_root) a->recovery_worker=workbook_recovery_worker_new(a->recovery_root,&error);
    if (!a->recovery_worker) snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",error ? error : "The recovery folder is unavailable.");
    free(error); return a->recovery_worker!=NULL;
}
static void recovery_scan(App *a,bool startup) {
    a->recovery_startup=startup;
    if (!recovery_reader(a)) return;
    WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(a->recovery_worker);
    if (state.active || state.ready) { a->recovery_refresh=true; a->recovery_request=0; workbook_recovery_worker_cancel(a->recovery_worker); return; }
    char *error=NULL;
    if (!workbook_recovery_worker_page(a->recovery_worker,true,&a->recovery_request,&error)) snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",error ? error : "Recovery scan could not start.");
    else snprintf(a->recovery_message,sizeof(a->recovery_message),"Looking for inactive sessions…");
    free(error);
}
static void recovery_open(App *a) {
    if (a->io_busy || a->document_action!=DOCUMENT_NONE) return;
    if (a->modal==MODAL_CELL && !commit_cell(a)) return;
    SOURCE_FORMAT_CANCEL(a,"Formatting cancelled by recovery inspection.");
    end_step_preview(a); cancel_values(a); cancel_source_validation(a);
    a->modal=MODAL_RECOVERY; a->recovery_view=(UIRecoveryView){0}; a->consume_grid_input=true; recovery_scan(a,false);
}
static void recovery_adopt(App *a,WorkbookRecoveryCandidate *candidate,bool discard) {
    if (!candidate) return;
    char *error=NULL;
    if (!app_versions_recovered(&a->versions,&a->worker,workbook_recovery_book(candidate),workbook_recovery_versions(candidate),&error)) {
        status(a,error); free(error); a->modal=MODAL_ERROR; recovery_candidate_free(a,candidate); return;
    }
    Workbook *book=workbook_recovery_take_book(candidate);
    if (!book) { recovery_candidate_free(a,candidate); return; }
    SOURCE_FORMAT_RESET(a); end_step_preview(a); cancel_values(a); statistics_cancel(a);
    close_journal(a,discard); recovery_resolve(a,discard);
    DEFINITION_ORIGINS_RETIRE(a);
    workbook_free(&a->book); a->book=*book; free(book); a->recovery_owner=candidate;
    a->modal=MODAL_NONE; a->key_tips=a->file_menu=a->ribbon_focus=false;
    if (!a->app_presentation_loaded) { ui_set_scale(&a->ui,a->book.user_scale); a->reduced_motion=a->book.reduced_motion; }
    activate_sheet(a,a->book.active,false);
    const WorkbookRecoveryInfo *info=workbook_recovery_info(candidate);
    status(a,info->tail_deferred ? "Checkpoint copy opened. Recorded changes are kept separately; use Save As for this copy." : "Recorded changes recovered. Use Save As to keep this workbook.");
#ifdef DUXCEL_DEV_REVIEW
    if (a->review) a->review->document_serial++;
#endif
}
static void recovery_poll(App *a) {
    WorkbookRecoveryReply *r=workbook_recovery_worker_poll(a->recovery_worker);
    if (r) {
        bool current=a->recovery_request && r->id==a->recovery_request;
        a->recovery_request=0;
        if (current && r->kind==RECOVERY_WORK_PAGE) {
            recovery_reply_free(a,a->recovery_page); a->recovery_page=r; r=NULL;
            a->recovery_view=(UIRecoveryView){0};
            WorkbookRecoveryReply *page=a->recovery_page;
            snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",page->error[0] ? page->error : page->page.scan.done ? "Inactive sessions. Active Duxcel instances are excluded." : "More sessions available on the next page.");
            if (a->recovery_startup) {
                a->recovery_startup=false;
                if (page->page.count && a->modal==MODAL_NONE && !a->io_busy && !a->book.dirty && a->book.id.high==a->recovery_start_document.high && a->book.id.low==a->recovery_start_document.low && a->document_action==DOCUMENT_NONE) a->modal=MODAL_RECOVERY;
            }
        } else if (current && r->kind==RECOVERY_WORK_PREPARE && a->modal==MODAL_RECOVERY) {
            if (r->prepare_result==RECOVERY_PREPARED) {
                a->recovery_staged=workbook_recovery_reply_take_candidate(r);
                a->modal=MODAL_NONE; request_document_action(a,DOCUMENT_RECOVER,NULL);
            } else snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",r->error[0] ? r->error : "Recovery could not complete. All files were kept.");
        }
        recovery_reply_free(a,r);
    }
    if (a->recovery_refresh) {
        WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(a->recovery_worker);
        if (!state.active && !state.ready) { bool startup=a->recovery_startup; a->recovery_refresh=false; recovery_scan(a,startup); }
    }
}
static void recovery_draw(App *a) {
    UIRecoveryRow rows[UI_RECOVERY_ROWS]={0}; char identities[UI_RECOVERY_ROWS][144];
    size_t count=a->recovery_page ? a->recovery_page->page.count : 0;
    for (size_t i=0;i<count;i++) {
        const WorkbookRecoveryEntry *e=a->recovery_page->page.rows+i;
        const char *title=e->original_path && *e->original_path ? dx_basename(e->original_path) : "Unsaved workbook";
        bool checkpoint=e->kind==RECOVERY_ENTRY_BOUND || e->kind==RECOVERY_ENTRY_UNBOUND || e->kind==RECOVERY_ENTRY_CHECKPOINT_ONLY;
        snprintf(identities[i],sizeof(identities[i]),"%s · revisions %" PRIu64 " → %" PRIu64,e->generation_name,e->baseline_revision,e->declared_head_revision);
        rows[i]=(UIRecoveryRow){title,e->detail,identities[i],e->kind==RECOVERY_ENTRY_BOUND,checkpoint};
    }
    WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(a->recovery_worker);
    char progress[512]; const char *message=a->recovery_message;
    if (state.active && state.kind==RECOVERY_WORK_PREPARE) {
        const char *phases[]={"Claiming session","Verifying checkpoint","Loading checkpoint","Replaying changes","Ready"};
        unsigned phase=(unsigned)state.progress.phase;
        snprintf(progress,sizeof(progress),"%s · %" PRIu64 " / %" PRIu64,phase<5 ? phases[phase] : "Preparing",state.progress.completed,state.progress.total); message=progress;
    }
    bool more=a->recovery_page && a->recovery_page->scan_result==RECOVERY_SCAN_MORE;
    UIRecoveryAction action=ui_recovery_draw(&a->ui,&a->recovery_view,(UIRecoveryContext){rows,count,state.active || state.ready || a->recovery_refresh,more,message},!a->consume_grid_input);
    char *error=NULL;
    if (action==UI_RECOVERY_LATER) { recovery_dismiss(a); status(a,"Recovery files were kept. Use File → Recover session to return."); }
    else if (action==UI_RECOVERY_REFRESH) recovery_scan(a,false);
    else if (action==UI_RECOVERY_NEXT) {
        if (!workbook_recovery_worker_page(a->recovery_worker,false,&a->recovery_request,&error)) snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",error ? error : "The next page is unavailable.");
    } else if ((action==UI_RECOVERY_REPLAY || action==UI_RECOVERY_CHECKPOINT) && a->recovery_page && a->recovery_view.selected<count) {
        WorkbookRecoveryEntry *entry=a->recovery_page->page.rows+a->recovery_view.selected;
        if (!source_format_budget(a,NULL,0,DX_WORKBOOK_FORMAT_CURRENT_BYTES,0,0,0)) { snprintf(a->recovery_message,sizeof(a->recovery_message),"Recovery needs additional format memory. Close the current workbook first; all files are kept."); return; }
        if (!workbook_recovery_worker_prepare(a->recovery_worker,entry->generation_name,action==UI_RECOVERY_REPLAY ? RECOVERY_REPLAY_TAIL : RECOVERY_OPEN_CHECKPOINT,&a->recovery_request,&error)) snprintf(a->recovery_message,sizeof(a->recovery_message),"%s",error ? error : "Recovery could not start.");
    }
    free(error);
}
