#include "../workbook/source_format_command.h"
/* Native raw-source formatting controller. App owns one captured transaction;
   the worker owns independent jobs/replies. No paint-time I/O. */
#define SOURCE_FORMAT_CAPTURE_BYTES (256u*1024u)
#define SOURCE_FORMAT_SQL_RESERVE (8u*1024u*1024u)
#define SOURCE_FORMAT_IO_RESERVE (16u*1024u*1024u)
typedef struct {
    uint64_t id,revision,row,rows;
    size_t column,columns,index;
    const Analysis *source;
    const Page *page;
    bool ready;
} SourceFormatOwner;
static void source_format_result_page(App *a,const SourceFormatOwner *owner,SourceFormatProof *proof);
static bool source_format_owner(App *a,uint64_t owner,SourceFormatOwner *out) {
    Worksheet *sheet=active_sheet(a); if (!sheet) return false;
    if (!owner) {
        *out=(SourceFormatOwner){.revision=a->analysis_revision,.source=&a->analysis,.page=&a->page,
            .ready=!a->page_stale && !a->fetch_failed}; return true;
    }
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *object=sheet->objects.items+i; if (object->id!=owner || object->kind!=OBJECT_TABLE) continue;
        const ObjectView *view=a->objects+i;
        *out=(SourceFormatOwner){.id=owner,.revision=object->revision,.row=object->row,.rows=object->rows,
            .column=object->col,.columns=object->columns,.index=i,.source=&object->source,.page=&view->page,
            .ready=view->id==owner && view->revision==object->revision && view->source_revision==object->revision &&
                !view->dirty && !view->failed && !view->waiting_count}; return true;
    }
    return false;
}
static SourceFormatTable *source_format_table(App *a,uint64_t owner,bool create) {
    SourceFormatTable *empty=NULL;
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) {
        SourceFormatTable *slot=a->source_format_tables+i;
        if (slot->owner==owner) return slot;
        if (!slot->owner && !empty) empty=slot;
    }
    if (create && empty) empty->owner=owner;
    return create ? empty : NULL;
}
static SourceFormatProof *source_format_owner_proof(App *a,uint64_t owner,bool create) {
    if (!owner) return &a->source_format_proof;
    SourceFormatTable *slot=source_format_table(a,owner,create); return slot ? &slot->proof : NULL;
}
static SourceFormatAttempt *source_format_owner_attempt(App *a,uint64_t owner,bool create) {
    if (!owner) return &a->source_format_attempt;
    SourceFormatTable *slot=source_format_table(a,owner,create); return slot ? &slot->attempt : NULL;
}
static const SourceVersion *source_format_owner_version(App *a,uint64_t owner,uint64_t version) {
    Worksheet *sheet=active_sheet(a); if (!sheet || !version) return NULL;
    for (size_t i=0;i<sheet->source_versions.count;i++) {
        const SourceVersion *entry=sheet->source_versions.items+i;
        if (entry->owner==owner && entry->version==version) return entry;
    }
    return NULL;
}
static const SourceVersion *source_format_version(App *a,uint64_t version) { return source_format_owner_version(a,0,version); }
static void source_format_intent_free(SourceFormatIntent *intent) {
    analysis_free(&intent->source); page_free(&intent->schema);
    source_packet_free(&intent->raw); *intent=(SourceFormatIntent){0};
}
static void source_format_proof_free(SourceFormatProof *proof) {
    source_packet_free(&proof->raw); source_packet_free(&proof->result); *proof=(SourceFormatProof){0};
}
static void source_format_proof_adopt(App *a,SourceFormatProof *proof,SourceFormatIntent *intent) {
    /* Reuse the owned canonical cache across raw formatting/no-op commands.
       Publication still matches its complete RAW and actual definition. */
    SourcePacket result=proof->result; proof->result=(SourcePacket){0}; source_format_proof_free(proof);
    *proof=(SourceFormatProof){.raw=intent->raw,.result=result,.digest=intent->digest,
        .document=a->format_document,.version=intent->version,.sheet=a->book.active};
    intent->raw=(SourcePacket){0};
}
static void source_format_cancel(App *a,const char *message) {
    SourceFormatIntent *intent=&a->source_format; if (!intent->active) return;
    if (a->request_id==intent->request) { worker_cancel(&a->worker); a->request_id++; a->busy=false; }
    source_format_intent_free(intent);
    if (message) status(a,message);
}
static void source_format_reset(App *a) {
    source_format_cancel(a,NULL); source_format_proof_free(&a->source_format_proof);
    a->source_format_proof=(SourceFormatProof){0}; a->source_format_attempt=(SourceFormatAttempt){0};
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) {
        source_format_proof_free(&a->source_format_tables[i].proof); a->source_format_tables[i]=(SourceFormatTable){0};
    }
    if (a->format_document!=UINT64_MAX) a->format_document++;
}
static void source_format_request(App *a) {
    /* Only a superseded automatic attempt can retry after a new settled Page.
       Failed proofs and explicit Escape stay terminal for the same observation. */
    if (a->source_format.active && a->source_format.restore) {
        SourceFormatAttempt *attempt=source_format_owner_attempt(a,a->source_format.owner,false);
        if (attempt) attempt->valid=false;
    }
    source_format_cancel(a,"Source formatting cancelled by a new data request.");
}
static bool source_format_epoch(App *a) {
    const SourceFormatIntent *intent=&a->source_format; Worksheet *sheet=active_sheet(a); SourceFormatOwner owner;
    return intent->active && sheet && source_format_owner(a,intent->owner,&owner) && owner.ready &&
        intent->document==a->format_document && intent->sheet==a->book.active && intent->request==a->request_id &&
        intent->analysis_revision==a->analysis_revision && intent->owner_revision==owner.revision &&
        intent->origin_row==owner.row && intent->origin_column==owner.column &&
        intent->origin_rows==owner.rows && intent->origin_columns==owner.columns &&
        intent->format_revision==sheet->formats.revision && intent->cell_revision==sheet->cells.revision &&
        intent->high_water==sheet->source_versions.high_water && intent->source.source_version==owner.source->source_version &&
        !a->io_busy && !a->step_preview.active;
}
static bool source_format_fence(App *a) {
    SourceFormatOwner owner;
    return source_format_epoch(a) && source_format_owner(a,a->source_format.owner,&owner) &&
        analysis_same_result(&a->source_format.source,owner.source);
}
static void source_format_tick(App *a) {
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) {
        SourceFormatTable *slot=a->source_format_tables+i; SourceFormatOwner owner;
        if (slot->owner && !source_format_owner(a,slot->owner,&owner)) {
            source_format_proof_free(&slot->proof); *slot=(SourceFormatTable){0};
        }
    }
    /* Result validation parses owned BETWEEN/composite predicates. Keep it at
       reply/publication boundaries; ordinary frames need only the captured
       epochs. Supported definition changes cancel or advance these epochs. */
    if (a->source_format.active && !source_format_epoch(a))
        source_format_cancel(a,"Formatting cancelled because its source, worksheet, placement or formatting changed.");
}
static void source_format_page_owner(App *a,uint64_t id) {
    SourceFormatProof *proof=source_format_owner_proof(a,id,false); if (!proof) return;
    SourceFormatOwner owner; proof->ready=proof->spare_ready=proof->result_ready=proof->tokens_ready=false;
    if (!source_format_owner(a,id,&owner) || !owner.ready || !proof->raw.data ||
        proof->document!=a->format_document || proof->sheet!=a->book.active || proof->version!=owner.source->source_version) return;
    const SourceVersion *entry=source_format_owner_version(a,id,proof->version);
    if (!entry || entry->raw.bytes!=proof->raw.bytes || memcmp(entry->raw.data,proof->raw.data,proof->raw.bytes) ||
        !source_version_page_verified(entry,id,proof->version,owner.source,owner.page,
            SOURCE_DIGEST_OK,&proof->digest,duckdb_library_version(),NULL)) return;
    proof->analysis_revision=owner.revision; proof->ready=true;
    proof->spare_ready=!id && a->spare.column_count &&
        source_version_page_verified(entry,0,proof->version,owner.source,&a->spare,
            SOURCE_DIGEST_OK,&proof->digest,duckdb_library_version(),NULL);
    source_format_result_page(a,&owner,proof);
}
static void source_format_page(App *a) {
    source_format_page_owner(a,0);
}
static bool source_format_owner_verified(App *a,uint64_t id) {
    SourceFormatOwner owner; SourceFormatProof *proof=source_format_owner_proof(a,id,false);
    return proof && proof->ready && source_format_owner(a,id,&owner) && owner.ready &&
        proof->document==a->format_document && proof->sheet==a->book.active &&
        proof->version==owner.source->source_version && proof->analysis_revision==owner.revision;
}
static size_t source_format_owner_token(App *a,uint64_t owner) {
    Worksheet *sheet=active_sheet(a); if (!sheet || !sheet->formats.source_aware) return SIZE_MAX;
    if (!source_format_owner_verified(a,owner)) return SIZE_MAX;
    SourceFormatProof *proof=source_format_owner_proof(a,owner,false);
    if (!proof->tokens_ready || proof->format_revision!=sheet->formats.revision) {
        proof->token=format_scope_token(&sheet->formats,&(FormatScopeView){owner,proof->raw.data,proof->raw.bytes});
        proof->result_token=proof->result_ready ? format_scope_token(&sheet->formats,&(FormatScopeView){owner,proof->result.data,proof->result.bytes}) : SIZE_MAX;
        proof->format_revision=sheet->formats.revision; proof->tokens_ready=true;
    }
    return proof->token;
}
static size_t source_format_owner_result_token(App *a,uint64_t owner) {
    Worksheet *sheet=active_sheet(a); if (!sheet || !sheet->formats.source_aware) return SIZE_MAX;
    if (!source_format_owner_verified(a,owner)) return SIZE_MAX;
    source_format_owner_token(a,owner);
    return source_format_owner_proof(a,owner,false)->result_token;
}
static size_t source_format_token(App *a) { return source_format_owner_token(a,0); }
static CellFormat source_format_value(App *a,uint64_t row,size_t column) {
    Worksheet *sheet=active_sheet(a); uint64_t record=0,id=0,local_row=row; size_t local_column=column;
    int hit=objects_hit(&sheet->objects,row,column); const Page *page=&a->page;
    if (hit>=0) {
        const SheetObject *object=sheet->objects.items+hit;
        if (object->kind!=OBJECT_TABLE || row<object->row+2) return format_resolve_token(&sheet->formats,row,column,SIZE_MAX);
        id=object->id; local_row=row-object->row-2; local_column=column-object->col; page=&a->objects[hit].page;
    }
    if ((!page->count_pending && local_row>=page->total) || local_column>=page->column_count) return format_resolve_token(&sheet->formats,row,column,SIZE_MAX);
    size_t token=source_format_owner_token(a,id),result=source_format_owner_result_token(a,id);
    if (token!=SIZE_MAX && token && local_column<page->column_count) {
        bool found=page_record_id(page,local_row,&record);
        if (!found && !id && a->source_format_proof.spare_ready) found=page_record_id(&a->spare,local_row,&record);
        if (found) return format_resolve_record_token(&sheet->formats,row,column,result,token,record,local_column);
    }
    return format_resolve_token(&sheet->formats,row,column,result);
}
static void source_format_adjust_source(App *a,uint64_t owner,Analysis *source) {
    source_format_cancel(a,"Formatting cancelled because a table source changed.");
    const SourceVersion *entry=source_format_owner_version(a,owner,source->source_version);
    if (source->source_version && (!entry || entry->kind!=source->kind || !source->path || strcmp(entry->path,source->path))) source->source_version=0;
}
static void source_format_refresh_tables(App *a,uint64_t owner) {
    source_format_cancel(a,"Formatting cancelled by source refresh."); Worksheet *sheet=active_sheet(a); if (!sheet) return;
    for (size_t i=0;i<sheet->objects.count;i++) {
        SheetObject *object=sheet->objects.items+i;
        if (object->kind!=OBJECT_TABLE || (owner && object->id!=owner)) continue;
        if (object->source.source_version) { object->source.source_version=0; a->book.dirty=true; }
        SourceFormatProof *proof=source_format_owner_proof(a,object->id,false); if (proof) proof->ready=false;
    }
}
static void source_format_table_navigation(App *a) {
    const SourceFormatIntent *intent=&a->source_format; SourceFormatOwner owner;
    if (!intent->active || !intent->owner || selected_pivot(a) || !source_format_owner(a,intent->owner,&owner) || !owner.ready) return;
    if ((int)owner.index!=a->selected_object && !CheckCollisionRecs(
        grid_range_rect(&a->grid,a->ui.scale*a->grid.zoom,owner.row,owner.column,owner.rows,owner.columns),a->grid.body)) return;
    uint64_t first=a->grid.top>owner.row+2 ? a->grid.top-owner.row-2 : 0;
    const Page *page=owner.page;
    bool cached=first>=page->offset && (first>=page->total || first+a->grid.render_rows<=page->offset+page->row_count || page->offset+page->row_count==page->total);
    if (!cached) source_format_request(a);
}
static bool source_format_broad(const FormatSnapshot *state) {
    /* A retained source-aware snapshot already has explicit imported/manual
       semantics. It remains safe when branching after Undo; it is never
       reinterpreted as manual coordinates or activated by this admission. */
    if (state->source_aware) return true;
    if (state->scope_count) return false;
    for (size_t i=0;i<state->count;i++) {
        const FormatPatch *patch=state->items+i;
        if (patch->selector!=FORMAT_SPATIAL || patch->range.r0 || patch->range.r1!=DX_SHEET_ROWS-1) return false;
    }
    return true;
}
static bool source_format_first_safe(const FormatStore *store) {
    FormatSnapshot state={.items=store->items,.count=store->count,.scopes=store->scopes,
        .scope_count=store->scope_count,.source_aware=store->source_aware};
    if (!source_format_broad(&state)) return false;
    for (size_t i=0;i<store->undo_count;i++) if (!source_format_broad(store->undo+i)) return false;
    for (size_t i=0;i<store->redo_count;i++) if (!source_format_broad(store->redo+i)) return false;
    return true;
}
static bool source_format_manual_import_ready(App *a) {
    Worksheet *sheet=active_sheet(a);
    return sheet && !sheet->formats.source_aware &&
        worksheet_manual_format_history_eligible(sheet,&a->analysis,NULL);
}
static void source_format_manual_adopt(App *a,bool advance_revision) {
    /* Callers proved all pre-import definitions manual, or all prior existing
       rectangles whole-column before a new manual patch. No existing finite
       imported rectangle is reinterpreted. No scope or journal entry needed. */
    FormatStore *store=&active_sheet(a)->formats; store->source_aware=true;
    for (size_t i=0;i<store->undo_count;i++) store->undo[i].source_aware=true;
    for (size_t i=0;i<store->redo_count;i++) store->redo[i].source_aware=true;
    if (advance_revision) store->revision++;
}
static bool source_format_save_ready(App *a) {
    for (size_t i=0;i<a->book.count;i++) {
        const Worksheet *sheet=a->book.sheets+i;
        const Analysis *source=i==a->book.active ? &a->analysis : &sheet->analysis;
        FormatSnapshot current={.items=sheet->formats.items,.count=sheet->formats.count,.scopes=sheet->formats.scopes,
            .scope_count=sheet->formats.scope_count,.source_aware=sheet->formats.source_aware};
        bool imported=source->kind!=SOURCE_NONE;
        for (size_t j=0;!imported && j<sheet->objects.count;j++) imported=sheet->objects.items[j].kind==OBJECT_TABLE;
        if (imported && !source_format_broad(&current)) {
            status(a,"Imported finite formats require source-aware ownership before saving.");
            return false;
        }
    }
    return true;
}
static bool source_format_add(size_t *bytes,size_t extra) {
    if (*bytes>SIZE_MAX-extra) return false; *bytes+=extra; return true;
}
static size_t source_format_page_bytes(const Page *page) {
    size_t bytes=0;
    if (page->records.count>DX_PAGE_ROWS || page->records.count>SIZE_MAX/sizeof(uint64_t)) return SIZE_MAX;
    if (page->records.ids) bytes=page->records.count*sizeof(uint64_t);
    if (page->records.source_key && !source_format_add(&bytes,strlen(page->records.source_key)+1)) return SIZE_MAX;
    return bytes;
}
/* An unpublished database open is a separately owned native probe. Retire it
   before document/cache destruction; published sheets are never rolled back. */
static bool app_primary_source_pending(const App *a) { return a->primary_source_draft || a->primary_source_plan || a->primary_source_topology; }
static size_t app_primary_source_plans_owned(const App *a) {
    size_t bytes=primary_source_draft_owned(a->primary_source_draft);
    if (!source_format_add(&bytes,primary_source_publish_owned(a->primary_source_plan)) || !source_format_add(&bytes,primary_source_topology_owned(a->primary_source_topology))) return SIZE_MAX;
    return bytes;
}
static void app_primary_source_cancel(App *a) {
    primary_source_topology_free(&a->primary_source_topology); primary_source_publish_cancel(&a->primary_source_plan);
    primary_source_draft_free(&a->primary_source_draft);
    reply_free(a->primary_source_reply); a->primary_source_reply=NULL; a->primary_source_request=0; a->primary_source_navigation=(SheetView){0};
}
static size_t app_primary_source_owned(const App *a) {
    size_t bytes=app_primary_source_plans_owned(a); const QueryReply *r=a->primary_source_reply;
    if (r && (!source_format_add(&bytes,sizeof(*r)) || !source_format_add(&bytes,analysis_text_bytes(&r->analysis)) ||
        !source_format_add(&bytes,r->page.bytes) || !source_format_add(&bytes,source_format_page_bytes(&r->page)))) return SIZE_MAX;
    if (r) {
        if (r->page.relation_count>SIZE_MAX/sizeof(Relation) || !source_format_add(&bytes,r->page.relation_count*sizeof(Relation))) return SIZE_MAX;
        for (size_t i=0;i<r->page.relation_count;i++) {
            const Relation *relation=r->page.relations+i; const char *texts[]={relation->schema,relation->name,relation->label};
            for (size_t j=0;j<3;j++) if (texts[j] && !source_format_add(&bytes,strlen(texts[j])+1)) return SIZE_MAX;
        }
        if (r->page.sql && !source_format_add(&bytes,strlen(r->page.sql)+1)) return SIZE_MAX;
        if (r->error && !source_format_add(&bytes,strlen(r->error)+1)) return SIZE_MAX;
    }
    return bytes;
}
/* Runtime origin arenas survive sheet/history switches. Only document-runtime
   retirement frees them; an admitted origin is never reset after model edits. */
struct AppDefinitionOrigin {
    AppDefinitionOrigin *next;
    WorkbookId document;
    uint64_t session,sheet;
    DefinitionOrigin *owner;
    bool admission_pending; /* Published usable session; no retained origin readiness. */
    WorkerNativeRetirement retirement; /* Intrusive whole-list owner, used only on the head. */
};
static size_t app_definition_origin_bytes(const App *a) {
    size_t bytes=0;
    for (const AppDefinitionOrigin *n=a->definition_origins;n;n=n->next) {
        DefinitionOriginState state=definition_origin_state(n->owner);
        if (!source_format_add(&bytes,sizeof(*n)) || !source_format_add(&bytes,state.owner_bytes) || !source_format_add(&bytes,state.asset_bytes)) return SIZE_MAX;
    }
    return bytes;
}
static void app_definition_origins_dispose(WorkbookNativeIO *owned) {
    AppDefinitionOrigin *node=(AppDefinitionOrigin *)owned;
    while (node) { AppDefinitionOrigin *next=node->next; definition_origin_free(&node->owner); free(node); node=next; }
}
static void app_definition_origins_retire(App *a) {
    AppDefinitionOrigin *head=a->definition_origins; if (!head) return;
    head->retirement=(WorkerNativeRetirement){.context=(WorkbookNativeIO *)head,.dispose=app_definition_origins_dispose,.reserved_bytes=app_definition_origin_bytes(a)};
    if (worker_retire_native(&a->worker,&head->retirement)) { a->definition_origins=NULL; return; }
    /* Overflow/stopping refusal keeps the complete list resident and charged.
       Only after worker stop may shutdown synchronously dispose ownership. */
    if (!a->worker.started) { a->definition_origins=NULL; app_definition_origins_dispose((WorkbookNativeIO *)head); }
}
static FormatBudgetStatus source_format_budget_status(App *a,const WorkbookFormatReplacement *replacement,
    size_t adopted,size_t captured_extra,size_t reply_owned,size_t new_jobs,size_t incoming_retained) {
    WorkbookFormatUsage live={0},next={0}; FormatBudget budget;
    FormatBudgetStatus result=workbook_format_usage(&a->book,NULL,0,&live);
    if (!result) result=workbook_format_usage(&a->book,replacement,replacement ? 1 : 0,&next);
    if (!result && !source_format_add(&next.retained,incoming_retained)) result=FORMAT_BUDGET_OVERFLOW;
    WorkerFormatUsage worker=worker_format_usage(&a->worker);
    FormatTransientUsage transient={.adopted_staging=adopted,
        .other_staging=sizeof(SourceFormatIntent)+sizeof(SourceFormatProof)+sizeof(SourceFormatAttempt)+sizeof(a->source_format_tables)+DX_FORMAT_VALIDATION_BYTES,.io_snapshots=SOURCE_FORMAT_IO_RESERVE};
    /* Terminal arenas stay resident until explicit document retirement. During
       history preview the detached Working Head coexists with the shown book.
       Save As may additionally reserve a held-head export copy; conservative
       coexistence reservations here do not claim exact process RSS. */
    size_t origins=app_definition_origin_bytes(a);
    WorkbookFormatUsage held={0}; const Workbook *working=app_preview_working(&a->history);
    if (!result && working && working!=&a->book) result=workbook_format_usage(working,NULL,0,&held);
    if (!result && (origins==SIZE_MAX || !source_format_add(&transient.other_staging,origins) ||
        !source_format_add(&transient.other_staging,held.retained))) result=FORMAT_BUDGET_OVERFLOW;
    size_t checkpoint_bytes=workbook_journal_session_state(a->journal,&a->book).checkpoint_format_bytes;
    if (!result && !source_format_add(&transient.io_snapshots,checkpoint_bytes)) result=FORMAT_BUDGET_OVERFLOW;
    size_t recovery_bytes=workbook_recovery_worker_state(a->recovery_worker).format_reservation_bytes;
    WorkbookFormatUsage staged={0}; const Workbook *candidate=workbook_recovery_book(a->recovery_staged);
    if (!result && candidate) result=workbook_format_usage(candidate,NULL,0,&staged);
    if (!result && (!source_format_add(&transient.io_snapshots,recovery_bytes) || !source_format_add(&transient.io_snapshots,staged.retained))) result=FORMAT_BUDGET_OVERFLOW;
    /* Keep capacity for two retiring/current save snapshots. SQL construction
       uses the existing bounded raw helper; its separate staging allowance is
       retained for every active/pending operation, including canceled work.
       These are explicit conservative reservations, not measured heap/RSS. */
    size_t jobs=(size_t)worker.active_count+worker.pending_count;
    if (!result && !source_format_add(&jobs,new_jobs)) result=FORMAT_BUDGET_OVERFLOW;
    size_t extra[]={app_primary_source_owned(a),a->source_format.bytes,a->source_format.object_reserved,a->source_format.raw.bytes,a->source_format_proof.raw.bytes,a->source_format_proof.result.bytes,captured_extra,reply_owned};
    for (size_t i=0;!result && i<sizeof(extra)/sizeof(*extra);i++)
        if (!source_format_add(&transient.other_staging,extra[i])) result=FORMAT_BUDGET_OVERFLOW;
    for (size_t i=0;!result && i<DX_MAX_OBJECTS;i++)
        if (!source_format_add(&transient.other_staging,a->source_format_tables[i].proof.raw.bytes) ||
            !source_format_add(&transient.other_staging,a->source_format_tables[i].proof.result.bytes)) result=FORMAT_BUDGET_OVERFLOW;
    size_t pages=source_format_page_bytes(&a->page),spare=source_format_page_bytes(&a->spare);
    if (!result && (pages==SIZE_MAX || spare==SIZE_MAX || !source_format_add(&pages,spare))) result=FORMAT_BUDGET_OVERFLOW;
    for (size_t i=0;!result && i<DX_MAX_OBJECTS;i++) {
        size_t bytes=source_format_page_bytes(&a->objects[i].page);
        if (bytes==SIZE_MAX || !source_format_add(&pages,bytes)) result=FORMAT_BUDGET_OVERFLOW;
    }
    transient.page_metadata_owned=pages;
    transient.collector_owned=worker.active_reserved;
    if (!result && !source_format_add(&transient.collector_owned,worker.retired_owned)) result=FORMAT_BUDGET_OVERFLOW;
    if (!result && (!source_format_add(&transient.collector_owned,worker.pending_reserved) ||
        !source_format_add(&transient.collector_owned,worker.published_owned))) result=FORMAT_BUDGET_OVERFLOW;
    transient.pending_reserved=replacement ? sizeof(size_t)*DX_SOURCE_SCOPE_FIELDS : 3u*1024u*1024u;
    /* Before planning, reserve ledger/format/definition coexistence; after
       planning, those actual buffers are counted above as adopted_staging. */
    if (!result && (jobs>SIZE_MAX/SOURCE_FORMAT_SQL_RESERVE ||
        !source_format_add(&transient.pending_reserved,jobs*SOURCE_FORMAT_SQL_RESERVE) ||
        new_jobs>SIZE_MAX/DX_WORKER_FORMAT_RESERVED_BYTES ||
        !source_format_add(&transient.pending_reserved,new_jobs*DX_WORKER_FORMAT_RESERVED_BYTES))) result=FORMAT_BUDGET_OVERFLOW;
    if (!result) result=workbook_format_preflight(&live,&next,&transient,&budget);
    if (result) status(a,workbook_format_budget_message(result));
    return result;
}
static bool source_format_budget(App *a,const WorkbookFormatReplacement *replacement,
    size_t adopted,size_t captured_extra,size_t reply_owned,size_t new_jobs,size_t incoming_retained) {
    FormatBudgetStatus result=source_format_budget_status(a,replacement,adopted,captured_extra,reply_owned,new_jobs,incoming_retained);
    return result==FORMAT_BUDGET_OK;
}
static void source_format_result_page(App *a,const SourceFormatOwner *owner,SourceFormatProof *proof) {
    /* The actual publication has already verified RAW/schema/locator. Cache
       canonical RESULT once per definition, never parse/build it in paint. */
    if (proof->result.data && source_scope_result_matches(proof->result.data,proof->result.bytes,&proof->raw,owner->source,NULL)) {
        proof->result_ready=true; return;
    }
    source_packet_free(&proof->result);
    if (!source_format_budget(a,NULL,0,DX_SOURCE_SCOPE_BYTES,a->source_format_polled,0,0)) return;
    char *error=NULL;
    proof->result_ready=source_scope_result_build(&proof->raw,owner->source,&proof->result,&error);
    if (error) status(a,error);
    free(error);
}
static bool source_format_schema_size(const Page *page,size_t *out) {
    if (!page->column_count || page->column_count>SOURCE_FORMAT_CAPTURE_BYTES/sizeof(Column)) return false;
    size_t bytes=page->column_count*sizeof(Column);
    for (size_t c=0;c<page->column_count;c++) {
        const Column *field=page->columns+c;
        if (!field->name || !field->type_name) return false;
        size_t lengths[]={strlen(field->name)+1,strlen(field->type_name)+1};
        for (size_t i=0;i<2;i++) if (lengths[i]>SOURCE_FORMAT_CAPTURE_BYTES-bytes) return false; else bytes+=lengths[i];
    }
    *out=bytes; return true;
}
static void source_format_schema_copy(Page *out,const Page *from,size_t bytes) {
    out->columns=dx_alloc(from->column_count*sizeof(Column)); out->column_count=from->column_count;
    out->bytes=bytes; out->total=from->total;
    out->records.file_identity=from->records.file_identity; out->records.file_identity_valid=true;
    out->records.stamp=from->records.stamp;
    for (size_t c=0;c<from->column_count;c++) {
        const Column *field=from->columns+c; Column *copy=out->columns+c;
        copy->name=dx_strdup(field->name); copy->type_name=dx_strdup(field->type_name);
        copy->type=field->type; copy->number_type=field->number_type; copy->numeric=field->numeric;
    }
}
static bool source_format_capture(App *a,const SourceFormatOwner *owner,FormatRange range,unsigned mask,CellFormat value,bool restore) {
    Worksheet *sheet=active_sheet(a); size_t schema_bytes=0,text=analysis_text_bytes(owner->source);
    if (text>SOURCE_FORMAT_CAPTURE_BYTES-sizeof(Analysis) || !source_format_schema_size(owner->page,&schema_bytes) ||
        schema_bytes>SOURCE_FORMAT_CAPTURE_BYTES-sizeof(Analysis)-text) {
        status(a,"This source definition or schema exceeds the bounded formatting capture. Whole-column defaults remain available."); return false;
    }
    size_t captured=text+schema_bytes,reserved=0,extra=captured;
    if (owner->id && !owner->source->source_version) {
        reserved=objects_snapshot_owned_bytes(&(ObjectSnapshot){sheet->objects.items,sheet->objects.count});
        if (reserved==SIZE_MAX || !source_format_add(&extra,reserved)) {
            status(a,"The table definitions cannot fit a safe source-format snapshot."); return false;
        }
    }
    if (!source_format_owner_proof(a,owner->id,true)) { status(a,"The source-format owner cache is full."); return false; }
    FormatBudgetStatus result=source_format_budget_status(a,NULL,0,extra,0,1,0);
    if (result!=FORMAT_BUDGET_OK) return false;
    SourceFormatIntent *intent=&a->source_format;
    *intent=(SourceFormatIntent){.active=true,.issue=!owner->source->source_version,.restore=restore,.document=a->format_document,
        .analysis_revision=a->analysis_revision,.format_revision=sheet->formats.revision,.cell_revision=sheet->cells.revision,
        .high_water=sheet->source_versions.high_water,.version=owner->source->source_version ? owner->source->source_version : sheet->source_versions.high_water+1,
        .owner=owner->id,.owner_revision=owner->revision,.origin_row=owner->row,.origin_rows=owner->rows,
        .origin_column=owner->column,.origin_columns=owner->columns,.object_reserved=reserved,
        .sheet=a->book.active,.bytes=captured,.range=range,.mask=mask,.value=value,.source=analysis_copy(owner->source)};
    source_format_schema_copy(&intent->schema,owner->page,schema_bytes); return true;
}
static void source_format_verify(App *a) {
    SourceFormatIntent *intent=&a->source_format;
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=++a->request_id; job->kind=JOB_VERIFY_SOURCE;
    job->path=dx_strdup(intent->source.path); job->expected_source_stamp=intent->schema.records.stamp;
    job->revision=intent->owner_revision; job->object_id=intent->owner;
    intent->stage=job->kind; intent->request=job->id; a->pending_kind=job->kind; a->busy=true;
    worker_submit(&a->worker,job);
    status(a,intent->restore ? "Verifying saved source formatting…" : "Verifying source content before formatting…");
}
static bool source_format_raw_ready(App *a,const SourceFormatOwner *owner) {
    const Analysis *source=owner->source; const Page *page=owner->page;
    return owner->ready && !page->count_pending && page->column_count && page->records.file_identity_valid &&
        page->records.stamp && !a->step_preview.active && (source->kind==SOURCE_CSV || source->kind==SOURCE_PARQUET) &&
        source->path && *source->path && !strpbrk(source->path,"*?[]") && !source->query && !source->unique_rows &&
        !source->group_column && !source->measure_column && !source->schema && !source->table;
}
static void source_format_schedule(App *a) {
    Worksheet *sheet=active_sheet(a);
    if (!sheet ||
        a->busy || a->io_busy || a->source_format.active || a->values_loading || a->modal!=MODAL_NONE ||
        a->document_action!=DOCUMENT_NONE || (a->statistics_enabled &&
        (a->statistics_state==STATS_WAITING || a->statistics_state==STATS_CALCULATING)) ||
        (a->count_request_id && a->count_request_id==a->request_id) || dx_millis()-a->viewport_changed<150 || a->format_document==UINT64_MAX) return;
    for (size_t i=0;i<=sheet->objects.count;i++) {
        uint64_t id=i ? sheet->objects.items[i-1].id : 0; SourceFormatOwner owner;
        if (!source_format_owner(a,id,&owner) || !owner.source->source_version || !source_format_raw_ready(a,&owner) || source_format_owner_verified(a,id)) continue;
        if (!id && !grid_cached(&a->grid,owner.page)) continue;
        if (id && (int)owner.index!=a->selected_object && !CheckCollisionRecs(
            grid_range_rect(&a->grid,a->ui.scale*a->grid.zoom,owner.row,owner.column,owner.rows,owner.columns),a->grid.body)) continue;
        const SourceVersion *entry=source_format_owner_version(a,id,owner.source->source_version);
        if (!entry || !entry->raw.bytes || entry->kind!=owner.source->kind || strcmp(entry->path,owner.source->path)) continue;
        SourceFormatAttempt *attempt=source_format_owner_attempt(a,id,true); if (!attempt) continue;
        if (attempt->valid && attempt->document==a->format_document && attempt->sheet==a->book.active &&
            attempt->version==entry->version && attempt->stamp==owner.page->records.stamp &&
            source_file_identity_same(&attempt->identity,&owner.page->records.file_identity)) continue;
        *attempt=(SourceFormatAttempt){.identity=owner.page->records.file_identity,.document=a->format_document,
            .version=entry->version,.stamp=owner.page->records.stamp,.sheet=a->book.active,.valid=true};
        if (source_format_capture(a,&owner,(FormatRange){0},0,format_default(),true)) source_format_verify(a);
        return;
    }
}
static bool source_format_collect(App *a,size_t reply_owned) {
    SourceFormatIntent *intent=&a->source_format; char *error=NULL;
    SourcePacket raw={0}; SourceFormatOwner owner;
    bool ok=source_format_owner(a,intent->owner,&owner) && source_scope_raw_build(intent->owner,intent->version,&intent->source,&intent->schema,&intent->digest,
        duckdb_library_version(),&raw,&error);
    SourceVersion candidate={.owner=intent->owner,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=raw};
    const SourceVersion *entry=intent->issue ? &candidate : source_format_owner_version(a,intent->owner,intent->version);
    if (ok) ok=entry && entry->raw.bytes==raw.bytes && !memcmp(entry->raw.data,raw.data,raw.bytes) &&
        source_version_page_verified(entry,intent->owner,intent->version,&intent->source,&intent->schema,
            SOURCE_DIGEST_OK,&intent->digest,duckdb_library_version(),NULL) &&
        source_version_page_verified(entry,intent->owner,intent->version,owner.source,owner.page,
            SOURCE_DIGEST_OK,&intent->digest,duckdb_library_version(),NULL);
    if (!ok) {
        status(a,error ? error : "The captured source schema or file proof no longer matches. No formatting was applied.");
        free(error); source_packet_free(&raw); source_format_cancel(a,NULL); return false;
    }
    if (!source_format_budget(a,NULL,0,raw.bytes,reply_owned,intent->restore ? 0 : 1,0)) {
        source_packet_free(&raw); source_format_cancel(a,NULL); return false;
    }
    intent->raw=raw;
    if (intent->restore) {
        /* Verification publishes only a session proof. Ledger, formats,
           Analysis, history and dirty state retain their saved ownership. */
        uint64_t id=intent->owner; SourceFormatProof *proof=source_format_owner_proof(a,id,false);
        source_format_proof_adopt(a,proof,intent);
        source_format_intent_free(intent); source_format_page_owner(a,id);
        status(a,source_format_owner_verified(a,id) ? "Saved source formatting verified." : "Saved source formatting remains suspended.");
        return true;
    }
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=++a->request_id; job->kind=JOB_COLLECT_RECORDS;
    job->analysis=analysis_copy(&intent->source); job->offset=intent->range.r0; job->rows=intent->range.r1-intent->range.r0+1;
    job->expected_source_stamp=intent->digest.observed_stamp; job->expected_file_identity=intent->digest.identity;
    job->expected_file_identity_valid=true; job->revision=intent->owner_revision;
    job->object_id=intent->owner; job->is_table=intent->owner!=0;
    intent->stage=JOB_COLLECT_RECORDS; intent->request=job->id; a->pending_kind=job->kind; a->busy=true;
    worker_submit(&a->worker,job); status(a,"Collecting the selected source records…"); return true;
}
static bool source_format_try(App *a,FormatRange range,unsigned mask,CellFormat value,bool *handled) {
    *handled=false; Worksheet *sheet=active_sheet(a);
    if (!sheet || (!range.r0 && range.r1==DX_SHEET_ROWS-1)) return false;
    bool primary=a->analysis.kind!=SOURCE_NONE && (!a->page.column_count ||
        (range.c0<a->page.column_count && (a->page.count_pending || range.r0<a->page.total)));
    uint64_t id=0; size_t intersections=0;
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *object=sheet->objects.items+i;
        if (range.r0<object->row+object->rows && range.r1>=object->row &&
            range.c0<object->col+object->columns && range.c1>=object->col) { id=object->id; intersections++; }
    }
    if (!primary && !intersections) return false;
    *handled=true;
    if (intersections>1 || (primary && intersections)) {
        status(a,"Mixed source, object or manual formatting is not available yet. Choose one raw source range."); return false;
    }
    SourceFormatOwner owner;
    if (!source_format_owner(a,id,&owner) || a->busy || a->io_busy || !source_format_raw_ready(a,&owner) ||
        a->grid.selection!=SELECT_CELLS || a->matrix_cells_active ||
        (id && (range.r0<owner.row+2 || range.c0<owner.column))) {
        status(a,"Record formatting needs a ready finite range in one raw native CSV or Parquet source. Whole-column defaults remain available."); return false;
    }
    if (id) {
        range.r0-=owner.row+2; range.r1-=owner.row+2; range.c0-=owner.column; range.c1-=owner.column;
    }
    if (range.r1>=owner.page->total || range.c1>=owner.page->column_count) {
        status(a,"Record formatting needs a ready finite range in one raw native CSV or Parquet source. Whole-column defaults remain available."); return false;
    }
    if (range.r1-range.r0+1>DX_RECORD_SELECTION_ROWS) {
        status(a,"Finite source formatting is limited to 20,000 records. Choose a smaller range or whole-column defaults."); return false;
    }
    if (!sheet->formats.source_aware && !source_format_first_safe(&sheet->formats)) {
        status(a,"Imported finite formats require source-aware ownership before verified record collection. Whole-column defaults remain available."); return false;
    }
    if (a->format_document==UINT64_MAX || (!owner.source->source_version && sheet->source_versions.high_water==UINT64_MAX)) {
        status(a,"The source-format lifetime counter is exhausted. No formatting was applied."); return false;
    }
    if (!source_format_capture(a,&owner,range,mask,value,false)) return false;
    SourceFormatIntent *intent=&a->source_format;
    if (!intent->issue && source_format_owner_verified(a,id)) {
        intent->digest=source_format_owner_proof(a,id,false)->digest;
        intent->request=a->request_id; return source_format_collect(a,0);
    }
    source_format_verify(a); return true;
}
static bool source_format_reply_apply(App *a,QueryReply *reply) {
    if (reply->kind!=JOB_VERIFY_SOURCE && reply->kind!=JOB_COLLECT_RECORDS) return false;
    SourceFormatIntent *intent=&a->source_format;
    if (!source_format_fence(a) || reply->kind!=intent->stage || reply->revision!=intent->owner_revision || reply->object_id!=intent->owner) {
        source_format_cancel(a,"Formatting cancelled because its captured request changed."); return true;
    }
    if (reply->error) {
        char message[768]; snprintf(message,sizeof(message),"Formatting failed: %.700s",reply->error);
        source_format_cancel(a,message); return true;
    }
    size_t reply_bytes=a->source_format_polled;
    if (reply->kind==JOB_VERIFY_SOURCE) {
        if (reply->source_digest_status!=SOURCE_DIGEST_OK) { source_format_cancel(a,"Source verification did not succeed. No formatting was applied."); return true; }
        intent->digest=reply->source_digest; source_format_collect(a,reply_bytes); return true;
    }
    const RecordSelection *records=&reply->records; char *error=NULL;
    SourceVersion candidate={.owner=intent->owner,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=intent->raw};
    const SourceVersion *verified=intent->issue ? &candidate : source_format_owner_version(a,intent->owner,intent->version);
    if (!verified || reply->analysis.source_version!=intent->source.source_version ||
        !source_version_page_verified(verified,intent->owner,intent->version,&reply->analysis,&intent->schema,
            SOURCE_DIGEST_OK,&intent->digest,duckdb_library_version(),NULL) ||
        records->count!=intent->range.r1-intent->range.r0+1 || records->count>DX_RECORD_SELECTION_ROWS ||
        records->bytes!=records->count*sizeof(uint64_t) || !records->ids ||
        records->stamp!=intent->digest.observed_stamp ||
        !source_file_identity_same(&records->file_identity,&intent->digest.identity) ||
        !analysis_same_result(&reply->analysis,&intent->source)) {
        source_format_cancel(a,"Collected records did not match the captured source selection. No formatting was applied."); return true;
    }
    for (size_t i=0;i<records->count;i++) if (!records->ids[i] || (i && records->ids[i]<=records->ids[i-1])) {
        source_format_cancel(a,"Collected record identities were invalid. No formatting was applied."); return true;
    }
    Worksheet *sheet=active_sheet(a); SourceVersions versions={0}; FormatSnapshot plan={0}; Analysis next={0}; ObjectSnapshot objects={0};
    size_t fields[DX_SOURCE_SCOPE_FIELDS]; size_t count=intent->range.c1-intent->range.c0+1;
    for (size_t c=0;c<count;c++) fields[c]=intent->range.c0+c;
    SourceVersion entry={.owner=intent->owner,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=intent->raw};
    /* Unrelated object names/Marks/placements may change while collection is
       pending. Re-reserve the complete current graph before cloning it. */
    if (intent->issue && intent->owner) {
        intent->object_reserved=objects_snapshot_owned_bytes(&(ObjectSnapshot){sheet->objects.items,sheet->objects.count});
        if (intent->object_reserved==SIZE_MAX) { source_format_cancel(a,"The table definitions changed beyond safe formatting capture."); return true; }
    }
    /* Version-only binding preserves all definition text. Admit its retained
       parser peak BEFORE the existing planners validate/copy those graphs.
       Reserve the current scope table plus one prospective RAW scope; the
       actual bounded format plan is measured again below before adoption. */
    size_t validation=0,prospective_scopes=sheet->formats.scope_count;
    if (prospective_scopes<DX_FORMAT_PATCHES*DX_FORMAT_GUARDS) prospective_scopes++;
    SourceFormatCommandPlan projection={.formats={.scope_count=prospective_scopes}};
    if (intent->issue && !intent->owner) projection.primary=intent->source;
    bool ok=workbook_source_format_scratch(sheet,&a->analysis,&projection,&validation,&error) &&
        source_format_budget(a,NULL,0,validation,reply_bytes,0,0);
    if (ok && intent->issue) ok=source_versions_plan(&sheet->source_versions,&entry,true,&versions,&error);
    if (ok) ok=format_plan_records(&sheet->formats,&(FormatScopeView){intent->owner,intent->raw.data,intent->raw.bytes},
        records->ids,records->count,fields,count,intent->mask,intent->value,&plan,&error);
    bool changed=false;
    if (ok && intent->issue && intent->owner) {
        ok=objects_plan_source_version(&sheet->objects,intent->owner,intent->source.source_version,intent->version,&objects,&changed,&error) && changed;
    } else if (ok && intent->issue) {
        next=analysis_copy(&intent->source); next.source_version=intent->version;
        ok=worksheet_analysis_preflight(sheet,&a->analysis,&next,&changed,&error) && changed;
    }
    FormatStore projected=sheet->formats;
    if (ok) {
        projected.items=plan.items; projected.count=plan.count; projected.scopes=plan.scopes;
        projected.scope_count=plan.scope_count; projected.source_aware=plan.source_aware; projected.redo_count=0;
        if (projected.undo_count==DX_FORMAT_HISTORY) {
            memmove(projected.undo,projected.undo+1,(DX_FORMAT_HISTORY-1)*sizeof(FormatSnapshot)); projected.undo_count--;
        }
        projected.undo[projected.undo_count++]=(FormatSnapshot){.items=sheet->formats.items,.count=sheet->formats.count,
            .scopes=sheet->formats.scopes,.scope_count=sheet->formats.scope_count,.source_aware=sheet->formats.source_aware};
        FormatStore new_current={.items=plan.items,.count=plan.count,.scopes=plan.scopes,
            .scope_count=plan.scope_count,.source_aware=plan.source_aware};
        size_t adopted=format_owned_bytes(&new_current,false),incoming=0;
        if (intent->issue) {
            size_t next_bytes=intent->owner ? objects_snapshot_owned_bytes(&objects) : sizeof(next)+analysis_text_bytes(&next);
            incoming=intent->owner ? objects_snapshot_owned_bytes(&(ObjectSnapshot){sheet->objects.items,sheet->objects.count}) : sizeof(Analysis)+analysis_text_bytes(&a->analysis);
            if (next_bytes==SIZE_MAX || incoming==SIZE_MAX || !source_format_add(&adopted,versions.bytes) || !source_format_add(&adopted,next_bytes)) ok=false;
        }
        WorkbookFormatReplacement replacement={.sheet=a->book.active,.formats=&projected,.versions=intent->issue ? &versions : NULL};
        /* The prepared object graph replaces its prior preallocation reserve.
           Old live objects remain the authority until the single journal commit. */
        intent->object_reserved=0;
        SourceFormatCommandPlan composite={.sources=versions,.primary=next,.objects=objects,.formats=plan};
        if (ok) ok=workbook_source_format_scratch(sheet,&a->analysis,&composite,&validation,&error);
        if (ok) ok=source_format_budget(a,&replacement,adopted,validation,reply_bytes,0,incoming);
    }
    if (!ok) {
        if (error) status(a,error);
        free(error); source_versions_free(&versions); format_snapshot_free(&plan); analysis_free(&next); objects_snapshot_free(&objects);
        source_format_cancel(a,NULL); return true;
    }
    bool formatted=false;
    if (intent->issue) {
        SourceFormatCommandContext context={.book=&a->book,.primary=&a->analysis,.request_epoch=&a->request_id,
            .validation_reserved=validation,.ready=source_format_fence(a),.read_only=history_locked(a)};
        SourceFormatCommandTarget target;
        SourceFormatCommandPlan composite={.sources=versions,.primary=next,.objects=objects,.formats=plan};
        versions=(SourceVersions){0}; next=(Analysis){0}; objects=(ObjectSnapshot){0}; plan=(FormatSnapshot){0};
        bool prepared=workbook_prepare_source_format(&context,a->book.active,intent->owner,&target,&error);
        if (prepared) formatted=workbook_apply_source_format_recorded(&context,&target,&composite,NULL,&error).status==SOURCE_FORMAT_COMMAND_APPLIED;
        source_format_command_plan_free(&composite);
        if (!formatted) { if (error) status(a,error); free(error); source_format_cancel(a,NULL); return true; }
    } else {
        SourceFormatCommandContext context={.book=&a->book,.primary=&a->analysis,.request_epoch=&a->request_id,
            .validation_reserved=validation,.ready=source_format_fence(a),.read_only=history_locked(a)};
        SourceFormatCommandTarget target; SourceFormatCommandStatus result=SOURCE_FORMAT_COMMAND_INVALID;
        if (workbook_prepare_source_format(&context,a->book.active,intent->owner,&target,&error))
            result=workbook_apply_bound_source_format_recorded(&context,&target,&plan,NULL,&error).status;
        if (result!=SOURCE_FORMAT_COMMAND_APPLIED && result!=SOURCE_FORMAT_COMMAND_NOOP) {
            format_snapshot_free(&plan); if (error) status(a,error); free(error); source_format_cancel(a,NULL); return true;
        }
        formatted=result==SOURCE_FORMAT_COMMAND_APPLIED;
    }
    uint64_t id=intent->owner; SourceFormatProof *proof=source_format_owner_proof(a,id,false);
    source_format_proof_adopt(a,proof,intent);
    source_format_intent_free(intent); source_format_page_owner(a,id);
    status(a,formatted ? "Formatting applied to the captured source records." : "The captured source records already have this format.");
    return true;
}
static bool source_format_reply(App *a,QueryReply *reply) {
    if (reply->kind!=JOB_VERIFY_SOURCE && reply->kind!=JOB_COLLECT_RECORDS) return false;
    size_t owned=sizeof(*reply);
    if (!source_format_add(&owned,analysis_text_bytes(&reply->analysis)) || !source_format_add(&owned,reply->records.bytes) ||
        (reply->path && !source_format_add(&owned,strlen(reply->path)+1))) owned=SIZE_MAX;
    a->source_format_polled=owned;
    bool handled=source_format_reply_apply(a,reply);
    a->source_format_polled=0;
    return handled;
}

/* A primary reply can enrich only its OPEN, exact authored preimage. No
   command/history/query clock advances. LIMIT permits the valid current Page
   to display, but keeps the origin pending with no canonical/readiness claim. */
static DefinitionOriginContext app_definition_origin_context(App *a) {
    return (DefinitionOriginContext){&a->book,&a->analysis,&a->request_id,&a->analysis_revision,0,!a->io_busy && !a->step_preview.active,history_locked(a)};
}
static DefinitionOriginStatus app_definition_origin_limit(App *a,char **error) {
    if (error) { free(*error); *error=dx_strdup(a->message ? a->message : "Source origin admission exceeds the current memory budget."); }
    return DEFINITION_ORIGIN_LIMIT;
}
static AppDefinitionOrigin *app_definition_origin_for_sheet(App *a,const Worksheet *s) {
    if (!s) return NULL;
    for (AppDefinitionOrigin *n=a->definition_origins;n;n=n->next)
        if (n->document.high==a->book.id.high && n->document.low==a->book.id.low && n->session==a->book.edit_session_id && n->sheet==s->id) return n;
    return NULL;
}
static AppDefinitionOrigin *app_definition_origin_find(App *a) { return app_definition_origin_for_sheet(a,active_sheet(a)); }
typedef struct {
    AppDefinitionOrigin *node;
    DefinitionOriginContext context;
    DefinitionOriginHandoff *ticket;
    size_t charge;
} AppAnalysisHandoff;
static bool app_analysis_budget(App *a,size_t charge,char **error) {
    if (source_format_budget(a,NULL,0,charge,0,0,0)) return true;
    if (error) { free(*error); *error=dx_strdup(a->message ? a->message : "Analysis ownership exceeds the current memory budget."); }
    return false;
}
static bool app_analysis_handoff_prepare(App *a,Worksheet *s,Analysis *authority,DefinitionOriginHandoffRequest request,
    size_t extra,AppAnalysisHandoff *out,char **error) {
    AppDefinitionOrigin *node=app_definition_origin_for_sheet(a,s);
    if (!node || node->admission_pending || definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED) return true;
    DefinitionOriginContext context=app_definition_origin_context(a); context.primary=authority;
    DefinitionOriginHandoffInfo info={0};
    DefinitionOriginHandoffStatus result=definition_origin_handoff_measure(&context,node->owner,request,&info,error);
    /* Authored edits can already have invalidated an origin while retained
       history is inactive. Continue ordinary capture without reattaching it. */
    if (result==DEFINITION_HANDOFF_STALE || result==DEFINITION_HANDOFF_ADMITTED) {
        if (error) { free(*error); *error=NULL; } return true;
    }
    if (result!=DEFINITION_HANDOFF_READY) return false;
    size_t charge=info.coexisting;
    if (!source_format_add(&charge,extra) || !app_analysis_budget(a,charge,error)) return false;
    DefinitionOriginHandoff *ticket=NULL;
    result=definition_origin_handoff_prepare(&context,node->owner,request,info.owned,&ticket,error);
    if (result!=DEFINITION_HANDOFF_READY) return false;
    *out=(AppAnalysisHandoff){node,context,ticket,info.coexisting}; return true;
}
static bool app_analysis_handoff_check(AppAnalysisHandoff *p,char **error) {
    return !p->ticket || definition_origin_handoff_check(&p->context,p->node->owner,p->ticket,error)==DEFINITION_HANDOFF_READY;
}
static void app_analysis_handoff_adopt(AppAnalysisHandoff *p) {
    if (!p->ticket) return;
    /* Both owners were checked while alive. The coordinator changes no global
       clock between transfers; adoption has no allocation or late admission. */
    DefinitionOriginHandoffStatus result=definition_origin_handoff_adopt(&p->context,p->node->owner,&p->ticket,NULL);
    assert(result==DEFINITION_HANDOFF_APPLIED); (void)result;
}
static bool app_analysis_clone(App *a,const Analysis *from,size_t extra,Analysis *out,size_t *charge,char **error) {
    size_t bytes=analysis_text_bytes(from),live=analysis_text_bytes(&a->analysis),reserved=sizeof(Analysis)*3;
    if (bytes==SIZE_MAX || live==SIZE_MAX || !source_format_add(&reserved,bytes) || !source_format_add(&reserved,bytes) ||
        !source_format_add(&reserved,live) || !source_format_add(&reserved,extra)) {
        if (error) { free(*error); *error=dx_strdup("Analysis ownership accounting overflowed."); } return false;
    }
    if (!app_analysis_budget(a,reserved,error)) return false;
    *out=analysis_copy(from); *charge=reserved-extra; return true;
}
static bool app_definition_capture(App *a) {
    Worksheet *s=active_sheet(a); if (!s) return true;
    if (definition_analysis_equal(&s->analysis,&a->analysis)) { analysis_merge_types(&s->analysis,&a->analysis); return true; }
    char *error=NULL; AppAnalysisHandoff plan={0}; Analysis candidate={0}; size_t charge=0;
    bool ok=app_analysis_handoff_prepare(a,s,&a->analysis,(DefinitionOriginHandoffRequest){DEFINITION_HANDOFF_CACHE_SYNC,NULL},0,&plan,&error);
    if (ok && !plan.ticket) {
        size_t cached=analysis_text_bytes(&s->analysis);
        ok=cached!=SIZE_MAX && source_format_add(&cached,sizeof(Analysis)) && app_analysis_clone(a,&a->analysis,cached,&candidate,&charge,&error);
    }
    if (ok) ok=app_analysis_handoff_check(&plan,&error);
    if (ok) {
        if (plan.ticket) app_analysis_handoff_adopt(&plan);
        else { analysis_free(&s->analysis); s->analysis=candidate; candidate=(Analysis){0}; }
    } else status(a,error ? error : "The current worksheet could not be captured.");
    definition_origin_handoff_cancel(&plan.ticket); analysis_free(&candidate); free(error); return ok;
}
static bool app_definition_activate_analysis(App *a,size_t index,bool capture) {
    if (capture && index==a->book.active) return app_definition_capture(a);
    Worksheet *outgoing=active_sheet(a),*incoming=a->book.sheets+index;
    AppAnalysisHandoff leave={0},enter={0}; Analysis cached={0},next={0}; size_t extra=0,plain=0; char *error=NULL;
    bool replace_cache=capture && outgoing && !definition_analysis_equal(&outgoing->analysis,&a->analysis);
    bool ok=!capture || !outgoing || app_analysis_handoff_prepare(a,outgoing,&a->analysis,
        (DefinitionOriginHandoffRequest){DEFINITION_HANDOFF_SUSPEND_TO_SHEET,NULL},0,&leave,&error);
    extra=leave.charge;
    if (ok && replace_cache && !leave.ticket) {
        size_t old=analysis_text_bytes(&outgoing->analysis);
        ok=old!=SIZE_MAX && source_format_add(&old,sizeof(Analysis)) && source_format_add(&extra,old) && app_analysis_clone(a,&a->analysis,extra,&cached,&plain,&error);
        if (ok) ok=source_format_add(&extra,plain);
    }
    if (ok) ok=app_analysis_handoff_prepare(a,incoming,&incoming->analysis,
        (DefinitionOriginHandoffRequest){DEFINITION_HANDOFF_RESUME_TO_APP,&a->analysis},extra,&enter,&error);
    if (ok && !enter.ticket) ok=app_analysis_clone(a,&incoming->analysis,extra,&next,&plain,&error);
    if (ok) ok=app_analysis_handoff_check(&leave,&error) && app_analysis_handoff_check(&enter,&error);
    if (ok) {
        Analysis previous={0};
        if (leave.ticket) app_analysis_handoff_adopt(&leave);
        else {
            if (capture && outgoing) {
                if (replace_cache) { analysis_free(&outgoing->analysis); outgoing->analysis=cached; cached=(Analysis){0}; }
                else analysis_merge_types(&outgoing->analysis,&a->analysis);
            }
            previous=a->analysis; a->analysis=(Analysis){0};
        }
        if (enter.ticket) app_analysis_handoff_adopt(&enter);
        else { a->analysis=next; next=(Analysis){0}; }
        analysis_free(&previous);
    } else status(a,error ? error : "The worksheet switch could not be prepared.");
    definition_origin_handoff_cancel(&leave.ticket); definition_origin_handoff_cancel(&enter.ticket);
    analysis_free(&cached); analysis_free(&next); free(error); return ok;
}
static DefinitionOriginStatus app_definition_origin_open(App *a,size_t reply_owned,AppDefinitionOrigin **out,char **error) {
    DefinitionOriginContext context=app_definition_origin_context(a); AppDefinitionOrigin *node=app_definition_origin_find(a);
    if (node) {
        if (node->admission_pending) return app_definition_origin_limit(a,error);
        if (definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED) { *out=node; return DEFINITION_ORIGIN_READY; }
        size_t scratch=0; DefinitionOriginStatus result=definition_origin_scratch(&context,node->owner,&scratch,error);
        if (result==DEFINITION_ORIGIN_READY) { *out=node; return result; }
        return result;
    }
    size_t reserve=definition_origin_owner_bytes();
    if (!source_format_add(&reserve,sizeof(*node)) || !source_format_budget(a,NULL,0,reserve,reply_owned,0,0)) return app_definition_origin_limit(a,error);
    DefinitionOrigin *owner=NULL;
    DefinitionOriginStatus result=definition_origin_begin(&context,a->book.active,definition_origin_owner_bytes(),&owner,error);
    if (result!=DEFINITION_ORIGIN_READY) return result;
    /* Existing owners returned above; only an absent preimage is created. */
    node=dx_alloc(sizeof(*node)); *node=(AppDefinitionOrigin){.next=a->definition_origins,.document=a->book.id,.session=a->book.edit_session_id,.sheet=active_sheet(a)->id,.owner=owner};
    a->definition_origins=node; *out=node; return DEFINITION_ORIGIN_READY;
}
static DefinitionOriginStatus app_definition_origin_seal(App *a,AppDefinitionOrigin *node,size_t reply_owned,char **error) {
    if (node->admission_pending) return app_definition_origin_limit(a,error);
    if (definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED) return DEFINITION_ORIGIN_READY;
    DefinitionOriginContext context=app_definition_origin_context(a); size_t scratch=0;
    DefinitionOriginStatus result=definition_origin_scratch(&context,node->owner,&scratch,error);
    if (result!=DEFINITION_ORIGIN_READY) return result;
    if (!source_format_budget(a,NULL,0,scratch,reply_owned,0,0)) return app_definition_origin_limit(a,error);
    context.validation_reserved=scratch; DefinitionOriginSealInfo info={0};
    result=definition_origin_seal_measure(&context,node->owner,&info,error);
    if (result!=DEFINITION_ORIGIN_READY) return result;
    if (!source_format_add(&scratch,info.asset_bytes) || !source_format_budget(a,NULL,0,scratch,reply_owned,0,0)) return app_definition_origin_limit(a,error);
    return definition_origin_seal(&context,node->owner,info.asset_bytes,error);
}
/* One accepted reply owns source metadata regardless of its result kind.
   Viewport/count/cache geometry and numeric inference remain ordinary derived
   state; only NULL DATABASE default strings enter the atomic origin transfer. */
static DefinitionOriginStatus app_definition_origin_reply_owner(App *a,const QueryReply *reply,uint64_t owner,char **error) {
    if (history_locked(a)) return DEFINITION_ORIGIN_READY;
    const Analysis *source=&a->analysis;
    if (reply->id!=a->request_id) {
        if (error) { free(*error); *error=dx_strdup("The source reply belongs to an earlier request."); } return DEFINITION_ORIGIN_STALE;
    }
    if (owner) {
        Worksheet *sheet=active_sheet(a); const SheetObject *table=NULL,*route=NULL;
        if (sheet) for (size_t i=0;i<sheet->objects.count;i++) {
            if (sheet->objects.items[i].id==owner && sheet->objects.items[i].kind==OBJECT_TABLE) table=&sheet->objects.items[i];
            if (sheet->objects.items[i].id==reply->object_id) route=&sheet->objects.items[i];
        }
        if (!table || !route || route->revision!=reply->revision || objects_source(&sheet->objects,route)!=table) {
            if (error) { free(*error); *error=dx_strdup("The source reply belongs to a different Table route or revision."); } return DEFINITION_ORIGIN_STALE;
        }
        source=&table->source;
    } else {
        if (reply->kind!=JOB_PAGE) return DEFINITION_ORIGIN_READY;
        if (reply->object_id || reply->revision!=a->analysis_revision) {
            if (error) { free(*error); *error=dx_strdup("The source reply belongs to an earlier primary revision."); } return DEFINITION_ORIGIN_STALE;
        }
    }
    bool completion=source->kind==SOURCE_DATABASE && !source->table && reply->analysis.table;
    AppDefinitionOrigin *node=app_definition_origin_find(a);
    /* Already resolved replies may retry sealing the SAME current OPEN owner;
       they cannot replace an immutable arena or invent an absent preimage. */
    if (!completion && (!node || definition_origin_state(node->owner).phase==DEFINITION_ORIGIN_ADMITTED)) return DEFINITION_ORIGIN_READY;
    size_t reply_owned=sizeof(*reply),text=analysis_text_bytes(&reply->analysis),ids=source_format_page_bytes(&reply->page);
    if (text==SIZE_MAX || ids==SIZE_MAX || !source_format_add(&reply_owned,text) || !source_format_add(&reply_owned,ids)) return app_definition_origin_limit(a,error);
    if (completion) {
        DefinitionOriginStatus result=app_definition_origin_open(a,reply_owned,&node,error);
        if (result!=DEFINITION_ORIGIN_READY) return result;
        DefinitionOriginContext context=app_definition_origin_context(a);
        DefinitionOriginReply completed={reply->id,owner,reply->object_id,reply->revision,&reply->analysis}; DefinitionOriginCompletionInfo info={0};
        result=definition_origin_completion_measure(&context,node->owner,&completed,&info,error);
        if (result!=DEFINITION_ORIGIN_READY) return result;
        size_t reserve=info.owned;
        if (!source_format_add(&reserve,info.validation_scratch) || !source_format_budget(a,NULL,0,reserve,reply_owned,0,0)) return app_definition_origin_limit(a,error);
        context.validation_reserved=info.validation_scratch;
        result=definition_origin_complete(&context,node->owner,&completed,info.owned,error);
        if (result!=DEFINITION_ORIGIN_APPLIED) return result;
    }
    DefinitionOriginStatus sealed=app_definition_origin_seal(a,node,reply_owned,error);
    /* A later ordinary request can display a Page while an older OPEN origin
       remains pending. Never waive its exact request fence to seal it. */
    if (!completion && sealed==DEFINITION_ORIGIN_STALE) { if (error) { free(*error); *error=NULL; } return DEFINITION_ORIGIN_READY; }
    return sealed;
}

static DefinitionOriginStatus app_definition_origin_reply(App *a,const QueryReply *reply,char **error) {
    return app_definition_origin_reply_owner(a,reply,0,error);
}
