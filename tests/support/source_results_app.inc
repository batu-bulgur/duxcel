/* Private real-worker association proof; source movement routes stay disabled. */
static unsigned source_results_stage,source_results_wait,source_results_checks;
static int source_results_result;
static bool source_results_ready,source_results_wrong;
static uint64_t source_results_first,source_results_other;
#ifdef DUXCEL_FEATURE_SOURCE_MOVE
static uint64_t source_move_revision,source_move_high,source_move_ledger;
static void source_move_select(App *a,uint64_t row,size_t col) { grid_select_cell(&a->grid,row,col,false); a->grid.end_row=row+312; a->grid.end_col=col+12; }
#endif
#ifdef DUXCEL_FEATURE_SOURCE_ROWS
static uint64_t source_rows_revision,source_rows_high,source_rows_ledger,source_rows_width;
static size_t source_rows_redo;
static void source_rows_select(App *a,uint64_t row) { grid_select_cell(&a->grid,row,0,false); a->grid.selection=SELECT_ROWS; a->grid.end_row=row+1; }
static void source_rows_menu(App *a,int action) { a->modal=MODAL_CONTEXT; a->context_origin=(Vector2){300,200}; a->context_focus=action; a->context_scroll=0; a->chrome_focus=a->pivot_focus=0; ui_note_key_press(KEY_ENTER,0); }
#endif
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
        invalidate_object(a,1); a->selected_object=1; a->pivot_reveal_object=true; SR_CHECK(!source_results_prepare(a,false)); break;
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
#if !defined(DUXCEL_FEATURE_SOURCE_MOVE) && !defined(DUXCEL_FEATURE_SOURCE_ROWS)
        source_results_result=1; printf("PASS: native actual source result association workflow (13 stages, %u checks)\n",source_results_checks);
#endif
        break;
    }
#ifdef DUXCEL_FEATURE_SOURCE_MOVE
    case 13: {
        char *error=NULL; SR_CHECK(cells_set(&s->cells,329,15,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        grid_select_cell(&a->grid,329,15,false); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; SR_CHECK(apply_format(a,FORMAT_BOLD,bold));
        source_move_revision=a->book.command_revision; source_move_high=s->objects.next_id; source_move_ledger=s->source_versions.high_water;
        source_move_select(a,329,15); begin_move_range(a,false); SR_CHECK(a->modal==MODAL_MOVE_RANGE); input_set(&a->input,"Q1001"); a->range_move_focus=1; ui_note_key_press(KEY_ENTER,0); free(error); break;
    }
    case 14:
        SR_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_move_revision+1 && s->objects.items[0].row==1001 && s->objects.items[0].col==17 && s->undo_actions[s->undo_actions_count-1]==EDIT_RANGE_FORMAT);
        SR_CHECK(!strcmp(cells_get(&s->cells,1000,16),"9007199254740993.0123456789") && (format_at(&s->formats,1000,16).flags&FORMAT_BOLD) && !*cells_get(&s->cells,329,15)); history(a,false); break;
    case 15:
        SR_CHECK(s->objects.items[0].row==330 && s->objects.items[0].col==16 && !strcmp(cells_get(&s->cells,329,15),"9007199254740993.0123456789")); history(a,true); break;
    case 16: {
        SR_CHECK(s->objects.items[0].row==1001 && !strcmp(cells_get(&s->cells,1000,16),"9007199254740993.0123456789"));
        char *error=NULL; SR_CHECK(cells_set(&s->cells,2000,16,"Replace destination",&error)); worksheet_record(s,EDIT_CELLS);
        source_move_select(a,1000,16); SR_CHECK(!request_range_move(a,1000,0,false,&error) && a->modal==MODAL_REPLACE_RANGE && a->replace_manual_target); free(error); break;
    }
    case 17:
        cancel_range_replace(a); SR_CHECK(!a->replace_manual_target && s->objects.items[0].row==1001 && !strcmp(cells_get(&s->cells,2000,16),"Replace destination"));
        { char *error=NULL; SR_CHECK(!request_range_move(a,1000,0,false,&error) && a->modal==MODAL_REPLACE_RANGE && a->replace_manual_target); free(error); }
        source_move_revision=a->book.command_revision; SR_CHECK(confirm_range_replace(a)); break;
    case 18:
        SR_CHECK(a->book.command_revision==source_move_revision+1 && s->objects.items[0].row==2001 && !strcmp(cells_get(&s->cells,2000,16),"9007199254740993.0123456789")); history(a,false); break;
    case 19: {
        SR_CHECK(s->objects.items[0].row==1001 && !strcmp(cells_get(&s->cells,2000,16),"Replace destination") && !strcmp(cells_get(&s->cells,1000,16),"9007199254740993.0123456789"));
        /* Undo changes the selection owner; explicitly select the restored full rectangle. */
        source_move_select(a,1000,16); char *error=NULL; SR_CHECK(!request_range_move(a,1000,0,false,&error) && a->replace_manual_target); free(error);
        source_move_revision=a->book.command_revision; run_action(a,ACTION_REFRESH); SR_CHECK(!confirm_range_replace(a) && !a->replace_manual_target); break;
    }
    case 20:
        if (a->page.count_pending || a->count_request_id) return;
        SR_CHECK(s->objects.items[0].row==1001 && a->book.command_revision==source_move_revision && !strcmp(cells_get(&s->cells,2000,16),"Replace destination") && (format_at(&s->formats,1000,16).flags&FORMAT_BOLD));
        SR_CHECK(s->objects.next_id==source_move_high && s->source_versions.high_water==source_move_ledger && a->analysis.kind==SOURCE_CSV && !a->replace_manual_target);
        source_results_result=1; printf("PASS: native source mixed move workflow (21 stages, %u checks)\n",source_results_checks); break;
#endif
#ifdef DUXCEL_FEATURE_SOURCE_ROWS
    case 13: {
        char *error=NULL; SR_CHECK(cells_set(&s->cells,740,15,"9007199254740993.0123456789",&error)); worksheet_record(s,EDIT_CELLS);
        grid_select_cell(&a->grid,740,15,false); CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; SR_CHECK(apply_format(a,FORMAT_BOLD,bold)); free(error);
        source_rows_high=s->objects.next_id; source_rows_ledger=s->source_versions.high_water; source_rows_width=s->width_revision; source_rows_revision=a->book.command_revision; break;
    }
    case 14: source_rows_select(a,320); source_rows_menu(a,0); break;
    case 15:
        SR_CHECK(a->modal==MODAL_NONE && a->book.command_revision==source_rows_revision+1 && s->objects.items[0].row==332 && s->undo_actions[s->undo_actions_count-1]==EDIT_RANGE_FORMAT);
        SR_CHECK(!strcmp(cells_get(&s->cells,742,15),"9007199254740993.0123456789") && (format_at(&s->formats,742,15).flags&FORMAT_BOLD) && a->grid.selection==SELECT_ROWS && a->grid.anchor_row==320 && a->grid.end_row==321); history(a,false); break;
    case 16:
        SR_CHECK(s->objects.items[0].row==330 && !strcmp(cells_get(&s->cells,740,15),"9007199254740993.0123456789")); history(a,true); break;
    case 17:
        SR_CHECK(s->objects.items[0].row==332 && !strcmp(cells_get(&s->cells,742,15),"9007199254740993.0123456789")); source_rows_select(a,320); source_rows_menu(a,2); break;
    case 18:
        SR_CHECK(s->objects.items[0].row==330 && !strcmp(cells_get(&s->cells,740,15),"9007199254740993.0123456789")); source_rows_revision=a->book.command_revision; source_rows_select(a,100); source_rows_menu(a,0); break;
    case 19:
        SR_CHECK(a->book.command_revision==source_rows_revision && s->objects.items[0].row==330); source_rows_select(a,331); source_rows_menu(a,0); break;
    case 20:
        SR_CHECK(a->book.command_revision==source_rows_revision && s->objects.items[0].row==330); history(a,false); break;
    case 21:
        SR_CHECK(s->objects.items[0].row==332); source_rows_revision=a->book.command_revision; source_rows_redo=s->redo_actions_count; SR_CHECK(source_rows_redo); source_rows_select(a,900); source_rows_menu(a,0); break;
    case 22:
        SR_CHECK(a->book.command_revision==source_rows_revision && s->redo_actions_count==source_rows_redo && s->objects.items[0].row==332); history(a,true); break;
    case 23:
        SR_CHECK(s->objects.items[0].row==330 && !strcmp(cells_get(&s->cells,740,15),"9007199254740993.0123456789") && (format_at(&s->formats,740,15).flags&FORMAT_BOLD));
        SR_CHECK(s->objects.next_id==source_rows_high && s->source_versions.high_water==source_rows_ledger && s->width_revision==source_rows_width && a->analysis.kind==SOURCE_CSV);
        source_results_result=1; printf("PASS: native source mixed row workflow (24 stages, %u checks)\n",source_results_checks); break;
#endif
    }
    source_results_stage++;
}
int app_test_source_results_result(void) { if (source_results_result) { free(source_results_target); source_results_target=NULL; } return source_results_result; }
unsigned app_test_source_results_stage(void) { return source_results_stage; }
#undef SR_CHECK
