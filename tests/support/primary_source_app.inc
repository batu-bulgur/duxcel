/* Actual Worker probes and session publication, independent of retained writing. */
static unsigned primary_source_stage,primary_source_wait,primary_source_checks;
static int primary_source_result;
static uint64_t primary_source_command,primary_source_high,primary_source_sheet;
static size_t primary_source_count,primary_source_index;
static const Cell *primary_source_cells;
static Analysis primary_source_before;
#define PRIMARY_SOURCE_CHECK(x) do { primary_source_checks++; if (!(x)) { fprintf(stderr,"FAIL pending primary stage %u:%d: %s (%s)\n",primary_source_stage,__LINE__,#x,a->message ? a->message : ""); primary_source_result=-1; return; } } while (0)
static bool primary_source_unchanged(const App *a) {
    return a->book.count==primary_source_count && a->book.active==primary_source_index && a->book.command_revision==primary_source_command &&
        a->book.sheet_high_water==primary_source_high && active_sheet((App *)a)->id==primary_source_sheet && a->page.cells==primary_source_cells &&
        !memcmp(&a->analysis,&primary_source_before,sizeof(Analysis));
}
static void primary_source_test_frame(AppSession *session) {
    App *a=&session->app;
    if (primary_source_result || a->busy || a->io_busy || ++primary_source_wait<20) return;
    primary_source_wait=0;
    switch (primary_source_stage) {
    case 0:
        PRIMARY_SOURCE_CHECK(a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha"));
        primary_source_count=a->book.count; primary_source_index=a->book.active; primary_source_command=a->book.command_revision;
        primary_source_high=a->book.sheet_high_water; primary_source_sheet=active_sheet(a)->id; primary_source_cells=a->page.cells; primary_source_before=a->analysis;
        open_source(a,"build/tests/generated/source-connection-b.duckdb",false);
        PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner && primary_source_unchanged(a));
        ui_note_key_press(KEY_ESCAPE,0); break;
    case 1:
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && primary_source_unchanged(a));
        open_source(a,"build/tests/generated/missing-directory/source.duckdb",false);
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && primary_source_unchanged(a));
        open_source(a,"build/tests/generated/source-connection-b.duckdb",false);
        PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner && primary_source_unchanged(a));
        a->source_format.bytes=SIZE_MAX; break;
    case 2:
        PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner && a->primary_source_reply && primary_source_unchanged(a));
        PRIMARY_SOURCE_CHECK(app_primary_source_owned(a)>sizeof(QueryReply));
        app_primary_source_publish(a); PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner && primary_source_unchanged(a));
        a->source_format.bytes=0; ui_note_key_press(KEY_F5,0); break;
    case 3:
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && !a->primary_source_reply && a->book.count==primary_source_count+1);
        PRIMARY_SOURCE_CHECK(a->book.sheet_high_water==primary_source_high+1 && a->book.command_revision==primary_source_command+1);
        PRIMARY_SOURCE_CHECK(a->analysis.schema && !strcmp(a->analysis.schema,"main") && !strcmp(a->analysis.table,"alpha") && a->page.total==3);
        PRIMARY_SOURCE_CHECK(a->page.cells[1].text && !strcmp(a->page.cells[1].text,"100.00"));
        PRIMARY_SOURCE_CHECK(app_definition_origin_find(a) && definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        PRIMARY_SOURCE_CHECK(activate_sheet(a,primary_source_index,true)); break;
    case 4:
        PRIMARY_SOURCE_CHECK(active_sheet(a)->id==primary_source_sheet && a->page.total==3 && strstr(a->analysis.path,"source-connection-a.duckdb"));
        PRIMARY_SOURCE_CHECK(definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        primary_source_count=a->book.count; primary_source_command=a->book.command_revision; primary_source_high=a->book.sheet_high_water;
        primary_source_cells=a->page.cells; primary_source_before=a->analysis;
        open_source(a,"build/tests/generated/source-connection-b.duckdb",false);
        PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner); a->analysis_revision++; break;
    case 5:
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && primary_source_unchanged(a));
        open_source(a,"build/tests/generated/source-connection-b.duckdb",false);
        PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner); a->source_format.bytes=SIZE_MAX; break;
    case 6:
        PRIMARY_SOURCE_CHECK(a->primary_source_reply && primary_source_unchanged(a));
        cancel_active_query(a); a->source_format.bytes=0;
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && !a->primary_source_reply && primary_source_unchanged(a));
        /* Real current OPEN pressure must be protected before CREATE can
           invalidate its document revision. No replacement preimage is made. */
        {
            AppDefinitionOrigin *existing=app_definition_origin_find(a); PRIMARY_SOURCE_CHECK(existing);
            /* Use a genuinely new loaded owner instead of reopening ADMITTED. */
            Analysis loaded={.kind=SOURCE_DATABASE,.path=dx_absolute_path("build/tests/generated/source-connection-a.duckdb")};
            PRIMARY_SOURCE_CHECK(loaded.path);
            PRIMARY_SOURCE_CHECK(capture_sheet(a));
            PRIMARY_SOURCE_CHECK(workbook_sheet_command(&a->book,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Loaded unresolved",.analysis=&loaded},NULL,NULL));
            analysis_free(&loaded);
            PRIMARY_SOURCE_CHECK(activate_sheet(a,a->book.active,false));
            a->source_format.bytes=SIZE_MAX;
        }
        break;
    case 7:
        PRIMARY_SOURCE_CHECK(!a->analysis.table && a->page.total==3);
        a->source_format.bytes=0;
        {
            AppDefinitionOrigin *pending=NULL; PRIMARY_SOURCE_CHECK(app_definition_origin_open(a,0,&pending,NULL)==DEFINITION_ORIGIN_READY);
            PRIMARY_SOURCE_CHECK(definition_origin_state(pending->owner).phase==DEFINITION_ORIGIN_OPEN);
            size_t count=a->book.count; uint64_t command=a->book.command_revision; const char *path=a->analysis.path;
            open_source(a,"build/tests/generated/source-connection-b.duckdb",false);
            PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && a->book.count==count && a->book.command_revision==command && a->analysis.path==path);
            PRIMARY_SOURCE_CHECK(app_definition_origin_find(a)==pending && definition_origin_state(pending->owner).phase==DEFINITION_ORIGIN_OPEN);
            change_analysis(a,false);
        }
        break;
    case 8:
        PRIMARY_SOURCE_CHECK(a->analysis.table && definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        open_source(a,"build/tests/generated/source-connection-b.duckdb",false); PRIMARY_SOURCE_CHECK(a->primary_source_pending_owner); break;
    case 9:
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && a->analysis.table && strstr(a->analysis.path,"source-connection-b.duckdb") && a->page.total==3);
        /* Complete document retirement cancels a held resolved reply. */
        open_source(a,"build/tests/generated/source-connection-a.duckdb",false); a->source_format.bytes=SIZE_MAX; break;
    case 10:
        PRIMARY_SOURCE_CHECK(a->primary_source_reply); a->source_format.bytes=0;
        a->document_action=DOCUMENT_NEW; a->book.dirty=false; finish_document_action(a);
        PRIMARY_SOURCE_CHECK(!a->primary_source_pending_owner && !a->primary_source_reply && a->book.count==1 && a->analysis.kind==SOURCE_NONE);
        primary_source_result=1;
        printf("PASS: native pending primary workflow (11 stages, %u checks)\n",primary_source_checks); return;
    }
    primary_source_stage++;
}
int app_test_primary_source_result(void) { return primary_source_result; }
unsigned app_test_primary_source_stage(void) { return primary_source_stage; }
