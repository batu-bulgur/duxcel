/* Private tagged real-App topology fixture. No production hook/export. */
static unsigned topology_stage,topology_wait,topology_checks;
static int topology_result;
static uint64_t topology_source,topology_copy,topology_offline,topology_revision;
static uint64_t topology_enriched,topology_objects,topology_binding_revision,topology_object_revision,topology_object_high,topology_request,topology_analysis_revision;
static Cell *topology_page;
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
        if (!topology_menu(a,topology_source,1)) return; break;
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
        if (!topology_menu(a,topology_offline,1)) return; break;
    case 11: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==3 && s->id!=topology_offline && a->fetch_failed && !a->analysis.table);
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,false,&selected) && selected.operation==SHEET_DUPLICATE && !selected.packet);
        char *error=NULL; uint64_t revision=a->book.command_revision; size_t count=a->book.count;
        a->io_busy=true; bool changed=apply_sheet_presence(a,a->book.active,false,&error); a->io_busy=false;
        TOPOLOGY_GUI_CHECK(!changed && error && a->book.count==count && a->book.command_revision==revision); free(error);
        TOPOLOGY_GUI_CHECK(activate_sheet(a,topology_index(a,topology_copy),true)); break;
    }
    case 12:
        TOPOLOGY_GUI_CHECK(s->id==topology_copy && a->analysis.kind==SOURCE_CSV && !a->analysis.source_version && !s->source_versions.count && a->page.column_count==3 && !a->fetch_failed);
        grid_select_cell(&a->grid,0,2,false); grid_reveal_cell(&a->grid,&a->page,a->ui.scale*a->grid.zoom);
        if (!topology_menu(a,topology_copy,1)) return; break;
    case 13:
        TOPOLOGY_GUI_CHECK(a->book.count==4 && s->id!=topology_copy && !s->undo_actions_count && !s->source_versions.count);
        topology_enriched=s->id; topology_binding_revision=a->book.command_revision; topology_page=a->page.cells; topology_analysis_revision=a->analysis_revision;
        TOPOLOGY_GUI_CHECK(a->page.total==6 && a->page.cells && a->page.offset==0 && a->page.row_count>=2 && a->page.cells[1].is_null && !a->page.cells[4].is_null && !strcmp(a->page.cells[4].text,"NULL"));
        a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        grid_select_cell(&a->grid,0,2,false); ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    case 14:
        if (a->source_format.active) return;
        TOPOLOGY_GUI_CHECK(s->id==topology_enriched && a->analysis.source_version==1 && s->source_versions.high_water==1 && s->source_versions.count==1 && s->source_versions.items[0].raw.bytes);
        TOPOLOGY_GUI_CHECK(a->book.command_revision==topology_binding_revision+1 && s->undo_actions_count==1 && s->undo_actions[0]==EDIT_ANALYSIS_FORMAT && (active_format(a).flags&FORMAT_BOLD) && a->page.cells==topology_page && a->analysis_revision==topology_analysis_revision);
        topology_request=a->request_id; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 15:
        TOPOLOGY_GUI_CHECK(s->id==topology_enriched && !a->analysis.source_version && !s->formats.source_aware && !s->formats.count && !s->undo_actions_count && s->redo_actions_count==1 && s->redo_actions[0]==EDIT_ANALYSIS_FORMAT);
        TOPOLOGY_GUI_CHECK(s->source_versions.high_water==1 && s->source_versions.count==1 && s->source_versions.items[0].raw.bytes && a->page.cells==topology_page && a->request_id==topology_request && a->analysis_revision==topology_analysis_revision && a->book.command_revision==topology_binding_revision+2);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 16: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==3 && s->id==topology_copy && a->book.command_revision==topology_binding_revision+3);
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,true,&selected) && selected.sheet==topology_enriched && selected.packet && selected.detached && selected.worksheet->source_versions.high_water==1 && selected.worksheet->redo_actions_count==1 && selected.worksheet->redo_actions[0]==EDIT_ANALYSIS_FORMAT);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 17:
        if (a->source_format.active) return;
        TOPOLOGY_GUI_CHECK(a->book.count==4 && s->id==topology_enriched && !a->analysis.source_version && s->source_versions.high_water==1 && s->source_versions.count==1 && s->redo_actions_count==1 && a->book.command_revision==topology_binding_revision+4);
        TOPOLOGY_GUI_CHECK(!strcmp(cells_get(&s->cells,20,200),"Topology note") && a->grid.widths[200]==1199.75f && a->page.total==6);
        topology_page=a->page.cells; topology_request=a->request_id; topology_analysis_revision=a->analysis_revision;
        grid_select_cell(&a->grid,0,2,false); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 18:
        if (a->source_format.active) return;
        /* Returning to a bound sheet may verify RAW under a new job ID; the
           primary query revision and actual Page remain unchanged. */
        TOPOLOGY_GUI_CHECK(s->id==topology_enriched && a->analysis.source_version==1 && s->source_versions.high_water==1 && !s->redo_actions_count && s->undo_actions_count==1 && s->undo_actions[0]==EDIT_ANALYSIS_FORMAT && (active_format(a).flags&FORMAT_BOLD));
        TOPOLOGY_GUI_CHECK(a->book.command_revision==topology_binding_revision+5 && a->page.cells==topology_page && a->analysis_revision==topology_analysis_revision && a->request_id>=topology_request);
        if (!topology_menu(a,topology_enriched,1)) return; break;
    case 19:
        if (a->source_format.active) return;
        TOPOLOGY_GUI_CHECK(a->book.count==5 && s->id!=topology_enriched && !s->objects.count && !s->objects.next_id && !s->undo_actions_count && a->analysis.source_version==1 && s->source_versions.high_water==1);
        topology_objects=s->id; topology_object_revision=a->book.command_revision; topology_page=a->page.cells;
        a->selected_object=-1; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        grid_select_cell(&a->grid,30,8,false); ui_note_key_press(KEY_P,DX_MOD_SUPER|DX_MOD_ALT); break;
    case 20:
        TOPOLOGY_GUI_CHECK(s->id==topology_objects && s->objects.count==1 && s->objects.items[0].kind==OBJECT_PIVOT && s->objects.next_id==1 && s->undo_actions_count==1 && (worksheet_edit_effects(s->undo_actions[0])&4));
        topology_object_high=s->objects.next_id;
        TOPOLOGY_GUI_CHECK(a->book.command_revision==topology_object_revision+1 && a->page.cells==topology_page && a->analysis.source_version==1);
        a->pivot_focus=a->chrome_focus=0; ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 21:
        TOPOLOGY_GUI_CHECK(s->id==topology_objects && !s->objects.count && s->objects.next_id==topology_object_high && s->redo_actions_count==1 && s->objects.redo_count==1 && a->book.command_revision==topology_object_revision+2 && a->page.cells==topology_page);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 22: {
        WorkbookLifecyclePresence selected;
        TOPOLOGY_GUI_CHECK(a->book.count==4 && s->id==topology_enriched && a->book.command_revision==topology_object_revision+3);
        TOPOLOGY_GUI_CHECK(workbook_lifecycle_presence(&a->book,true,&selected) && selected.sheet==topology_objects && selected.detached && selected.packet && selected.worksheet->objects.next_id==topology_object_high && selected.worksheet->objects.redo_count==1 && selected.worksheet->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 23:
        if (a->source_format.active) return;
        TOPOLOGY_GUI_CHECK(a->book.count==5 && s->id==topology_objects && !s->objects.count && s->objects.next_id==topology_object_high && s->redo_actions_count==1 && a->analysis.source_version==1 && s->source_versions.high_water==1 && a->book.command_revision==topology_object_revision+4);
        topology_page=a->page.cells; ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 24:
        TOPOLOGY_GUI_CHECK(s->id==topology_objects && s->objects.count==1 && s->objects.items[0].id==topology_object_high && s->objects.next_id==topology_object_high && !s->redo_actions_count && a->book.command_revision==topology_object_revision+5);
        TOPOLOGY_GUI_CHECK(a->page.cells==topology_page && a->page.total==6 && a->page.cells && a->page.offset==0 && a->page.row_count>=2 && a->page.cells[1].is_null && !a->page.cells[4].is_null && !strcmp(a->page.cells[4].text,"NULL") && a->analysis.source_version==1 && s->source_versions.high_water==1);
        TOPOLOGY_GUI_CHECK(!strcmp(cells_get(&s->cells,20,200),"Topology note") && s->view.widths[200]==1199.75f);
        printf("PASS: native sheet topology workflow (25 stages, %u checks)\n",topology_checks); topology_result=1; break;
    }
    topology_stage++;
}
int app_test_sheet_topology_result(void) { return topology_result; }
#undef TOPOLOGY_GUI_CHECK
