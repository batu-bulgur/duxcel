/* Private source chooser/binding/cache fixture; excluded from production. */
static unsigned binding_stage,binding_wait,binding_checks;
static int binding_result;
static uint64_t binding_revision,binding_id,binding_row,binding_rows;
static size_t binding_col,binding_columns;
static Cell *binding_sources[2],*binding_noop_page;
static uint64_t binding_noop_request,binding_noop_jobs;
static const char *binding_path="build/tests/generated/object-binding.dxls";
#define BIND_GUI_CHECK(x) do { binding_checks++; if (!(x)) { fprintf(stderr,"FAIL object binding GUI stage %u:%d: %s (%s)\n",binding_stage,__LINE__,#x,a->message ? a->message : ""); binding_result=-1; return; } } while (0)
static bool binding_caches(App *a) { return a->objects[0].page.cells==binding_sources[0] && a->objects[1].page.cells==binding_sources[1]; }
static double binding_sum(App *a) { Page *p=&a->objects[2].page; double sum=0; for (size_t r=0;r<p->row_count;r++) sum+=p->cells[r*p->column_count+1].number; return sum; }
static bool binding_shape(const SheetObject *o) { return o->id==binding_id && o->row==binding_row && o->rows==binding_rows && o->col==binding_col && o->columns==binding_columns && o->kind==OBJECT_CHART && o->mark==MARK_LINE && o->marks.counts[MARKS_TOOLTIP]==1 && !strcmp(o->marks.fields[MARKS_TOOLTIP][0].name,"Amount"); }
static void object_binding_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (binding_result || !s || a->busy || a->io_busy || ++binding_wait<24) return;
    binding_wait=0; BIND_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (binding_stage) {
    case 0: {
        remove(binding_path); BIND_GUI_CHECK(a->page.total==3); grid_select_cell(&a->grid,0,4,false); create_pivot(a); PivotDefinition p={0};
        BIND_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 1:
        if (!object_ready(a,0)) return;
        a->selected_object=-1; grid_select_cell(&a->grid,40,4,false); BIND_GUI_CHECK(import_table(a,"build/tests/generated/source-connection-b.duckdb")); break;
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=0; create_chart(a); break;
    case 3: {
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        binding_sources[0]=a->objects[0].page.cells; binding_sources[1]=a->objects[1].page.cells; a->selected_object=2;
        MarkField field={.name="Amount",.aggregate=AGG_SUM,.measure=true}; BIND_GUI_CHECK(assign_mark_field(a,MARKS_TOOLTIP,&field)); set_chart_mark(a,MARK_LINE);
        SheetObject *o=store->items+2; binding_id=o->id; binding_row=o->row; binding_rows=o->rows; binding_col=o->col; binding_columns=o->columns; binding_revision=a->book.command_revision;
        BIND_GUI_CHECK(binding_sum(a)==60 && binding_shape(o));
        a->modal=MODAL_OBJECT; a->context_origin=(Vector2){200,200}; a->context_focus=7; a->context_scroll=0; ui_note_key_press(KEY_ENTER,0); break;
    }
    case 4:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE && a->object_source_count==2 && a->object_source_choices[1]==store->items[1].id); ui_note_key_press(KEY_DOWN,0); break;
    case 5:
        BIND_GUI_CHECK(a->object_source_focus==1); ui_note_key_press(KEY_ENTER,0); break;
    case 6:
        if (!object_ready(a,2)) return;
        BIND_GUI_CHECK(a->modal==MODAL_NONE && store->items[2].source_id==store->items[1].id && binding_sum(a)==600 && a->book.command_revision==binding_revision+1 && binding_shape(store->items+2) && binding_caches(a));
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7:
        if (!object_ready(a,2)) return;
        BIND_GUI_CHECK(!store->items[2].source_id && binding_sum(a)==60 && s->redo_actions_count==1 && binding_caches(a));
        binding_noop_page=a->objects[2].page.cells; binding_noop_request=a->request_id; binding_noop_jobs=a->worker.jobs_started; begin_object_source(a); break;
    case 8:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE && !a->object_source_focus); ui_note_key_press(KEY_ENTER,0); break;
    case 9:
        BIND_GUI_CHECK(a->modal==MODAL_NONE && s->redo_actions_count==1 && a->book.command_revision==binding_revision+2 && a->objects[2].page.cells==binding_noop_page && a->request_id==binding_noop_request && a->worker.jobs_started==binding_noop_jobs);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 10:
        if (!object_ready(a,2)) return;
        BIND_GUI_CHECK(store->items[2].source_id==store->items[1].id && binding_sum(a)==600 && a->book.command_revision==binding_revision+3 && binding_caches(a)); begin_object_source(a); break;
    case 11:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE); ui_note_key_press(KEY_ESCAPE,0); break;
    case 12: {
        BIND_GUI_CHECK(a->modal==MODAL_NONE && a->book.command_revision==binding_revision+3 && binding_caches(a)); begin_object_source(a);
        ObjectBindingContext context={{&a->book,&a->request_id,SIZE_MAX,SIZE_MAX,true,false},&a->analysis,&a->analysis_revision}; ObjectBindingTarget t;
        BIND_GUI_CHECK(workbook_prepare_object_binding(&context,a->book.active,binding_id,&t,NULL) && workbook_apply_object_binding_recorded(&context,&t,0,NULL,NULL).status==OBJECT_BINDING_APPLIED);
        invalidate_object(a,2); break;
    }
    case 13:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE && !store->items[2].source_id); a->object_source_focus=1; ui_note_key_press(KEY_ENTER,0); break;
    case 14:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE && !store->items[2].source_id && a->book.command_revision==binding_revision+4); ui_note_key_press(KEY_ESCAPE,0); break;
    case 15:
        if (!object_ready(a,2)) return;
        BIND_GUI_CHECK(binding_sum(a)==60 && binding_caches(a)); begin_object_source(a); a->object_source_focus=1; break;
    case 16:
        BIND_GUI_CHECK(a->modal==MODAL_OBJECT_SOURCE); ui_note_key_press(KEY_ENTER,0); break;
    case 17:
        if (!object_ready(a,2)) return;
        BIND_GUI_CHECK(store->items[2].source_id==store->items[1].id && binding_sum(a)==600 && a->book.command_revision==binding_revision+5 && binding_caches(a) && binding_shape(store->items+2)); ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 18:
        workspace_job(a,JOB_SAVE,binding_path); break;
    case 19:
        BIND_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,binding_path); break;
    case 20:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        BIND_GUI_CHECK(binding_shape(store->items+2) && store->items[2].source_id==store->items[1].id && a->page.total==3 && a->objects[1].page.total==3 && binding_sum(a)==600 && a->book.command_revision==binding_revision+5 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); binding_result=1;
        printf("PASS: native object binding workflow (21 stages, %u checks)\n",binding_checks); break;
    }
    binding_stage++;
}
int app_test_object_binding_command_result(void) { return binding_result; }
unsigned app_test_object_binding_command_stage(void) { return binding_stage; }
#undef BIND_GUI_CHECK
