/* The complete owned sparse model is read only during this synchronous call.
   Resolve the current owner/fences before any captured span is dereferenced. */
static bool app_find_read(const void *context,size_t index,GridFindCell *out) {
    App *a=(App *)context;
    if (!manual_navigation_check(a)) return false;
    Worksheet *s=active_sheet(a); if (index>=s->cells.count || !s->cells.cells) return false;
    const SheetCell *cell=s->cells.cells+index;
    *out=(GridFindCell){cell->row,cell->col,true,cell->text!=NULL,cell->text,cell->text ? strlen(cell->text) : 0}; return true;
}
static void begin_find(App *a) {
    if (!manual_navigation_open(a,true)) return;
    input_set(&a->input,""); a->find.focus=0; a->find.whole_cell=false; a->find.message[0]=0;
    a->find.message_revision=a->input.revision; a->modal=MODAL_FIND;
}
static void apply_find(App *a,GridFindDirection direction) {
    if (!manual_navigation_check(a)) { snprintf(a->find.message,sizeof(a->find.message),"Worksheet changed. Close and reopen Find."); return; }
    GridFindInventory inventory={app_find_read,a,a->manual_navigation.count,GRID_BOUNDARY_CELLS_MAX,GRID_FIND_TEXT_BYTES}; GridFindResult result;
    GridFindStatus found=grid_manual_find(&inventory,a->grid.row,a->grid.col,a->input.text,a->input.length,direction,a->find.whole_cell ? GRID_FIND_WHOLE_CELL : GRID_FIND_CONTAINS,&result);
    if (!manual_navigation_check(a)) { snprintf(a->find.message,sizeof(a->find.message),"Worksheet changed. Close and reopen Find."); return; }
    if (found==GRID_FIND_FOUND) {
        GridGoToStatus moved=grid_go_to_apply(&a->grid,&a->page,a->ui.scale,(GridGoToTarget){result.row,result.column});
        if (moved==GRID_GO_TO_APPLIED || moved==GRID_GO_TO_NOOP) {
            char address[48]; grid_format_address(result.row,result.column,false,address);
            snprintf(a->find.message,sizeof(a->find.message),"%zu match%s · %s%s",result.matches,result.matches==1 ? "" : "es",address,result.wrapped ? " · wrapped" : ""); return;
        }
        snprintf(a->find.message,sizeof(a->find.message),"Column capacity unavailable. Selection unchanged."); return;
    }
    snprintf(a->find.message,sizeof(a->find.message),"%s",found==GRID_FIND_NOT_FOUND ? "No matching manual text." : found==GRID_FIND_LIMIT ? "Query or manual inventory exceeds its search limit." : "Enter nonempty literal text (case-sensitive).");
}
static void draw_find(App *a) {
    UIContext *u=&a->ui;
    if (ui_pressed(KEY_ESCAPE)) { a->modal=MODAL_NONE; a->consume_grid_input=true; return; }
    UIFindLayout l=ui_find_layout((Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},u->scale); if (!l.valid) return;
    if (ui_pressed(KEY_TAB)) a->find.focus=(a->find.focus+(ui_key_shift(KEY_TAB) ? 4 : 1))%5;
    if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(ui_mouse_position(),l.input)) || ui_gesture_starts_in(l.input)) a->find.focus=0;
    if (!a->find.focus) input_update(&a->input);
    if (a->find.message_revision!=a->input.revision) { a->find.message[0]=0; a->find.message_revision=a->input.revision; }
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){20,32,45,70}); DrawRectangleRec(l.panel,UI_BG); DrawRectangleLinesEx(l.panel,1,UI_BORDER);
    ui_text_clip_weight(u,"Find manual text",l.title,17,UI_TEXT,false,UI_SEMIBOLD);
    ui_label(u,"Literal authored text · case-sensitive",l.label,11,UI_MUTED,false,UI_REGULAR); input_draw(u,&a->input,l.input);
    bool whole=ui_tool_button(u,l.whole_cell,a->find.whole_cell ? "Whole cell: On" : "Whole cell: Off",a->find.whole_cell,true);
    ui_text_clip(u,a->find.message[0] ? a->find.message : "Manual values only. Sources and Replace are not searched.",l.message,11,UI_MUTED,false);
    bool enabled=manual_navigation_check(a) && a->input.length && a->input.length<=GRID_FIND_QUERY_BYTES;
    bool close=ui_button(u,l.close,"Close",a->find.focus==4,true),previous=ui_button(u,l.previous,"Previous",a->find.focus==2,enabled),next=ui_button(u,l.next,"Next",a->find.focus==3,enabled);
    REVIEW_AUTHOR_PANEL(a,"manual_find","Find manual text",l.panel,l.title);
    REVIEW_AUTHOR_INPUT(a,"manual_find","query","grid.find","Find text",l.input,l.panel,true,!a->find.focus,&a->input);
    REVIEW_AUTHOR_ITEM(a,"manual_find","whole_cell","grid.find.whole_cell","button","Whole cell",l.whole_cell,l.panel,true,a->find.whole_cell,a->find.focus==1,NULL);
    REVIEW_AUTHOR_ITEM(a,"manual_find","previous","grid.find.previous","button","Previous",l.previous,l.panel,enabled,false,a->find.focus==2,NULL);
    REVIEW_AUTHOR_ITEM(a,"manual_find","next","grid.find.next","button","Next",l.next,l.panel,enabled,false,a->find.focus==3,NULL);
    REVIEW_AUTHOR_ITEM(a,"manual_find","close","dialog.cancel","button","Close",l.close,l.panel,true,false,a->find.focus==4,NULL);
    bool enter=ui_pressed(KEY_ENTER),space=ui_pressed(KEY_SPACE);
    if (close || (enter && a->find.focus==4)) { a->modal=MODAL_NONE; a->consume_grid_input=true; }
    else if (whole || ((enter || space) && a->find.focus==1)) { a->find.whole_cell=!a->find.whole_cell; a->find.message[0]=0; }
    else if (previous || (enter && a->find.focus==2)) apply_find(a,GRID_FIND_PREVIOUS);
    else if (next || enter) apply_find(a,GRID_FIND_NEXT);
}
