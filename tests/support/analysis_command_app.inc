/* Private real-App fixture; no test hook/state in the production App object. */
static unsigned analysis_command_stage,analysis_command_wait,analysis_command_checks;
static int analysis_command_result;
static uint64_t analysis_command_revision,analysis_command_request;
static size_t analysis_command_undo;
static AnalysisTransformTarget analysis_command_old_target;
static const char *analysis_command_path="build/tests/generated/analysis-command.dxls";
#define ANALYSIS_GUI_CHECK(x) do { analysis_command_checks++; if (!(x)) { fprintf(stderr,"FAIL analysis command GUI stage %u:%d: %s (%s)\n",analysis_command_stage,__LINE__,#x,a->message ? a->message : ""); analysis_command_result=-1; return; } } while (0)
static void analysis_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (analysis_command_result || !s || a->busy || a->io_busy || a->page_stale || ++analysis_command_wait<24) return;
    analysis_command_wait=0;
    ANALYSIS_GUI_CHECK(!a->fetch_failed && a->modal!=MODAL_ERROR);
    switch (analysis_command_stage) {
    case 0: {
        remove(analysis_command_path);
        ANALYSIS_GUI_CHECK(a->analysis.kind==SOURCE_CSV && a->page.total==8 && !a->analysis.unique_rows);
        AnalysisCommandContext context={&a->book,&a->analysis,&a->analysis_revision,SIZE_MAX,true,false};
        ANALYSIS_GUI_CHECK(workbook_prepare_analysis_transform(&context,a->book.active,&analysis_command_old_target,NULL));
        analysis_command_revision=a->book.command_revision; analysis_command_undo=s->undo_actions_count;
        ANALYSIS_GUI_CHECK(set_unique_rows(a,-1,true)); break;
    }
    case 1:
        ANALYSIS_GUI_CHECK(a->page.total==4 && a->analysis.unique_rows && a->book.command_revision==analysis_command_revision+1 && s->undo_actions_count==analysis_command_undo+1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 2: {
        ANALYSIS_GUI_CHECK(a->page.total==8 && !a->analysis.unique_rows && s->redo_actions_count==1);
        Analysis p=analysis_copy(&a->analysis); p.unique_rows=true; char *error=NULL;
        AnalysisCommandContext context={&a->book,&a->analysis,&a->analysis_revision,SIZE_MAX,true,false};
        AnalysisTransformResult r=workbook_apply_analysis_transform_recorded(&context,&analysis_command_old_target,&p,NULL,&error);
        ANALYSIS_GUI_CHECK(r.status==ANALYSIS_TRANSFORM_STALE && error && !a->analysis.unique_rows && p.unique_rows && s->redo_actions_count==1);
        free(error); analysis_free(&p);
        analysis_command_request=a->request_id;
        ANALYSIS_GUI_CHECK(set_unique_rows(a,-1,false) && a->request_id==analysis_command_request && s->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 3:
        ANALYSIS_GUI_CHECK(a->page.total==4 && a->analysis.unique_rows && !s->redo_actions_count);
        a->pipeline=true; a->pipeline_collapsed=false; a->chrome_focus=8; a->pipeline_focus_index=1;
        ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        ANALYSIS_GUI_CHECK(a->page.total==8 && !a->analysis.unique_rows && a->book.command_revision==analysis_command_revision+4);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        ANALYSIS_GUI_CHECK(a->page.total==4 && a->analysis.unique_rows && a->book.command_revision==analysis_command_revision+5);
        ui_set_scale(&a->ui,1.5f); SetWindowSize(860,640); break;
    case 6:
        /* Let resized view/layout publication settle before capturing Save R. */
        workspace_job(a,JOB_SAVE,analysis_command_path); break;
    case 7:
        ANALYSIS_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,analysis_command_path); break;
    case 8:
        ANALYSIS_GUI_CHECK(a->page.total==4 && a->analysis.unique_rows && s->analysis.unique_rows && a->book.command_revision==analysis_command_revision+5 && !s->undo_actions_count && !a->book.dirty);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        ANALYSIS_GUI_CHECK(a->page.total==4 && a->analysis.unique_rows && !a->book.dirty && !s->undo_actions_count);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); analysis_command_result=1;
        printf("PASS: native primary transform workflow (10 stages, %u checks)\n",analysis_command_checks); break;
    }
    analysis_command_stage++;
}
int app_test_analysis_command_result(void) { return analysis_command_result; }
#undef ANALYSIS_GUI_CHECK
