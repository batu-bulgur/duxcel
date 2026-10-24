/* Private deletion/input/cache fixture; excluded from production App. */
static unsigned deletion_stage,deletion_wait,deletion_checks;
static int deletion_result;
static uint64_t deletion_revision,deletion_high,deletion_ids[3];
static Cell *deletion_table,*deletion_pivot;
static const char *deletion_path="build/tests/generated/object-delete-command.dxls";
#define DELETE_GUI_CHECK(x) do { deletion_checks++; if (!(x)) { fprintf(stderr,"FAIL object delete GUI stage %u:%d: %s (%s)\n",deletion_stage,__LINE__,#x,a->message ? a->message : ""); deletion_result=-1; return; } } while (0)
static void deletion_menu(App *a,int index) {
    a->selected_object=index; a->modal=MODAL_OBJECT; a->context_origin=(Vector2){200,200}; a->context_focus=5; a->context_scroll=0;
    a->chrome_focus=a->pivot_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_ENTER,0);
}
static void object_delete_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (deletion_result || !s || a->busy || a->io_busy || ++deletion_wait<24) return;
    deletion_wait=0; DELETE_GUI_CHECK(a->modal!=MODAL_ERROR);
    switch (deletion_stage) {
    case 0:
        remove(deletion_path); grid_select_cell(&a->grid,2,1,false); DELETE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); PivotDefinition p={0};
        DELETE_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=1; create_chart(a); break;
    case 3:
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        deletion_table=a->objects[0].page.cells; deletion_pivot=a->objects[1].page.cells;
        for (unsigned i=0;i<3;i++) deletion_ids[i]=store->items[i].id;
        deletion_high=store->next_id; deletion_revision=a->book.command_revision;
        deletion_menu(a,0); break;
    case 4:
        DELETE_GUI_CHECK(a->modal==MODAL_NONE && store->count==3 && a->book.command_revision==deletion_revision && a->objects[0].page.cells==deletion_table && a->objects[1].page.cells==deletion_pivot);
        DELETE_GUI_CHECK(a->message && strstr(a->message,"supplies a pivot or chart")); deletion_menu(a,2); break;
    case 5:
        DELETE_GUI_CHECK(store->count==2 && store->next_id==deletion_high && a->book.command_revision==deletion_revision+1 && a->objects[0].page.cells==deletion_table && a->objects[1].page.cells==deletion_pivot);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        if (!object_ready(a,2)) { a->selected_object=2; return; }
        DELETE_GUI_CHECK(store->count==3 && store->items[2].id==deletion_ids[2] && store->items[2].source_id==deletion_ids[0] && store->next_id==deletion_high && s->redo_actions_count==1 && a->objects[0].page.cells==deletion_table && a->objects[1].page.cells==deletion_pivot);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        DELETE_GUI_CHECK(store->count==2 && a->book.command_revision==deletion_revision+3 && a->objects[0].page.cells==deletion_table && a->objects[1].page.cells==deletion_pivot); deletion_menu(a,1); break;
    case 8:
        DELETE_GUI_CHECK(store->count==1 && store->items[0].id==deletion_ids[0] && a->book.command_revision==deletion_revision+4 && a->objects[0].page.cells==deletion_table); deletion_menu(a,0); break;
    case 9:
        DELETE_GUI_CHECK(!store->count && !store->items && store->next_id==deletion_high && a->book.command_revision==deletion_revision+5 && !a->objects[0].page.cells);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 10:
        if (!object_ready(a,0)) { a->selected_object=0; return; }
        DELETE_GUI_CHECK(store->count==1 && store->items[0].id==deletion_ids[0] && store->next_id==deletion_high && a->objects[0].page.total==8);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 11:
        if (!object_ready(a,1)) { a->selected_object=1; return; }
        DELETE_GUI_CHECK(store->count==2 && store->items[1].id==deletion_ids[1] && store->items[1].source_id==deletion_ids[0] && a->objects[1].page.total==3 && a->book.command_revision==deletion_revision+7);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 12:
        DELETE_GUI_CHECK(store->count==1 && store->next_id==deletion_high && a->book.command_revision==deletion_revision+8);
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 13:
        workspace_job(a,JOB_SAVE,deletion_path); break;
    case 14:
        DELETE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,deletion_path); deletion_stage++; return;
    case 15:
        if (!object_ready(a,0)) { a->selected_object=0; return; }
        DELETE_GUI_CHECK(store->count==1 && store->items[0].id==deletion_ids[0] && store->next_id==deletion_high && a->objects[0].page.total==8 && a->book.command_revision==deletion_revision+8 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); deletion_result=1;
        printf("PASS: native object delete workflow (16 stages, %u checks)\n",deletion_checks); break;
    }
    deletion_stage++;
}
int app_test_object_delete_command_result(void) { return deletion_result; }
unsigned app_test_object_delete_command_stage(void) { return deletion_stage; }
#undef DELETE_GUI_CHECK
