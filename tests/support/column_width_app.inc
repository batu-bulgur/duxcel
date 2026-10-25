/* Private actual-input/chronology/persistence fixture; excluded from production. */
static unsigned width_stage,width_wait,width_checks;
static int width_result;
static uint64_t width_revision,width_request;
static const char *width_path="build/tests/generated/column-width.dxls";
#define WIDTH_CHECK(x) do { width_checks++; if (!(x)) { fprintf(stderr,"FAIL width GUI stage %u:%d: %s (%s)\n",width_stage,__LINE__,#x,a->message ? a->message : ""); width_result=-1; return; } } while (0)
static void width_dialog_input(App *a,const char *text) {
    column_action(a,COLUMN_WIDTH); input_set(&a->input,text); a->width_focus=1; ui_note_key_press(KEY_ENTER,0);
}
static void column_width_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (width_result || !s || a->busy || a->io_busy || a->query_loading || a->page_stale || ++width_wait<24) return;
    width_wait=0; WIDTH_CHECK(!a->fetch_failed && a->modal!=MODAL_ERROR);
    switch (width_stage) {
    case 0:
        remove(width_path); WIDTH_CHECK(s->view.column_count && s->view.widths[0]==96);
        width_revision=a->book.command_revision; width_request=a->request_id; width_dialog_input(a,"177.5"); break;
    case 1:
        WIDTH_CHECK(a->grid.widths[0]==177.5f && s->view.widths[0]==177.5f && a->book.command_revision==width_revision+1 && a->request_id==width_request);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 2:
        WIDTH_CHECK(a->grid.widths[0]==96 && s->view.widths[0]==96); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 3:
        WIDTH_CHECK(a->grid.widths[0]==177.5f); a->chrome_focus=a->ribbon_focus=a->pivot_focus=0; ui_note_key_press(KEY_RIGHT,DX_MOD_ALT); break;
    case 4:
        WIDTH_CHECK(a->grid.widths[0]==189.5f && s->view.widths[0]==189.5f); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 5:
        WIDTH_CHECK(a->grid.widths[0]==177.5f && s->redo_actions_count==1); width_revision=a->book.command_revision; width_dialog_input(a,"177.5"); break;
    case 6:
        WIDTH_CHECK(a->book.command_revision==width_revision && s->redo_actions_count==1); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 7: {
        WIDTH_CHECK(a->grid.widths[0]==189.5f); width_revision=a->book.command_revision;
        float scale=a->ui.scale*a->grid.zoom,x=grid_column_x(&a->grid,scale,0)+a->grid.widths[0]*scale,y=(a->grid.rect.y+a->grid.body.y)*.5f;
        ui_note_pointer_gesture((DxPointerGesture){.x0=x,.y0=y,.x1=x+40*scale,.y1=y}); break;
    }
    case 8: {
        WIDTH_CHECK(a->grid.widths[0]==229.5f && s->view.widths[0]==229.5f && a->book.command_revision==width_revision+1);
        width_revision=a->book.command_revision;
        float scale=a->ui.scale*a->grid.zoom,x=grid_column_x(&a->grid,scale,0)+a->grid.widths[0]*scale,y=(a->grid.rect.y+a->grid.body.y)*.5f;
        UIContext ui=a->ui; ui.scale=scale;
        grid_handle_input(&a->grid,&ui,&a->page,&(GridInput){.mouse={x,y},.pressed=true,.down=true});
        grid_handle_input(&a->grid,&ui,&a->page,&(GridInput){.mouse={x+60*scale,y},.down=true});
        WIDTH_CHECK(s->view.widths[0]==229.5f); ui_note_key_press(KEY_ESCAPE,0); break;
    }
    case 9:
        WIDTH_CHECK(a->grid.widths[0]==229.5f && a->book.command_revision==width_revision); new_sheet(a); break;
    case 10:
        WIDTH_CHECK(a->book.count==2 && a->book.active==1); history(a,false); history(a,false); break;
    case 11:
        WIDTH_CHECK(a->book.count==1 && a->grid.widths[0]==189.5f && s->view.widths[0]==189.5f);
        history(a,true); history(a,true); break;
    case 12:
        WIDTH_CHECK(a->book.count==2 && a->book.active==1); open_source(a,"tests/fixtures/duplicates.csv",false); break;
    case 13:
        WIDTH_CHECK(a->page.total==8 && a->grid.widths[0]==128 && s->view.widths[0]==128); width_dialog_input(a,"220"); break;
    case 14:
        WIDTH_CHECK(a->grid.widths[0]==220 && s->view.widths[0]==220);
        begin_source_query(a); input_set(&a->input,"SELECT City AS Renamed, Amount FROM input"); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    case 15:
        WIDTH_CHECK(a->analysis.query && a->page.column_count==2 && !strcmp(a->page.columns[0].name,"Renamed") && a->grid.widths[0]==220 && s->view.widths[0]==220);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 16:
        WIDTH_CHECK(!a->analysis.query && a->grid.widths[0]==220); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 17:
        WIDTH_CHECK(a->grid.widths[0]==128 && s->view.widths[0]==128); ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 18:
        WIDTH_CHECK(a->grid.widths[0]==220); workspace_job(a,JOB_SAVE,width_path); break;
    case 19:
        WIDTH_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,width_path); break;
    case 20:
        WIDTH_CHECK(!a->book.dirty && !s->undo_actions_count && a->grid.widths[0]==220 && s->view.widths[0]==220 && a->book.sheets[0].view.widths[0]==229.5f);
        WIDTH_CHECK(activate_sheet(a,0,true)); break;
    case 21: {
        char *error=NULL; WIDTH_CHECK(cells_set(&s->cells,2,3,"NULL",&error)); worksheet_record(s,EDIT_CELLS);
        CellFormat format=format_default(); format.flags=FORMAT_BOLD;
        WIDTH_CHECK(format_apply(&s->formats,(FormatRange){2,2,3,3},FORMAT_BOLD,format,&error)); worksheet_record(s,EDIT_FORMAT);
        grid_ensure_columns(&a->grid,16384); grid_set_width(&a->grid,3,150); grid_set_width(&a->grid,16383,166);
        WIDTH_CHECK(commit_grid_widths(a)); free(error); width_revision=a->book.command_revision;
        a->grid.col=1; column_action(a,COLUMN_LEFT); break;
    }
    case 22:
        WIDTH_CHECK(a->book.command_revision==width_revision+1 && !strcmp(cells_get(&s->cells,2,4),"NULL") && !*cells_get(&s->cells,2,3) &&
            (format_at(&s->formats,2,4).flags&FORMAT_BOLD) && a->grid.widths[4]==150 && a->grid.widths[16383]==96);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 23:
        WIDTH_CHECK(!strcmp(cells_get(&s->cells,2,3),"NULL") && (format_at(&s->formats,2,3).flags&FORMAT_BOLD) && a->grid.widths[3]==150 && a->grid.widths[16383]==166);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 24:
        WIDTH_CHECK(!strcmp(cells_get(&s->cells,2,4),"NULL") && a->grid.widths[4]==150 && a->grid.widths[16383]==96);
        a->grid.col=1; column_action(a,COLUMN_DELETE); break;
    case 25:
        WIDTH_CHECK(!strcmp(cells_get(&s->cells,2,3),"NULL") && (format_at(&s->formats,2,3).flags&FORMAT_BOLD) && a->grid.widths[3]==150 && a->grid.widths[16383]==96);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 26:
        WIDTH_CHECK(!strcmp(cells_get(&s->cells,2,4),"NULL") && a->grid.widths[4]==150); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 27:
        WIDTH_CHECK(!strcmp(cells_get(&s->cells,2,3),"NULL") && a->grid.widths[3]==150 && a->grid.widths[16383]==166);
        workspace_job(a,JOB_SAVE,width_path); break;
    case 28:
        WIDTH_CHECK(!a->book.dirty); workspace_job(a,JOB_LOAD,width_path); break;
    case 29:
        WIDTH_CHECK(!a->book.dirty && !s->undo_actions_count && !strcmp(cells_get(&s->cells,2,3),"NULL") &&
            (format_at(&s->formats,2,3).flags&FORMAT_BOLD) && a->grid.widths[3]==150 && a->grid.widths[16383]==166);
        width_result=1; printf("PASS: native column width workflow (30 stages, %u checks)\n",width_checks); break;
    }
    width_stage++;
}
int app_test_column_width_result(void) { return width_result; }
unsigned app_test_column_width_stage(void) { return width_stage; }
#undef WIDTH_CHECK
