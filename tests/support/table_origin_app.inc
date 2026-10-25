/* Current loaded defaults exercise actual Worker replies through the same App
   dispatch as viewport, bound analytical, count and available-value results. */
static unsigned table_origin_stage,table_origin_wait,table_origin_checks;
static int table_origin_result;
static DefinitionOrigin *table_origin_pending;
static const Cell *table_origin_primary;
static unsigned char *table_origin_frozen;
static size_t table_origin_bytes;
static uint64_t table_origin_command,table_origin_branch,table_origin_jobs;
static size_t table_origin_undo,table_origin_redo;
static const char *table_origin_saved="build/tests/generated/table-origin-pending.dxls";
static const char *table_origin_closed="build/tests/generated/table-origin-closed.dxls";
#define TABLE_ORIGIN_CHECK(x) do { table_origin_checks++; if (!(x)) { fprintf(stderr,"FAIL Table origin stage %u:%d: %s (%s)\n",table_origin_stage,__LINE__,#x,a->message ? a->message : ""); table_origin_result=-1; return; } } while (0)
static void table_origin_job(App *a,size_t index) {
    SheetObject *object=&active_sheet(a)->objects.items[index];
    QueryJob *job=dx_alloc(sizeof(*job)); job->kind=JOB_PIVOT; job->id=++a->request_id;
    job->analysis=analysis_copy(object_source(a,object)); job->pivot=pivot_copy(&object->pivot);
    job->object_id=object->id; job->revision=object->revision; job->is_table=object->kind==OBJECT_TABLE;
    job->include_totals=!job->is_table; job->known_total=-1;
    a->busy=true; a->pending_kind=JOB_PIVOT; a->pending_object_id=object->id; app_worker_submit(a,job);
}
static bool table_origin_clocks(const App *a,const Worksheet *s) {
    return a->book.command_revision==table_origin_command && a->book.history_branch==table_origin_branch &&
        s->undo_actions_count==table_origin_undo && s->redo_actions_count==table_origin_redo;
}
static bool table_origin_immutable(App *a) {
    DefinitionOriginAssets assets=definition_origin_assets(table_origin_pending);
    return assets.primary && !memcmp(table_origin_frozen,assets.primary,table_origin_bytes);
}
static void table_origin_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (table_origin_result || !s || a->busy || a->io_busy || a->values_loading || ++table_origin_wait<24) return;
    table_origin_wait=0; TABLE_ORIGIN_CHECK(a->modal!=MODAL_ERROR);
    switch (table_origin_stage) {
    case 0: {
        if (a->page_stale || !a->page.column_count) return;
        remove(table_origin_saved); remove(table_origin_closed); a->statistics_enabled=false;
        TABLE_ORIGIN_CHECK(a->page.total==8 && s->objects.count==3 && !s->objects.items[0].source.table && !s->objects.items[1].source.table);
        table_origin_primary=a->page.cells; table_origin_command=a->book.command_revision; table_origin_branch=a->book.history_branch;
        table_origin_undo=s->undo_actions_count; table_origin_redo=s->redo_actions_count;
        AppDefinitionOrigin *node=NULL; TABLE_ORIGIN_CHECK(app_definition_origin_open(a,0,&node,NULL)==DEFINITION_ORIGIN_READY && node);
        table_origin_pending=node->owner; a->source_format.bytes=SIZE_MAX;
        /* The selected owner stays offscreen; submit the actual bound route
           before its source Table viewport is cached. */
        a->selected_object=-1; table_origin_job(a,2); break;
    }
    case 1: {
        TABLE_ORIGIN_CHECK(a->objects[2].page.total==14 && s->objects.items[2].rows==16 && !s->objects.items[0].source.table);
        TABLE_ORIGIN_CHECK(s->objects.items[0].source.filter_number.kind==NUM_DECIMAL && definition_origin_state(table_origin_pending).phase==DEFINITION_ORIGIN_OPEN && !definition_origin_assets(table_origin_pending).primary);
        TABLE_ORIGIN_CHECK(a->page.cells==table_origin_primary && table_origin_clocks(a,s)); a->source_format.bytes=0;
        QueryReply wrong={.kind=JOB_PIVOT,.id=a->request_id,.object_id=s->objects.items[2].id,.revision=s->objects.items[2].revision,.analysis=analysis_copy(&s->objects.items[0].source)};
        free(wrong.analysis.schema); wrong.analysis.schema=dx_strdup("different"); wrong.analysis.table=dx_strdup("alpha"); char *error=NULL;
        TABLE_ORIGIN_CHECK(app_definition_origin_reply_owner(a,&wrong,1,&error)==DEFINITION_ORIGIN_STALE && error && !s->objects.items[0].source.table); free(error); error=NULL;
        free(wrong.analysis.schema); wrong.analysis.schema=dx_strdup("main"); wrong.revision++;
        TABLE_ORIGIN_CHECK(app_definition_origin_reply_owner(a,&wrong,1,&error)==DEFINITION_ORIGIN_STALE && error); free(error); error=NULL; wrong.revision--;
        free(wrong.analysis.path); wrong.analysis.path=dx_strdup("/different.duckdb");
        TABLE_ORIGIN_CHECK(app_definition_origin_reply_owner(a,&wrong,1,&error)==DEFINITION_ORIGIN_STALE && error && table_origin_clocks(a,s)); free(error); analysis_free(&wrong.analysis);
        workspace_job(a,JOB_SAVE,table_origin_saved); TABLE_ORIGIN_CHECK(a->io_busy); break;
    }
    case 2: {
        size_t scratch=0;
        TABLE_ORIGIN_CHECK(!a->book.dirty && app_definition_origin_find(a)->owner==table_origin_pending && definition_origin_scratch(&(DefinitionOriginContext){&a->book,&a->analysis,&a->request_id,&a->analysis_revision,0,true,false},table_origin_pending,&scratch,NULL)==DEFINITION_ORIGIN_READY);
        TABLE_ORIGIN_CHECK(table_origin_clocks(a,s) && a->page.cells==table_origin_primary);
        table_origin_job(a,2); break;
    }
    case 3:
        TABLE_ORIGIN_CHECK(!strcmp(s->objects.items[0].source.table,"alpha") && !s->objects.items[1].source.table && definition_origin_state(table_origin_pending).phase==DEFINITION_ORIGIN_OPEN);
        TABLE_ORIGIN_CHECK(table_origin_clocks(a,s) && a->page.cells==table_origin_primary);
        a->objects[1].count_paused=true; table_origin_job(a,1); break;
    case 4: {
        TABLE_ORIGIN_CHECK(s->objects.items[1].source.table && !strcmp(s->objects.items[1].source.table,"alpha") && s->objects.items[1].source.filter_number.kind==NUM_DECIMAL && s->objects.items[1].source.filter_number.precision==12);
        TABLE_ORIGIN_CHECK(a->objects[1].page.count_pending && a->objects[1].waiting_count && definition_origin_state(table_origin_pending).phase==DEFINITION_ORIGIN_ADMITTED && table_origin_clocks(a,s));
        table_origin_bytes=definition_origin_state(table_origin_pending).asset_bytes; table_origin_frozen=dx_alloc(table_origin_bytes); memcpy(table_origin_frozen,definition_origin_assets(table_origin_pending).primary,table_origin_bytes);
        a->objects[1].count_paused=false; break;
    }
    case 5:
        if (!a->objects[1].count_resolved) return;
        TABLE_ORIGIN_CHECK(a->objects[1].failed && a->objects[1].counted_total==903 && a->objects[1].page.count_pending && *cells_get(&s->cells,10800,16));
        TABLE_ORIGIN_CHECK(table_origin_immutable(a) && a->page.cells==table_origin_primary && table_origin_clocks(a,s));
        table_origin_jobs=a->worker.jobs_started;
        a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; grid_select_cell(&a->grid,10800,16,false); open_modal(a,MODAL_CELL); input_set(&a->input,""); TABLE_ORIGIN_CHECK(commit_cell(a)); grid_select_cell(&a->grid,0,0,false); break;
    case 6:
        if (a->objects[1].page.count_pending || a->objects[1].failed) return;
        TABLE_ORIGIN_CHECK(a->objects[1].page.total==903 && s->objects.items[1].rows==905 && !*cells_get(&s->cells,10800,16));
        TABLE_ORIGIN_CHECK(a->book.command_revision==table_origin_command+1 && a->worker.jobs_started==table_origin_jobs && table_origin_immutable(a));
        /* Replay-like provisional footprint restoration uses the real cached
           result reconciliation; this is derived state, not a user action. */
        s->objects.items[1].rows=5; s->objects.items[1].columns=4; reconcile_object_views(a);
        TABLE_ORIGIN_CHECK(s->objects.items[1].rows==905 && !a->objects[1].failed && table_origin_immutable(a) && a->page.cells==table_origin_primary);
        versions_open(a); a->version_view.selected=SIZE_MAX; history_open(a); TABLE_ORIGIN_CHECK(a->history.pending.request); break;
    case 7: {
        TABLE_ORIGIN_CHECK(historical(a) && !s->objects.items[0].source.table && !s->objects.items[1].source.table);
        QueryReply readonly={.kind=JOB_PIVOT,.id=a->request_id,.object_id=3,.revision=1,.analysis=analysis_copy(&s->objects.items[0].source)};
        readonly.analysis.table=dx_strdup("alpha");
        TABLE_ORIGIN_CHECK(app_definition_origin_reply_owner(a,&readonly,1,NULL)==DEFINITION_ORIGIN_READY && !s->objects.items[0].source.table && table_origin_immutable(a));
        analysis_free(&readonly.analysis); TABLE_ORIGIN_CHECK(history_return(a)); break;
    }
    case 8:
        TABLE_ORIGIN_CHECK(!historical(a) && s->objects.items[1].rows==905 && table_origin_immutable(a) && a->page.cells==table_origin_primary);
        a->filter_object=1; a->field=0; a->date_level=-1; a->values_total=-1; input_set(&a->input,""); request_values(a,0); TABLE_ORIGIN_CHECK(a->values_loading); break;
    case 9:
        TABLE_ORIGIN_CHECK(!a->values_failed && a->values_total==2 && table_origin_immutable(a));
        workspace_job(a,JOB_SAVE,table_origin_closed); TABLE_ORIGIN_CHECK(a->io_busy); break;
    case 10:
        TABLE_ORIGIN_CHECK(!a->book.dirty && table_origin_immutable(a)); free(table_origin_frozen); table_origin_frozen=NULL;
        workspace_job(a,JOB_LOAD,table_origin_closed); TABLE_ORIGIN_CHECK(a->io_busy); break;
    case 11:
        if (a->page_stale || !a->page.column_count) return;
        TABLE_ORIGIN_CHECK(s->objects.count==3 && !strcmp(s->objects.items[0].source.table,"alpha") && !strcmp(s->objects.items[1].source.table,"alpha") && s->objects.items[1].rows==905 && !*cells_get(&s->cells,10800,16) && a->book.command_revision==table_origin_command+1 && !a->book.dirty && a->page.total==8);
        table_origin_result=1; printf("PASS: native loaded Table origin workflow (12 stages, %u checks)\n",table_origin_checks); break;
    }
    table_origin_stage++;
}
int app_test_table_origin_result(void) { return table_origin_result; }
unsigned app_test_table_origin_stage(void) { return table_origin_stage; }
#undef TABLE_ORIGIN_CHECK
