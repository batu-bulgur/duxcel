/* Actual external-source provenance, additive source action and failure/retry. */
#ifdef DUXCEL_SOURCE_DETAILS_PARQUET
#define SD_FILE "build/tests/generated/design-definition.parquet"
#define SD_KIND SOURCE_PARQUET
#else
#define SD_FILE "build/tests/generated/design-definition.csv"
#define SD_KIND SOURCE_CSV
#endif
#define SD_HIDDEN SD_FILE ".source-details-offline"
static unsigned source_details_stage,source_details_wait,source_details_checks;
static int source_details_result;
static bool source_details_hidden;
static uint64_t source_details_revision,source_details_file_hash;
static size_t source_details_undo,source_details_redo;
static char *source_details_clipboard;
static uint64_t source_details_before_query;
#define SD_CHECK(c) do { source_details_checks++; if (!(c)) { fprintf(stderr,"FAIL source details stage %u:%d: %s (%s)\n",source_details_stage,__LINE__,#c,a->message ? a->message : ""); source_details_result=-1; return; } } while (0)
static uint64_t source_details_hash(void) {
    FILE *file=fopen(SD_FILE,"rb"); if (!file) return 0;
    uint64_t hash=UINT64_C(1469598103934665603); int byte;
    while ((byte=fgetc(file))!=EOF) { hash^=(unsigned char)byte; hash*=UINT64_C(1099511628211); }
    bool ok=!ferror(file); fclose(file); return ok ? hash : 0;
}
static void source_details_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *sheet=active_sheet(a); SourceProvenanceView view;
    if (source_details_result || !sheet || a->busy || a->io_busy || ++source_details_wait<24) return;
    source_details_wait=0;
    switch (source_details_stage) {
    case 0:
        SD_CHECK(!FileExists(SD_HIDDEN)); source_details_file_hash=source_details_hash(); SD_CHECK(source_details_file_hash);
        source_details_clipboard=dx_strdup(GetClipboardText()); open_source(a,SD_FILE,false); break;
    case 1:
        if (a->page.count_pending || a->count_request_id) return;
        SD_CHECK(source_details_view(a,&view) && view.definition==&a->analysis && view.kind==SD_KIND && view.state==SOURCE_VIEW_READY);
        { char *expected=dx_absolute_path(SD_FILE); SD_CHECK(expected && !strcmp(view.path,expected)); free(expected); }
        source_details_revision=a->book.command_revision; source_details_undo=sheet->undo_actions_count; source_details_redo=sheet->redo_actions_count;
        begin_step_menu(a,0,(Vector2){300,200}); break;
    case 2:
        SD_CHECK(a->modal==MODAL_STEP && !a->context_focus); ui_note_key_press(KEY_END,0); break;
    case 3:
        SD_CHECK(a->modal==MODAL_STEP && a->context_focus==5); ui_note_key_press(KEY_ENTER,0); break;
    case 4:
        SD_CHECK(a->modal==MODAL_SOURCE_DETAILS && source_details_view(a,&view) && view.state==SOURCE_VIEW_READY);
        SD_CHECK(strstr(a->source_details_text,view.path) && strstr(a->source_details_text,"External · Read-only") && strstr(a->source_details_text,"Source rows remain external"));
        ui_note_key_press(KEY_C,DX_MOD_SUPER); break;
    case 5:
        SD_CHECK(!strcmp(GetClipboardText(),a->source_details_text));
        /* A real new-Table probe uses JOB_PAGE but is not this primary source.
           Cancel before its reply is admitted; no Table/user edit is created. */
        grid_select_cell(&a->grid,900,40,false);
        SD_CHECK(import_table(a,"build/tests/generated/source-connection-a.duckdb"));
        SD_CHECK(a->object_import.active && a->busy && a->pending_kind==JOB_PAGE);
        SD_CHECK(source_details_view(a,&view) && view.definition==&a->analysis && view.state==SOURCE_VIEW_READY);
        cancel_active_query(a); SD_CHECK(!a->object_import.active && !a->busy && !sheet->objects.count);
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 6:
        SD_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_details_revision && sheet->undo_actions_count==source_details_undo && sheet->redo_actions_count==source_details_redo);
        begin_step_menu(a,0,(Vector2){300,200}); break;
    case 7:
        SD_CHECK(a->modal==MODAL_STEP && !a->context_focus); ui_note_key_press(KEY_ENTER,0); break;
    case 8:
        SD_CHECK(a->modal==MODAL_SOURCE_QUERY); ui_note_key_press(KEY_ESCAPE,0); break;
    case 9:
        SD_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_details_revision);
        grid_select_cell(&a->grid,330,16,false); SD_CHECK(import_table(a,"build/tests/generated/design-definition.csv")); break;
    case 10:
        if (!object_ready(a,0)) return;
        a->selected_object=0; create_pivot(a); SD_CHECK(sheet->objects.count==2); break;
    case 11:
        SD_CHECK(a->selected_object==1 && source_details_view(a,&view));
        SD_CHECK(view.definition==&sheet->objects.items[0].source && view.state==SOURCE_VIEW_READY && view.kind==SOURCE_CSV);
        source_details_revision=a->book.command_revision; source_details_undo=sheet->undo_actions_count; source_details_redo=sheet->redo_actions_count;
        begin_source_details(a); ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); break;
    case 12:
        SD_CHECK(a->modal==MODAL_SOURCE_DETAILS && source_details_view(a,&view) && strstr(a->source_details_text,view.path));
        SD_CHECK(a->source_details_lines.count>8); ui_note_key_press(KEY_END,0); break;
    case 13:
        SD_CHECK(a->source_details_scroll>0 && a->source_details_lines.count<DX_TEXT_LINES);
        ui_note_key_press(KEY_ENTER,0); break;
    case 14:
        SD_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_details_revision && sheet->undo_actions_count==source_details_undo && sheet->redo_actions_count==source_details_redo);
        a->selected_object=-1; ui_set_scale(&a->ui,1); SetWindowSize(1280,820);
        SD_CHECK(rename(SD_FILE,SD_HIDDEN)==0); source_details_hidden=true; ui_note_key_press(KEY_F5,0); break;
    case 15:
        SD_CHECK(a->fetch_failed && a->modal==MODAL_ERROR && source_details_view(a,&view) && view.definition==&a->analysis && view.state==SOURCE_VIEW_UNAVAILABLE);
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 16:
        SD_CHECK(a->modal==MODAL_NONE); begin_source_details(a);
        SD_CHECK(a->modal==MODAL_SOURCE_DETAILS && strstr(a->source_details_text,"Availability: Unavailable"));
        SD_CHECK(rename(SD_HIDDEN,SD_FILE)==0); source_details_hidden=false; ui_note_key_press(KEY_ESCAPE,0); break;
    case 17:
        SD_CHECK(a->modal==MODAL_NONE); ui_note_key_press(KEY_F5,0); break;
    case 18:
        if (a->page.count_pending || a->count_request_id || a->refresh_plans) return;
        SD_CHECK(!a->fetch_failed && source_details_view(a,&view) && view.definition==&a->analysis && view.kind==SD_KIND && view.state==SOURCE_VIEW_READY);
        SD_CHECK(a->book.command_revision==source_details_revision && sheet->undo_actions_count==source_details_undo && sheet->redo_actions_count==source_details_redo && source_details_hash()==source_details_file_hash);
        source_details_before_query=a->book.command_revision; begin_source_query(a); break;
    case 19: {
        SD_CHECK(a->modal==MODAL_SOURCE_QUERY);
        char query[8192]="SELECT * FROM input /* "; size_t bytes=strlen(query);
        while (bytes+sizeof("İstanbul-表-🦆 \n")+4<sizeof(query)) { memcpy(query+bytes,"İstanbul-表-🦆 \n",sizeof("İstanbul-表-🦆 \n")-1); bytes+=sizeof("İstanbul-表-🦆 \n")-1; }
        memcpy(query+bytes," */",4); input_set(&a->input,query); ui_note_key_press(KEY_ENTER,DX_MOD_SUPER); break;
    }
    case 20:
        if (a->page.count_pending || a->count_request_id) return;
        SD_CHECK(a->modal==MODAL_NONE && !a->query_loading && a->analysis.query && strlen(a->analysis.query)>8000);
        SD_CHECK(a->book.command_revision==source_details_before_query+1); begin_source_details(a); break;
    case 21: {
        SD_CHECK(a->modal==MODAL_SOURCE_DETAILS && strstr(a->source_details_text,a->analysis.query) && a->source_details_lines.count>100);
        /* Replace the actual definition while the dialog owns its prior text.
           The new query only appends a comment to the validated SELECT. */
        Analysis proposal=a->analysis; proposal.query=dx_printf("%s /* replaced owner */",a->analysis.query);
        SD_CHECK(replace_primary_connection(a,&proposal)); free(proposal.query);
        SD_CHECK(source_details_view(a,&view) && view.state==SOURCE_VIEW_LOADING && view.definition==&a->analysis);
        break;
    }
    case 22:
        if (a->page.count_pending || a->count_request_id) return;
        SD_CHECK(a->modal==MODAL_SOURCE_DETAILS && source_details_view(a,&view) && view.state==SOURCE_VIEW_READY);
        SD_CHECK(strstr(a->source_details_text,a->analysis.query) && strstr(a->source_details_text,"replaced owner"));
        ui_note_key_press(KEY_END,0); break;
    case 23:
        SD_CHECK(a->source_details_scroll>100 && !a->source_details_lines.limited); ui_note_key_press(KEY_C,DX_MOD_SUPER); break;
    case 24:
        SD_CHECK(!strcmp(GetClipboardText(),a->source_details_text) && a->book.command_revision==source_details_before_query+2 && source_details_hash()==source_details_file_hash);
        source_details_result=1; printf("PASS: native source provenance workflow (25 stages, %u checks)\n",source_details_checks); break;
    }
    source_details_stage++;
}
int app_test_source_details_result(void) { return source_details_result; }
unsigned app_test_source_details_stage(void) { return source_details_stage; }
int app_test_source_details_cleanup(void) {
    bool ok=true;
    if (source_details_hidden) {
        if (rename(SD_HIDDEN,SD_FILE)) { fprintf(stderr,"FAIL: source fixture restoration failed\n"); ok=false; }
        else source_details_hidden=false;
    }
    if (source_details_file_hash && source_details_hash()!=source_details_file_hash) ok=false;
    if (source_details_clipboard) { SetClipboardText(source_details_clipboard); free(source_details_clipboard); source_details_clipboard=NULL; }
    return ok;
}
#undef SD_CHECK
#undef SD_FILE
#undef SD_HIDDEN
#undef SD_KIND
