/* Private tagged real-App topology fixture. No production hook/export. */
static unsigned topology_stage,topology_wait,topology_checks;
static int topology_result;
static uint64_t topology_source,topology_copy,topology_offline,topology_revision;
#define TOPOLOGY_GUI_CHECK(x) do { topology_checks++; if (!(x)) { fprintf(stderr,"FAIL: topology GUI stage %u line %d: %s (%s)\n",topology_stage,__LINE__,#x,a->message ? a->message : ""); topology_result=-1; return; } } while (0)
static size_t topology_index(App *a,uint64_t id) { for (size_t i=0;i<a->book.count;i++) if (a->book.sheets[i].id==id) return i; return a->book.count; }
static bool topology_menu(App *a,uint64_t id,int focus) {
    if (a->modal!=MODAL_SHEET) { begin_sheet_menu(a,topology_index(a,id),(Vector2){300,300}); a->sheet_menu_focus=focus; return false; }
    ui_note_key_press(KEY_ENTER,0); return true;
}
static void sheet_topology_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (topology_result || a->busy || a->io_busy || !s || ++topology_wait<12) return;
    topology_wait=0;
    switch (topology_stage) {
    case 0:
        TOPOLOGY_GUI_CHECK(a->page.column_count && !a->fetch_failed && a->analysis.kind==SOURCE_CSV);
        topology_source=s->id;
        grid_ensure_columns(&a->grid,201); grid_set_width(&a->grid,200,1199.75f); TOPOLOGY_GUI_CHECK(commit_grid_widths(a));
        grid_select_cell(&a->grid,20,200,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Topology note"); ui_note_key_press(KEY_ENTER,0); break;
    case 1:
        TOPOLOGY_GUI_CHECK(!strcmp(cells_get(&s->cells,20,200),"Topology note") && s->view.widths[200]==1199.75f);
        grid_ensure_columns(&a->grid,a->grid.width_count+52); /* Representation-only outgoing capacity. */
        if (!topology_menu(a,topology_source,0)) return; break;
    case 2: {
        TOPOLOGY_GUI_CHECK(a->book.count==2 && s->id!=topology_source && !strcmp(cells_get(&s->cells,20,200),"Topology note") && a->grid.widths[200]==1199.75f);
        topology_copy=s->id; WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,false,&selected) && selected.packet && selected.operation==SHEET_DUPLICATE);
        TOPOLOGY_GUI_CHECK(!s->undo_actions_count && !s->width_history);
        grid_select_cell(&a->grid,9001,200,false); a->grid.horizontal=100; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 3:
        TOPOLOGY_GUI_CHECK(a->book.count==1 && s->id==topology_source); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 4:
        TOPOLOGY_GUI_CHECK(a->book.count==2 && s->id==topology_copy && a->grid.row==9001 && a->grid.col==200 && a->grid.widths[200]==1199.75f);
        if (!topology_menu(a,topology_source,6)) return; break;
    case 5: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==1 && s->id==topology_copy && workbook_lifecycle_presence(&a->book,false,&selected) && selected.packet && selected.operation==SHEET_DELETE);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 6:
        TOPOLOGY_GUI_CHECK(a->book.count==2 && topology_index(a,topology_source)<a->book.count && !strcmp(cells_get(&a->book.sheets[topology_index(a,topology_source)].cells,20,200),"Topology note"));
        if (!topology_menu(a,topology_source,6)) return; break;
    case 7:
        TOPOLOGY_GUI_CHECK(a->book.count==1 && s->id==topology_copy);
        TOPOLOGY_GUI_CHECK(capture_sheet(a));
        /* Genuine existing producer creates an unresolved offline source. No
           fake result, origin binding, packet or worksheet action is seeded. */
        TOPOLOGY_GUI_CHECK(workbook_sheet_command(&a->book,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Offline",.analysis=&(Analysis){.kind=SOURCE_DATABASE,.path="build/tests/generated/topology-missing.duckdb"}},NULL,NULL));
        topology_offline=active_sheet(a)->id; TOPOLOGY_GUI_CHECK(activate_sheet(a,a->book.active,false)); break;
    case 8:
        TOPOLOGY_GUI_CHECK(s->id==topology_offline && a->fetch_failed && !a->analysis.table);
        topology_revision=a->book.command_revision;
        if (!topology_menu(a,topology_offline,6)) return; break;
    case 9: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==1 && s->id==topology_copy && a->book.command_revision==topology_revision+1);
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,false,&selected) && selected.operation==SHEET_DELETE && !selected.packet);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    }
    case 10:
        TOPOLOGY_GUI_CHECK(a->book.count==2 && s->id==topology_offline && a->fetch_failed && !a->analysis.table);
        if (!topology_menu(a,topology_offline,0)) return; break;
    case 11: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==3 && s->id!=topology_offline && a->fetch_failed && !a->analysis.table);
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,false,&selected) && selected.operation==SHEET_DUPLICATE && !selected.packet);
        char *error=NULL; uint64_t revision=a->book.command_revision; size_t count=a->book.count;
        a->io_busy=true; bool changed=apply_sheet_presence(a,a->book.active,false,&error); a->io_busy=false;
        TOPOLOGY_GUI_CHECK(!changed && error && a->book.count==count && a->book.command_revision==revision); free(error);
        printf("PASS: native sheet topology workflow (12 stages, %u checks)\n",topology_checks); topology_result=1; break;
    }
    }
    topology_stage++;
}
int app_test_sheet_topology_result(void) { return topology_result; }
#undef TOPOLOGY_GUI_CHECK
