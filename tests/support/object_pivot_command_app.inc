/* Private field/filter/date/cache fixture; excluded from production App. */
static unsigned pivot_command_stage,pivot_command_wait,pivot_command_checks;
static int pivot_command_result;
static uint64_t pivot_command_revision,pivot_command_query;
static Cell *pivot_command_unrelated[2];
static const char *pivot_command_path="build/tests/generated/object-pivot-command.dxls";
#define PIVOT_GUI_CHECK(x) do { pivot_command_checks++; if (!(x)) { fprintf(stderr,"FAIL Pivot GUI stage %u:%d: %s (%s)\n",pivot_command_stage,__LINE__,#x,a->message ? a->message : ""); pivot_command_result=-1; return; } } while (0)
static bool pivot_command_caches(const App *a) { return a->objects[0].page.cells==pivot_command_unrelated[0] && a->objects[1].page.cells==pivot_command_unrelated[1]; }
static void object_pivot_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (pivot_command_result || !s || a->busy || a->io_busy || a->values_loading || ++pivot_command_wait<24) return;
    pivot_command_wait=0; PIVOT_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (pivot_command_stage) {
    case 0:
        remove(pivot_command_path); grid_select_cell(&a->grid,2,1,false); PIVOT_GUI_CHECK(import_table(a,"tests/fixtures/pivot-command.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition p={0};
        PIVOT_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_COUNT,NULL));
        PIVOT_GUI_CHECK(set_pivot_definition(a,&p)); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        pivot_command_unrelated[0]=a->objects[0].page.cells; pivot_command_unrelated[1]=a->objects[1].page.cells;
        a->selected_object=2; PIVOT_GUI_CHECK(set_chart_measure(a,1)); pivot_command_revision=a->book.command_revision; pivot_command_query=store->items[2].revision;
        a->pivot_focus=4; a->pivot_value=1; a->chrome_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_LEFT,DX_MOD_ALT); break;
    case 4:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_COUNT && !store->items[2].chart_measure && store->items[2].revision==pivot_command_query+1 && a->book.command_revision==pivot_command_revision+1 && pivot_command_caches(a));
        a->pivot_focus=4; a->pivot_value=0; ui_note_key_press(KEY_FOUR,0); break;
    case 5:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_AVG && !store->items[2].chart_measure && a->book.command_revision==pivot_command_revision+2 && pivot_command_caches(a));
        a->pivot_focus=4; a->pivot_value=1; ui_note_key_press(KEY_DELETE,0); break;
    case 6:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.counts[PIVOT_VALUES]==1 && a->book.command_revision==pivot_command_revision+3 && pivot_command_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7: {
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.counts[PIVOT_VALUES]==2 && s->redo_actions_count==1 && a->book.command_revision==pivot_command_revision+4 && pivot_command_caches(a));
        Cell *page=a->objects[2].page.cells; uint64_t request=a->request_id,jobs=a->worker.jobs_started;
        PivotDefinition same=pivot_copy(&store->items[2].pivot); PIVOT_GUI_CHECK(set_pivot_definition(a,&same));
        PIVOT_GUI_CHECK(a->objects[2].page.cells==page && a->request_id==request && a->worker.jobs_started==jobs && s->redo_actions_count==1 && a->book.command_revision==pivot_command_revision+4);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 8:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.counts[PIVOT_VALUES]==1 && a->book.command_revision==pivot_command_revision+5 && pivot_command_caches(a)); open_view_filter_field(a,2); break;
    case 9:
        PIVOT_GUI_CHECK(a->modal==MODAL_FILTER && a->filter_object==2);
        a->filter_condition_enabled=true; a->filter_op=FILTER_GREATER; a->values_all=true; input_set(&a->filter_term,"40"); a->filter_focus=11; ui_note_key_press(KEY_ENTER,0); break;
    case 10:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].pivot.counts[PIVOT_FILTERS]==1 && store->items[2].pivot.fields[PIVOT_FILTERS][0].filter==FILTER_GREATER && !strcmp(store->items[2].pivot.fields[PIVOT_FILTERS][0].value,"40") && a->book.command_revision==pivot_command_revision+6 && pivot_command_caches(a));
        a->pivot_focus=2; a->pivot_value=0; ui_note_key_press(KEY_DELETE,0); break;
    case 11:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(!store->items[2].pivot.counts[PIVOT_ROWS] && a->objects[2].page.total==1 && a->book.command_revision==pivot_command_revision+7 && pivot_command_caches(a));
        a->pivot_drag_field=0; a->pivot_drag_role=-1; assign_pivot_field(a,PIVOT_ROWS,0); break;
    case 12:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_MONTH && a->objects[2].page.total==2 && a->book.command_revision==pivot_command_revision+8 && pivot_command_caches(a));
        a->pivot_drag_role=PIVOT_ROWS; a->pivot_drag_index=0; a->pivot_drag_field=-1; pivot_menu_open(a,(Vector2){200,200}); break;
    case 13:
        PIVOT_GUI_CHECK(a->modal==MODAL_PIVOT_FIELD); a->context_focus=3; ui_note_key_press(KEY_ENTER,0); break;
    case 14:
        if (!object_ready(a,2)) return;
        PIVOT_GUI_CHECK(store->items[2].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_YEAR && a->objects[2].page.total==1 && a->objects[2].page.column_count==2 &&
            !a->objects[2].page.cells[1].is_null && fabs(a->objects[2].page.cells[1].number-65)<.0001 && a->book.command_revision==pivot_command_revision+9 && pivot_command_caches(a));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 15:
        PIVOT_GUI_CHECK(pivot_command_caches(a)); workspace_job(a,JOB_SAVE,pivot_command_path); break;
    case 16:
        PIVOT_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,pivot_command_path); break;
    case 17:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        PIVOT_GUI_CHECK(store->items[2].kind==OBJECT_CHART && !store->items[2].chart_measure && store->items[2].pivot.fields[PIVOT_ROWS][0].grain==PIVOT_DATE_YEAR &&
            store->items[2].pivot.fields[PIVOT_VALUES][0].aggregate==AGG_AVG && store->items[2].pivot.counts[PIVOT_FILTERS]==1 && !strcmp(store->items[2].pivot.fields[PIVOT_FILTERS][0].value,"40") &&
            store->items[2].source_id==store->items[0].id && a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==1 &&
            fabs(a->objects[2].page.cells[1].number-65)<.0001 && a->book.command_revision==pivot_command_revision+9 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); pivot_command_result=1;
        printf("PASS: native object Pivot workflow (18 stages, %u checks)\n",pivot_command_checks); break;
    }
    pivot_command_stage++;
}
int app_test_object_pivot_command_result(void) { return pivot_command_result; }
#undef PIVOT_GUI_CHECK
