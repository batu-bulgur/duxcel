#include "worker.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void copy_request_free(CopyRequest *request) {
    if (!request) return;
    clipboard_free(&request->cells);
    for (size_t i=0;i<request->count && i<DX_MAX_OBJECTS+1;i++) pivot_free(&request->patches[i].pivot);
    source_snapshot_free(&request->sources);
    free(request);
}
static void source_digest_rejected_analysis(Analysis *analysis) {
    /* Trusted C jobs transfer ownership of their fixed-slot strings. A rejected
       verification/collection payload can contain strings with zero count; clear
       every actual slot locally before the usual count-based reply cleanup. */
    for (size_t i=0;i<sizeof(analysis->filters)/sizeof(*analysis->filters);i++) { free(analysis->filters[i].column); free(analysis->filters[i].value); analysis->filters[i]=(AnalysisFilter){0}; }
    for (size_t i=0;i<sizeof(analysis->sorts)/sizeof(*analysis->sorts);i++) { free(analysis->sorts[i].column); analysis->sorts[i]=(AnalysisSort){0}; }
    analysis->filter_count=analysis->sort_count=0;
}
#ifndef DUXCEL_BROWSER_HOST
static bool format_job_kind(JobKind kind) { return kind==JOB_VERIFY_SOURCE || kind==JOB_COLLECT_RECORDS; }
static bool format_text_add(const char *text,size_t limit,size_t *bytes) {
    if (!text) return true;
    if (*bytes>=limit) return false;
    size_t length=0,room=limit-*bytes;
    while (length<room && text[length]) length++;
    if (length==room) return false;
    *bytes+=length+1; return true;
}
static bool format_analysis_bytes(const Analysis *a,size_t limit,size_t *bytes) {
    const char *texts[]={a->path,a->schema,a->table,a->query,a->filter_column,a->filter_value,a->group_column,a->measure_column,a->sort_column};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (!format_text_add(texts[i],limit,bytes)) return false;
    /* Rejected C jobs can own zero-count slots; cleanup owns those too. */
    for (size_t i=0;i<sizeof(a->filters)/sizeof(*a->filters);i++)
        if (!format_text_add(a->filters[i].column,limit,bytes) || !format_text_add(a->filters[i].value,limit,bytes)) return false;
    for (size_t i=0;i<sizeof(a->sorts)/sizeof(*a->sorts);i++) if (!format_text_add(a->sorts[i].column,limit,bytes)) return false;
    return true;
}
static bool format_definition_fits(const QueryJob *j) {
    size_t bytes=0;
    if (!format_analysis_bytes(&j->analysis,DX_WORKER_FORMAT_DEFINITION_BYTES,&bytes)) return false;
    const char *texts[]={j->path,j->column,j->search,j->date_start,j->date_end};
    for (size_t i=0;i<sizeof(texts)/sizeof(*texts);i++) if (!format_text_add(texts[i],DX_WORKER_FORMAT_DEFINITION_BYTES,&bytes)) return false;
    for (size_t role=0;role<4;role++) for (size_t i=0;i<DX_PIVOT_FIELDS;i++)
        if (!format_text_add(j->pivot.fields[role][i].name,DX_WORKER_FORMAT_DEFINITION_BYTES,&bytes) ||
            !format_text_add(j->pivot.fields[role][i].value,DX_WORKER_FORMAT_DEFINITION_BYTES,&bytes)) return false;
    return true;
}
static size_t format_reply_owned(const QueryReply *r) {
    if (!r || !format_job_kind(r->kind)) return 0;
    size_t bytes=sizeof(*r);
    if (!format_analysis_bytes(&r->analysis,SIZE_MAX,&bytes) || !format_text_add(r->path,SIZE_MAX,&bytes) || !format_text_add(r->error,SIZE_MAX,&bytes) ||
        r->records.count>DX_RECORD_SELECTION_ROWS || r->records.bytes!=r->records.count*sizeof(uint64_t) ||
        ((r->records.ids!=NULL)!=(r->records.count!=0)) || r->records.bytes>SIZE_MAX-bytes) return SIZE_MAX;
    return bytes+r->records.bytes;
}
static bool copy_sources_match(const Analysis *primary,const CopyRequest *copy,char **error) {
    for (size_t i=0;i<copy->guard_count;i++) {
        const CopySourceGuard *guard=&copy->guards[i];
        const Analysis *source=guard->source ? &copy->sources.items[guard->source-1] : primary;
        if (!database_source_matches(source,guard->expected_source_stamp,error)) return false;
    }
    return true;
}
static bool copy_complete(Database *db,Analysis *analysis,CopyRequest *copy,char **text,char **error) {
    if (!copy || copy->count>DX_MAX_OBJECTS+1 || copy->guard_count>DX_MAX_OBJECTS+1 || copy->sources.count>DX_MAX_OBJECTS || copy->sources.bytes>8*1024*1024) goto invalid;
    for (size_t i=0;i<copy->guard_count;i++) {
        if (copy->guards[i].source>copy->sources.count) goto invalid;
        for (size_t previous=0;previous<i;previous++) if (copy->guards[previous].source==copy->guards[i].source) goto invalid;
    }
    for (size_t i=0;i<copy->count;i++) {
        const CopyPatch *patch=&copy->patches[i];
        if (patch->source>copy->sources.count || patch->target_row>copy->cells.rows || patch->rows>copy->cells.rows-patch->target_row ||
            patch->target_column>copy->cells.columns || patch->columns>copy->cells.columns-patch->target_column) goto invalid;
    }
    if (!copy_sources_match(analysis,copy,error)) return false;
    for (size_t i=0;i<copy->count;i++) {
        CopyPatch *patch=&copy->patches[i]; Page page={0};
        Analysis *source=patch->source ? &copy->sources.items[patch->source-1] : analysis;
        uint64_t expected=0; for (size_t i=0;i<copy->guard_count;i++) if (copy->guards[i].source==patch->source) expected=copy->guards[i].expected_source_stamp;
        bool ok=database_copy_page_bound(db,source,patch->is_pivot ? &patch->pivot : NULL,patch->first,patch->rows,patch->column,patch->columns,expected,&page,error);
        if (!ok) { page_free(&page); return false; }
        for (size_t r=0;r<patch->rows && ok;r++) for (size_t c=0;c<patch->columns && ok;c++) {
            Cell *cell=r<page.row_count ? &page.cells[r*page.column_count+c] : NULL;
            ok=clipboard_set(&copy->cells,patch->target_row+r,patch->target_column+c,cell && !cell->is_null ? cell->text : NULL,error);
        }
        page_free(&page); if (!ok) return false;
    }
    char *out=clipboard_tsv(&copy->cells,error); if (!out) return false;
    if (!copy_sources_match(analysis,copy,error)) { free(out); return false; }
    *text=out; return true;
invalid:
    if (error) { free(*error); *error=dx_strdup("Invalid clipboard source snapshot."); } return false;
}
#endif
static void job_free(QueryJob *j) {
    if (j) { if (j->kind==JOB_VERIFY_SOURCE || j->kind==JOB_COLLECT_RECORDS) source_digest_rejected_analysis(&j->analysis); copy_request_free(j->copy); statistics_request_free(j->statistics); analysis_free(&j->analysis); pivot_free(&j->pivot); if (j->workbook) { workbook_free(j->workbook); free(j->workbook); } free(j->path); free(j->column); free(j->search); free(j->date_start); free(j->date_end); free(j); }
}
void reply_free(QueryReply *r) {
    if (!r) return;
    if (r->kind==JOB_VERIFY_SOURCE || r->kind==JOB_COLLECT_RECORDS) source_digest_rejected_analysis(&r->analysis);
    analysis_free(&r->analysis); page_free(&r->page); free(r->error); free(r->clipboard); free(r->export_data);
    if (r->workbook) { workbook_free(r->workbook); free(r->workbook); }
    distinct_free(&r->distinct); statistics_free(&r->statistics); record_selection_free(&r->records); free(r->path); free(r);
}
#ifndef DUXCEL_BROWSER_HOST
static bool obsolete(QueryWorker *w, uint64_t id) {
    dx_lock(&w->mutex); bool stale = w->stop || w->latest_id != id; dx_unlock(&w->mutex); return stale;
}
static bool job_cancelled(void *context) {
    QueryWorker *w=context; return obsolete(w,w->active_id);
}
#ifdef DUXCEL_TEST_SOURCE_DIGEST
/* Isolated test objects only. No checkpoint module/symbol in shipping builds. */
extern void dx_test_source_digest_checkpoint(QueryWorker *worker,uint64_t bytes_read);
#endif
static bool source_digest_cancelled(void *context,uint64_t bytes_read) {
    QueryWorker *worker=context;
#ifdef DUXCEL_TEST_SOURCE_DIGEST
    dx_test_source_digest_checkpoint(worker,bytes_read);
#else
    (void)bytes_read;
#endif
    return job_cancelled(worker);
}
static bool source_digest_payload_valid(const QueryJob *job,const QueryReply *reply) {
    const Analysis *a=&reply->analysis;
    if (!reply->path || !*reply->path) return false;
    size_t length=0; while (length<=65536 && reply->path[length]) length++;
    if (length>65536 || a->kind!=SOURCE_NONE || a->source_version || a->path || a->schema || a->table || a->query || a->filter_column || a->filter_value || a->group_column || a->measure_column || a->sort_column ||
        a->filter_count || a->sort_count || a->unique_rows || a->descending || a->filter_op || a->aggregate || a->filter_number.kind || a->filter_number.precision || a->filter_number.scale ||
        job->workbook || job->copy || job->statistics || job->column || job->search || job->date_start || job->date_end || job->offset || job->rows || job->first_col || job->cols ||
        (job->known_total!=0 && job->known_total!=-1) || job->include_header || job->include_totals || job->include_chart_other || job->is_preview || job->refresh_source || job->is_table || job->date_level) return false;
    for (size_t i=0;i<sizeof(a->filters)/sizeof(*a->filters);i++) {
        const AnalysisFilter *field=&a->filters[i];
        if (field->column || field->value || field->op || field->number.kind || field->number.precision || field->number.scale) return false;
    }
    for (size_t i=0;i<sizeof(a->sorts)/sizeof(*a->sorts);i++) if (a->sorts[i].column || a->sorts[i].descending) return false;
    for (size_t role=0;role<4;role++) if (job->pivot.counts[role]) return false;
    return true;
}

static void verify_source(QueryWorker *worker,const QueryJob *job,QueryReply *reply) {
    reply->source_digest_status=SOURCE_DIGEST_ERROR;
    if (!source_digest_payload_valid(job,reply)) { source_digest_rejected_analysis(&reply->analysis); reply->error=dx_strdup("Source verification requires one bounded file path without analytical payloads."); return; }
    if (obsolete(worker,job->id)) { reply->source_digest_status=SOURCE_DIGEST_CANCELLED; reply->error=dx_strdup("Source-content verification was cancelled."); return; }
    if (job->expected_source_stamp && dx_file_stamp(reply->path)!=job->expected_source_stamp) {
        reply->source_digest_status=SOURCE_DIGEST_CHANGED; reply->error=dx_strdup("The source changed since its displayed result. Refresh before verifying its content."); return;
    }
    SourceDigest digest={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    reply->source_digest_status=source_digest_file(reply->path,source_digest_cancelled,worker,&digest,error);
    if (reply->source_digest_status==SOURCE_DIGEST_OK && job->expected_source_stamp &&
        (digest.observed_stamp!=job->expected_source_stamp || dx_file_stamp(reply->path)!=job->expected_source_stamp)) {
        reply->source_digest_status=SOURCE_DIGEST_CHANGED; snprintf(error,sizeof(error),"The source changed before its verified content could be published.");
    }
    if (reply->source_digest_status==SOURCE_DIGEST_OK && obsolete(worker,job->id)) {
        reply->source_digest_status=SOURCE_DIGEST_CANCELLED; snprintf(error,sizeof(error),"Source-content verification was cancelled.");
    }
    if (reply->source_digest_status==SOURCE_DIGEST_OK) reply->source_digest=digest;
    else reply->error=dx_strdup(*error ? error : "Source-content verification failed.");
}
static bool record_selection_payload_valid(const QueryJob *job,const QueryReply *reply) {
    const Analysis *a=&reply->analysis;
    if (!job->expected_file_identity_valid || !job->expected_source_stamp || !job->rows || job->offset>=DX_SHEET_ROWS || job->rows>DX_SHEET_ROWS-job->offset ||
        (a->kind!=SOURCE_CSV && a->kind!=SOURCE_PARQUET) || a->query || a->unique_rows || a->schema || a->table || a->group_column || a->measure_column ||
        a->filter_count>=DX_ANALYSIS_FILTERS || a->sort_count>=DX_ANALYSIS_SORTS || analysis_text_bytes(a)>8*1024*1024 ||
        job->workbook || job->copy || job->statistics || reply->path || job->column || job->search || job->date_start || job->date_end || job->first_col || job->cols ||
        (job->known_total!=0 && job->known_total!=-1) || job->include_header || job->include_totals || job->include_chart_other || job->is_preview || job->refresh_source || job->date_level ||
        (job->object_id ? !job->is_table : job->is_table)) return false;
    for (size_t role=0;role<4;role++) if (job->pivot.counts[role]) return false;
    return true;
}
static void *worker_main(void *ptr) {
    QueryWorker *w = ptr;
    for (;;) {
        dx_lock(&w->mutex);
        while (!w->stop && !w->pending) dx_cond_wait(&w->condition, &w->mutex);
        if (w->stop) { dx_unlock(&w->mutex); break; }
        QueryJob *j = w->pending; w->pending = NULL; w->running = true; w->active_id=j->id; w->jobs_started++;
        bool format_definition_rejected=w->pending_format_definition_rejected;
        w->active_kind=j->kind; w->active_format_reserved=w->pending_format_reserved;
        w->pending_format_reserved=0; w->pending_format_definition_rejected=false;
        dx_unlock(&w->mutex);
        QueryReply *r = dx_alloc(sizeof(*r)); r->id = j->id; r->kind = j->kind; r->object_id=j->object_id; r->revision=j->revision; r->is_preview=j->is_preview;
        r->analysis=j->analysis; memset(&j->analysis,0,sizeof(j->analysis));
        r->path=j->path; j->path=NULL;
        /* File hashing never opens DuckDB or runs source/result SQL. */
        if (format_definition_rejected) {
            r->source_digest_status=SOURCE_DIGEST_ERROR;
            r->error=dx_strdup("Native formatting request definitions exceed the 8 MiB owned limit. No file or identity query was opened.");
        }
        else if (j->include_chart_other && (j->kind!=JOB_PIVOT || j->is_table)) r->error=dx_strdup("Chart Other readouts require an analytical Pivot job.");
        else if (j->kind==JOB_VERIFY_SOURCE) verify_source(w,j,r);
        else if (j->kind==JOB_COLLECT_RECORDS && !record_selection_payload_valid(j,r)) r->error=dx_strdup("Record collection requires one bounded raw-source range and its captured native identity without analytical payloads.");
        else if (j->kind==JOB_SAVE) workbook_save(j->workbook,r->path,&r->error);
        else if (j->kind==JOB_LOAD) {
            r->workbook=dx_alloc(sizeof(*r->workbook)); workbook_init(r->workbook);
            workbook_load(r->workbook,r->path,&r->error);
        } else {
        /* Opening a connection is outside the UI mutex. Publish it only when complete. */
        if (!w->db.connection) {
            Database opened;
            if (database_open(&opened, &r->error)) {
                opened.cancelled=job_cancelled; opened.cancel_context=w;
                dx_lock(&w->mutex); w->db = opened; dx_unlock(&w->mutex);
            }
        }
        if (!r->error && !obsolete(w, j->id)) {
            if (j->refresh_source && !database_refresh(&w->db,&r->error)) goto finished;
            if (j->kind == JOB_VALIDATE_SQL) database_validate_relation(&w->db,&r->analysis,&r->error);
            else if (j->kind==JOB_COLLECT_RECORDS) database_collect_records(&w->db,&r->analysis,j->offset,j->rows,&j->expected_file_identity,j->expected_source_stamp,&r->records,&r->error);
            else if (j->kind == JOB_DISTINCT) {
                DistinctPage values={0};
                if (database_source_matches(&r->analysis,j->expected_source_stamp,&r->error) &&
                    database_pivot_distinct(&w->db,&r->analysis,j->object_id && !j->is_table ? &j->pivot : NULL,j->column,j->search,j->date_level,j->date_start,j->date_end,j->offset,j->known_total,&values,&r->error) &&
                    database_source_matches(&r->analysis,j->expected_source_stamp,&r->error)) { r->distinct=values; memset(&values,0,sizeof(values)); }
                distinct_free(&values);
            }
            else if (j->kind == JOB_PAGE) database_fetch_window(&w->db, &r->analysis, j->offset, j->known_total, &r->page, &r->error);
            else if (j->kind == JOB_STEP_PREVIEW) database_fetch_table(&w->db,&r->analysis,j->offset,j->known_total,&r->page,&r->error);
            else if (j->kind == JOB_PIVOT && j->is_table) database_fetch_table_window(&w->db,&r->analysis,j->offset,j->known_total,&r->page,&r->error);
            else if (j->kind == JOB_PIVOT) {
                if (j->include_chart_other)
                    database_pivot_with_other(&w->db,&r->analysis,&j->pivot,j->offset,j->known_total,j->include_totals,j->expected_source_stamp,&r->page,&r->error);
                else if (database_pivot(&w->db,&r->analysis,&j->pivot,j->offset,j->known_total,&r->page,&r->error) && j->include_totals)
                    database_pivot_totals(&w->db,&r->analysis,&j->pivot,&r->page,&r->error);
            }
            else if (j->kind == JOB_PIVOT_COPY) {
                if (database_source_matches(&r->analysis,j->expected_source_stamp,&r->error) &&
                    database_pivot_copy(&w->db,&r->analysis,&j->pivot,j->offset,j->rows,j->first_col,j->cols,j->include_header,&r->clipboard,&r->error) &&
                    !database_source_matches(&r->analysis,j->expected_source_stamp,&r->error)) { free(r->clipboard); r->clipboard=NULL; }
            }
            else if (j->kind == JOB_EXPORT && j->object_id && !j->is_table) database_export_pivot_parquet_bound(&w->db,&r->analysis,&j->pivot,r->path,j->expected_source_stamp,&r->exported_rows,&r->error);
            else if (j->kind == JOB_EXPORT) database_export_parquet_bound(&w->db,&r->analysis,r->path,j->expected_source_stamp,&r->exported_rows,&r->error);
            else if (j->kind == JOB_COUNT) {
                uint64_t total=0;
                if (database_source_matches(&r->analysis,j->expected_source_stamp,&r->error) && database_count(&w->db,&r->analysis,&total,&r->error) &&
                    database_source_matches(&r->analysis,j->expected_source_stamp,&r->error)) r->counted_rows=total;
            }
            else if (j->kind == JOB_RANGE_COPY) copy_complete(&w->db,&r->analysis,j->copy,&r->clipboard,&r->error);
            else if (j->kind == JOB_STATISTICS && j->statistics) database_statistics_ranges(&w->db,&r->analysis,j->statistics,&r->statistics,&r->error);
            else if (j->kind == JOB_STATISTICS) database_statistics_bound(&w->db,&r->analysis,j->object_id && !j->is_table ? &j->pivot : NULL,j->offset,j->rows,j->first_col,j->cols,j->expected_source_stamp,&r->statistics,&r->error);
            else database_copy_bound(&w->db, &r->analysis, j->offset, j->rows, j->first_col, j->cols,j->expected_source_stamp,&r->clipboard,&r->error);
        }
        }
finished:
        dx_lock(&w->mutex); w->running = false;
        if (!w->stop && w->latest_id == j->id) {
            reply_free(w->reply); w->reply = r; w->published_format_owned=format_reply_owned(r); r = NULL;
        }
        dx_unlock(&w->mutex); reply_free(r); job_free(j);
        /* Cancellation only requests retirement. Keep the reservation until
           these actual frees finish, including after running became false. */
        dx_lock(&w->mutex); w->active_format_reserved=0; w->active_kind=JOB_PAGE; dx_unlock(&w->mutex);
    }
    return NULL;
}
bool worker_start(QueryWorker *w) {
    memset(w, 0, sizeof(*w)); dx_mutex_init(&w->mutex); dx_cond_init(&w->condition);
    w->started = dx_thread_start(&w->thread, worker_main, w);
    if (!w->started) { dx_cond_destroy(&w->condition); dx_mutex_destroy(&w->mutex); }
    return w->started;
}
void worker_submit(QueryWorker *w, QueryJob *j) {
    bool format=format_job_kind(j->kind),definition_rejected=format && !format_definition_fits(j);
    size_t reserved=format ? (definition_rejected || j->workbook || j->copy || j->statistics ? SIZE_MAX : DX_WORKER_FORMAT_RESERVED_BYTES) : 0;
    dx_lock(&w->mutex); w->latest_id = j->id;
    job_free(w->pending); w->pending = j; reply_free(w->reply); w->reply = NULL;
    w->pending_format_reserved=reserved; w->pending_format_definition_rejected=definition_rejected; w->published_format_owned=0;
    if (w->running && w->db.connection) duckdb_interrupt(w->db.connection);
    dx_signal(&w->condition); dx_unlock(&w->mutex);
}
QueryReply *worker_poll(QueryWorker *w) {
    dx_lock(&w->mutex); QueryReply *r = w->reply; w->reply = NULL; w->published_format_owned=0; dx_unlock(&w->mutex); return r;
}
WorkerFormatUsage worker_format_usage(QueryWorker *w) {
    if (!w || !w->started) return (WorkerFormatUsage){0};
    dx_lock(&w->mutex);
    WorkerFormatUsage usage={.active_reserved=w->active_format_reserved,.pending_reserved=w->pending_format_reserved,.published_owned=w->published_format_owned,
        .active_count=w->active_format_reserved!=0,.pending_count=w->pending_format_reserved!=0,.published_count=w->reply && format_job_kind(w->reply->kind)};
    dx_unlock(&w->mutex); return usage;
}
void worker_cancel(QueryWorker *w) {
    dx_lock(&w->mutex); w->latest_id++; job_free(w->pending); w->pending = NULL;
    reply_free(w->reply); w->reply = NULL;
    w->pending_format_reserved=w->published_format_owned=0; w->pending_format_definition_rejected=false;
    if (w->running && w->db.connection) duckdb_interrupt(w->db.connection);
    dx_unlock(&w->mutex);
}
void worker_stop(QueryWorker *w) {
    if (!w->started) return;
    dx_lock(&w->mutex); w->stop = true;
    if (w->db.connection) duckdb_interrupt(w->db.connection);
    dx_signal(&w->condition); dx_unlock(&w->mutex);
    dx_thread_join(w->thread); job_free(w->pending); w->pending=NULL; reply_free(w->reply); w->reply=NULL;
    w->active_format_reserved=w->pending_format_reserved=w->published_format_owned=0; w->active_kind=JOB_PAGE; w->pending_format_definition_rejected=false;
    w->sql_executed=w->db.query_count; w->counts_executed=w->db.count_queries; database_close(&w->db);
    dx_cond_destroy(&w->condition); dx_mutex_destroy(&w->mutex); w->started = false;
}

#else
#include "worker_browser.inc"
#endif
