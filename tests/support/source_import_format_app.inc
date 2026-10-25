/* Real manual style -> first Table import -> first/bound source formatting. */
static unsigned source_import_stage,source_import_wait,source_import_checks;
static int source_import_result;
static uint64_t source_import_object,source_import_allocator,source_import_order,source_import_revision;
static FormatPatch *source_import_manual;
static SourceVersions source_import_ledger;
#define SOURCE_IMPORT_CHECK(x) do { source_import_checks++; if (!(x)) { fprintf(stderr,"FAIL source import format stage %u:%d: %s (%s)\n",source_import_stage,__LINE__,#x,a->message ? a->message : ""); source_import_result=-1; return; } } while (0)
static bool source_import_immutable(const Worksheet *s) {
    return !memcmp(&source_import_ledger,&s->source_versions,sizeof(source_import_ledger)) && s->objects.next_id==source_import_allocator;
}
static void source_import_format_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (source_import_result || !s || a->busy || a->io_busy || a->source_format.active || ++source_import_wait<20) return;
    source_import_wait=0; SOURCE_IMPORT_CHECK(a->modal!=MODAL_ERROR);
    switch (source_import_stage) {
    case 0:
        a->statistics_enabled=false; a->pivot_focus=a->chrome_focus=0; a->ribbon_focus=false;
        new_sheet(a); s=active_sheet(a); SOURCE_IMPORT_CHECK(a->analysis.kind==SOURCE_NONE && !s->objects.count && !s->formats.source_aware);
        grid_select_cell(&a->grid,20,20,false); ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    case 1:
        SOURCE_IMPORT_CHECK(!s->formats.source_aware && format_at(&s->formats,20,20).flags==FORMAT_BOLD);
        source_import_manual=s->formats.items;
        grid_select_cell(&a->grid,0,0,false); SOURCE_IMPORT_CHECK(import_table(a,"tests/fixtures/transactions.csv"));
        SOURCE_IMPORT_CHECK(s->objects.count==1 && s->formats.source_aware && s->formats.undo[1].items==source_import_manual && !s->formats.undo[1].source_aware);
        source_import_object=s->objects.items[0].id; source_import_allocator=s->objects.next_id; break;
    case 2:
        if (!object_ready(a,0)) return;
        SOURCE_IMPORT_CHECK(a->objects[0].page.total==5 && a->objects[0].page.column_count==6 && !s->objects.items[0].source.source_version);
        grid_select_cell(&a->grid,s->objects.items[0].row+2,s->objects.items[0].col+1,false); a->selected_object=0;
        ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    case 3:
        SOURCE_IMPORT_CHECK(s->objects.items[0].source.source_version==1 && s->source_versions.count==1 && imported_cell_format(a,2,1).flags==FORMAT_BOLD && !imported_cell_format(a,3,1).flags);
        SOURCE_IMPORT_CHECK(format_at(&s->formats,20,20).flags==FORMAT_BOLD && !s->formats.undo[1].source_aware && s->formats.undo[1].items==source_import_manual);
        ui_note_key_press(KEY_I,DX_MOD_SUPER); break;
    case 4:
        SOURCE_IMPORT_CHECK(imported_cell_format(a,2,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && !imported_cell_format(a,3,1).flags && format_at(&s->formats,20,20).flags==FORMAT_BOLD);
        source_import_ledger=s->source_versions; source_import_order=worksheet_history_high_water(); source_import_revision=a->book.command_revision;
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && s->objects.items[0].source.source_version==1 && imported_cell_format(a,2,1).flags==FORMAT_BOLD);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+1); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 6:
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && s->formats.source_aware && !s->objects.items[0].source.source_version && !imported_cell_format(a,2,1).flags && format_at(&s->formats,20,20).flags==FORMAT_BOLD);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+2); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 7:
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && !s->objects.count && !s->formats.source_aware && s->formats.items==source_import_manual && format_at(&s->formats,20,20).flags==FORMAT_BOLD);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+3); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 8:
        if (!object_ready(a,0)) return;
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && s->objects.count==1 && s->objects.items[0].id==source_import_object && !s->objects.items[0].source.source_version && s->formats.source_aware);
        SOURCE_IMPORT_CHECK(format_at(&s->formats,20,20).flags==FORMAT_BOLD && !s->formats.undo[1].source_aware && s->formats.undo[1].items==source_import_manual && !imported_cell_format(a,2,1).flags);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+4); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 9:
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && s->objects.items[0].source.source_version==1 && imported_cell_format(a,2,1).flags==FORMAT_BOLD && !imported_cell_format(a,3,1).flags);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+5); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 10:
        SOURCE_IMPORT_CHECK(source_import_immutable(s) && imported_cell_format(a,2,1).flags==(FORMAT_BOLD|FORMAT_ITALIC) && !imported_cell_format(a,3,1).flags && format_at(&s->formats,20,20).flags==FORMAT_BOLD);
        SOURCE_IMPORT_CHECK(a->book.command_revision==source_import_revision+6 && worksheet_history_high_water()==source_import_order && !s->formats.redo_count && !s->formats.undo[1].source_aware);
        source_import_result=1; printf("PASS: native source import format workflow (11 stages, %u checks)\n",source_import_checks); return;
    }
    source_import_stage++;
}
int app_test_source_import_format_result(void) { return source_import_result; }
unsigned app_test_source_import_format_stage(void) { return source_import_stage; }
