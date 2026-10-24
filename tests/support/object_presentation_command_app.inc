/* Private presentation/cache/input fixture; excluded from production App. */
static unsigned presentation_stage,presentation_wait,presentation_checks;
static int presentation_result;
static uint64_t presentation_revision,presentation_request,presentation_jobs,presentation_titles;
static Cell *presentation_pages[3];
static const char *presentation_path="build/tests/generated/object-presentation-command.dxls";
#define PRESENT_GUI_CHECK(x) do { presentation_checks++; if (!(x)) { fprintf(stderr,"FAIL presentation GUI stage %u:%d: %s (%s)\n",presentation_stage,__LINE__,#x,a->message ? a->message : ""); presentation_result=-1; return; } } while (0)
static bool presentation_caches(const App *a) {
    return a->objects[0].page.cells==presentation_pages[0] && a->objects[1].page.cells==presentation_pages[1] && a->objects[2].page.cells==presentation_pages[2] &&
        a->request_id==presentation_request && a->worker.jobs_started==presentation_jobs;
}
static void object_presentation_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (presentation_result || !s || a->busy || a->io_busy || ++presentation_wait<24) return;
    presentation_wait=0; PRESENT_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (presentation_stage) {
    case 0:
        remove(presentation_path); grid_select_cell(&a->grid,2,1,false); PRESENT_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition pivot={0}; char *error=NULL;
        PRESENT_GUI_CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_COUNT,&error));
        set_pivot_definition(a,&pivot); free(error); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        for (unsigned i=0;i<3;i++) presentation_pages[i]=a->objects[i].page.cells;
        presentation_revision=a->book.command_revision; presentation_request=a->request_id; presentation_jobs=a->worker.jobs_started; presentation_titles=store->title_revision;
        a->selected_object=2; a->pivot_focus=6; a->chrome_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_TWO,0); break;
    case 4:
        PRESENT_GUI_CHECK(store->items[2].mark==MARK_LINE && a->book.command_revision==presentation_revision+1 && store->title_revision==presentation_titles && presentation_caches(a));
        a->pivot_focus=7; ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        PRESENT_GUI_CHECK(a->modal==MODAL_CHART_MEASURE); ui_note_key_press(KEY_DOWN,0); break;
    case 6:
        PRESENT_GUI_CHECK(a->context_focus==1); ui_note_key_press(KEY_ENTER,0); break;
    case 7:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].chart_measure==1 && a->chart_view.measure==1 && store->title_revision==presentation_titles+1 && a->book.command_revision==presentation_revision+2 && presentation_caches(a));
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        PRESENT_GUI_CHECK(!store->items[2].chart_measure && store->items[2].mark==MARK_LINE && s->redo_actions_count==1 && presentation_caches(a));
        PRESENT_GUI_CHECK(set_chart_measure(a,0)); set_chart_mark(a,MARK_LINE); set_chart_mark(a,MARK_SCATTER);
        PRESENT_GUI_CHECK(s->redo_actions_count==1 && a->book.command_revision==presentation_revision+3 && store->items[2].mark==MARK_LINE && presentation_caches(a));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        PRESENT_GUI_CHECK(store->items[2].mark==MARK_BAR && s->redo_actions_count==2 && presentation_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 10:
        PRESENT_GUI_CHECK(store->items[2].mark==MARK_LINE && !store->items[2].chart_measure && s->redo_actions_count==1 && presentation_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 11:
        PRESENT_GUI_CHECK(store->items[2].mark==MARK_LINE && store->items[2].chart_measure==1 && a->book.command_revision==presentation_revision+6 && presentation_caches(a));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 12:
        PRESENT_GUI_CHECK(presentation_caches(a)); workspace_job(a,JOB_SAVE,presentation_path); break;
    case 13:
        PRESENT_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,presentation_path); break;
    case 14:
        /* Reopen loads visible/selected results lazily. Select each offscreen
           definition explicitly instead of assuming all Pages are resident. */
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        PRESENT_GUI_CHECK(store->items[2].kind==OBJECT_CHART && store->items[2].mark==MARK_LINE && store->items[2].chart_measure==1 && store->items[2].source_id==store->items[0].id &&
            a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==3 && a->book.command_revision==presentation_revision+6 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); presentation_result=1;
        printf("PASS: native object presentation workflow (15 stages, %u checks)\n",presentation_checks); break;
    }
    presentation_stage++;
}
int app_test_object_presentation_command_result(void) { return presentation_result; }
#undef PRESENT_GUI_CHECK
