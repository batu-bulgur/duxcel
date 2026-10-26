/* Long-lived transient modal target. C01's frame-stack sparse borrowing stays
   separate; neither target is a document edit or a durable identity owner. */
static bool manual_navigation_ready(App *a,bool ribbon) {
    return app_grid_boundary_ready(a) && !historical(a) && !a->step_preview.active && !a->file_menu && !a->key_tips &&
        !a->chrome_focus && !a->pivot_focus && !a->pivot_search_focus && (ribbon || !a->ribbon_focus) && !a->left_drawer &&
        a->grid.selection==SELECT_CELLS && !a->grid.bounded && !a->grid.dragging && !a->grid.moving && a->grid.resizing<0 && !a->grid.scrollbar;
}
static bool manual_navigation_check(App *a) {
    Worksheet *s=active_sheet(a);
    return manual_navigation_ready(a,false) && s==a->manual_navigation.sheet && a->book.id.high==a->manual_navigation.document.high && a->book.id.low==a->manual_navigation.document.low &&
        a->book.edit_session_id==a->manual_navigation.document_session && s->edit_session_id==a->manual_navigation.sheet_session && s->id==a->manual_navigation.sheet_id &&
        a->book.command_revision==a->manual_navigation.revision && s->cells.revision==a->manual_navigation.cell_revision && s->formats.revision==a->manual_navigation.format_revision &&
        s->width_revision==a->manual_navigation.width_revision && a->request_id==a->manual_navigation.request && s->cells.cells==a->manual_navigation.cells &&
        s->cells.count==a->manual_navigation.count && s->cells.capacity==a->manual_navigation.capacity;
}
static bool manual_navigation_open(App *a,bool ribbon) {
    if ((a->modal!=MODAL_NONE && !(ribbon && a->modal==MODAL_COMMANDS)) || !manual_navigation_ready(a,ribbon)) return false;
    char *error=NULL;
    if (!workbook_prepare_command_session(&a->book,a->book.active,&error)) { status(a,error ? error : "Cannot open worksheet navigation."); free(error); return false; }
    Worksheet *s=active_sheet(a);
    a->manual_navigation.sheet=s; a->manual_navigation.cells=s->cells.cells; a->manual_navigation.document=a->book.id;
    a->manual_navigation.document_session=a->book.edit_session_id; a->manual_navigation.sheet_session=s->edit_session_id; a->manual_navigation.sheet_id=s->id;
    a->manual_navigation.revision=a->book.command_revision; a->manual_navigation.cell_revision=s->cells.revision; a->manual_navigation.format_revision=s->formats.revision;
    a->manual_navigation.width_revision=s->width_revision; a->manual_navigation.request=a->request_id; a->manual_navigation.count=s->cells.count; a->manual_navigation.capacity=s->cells.capacity;
    a->ribbon_focus=false; a->consume_grid_input=true; return true;
}
