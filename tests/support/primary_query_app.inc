/* Private primary group/reset/query fixture; no production hook or state. */
static unsigned primary_query_stage,primary_query_wait,primary_query_checks;
static int primary_query_result;
static uint64_t primary_query_revision,primary_query_request;
static const char *primary_query_sql="SELECT City, Amount, Value FROM input WHERE Amount > 20";
static const char *primary_query_path="build/tests/generated/primary-query.dxls";
#define QUERY_GUI_CHECK(x) do { primary_query_checks++; if (!(x)) { fprintf(stderr,"FAIL primary query GUI stage %u:%d: %s (%s)\n",primary_query_stage,__LINE__,#x,a->query_error ? a->query_error : a->message ? a->message : ""); primary_query_result=-1; return; } } while (0)
static void primary_query_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (primary_query_stage==14 && !a->busy && !a->query_loading && a->query_error) QUERY_GUI_CHECK(!a->query_error);
    if (primary_query_result || !s || a->busy || a->io_busy || a->query_loading || a->page_stale || ++primary_query_wait<24) return;
    primary_query_wait=0; QUERY_GUI_CHECK(!a->fetch_failed && a->modal!=MODAL_ERROR);
    switch (primary_query_stage) {
    case 0:
        remove(primary_query_path); QUERY_GUI_CHECK(a->page.total==8 && !a->analysis.group_column && !a->analysis.query);
        primary_query_revision=a->book.command_revision; a->field=0; a->measure=1; a->aggregate=AGG_SUM; apply_group(a); break;
    case 1: {
        QUERY_GUI_CHECK(a->page.total==3 && a->page.column_count==2 && a->analysis.group_column && a->analysis.aggregate==AGG_SUM && a->book.command_revision==primary_query_revision+1);
        double sum=0; for (size_t i=0;i<a->page.row_count;i++) sum+=a->page.cells[i*2+1].number;
        QUERY_GUI_CHECK(sum==101); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 2:
        QUERY_GUI_CHECK(a->page.total==8 && !a->analysis.group_column && s->redo_actions_count==1);
        primary_query_request=a->request_id; reset_analysis(a);
        QUERY_GUI_CHECK(a->request_id==primary_query_request && s->redo_actions_count==1 && a->book.command_revision==primary_query_revision+2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 3:
        QUERY_GUI_CHECK(a->page.total==3 && a->analysis.group_column); reset_analysis(a); break;
    case 4:
        QUERY_GUI_CHECK(a->page.total==8 && !a->analysis.group_column && a->book.command_revision==primary_query_revision+4);
        begin_source_query(a); input_set(&a->input,"SELECT Missing FROM input"); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 5:
        QUERY_GUI_CHECK(a->modal==MODAL_SOURCE_QUERY && a->query_error && !a->analysis.query && a->page.total==8 && a->book.command_revision==primary_query_revision+4 && !strcmp(a->input.text,"SELECT Missing FROM input"));
        input_set(&a->input,primary_query_sql); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 6:
        QUERY_GUI_CHECK(a->modal==MODAL_NONE && a->analysis.query && !strcmp(a->analysis.query,primary_query_sql) && a->page.total==2 && !strcmp(a->page.cells[0].text,"Ankara") && a->book.command_revision==primary_query_revision+5);
        primary_query_request=a->request_id; begin_source_query(a); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 7:
        QUERY_GUI_CHECK(a->modal==MODAL_NONE && a->request_id==primary_query_request && a->book.command_revision==primary_query_revision+5);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        QUERY_GUI_CHECK(a->page.total==8 && !a->analysis.query && s->redo_actions_count==1);
        primary_query_request=a->request_id; reset_analysis(a);
        QUERY_GUI_CHECK(a->request_id==primary_query_request && s->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9:
        QUERY_GUI_CHECK(a->page.total==2 && a->analysis.query && a->book.command_revision==primary_query_revision+7);
        set_source_filter(a,"City",FILTER_EQUAL,"Ankara",false); break;
    case 10:
        QUERY_GUI_CHECK(a->page.total==2 && a->analysis.filter_column && a->book.command_revision==primary_query_revision+8);
        clear_primary_filters(a); break;
    case 11:
        QUERY_GUI_CHECK(a->page.total==2 && a->analysis.query && !a->analysis.filter_column && a->book.command_revision==primary_query_revision+9);
        primary_query_request=a->request_id; reset_analysis(a); clear_primary_filters(a);
        QUERY_GUI_CHECK(a->request_id==primary_query_request && a->book.command_revision==primary_query_revision+9 && !strcmp(a->analysis.query,primary_query_sql));
        workspace_job(a,JOB_SAVE,primary_query_path); break;
    case 12:
        QUERY_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,primary_query_path); break;
    case 13:
        QUERY_GUI_CHECK(a->page.total==2 && a->analysis.query && !strcmp(a->analysis.query,primary_query_sql) && !a->analysis.group_column && !a->analysis.filter_column &&
            a->book.command_revision==primary_query_revision+9 && !s->undo_actions_count && !a->book.dirty);
        /* A failed/stale prior Page is presentation state, not authority to
           reject a replacement relation the worker has successfully validated. */
        a->fetch_failed=true; a->page_stale=true;
        QUERY_GUI_CHECK(!set_unique_rows(a,-1,true) && !a->analysis.unique_rows && a->book.command_revision==primary_query_revision+9);
        QueryReply stale={.kind=JOB_VALIDATE_SQL,.id=a->request_id+1,.revision=a->analysis_revision}; stale.analysis=analysis_copy(&a->analysis);
        bool changed=true; char *error=NULL;
        bool accepted=primary_transform_adopt(a,&stale.analysis,0,&stale,&changed,&error);
        QUERY_GUI_CHECK(!accepted && changed && error && stale.analysis.query && a->book.command_revision==primary_query_revision+9);
        free(error); analysis_free(&stale.analysis); begin_source_query(a);
        input_set(&a->input,"SELECT City, Amount, Value FROM input WHERE Amount < 0"); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 14:
        QUERY_GUI_CHECK(a->modal==MODAL_NONE && !a->fetch_failed && !a->page_stale && a->analysis.query && strstr(a->analysis.query,"Amount < 0") &&
            a->page.total==2 && !strcmp(a->page.cells[0].text,"Bursa") && a->book.command_revision==primary_query_revision+10 && s->undo_actions_count==1 && a->book.dirty);
        primary_query_result=1; printf("PASS: native primary group/query workflow (15 stages, %u checks)\n",primary_query_checks); break;
    }
    primary_query_stage++;
}
int app_test_primary_query_result(void) { return primary_query_result; }
#undef QUERY_GUI_CHECK
