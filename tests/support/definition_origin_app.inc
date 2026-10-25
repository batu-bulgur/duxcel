/* Private real-worker primary default reply plus typed admission pressure tests. */
static unsigned origin_stage,origin_wait,origin_checks;
static int origin_result;
static const void *origin_immutable;
static size_t origin_initial_bytes,origin_undo,origin_redo;
static uint64_t origin_command,origin_branch,origin_revision,origin_request;
#define ORIGIN_CHECK(x) do { origin_checks++; if (!(x)) { fprintf(stderr,"FAIL primary origin stage %u:%d: %s (%s)\n",origin_stage,__LINE__,#x,a->message ? a->message : ""); origin_result=-1; return; } } while (0)
static void definition_origin_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (origin_result || !s || a->busy || a->io_busy || ++origin_wait<24) return;
    origin_wait=0; ORIGIN_CHECK(a->modal!=MODAL_ERROR);
    switch (origin_stage) {
    case 0: {
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha"));
        AppDefinitionOrigin *node=app_definition_origin_find(a); ORIGIN_CHECK(node && definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        origin_immutable=definition_origin_assets(node->owner).primary; origin_initial_bytes=app_definition_origin_bytes(a);
        ORIGIN_CHECK(origin_immutable && origin_initial_bytes>definition_origin_owner_bytes());
        open_source(a,"build/tests/generated/source-connection-a.duckdb",false);
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
        origin_command=command; origin_revision=revision; origin_branch=branch; origin_undo=undo; origin_redo=redo;
        analysis_free(&reply.analysis);
        /* A normal new Page request retries the still-authored NULL default.
           Exact unchanged model fences permit replacing only the old request. */
        request_page(a,false); origin_request=a->request_id; break;
    }
    case 2: {
        AppDefinitionOrigin *current=app_definition_origin_find(a);
        ORIGIN_CHECK(!a->page_stale && a->page.total==3 && a->analysis.table && !strcmp(a->analysis.table,"alpha") && current && definition_origin_state(current->owner).phase==DEFINITION_ORIGIN_ADMITTED);
        ORIGIN_CHECK(a->book.command_revision==origin_command && a->request_id==origin_request && a->analysis_revision==origin_revision && a->book.history_branch==origin_branch && s->undo_actions_count==origin_undo && s->redo_actions_count==origin_redo);
        QueryReply same={.kind=JOB_PAGE,.id=a->request_id,.revision=a->analysis_revision,.analysis=a->analysis};
        ORIGIN_CHECK(app_definition_origin_reply(a,&same,NULL)==DEFINITION_ORIGIN_READY);
        ORIGIN_CHECK(app_definition_origin_bytes(a)>origin_initial_bytes);
        AppDefinitionOrigin *first=a->definition_origins->next;
        ORIGIN_CHECK(first && definition_origin_assets(first->owner).primary==origin_immutable);
        /* Capture/sheet switches cannot retire or rewrite the first arena. */
        capture_sheet(a); ORIGIN_CHECK(definition_origin_assets(first->owner).primary==origin_immutable);
        a->book.dirty=false; a->document_action=DOCUMENT_NEW; finish_document_action(a); break;
    }
    case 3:
        ORIGIN_CHECK(!a->definition_origins && a->analysis.kind==SOURCE_NONE);
        ORIGIN_CHECK(app_definition_origin_bytes(a)==0); break;
    case 4:
        origin_result=1; printf("PASS: native primary origin workflow (5 stages, %u checks)\n",origin_checks); break;
    }
    origin_stage++;
}
int app_test_definition_origin_result(void) { return origin_result; }
unsigned app_test_definition_origin_stage(void) { return origin_stage; }
#undef ORIGIN_CHECK
