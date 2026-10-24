/* Private resolved connection/relation fixture; absent from production App. */
static unsigned table_replace_stage,table_replace_wait,table_replace_checks;
static int table_replace_result;
static uint64_t table_replace_revision,table_replace_ids[3];
static Cell *table_replace_unrelated;
static FormatStore table_replace_formats;
static const char *table_replace_path="build/tests/generated/table-replacement.dxls";
#define REPLACE_GUI_CHECK(x) do { table_replace_checks++; if (!(x)) { fprintf(stderr,"FAIL Table replacement GUI stage %u:%d: %s (%s)\n",table_replace_stage,__LINE__,#x,a->message ? a->message : ""); table_replace_result=-1; return; } } while (0)
static size_t table_replace_relation(App *a,const char *name) { Page *p=&a->objects[0].page; size_t i=0; while (i<p->relation_count && strcmp(p->relations[i].name,name)) i++; return i; }
static bool table_replace_stable(App *a) { Worksheet *s=active_sheet(a); return a->objects[2].page.cells==table_replace_unrelated && !memcmp(&s->formats,&table_replace_formats,sizeof(s->formats)); }
static bool table_replace_ready(App *a) {
    for (unsigned i=0;i<2;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return false; }
    return true;
}
static double table_replace_sum(App *a) { Page *p=&a->objects[1].page; double n=0; for (size_t r=0;r<p->row_count;r++) n+=p->cells[r*p->column_count+1].number; return n; }
static void table_replacement_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a); ObjectStore *store=s ? &s->objects : NULL;
    if (table_replace_result || !s || a->busy || a->io_busy || ++table_replace_wait<24) return;
    table_replace_wait=0; REPLACE_GUI_CHECK(a->modal!=MODAL_ERROR && !a->objects[0].failed && !a->objects[1].failed && !a->objects[2].failed);
    switch (table_replace_stage) {
    case 0:
        remove(table_replace_path); grid_select_cell(&a->grid,2,1,false); REPLACE_GUI_CHECK(import_table(a,"build/tests/generated/source-connection-a.duckdb")); break;
    case 1: {
        if (!object_ready(a,0)) return;
        REPLACE_GUI_CHECK(store->items[0].source.table && !strcmp(store->items[0].source.table,"alpha") && a->objects[0].page.total==3);
        a->selected_object=0; create_pivot(a); PivotDefinition p={0};
        REPLACE_GUI_CHECK(pivot_assign(&p,PIVOT_ROWS,"City",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 2:
        if (!object_ready(a,1)) return;
        a->selected_object=-1; grid_select_cell(&a->grid,40,1,false); REPLACE_GUI_CHECK(import_table(a,"tests/fixtures/duplicates.csv")); break;
    case 3: {
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        table_replace_unrelated=a->objects[2].page.cells; for (unsigned i=0;i<3;i++) table_replace_ids[i]=store->items[i].id;
        CellFormat f=format_default(); f.number=FORMAT_ACCOUNTING;
        REPLACE_GUI_CHECK(format_apply(&s->formats,(FormatRange){0,DX_SHEET_ROWS-1,2,2},FORMAT_NUMERIC,f,NULL)); worksheet_record(s,EDIT_FORMAT);
        table_replace_formats=s->formats; table_replace_revision=a->book.command_revision; a->selected_object=0; begin_table_relation(a); break;
    }
    case 4:
        REPLACE_GUI_CHECK(a->modal==MODAL_TABLE_RELATION); a->table_relation_focus=(int)table_replace_relation(a,"beta");
        REPLACE_GUI_CHECK((size_t)a->table_relation_focus<a->objects[0].page.relation_count); ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(!strcmp(store->items[0].source.table,"beta") && a->objects[0].page.total==2 && fabs(table_replace_sum(a)-300)<.001 && a->book.command_revision==table_replace_revision+1 && table_replace_stable(a));
        a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(!strcmp(store->items[0].source.table,"alpha") && a->objects[0].page.total==3 && fabs(table_replace_sum(a)-60)<.001 && a->book.command_revision==table_replace_revision+2 && s->redo_actions_count==1 && table_replace_stable(a));
        { uint64_t request=a->request_id,jobs=a->worker.jobs_started; a->selected_object=0; begin_table_relation(a); REPLACE_GUI_CHECK(choose_table_relation(a,table_replace_relation(a,"alpha")) && s->redo_actions_count==1 && a->request_id==request && a->worker.jobs_started==jobs); }
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7: {
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(!strcmp(store->items[0].source.table,"beta") && fabs(table_replace_sum(a)-300)<.001 && a->book.command_revision==table_replace_revision+3 && table_replace_stable(a));
        Analysis p=analysis_copy(&store->items[0].source); free(p.path); p.path=dx_absolute_path("build/tests/generated/source-connection-b.duckdb");
        REPLACE_GUI_CHECK(p.path && set_table_source(a,0,&p)); analysis_free(&p); break;
    }
    case 8:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(fabs(table_replace_sum(a)-3000)<.001 && a->book.command_revision==table_replace_revision+4 && table_replace_stable(a)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 9:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(fabs(table_replace_sum(a)-300)<.001 && a->book.command_revision==table_replace_revision+5 && s->redo_actions_count==1 && table_replace_stable(a));
        { uint64_t request=a->request_id,jobs=a->worker.jobs_started; REPLACE_GUI_CHECK(set_table_source(a,0,&store->items[0].source) && s->redo_actions_count==1 && a->request_id==request && a->worker.jobs_started==jobs); }
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 10:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(fabs(table_replace_sum(a)-3000)<.001 && a->book.command_revision==table_replace_revision+6 && table_replace_stable(a)); a->selected_object=0; begin_table_relation(a); break;
    case 11:
        REPLACE_GUI_CHECK(a->modal==MODAL_TABLE_RELATION); a->table_relation_focus=(int)table_replace_relation(a,"beta_view");
        REPLACE_GUI_CHECK((size_t)a->table_relation_focus<a->objects[0].page.relation_count); ui_note_key_press(KEY_ENTER,0); break;
    case 12:
        if (!table_replace_ready(a)) return;
        REPLACE_GUI_CHECK(!strcmp(store->items[0].source.table,"beta_view") && fabs(table_replace_sum(a)-3000)<.001 && a->book.command_revision==table_replace_revision+7 && table_replace_stable(a)); ui_set_scale(&a->ui,1.5f); SetWindowSize(1000,700); break;
    case 13:
        REPLACE_GUI_CHECK(table_replace_stable(a)); workspace_job(a,JOB_SAVE,table_replace_path); break;
    case 14:
        REPLACE_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,table_replace_path); break;
    case 15:
        for (unsigned i=0;i<3;i++) if (!object_ready(a,i)) { a->selected_object=(int)i; return; }
        for (unsigned i=0;i<3;i++) REPLACE_GUI_CHECK(store->items[i].id==table_replace_ids[i]);
        REPLACE_GUI_CHECK(store->items[1].source_id==store->items[0].id && !strcmp(store->items[0].source.table,"beta_view") && a->objects[0].page.total==2 && a->objects[2].page.total==8 && fabs(table_replace_sum(a)-3000)<.001 &&
            format_at(&s->formats,12,2).number==FORMAT_ACCOUNTING && a->book.command_revision==table_replace_revision+7 && !s->undo_actions_count && !a->book.dirty);
        ui_set_scale(&a->ui,1); SetWindowSize(1280,820); table_replace_result=1;
        printf("PASS: native Table replacement workflow (16 stages, %u checks)\n",table_replace_checks); break;
    }
    table_replace_stage++;
}
int app_test_table_replacement_result(void) { return table_replace_result; }
#undef REPLACE_GUI_CHECK

unsigned app_test_table_replacement_stage(void) { return table_replace_stage; }
