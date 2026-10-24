/* Native adapter: presentation stays in the shared C widget; App owns label,
   destination, document readiness and the ordered Save snapshot/publication. */
static const WorkbookVersionIndex *versions_index(App *a) {
    const WorkbookVersionIndex *index=workbook_version_core_set_index(a->versions.store);
    return index && index->document_high==a->book.id.high && index->document_low==a->book.id.low ? index : NULL;
}
static void versions_open(App *a) {
    if (a->io_busy || a->document_action!=DOCUMENT_NONE) return;
    end_step_preview(a); cancel_values(a); cancel_source_validation(a);
    SOURCE_FORMAT_CANCEL(a,"Formatting cancelled by version inspection.");
    a->version_view=(UIVersionView){0}; a->version_message[0]=0;
    input_set(&a->input,""); a->modal=MODAL_VERSIONS; a->consume_grid_input=true;
    while (GetCharPressed()) { }
}
#ifdef DUXCEL_FEATURE_VERSION_CONTROLS
extern char *dx_test_version_destination(void);
#endif
static void versions_create(App *a) {
    WorkbookVersionIndex empty={.document_high=a->book.id.high,.document_low=a->book.id.low};
    const WorkbookVersionIndex *index=versions_index(a); if (!index) index=&empty;
    WorkbookVersionText label={a->input.text,a->input.length};
    WorkbookVersionStatus status_code=workbook_version_tag_label_check(index,label);
    if (status_code) { snprintf(a->version_message,sizeof(a->version_message),"%s",workbook_version_message(status_code)); return; }
    char *path=NULL;
    if (a->book.path) path=dx_strdup(a->book.path);
#ifdef DUXCEL_FEATURE_VERSION_CONTROLS
    else path=dx_test_version_destination();
#else
    else path=dx_save_file_panel(NULL);
#endif
    if (!path) { snprintf(a->version_message,sizeof(a->version_message),"Choose a destination to save the first version."); return; }
    workspace_job_context(a,JOB_SAVE,path,&label); free(path);
    if (!a->io_busy) snprintf(a->version_message,sizeof(a->version_message),"%s",a->message ? a->message : "The version could not start.");
}
static void versions_draw(App *a) {
    DrawRectangleRec((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},(Color){12,20,30,56});
    UIVersionAction action=ui_version_draw(&a->ui,&a->version_view,&a->input,
        (UIVersionContext){versions_index(a),a->version_message,!a->io_busy && a->book.count>0},!a->consume_grid_input);
    if (action==UI_VERSION_CLOSE) { a->modal=MODAL_NONE; a->consume_grid_input=true; }
    else if (action==UI_VERSION_CREATE) versions_create(a);
}
