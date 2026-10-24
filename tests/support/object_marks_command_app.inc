/* Private Marks binding/cache/input fixture; excluded from production App. */
static unsigned object_marks_stage,object_marks_wait,object_marks_checks;
static int object_marks_result;
static uint64_t object_marks_revision,object_marks_request,object_marks_jobs,object_marks_titles;
static Cell *object_marks_pages[3];
static const char *object_marks_path="build/tests/generated/object-marks-command.dxls";
#define PRESENT_GUI_CHECK(x) do { object_marks_checks++; if (!(x)) { fprintf(stderr,"FAIL presentation GUI stage %u:%d: %s (%s)\n",object_marks_stage,__LINE__,#x,a->message ? a->message : ""); object_marks_result=-1; return; } } while (0)
static bool object_marks_caches(const App *a) {
    return a->objects[0].page.cells==object_marks_pages[0] && a->objects[1].page.cells==object_marks_pages[1] && a->objects[2].page.cells==object_marks_pages[2] &&
        a->request_id==object_marks_request && a->worker.jobs_started==object_marks_jobs;
}
static void object_marks_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (object_marks_result || !s || a->busy || a->io_busy || ++object_marks_wait<24) return;
    object_marks_wait=0; PRESENT_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (object_marks_stage) {
    case 0:
        remove(object_marks_path); grid_select_cell(&a->grid,2,1,false); PRESENT_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
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
        for (unsigned i=0;i<3;i++) object_marks_pages[i]=a->objects[i].page.cells;
        object_marks_revision=a->book.command_revision; object_marks_request=a->request_id; object_marks_jobs=a->worker.jobs_started; object_marks_titles=store->marks_revision;
        a->selected_object=2; a->pivot_focus=8; a->mark_role=MARKS_COLOR; a->chrome_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        PRESENT_GUI_CHECK(a->modal==MODAL_MARKS); ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].marks.counts[MARKS_COLOR]==1 && !strcmp(store->items[2].marks.fields[MARKS_COLOR][0].name,"City") && a->book.command_revision==object_marks_revision+1 && store->marks_revision==object_marks_titles+1 && object_marks_caches(a));
        begin_marks(a,MARKS_SIZE,a->author_marks[MARKS_SIZE],false); break;
    case 6:
        PRESENT_GUI_CHECK(a->modal==MODAL_MARKS); ui_note_key_press(KEY_DOWN,0); break;
    case 7:
        PRESENT_GUI_CHECK(a->context_focus==1); ui_note_key_press(KEY_ENTER,0); break;
    case 8:
        PRESENT_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].marks.counts[MARKS_SIZE]==1 && store->items[2].marks.fields[MARKS_SIZE][0].measure && store->items[2].marks.fields[MARKS_SIZE][0].aggregate==AGG_SUM && a->book.command_revision==object_marks_revision+2 && object_marks_caches(a));
        a->pivot_focus=8; a->mark_role=MARKS_SIZE; ui_note_key_press(KEY_DELETE,0); break;
    case 9:
        PRESENT_GUI_CHECK(!store->items[2].marks.counts[MARKS_SIZE] && a->book.command_revision==object_marks_revision+3 && object_marks_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        PRESENT_GUI_CHECK(store->items[2].marks.counts[MARKS_SIZE]==1 && s->redo_actions_count==1 && object_marks_caches(a));
        PRESENT_GUI_CHECK(set_marks(a,&store->items[2].marks));
        PRESENT_GUI_CHECK(s->redo_actions_count==1 && a->book.command_revision==object_marks_revision+4 && object_marks_caches(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 11: {
        PRESENT_GUI_CHECK(!store->items[2].marks.counts[MARKS_SIZE] && a->book.command_revision==object_marks_revision+5 && object_marks_caches(a));
        MarkDefinition m={0}; MarkField city={.name="City"},amount={.name="Amount",.measure=true,.aggregate=AGG_SUM};
        for (unsigned r=0;r<DX_MARK_ROLES;r++) PRESENT_GUI_CHECK(marks_assign(&m,(MarkRole)r,r==MARKS_SIZE || r==MARKS_TOOLTIP ? &amount : &city,NULL));
        PRESENT_GUI_CHECK(set_marks(a,&m)); marks_free(&m); ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    }
    case 12:
        for (unsigned r=0;r<DX_MARK_ROLES;r++) PRESENT_GUI_CHECK(store->items[2].marks.counts[r]==1);
        PRESENT_GUI_CHECK(object_marks_caches(a) && a->book.command_revision==object_marks_revision+6 && store->items[2].marks.fields[MARKS_SIZE][0].aggregate==AGG_SUM);
        workspace_job(a,JOB_SAVE,object_marks_path); break;
    case 13:
        PRESENT_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,object_marks_path); break;
    case 14:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        for (unsigned r=0;r<DX_MARK_ROLES;r++) PRESENT_GUI_CHECK(store->items[2].marks.counts[r]==1);
        PRESENT_GUI_CHECK(store->items[2].kind==OBJECT_CHART && store->items[2].source_id==store->items[0].id && store->items[2].marks.fields[MARKS_TOOLTIP][0].measure &&
            a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==3 && a->book.command_revision==object_marks_revision+6 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); object_marks_result=1;
        printf("PASS: native object Marks workflow (15 stages, %u checks)\n",object_marks_checks); break;
    }
    object_marks_stage++;
}
int app_test_object_marks_command_result(void) { return object_marks_result; }
#undef PRESENT_GUI_CHECK
