/* Private real-App Table/dependent/unrelated cache fixture. */
static unsigned table_command_stage,table_command_wait,table_command_checks;
static int table_command_result;
static uint64_t table_command_revision,table_command_request,table_command_epoch,table_command_jobs;
static Cell *table_command_other,*table_command_rows,*table_command_pivot;
static const char *table_command_path="build/tests/generated/table-command.dxls";
#define TABLE_GUI_CHECK(x) do { table_command_checks++; if (!(x)) { fprintf(stderr,"FAIL Table command GUI stage %u:%d: %s (%s)\n",table_command_stage,__LINE__,#x,a->objects[0].error ? a->objects[0].error : a->query_error ? a->query_error : a->message ? a->message : ""); table_command_result=-1; return; } } while (0)
static void table_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (table_command_result || !s || a->busy || a->io_busy || a->query_loading || ++table_command_wait<24) return;
    table_command_wait=0;
    TABLE_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (table_command_stage) {
    case 0:
        remove(table_command_path); grid_select_cell(&a->grid,2,1,false); TABLE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==8); a->selected_object=0; create_pivot(a);
        PivotDefinition pivot={0}; char *error=NULL;
        TABLE_GUI_CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error));
        set_pivot_definition(a,&pivot); free(error); break;
    }
    case 2:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->objects[1].page.total==3 && store->items[1].source_id==store->items[0].id);
        a->selected_object=-1; grid_select_cell(&a->grid,25,1,false); TABLE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        table_command_other=a->objects[2].page.cells; table_command_revision=a->book.command_revision;
        TABLE_GUI_CHECK(a->objects[2].page.total==8 && set_unique_rows(a,0,true)); break;
    case 4: {
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==4 && a->objects[1].page.total==3 && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+1);
        uint64_t request=a->request_id; size_t count=s->undo_actions_count;
        TABLE_GUI_CHECK(set_unique_rows(a,0,true) && request==a->request_id && count==s->undo_actions_count);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 5:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==8 && a->objects[2].page.cells==table_command_other && !store->items[0].source.unique_rows && s->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 6:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==4 && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+3);
        a->selected_object=0; begin_source_query(a); input_set(&a->input,"SELECT City, Amount, Value FROM input WHERE Amount > 20"); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 7: {
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->modal==MODAL_NONE && a->objects[0].page.total==1 && a->objects[1].page.total==1 && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+4);
        Analysis filters=analysis_copy(&store->items[0].source);
        TABLE_GUI_CHECK(analysis_filter_set(&filters,"City",FILTER_EQUAL,"Ankara",NULL) && analysis_filter_set(&filters,"Amount",FILTER_GREATER,"20",NULL) && set_table_source(a,0,&filters));
        analysis_free(&filters); break;
    }
    case 8:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==1 && a->objects[1].page.total==1 && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+5);
        table_command_rows=a->objects[0].page.cells; table_command_pivot=a->objects[1].page.cells; table_command_request=a->request_id;
        table_command_epoch=store->items[0].revision; table_command_jobs=a->worker.jobs_started;
        a->selected_object=0; a->pipeline=true; a->pipeline_collapsed=false; a->chrome_focus=8; a->pipeline_focus_index=3; ui_note_key_press(KEY_UP,DX_MOD_ALT); break;
    case 9:
        TABLE_GUI_CHECK(!strcmp(store->items[0].source.filter_column,"Amount") && store->items[0].revision==table_command_epoch && a->book.command_revision==table_command_revision+6);
        TABLE_GUI_CHECK(a->objects[0].page.cells==table_command_rows && a->objects[1].page.cells==table_command_pivot && a->objects[2].page.cells==table_command_other && a->request_id==table_command_request && a->worker.jobs_started==table_command_jobs);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(!strcmp(store->items[0].source.filter_column,"City") && a->objects[0].page.cells==table_command_rows && a->objects[1].page.cells==table_command_pivot && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+7);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 11:
        if (!object_ready(a,0) || !object_ready(a,1)) return;
        TABLE_GUI_CHECK(!strcmp(store->items[0].source.filter_column,"Amount") && a->objects[0].page.cells==table_command_rows && a->objects[1].page.cells==table_command_pivot && a->objects[2].page.cells==table_command_other && a->book.command_revision==table_command_revision+8);
        workspace_job(a,JOB_SAVE,table_command_path); break;
    case 12:
        TABLE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,table_command_path); break;
    case 13:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        TABLE_GUI_CHECK(a->objects[0].page.total==1 && a->objects[1].page.total==1 && a->objects[2].page.total==8 && store->items[0].source.query && store->items[0].source.unique_rows &&
            !strcmp(store->items[0].source.filter_column,"Amount") && store->items[1].source_id==store->items[0].id && !s->undo_actions_count && !a->book.dirty && a->book.command_revision==table_command_revision+8);
        table_command_result=1; printf("PASS: native Table transform workflow (14 stages, %u checks)\n",table_command_checks); break;
    }
    table_command_stage++;
}
int app_test_table_command_result(void) { return table_command_result; }
#undef TABLE_GUI_CHECK
