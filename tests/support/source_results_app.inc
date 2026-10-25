/* Private real-worker association proof; source movement routes stay disabled. */
static unsigned source_results_stage,source_results_wait,source_results_checks;
static int source_results_result;
static bool source_results_ready,source_results_wrong;
static uint64_t source_results_first,source_results_other;
static RangeSourcePolicy source_results_policy;
static RangeResultContext source_results_resolver;
static RangeCommandTarget *source_results_target;
#define SR_CHECK(x) do { source_results_checks++; if (!(x)) { fprintf(stderr,"FAIL actual source result stage %u:%d: %s (%s)\n",source_results_stage,__LINE__,#x,a->message ? a->message : ""); source_results_result=-1; return; } } while (0)
static bool source_results_resolve(void *context,uint64_t id,RangeResultProof *out) {
    return app_range_result(context,source_results_wrong && id==source_results_first ? source_results_other : id,out);
}
static RangeCommandContext source_results_context(App *a) {
    RangeCommandContext c=manual_range_context(a); source_results_ready=c.command.ready;
    source_results_policy=(RangeSourcePolicy){&a->page.total,&a->page.column_count,&a->page.count_pending,&a->fetch_failed,&source_results_ready};
    source_results_resolver=(RangeResultContext){source_results_resolve,a}; c.source=&source_results_policy; c.results=&source_results_resolver; return c;
}
static bool source_results_prepare(App *a,bool retain) {
    RangeCommandContext c=source_results_context(a); RangeCommandTarget *target=dx_alloc(sizeof(*target));
    bool ready=workbook_prepare_range_command(&c,a->book.active,target,NULL);
    if (ready && retain) { free(source_results_target); source_results_target=target; } else free(target);
    return ready;
}
static RangeCommandResult source_results_noop(App *a) {
    RangeCommandContext c=source_results_context(a);
    return workbook_apply_range_command_recorded(&c,source_results_target,(RangeCommand){.operation=RANGE_OPERATION_MOVE,.range={1000,1001,100,101}},NULL,NULL);
}
static void source_results_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (source_results_result || !s || a->busy || a->io_busy) return;
    /* Observe the first real partial result before the automatic count scheduler. */
    if (source_results_stage!=1 && source_results_stage!=4 && ++source_results_wait<24) return;
    source_results_wait=0; SR_CHECK(a->modal!=MODAL_ERROR);
    switch (source_results_stage) {
    case 0: open_source(a,"build/tests/generated/design-definition.csv",false); break;
    case 1:
        if (!a->page.column_count) return;
        SR_CHECK(a->analysis.kind==SOURCE_CSV && a->page.count_pending && a->page.row_count>0 && a->page.row_count<=a->page.total && a->page.total<310);
        pause_count(a); SR_CHECK(!source_results_prepare(a,false));
        a->count_paused=false; schedule_count(a); break;
    case 2:
        if (a->page.count_pending || a->count_request_id) return;
        SR_CHECK(a->page.total==310 && a->page.column_count==12 && a->page.row_count<a->page.total && source_results_prepare(a,true));
        run_action(a,ACTION_REFRESH); break;
    case 3: {
        if (a->page.count_pending || a->count_request_id || a->refresh_plans) return;
        uint64_t revision=a->book.command_revision;
        SR_CHECK(a->page.total==310 && source_results_noop(a).status==RANGE_COMMAND_STALE && a->book.command_revision==revision); free(source_results_target); source_results_target=NULL;
        grid_select_cell(&a->grid,0,14,false); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; SR_CHECK(apply_format(a,FORMAT_BOLD,bold));
        grid_select_cell(&a->grid,330,16,false); SR_CHECK(import_table(a,"build/tests/generated/design-definition.csv")); break;
    }
    case 4:
        if (!a->objects[0].page.column_count) return;
        SR_CHECK(s->objects.count==1 && a->objects[0].page.count_pending && a->objects[0].page.total<310 && !source_results_prepare(a,false));
        a->objects[0].count_paused=false; schedule_table_count(a); break;
    case 5: {
        if (!object_ready(a,0) || a->count_request_id) return;
        RangeResultProof proof={0}; SR_CHECK(a->objects[0].page.total==310 && s->objects.items[0].rows==312 && a->objects[0].page.row_count<310 && app_range_result(a,s->objects.items[0].id,&proof));
        SR_CHECK(proof.object==s->objects.items[0].id && proof.owner==&a->objects[0] && proof.page==&a->objects[0].page && proof.query_epoch==s->objects.items[0].revision && proof.source_epoch==s->objects.items[0].revision && proof.ready);
        a->selected_object=0; create_pivot(a); SR_CHECK(s->objects.count==2); break;
    }
    case 6: {
        SR_CHECK(!s->objects.items[1].pivot.counts[PIVOT_VALUES] && source_results_prepare(a,true));
        a->selected_object=1; PivotDefinition p={0}; SR_CHECK(pivot_assign(&p,PIVOT_ROWS,"Region",AGG_SUM,NULL) && pivot_assign(&p,PIVOT_VALUES,"Amount (USD)",AGG_SUM,NULL) && set_pivot_definition(a,&p)); break;
    }
    case 7:
        if (!object_ready(a,1)) return;
        SR_CHECK(source_results_noop(a).status==RANGE_COMMAND_STALE); free(source_results_target); source_results_target=NULL;
        source_results_first=s->objects.items[1].id; a->selected_object=0; create_pivot(a);
        SR_CHECK(s->objects.count==3); a->selected_object=2;
        { PivotDefinition p=pivot_copy(&s->objects.items[1].pivot); SR_CHECK(set_pivot_definition(a,&p)); } break;
    case 8: {
        if (!object_ready(a,1) || !object_ready(a,2)) return;
        const SheetObject *first=s->objects.items+1,*other=s->objects.items+2; source_results_other=other->id;
        SR_CHECK(first->rows==other->rows && first->columns==other->columns && a->objects[1].page.total==a->objects[2].page.total && source_results_prepare(a,true));
        uint64_t revision=a->book.command_revision; source_results_wrong=true;
        SR_CHECK(source_results_noop(a).status==RANGE_COMMAND_STALE && a->book.command_revision==revision); source_results_wrong=false;
        SR_CHECK(source_results_noop(a).status==RANGE_COMMAND_NOOP && a->book.command_revision==revision);
        invalidate_object(a,1); SR_CHECK(!source_results_prepare(a,false)); break;
    }
    case 9:
        if (!object_ready(a,1)) return;
        SR_CHECK(source_results_noop(a).status==RANGE_COMMAND_STALE && source_results_prepare(a,true)); break;
    case 10: {
        a->selected_object=1; PivotDefinition invalid=pivot_copy(&s->objects.items[1].pivot);
        free(invalid.fields[PIVOT_VALUES][0].name); invalid.fields[PIVOT_VALUES][0].name=dx_strdup("MissingProofField"); SR_CHECK(set_pivot_definition(a,&invalid)); break;
    }
    case 11: {
        if (!a->objects[1].failed) return;
        RangeResultProof failed={0}; SR_CHECK(app_range_result(a,s->objects.items[1].id,&failed) && failed.failed && !failed.ready && !source_results_prepare(a,false) && source_results_noop(a).status==RANGE_COMMAND_STALE);
        a->selected_object=1; PivotDefinition valid=pivot_copy(&s->objects.items[2].pivot); SR_CHECK(set_pivot_definition(a,&valid)); break;
    }
    case 12: {
        if (!object_ready(a,0) || !object_ready(a,1) || !object_ready(a,2)) return;
        RangeResultProof proof={0}; SR_CHECK(app_range_result(a,source_results_first,&proof) && proof.ready && proof.object==source_results_first && proof.owner==&a->objects[1] && proof.definition==&s->objects.items[0].source && proof.query_epoch==s->objects.items[1].revision && proof.source_epoch==s->objects.items[0].revision && proof.request==a->request_id);
        SR_CHECK(source_results_noop(a).status==RANGE_COMMAND_STALE && source_results_prepare(a,true) && source_results_noop(a).status==RANGE_COMMAND_NOOP && !s->cells.count);
        source_results_result=1; printf("PASS: native actual source result association workflow (13 stages, %u checks)\n",source_results_checks); break;
    }
    }
    source_results_stage++;
}
int app_test_source_results_result(void) { if (source_results_result) { free(source_results_target); source_results_target=NULL; } return source_results_result; }
unsigned app_test_source_results_stage(void) { return source_results_stage; }
#undef SR_CHECK
