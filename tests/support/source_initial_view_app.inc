/* The same typed schema must not carry an outgoing Grid allocation/width draft
   into the new source's authoritative creation state. Actual Worker + v16 IO. */
static unsigned initial_view_stage,initial_view_wait,initial_view_checks;
static int initial_view_result;
static uint64_t initial_view_revision,initial_view_order,initial_view_high,initial_view_old_sheet,initial_view_new_sheet;
static size_t initial_view_count,initial_view_capacity,initial_view_old_columns;
static const Cell *initial_view_old_cells;
static const char *initial_view_saved="build/tests/generated/source-initial-view.dxls";
#define INITIAL_VIEW_CHECK(x) do { initial_view_checks++; if (!(x)) { fprintf(stderr,"FAIL source initial view stage %u:%d: %s (%s)\n",initial_view_stage,__LINE__,#x,a->message ? a->message : ""); initial_view_result=-1; return; } } while (0)
static bool initial_view_current(const App *a) {
    const Worksheet *s=a->book.sheets+a->book.active;
    return a->page.column_count==40 && a->page.total==3 && s->view.column_count==56 && a->grid.width_count==56 &&
        s->view.widths[0]==190 && s->view.widths[39]==128 && s->view.widths[55]==96 && a->grid.widths[0]==190 && a->grid.widths[55]==96 &&
        s->view.canvas_zoom==1.5f && a->grid.zoom==1.5f && !s->view.col && !a->grid.col && !s->view.horizontal && !a->grid.horizontal;
}
static void source_initial_view_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (initial_view_result || !s || a->busy || a->io_busy || ++initial_view_wait<24) return;
    initial_view_wait=0; INITIAL_VIEW_CHECK(a->modal!=MODAL_ERROR);
    switch (initial_view_stage) {
    case 0:
        remove(initial_view_saved);
        INITIAL_VIEW_CHECK(a->page.column_count==40 && a->page.total==3 && a->page.columns[0].type==DUCKDB_TYPE_TIMESTAMP);
        grid_ensure_columns(&a->grid,9001); float prior_width=a->grid.widths[0]; a->grid.widths[0]=333.5f;
        INITIAL_VIEW_CHECK(commit_grid_widths(a));
        /* Reproject the committed model after the synthetic drag proposal. */
        a->grid.widths[0]=prior_width; project_sheet_widths(a);
        initial_view_capacity=a->grid.width_count; initial_view_old_columns=s->view.column_count;
        a->grid.zoom=1.5f; grid_select_cell(&a->grid,0,9000,false); a->grid.horizontal=a->grid.offsets[8998];
        initial_view_revision=a->book.command_revision; initial_view_order=worksheet_history_high_water(); initial_view_high=a->book.sheet_high_water;
        initial_view_count=a->book.count; initial_view_old_sheet=s->id; initial_view_old_cells=a->page.cells;
        open_source(a,"build/tests/generated/source-open-wide.parquet",false);
        INITIAL_VIEW_CHECK(a->primary_source_draft && a->book.count==initial_view_count && a->book.command_revision==initial_view_revision && a->page.cells==initial_view_old_cells);
        INITIAL_VIEW_CHECK(a->grid.width_count==initial_view_capacity && initial_view_capacity>=9001 && a->grid.widths[0]==333.5f && a->primary_source_navigation.canvas_zoom==1.5f);
        break;
    case 1:
        INITIAL_VIEW_CHECK(!a->primary_source_draft && initial_view_current(a));
        INITIAL_VIEW_CHECK(a->book.count==initial_view_count+1 && a->book.sheet_high_water==initial_view_high+1 && s->id!=initial_view_old_sheet);
        INITIAL_VIEW_CHECK(a->book.command_revision==initial_view_revision+1 && worksheet_history_high_water()==initial_view_order+1);
        INITIAL_VIEW_CHECK(!s->width_history && !s->width_revision && !s->undo_actions_count && !s->redo_actions_count);
        INITIAL_VIEW_CHECK(a->book.sheets[0].id==initial_view_old_sheet && a->book.sheets[0].view.widths[0]==333.5f && a->book.sheets[0].view.col==9000 && a->book.sheets[0].view.column_count==initial_view_old_columns);
        initial_view_new_sheet=s->id; break;
    case 2:
        INITIAL_VIEW_CHECK(initial_view_current(a) && a->book.command_revision==initial_view_revision+1 && worksheet_history_high_water()==initial_view_order+1);
        workspace_job(a,JOB_SAVE,initial_view_saved); INITIAL_VIEW_CHECK(a->io_busy); break;
    case 3:
        INITIAL_VIEW_CHECK(!a->book.dirty && initial_view_current(a) && a->book.command_revision==initial_view_revision+1);
        workspace_job(a,JOB_LOAD,initial_view_saved); INITIAL_VIEW_CHECK(a->io_busy); break;
    case 4:
        INITIAL_VIEW_CHECK(initial_view_current(a) && s->id==initial_view_new_sheet && a->book.sheets[0].id==initial_view_old_sheet);
        INITIAL_VIEW_CHECK(a->book.sheets[0].view.widths[0]==333.5f && a->book.sheets[0].view.col==9000 && a->book.sheets[0].view.column_count==initial_view_old_columns);
        INITIAL_VIEW_CHECK(s->view.widths[0]==190 && !strcmp(s->view.columns[0],"Moment") && !strcmp(s->view.columns[39],"Field39") && !*s->view.columns[55]);
        initial_view_result=1; printf("PASS: native initial source view workflow (5 stages, %u checks)\n",initial_view_checks); return;
    }
    initial_view_stage++;
}
int app_test_source_initial_view_result(void) { return initial_view_result; }
unsigned app_test_source_initial_view_stage(void) { return initial_view_stage; }
