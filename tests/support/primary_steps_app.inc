/* Private primary Steps input/cache fixture; absent from production builds. */
static unsigned primary_steps_stage,primary_steps_wait,primary_steps_checks;
static int primary_steps_result;
static uint64_t primary_steps_revision,primary_steps_request,primary_steps_epoch,primary_steps_jobs,primary_steps_sql;
static Cell *primary_steps_cells;
static const char *primary_steps_path="build/tests/generated/primary-steps.dxls";
#define STEPS_GUI_CHECK(x) do { primary_steps_checks++; if (!(x)) { fprintf(stderr,"FAIL primary steps GUI stage %u:%d: %s (%s)\n",primary_steps_stage,__LINE__,#x,a->message ? a->message : ""); primary_steps_result=-1; return; } } while (0)
static void primary_steps_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (primary_steps_result || !s || a->busy || a->io_busy || a->page_stale || ++primary_steps_wait<24) return;
    primary_steps_wait=0; STEPS_GUI_CHECK(!a->fetch_failed && a->modal!=MODAL_ERROR);
    switch (primary_steps_stage) {
    case 0:
        remove(primary_steps_path); STEPS_GUI_CHECK(a->page.total==8 && !a->analysis.filter_column);
        primary_steps_revision=a->book.command_revision; set_source_filter(a,"City",FILTER_EQUAL,"Ankara",false); break;
    case 1:
        STEPS_GUI_CHECK(a->page.total==2); set_source_filter(a,"Amount",FILTER_GREATER,"20",false); break;
    case 2:
        STEPS_GUI_CHECK(a->page.total==2 && a->analysis.filter_count==1 && !strcmp(a->analysis.filter_column,"City") && a->book.command_revision==primary_steps_revision+2);
        primary_steps_cells=a->page.cells; primary_steps_request=a->request_id; primary_steps_epoch=a->analysis_revision;
        primary_steps_jobs=a->worker.jobs_started; primary_steps_sql=a->worker.sql_executed;
        a->pipeline=true; a->pipeline_collapsed=false; a->chrome_focus=8; a->pipeline_focus_index=2; ui_note_key_press(KEY_UP,DX_MOD_ALT); break;
    case 3:
        STEPS_GUI_CHECK(!strcmp(a->analysis.filter_column,"Amount") && !strcmp(a->analysis.filters[0].column,"City") && a->pipeline_focus_index==1 && a->book.command_revision==primary_steps_revision+3);
        STEPS_GUI_CHECK(a->page.cells==primary_steps_cells && a->request_id==primary_steps_request && a->analysis_revision==primary_steps_epoch && a->worker.jobs_started==primary_steps_jobs && a->worker.sql_executed==primary_steps_sql);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        STEPS_GUI_CHECK(!strcmp(a->analysis.filter_column,"City") && a->book.command_revision==primary_steps_revision+4 && s->redo_actions_count==1);
        STEPS_GUI_CHECK(a->page.cells==primary_steps_cells && a->request_id==primary_steps_request && a->analysis_revision==primary_steps_epoch);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 5:
        STEPS_GUI_CHECK(!strcmp(a->analysis.filter_column,"Amount") && a->page.cells==primary_steps_cells && a->request_id==primary_steps_request && a->analysis_revision==primary_steps_epoch && a->book.command_revision==primary_steps_revision+5);
        a->chrome_focus=8; a->pipeline_focus_index=1; ui_note_key_press(KEY_DELETE,0); break;
    case 6:
        STEPS_GUI_CHECK(a->page.total==2 && !strcmp(a->analysis.filter_column,"City") && !a->analysis.filter_count && a->book.command_revision==primary_steps_revision+6 && a->analysis_revision>primary_steps_epoch);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7:
        STEPS_GUI_CHECK(a->page.total==2 && !strcmp(a->analysis.filter_column,"Amount") && a->analysis.filter_count==1 && a->book.command_revision==primary_steps_revision+7);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        STEPS_GUI_CHECK(a->page.total==2 && !strcmp(a->analysis.filter_column,"City") && !a->analysis.filter_count && a->book.command_revision==primary_steps_revision+8);
        workspace_job(a,JOB_SAVE,primary_steps_path); break;
    case 9:
        STEPS_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,primary_steps_path); break;
    case 10:
        STEPS_GUI_CHECK(a->page.total==2 && a->analysis.filter_column && !strcmp(a->analysis.filter_column,"City") && !a->analysis.filter_count && a->book.command_revision==primary_steps_revision+8 && !a->book.dirty && !s->undo_actions_count);
        primary_steps_result=1; printf("PASS: native primary step workflow (11 stages, %u checks)\n",primary_steps_checks); break;
    }
    primary_steps_stage++;
}
int app_test_primary_steps_result(void) { return primary_steps_result; }
#undef STEPS_GUI_CHECK
