/* Private fixture: real renderer, version worker, source-unavailable semantics,
   keyboard/pointer input and native document guard; no production hooks. */
static unsigned hp_stage,hp_wait,hp_checks;
static int hp_result;
static bool hp_painted;
static const char *hp_path="build/tests/generated/history-preview.dxls";
static Workbook hp_working;
static AppVersionState hp_versions;
static Cell *hp_page;
static float *hp_widths;
static GridLocation hp_location;
static TextEditor hp_editor;
static CellEditTarget hp_target;
static uint64_t hp_undo,hp_redo,hp_sql;
static struct { DxMutex mutex; DxCond condition; bool ready,armed,entered,released; } hp_gate;
#define HP_CHECK(x) do { hp_checks++; if (!(x)) { fprintf(stderr,"FAIL historical GUI stage %u:%d: %s (%s)\n",hp_stage,__LINE__,#x,a->message ? a->message : ""); hp_result=-1; return; } } while (0)
char *dx_test_version_destination(void) { return dx_strdup(hp_path); }
void dx_test_preview_checkpoint(QueryWorker *worker,WorkbookVersionPreviewPhase phase) {
    (void)worker; if (!hp_gate.ready) return;
    dx_lock(&hp_gate.mutex);
    if (hp_gate.armed && phase==VERSION_PREVIEW_READY && !hp_gate.entered) {
        hp_gate.entered=true; while (!hp_gate.released) dx_cond_wait(&hp_gate.condition,&hp_gate.mutex);
    }
    dx_unlock(&hp_gate.mutex);
}
static void hp_click(Rectangle r) { ui_note_pointer_gesture((DxPointerGesture){r.x+10,r.y+10,r.x+10,r.y+10,0,false}); }
static void hp_name(App *a,const char *name) { input_set(&a->input,name); a->version_view.focus=0; ui_note_key_press(KEY_ENTER,0); }
static bool hp_working_location(App *a) {
    GridLocation g=grid_location(&a->grid),p=hp_location;
    return g.row==p.row && g.col==p.col && g.anchor_row==p.anchor_row && g.anchor_col==p.anchor_col &&
        g.end_row==p.end_row && g.end_col==p.end_col && g.top==p.top && g.horizontal==p.horizontal &&
        g.fraction==p.fraction && g.selection==p.selection && g.valid==p.valid;
}
static bool hp_text_visible(App *a,uint64_t row,size_t column) {
    Rectangle cell=grid_range_rect(&a->history_view.grid,a->ui.scale*a->history_view.grid.zoom,row,column,1,1);
    Image frame=LoadImageFromScreen(); Color *pixels=LoadImageColors(frame); size_t ink=0;
    if (pixels && GetScreenWidth()>0 && GetScreenHeight()>0) {
        float sx=(float)frame.width/GetScreenWidth(),sy=(float)frame.height/GetScreenHeight();
        int left=(int)((cell.x+8)*sx),right=(int)((cell.x+cell.width-8)*sx),top=(int)((cell.y+4)*sy),bottom=(int)((cell.y+cell.height-4)*sy);
        for (int y=top;y<bottom && y<frame.height;y++) for (int x=left;x<right && x<frame.width;x++)
            if (x>=0 && y>=0) { Color p=pixels[y*frame.width+x]; if (p.r<100 && p.g<100 && p.b<100) ink++; }
    }
    UnloadImageColors(pixels); UnloadImage(frame); return ink>10;
}
static void history_preview_test_frame(AppSession *session) {
    App *a=&session->app; char *error=NULL;
    if (hp_result || !active_sheet(a) || ++hp_wait<8) return; hp_wait=0;
    if ((a->busy || a->io_busy) && hp_stage!=19) return;
    if (historical(a) && (!a->history_view.back.width || !a->history_view.canvas.width)) return;
    switch (hp_stage) {
    case 0: {
        UIVersionLayout compact=ui_version_layout((Vector2){640,640},2,2);
        HP_CHECK(compact.valid && !CheckCollisionRecs(compact.open,compact.create) && !CheckCollisionRecs(compact.open,compact.message));
        remove(hp_path); dx_mutex_init(&hp_gate.mutex); dx_cond_init(&hp_gate.condition); hp_gate.ready=true;
        HP_CHECK(cells_set(&a->book.sheets[0].cells,0,0,"Version one",&error)); worksheet_record(a->book.sheets,EDIT_CELLS);
        HP_CHECK(cells_set(&a->book.sheets[0].cells,9000,30,"Offscreen retained",&error)); worksheet_record(a->book.sheets,EDIT_CELLS);
        CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
        HP_CHECK(format_apply(&a->book.sheets[0].formats,(FormatRange){0,0,0,0},FORMAT_BOLD,bold,&error)); worksheet_record(a->book.sheets,EDIT_FORMAT);
        HP_CHECK(workbook_add(&a->book,"Offline source",&(Analysis){.kind=SOURCE_PARQUET,.path="/unavailable/historical.parquet",.query="SELECT * FROM source WHERE amount > 10"},&error));
        HP_CHECK(objects_add(&a->book.sheets[1].objects,3,3,"Retained pivot",&error)>=0); worksheet_record(a->book.sheets+1,EDIT_OBJECTS);
        a->book.active=0; versions_open(a); hp_name(a,"V1"); break;
    }
    case 1:
        HP_CHECK(versions_index(a)->tag_count==1 && !a->book.dirty);
        a->modal=MODAL_NONE; grid_select_cell(&a->grid,0,0,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Version two"); ui_note_key_press(KEY_ENTER,0); break;
    case 2:
        HP_CHECK(!strcmp(cells_get(&a->book.sheets[0].cells,0,0),"Version two"));
        versions_open(a); hp_name(a,"V2"); break;
    case 3:
        HP_CHECK(versions_index(a)->tag_count==2 && versions_index(a)->root_count==2);
        a->modal=MODAL_NONE; grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Unsaved draft"); ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        HP_CHECK(a->book.dirty && !strcmp(cells_get(&active_sheet(a)->cells,1,1),"Unsaved draft"));
        HP_CHECK(workbook_history_step(&a->book,false,&error).status==WORKBOOK_HISTORY_APPLIED);
        HP_CHECK(workbook_prepare_cell_edit(&a->book,0,2,2,&hp_target,&error));
        hp_undo=workbook_history_peek(&a->book,false,&error).order; hp_redo=workbook_history_peek(&a->book,true,&error).order;
        HP_CHECK(hp_undo && hp_redo);
        text_lines_layout_sized(&a->query_editor.lines,&a->ui,"Retained editor layout",200,12);
        hp_page=a->page.cells; hp_widths=a->grid.widths; hp_location=grid_location(&a->grid); hp_editor=a->query_editor;
        a->grid.dragging=true; versions_open(a);
        HP_CHECK(a->modal==MODAL_NONE && !a->history.pending.request); a->grid.dragging=false;
        versions_open(a); a->version_view.selected=0; a->version_view.focus=4;
        { uint64_t request=a->request_id;
          a->grid.dragging=true; history_open(a);
          HP_CHECK(!a->history.pending.request && !a->io_busy && a->request_id==request); a->grid.dragging=false;
          a->grid.moving=true; HP_CHECK(!history_ready(a)); a->grid.moving=false;
          a->grid.resizing=0; HP_CHECK(!history_ready(a)); a->grid.resizing=-1;
          a->grid.scrollbar=1; HP_CHECK(!history_ready(a)); a->grid.scrollbar=0;
          a->sheet_action=SHEET_ACTION_DUPLICATE; HP_CHECK(!history_ready(a)); a->sheet_action=SHEET_ACTION_NONE;
        }
        ui_note_key_press(KEY_ENTER,0); break;
    case 5:
        HP_CHECK(historical(a) && a->modal==MODAL_NONE && !a->book.path && !a->book.dirty);
        HP_CHECK(!strcmp(cells_get(&a->book.sheets[0].cells,0,0),"Version one") && !strcmp(cells_get(&a->book.sheets[0].cells,9000,30),"Offscreen retained"));
        HP_CHECK(format_at(&a->book.sheets[0].formats,0,0).flags & FORMAT_BOLD);
        hp_working=*app_preview_working(&a->history); hp_versions=*app_preview_working_versions(&a->history); hp_sql=a->worker.sql_executed;
        HP_CHECK(a->page.cells==hp_page && a->grid.widths==hp_widths && !memcmp(&a->query_editor,&hp_editor,sizeof(hp_editor)));
        ui_note_key_press(KEY_F2,0); ui_note_key_press(KEY_Z,DX_MOD_SUPER); ui_note_key_press(KEY_S,DX_MOD_SUPER); ui_note_key_press(KEY_END,DX_MOD_SUPER); break;
    case 6:
        HP_CHECK(a->history_view.grid.row==9000 && a->history_view.grid.col==30);
        HP_CHECK(hp_text_visible(a,9000,30) && hp_working_location(a));
        HP_CHECK(historical(a) && a->modal==MODAL_NONE && !a->busy && !a->io_busy);
        HP_CHECK(!memcmp(app_preview_working(&a->history),&hp_working,sizeof(hp_working)));
        workspace_save_as(a,"build/tests/generated/forbidden-preview-save.dxls");
        HP_CHECK(!a->io_busy && historical(a)); versions_create(a); run_action(a,ACTION_BOLD);
        hp_click(a->history_view.next); break;
    case 7:
        HP_CHECK(a->history_view.sheet==1 && a->book.active==0 && a->book.sheets[1].analysis.kind==SOURCE_PARQUET);
        HP_CHECK(a->book.sheets[1].objects.count==1 && a->worker.sql_executed==hp_sql);
        smoke_snapshot("build/tests/results/history-preview/unavailable.png"); hp_click(a->history_view.definition); break;
    case 8:
        HP_CHECK(a->history_view.definitions && strstr(a->history_view.definition_text,"SELECT * FROM source WHERE amount > 10") && strstr(a->history_view.definition_text,"Retained pivot"));
        HP_CHECK(a->worker.sql_executed==hp_sql); smoke_snapshot("build/tests/results/history-preview/definitions.png"); hp_click(a->history_view.versions); break;
    case 9:
        HP_CHECK(a->modal==MODAL_VERSIONS); a->version_view.selected=SIZE_MAX; a->version_view.focus=4; hp_click(a->version_view.layout.open); break;
    case 10:
        HP_CHECK(historical(a) && a->modal==MODAL_NONE && !strcmp(a->history_label,"Latest Saved"));
        HP_CHECK(!strcmp(cells_get(&a->book.sheets[0].cells,0,0),"Version two") && !memcmp(app_preview_working(&a->history),&hp_working,sizeof(hp_working)));
        hp_click(a->history_view.back); break;
    case 11:
        HP_CHECK(!historical(a));
        HP_CHECK(!memcmp(&a->book,&hp_working,sizeof(hp_working)));
        HP_CHECK(!memcmp(&a->versions,&hp_versions,sizeof(hp_versions)) && hp_working_location(a));
        HP_CHECK(a->page.cells==hp_page && a->grid.widths==hp_widths && !memcmp(&a->query_editor,&hp_editor,sizeof(hp_editor)));
        HP_CHECK(workbook_history_peek(&a->book,false,&error).order==hp_undo && workbook_history_peek(&a->book,true,&error).order==hp_redo);
        HP_CHECK(workbook_check_cell_edit(&a->book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,hp_target,"Still valid",11},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_READY);
        ui_note_key_press(KEY_Z,DX_MOD_SUPER|DX_MOD_SHIFT); break;
    case 12:
        HP_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Unsaved draft") && a->book.dirty);
        versions_open(a); a->version_view.selected=0; a->version_view.focus=4; ui_note_key_press(KEY_ENTER,0); break;
    case 13:
        HP_CHECK(historical(a)); ui_note_key_press(KEY_W,DX_MOD_SUPER); break;
    case 14:
        HP_CHECK(!historical(a) && a->modal==MODAL_UNSAVED && a->document_action==DOCUMENT_CLOSE && a->book.dirty);
        HP_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Unsaved draft")); ui_note_key_press(KEY_ESCAPE,0); break;
    case 15:
        HP_CHECK(a->modal==MODAL_NONE && a->document_action==DOCUMENT_NONE); versions_open(a);
        a->version_view.selected=0; a->version_view.focus=4; ui_note_key_press(KEY_ENTER,0); break;
    case 16:
        HP_CHECK(historical(a)); if (!hp_painted) { hp_painted=true; return; }
        HP_CHECK(hp_text_visible(a,0,0));
        smoke_snapshot("build/tests/results/history-preview/worksheet.png"); ui_note_key_press(KEY_ESCAPE,0); break;
    case 17:
        HP_CHECK(!historical(a) && a->book.dirty); versions_open(a);
        dx_lock(&hp_gate.mutex); hp_gate.armed=true; dx_unlock(&hp_gate.mutex);
        a->version_view.selected=0; a->version_view.focus=4; ui_note_key_press(KEY_ENTER,0); break;
    case 18: HP_CHECK(false); break; /* Advanced below while the worker is held. */
    case 19: {
        dx_lock(&hp_gate.mutex); bool entered=hp_gate.entered; dx_unlock(&hp_gate.mutex); if (!entered) return;
        HP_CHECK(a->history.pending.request && !historical(a)); ui_note_key_press(KEY_ESCAPE,0);
        dx_lock(&hp_gate.mutex); hp_gate.released=true; dx_signal(&hp_gate.condition); dx_unlock(&hp_gate.mutex); break;
    }
    case 20:
        HP_CHECK(!historical(a) && !a->history.pending.request && !a->io_busy && a->book.dirty);
        HP_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Unsaved draft")); hp_result=1; break;
    default: hp_result=-1; return;
    }
    HP_CHECK(!error); hp_stage++; if (hp_stage==18) hp_stage=19;
}
int app_test_history_preview_result(void) { return hp_result; }
void app_test_history_preview_cleanup(void) {
    if (hp_gate.ready) { dx_lock(&hp_gate.mutex); hp_gate.released=true; dx_signal(&hp_gate.condition); dx_unlock(&hp_gate.mutex); }
}
void app_test_history_preview_finished(void) {
    if (hp_gate.ready) { dx_cond_destroy(&hp_gate.condition); dx_mutex_destroy(&hp_gate.mutex); }
    if (hp_result==1) printf("PASS: native historical preview workflow (20 stages, %u checks)\n",hp_checks);
}

static void history_preview_test_painted(App *a) {
    if (hp_result || !historical(a) || !a->history_view.ready || a->history_view.definitions) return;
    HP_CHECK(a->history_view.canvas.width>0 && a->history_view.grid.rect.width>0 && a->history_view.grid.rect.height>0);
}
