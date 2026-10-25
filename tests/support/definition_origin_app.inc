/* Private real-worker primary default reply plus typed admission pressure tests. */
static unsigned origin_stage,origin_wait,origin_checks;
static int origin_result;
static const void *origin_immutable;
static size_t origin_initial_bytes,origin_undo,origin_redo;
static uint64_t origin_command,origin_branch,origin_revision,origin_request;
static DefinitionOrigin *origin_pending;
static const char *origin_save_path="build/tests/generated/origin-lifetime.dxls";
#define ORIGIN_CHECK(x) do { origin_checks++; if (!(x)) { fprintf(stderr,"FAIL primary origin stage %u:%d: %s (%s)\n",origin_stage,__LINE__,#x,a->message ? a->message : ""); origin_result=-1; return; } } while (0)
static void definition_origin_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (origin_result || !s || a->busy || a->io_busy || ++origin_wait<24) return;
    origin_wait=0; ORIGIN_CHECK(a->modal!=MODAL_ERROR);
    switch (origin_stage) {
    case 0: {
        remove(origin_save_path);
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha"));
        AppDefinitionOrigin *node=app_definition_origin_find(a); ORIGIN_CHECK(node && definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        origin_immutable=definition_origin_assets(node->owner).primary; origin_initial_bytes=app_definition_origin_bytes(a);
        ORIGIN_CHECK(origin_immutable && origin_initial_bytes>definition_origin_owner_bytes());
        /* A current loaded DATABASE definition may still lack its default.
           New-primary opening now resolves before creation; exercise the actual
           legacy-free loaded-owner path through the real producer/activation. */
        ORIGIN_CHECK(capture_sheet(a));
        Analysis loaded={.kind=SOURCE_DATABASE,.path=dx_absolute_path("build/tests/generated/source-connection-a.duckdb")};
        ORIGIN_CHECK(loaded.path);
        ORIGIN_CHECK(workbook_sheet_command(&a->book,(WorkbookSheetCommand){.operation=SHEET_CREATE,.name="Pending loaded primary",.analysis=&loaded},NULL,NULL));
        analysis_free(&loaded);
        ORIGIN_CHECK(activate_sheet(a,a->book.active,false));
        ORIGIN_CHECK(!a->analysis.table && !app_definition_origin_find(a));
        /* Existing governor field exercises real reply pressure, not a test-only
           admission flag. No allocation is represented by this sentinel. */
        a->analysis.schema=dx_strdup("main");
        a->source_format.bytes=SIZE_MAX; break;
    }
    case 1: {
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && !a->analysis.table && !app_definition_origin_find(a));
        a->source_format.bytes=0;
        uint64_t command=a->book.command_revision,request=a->request_id,revision=a->analysis_revision,branch=a->book.history_branch;
        size_t undo=s->undo_actions_count,redo=s->redo_actions_count;
        QueryReply reply={.kind=JOB_PAGE,.id=request,.revision=revision,.analysis=analysis_copy(&a->analysis)};
        free(reply.analysis.schema); reply.analysis.schema=dx_strdup("main"); reply.analysis.table=dx_strdup("alpha");
        char *error=NULL; AppDefinitionOrigin *pending=NULL;
        ORIGIN_CHECK(app_definition_origin_open(a,0,&pending,&error)==DEFINITION_ORIGIN_READY && pending && !error);
        reply.id++;
        ORIGIN_CHECK(app_definition_origin_reply(a,&reply,&error)==DEFINITION_ORIGIN_STALE && !a->analysis.table && error); free(error); error=NULL; reply.id--;
        /* An authored non-NULL schema cannot be replaced by a default reply. */
        free(reply.analysis.schema); reply.analysis.schema=dx_strdup("different");
        ORIGIN_CHECK(app_definition_origin_reply(a,&reply,&error)==DEFINITION_ORIGIN_STALE && !strcmp(a->analysis.schema,"main") && !a->analysis.table && error); free(error); error=NULL;
        free(reply.analysis.schema); reply.analysis.schema=dx_strdup("main");
        DefinitionOrigin *unchanged_owner=pending->owner;
        a->analysis_revision++;
        ORIGIN_CHECK(app_definition_origin_reply(a,&reply,&error)==DEFINITION_ORIGIN_STALE && pending->owner==unchanged_owner && !a->analysis.table); free(error); error=NULL;
        a->analysis_revision--;
        a->source_format.bytes=SIZE_MAX;
        ORIGIN_CHECK(app_definition_origin_reply(a,&reply,&error)==DEFINITION_ORIGIN_LIMIT && !a->analysis.table && definition_origin_state(pending->owner).phase==DEFINITION_ORIGIN_OPEN && !definition_origin_assets(pending->owner).primary); free(error); error=NULL;
        a->source_format.bytes=0;
        ORIGIN_CHECK(a->book.command_revision==command && a->request_id==request && a->analysis_revision==revision && a->book.history_branch==branch && s->undo_actions_count==undo && s->redo_actions_count==redo);
        Analysis old_actual=a->analysis,old_cached=s->analysis;
        a->source_format.bytes=SIZE_MAX;
        workspace_job(a,JOB_SAVE,origin_save_path);
        ORIGIN_CHECK(!a->io_busy && !a->busy && a->request_id==request && pending->owner==unchanged_owner &&
            !memcmp(&old_actual,&a->analysis,sizeof(Analysis)) && !memcmp(&old_cached,&s->analysis,sizeof(Analysis)));
        a->source_format.bytes=0;
        /* Save and navigation capture must preserve a pending current origin,
           including a cached definition whose default schema still lags. */
        ORIGIN_CHECK(capture_sheet(a));
        DefinitionOriginContext current_context=app_definition_origin_context(a); size_t scratch=0;
        ORIGIN_CHECK(pending->owner==unchanged_owner && definition_origin_scratch(&current_context,pending->owner,&scratch,&error)==DEFINITION_ORIGIN_READY);
        free(error); error=NULL;
        origin_command=command; origin_revision=revision; origin_branch=branch; origin_undo=undo; origin_redo=redo;
        origin_pending=pending->owner;
        analysis_free(&reply.analysis);
        workspace_job(a,JOB_SAVE,origin_save_path); ORIGIN_CHECK(a->io_busy); break;
    }
    case 2: {
        AppDefinitionOrigin *pending=app_definition_origin_find(a);
        ORIGIN_CHECK(!a->book.dirty && a->book.path && pending && pending->owner==origin_pending &&
            !a->analysis.table && definition_origin_state(origin_pending).phase==DEFINITION_ORIGIN_OPEN);
        Analysis old_actual=a->analysis,old_cached=s->analysis; size_t active=a->book.active;
        uint64_t request=a->request_id,revision=a->analysis_revision,epoch=definition_origin_state(origin_pending).epoch;
        a->source_format.bytes=SIZE_MAX; activate_sheet(a,0,true);
        ORIGIN_CHECK(a->book.active==active && a->request_id==request && a->analysis_revision==revision &&
            a->source_format.bytes==SIZE_MAX && definition_origin_state(origin_pending).epoch==epoch &&
            !memcmp(&old_actual,&a->analysis,sizeof(Analysis)) && !memcmp(&old_cached,&s->analysis,sizeof(Analysis)));
        a->source_format.bytes=0; activate_sheet(a,0,true); ORIGIN_CHECK(a->book.active==0); break;
    }
    case 3: {
        ORIGIN_CHECK(a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha"));
        Worksheet *held=a->book.sheets+1; AppDefinitionOrigin *pending=app_definition_origin_for_sheet(a,held);
        DefinitionOriginContext context=app_definition_origin_context(a); context.primary=&held->analysis; size_t scratch=0;
        ORIGIN_CHECK(pending && pending->owner==origin_pending && !held->analysis.table &&
            definition_origin_scratch(&context,origin_pending,&scratch,NULL)==DEFINITION_ORIGIN_READY);
        ORIGIN_CHECK(definition_origin_assets(app_definition_origin_find(a)->owner).primary==origin_immutable);
        activate_sheet(a,1,true); ORIGIN_CHECK(a->book.active==1);
        a->source_format.bytes=SIZE_MAX; break;
    }
    case 4: {
        AppDefinitionOrigin *pending=app_definition_origin_find(a);
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && !a->analysis.table && pending && pending->owner==origin_pending &&
            definition_origin_state(origin_pending).phase==DEFINITION_ORIGIN_OPEN);
        a->source_format.bytes=0;
        versions_open(a); a->version_view.selected=SIZE_MAX; history_open(a); ORIGIN_CHECK(a->io_busy); break;
    }
    case 5:
        ORIGIN_CHECK(historical(a) && app_preview_working(&a->history) && definition_origin_state(origin_pending).phase==DEFINITION_ORIGIN_OPEN);
        ORIGIN_CHECK(history_return(a)); break;
    case 6: {
        AppDefinitionOrigin *pending=app_definition_origin_find(a); DefinitionOriginContext context=app_definition_origin_context(a); size_t scratch=0;
        ORIGIN_CHECK(!historical(a) && a->book.active==1 && pending && pending->owner==origin_pending && !a->analysis.table &&
            definition_origin_scratch(&context,origin_pending,&scratch,NULL)==DEFINITION_ORIGIN_READY);
        ORIGIN_CHECK(a->book.command_revision==origin_command && a->book.history_branch==origin_branch && s->undo_actions_count==origin_undo && s->redo_actions_count==origin_redo);
        origin_revision=a->analysis_revision;
        /* A normal current reply completes the same owner after both Save
           captures, navigation and Working Head inspection. */
        request_page(a,false); origin_request=a->request_id; break;
    }
    case 7: {
        AppDefinitionOrigin *current=app_definition_origin_find(a);
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha") && current && definition_origin_state(current->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        ORIGIN_CHECK(a->book.command_revision==origin_command && a->request_id==origin_request && a->analysis_revision==origin_revision && a->book.history_branch==origin_branch && s->undo_actions_count==origin_undo && s->redo_actions_count==origin_redo);
        QueryReply same={.kind=JOB_PAGE,.id=a->request_id,.revision=a->analysis_revision,.analysis=a->analysis};
        ORIGIN_CHECK(app_definition_origin_reply(a,&same,NULL)==DEFINITION_ORIGIN_READY);
        ORIGIN_CHECK(app_definition_origin_bytes(a)>origin_initial_bytes);
        AppDefinitionOrigin *first=a->definition_origins->next;
        ORIGIN_CHECK(first && definition_origin_assets(first->owner).primary==origin_immutable);
        /* Capture/sheet switches cannot retire or rewrite the first arena. */
        ORIGIN_CHECK(capture_sheet(a)); ORIGIN_CHECK(definition_origin_assets(first->owner).primary==origin_immutable);
        Worksheet *other=a->book.sheets;
        ORIGIN_CHECK(cells_set(&other->cells,100,20,"Undo owner",NULL));
        worksheet_record(other,EDIT_CELLS); a->book.command_revision++; workbook_history_note_edit(&a->book); a->book.dirty=true;
        uint64_t command=a->book.command_revision,order=other->undo_order[other->undo_actions_count-1];
        a->source_format.bytes=SIZE_MAX; history(a,false);
        ORIGIN_CHECK(a->book.active==1 && a->book.command_revision==command &&
            other->undo_order[other->undo_actions_count-1]==order && !strcmp(cells_get(&other->cells,100,20),"Undo owner"));
        a->source_format.bytes=0; history(a,false);
        ORIGIN_CHECK(a->book.active==0 && !*cells_get(&a->book.sheets[0].cells,100,20)); break;
    }
    case 8: {
        ORIGIN_CHECK(workbook_history_available(&a->book,true)); history(a,true);
        ORIGIN_CHECK(!strcmp(cells_get(&a->book.sheets[0].cells,100,20),"Undo owner"));
        size_t count=a->book.count;
        a->source_format.bytes=SIZE_MAX; new_sheet(a);
        ORIGIN_CHECK(a->book.count==count+1 && a->book.active==count && a->analysis.kind==SOURCE_NONE && !a->page.column_count);
        a->source_format.bytes=0;
        a->book.dirty=false; a->document_action=DOCUMENT_NEW; finish_document_action(a); break;
    }
    case 9:
        ORIGIN_CHECK(!a->definition_origins && a->analysis.kind==SOURCE_NONE);
        ORIGIN_CHECK(app_definition_origin_bytes(a)==0); break;
    case 10:
        origin_result=1; printf("PASS: native primary origin workflow (11 stages, %u checks)\n",origin_checks); break;
    }
    origin_stage++;
}
int app_test_definition_origin_result(void) { return origin_result; }
unsigned app_test_definition_origin_stage(void) { return origin_stage; }
#undef ORIGIN_CHECK
