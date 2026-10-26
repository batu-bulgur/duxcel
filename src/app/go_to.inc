/* The draft borrows App.input only after all other editors/panes relinquish it.
   Session identities are transient fences, never a document edit. */
static bool app_go_to_ready(App *a) {
    return app_grid_boundary_ready(a) && !historical(a) && !a->step_preview.active && !a->file_menu && !a->key_tips &&
        !a->chrome_focus && !a->pivot_focus && !a->pivot_search_focus && !a->ribbon_focus && !a->left_drawer &&
        a->grid.selection==SELECT_CELLS && !a->grid.bounded && !a->grid.dragging && !a->grid.moving && a->grid.resizing<0 && !a->grid.scrollbar;
}
static bool go_to_owner(App *a) {
    Worksheet *s=active_sheet(a);
    return app_go_to_ready(a) && s==a->go_to.sheet && a->book.id.high==a->go_to.document.high && a->book.id.low==a->go_to.document.low &&
        a->book.edit_session_id==a->go_to.document_session && s->edit_session_id==a->go_to.sheet_session && s->id==a->go_to.sheet_id &&
        a->book.command_revision==a->go_to.revision && s->cells.revision==a->go_to.cell_revision && s->formats.revision==a->go_to.format_revision &&
        s->width_revision==a->go_to.width_revision && a->request_id==a->go_to.request && s->cells.cells==a->go_to.cells &&
        s->cells.count==a->go_to.count && s->cells.capacity==a->go_to.capacity;
}
static void begin_go_to(App *a) {
    if (a->modal!=MODAL_NONE || !app_go_to_ready(a)) return;
    char *error=NULL;
    if (!workbook_prepare_command_session(&a->book,a->book.active,&error)) { status(a,error ? error : "Cannot open Go to cell."); free(error); return; }
    Worksheet *s=active_sheet(a);
    a->go_to.sheet=s; a->go_to.cells=s->cells.cells; a->go_to.document=a->book.id;
    a->go_to.document_session=a->book.edit_session_id; a->go_to.sheet_session=s->edit_session_id; a->go_to.sheet_id=s->id;
    a->go_to.revision=a->book.command_revision; a->go_to.cell_revision=s->cells.revision; a->go_to.format_revision=s->formats.revision;
    a->go_to.width_revision=s->width_revision; a->go_to.request=a->request_id; a->go_to.count=s->cells.count; a->go_to.capacity=s->cells.capacity;
    a->go_to.focus=0; a->go_to.message[0]=0;
    char address[48]; grid_format_address(a->grid.row,a->grid.col,false,address);
    input_set(&a->input,address); a->input.select_all=true; a->modal=MODAL_GO_TO; a->consume_grid_input=true;
}
static void apply_go_to(App *a) {
    if (!go_to_owner(a)) { snprintf(a->go_to.message,sizeof(a->go_to.message),"Worksheet changed. Cancel and reopen Go to cell."); return; }
    GridGoToTarget target; GridGoToStatus result=grid_go_to_parse(a->input.text,a->input.length,false,&target);
    if (result==GRID_GO_TO_READY) result=grid_go_to_apply(&a->grid,&a->page,a->ui.scale,target);
    if (result==GRID_GO_TO_APPLIED || result==GRID_GO_TO_NOOP) { a->modal=MODAL_NONE; a->consume_grid_input=true; return; }
    snprintf(a->go_to.message,sizeof(a->go_to.message),"%s",result==GRID_GO_TO_LIMIT ? "Address too long or column capacity unavailable." : "Enter one cell address, for example A1 or $H$12.");
}
static void draw_go_to(App *a) {
    UIContext *u=&a->ui;
    if (ui_pressed(KEY_ESCAPE)) { a->modal=MODAL_NONE; a->consume_grid_input=true; return; }
    UIGoToLayout l=ui_go_to_layout((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},u->scale);
    if (!l.valid) return;
    if (ui_pressed(KEY_TAB)) a->go_to.focus=(a->go_to.focus+(ui_key_shift(KEY_TAB) ? 2 : 1))%3;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){20,32,45,70}); DrawRectangleRec(l.panel,UI_BG); DrawRectangleLinesEx(l.panel,1,UI_BORDER);
    ui_text_clip_weight(u,"Go to cell",l.title,17,UI_TEXT,false,UI_SEMIBOLD);
    ui_label(u,"Cell address on this worksheet",l.label,11,UI_MUTED,false,UI_REGULAR);
    if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui_mouse_position(),l.input)) || ui_gesture_starts_in(l.input)) a->go_to.focus=0;
    if (!a->go_to.focus) input_update(&a->input);
    input_draw(u,&a->input,l.input);
    ui_text_clip(u,a->go_to.message[0] ? a->go_to.message : "Single cell only. Data and formatting stay unchanged.",l.message,10,a->go_to.message[0] ? (Color){160,43,43,255} : UI_MUTED,false);
    bool cancel=ui_button(u,l.cancel,"Cancel",a->go_to.focus==1,true);
    bool go=ui_button(u,l.go,"Go",a->go_to.focus==2,go_to_owner(a));
    REVIEW_AUTHOR_PANEL(a,"go_to","Go to cell",l.panel,l.title);
    REVIEW_AUTHOR_INPUT(a,"go_to","address","grid.go_to","Cell address",l.input,l.panel,true,!a->go_to.focus,&a->input);
    REVIEW_AUTHOR_ITEM(a,"go_to","apply","grid.go_to","button","Go",l.go,l.panel,go_to_owner(a),false,false,NULL);
    REVIEW_AUTHOR_ITEM(a,"go_to","cancel","dialog.cancel","button","Cancel",l.cancel,l.panel,true,false,false,NULL);
    if (cancel || (ui_pressed(KEY_ENTER) && a->go_to.focus==1)) { a->modal=MODAL_NONE; a->consume_grid_input=true; }
    else if (go || ui_pressed(KEY_ENTER)) apply_go_to(a);
}
