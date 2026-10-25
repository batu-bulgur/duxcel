/* Actual imported-record and manual-space inputs share one effect8 owner. */
static unsigned source_spatial_stage,source_spatial_wait,source_spatial_checks;
static int source_spatial_result;
static uint64_t source_spatial_revision,source_spatial_order,source_spatial_request;
static const Cell *source_spatial_page;
static SourceVersions source_spatial_ledger;
#define SOURCE_SPATIAL_CHECK(x) do { source_spatial_checks++; if (!(x)) { fprintf(stderr,"FAIL source spatial stage %u:%d: %s (%s)\n",source_spatial_stage,__LINE__,#x,a->message ? a->message : ""); source_spatial_result=-1; return; } } while (0)
static bool source_spatial_unchanged(const App *a) {
    return !memcmp(&source_spatial_ledger,&a->book.sheets[a->book.active].source_versions,sizeof(source_spatial_ledger)) &&
        a->request_id==source_spatial_request && a->page.cells==source_spatial_page && a->analysis.source_version==1;
}
static void source_spatial_format_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (source_spatial_result || !s || a->busy || a->io_busy || a->source_format.active || ++source_spatial_wait<20) return;
    source_spatial_wait=0; SOURCE_SPATIAL_CHECK(a->modal!=MODAL_ERROR);
    switch (source_spatial_stage) {
    case 0:
        SOURCE_SPATIAL_CHECK(a->page.total==5 && a->page.column_count==6 && !s->formats.source_aware);
        a->statistics_enabled=false; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        grid_select_cell(&a->grid,0,1,false); ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    case 1: {
        SOURCE_SPATIAL_CHECK(s->formats.source_aware && a->analysis.source_version==1 && s->source_versions.count==1 && imported_cell_format(a,0,1).flags==FORMAT_BOLD);
        source_spatial_ledger=s->source_versions; source_spatial_page=a->page.cells; source_spatial_request=a->request_id;
        source_spatial_revision=a->book.command_revision; source_spatial_order=worksheet_history_high_water();
        grid_select_cell(&a->grid,0,1,false); a->grid.end_row=DX_SHEET_ROWS-1;
        CellFormat style=format_default(); style.flags=FORMAT_ITALIC;
        a->source_format.bytes=SIZE_MAX;
        SOURCE_SPATIAL_CHECK(!apply_format(a,FORMAT_ITALIC,style) && a->book.command_revision==source_spatial_revision && worksheet_history_high_water()==source_spatial_order && source_spatial_unchanged(a));
        a->source_format.bytes=0; ui_note_key_press(KEY_I,DX_MOD_SUPER); break;
    }
    case 2:
        SOURCE_SPATIAL_CHECK(source_spatial_unchanged(a) && a->book.command_revision==source_spatial_revision+1 && worksheet_history_high_water()==source_spatial_order+1);
        SOURCE_SPATIAL_CHECK(imported_cell_format(a,0,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && s->undo_actions[s->undo_actions_count-1]==EDIT_FORMAT);
        grid_select_cell(&a->grid,20,20,false); ui_note_key_press(KEY_U,DX_MOD_SUPER); break;
    case 3:
        SOURCE_SPATIAL_CHECK(source_spatial_unchanged(a) && a->book.command_revision==source_spatial_revision+2 && worksheet_history_high_water()==source_spatial_order+2);
        SOURCE_SPATIAL_CHECK(format_at(&s->formats,20,20).flags==FORMAT_UNDERLINE && imported_cell_format(a,0,1).flags==(FORMAT_BOLD|FORMAT_ITALIC));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 4: {
        SOURCE_SPATIAL_CHECK(source_spatial_unchanged(a) && a->book.command_revision==source_spatial_revision+3 && !format_at(&s->formats,20,20).flags && s->formats.redo_count==1);
        size_t redo=s->redo_actions_count; uint64_t revision=a->book.command_revision,order=worksheet_history_high_water(); bool dirty=a->book.dirty;
        grid_select_cell(&a->grid,0,1,false); a->grid.end_row=DX_SHEET_ROWS-1;
        CellFormat style=format_default(); style.flags=FORMAT_ITALIC;
        SOURCE_SPATIAL_CHECK(apply_format(a,FORMAT_ITALIC,style) && a->book.command_revision==revision && worksheet_history_high_water()==order && a->book.dirty==dirty && s->redo_actions_count==redo && s->formats.redo_count==1 && source_spatial_unchanged(a));
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    }
    case 5:
        SOURCE_SPATIAL_CHECK(source_spatial_unchanged(a) && a->book.command_revision==source_spatial_revision+4 && worksheet_history_high_water()==source_spatial_order+2);
        SOURCE_SPATIAL_CHECK(format_at(&s->formats,20,20).flags==FORMAT_UNDERLINE && imported_cell_format(a,0,1).flags==(FORMAT_BOLD|FORMAT_ITALIC));
        source_spatial_result=1; printf("PASS: native source spatial format workflow (6 stages, %u checks)\n",source_spatial_checks); return;
    }
    source_spatial_stage++;
}
int app_test_source_spatial_format_result(void) { return source_spatial_result; }
unsigned app_test_source_spatial_format_stage(void) { return source_spatial_stage; }
