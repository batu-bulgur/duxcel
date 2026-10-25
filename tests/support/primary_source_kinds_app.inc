/* Native finite file IO failures preserve the entire previous visible owner. */
static unsigned source_kinds_stage,source_kinds_wait,source_kinds_checks;
static int source_kinds_result;
static uint64_t source_kinds_command,source_kinds_high,source_kinds_sheet;
static size_t source_kinds_count,source_kinds_index;
static const Cell *source_kinds_cells;
static Analysis source_kinds_analysis;
#define SOURCE_KINDS_CHECK(x) do { source_kinds_checks++; if (!(x)) { fprintf(stderr,"FAIL source kinds stage %u:%d: %s (%s)\n",source_kinds_stage,__LINE__,#x,a->message ? a->message : ""); source_kinds_result=-1; return; } } while (0)
static void source_kinds_capture(App *a) {
    source_kinds_count=a->book.count; source_kinds_index=a->book.active; source_kinds_sheet=active_sheet(a)->id;
    source_kinds_command=a->book.command_revision; source_kinds_high=a->book.sheet_high_water;
    source_kinds_cells=a->page.cells; source_kinds_analysis=a->analysis;
}
static bool source_kinds_unchanged(const App *a) {
    return a->book.count==source_kinds_count && a->book.active==source_kinds_index && a->book.command_revision==source_kinds_command &&
        a->book.sheet_high_water==source_kinds_high && a->book.sheets[a->book.active].id==source_kinds_sheet && a->page.cells==source_kinds_cells &&
        !memcmp(&a->analysis,&source_kinds_analysis,sizeof(Analysis));
}
static void primary_source_kinds_test_frame(AppSession *session) {
    App *a=&session->app;
    if (source_kinds_result || a->busy || a->io_busy || ++source_kinds_wait<20) return;
    source_kinds_wait=0;
    SOURCE_KINDS_CHECK(a->modal!=MODAL_ERROR);
    switch (source_kinds_stage) {
    case 0:
        SOURCE_KINDS_CHECK(a->analysis.kind==SOURCE_DATABASE && a->page.total==3);
        source_kinds_capture(a);
        {
            uint64_t request=a->request_id; a->request_id=UINT64_MAX-1;
            open_source(a,"build/tests/generated/source-open.csv",false);
            SOURCE_KINDS_CHECK(!a->primary_source_draft && a->request_id==UINT64_MAX-1 && source_kinds_unchanged(a));
            a->request_id=request;
            uint64_t revision=a->analysis_revision; a->analysis_revision=UINT64_MAX;
            open_source(a,"build/tests/generated/source-open.csv",false);
            SOURCE_KINDS_CHECK(!a->primary_source_draft && a->analysis_revision==UINT64_MAX && source_kinds_unchanged(a));
            a->analysis_revision=revision;
        }
        open_source(a,"build/tests/generated/source-open.csv",false);
        SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a));
        a->source_format.bytes=SIZE_MAX; break;
    case 1:
        SOURCE_KINDS_CHECK(a->primary_source_reply && source_kinds_unchanged(a));
        a->source_format.bytes=0; ui_note_key_press(KEY_F5,0); break;
    case 2:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && a->analysis.kind==SOURCE_CSV && a->page.total==3 && a->page.column_count==3);
        SOURCE_KINDS_CHECK(!a->analysis.schema && !a->analysis.table && a->book.count==source_kinds_count+1 && a->book.command_revision==source_kinds_command+1);
        SOURCE_KINDS_CHECK(a->page.cells[1].text && strtod(a->page.cells[1].text,NULL)==10);
        SOURCE_KINDS_CHECK(a->page.cells[2].is_null && !a->page.cells[5].is_null && a->page.cells[5].text && !*a->page.cells[5].text &&
            !a->page.cells[8].is_null && a->page.cells[8].text && !strcmp(a->page.cells[8].text,"NULL"));
        SOURCE_KINDS_CHECK(definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        source_kinds_capture(a); open_source(a,"build/tests/generated/source-open-bad.csv.gz",false);
        SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a)); break;
    case 3:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && !a->primary_source_reply && source_kinds_unchanged(a));
        open_source(a,"build/tests/generated/source-open.parquet",false);
        SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a)); break;
    case 4:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && a->analysis.kind==SOURCE_PARQUET && a->page.total==3 && a->page.column_count==3);
        SOURCE_KINDS_CHECK(!a->analysis.schema && !a->analysis.table && a->book.count==source_kinds_count+1 && a->book.sheet_high_water==source_kinds_high+1);
        SOURCE_KINDS_CHECK(a->page.cells[1].text && !strcmp(a->page.cells[1].text,"10.00"));
        SOURCE_KINDS_CHECK(a->page.cells[2].is_null && !a->page.cells[5].is_null && a->page.cells[5].text && !*a->page.cells[5].text &&
            !a->page.cells[8].is_null && a->page.cells[8].text && !strcmp(a->page.cells[8].text,"NULL"));
        SOURCE_KINDS_CHECK(definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        source_kinds_capture(a); open_source(a,"build/tests/generated/source-open-bad.parquet",false);
        SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a)); break;
    case 5:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && !a->primary_source_reply && source_kinds_unchanged(a));
        open_source(a,"",true); SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a)); break;
    case 6:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && a->analysis.kind==SOURCE_DEMO && a->analysis.path && !*a->analysis.path && a->page.total==1000000);
        SOURCE_KINDS_CHECK(a->book.count==source_kinds_count+1 && a->book.command_revision==source_kinds_command+1 && a->book.sheet_high_water==source_kinds_high+1);
        SOURCE_KINDS_CHECK(definition_origin_state(app_definition_origin_find(a)->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        source_kinds_capture(a); open_source(a,"build/tests/generated/source-open.csv",false);
        SOURCE_KINDS_CHECK(a->primary_source_draft && source_kinds_unchanged(a)); ui_note_key_press(KEY_ESCAPE,0); break;
    case 7:
        SOURCE_KINDS_CHECK(!a->primary_source_draft && !a->primary_source_reply && source_kinds_unchanged(a));
        open_source(a,"build/tests/generated/source-open.parquet",false); a->source_format.bytes=SIZE_MAX; break;
    case 8:
        SOURCE_KINDS_CHECK(a->primary_source_reply && source_kinds_unchanged(a)); a->source_format.bytes=0;
        a->document_action=DOCUMENT_CLOSE; a->book.dirty=false; finish_document_action(a);
        SOURCE_KINDS_CHECK(!a->primary_source_draft && !a->primary_source_reply && a->book.count==1 && a->analysis.kind==SOURCE_NONE);
        source_kinds_result=1; printf("PASS: native source kind workflow (9 stages, %u checks)\n",source_kinds_checks); return;
    }
    source_kinds_stage++;
}
int app_test_primary_source_kinds_result(void) { return source_kinds_result; }
unsigned app_test_primary_source_kinds_stage(void) { return source_kinds_stage; }
