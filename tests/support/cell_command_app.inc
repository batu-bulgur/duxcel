/* Test-only real App adapter fixture. Included exclusively in its feature App
   object; normal/review/browser builds contain neither hook nor stage state. */
static unsigned cell_command_stage,cell_command_wait,cell_command_checks;
static int cell_command_result;
static bool cell_command_narrow;
static uint64_t cell_command_revision;
static size_t cell_command_history;
static CellEditTarget cell_command_old_target;
static bool cell_command_warning_last_line(App *a) {
    /* Inspect actual ink near the final wrapped line's center. Rectangle-only
       checks miss double-scaled line spacing that clips the recovery action. */
    Image image=LoadImageFromScreen(); if (!image.data) return false;
    Color *pixels=LoadImageColors(image); unsigned ink=0; float s=a->ui.scale*a->grid.zoom;
    Rectangle panel=a->cell_edit_error_panel;
    float middle=panel.y+8*s+(a->cell_edit_error_lines.count-.5f)*16*s;
    float rx=(float)image.width/GetScreenWidth(),ry=(float)image.height/GetScreenHeight();
    int left=(int)((panel.x+10*s)*rx),right=(int)((panel.x+panel.width-10*s)*rx);
    int top=(int)((middle-3*s)*ry),bottom=(int)((middle+3*s)*ry);
    for (int y=top;y<bottom && y<image.height;y++) for (int x=left;x<right && x<image.width;x++) if (x>=0 && y>=0) {
        Color color=pixels[y*image.width+x];
        if (color.r>90 && color.r<180 && color.g>35 && color.g<125 && color.b<85) ink++;
    }
    UnloadImageColors(pixels); UnloadImage(image); return ink>8;
}
#define CELL_GUI_CHECK(x) do { cell_command_checks++; if (!(x)) { fprintf(stderr,"FAIL: cell command GUI stage %u line %d: %s (%s)\n",cell_command_stage,__LINE__,#x,a->message ? a->message : ""); cell_command_result=-1; return; } } while (0)
static void cell_command_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (cell_command_result || a->busy || a->io_busy || !s || ++cell_command_wait<12) return;
    cell_command_wait=0;
    switch (cell_command_stage) {
    case 0:
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL);
        CELL_GUI_CHECK(a->cell_edit_target_valid); input_set(&a->input,"12345678901234567890.0123"); ui_note_key_press(KEY_ENTER,0); break;
    case 1:
        CELL_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123"));
        CELL_GUI_CHECK(s->undo_actions_count==1 && a->book.dirty);
        cell_command_revision=s->cells.revision; cell_command_history=s->undo_actions_count;
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); ui_note_key_press(KEY_ENTER,0); break;
    case 2:
        CELL_GUI_CHECK(a->modal==MODAL_NONE && s->cells.revision==cell_command_revision && s->undo_actions_count==cell_command_history);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 3:
        CELL_GUI_CHECK(!*cells_get(&s->cells,1,1) && s->redo_actions_count==1);
        cell_command_revision=s->cells.revision;
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        CELL_GUI_CHECK(s->cells.revision==cell_command_revision && s->redo_actions_count==1);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 5: {
        CELL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"12345678901234567890.0123"));
        grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Draft kept");
        CellEditTarget target; char *error=NULL;
        CELL_GUI_CHECK(workbook_prepare_cell_edit(&a->book,a->book.active,1,1,&target,&error));
        CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"Newer data",10};
        CELL_GUI_CHECK(workbook_apply_cell_edit(&a->book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
        free(error); cell_command_history=s->undo_actions_count; break;
    }
    case 6: ui_note_key_press(KEY_ENTER,0); break;
    case 7:
        CELL_GUI_CHECK(a->modal==MODAL_CELL && !strcmp(a->input.text,"Draft kept"));
        CELL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Newer data") && s->undo_actions_count==cell_command_history);
        CELL_GUI_CHECK(a->message && strstr(a->message,"Cell data changed"));
        CELL_GUI_CHECK(a->cell_edit_error && a->cell_edit_error_lines.count);
        CELL_GUI_CHECK(cell_command_warning_last_line(a));
        CELL_GUI_CHECK(a->cell_edit_error_panel.x>=a->grid.body.x && a->cell_edit_error_panel.y>=a->grid.body.y &&
            a->cell_edit_error_panel.x+a->cell_edit_error_panel.width<=a->grid.body.x+a->grid.body.width+1 &&
            a->cell_edit_error_panel.y+a->cell_edit_error_panel.height<=a->grid.body.y+a->grid.body.height+1);
        if (!cell_command_narrow) {
            smoke_snapshot("build/tests/results/cell-command/stale-draft.png");
            cell_command_narrow=true; ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); return;
        }
        smoke_snapshot("build/tests/results/cell-command/stale-draft-150.png");
        ui_note_key_press(KEY_ESCAPE,0); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); break;
    case 8:
        CELL_GUI_CHECK(a->modal==MODAL_NONE && !a->cell_edit_error); grid_select_cell(&a->grid,1,1,false);
        open_modal(a,MODAL_CELL); input_set(&a->input,"Final command value"); ui_note_key_press(KEY_ENTER,0); break;
    case 9:
        CELL_GUI_CHECK(a->modal==MODAL_NONE && !strcmp(cells_get(&s->cells,1,1),"Final command value"));
        CELL_GUI_CHECK(s->undo_actions_count==cell_command_history+1);
        cell_command_old_target=a->cell_edit_target;
        workspace_job(a,JOB_SAVE,"build/tests/generated/cell-command-demo.dxls"); break;
    case 10:
        CELL_GUI_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,"build/tests/generated/cell-command-demo.dxls"); break;
    case 11: {
        CELL_GUI_CHECK(!a->book.dirty && !strcmp(cells_get(&s->cells,1,1),"Final command value"));
        CellEditCommand command={CELL_EDIT_COMMAND_VERSION,cell_command_old_target,"Wrong document",14}; char *error=NULL;
        CELL_GUI_CHECK(workbook_apply_cell_edit(&a->book,&command,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_STALE);
        free(error); CELL_GUI_CHECK(!a->book.dirty && !s->undo_actions_count);
        grid_select_cell(&a->grid,1,1,false); break;
    }
    case 12:
        CELL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Final command value"));
        smoke_snapshot("build/tests/results/cell-command/final.png");
        cell_command_result=1;
        printf("PASS: native cell command workflow (13 stages, %u checks)\n",cell_command_checks); break;
    }
    cell_command_stage++;
}
int app_test_cell_command_result(void) { return cell_command_result; }
#undef CELL_GUI_CHECK
