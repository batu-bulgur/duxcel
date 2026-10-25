/* Actual imported record-format action followed by complete layout history. */
static struct {
    SourceVersions ledger; SourceVersion entry;
    uint64_t raw_hash,record,version,base,request,width_revision;
    Cell *cells[4]; Column *columns[4]; uint64_t values[4];
} source_record_range;
#define SRR_CHECK(x) do { source_results_checks++; if (!(x)) { fprintf(stderr,"FAIL active record range stage %u:%d: %s (%s)\n",source_results_stage,__LINE__,#x,a->message ? a->message : ""); source_results_result=-1; return false; } } while (0)
static uint64_t source_record_hash(const void *data,size_t bytes) {
    const unsigned char *p=data; uint64_t hash=UINT64_C(1469598103934665603);
    for (size_t i=0;i<bytes;i++) { hash^=p[i]; hash*=UINT64_C(1099511628211); } return hash;
}
static uint64_t source_record_values(const Page *page) {
    uint64_t hash=UINT64_C(1469598103934665603);
    for (size_t i=0;i<page->row_count*page->column_count;i++) {
        const Cell *cell=page->cells+i; hash^=cell->is_null ? 1u : 2u; hash*=UINT64_C(1099511628211);
        if (!cell->text) { hash^=3u; hash*=UINT64_C(1099511628211); }
        else { size_t bytes=strlen(cell->text); hash^=source_record_hash(cell->text,bytes); hash*=UINT64_C(1099511628211); hash^=bytes; }
    }
    return hash;
}
static bool source_record_range_check(App *a,uint64_t row,size_t col) {
    Worksheet *s=active_sheet(a); const SheetObject *table=s->objects.items;
    SRR_CHECK(table->row==row && table->col==col && table->source.source_version==source_record_range.version && !a->source_format.active);
    SRR_CHECK(!memcmp(&s->source_versions,&source_record_range.ledger,sizeof(s->source_versions)));
    const SourceVersion *entry=source_versions_find(&s->source_versions,source_record_range.version);
    SRR_CHECK(entry && !memcmp(entry,&source_record_range.entry,sizeof(*entry)) && source_record_hash(entry->raw.data,entry->raw.bytes)==source_record_range.raw_hash);
    size_t found=0;
    for (size_t i=0;i<s->formats.count;i++) if (s->formats.items[i].selector==FORMAT_RECORDS) {
        const FormatPatch *patch=s->formats.items+i; found++;
        SRR_CHECK(patch->record_count==1 && patch->record_ids[0]==source_record_range.record && patch->field_count==1 && patch->fields[0]==5 && patch->guard_count==1 && (patch->value.flags&FORMAT_BOLD));
        const FormatScope *scope=s->formats.scopes+patch->guard_ids[0];
        SRR_CHECK(scope->owner==table->id && scope->bytes==entry->raw.bytes && !memcmp(scope->definition,entry->raw.data,entry->raw.bytes));
    }
    SRR_CHECK(found==1 && (source_format_value(a,row+4,col+5).flags&FORMAT_BOLD) && !(source_format_value(a,row+5,col+5).flags&FORMAT_BOLD));
    SRR_CHECK(a->request_id==source_record_range.request && a->page.cells==source_record_range.cells[0] && a->page.columns==source_record_range.columns[0] && source_record_values(&a->page)==source_record_range.values[0]);
    for (size_t i=0;i<3;i++) {
        const Page *page=&a->objects[i].page; RangeResultProof proof={0};
        SRR_CHECK(object_ready(a,i) && page->cells==source_record_range.cells[i+1] && page->columns==source_record_range.columns[i+1] && source_record_values(page)==source_record_range.values[i+1]);
        SRR_CHECK(app_range_result(a,s->objects.items[i].id,&proof) && proof.ready && proof.owner==a->objects+i && proof.page==page && proof.query_epoch==s->objects.items[i].revision && proof.source_epoch==table->revision && proof.definition==&table->source && proof.request==source_record_range.request);
    }
    return true;
}
static void source_record_rows_menu(App *a) {
    grid_select_cell(&a->grid,1000,0,false); a->grid.selection=SELECT_ROWS; a->grid.end_row=1001;
    a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=0; a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0);
}
static void source_record_columns_menu(App *a) {
    grid_select_cell(&a->grid,0,16,false); a->grid.selection=SELECT_COLUMNS;
    a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=2; a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0);
}
static bool source_record_range_step(App *a,Worksheet *s) {
    switch (source_results_stage) {
    case 13:
        free(source_results_target); source_results_target=NULL;
        a->selected_object=0; a->pivot_reveal_object=true; a->chrome_focus=a->pivot_focus=0;
        grid_select_cell(&a->grid,s->objects.items[0].row+4,s->objects.items[0].col+5,false); grid_reveal_cell(&a->grid,&a->page,a->ui.scale*a->grid.zoom); break;
    case 14: {
        SourceFormatOwner owner; SRR_CHECK(source_format_owner(a,s->objects.items[0].id,&owner) && source_format_raw_ready(a,&owner));
#ifdef DUXCEL_SOURCE_RECORD_PARQUET
        SRR_CHECK(s->objects.items[0].source.kind==SOURCE_PARQUET && a->objects[0].page.columns[5].type==DUCKDB_TYPE_DECIMAL);
#else
        SRR_CHECK(s->objects.items[0].source.kind==SOURCE_CSV);
#endif
        uint64_t record=0; SRR_CHECK(page_record_id(&a->objects[0].page,2,&record) && record);
        ui_note_key_press(KEY_B,DX_MOD_SUPER); break;
    }
    case 15: {
        if (a->source_format.active) return false;
        SheetObject *table=s->objects.items; source_record_range.version=table->source.source_version;
        const SourceVersion *entry=source_versions_find(&s->source_versions,source_record_range.version);
        SRR_CHECK(source_record_range.version && entry && entry->owner==table->id && entry->kind==table->source.kind && entry->raw.bytes && source_format_owner_verified(a,table->id));
        SRR_CHECK(page_record_id(&a->objects[0].page,2,&source_record_range.record) && (source_format_value(a,table->row+4,table->col+5).flags&FORMAT_BOLD));
        source_record_range.ledger=s->source_versions; source_record_range.entry=*entry; source_record_range.raw_hash=source_record_hash(entry->raw.data,entry->raw.bytes); break;
    }
    case 16: {
        char *error=NULL; SRR_CHECK(cells_set(&s->cells,329,15,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS); free(error);
        grid_select_cell(&a->grid,329,15,false); CellFormat color=format_default(); color.color=3; SRR_CHECK(apply_format(a,FORMAT_COLOR,color));
        grid_ensure_columns(&a->grid,16384); grid_set_width(&a->grid,16,123); grid_set_width(&a->grid,16383,333); SRR_CHECK(commit_grid_widths(a));
        source_record_range.base=a->book.command_revision; source_record_range.request=a->request_id; source_record_range.width_revision=s->width_revision;
        const Page *pages[]={&a->page,&a->objects[0].page,&a->objects[1].page,&a->objects[2].page};
        for (size_t i=0;i<4;i++) { source_record_range.cells[i]=pages[i]->cells; source_record_range.columns[i]=pages[i]->columns; source_record_range.values[i]=source_record_values(pages[i]); }
        SRR_CHECK(source_record_range_check(a,330,16)); break;
    }
    case 17: {
        grid_select_cell(&a->grid,329,15,false); a->grid.end_row=641; a->grid.end_col=27; begin_move_range(a,false); SRR_CHECK(a->modal==MODAL_MOVE_RANGE);
        char address[48]; grid_format_address(1000,16,true,address); input_set(&a->input,address); a->range_move_focus=1; ui_note_key_press(KEY_ENTER,0); break;
    }
    case 18:
        SRR_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_record_range.base+1 && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==14 && source_record_range_check(a,1001,17));
        SRR_CHECK(!strcmp(cells_get(&s->cells,1000,16),"9007199254740993.0123456789") && format_at(&s->formats,1000,16).color==3 && s->width_revision==source_record_range.width_revision); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 19: SRR_CHECK(source_record_range_check(a,330,16) && !strcmp(cells_get(&s->cells,329,15),"9007199254740993.0123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 20: SRR_CHECK(source_record_range_check(a,1001,17)); source_record_rows_menu(a); break;
    case 21:
        SRR_CHECK(source_record_range_check(a,1003,17) && !strcmp(cells_get(&s->cells,1002,16),"9007199254740993.0123456789") && format_at(&s->formats,1002,16).color==3 && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==14 && s->width_revision==source_record_range.width_revision); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 22: SRR_CHECK(source_record_range_check(a,1001,17)); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 23: SRR_CHECK(source_record_range_check(a,1003,17)); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 24: SRR_CHECK(source_record_range_check(a,1001,17)); source_record_columns_menu(a); break;
    case 25:
        SRR_CHECK(source_record_range_check(a,1001,18) && !strcmp(cells_get(&s->cells,1000,17),"9007199254740993.0123456789") && format_at(&s->formats,1000,17).color==3 && worksheet_edit_effects(s->undo_actions[s->undo_actions_count-1])==30);
        SRR_CHECK(s->view.widths[17]==123 && s->view.widths[16383]==96 && a->grid.widths[16383]==96); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 26: SRR_CHECK(source_record_range_check(a,1001,17) && s->view.widths[16]==123 && s->view.widths[16383]==333 && a->grid.widths[16383]==333); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 27: SRR_CHECK(source_record_range_check(a,1001,18) && s->view.widths[16383]==96); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 28: SRR_CHECK(source_record_range_check(a,1001,17) && s->view.widths[16383]==333); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 29: SRR_CHECK(source_record_range_check(a,330,16) && !strcmp(cells_get(&s->cells,329,15),"9007199254740993.0123456789")); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 30:
        SRR_CHECK(source_record_range_check(a,1001,17) && a->book.command_revision==source_record_range.base+13 && !strcmp(cells_get(&s->cells,1000,16),"9007199254740993.0123456789") && format_at(&s->formats,1000,16).color==3 && s->view.widths[16383]==333 && !a->source_format.active);
        source_results_result=1;
#ifdef DUXCEL_SOURCE_RECORD_PARQUET
        printf("PASS: native Parquet record range workflow (31 stages, %u checks)\n",source_results_checks);
#else
        printf("PASS: native CSV record range workflow (31 stages, %u checks)\n",source_results_checks);
#endif
        break;
    }
    return true;
}
#undef SRR_CHECK
