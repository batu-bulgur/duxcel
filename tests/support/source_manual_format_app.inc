/* Current manual-first mode owns only the new after-state, including replay. */
static unsigned source_manual_stage,source_manual_wait,source_manual_checks;
static int source_manual_result;
static uint64_t source_manual_revision,source_manual_order,source_manual_request;
static const Cell *source_manual_page;
static SourceVersions source_manual_ledger;
#define SOURCE_MANUAL_CHECK(x) do { source_manual_checks++; if (!(x)) { fprintf(stderr,"FAIL source manual-first stage %u:%d: %s (%s)\n",source_manual_stage,__LINE__,#x,a->message ? a->message : ""); source_manual_result=-1; return; } } while (0)
static bool source_manual_immutable(const App *a) {
    return a->page.cells==source_manual_page && a->request_id==source_manual_request &&
        !memcmp(&source_manual_ledger,&a->book.sheets[a->book.active].source_versions,sizeof(source_manual_ledger));
}
static void source_manual_format_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (source_manual_result || !s || a->busy || a->io_busy || a->source_format.active || ++source_manual_wait<20) return;
    source_manual_wait=0; SOURCE_MANUAL_CHECK(a->modal!=MODAL_ERROR);
    switch (source_manual_stage) {
    case 0: {
        SOURCE_MANUAL_CHECK(a->page.total==5 && a->page.column_count==6 && !s->formats.source_aware && !s->source_versions.count);
        a->statistics_enabled=false; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        grid_select_cell(&a->grid,30,20,false); source_manual_revision=a->book.command_revision; source_manual_order=worksheet_history_high_water();
        CellFormat style=format_default(); style.flags=FORMAT_BOLD; a->source_format.bytes=SIZE_MAX;
        SOURCE_MANUAL_CHECK(!apply_format(a,FORMAT_BOLD,style) && !s->formats.source_aware && a->book.command_revision==source_manual_revision && worksheet_history_high_water()==source_manual_order && !s->formats.count);
        a->source_format.bytes=0; ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    }
    case 1:
        SOURCE_MANUAL_CHECK(s->formats.source_aware && !a->analysis.source_version && !s->source_versions.count && format_at(&s->formats,30,20).flags==FORMAT_BOLD && !imported_cell_format(a,0,1).flags);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+1 && worksheet_history_high_water()==source_manual_order+1 && s->formats.undo_count==1 && !s->formats.undo[0].source_aware);
        grid_select_cell(&a->grid,0,1,false); ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    case 2:
        SOURCE_MANUAL_CHECK(a->analysis.source_version==1 && s->source_versions.count==1 && imported_cell_format(a,0,1).flags==FORMAT_BOLD && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+2 && worksheet_history_high_water()==source_manual_order+2 && !s->formats.undo[0].source_aware);
        grid_select_cell(&a->grid,0,1,false); ui_note_key_press(KEY_I,DX_MOD_SUPER); break;
    case 3:
        SOURCE_MANUAL_CHECK(imported_cell_format(a,0,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && !imported_cell_format(a,1,1).flags && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+3 && worksheet_history_high_water()==source_manual_order+3 && !s->formats.undo[0].source_aware);
        source_manual_ledger=s->source_versions; source_manual_page=a->page.cells; source_manual_request=a->request_id;
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && a->analysis.source_version==1 && imported_cell_format(a,0,1).flags==FORMAT_BOLD && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+4); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && !a->analysis.source_version && s->formats.source_aware && !imported_cell_format(a,0,1).flags && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+5); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && !a->analysis.source_version && !s->formats.source_aware && !format_at(&s->formats,30,20).flags && !imported_cell_format(a,0,1).flags);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+6 && s->formats.redo_count==3); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && !a->analysis.source_version && s->formats.source_aware && format_at(&s->formats,30,20).flags==FORMAT_BOLD && !imported_cell_format(a,0,1).flags);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+7 && !s->formats.undo[0].source_aware); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && a->analysis.source_version==1 && imported_cell_format(a,0,1).flags==FORMAT_BOLD && !imported_cell_format(a,1,1).flags && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+8); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9:
        SOURCE_MANUAL_CHECK(source_manual_immutable(a) && imported_cell_format(a,0,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && !imported_cell_format(a,1,1).flags && format_at(&s->formats,30,20).flags==FORMAT_BOLD);
        SOURCE_MANUAL_CHECK(a->book.command_revision==source_manual_revision+9 && worksheet_history_high_water()==source_manual_order+3 && !s->formats.redo_count && !s->formats.undo[0].source_aware);
        source_manual_result=1; printf("PASS: native source manual-first workflow (10 stages, %u checks)\n",source_manual_checks); return;
    }
    source_manual_stage++;
}
int app_test_source_manual_format_result(void) { return source_manual_result; }
unsigned app_test_source_manual_format_stage(void) { return source_manual_stage; }
