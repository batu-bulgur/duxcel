/* Private rename editor/cache/draft fixture; absent from production App code. */
static unsigned object_name_stage,object_name_wait,object_name_checks;
static int object_name_result;
static uint64_t object_name_revision,object_name_request,object_name_jobs;
static Cell *object_name_pages[3];
static char object_name_original[DX_OBJECT_NAME_BYTES+1];
static const char *object_name_path="build/tests/generated/object-name-command.dxls";
#define NAME_GUI_CHECK(x) do { object_name_checks++; if (!(x)) { fprintf(stderr,"FAIL object name GUI stage %u:%d: %s (%s)\n",object_name_stage,__LINE__,#x,a->object_name_error ? a->object_name_error : a->message ? a->message : ""); object_name_result=-1; return; } } while (0)
static bool object_name_caches(const App *a) {
    return a->objects[0].page.cells==object_name_pages[0] && a->objects[1].page.cells==object_name_pages[1] && a->objects[2].page.cells==object_name_pages[2] &&
        a->request_id==object_name_request && a->worker.jobs_started==object_name_jobs;
}
static void object_name_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (object_name_result || !s || a->busy || a->io_busy || ++object_name_wait<24) return;
    object_name_wait=0; NAME_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (object_name_stage) {
    case 0:
        remove(object_name_path); grid_select_cell(&a->grid,2,1,false); NAME_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition pivot={0}; char *error=NULL;
        NAME_GUI_CHECK(pivot_assign(&pivot,PIVOT_ROWS,"City",AGG_SUM,&error) && pivot_assign(&pivot,PIVOT_VALUES,"Amount",AGG_SUM,&error)); set_pivot_definition(a,&pivot); free(error); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        for (unsigned i=0;i<3;i++) object_name_pages[i]=a->objects[i].page.cells;
        object_name_revision=a->book.command_revision; object_name_request=a->request_id; object_name_jobs=a->worker.jobs_started;
        snprintf(object_name_original,sizeof(object_name_original),"%s",store->items[1].name);
        a->selected_object=1; begin_object_name(a); NAME_GUI_CHECK(a->modal==MODAL_OBJECT_NAME && a->object_name_target_valid);
        input_set(&a->input,"  Regional view  "); ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        NAME_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(store->items[1].name,"Regional view") && a->book.command_revision==object_name_revision+1 && object_name_caches(a));
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        NAME_GUI_CHECK(!strcmp(store->items[1].name,object_name_original) && s->redo_actions_count==1 && object_name_caches(a));
        a->selected_object=1; begin_object_name(a); ui_note_key_press(KEY_ENTER,0); break;
    case 6:
        NAME_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(store->items[1].name,object_name_original) && s->redo_actions_count==1 && a->book.command_revision==object_name_revision+2 && object_name_caches(a));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        NAME_GUI_CHECK(!strcmp(store->items[1].name,"Regional view") && a->book.command_revision==object_name_revision+3 && object_name_caches(a));
        a->selected_object=1; begin_object_name(a); input_set(&a->input,store->items[0].name); ui_note_key_press(KEY_ENTER,0); break;
    case 8:
        NAME_GUI_CHECK(a->modal==MODAL_OBJECT_NAME && a->object_name_error && !strcmp(a->input.text,store->items[0].name) && !strcmp(store->items[1].name,"Regional view") && a->book.command_revision==object_name_revision+3 && object_name_caches(a));
        input_set(&a->input,"Draft stays");
        { ObjectCommandContext context={&a->book,&a->request_id,SIZE_MAX,SIZE_MAX,true,false}; ObjectNameTarget newer;
          NAME_GUI_CHECK(workbook_prepare_object_name(&context,a->book.active,store->items[1].id,&newer,NULL) && workbook_apply_object_name_recorded(&context,&newer,"External label",NULL,NULL).status==OBJECT_NAME_APPLIED); }
        ui_note_key_press(KEY_ENTER,0); break;
    case 9:
        NAME_GUI_CHECK(a->modal==MODAL_OBJECT_NAME && a->object_name_error && !strcmp(a->input.text,"Draft stays") && !strcmp(store->items[1].name,"External label") && a->book.command_revision==object_name_revision+4 && object_name_caches(a));
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 10:
        NAME_GUI_CHECK(a->modal==MODAL_NONE); a->selected_object=1; begin_object_name(a); input_set(&a->input,"Final view");
        /* A background request epoch can advance while a property draft is open;
           it is captured afresh at commit rather than invalidating the name. */
        a->request_id++; object_name_request=a->request_id; ui_note_key_press(KEY_ENTER,0); break;
    case 11:
        NAME_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(store->items[1].name,"Final view") && a->book.command_revision==object_name_revision+5 && object_name_caches(a));
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 12:
        NAME_GUI_CHECK(object_name_caches(a)); workspace_job(a,JOB_SAVE,object_name_path); break;
    case 13:
        NAME_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,object_name_path); break;
    case 14:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        NAME_GUI_CHECK(a->objects[0].page.total==8 && a->objects[1].page.total==3 && a->objects[2].page.total==3 && !strcmp(store->items[1].name,"Final view") &&
            store->items[1].source_id==store->items[0].id && store->items[2].source_id==store->items[0].id && a->book.command_revision==object_name_revision+5 && !s->undo_actions_count && !a->book.dirty);
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 15:
        NAME_GUI_CHECK(!strcmp(store->items[1].name,"Final view") && !a->book.dirty && !s->undo_actions_count);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); object_name_result=1;
        printf("PASS: native object name workflow (16 stages, %u checks)\n",object_name_checks); break;
    }
    object_name_stage++;
}
int app_test_object_name_command_result(void) { return object_name_result; }
#undef NAME_GUI_CHECK
