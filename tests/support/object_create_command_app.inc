/* Private creation/cache/Undo fixture; excluded from production App. */
static unsigned creation_stage,creation_wait,creation_checks;
static int creation_result;
static uint64_t creation_revision,creation_high,creation_table;
static Analysis creation_primary;
static Cell *creation_page;
static const char *creation_path="build/tests/generated/object-create-command.dxls";
#define CREATE_GUI_CHECK(x) do { creation_checks++; if (!(x)) { fprintf(stderr,"FAIL object create GUI stage %u:%d: %s (%s)\n",creation_stage,__LINE__,#x,a->message ? a->message : ""); creation_result=-1; return; } } while (0)
static void object_create_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (creation_result || !s || a->busy || a->io_busy || ++creation_wait<24) return;
    creation_wait=0; CREATE_GUI_CHECK(a->modal!=MODAL_ERROR);
    switch (creation_stage) {
    case 0:
        remove(creation_path); CREATE_GUI_CHECK(!s->formats.source_aware);
        creation_primary=a->analysis; grid_select_cell(&a->grid,2,1,false); CREATE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv"));
        CREATE_GUI_CHECK(s->formats.source_aware && s->undo_actions[s->undo_actions_count-1]==EDIT_OBJECTS_FORMAT && !s->formats.undo[s->formats.undo_count-1].source_aware); break;
    case 1: {
        if (!object_ready(a,0)) return;
        creation_revision=a->book.command_revision; creation_high=store->next_id; creation_table=store->items[0].id; creation_page=a->objects[0].page.cells;
        a->selected_object=0; create_pivot(a);
        CREATE_GUI_CHECK(store->count==2 && store->next_id==creation_high+1 && store->items[1].source_id==creation_table && a->book.command_revision==creation_revision+1);
        PivotDefinition p={0}; CREATE_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        CREATE_GUI_CHECK(a->objects[1].page.total==3 && a->objects[0].page.cells==creation_page);
        a->selected_object=1; a->modal=MODAL_OBJECT; a->context_origin=(Vector2){200,200}; a->context_focus=3; a->context_scroll=0;
        a->chrome_focus=a->pivot_focus=0; a->ribbon_focus=false; ui_note_key_press(KEY_ENTER,0); break;
    case 3:
        if (!object_ready(a,2)) return;
        CREATE_GUI_CHECK(store->count==3 && store->items[2].kind==OBJECT_CHART && store->items[2].source_id==creation_table && store->next_id==creation_high+2 && a->book.command_revision==creation_revision+3 && a->objects[0].page.cells==creation_page);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        CREATE_GUI_CHECK(store->count==2 && store->next_id==creation_high+2 && s->redo_actions_count==1 && a->objects[0].page.cells==creation_page);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 5:
        if (!object_ready(a,2)) { a->selected_object=2; return; }
        CREATE_GUI_CHECK(store->count==3 && store->next_id==creation_high+2 && store->items[2].source_id==creation_table);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        CREATE_GUI_CHECK(store->count==2 && s->redo_actions_count==1);
        a->selected_object=1; create_chart(a);
        CREATE_GUI_CHECK(store->count==3 && store->items[2].id==creation_high+3 && !s->redo_actions_count); break;
    case 7:
        if (!object_ready(a,2)) return;
        grid_select_cell(&a->grid,30,20,false); CREATE_GUI_CHECK(import_table(a,"tests/fixtures/null-text.csv"));
        CREATE_GUI_CHECK(store->count==4 && store->items[3].kind==OBJECT_TABLE && store->next_id==creation_high+4 && a->book.command_revision==creation_revision+8); break;
    case 8:
        if (!object_ready(a,3)) return;
        CREATE_GUI_CHECK(a->objects[0].page.cells==creation_page && a->objects[3].page.total>0);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        CREATE_GUI_CHECK(store->count==3 && store->next_id==creation_high+4); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 10:
        if (!object_ready(a,3)) { a->selected_object=3; return; }
        CREATE_GUI_CHECK(store->count==4 && store->items[3].id==creation_high+4 && a->book.command_revision==creation_revision+10);
        ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 11:
        grid_select_cell(&a->grid,55,20,false); CREATE_GUI_CHECK(import_table(a,"build/tests/generated/source-connection-a.duckdb"));
        CREATE_GUI_CHECK(a->object_import.active && store->count==4 && store->next_id==creation_high+4 && a->book.command_revision==creation_revision+10);
        cancel_active_query(a); CREATE_GUI_CHECK(!a->object_import.active && !a->busy && store->count==4 && store->next_id==creation_high+4 && !memcmp(&a->analysis,&creation_primary,sizeof(creation_primary))); break;
    case 12:
        CREATE_GUI_CHECK(import_table(a,"build/tests/generated/source-connection-a.duckdb"));
        CREATE_GUI_CHECK(a->object_import.active && store->count==4 && store->next_id==creation_high+4 && a->book.command_revision==creation_revision+10); break;
    case 13:
        if (!object_ready(a,4)) { a->selected_object=4; return; }
        CREATE_GUI_CHECK(!a->object_import.active && store->count==5 && store->next_id==creation_high+5 && a->book.command_revision==creation_revision+11 && store->items[4].source.kind==SOURCE_DATABASE && store->items[4].source.schema && store->items[4].source.table && !strcmp(store->items[4].source.table,"alpha") && !memcmp(&a->analysis,&creation_primary,sizeof(creation_primary)));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 14:
        CREATE_GUI_CHECK(store->count==4 && store->next_id==creation_high+5); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 15:
        if (!object_ready(a,4)) { a->selected_object=4; return; }
        CREATE_GUI_CHECK(store->count==5 && store->next_id==creation_high+5 && a->book.command_revision==creation_revision+13 && !strcmp(store->items[4].source.table,"alpha"));
        break;
    case 16:
        workspace_job(a,JOB_SAVE,creation_path); break;
    case 17:
        CREATE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,creation_path); creation_stage++; return;
    case 18:
        if (!object_ready(a,0)) { a->selected_object=0; return; }
        CREATE_GUI_CHECK(store->count==5 && store->next_id==creation_high+5 && store->items[1].source_id==creation_table && store->items[2].source_id==creation_table && !strcmp(store->items[4].source.table,"alpha") && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); creation_result=1;
        printf("PASS: native object create workflow (19 stages, %u checks)\n",creation_checks); break;
    }
    creation_stage++;
}
int app_test_object_create_command_result(void) { return creation_result; }
unsigned app_test_object_create_command_stage(void) { return creation_stage; }
#undef CREATE_GUI_CHECK
