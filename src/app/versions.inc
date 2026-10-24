/* Native adapter: presentation stays in the shared C widget; App owns label,
   destination, document readiness and the ordered Save snapshot/publication. */
static const WorkbookVersionIndex *versions_index(App *a) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(a->versions.store);
    return index && index->document_high==a->book.id.high && index->document_low==a->book.id.low ? index : NULL;
}
static AppPreviewContext history_context(App *a) { return (AppPreviewContext){&a->book,&a->versions,&a->worker}; }
static void history_cancel(App *a) {
    if (!a->history.pending.request) return;
    worker_cancel(&a->worker); a->request_id++; app_preview_cancel(&a->history);
    a->busy=a->io_busy=false; a->consume_grid_input=true;
    snprintf(a->version_message,sizeof(a->version_message),"Historical read cancelled. Working Head is unchanged.");
}
static bool history_return(App *a) {
    history_cancel(a); if (!a->history.active) return true;
    char *error=NULL; AppPreviewOutcome outcome=app_preview_return(&a->history,history_context(a),&error);
    if (outcome!=APP_PREVIEW_APPLIED) { status(a,error ? error : "The Working Head owner changed; return was refused."); free(error); return false; }
    free(error); ui_history_free(&a->history_view); a->modal=MODAL_NONE; a->consume_grid_input=true;
    status(a,"Returned to Working Head."); return true;
}
static bool history_surface_pending(App *a) {
    return a->pending_sheet || a->pending_new_sheet || a->pending_relation || a->sheet_action!=SHEET_ACTION_NONE ||
        a->grid.dragging || a->grid.moving || a->grid.resizing>=0 || a->grid.scrollbar;
}
static bool history_ready(App *a) {
    if (a->busy || a->io_busy || a->values_loading || a->query_loading || a->count_deferred || SOURCE_FORMAT_PENDING(a) || history_surface_pending(a)) return false;
    dx_lock(&a->worker.mutex); bool idle=!a->worker.running && !a->worker.pending && !a->worker.reply; dx_unlock(&a->worker.mutex); return idle;
}
static void history_open(App *a) {
    const WorkbookVersionIndex *index=versions_index(a); size_t selected=a->version_view.selected;
    if (!index || !history_ready(a)) { snprintf(a->version_message,sizeof(a->version_message),"Wait for the current workbook operation before opening a version."); return; }
    WorkbookVersionKey root;
    if (selected==SIZE_MAX && index->saved.present) { root=index->saved.key; snprintf(a->history_pending_label,sizeof(a->history_pending_label),"Latest Saved"); }
    else if (selected<index->tag_count) {
        const WorkbookVersionTag *tag=index->tags+selected; root=tag->root;
        memcpy(a->history_pending_label,tag->label.data,tag->label.bytes); a->history_pending_label[tag->label.bytes]=0;
    } else return;
    capture_sheet(a); if (!historical(a)) workbook_history_sync(&a->book);
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=a->request_id+1; job->kind=JOB_LOAD; job->path=dx_strdup("Retained historical version"); char *error=NULL;
    if (!app_preview_job(&a->history,history_context(a),job,root,&error)) {
        snprintf(a->version_message,sizeof(a->version_message),"%s",error ? error : "Historical read could not start."); free(error); worker_job_free(job); return;
    }
    a->request_id=job->id; a->pending_kind=JOB_LOAD; a->busy=a->io_busy=true;
    snprintf(a->version_message,sizeof(a->version_message),"Opening retained version… Escape cancels."); app_worker_submit(a,job);
}
static void history_reply(App *a,QueryReply *reply) {
    char *error=NULL; AppPreviewOutcome outcome=app_preview_adopt(&a->history,history_context(a),reply,&error); a->io_busy=false;
    if (outcome==APP_PREVIEW_APPLIED) {
        ui_history_begin(&a->history_view,&a->book);
        snprintf(a->history_label,sizeof(a->history_label),"%s",a->history_pending_label); a->history_message[0]=0;
        a->modal=MODAL_NONE; a->consume_grid_input=true;
    } else snprintf(a->version_message,sizeof(a->version_message),"%s",error ? error : "An earlier historical read was ignored. Working Head is unchanged.");
    free(error);
}
static void versions_open(App *a) {
    if (a->io_busy || a->document_action!=DOCUMENT_NONE) return;
    if (!historical(a) && history_surface_pending(a)) { status(a,"Finish the current worksheet interaction before inspecting versions."); return; }
    if (!historical(a)) { end_step_preview(a); cancel_values(a); cancel_source_validation(a);
        SOURCE_FORMAT_CANCEL(a,"Formatting cancelled by version inspection."); }
    a->version_view=(UIVersionView){.selected=SIZE_MAX}; a->version_message[0]=0;
    input_set(&a->input,""); a->modal=MODAL_VERSIONS; a->consume_grid_input=true;
    while (GetCharPressed()) { }
}
#if defined(DUXCEL_FEATURE_VERSION_CONTROLS) || defined(DUXCEL_FEATURE_HISTORY_PREVIEW)
extern char *dx_test_version_destination(void);
#endif
static void versions_create(App *a) {
    if (historical(a) || a->history.pending.request) return;
    WorkbookVersionIndex empty={.document_high=a->book.id.high,.document_low=a->book.id.low};
    const WorkbookVersionIndex *index=versions_index(a); if (!index) index=&empty;
    WorkbookVersionText label={a->input.text,a->input.length};
    WorkbookVersionStatus status_code=workbook_version_tag_label_check(index,label);
    if (status_code) { snprintf(a->version_message,sizeof(a->version_message),"%s",workbook_version_message(status_code)); return; }
    char *path=NULL;
    if (a->book.path) path=dx_strdup(a->book.path);
#if defined(DUXCEL_FEATURE_VERSION_CONTROLS) || defined(DUXCEL_FEATURE_HISTORY_PREVIEW)
    else path=dx_test_version_destination();
#else
    else path=dx_save_file_panel(NULL);
#endif
    if (!path) { snprintf(a->version_message,sizeof(a->version_message),"Choose a destination to save the first version."); return; }
    workspace_job_context(a,JOB_SAVE,path,&label,false); free(path);
    if (!a->io_busy) snprintf(a->version_message,sizeof(a->version_message),"%s",a->message ? a->message : "The version could not start.");
}
static void versions_draw(App *a) {
    DrawRectangleRec((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},(Color){12,20,30,56});
    UIVersionAction action=ui_version_draw(&a->ui,&a->version_view,&a->input,
        (UIVersionContext){versions_index(a),a->version_message,!historical(a) && !a->io_busy && a->book.count>0,history_ready(a)},!a->consume_grid_input);
    if (action==UI_VERSION_CLOSE) { history_cancel(a); a->modal=MODAL_NONE; a->consume_grid_input=true; }
    else if (action==UI_VERSION_CREATE) versions_create(a);
    else if (action==UI_VERSION_OPEN) history_open(a);
}
