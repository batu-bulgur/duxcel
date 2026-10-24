/* Private primary relation/relink/recovery fixture; absent from production. */
static unsigned primary_replace_stage,primary_replace_wait,primary_replace_checks;
static int primary_replace_result;
static uint64_t primary_replace_revision,primary_replace_request,primary_replace_jobs;
static Cell *primary_replace_unrelated;
static const char *primary_replace_path="build/tests/generated/primary-replacement.dxls";
#define PRIMARY_REPLACE_CHECK(x) do { primary_replace_checks++; if (!(x)) { fprintf(stderr,"FAIL primary replacement GUI stage %u:%d: %s (%s)\n",primary_replace_stage,__LINE__,#x,a->message ? a->message : ""); primary_replace_result=-1; return; } } while (0)
static size_t primary_replace_relation(App *a,const char *name) { size_t i=0; while (i<a->page.relation_count && strcmp(a->page.relations[i].name,name)) i++; return i; }
static void primary_replace_choose(App *a,const char *name) { a->pending_relation=true; a->pending_relation_index=primary_replace_relation(a,name); }
static bool primary_replace_ready(App *a) { if (!object_ready(a,0)) { a->selected_object=0; return false; } return true; }
static double primary_replace_sum(App *a) { Page *p=&a->objects[0].page; double n=0; for (size_t r=0;r<p->row_count;r++) n+=p->cells[r*p->column_count+1].number; return n; }
static void primary_replacement_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (primary_replace_result || !s || a->busy || a->io_busy || a->query_loading || ++primary_replace_wait<24) return;
    primary_replace_wait=0;
    if (primary_replace_stage!=10 && a->page_stale) return;
    PRIMARY_REPLACE_CHECK((primary_replace_stage==10 || a->modal!=MODAL_ERROR) && !a->objects[0].failed && !a->objects[1].failed);
    switch (primary_replace_stage) {
    case 0: {
        remove(primary_replace_path); remove("build/tests/generated/primary-missing.duckdb"); PRIMARY_REPLACE_CHECK(a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha"));
        grid_select_cell(&a->grid,0,4,false); create_pivot(a); PivotDefinition p={0};
        PRIMARY_REPLACE_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 1:
        if (!primary_replace_ready(a)) return;
        a->selected_object=-1; grid_select_cell(&a->grid,40,4,false); PRIMARY_REPLACE_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 2:
        if (!primary_replace_ready(a) || !object_ready(a,1)) return;
        primary_replace_unrelated=a->objects[1].page.cells; primary_replace_revision=a->book.command_revision;
        PRIMARY_REPLACE_CHECK(fabs(primary_replace_sum(a)-60)<.001 && primary_replace_relation(a,"beta")<a->page.relation_count); primary_replace_choose(a,"beta"); break;
    case 3:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(a->page.total==2 && !strcmp(a->analysis.table,"beta") && fabs(primary_replace_sum(a)-300)<.001 && a->book.command_revision==primary_replace_revision+1 && a->objects[1].page.cells==primary_replace_unrelated);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(a->page.total==3 && !strcmp(a->analysis.table,"alpha") && fabs(primary_replace_sum(a)-60)<.001 && s->redo_actions_count==1 && a->book.command_revision==primary_replace_revision+2);
        primary_replace_request=a->request_id; primary_replace_jobs=a->worker.jobs_started; primary_replace_choose(a,"alpha"); break;
    case 5:
        PRIMARY_REPLACE_CHECK(s->redo_actions_count==1 && a->request_id==primary_replace_request && a->worker.jobs_started==primary_replace_jobs && a->book.command_revision==primary_replace_revision+2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 6:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(fabs(primary_replace_sum(a)-300)<.001 && a->book.command_revision==primary_replace_revision+3);
        PRIMARY_REPLACE_CHECK(relink_primary_path(a,"build/tests/generated/source-connection-b.duckdb")); break;
    case 7:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(fabs(primary_replace_sum(a)-3000)<.001 && a->book.command_revision==primary_replace_revision+4 && a->objects[1].page.cells==primary_replace_unrelated); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(fabs(primary_replace_sum(a)-300)<.001 && a->book.command_revision==primary_replace_revision+5 && s->redo_actions_count==1);
        { uint64_t request=a->request_id; PRIMARY_REPLACE_CHECK(relink_primary_path(a,"build/tests/generated/source-connection-a.duckdb") && a->request_id==request && s->redo_actions_count==1); }
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9: {
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(fabs(primary_replace_sum(a)-3000)<.001 && a->book.command_revision==primary_replace_revision+6);
        /* A valid saved definition can point at a file that has disappeared.
           Exercise an actual worker failure before the normal relink adapter. */
        Analysis missing=a->analysis; missing.path="build/tests/generated/primary-missing.duckdb"; PRIMARY_REPLACE_CHECK(replace_primary_connection(a,&missing)); break;
    }
    case 10:
        PRIMARY_REPLACE_CHECK(a->fetch_failed && a->book.command_revision==primary_replace_revision+7);
        PRIMARY_REPLACE_CHECK(!set_unique_rows(a,-1,true) && !a->analysis.unique_rows && a->book.command_revision==primary_replace_revision+7);
        PRIMARY_REPLACE_CHECK(relink_primary_path(a,"build/tests/generated/source-connection-a.duckdb")); break;
    case 11: {
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(!a->fetch_failed && !a->page_stale && fabs(primary_replace_sum(a)-300)<.001 && a->book.command_revision==primary_replace_revision+8 && a->objects[1].page.cells==primary_replace_unrelated);
        Analysis limited=a->analysis; limited.query="SELECT City, Amount FROM input"; limited.filter_column="Amount"; limited.filter_value="150"; limited.filter_op=FILTER_GREATER;
        limited.filter_number=(NumberType){NUM_DECIMAL,12,2}; limited.sort_column="Amount"; limited.descending=true; limited.unique_rows=true;
        PRIMARY_REPLACE_CHECK(replace_primary_connection(a,&limited)); break;
    }
    case 12:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(a->page.total==1 && fabs(primary_replace_sum(a)-200)<.001 && a->analysis.query && a->analysis.filter_column && a->book.command_revision==primary_replace_revision+9);
        PRIMARY_REPLACE_CHECK(primary_replace_relation(a,"beta_view")<a->page.relation_count); primary_replace_choose(a,"beta_view"); break;
    case 13:
        if (!primary_replace_ready(a)) return;
        PRIMARY_REPLACE_CHECK(a->page.total==2 && fabs(primary_replace_sum(a)-300)<.001 && !strcmp(a->analysis.table,"beta_view") && !a->analysis.query && !a->analysis.filter_column && !a->analysis.group_column && !a->analysis.sort_column && !a->analysis.unique_rows && a->book.command_revision==primary_replace_revision+10 && a->objects[1].page.cells==primary_replace_unrelated);
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 14:
        workspace_job(a,JOB_SAVE,primary_replace_path); break;
    case 15:
        PRIMARY_REPLACE_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,primary_replace_path); break;
    case 16:
        for (unsigned i=0;i<2;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        PRIMARY_REPLACE_CHECK(a->page.total==2 && !strcmp(a->analysis.table,"beta_view") && !a->analysis.query && fabs(primary_replace_sum(a)-300)<.001 && a->objects[1].page.total==8 && a->book.command_revision==primary_replace_revision+10 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); primary_replace_result=1;
        printf("PASS: native primary replacement workflow (17 stages, %u checks)\n",primary_replace_checks); break;
    }
    primary_replace_stage++;
}
int app_test_primary_replacement_result(void) { return primary_replace_result; }
unsigned app_test_primary_replacement_stage(void) { return primary_replace_stage; }
#undef PRIMARY_REPLACE_CHECK
