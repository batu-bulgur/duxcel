/* Native primary-source formatting controller. App owns this one captured
   transaction; the worker owns independent jobs/replies. No paint-time I/O. */
#define SOURCE_FORMAT_CAPTURE_BYTES (256u*1024u)
#define SOURCE_FORMAT_SQL_RESERVE (8u*1024u*1024u)
#define SOURCE_FORMAT_IO_RESERVE (16u*1024u*1024u)

static const SourceVersion *source_format_version(App *a,uint64_t version) {
    Worksheet *sheet=active_sheet(a); if (!sheet || !version) return NULL;
    for (size_t i=0;i<sheet->source_versions.count;i++) {
        const SourceVersion *entry=sheet->source_versions.items+i;
        if (!entry->owner && entry->version==version) return entry;
    }
    return NULL;
}
static void source_format_intent_free(SourceFormatIntent *intent) {
    analysis_free(&intent->source); page_free(&intent->schema);
    source_packet_free(&intent->raw); *intent=(SourceFormatIntent){0};
}
static void source_format_cancel(App *a,const char *message) {
    SourceFormatIntent *intent=&a->source_format; if (!intent->active) return;
    if (a->request_id==intent->request) { worker_cancel(&a->worker); a->request_id++; a->busy=false; }
    source_format_intent_free(intent);
    if (message) status(a,message);
}
static void source_format_reset(App *a) {
    source_format_cancel(a,NULL); source_packet_free(&a->source_format_proof.raw);
    a->source_format_proof=(SourceFormatProof){0};
    if (a->format_document!=UINT64_MAX) a->format_document++;
}
static bool source_format_fence(App *a) {
    const SourceFormatIntent *intent=&a->source_format; Worksheet *sheet=active_sheet(a);
    return intent->active && sheet && intent->document==a->format_document &&
        intent->sheet==a->book.active && intent->request==a->request_id &&
        intent->analysis_revision==a->analysis_revision && intent->format_revision==sheet->formats.revision &&
        intent->cell_revision==sheet->cells.revision && intent->high_water==sheet->source_versions.high_water &&
        intent->source.source_version==a->analysis.source_version &&
        analysis_same_result(&intent->source,&a->analysis) && !a->page_stale && !a->fetch_failed && !a->io_busy && !a->step_preview.active;
}
static void source_format_tick(App *a) {
    if (a->source_format.active && !source_format_fence(a))
        source_format_cancel(a,"Formatting cancelled because its source, worksheet or formatting changed.");
}
static void source_format_page(App *a) {
    SourceFormatProof *proof=&a->source_format_proof; proof->ready=proof->spare_ready=false;
    if (!proof->raw.data || proof->document!=a->format_document || proof->sheet!=a->book.active ||
        proof->version!=a->analysis.source_version || a->page_stale || a->fetch_failed) return;
    const SourceVersion *entry=source_format_version(a,proof->version);
    if (!entry || entry->raw.bytes!=proof->raw.bytes || memcmp(entry->raw.data,proof->raw.data,proof->raw.bytes) ||
        !source_version_page_verified(entry,0,proof->version,&a->analysis,&a->page,
            SOURCE_DIGEST_OK,&proof->digest,duckdb_library_version(),NULL)) return;
    proof->analysis_revision=a->analysis_revision; proof->format_revision=UINT64_MAX; proof->ready=true;
    proof->spare_ready=a->spare.column_count &&
        source_version_page_verified(entry,0,proof->version,&a->analysis,&a->spare,
            SOURCE_DIGEST_OK,&proof->digest,duckdb_library_version(),NULL);
}
static bool source_format_verified(App *a) {
    SourceFormatProof *proof=&a->source_format_proof;
    return proof->ready && proof->document==a->format_document && proof->sheet==a->book.active &&
        proof->version==a->analysis.source_version && proof->analysis_revision==a->analysis_revision &&
        !a->page_stale && !a->fetch_failed;
}
static size_t source_format_token(App *a) {
    Worksheet *sheet=active_sheet(a); if (!sheet || !sheet->formats.source_aware) return 0;
    SourceFormatProof *proof=&a->source_format_proof;
    if (!source_format_verified(a)) return SIZE_MAX;
    if (proof->format_revision!=sheet->formats.revision) {
        proof->token=format_scope_token(&sheet->formats,&(FormatScopeView){0,proof->raw.data,proof->raw.bytes});
        proof->format_revision=sheet->formats.revision;
    }
    return proof->token;
}
static CellFormat source_format_value(App *a,uint64_t row,size_t column) {
    Worksheet *sheet=active_sheet(a); size_t token=source_format_token(a); uint64_t record=0;
    if (token!=SIZE_MAX && token && objects_hit(&sheet->objects,row,column)<0 && column<a->page.column_count) {
        const Page *page=page_record_id(&a->page,row,&record) ? &a->page :
            (a->source_format_proof.spare_ready && page_record_id(&a->spare,row,&record)) ? &a->spare : NULL;
        if (page && column<page->column_count)
            return format_resolve_record_token(&sheet->formats,row,column,SIZE_MAX,token,record,column);
    }
    return format_resolve_token(&sheet->formats,row,column,SIZE_MAX);
}
static bool source_format_broad(const FormatSnapshot *state) {
    /* A retained source-aware snapshot already has explicit imported/manual
       semantics. It remains safe when branching after Undo; it is never
       reinterpreted as legacy coordinates or activated by this admission. */
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
static bool source_format_budget(App *a,const WorkbookFormatReplacement *replacement,
    size_t adopted,size_t captured_extra,size_t reply_owned,size_t new_jobs,size_t incoming_analysis) {
    WorkbookFormatUsage live={0},next={0}; FormatBudget budget;
    FormatBudgetStatus result=workbook_format_usage(&a->book,NULL,0,&live);
    if (!result) result=workbook_format_usage(&a->book,replacement,replacement ? 1 : 0,&next);
    if (!result && !source_format_add(&next.retained,incoming_analysis)) result=FORMAT_BUDGET_OVERFLOW;
    WorkerFormatUsage worker=worker_format_usage(&a->worker);
    FormatTransientUsage transient={.adopted_staging=adopted,
        .other_staging=sizeof(SourceFormatIntent)+sizeof(SourceFormatProof),.io_snapshots=SOURCE_FORMAT_IO_RESERVE};
    /* Keep capacity for two retiring/current save snapshots. SQL construction
       uses the existing bounded raw helper; its separate staging allowance is
       retained for every active/pending operation, including canceled work.
       These are explicit conservative reservations, not measured heap/RSS. */
    size_t jobs=(size_t)worker.active_count+worker.pending_count;
    if (!result && !source_format_add(&jobs,new_jobs)) result=FORMAT_BUDGET_OVERFLOW;
    size_t extra[]={a->source_format.bytes,a->source_format.raw.bytes,a->source_format_proof.raw.bytes,captured_extra,reply_owned};
    for (size_t i=0;!result && i<sizeof(extra)/sizeof(*extra);i++)
        if (!source_format_add(&transient.other_staging,extra[i])) result=FORMAT_BUDGET_OVERFLOW;
    size_t pages=source_format_page_bytes(&a->page),spare=source_format_page_bytes(&a->spare);
    if (!result && (pages==SIZE_MAX || spare==SIZE_MAX || !source_format_add(&pages,spare))) result=FORMAT_BUDGET_OVERFLOW;
    transient.page_metadata_owned=pages;
    transient.collector_owned=worker.active_reserved;
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
    return result==FORMAT_BUDGET_OK;
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
static bool source_format_collect(App *a,size_t reply_owned) {
    SourceFormatIntent *intent=&a->source_format; char *error=NULL;
    SourcePacket raw={0};
    bool ok=source_scope_raw_build(0,intent->version,&intent->source,&intent->schema,&intent->digest,
        duckdb_library_version(),&raw,&error);
    SourceVersion candidate={.owner=0,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=raw};
    const SourceVersion *entry=intent->issue ? &candidate : source_format_version(a,intent->version);
    if (ok) ok=entry && entry->raw.bytes==raw.bytes && !memcmp(entry->raw.data,raw.data,raw.bytes) &&
        source_version_page_verified(entry,0,intent->version,&intent->source,&intent->schema,
            SOURCE_DIGEST_OK,&intent->digest,duckdb_library_version(),NULL);
    if (!ok) {
        status(a,error ? error : "The captured source schema or file proof no longer matches. No formatting was applied.");
        free(error); source_packet_free(&raw); source_format_cancel(a,NULL); return false;
    }
    if (!source_format_budget(a,NULL,0,raw.bytes,reply_owned,1,0)) {
        source_packet_free(&raw); source_format_cancel(a,NULL); return false;
    }
    intent->raw=raw;
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=++a->request_id; job->kind=JOB_COLLECT_RECORDS;
    job->analysis=analysis_copy(&intent->source); job->offset=intent->range.r0; job->rows=intent->range.r1-intent->range.r0+1;
    job->expected_source_stamp=intent->digest.observed_stamp; job->expected_file_identity=intent->digest.identity;
    job->expected_file_identity_valid=true; job->revision=intent->analysis_revision;
    intent->stage=JOB_COLLECT_RECORDS; intent->request=job->id; a->pending_kind=job->kind; a->busy=true;
    worker_submit(&a->worker,job); status(a,"Collecting the selected source records…"); return true;
}
static bool source_format_try(App *a,FormatRange range,unsigned mask,CellFormat value,bool *handled) {
    *handled=false; Worksheet *sheet=active_sheet(a);
    if (!sheet || (!range.r0 && range.r1==DX_SHEET_ROWS-1) || a->analysis.kind==SOURCE_NONE ||
        (a->page.column_count && range.c0>=a->page.column_count) ||
        (!a->page.count_pending && a->page.column_count && range.r0>=a->page.total)) return false;
    *handled=true;
    if (a->busy || a->io_busy || a->fetch_failed || a->page_stale || a->page.count_pending || !a->page.column_count ||
        !a->page.records.file_identity_valid || !a->page.records.stamp || a->grid.selection!=SELECT_CELLS ||
        range.r1>=a->page.total || range.c1>=a->page.column_count || a->step_preview.active || a->matrix_cells_active ||
        (a->analysis.kind!=SOURCE_CSV && a->analysis.kind!=SOURCE_PARQUET) || !a->analysis.path || !*a->analysis.path ||
        strpbrk(a->analysis.path,"*?[]") || a->analysis.query || a->analysis.unique_rows ||
        a->analysis.group_column || a->analysis.measure_column || a->analysis.schema || a->analysis.table) {
        status(a,"Record formatting needs a ready finite range in one raw native CSV or Parquet source. Whole-column defaults remain available."); return false;
    }
    for (size_t i=0;i<sheet->objects.count;i++) {
        const SheetObject *object=sheet->objects.items+i;
        if (range.r0<object->row+object->rows && range.r1>=object->row &&
            range.c0<object->col+object->columns && range.c1>=object->col) {
            status(a,"Mixed source and object formatting is not available yet. Choose one primary-source range."); return false;
        }
    }
    if (range.r1-range.r0+1>DX_RECORD_SELECTION_ROWS) {
        status(a,"Finite source formatting is limited to 20,000 records. Choose a smaller range or whole-column defaults."); return false;
    }
    if ((!sheet->formats.source_aware && !source_format_first_safe(&sheet->formats)) ||
        (a->analysis.source_version && !source_format_verified(a))) {
        status(a,"Existing finite or restored source formats need migration or verified reactivation before verified record collection. Whole-column defaults remain available."); return false;
    }
    if (a->format_document==UINT64_MAX || (!a->analysis.source_version && sheet->source_versions.high_water==UINT64_MAX)) {
        status(a,"The source-format lifetime counter is exhausted. No formatting was applied."); return false;
    }
    size_t schema_bytes=0,text=analysis_text_bytes(&a->analysis);
    if (text>SOURCE_FORMAT_CAPTURE_BYTES-sizeof(Analysis) || !source_format_schema_size(&a->page,&schema_bytes) ||
        schema_bytes>SOURCE_FORMAT_CAPTURE_BYTES-sizeof(Analysis)-text) {
        status(a,"This source definition or schema exceeds the bounded formatting capture. Whole-column defaults remain available."); return false;
    }
    size_t captured=text+schema_bytes;
    if (!source_format_budget(a,NULL,0,captured,0,1,0)) return false;
    SourceFormatIntent *intent=&a->source_format;
    *intent=(SourceFormatIntent){.active=true,.issue=!a->analysis.source_version,.document=a->format_document,
        .analysis_revision=a->analysis_revision,.format_revision=sheet->formats.revision,.cell_revision=sheet->cells.revision,
        .high_water=sheet->source_versions.high_water,.version=a->analysis.source_version ? a->analysis.source_version : sheet->source_versions.high_water+1,
        .sheet=a->book.active,.bytes=captured,.range=range,.mask=mask,.value=value,.source=analysis_copy(&a->analysis)};
    source_format_schema_copy(&intent->schema,&a->page,schema_bytes);
    if (!intent->issue) {
        intent->digest=a->source_format_proof.digest;
        intent->request=a->request_id; return source_format_collect(a,0);
    }
    QueryJob *job=dx_alloc(sizeof(*job)); job->id=++a->request_id; job->kind=JOB_VERIFY_SOURCE;
    job->path=dx_strdup(intent->source.path); job->expected_source_stamp=intent->schema.records.stamp; job->revision=intent->analysis_revision;
    intent->stage=job->kind; intent->request=job->id; a->pending_kind=job->kind; a->busy=true;
    worker_submit(&a->worker,job); status(a,"Verifying source content before formatting…"); return true;
}
static bool source_format_reply(App *a,QueryReply *reply) {
    if (reply->kind!=JOB_VERIFY_SOURCE && reply->kind!=JOB_COLLECT_RECORDS) return false;
    SourceFormatIntent *intent=&a->source_format;
    if (!source_format_fence(a) || reply->kind!=intent->stage || reply->revision!=intent->analysis_revision || reply->object_id) {
        source_format_cancel(a,"Formatting cancelled because its captured request changed."); return true;
    }
    if (reply->error) {
        char message[768]; snprintf(message,sizeof(message),"Formatting failed: %.700s",reply->error);
        source_format_cancel(a,message); return true;
    }
    size_t reply_bytes=sizeof(*reply)+analysis_text_bytes(&reply->analysis)+reply->records.bytes;
    if (reply->path) reply_bytes+=strlen(reply->path)+1;
    if (reply->kind==JOB_VERIFY_SOURCE) {
        if (reply->source_digest_status!=SOURCE_DIGEST_OK) { source_format_cancel(a,"Source verification did not succeed. No formatting was applied."); return true; }
        intent->digest=reply->source_digest; source_format_collect(a,reply_bytes); return true;
    }
    const RecordSelection *records=&reply->records; char *error=NULL;
    SourceVersion candidate={.owner=0,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=intent->raw};
    const SourceVersion *verified=intent->issue ? &candidate : source_format_version(a,intent->version);
    if (!verified || reply->analysis.source_version!=intent->source.source_version ||
        !source_version_page_verified(verified,0,intent->version,&reply->analysis,&intent->schema,
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
    Worksheet *sheet=active_sheet(a); SourceVersions versions={0}; FormatSnapshot plan={0}; Analysis next={0};
    size_t fields[DX_SOURCE_SCOPE_FIELDS]; size_t count=intent->range.c1-intent->range.c0+1;
    for (size_t c=0;c<count;c++) fields[c]=intent->range.c0+c;
    SourceVersion entry={.owner=0,.version=intent->version,.kind=intent->source.kind,.path=intent->source.path,.raw=intent->raw};
    bool ok=source_format_budget(a,NULL,0,0,reply_bytes,0,0);
    if (ok && intent->issue) ok=source_versions_plan(&sheet->source_versions,&entry,true,&versions,&error);
    if (ok) ok=format_plan_records(&sheet->formats,&(FormatScopeView){0,intent->raw.data,intent->raw.bytes},
        records->ids,records->count,fields,count,intent->mask,intent->value,&plan,&error);
    bool changed=false;
    if (ok && intent->issue) {
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
        size_t adopted=format_owned_bytes(&new_current,false)+(intent->issue ? versions.bytes+sizeof(next)+analysis_text_bytes(&next) : 0);
        WorkbookFormatReplacement replacement={a->book.active,&projected,intent->issue ? &versions : NULL};
        ok=source_format_budget(a,&replacement,adopted,0,reply_bytes,0,
            intent->issue ? sizeof(Analysis)+analysis_text_bytes(&a->analysis) : 0);
    }
    if (!ok) {
        if (error) status(a,error);
        free(error); source_versions_free(&versions); format_snapshot_free(&plan); analysis_free(&next);
        source_format_cancel(a,NULL); return true;
    }
    if (intent->issue) {
        source_versions_free(&sheet->source_versions); sheet->source_versions=versions; versions=(SourceVersions){0};
        worksheet_analysis_adopt(sheet,&a->analysis,&next);
    }
    bool formatted=format_commit(&sheet->formats,&plan);
    if (formatted || intent->issue) { worksheet_record(sheet,intent->issue ? EDIT_ANALYSIS_FORMAT : EDIT_FORMAT); a->book.dirty=true; }
    source_packet_free(&a->source_format_proof.raw);
    a->source_format_proof=(SourceFormatProof){.raw=intent->raw,.digest=intent->digest,.document=a->format_document,
        .version=intent->version,.sheet=a->book.active};
    intent->raw=(SourcePacket){0}; source_format_intent_free(intent); source_format_page(a);
    status(a,formatted ? "Formatting applied to the captured source records." : "The captured source records already have this format.");
    return true;
}
