#include "worker.h"
#include <stdlib.h>
#include <string.h>

void copy_request_free(CopyRequest *request) {
    if (!request) return;
    clipboard_free(&request->cells);
    for (size_t i=0;i<request->count && i<DX_MAX_OBJECTS+1;i++) pivot_free(&request->patches[i].pivot);
    source_snapshot_free(&request->sources);
    free(request);
}
#ifndef DUXCEL_BROWSER_HOST
static bool copy_complete(Database *db,Analysis *analysis,CopyRequest *copy,char **text,char **error) {
    if (copy->count>DX_MAX_OBJECTS+1 || copy->sources.count>DX_MAX_OBJECTS || copy->sources.bytes>8*1024*1024) { if (error) { free(*error); *error=dx_strdup("Invalid clipboard source snapshot."); } return false; }
    for (size_t i=0;i<copy->count;i++) {
        CopyPatch *patch=&copy->patches[i]; Page page={0};
        if (patch->source>copy->sources.count) { if (error) { free(*error); *error=dx_strdup("Missing clipboard table source."); } return false; }
        Analysis *source=patch->source ? &copy->sources.items[patch->source-1] : analysis;
        bool ok=database_copy_page(db,source,patch->is_pivot ? &patch->pivot : NULL,patch->first,patch->rows,patch->column,patch->columns,&page,error);
        if (!ok) { page_free(&page); return false; }
        for (size_t r=0;r<patch->rows && ok;r++) for (size_t c=0;c<patch->columns && ok;c++) {
            Cell *cell=r<page.row_count ? &page.cells[r*page.column_count+c] : NULL;
            ok=clipboard_set(&copy->cells,patch->target_row+r,patch->target_column+c,cell && !cell->is_null ? cell->text : NULL,error);
        }
        page_free(&page); if (!ok) return false;
    }
    *text=clipboard_tsv(&copy->cells,error); return *text!=NULL;
}
#endif
static void job_free(QueryJob *j) {
    if (j) { copy_request_free(j->copy); statistics_request_free(j->statistics); analysis_free(&j->analysis); pivot_free(&j->pivot); if (j->workbook) { workbook_free(j->workbook); free(j->workbook); } free(j->path); free(j->column); free(j->search); free(j->date_start); free(j->date_end); free(j); }
}
void reply_free(QueryReply *r) {
    if (!r) return;
    analysis_free(&r->analysis); page_free(&r->page); free(r->error); free(r->clipboard);
    if (r->workbook) { workbook_free(r->workbook); free(r->workbook); }
    distinct_free(&r->distinct); statistics_free(&r->statistics); free(r->path); free(r);
}
#ifndef DUXCEL_BROWSER_HOST
static bool obsolete(QueryWorker *w, uint64_t id) {
    dx_lock(&w->mutex); bool stale = w->stop || w->latest_id != id; dx_unlock(&w->mutex); return stale;
}
static bool job_cancelled(void *context) {
    QueryWorker *w=context; return obsolete(w,w->active_id);
}
static void *worker_main(void *ptr) {
    QueryWorker *w = ptr;
    for (;;) {
        dx_lock(&w->mutex);
        while (!w->stop && !w->pending) dx_cond_wait(&w->condition, &w->mutex);
        if (w->stop) { dx_unlock(&w->mutex); break; }
        QueryJob *j = w->pending; w->pending = NULL; w->running = true; w->active_id=j->id; w->jobs_started++;
        dx_unlock(&w->mutex);
        QueryReply *r = dx_alloc(sizeof(*r)); r->id = j->id; r->kind = j->kind; r->object_id=j->object_id; r->revision=j->revision; r->is_preview=j->is_preview;
        r->analysis=j->analysis; memset(&j->analysis,0,sizeof(j->analysis));
        r->path=j->path; j->path=NULL;
        if (j->kind==JOB_SAVE) workbook_save(j->workbook,r->path,&r->error);
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
            else if (j->kind == JOB_DISTINCT) database_pivot_distinct(&w->db,&r->analysis,j->object_id && !j->is_table ? &j->pivot : NULL,j->column,j->search,j->date_level,j->date_start,j->date_end,j->offset,j->known_total,&r->distinct,&r->error);
            else if (j->kind == JOB_PAGE) database_fetch_window(&w->db, &r->analysis, j->offset, j->known_total, &r->page, &r->error);
            else if (j->kind == JOB_STEP_PREVIEW) database_fetch_table(&w->db,&r->analysis,j->offset,j->known_total,&r->page,&r->error);
            else if (j->kind == JOB_PIVOT && j->is_table) database_fetch_table_window(&w->db,&r->analysis,j->offset,j->known_total,&r->page,&r->error);
            else if (j->kind == JOB_PIVOT) {
                if (database_pivot(&w->db,&r->analysis,&j->pivot,j->offset,j->known_total,&r->page,&r->error) && j->include_totals)
                    database_pivot_totals(&w->db,&r->analysis,&j->pivot,&r->page,&r->error);
            }
            else if (j->kind == JOB_PIVOT_COPY) database_pivot_copy(&w->db,&r->analysis,&j->pivot,j->offset,j->rows,j->first_col,j->cols,j->include_header,&r->clipboard,&r->error);
            else if (j->kind == JOB_EXPORT && j->object_id && !j->is_table) database_export_pivot_parquet(&w->db,&r->analysis,&j->pivot,r->path,&r->exported_rows,&r->error);
            else if (j->kind == JOB_EXPORT) database_export_parquet(&w->db,&r->analysis,r->path,&r->exported_rows,&r->error);
            else if (j->kind == JOB_COUNT) database_count(&w->db,&r->analysis,&r->counted_rows,&r->error);
            else if (j->kind == JOB_RANGE_COPY) copy_complete(&w->db,&r->analysis,j->copy,&r->clipboard,&r->error);
            else if (j->kind == JOB_STATISTICS && j->statistics) database_statistics_ranges(&w->db,&r->analysis,j->statistics,&r->statistics,&r->error);
            else if (j->kind == JOB_STATISTICS) database_statistics(&w->db,&r->analysis,j->object_id && !j->is_table ? &j->pivot : NULL,j->offset,j->rows,j->first_col,j->cols,&r->statistics,&r->error);
            else database_copy(&w->db, &r->analysis, j->offset, j->rows, j->first_col, j->cols, &r->clipboard, &r->error);
        }
        }
finished:
        dx_lock(&w->mutex); w->running = false;
        if (!w->stop && w->latest_id == j->id) {
            reply_free(w->reply); w->reply = r; r = NULL;
        }
        dx_unlock(&w->mutex); reply_free(r); job_free(j);
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
    dx_lock(&w->mutex); w->latest_id = j->id;
    job_free(w->pending); w->pending = j; reply_free(w->reply); w->reply = NULL;
    if (w->running && w->db.connection) duckdb_interrupt(w->db.connection);
    dx_signal(&w->condition); dx_unlock(&w->mutex);
}
QueryReply *worker_poll(QueryWorker *w) {
    dx_lock(&w->mutex); QueryReply *r = w->reply; w->reply = NULL; dx_unlock(&w->mutex); return r;
}
void worker_cancel(QueryWorker *w) {
    dx_lock(&w->mutex); w->latest_id++; job_free(w->pending); w->pending = NULL;
    reply_free(w->reply); w->reply = NULL;
    if (w->running && w->db.connection) duckdb_interrupt(w->db.connection);
    dx_unlock(&w->mutex);
}
void worker_stop(QueryWorker *w) {
    if (!w->started) return;
    dx_lock(&w->mutex); w->stop = true;
    if (w->db.connection) duckdb_interrupt(w->db.connection);
    dx_signal(&w->condition); dx_unlock(&w->mutex);
    dx_thread_join(w->thread); job_free(w->pending); reply_free(w->reply);
    w->sql_executed=w->db.query_count; w->counts_executed=w->db.count_queries; database_close(&w->db);
    dx_cond_destroy(&w->condition); dx_mutex_destroy(&w->mutex); w->started = false;
}

#else
#include "worker_browser.inc"
#endif
