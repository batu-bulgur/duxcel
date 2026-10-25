/* Native new file/demo or DATABASE primary: actual probe first, one real session CREATE last.
   Old visible source/Page remain authoritative until complete admission. This
   does not activate retained topology events or a retained writer. */
static PrimarySourceDraftContext app_primary_source_context(App *a) {
    return (PrimarySourceDraftContext){&a->book,&a->analysis,&a->request_id,&a->analysis_revision,
        !a->io_busy && !a->step_preview.active,history_locked(a)};
}
static bool app_primary_source_current(App *a,char **error) {
    return primary_source_pending_check(app_primary_source_context(a),a->primary_source_pending_owner,error)==PRIMARY_TOPOLOGY_READY;
}
static bool app_primary_source_admit(App *a,size_t total) {
    size_t resident=app_primary_source_plans_owned(a);
    if (resident==SIZE_MAX || resident>total) { status(a,"Source creation ownership accounting is unavailable."); return false; }
    /* Resident stage owners are already charged by the common governor.
       Admit only the remaining peak, with one pre-reserved registry node. */
    size_t extra=total-resident;
    return source_format_add(&extra,sizeof(AppDefinitionOrigin)) && source_format_budget(a,NULL,0,extra,0,0,0);
}
static bool app_primary_source_begin(App *a,const Analysis *source,const char *name) {
    char *error=NULL;
    AppDefinitionOrigin *old=app_definition_origin_find(a);
    if (old && !old->admission_pending && definition_origin_state(old->owner).phase==DEFINITION_ORIGIN_OPEN) {
        DefinitionOriginStatus sealed=app_definition_origin_seal(a,old,0,&error);
        if (sealed!=DEFINITION_ORIGIN_READY && sealed!=DEFINITION_ORIGIN_APPLIED && sealed!=DEFINITION_ORIGIN_STALE) {
            status(a,"Finish resolving the current worksheet before opening another source. F5 retries its pending source.");
            free(error); return false;
        }
        free(error); error=NULL;
    }
    /* Reserve the probe AND its cancellation/stale-reply invalidation ID. */
    if (a->request_id>=UINT64_MAX-1 || a->analysis_revision==UINT64_MAX || !workbook_prepare_command_session(&a->book,a->book.active,&error)) {
        status(a,error ? error : "Source request or query revision space is exhausted."); free(error); return false;
    }
    uint64_t previous=a->request_id; a->request_id++;
    PrimarySourcePendingUsage info={0}; PrimarySourceDraftContext context=app_primary_source_context(a);
    if (primary_source_pending_begin_measure(context,source,name,&info,&error)!=PRIMARY_TOPOLOGY_READY) { a->request_id=previous; status(a,error); free(error); return false; }
    size_t reserve=info.required;
    if (!source_format_budget(a,NULL,0,reserve,sizeof(Analysis)+analysis_text_bytes(source),1,0)) { a->request_id=previous; return false; }
    PrimarySourcePending *pending=NULL;
    if (primary_source_pending_begin(context,source,name,reserve,&pending,&error)!=PRIMARY_TOPOLOGY_READY) {
        a->request_id=previous; status(a,error); free(error); return false;
    }
    SOURCE_FORMAT_CANCEL(a,NULL);
    worker_cancel(&a->worker); app_primary_source_cancel(a); a->primary_source_pending_owner=pending; a->primary_source_request=a->request_id;
    a->primary_source_navigation=(SheetView){.canvas_zoom=a->grid.zoom};
    QueryJob *job=dx_alloc(sizeof(*job)); job->kind=JOB_PAGE; job->id=a->request_id; job->revision=a->analysis_revision;
    job->analysis=analysis_copy(source); job->known_total=-1;
    a->busy=true; a->pending_kind=JOB_PAGE; a->pending_object_id=0; app_worker_submit(a,job);
    a->modal=MODAL_NONE; a->consume_grid_input=true;
    status(a,"Loading the source before creating its worksheet… Escape cancels."); return true;
}
static void app_primary_source_publish(App *a) {
    QueryReply *reply=a->primary_source_reply; if (!reply || !app_primary_source_pending(a)) return;
    PrimarySourceDraftContext context=app_primary_source_context(a); char *error=NULL; SheetSourceView *initial=NULL;
    if (reply->error) { error=dx_strdup(reply->error); goto rejected; }
    if (!app_primary_source_current(a,&error)) goto rejected;
    PrimarySourcePending *owner=a->primary_source_pending_owner; PrimarySourcePendingUsage usage;
    if (primary_source_pending_stage(owner)==PRIMARY_PENDING_DRAFT) {
        PrimarySourceDraftReply actual={reply->id,&reply->analysis,reply->error};
        if (primary_source_pending_reply_measure(context,owner,actual,&usage,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
        if (!app_primary_source_admit(a,usage.required)) goto pending;
        if (primary_source_pending_resolve(context,owner,actual,usage.required,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
    }
    if (primary_source_pending_stage(owner)==PRIMARY_PENDING_RESOLVED) {
        SheetSourceViewSpec view_spec={reply->page.columns,reply->page.column_count,a->primary_source_navigation}; SheetSourceViewInfo view_info;
        if (sheet_source_view_measure(view_spec,&view_info)!=SHEET_SOURCE_VIEW_OK) { error=dx_strdup("The source schema or initial navigation is invalid."); goto rejected; }
        if (!source_format_budget(a,NULL,0,view_info.owned,0,0,0)) goto pending;
        if (sheet_source_view_build(view_spec,view_info.owned,&initial)!=SHEET_SOURCE_VIEW_OK) goto rejected;
        PrimarySourceInitialSchema schema={reply->id,reply->revision,&reply->analysis,reply->page.columns,reply->page.column_count,reply->error};
        if (primary_source_pending_initial_measure(context,owner,initial,schema,&usage,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
        if (!app_primary_source_admit(a,usage.required)) goto pending;
        if (primary_source_pending_prepare_initial(context,owner,&initial,schema,usage.required,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
    }
    if (primary_source_pending_stage(owner)==PRIMARY_PENDING_SOURCE) {
        if (primary_source_pending_topology_measure(context,owner,&usage,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
        if (!app_primary_source_admit(a,usage.required)) goto pending;
        if (primary_source_pending_prepare_topology(context,owner,usage.required,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
    }
    if (primary_source_pending_topology_measure(context,owner,&usage,&error)!=PRIMARY_TOPOLOGY_READY) goto rejected;
    if (!app_primary_source_admit(a,usage.required)) goto pending;
    /* Scalar capture preserves navigation made while the probe was pending,
       without rebinding the exact Analysis/cache lease. */
    capture_sheet_view(a);
    PrimarySourcePublication published={0};
    PrimarySourceTopologyStatus result=primary_source_pending_apply(context,&a->primary_source_pending_owner,NULL,usage.required,&published,&error);
    if (result==PRIMARY_TOPOLOGY_LIMIT) goto pending;
    if (result!=PRIMARY_TOPOLOGY_APPLIED && result!=PRIMARY_TOPOLOGY_APPLIED_PENDING) goto rejected;
    /* Everything needed below was included in the prepublication reservation.
       No global governor check or source IO can reject this published session. */
    AppDefinitionOrigin *node=dx_alloc(sizeof(*node));
    *node=(AppDefinitionOrigin){.next=a->definition_origins,.document=a->book.id,.session=a->book.edit_session_id,.sheet=published.sheet,.owner=published.origin};
    published.origin=NULL;
    if (result==PRIMARY_TOPOLOGY_APPLIED_PENDING) {
        /* An incomplete postpublication owner cannot follow the independent App
           source without another typed handoff. Keep an explicit blocked slot,
           not stale borrowed authority or a fabricated replacement preimage. */
        definition_origin_free(&node->owner); node->admission_pending=true;
    }
    a->definition_origins=node;
    SOURCE_FORMAT_RESET(a); memset(a->source_locations,0,sizeof(a->source_locations)); clear_object_views(a);
    analysis_free(&a->analysis); a->analysis=published.candidate.source; published.candidate.source=(Analysis){0};
    page_free(&a->page); page_free(&a->spare); a->page=reply->page; reply->page=(Page){0};
    a->restore_view=true; a->active_readout.value.valid=false; a->page_stale=a->waiting_count_page=false;
    a->selected_object=a->filter_object=-1; a->pivot_focus=0; a->pivot_search_focus=false; a->reveal_tab=true;
    a->analysis_revision=published.primary_epoch; a->known_total=a->page.count_pending ? -1 : (int64_t)a->page.total; a->observed_rows=a->page.total;
    a->count_request_id=0; a->count_paused=a->count_deferred=false; a->fetch_failed=false; a->new_schema=true;
    /* A new owner never inherits a same-schema outgoing Grid's custom widths
       or distant virtual allocation. Worksheet.view is already committed. */
    grid_schema(&a->grid,&a->page,true);
    grid_project_widths(&a->grid,active_sheet(a)->view.widths,active_sheet(a)->view.column_count);
    a->relation_scroll=0; a->modal=MODAL_NONE; cache_pipeline(a); source_ready(a);
    if (result==PRIMARY_TOPOLOGY_APPLIED_PENDING) status(a,"Source worksheet opened. Retained origin admission remains unavailable.");
    primary_source_publication_free(&published); app_primary_source_cancel(a); free(error); return;
pending:
    sheet_source_view_free(&initial);
    status(a,"Source loaded. Publication needs memory; F5 retries and Escape cancels. The current worksheet is unchanged."); free(error); return;
rejected:
    sheet_source_view_free(&initial);
    status(a,error ? error : "Source open was not admitted. The current worksheet is unchanged.");
    free(error); app_primary_source_cancel(a);
}
static bool app_primary_source_reply(App *a,QueryReply *reply) {
    if (!app_primary_source_pending(a) || reply->kind!=JOB_PAGE || reply->object_id || reply->is_preview) return false;
    if (reply->revision!=a->analysis_revision) { reply_free(reply); app_primary_source_cancel(a); status(a,"The source probe belongs to an earlier source request."); return true; }
    reply_free(a->primary_source_reply); a->primary_source_reply=reply;
    app_primary_source_publish(a); return true;
}
