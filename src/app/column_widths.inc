/* Native and shared-C presentation command adapter. SheetView owns committed
   widths; Grid changes are a draft until this complete owner admits them. */
typedef struct { App *app; size_t captured; } WidthAdmission;
static bool column_widths_ready(const App *a) {
    return !a->restore_view && !(a->analysis.kind!=SOURCE_NONE && a->new_schema);
}
static bool admit_column_width(void *context,size_t staging,size_t retained_add,char **error) {
    WidthAdmission *admission=context;
#ifndef DUXCEL_BROWSER_HOST
    if (staging>SIZE_MAX-admission->captured || source_format_budget_status(admission->app,NULL,
        staging+admission->captured,0,0,0,retained_add)) {
        if (error) *error=dx_strdup(admission->app->message ? admission->app->message : "Column width exceeds the current memory budget.");
        return false;
    }
#else
    (void)staging; (void)retained_add; (void)error; (void)admission;
#endif
    return true;
}
static void project_sheet_widths(App *a) {
    Worksheet *sheet=active_sheet(a);
    if (sheet) grid_project_widths(&a->grid,sheet->view.widths,sheet->view.column_count);
}
static bool commit_grid_widths(App *a) {
    Worksheet *sheet=active_sheet(a); Grid *g=&a->grid;
    if (!sheet) return false;
    /* During activation the cache can still belong to the outgoing sheet;
       during first source load its default widths are not initialized yet.
       Neither representation is an authored draft of the current owner. */
    if (!column_widths_ready(a)) return false;
    bool changed=false;
    size_t count=g->width_count>sheet->view.column_count ? g->width_count : sheet->view.column_count;
    for (size_t c=0;c<count;c++) {
        float before=c<sheet->view.column_count ? sheet->view.widths[c] : 96;
        float after=c<g->width_count ? g->widths[c] : 96;
        if (before!=after) { changed=true; break; }
    }
    if (!changed) return true;
    if (historical(a)) return true; /* Inspection-only projection; no copyback. */
    if (a->io_busy || history_locked(a) || a->step_preview.active) { project_sheet_widths(a); return false; }
    size_t captured=DX_COLUMN_WIDTH_COLUMNS*sizeof(ColumnWidthEntry);
#ifndef DUXCEL_BROWSER_HOST
    if (source_format_budget_status(a,NULL,0,captured,0,0,0)) { project_sheet_widths(a); return false; }
#endif
    ColumnWidthState after={0}; char *error=NULL;
    ColumnWidthStatus result=column_width_capture(g->widths,g->width_count,captured,&after);
    WidthAdmission admission={a,captured};
    WorkbookWidthContext context={.book=&a->book,.request_epoch=&a->request_id,
        .reserved=DX_COLUMN_WIDTH_HISTORY_BYTES,.ready=true,.read_only=false,
        .admit=admit_column_width,.admission_context=&admission};
    WorkbookWidthTarget target;
    if (result==COLUMN_WIDTH_OK && workbook_prepare_width_command(&context,a->book.active,&target,&error))
        result=workbook_apply_width_command_recorded(&context,&target,column_width_view(&after),NULL,&error).status;
    else if (result==COLUMN_WIDTH_OK) result=COLUMN_WIDTH_INVALID;
    column_width_free(&after); project_sheet_widths(a);
    if (error) { status(a,error); free(error); }
    return result==COLUMN_WIDTH_OK || result==COLUMN_WIDTH_NOOP;
}
