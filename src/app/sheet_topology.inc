/* Session topology uses the same model owner as direct C commands. Retained
   writer/traversal activation is intentionally absent from this adapter. */
static bool sheet_topology_error(char **error,const char *message) {
    if (error && !*error) *error=dx_strdup(message); return false;
}
static bool sheet_topology_ready(App *a,char **error) {
    if (a->io_busy || a->step_preview.active || history_locked(a))
        return sheet_topology_error(error,"Return to an available Working Head before changing worksheets.");
    return true;
}
static size_t sheet_topology_activation_staging(App *a,const Worksheet *detached) {
    size_t bytes=analysis_text_bytes(&a->analysis);
    if (bytes==SIZE_MAX) return SIZE_MAX;
    for (size_t i=0;i<a->book.count;i++) {
        size_t next=analysis_text_bytes(&a->book.sheets[i].analysis); if (next==SIZE_MAX) return SIZE_MAX; if (next>bytes) bytes=next;
    }
    if (detached) { size_t next=analysis_text_bytes(&detached->analysis); if (next==SIZE_MAX) return SIZE_MAX; if (next>bytes) bytes=next; }
    return bytes>SIZE_MAX-sizeof(Analysis) ? SIZE_MAX : bytes+sizeof(Analysis);
}
static bool app_sheet_topology_admit(App *a,size_t staging,size_t activation,size_t retained,char **error) {
    if (staging==SIZE_MAX || activation==SIZE_MAX || staging>SIZE_MAX-activation)
        return sheet_topology_error(error,"Worksheet operation allocation exceeds its bounded admission.");
#ifndef DUXCEL_BROWSER_HOST
    FormatBudgetStatus result=source_format_budget_status(a,NULL,staging+activation,0,0,0,retained);
    if (result!=FORMAT_BUDGET_OK) return sheet_topology_error(error,workbook_format_budget_message(result));
#else
    (void)a; (void)retained;
#endif
    return true;
}
static bool apply_sheet_presence(App *a,size_t index,bool duplicate,char **error) {
    if (!sheet_topology_ready(a,error) || index>=a->book.count) return false;
    if (!duplicate && !a->book.sheets[index].hidden && workbook_visible_count(&a->book)<=1)
        return sheet_topology_error(error,"Keep at least one worksheet visible.");
    if (duplicate && a->book.count>=DX_MAX_SHEETS)
        return sheet_topology_error(error,"A workspace supports up to 64 worksheets.");
    if (!workbook_prepare_command_session(&a->book,index,error)) return false;
    Worksheet *s=a->book.sheets+index; const Analysis *actual=index==a->book.active ? &a->analysis : &s->analysis;
    ObjectSnapshot objects={s->objects.items,s->objects.count};
    DefinitionBaselineStatus baseline=definition_baseline_status((DefinitionDependencyView){actual,&objects});
    size_t adoption=duplicate ? workbook_lifecycle_duplicate_staging(&a->book,index) : workbook_lifecycle_remove_staging(&a->book,index);
    size_t activation=sheet_topology_activation_staging(a,NULL);
    /* Existing session commands legitimately support unresolved/offline database
       definitions. Choose that route BEFORE typed preparation, without inventing
       a topology asset or requiring source IO. Typed errors never fall back. */
    if (baseline==DEFINITION_BASELINE_UNRESOLVED) {
        if (!app_sheet_topology_admit(a,adoption,activation,0,error)) return false;
        return workbook_sheet_command(&a->book,(WorkbookSheetCommand){.operation=duplicate ? SHEET_DUPLICATE : SHEET_DELETE,.sheet=index},NULL,error);
    }
    if (baseline!=DEFINITION_BASELINE_READY) return sheet_topology_error(error,"The worksheet definition has an invalid source owner.");
    if (duplicate) {
        SheetTopologyDuplicateContext c={&a->book,&a->request_id,actual,s->id,0,true,false,adoption}; SheetTopologyDuplicateMeasure usage;
        SheetTopologyDuplicateStatus result=sheet_topology_duplicate_measure(c,s->id,&usage);
        if (result!=SHEET_DUPLICATE_READY || usage.staging>SIZE_MAX-adoption || !app_sheet_topology_admit(a,usage.staging+adoption,activation,usage.packet_bytes,error))
            return sheet_topology_error(error,"The complete worksheet duplicate is unavailable.");
        c.reserved=usage.staging; SheetTopologyDuplicatePlan *plan=NULL;
        result=sheet_topology_duplicate_prepare(c,s->id,&plan,error);
        if (result==SHEET_DUPLICATE_READY) result=sheet_topology_duplicate_apply(c,&plan,NULL,0,error);
        sheet_topology_duplicate_free(&plan); return result==SHEET_DUPLICATE_APPLIED || sheet_topology_error(error,"The worksheet changed before duplicate adoption.");
    }
    SheetTopologyRemoveContext c={&a->book,&a->request_id,actual,s->id,0,true,false,adoption}; SheetTopologyRemoveMeasure usage;
    SheetTopologyRemoveStatus result=sheet_topology_remove_measure(c,s->id,&usage);
    if (result!=SHEET_REMOVE_READY || usage.staging>SIZE_MAX-adoption || !app_sheet_topology_admit(a,usage.staging+adoption,activation,usage.packet_bytes,error))
        return sheet_topology_error(error,"The complete worksheet removal is unavailable.");
    c.reserved=usage.staging; SheetTopologyRemovePlan *plan=NULL;
    result=sheet_topology_remove_prepare(c,s->id,&plan,error);
    if (result==SHEET_REMOVE_READY) result=sheet_topology_remove_apply(c,&plan,NULL,0,error);
    sheet_topology_remove_free(&plan); return result==SHEET_REMOVE_APPLIED || sheet_topology_error(error,"The worksheet changed before removal adoption.");
}
static bool replay_sheet_presence(App *a,bool redo,const WorkbookLifecyclePresence *selected,char **error) {
    if (!sheet_topology_ready(a,error) || !selected || !selected->packet) return false;
    const Analysis *actual=!selected->detached && selected->worksheet->id==active_sheet(a)->id ? &a->analysis : &selected->worksheet->analysis;
    SheetTopologyReplayMeasure usage; SheetTopologyReplayStatus result=sheet_topology_replay_measure(selected->packet,&usage);
    size_t adoption=workbook_lifecycle_presence_staging(&a->book,redo),activation=sheet_topology_activation_staging(a,selected->detached ? selected->worksheet : NULL);
    if (result!=SHEET_REPLAY_READY || usage.staging>SIZE_MAX-adoption || !app_sheet_topology_admit(a,usage.staging+adoption,activation,0,error))
        return sheet_topology_error(error,"The complete worksheet history transfer is unavailable.");
    SheetTopologyReplayContext c={&a->book,&a->request_id,actual,selected->sheet,usage.staging,adoption,true,false}; SheetTopologyReplayPlan *plan=NULL;
    result=sheet_topology_replay_prepare(c,selected->packet,redo,selected->order,&plan,error);
    if (result==SHEET_REPLAY_READY) result=sheet_topology_replay_apply(c,&plan,error);
    sheet_topology_replay_free(&plan); return result==SHEET_REPLAY_APPLIED || sheet_topology_error(error,"The worksheet history owner changed before adoption.");
}
