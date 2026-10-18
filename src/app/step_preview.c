#include "step_preview.h"
#include <stdlib.h>
#include <string.h>
static void clear_widths(StepPreview *preview) {
    for (size_t i=0;i<preview->width_count;i++) free(preview->width_names[i]);
    free(preview->width_names); free(preview->width_values); preview->width_names=NULL; preview->width_values=NULL; preview->width_count=0;
}
void step_preview_free(StepPreview *preview) {
    analysis_free(&preview->source); page_free(&preview->page); page_free(&preview->spare); free(preview->title); free(preview->error);
    clear_widths(preview); format_free(&preview->formats); statistics_free(&preview->statistics); free(preview->stats_error); if (preview->initialized) grid_destroy(&preview->grid); memset(preview,0,sizeof(*preview));
}
bool step_preview_start_named(StepPreview *preview,const Analysis *source,size_t step,uint64_t id,uint64_t revision,const Grid *presentation,const char *display_filename) {
    Analysis prefix={0}; UIStep rows[DX_UI_STEPS]; size_t count=ui_source_steps_named(source,0,display_filename,rows);
    if (step>=count || !source || source->kind==SOURCE_NONE || !ui_step_source(source,step,&prefix)) return false;
    char *title=dx_strdup(rows[step].title); step_preview_free(preview);
    preview->source=prefix; preview->title=title; preview->step=step; preview->source_id=id; preview->source_revision=revision; preview->active=true;
    grid_init(&preview->grid); preview->initialized=true; preview->grid.bounded=true; preview->grid.source_header=true;
    if (presentation) { preview->grid.zoom=presentation->zoom; preview->grid.row_height=presentation->row_height; preview->grid.freeze=presentation->freeze; preview->grid.hide_gridlines=presentation->hide_gridlines; preview->grid.elastic_enabled=presentation->elastic_enabled; }
    if (presentation && presentation->data_columns) {
        preview->width_count=presentation->data_columns; preview->width_names=dx_alloc(preview->width_count*sizeof(char *)); preview->width_values=dx_alloc(preview->width_count*sizeof(float));
        for (size_t i=0;i<preview->width_count;i++) { preview->width_names[i]=dx_strdup(presentation->column_names[i]); preview->width_values[i]=presentation->widths[i]; }
    }
    return true;
}
bool step_preview_start(StepPreview *preview,const Analysis *source,size_t step,uint64_t id,uint64_t revision,const Grid *presentation) {
    return step_preview_start_named(preview,source,step,id,revision,presentation,NULL);
}
void step_preview_presentation(StepPreview *preview,const Page *fields,const Grid *grid,const FormatStore *formats,size_t first) {
    if (!preview->active || !fields || !grid || fields->column_count>16384) return;
    clear_widths(preview); format_free(&preview->formats); preview->width_count=fields->column_count;
    if (preview->width_count) {
        preview->width_names=dx_alloc(preview->width_count*sizeof(char *)); preview->width_values=dx_alloc(preview->width_count*sizeof(float));
        for (size_t c=0;c<preview->width_count;c++) { preview->width_names[c]=dx_strdup(fields->columns[c].name); preview->width_values[c]=c+first<grid->width_count ? grid->widths[c+first] : 128; }
    }
    if (!formats || !fields->column_count) return;
    preview->formats.items=dx_alloc(formats->count*sizeof(FormatPatch));
    for (size_t i=0;i<formats->count;i++) {
        FormatPatch patch=formats->items[i];
        if (patch.selector!=FORMAT_SPATIAL || patch.range.r0 || patch.range.r1!=DX_SHEET_ROWS-1 || patch.range.c1<first || patch.range.c0>=first+fields->column_count) continue;
        patch.range.c0=patch.range.c0>first ? patch.range.c0-first : 0;
        patch.range.c1=patch.range.c1-first<fields->column_count ? patch.range.c1-first : fields->column_count-1;
        preview->formats.items[preview->formats.count++]=patch;
    }
}
QueryJob *step_preview_job(StepPreview *preview,uint64_t request,bool force) {
    if (!preview->active || preview->loading || preview->error || preview->grid.dragging || preview->grid.resizing>=0) return NULL;
    uint64_t first=preview->grid.top>64 ? preview->grid.top-64 : 0;
    if (!force && preview->page.column_count && grid_cached(&preview->grid,&preview->page)) return NULL;
    QueryJob *job=dx_alloc(sizeof(*job)); job->kind=JOB_STEP_PREVIEW; job->id=request; job->is_preview=true; job->analysis=analysis_copy(&preview->source);
    job->offset=first; job->known_total=preview->page.column_count ? (int64_t)preview->page.total : -1;
    preview->request=request; preview->loading=true; return job;
}
bool step_preview_reply(StepPreview *preview,QueryReply *reply) {
    if (!preview->active || reply->kind!=JOB_STEP_PREVIEW || reply->id!=preview->request) return false;
    preview->loading=false; preview->request=0; free(preview->error); preview->error=reply->error; reply->error=NULL;
    if (!preview->error && page_records_changed(&preview->page,&reply->page))
        preview->error=dx_strdup("The source changed between pages. Reopen the preview after refreshing the source.");
    if (!preview->error) {
        bool first=!preview->page.column_count; page_free(&preview->spare); preview->spare=preview->page; preview->page=reply->page; memset(&reply->page,0,sizeof(reply->page));
        analysis_merge_types(&preview->source,&reply->analysis);
        if (first) {
            bool elastic=preview->grid.elastic_enabled; grid_schema(&preview->grid,&preview->page,true); preview->grid.elastic_enabled=elastic;
            bool same=preview->width_count==preview->page.column_count;
            for (size_t c=0;same && c<preview->page.column_count;c++) same=!strcmp(preview->page.columns[c].name,preview->width_names[c]);
            if (!same) format_free(&preview->formats);
            preview->grid.formats=&preview->formats;
            for (size_t c=0;c<preview->page.column_count;c++) {
                /* Ordinary filter/sort prefixes retain schema order. Avoid a
                   quadratic name search on a wide, unchanged source. */
                if (c<preview->width_count && !strcmp(preview->page.columns[c].name,preview->width_names[c])) grid_set_width(&preview->grid,c,preview->width_values[c]);
                else for (size_t i=0;i<preview->width_count;i++) if (!strcmp(preview->page.columns[c].name,preview->width_names[i])) { grid_set_width(&preview->grid,c,preview->width_values[i]); break; }
            }
            clear_widths(preview);
        }
    }
    return true;
}
bool step_preview_watch_statistics(StepPreview *preview,bool requested) {
    GridRegion bounds={0}; uint64_t request=preview->stats_request;
    if (preview->active && preview->page.column_count && preview->page.total) {
        grid_bounds(&preview->grid,&bounds.r0,&bounds.r1,&bounds.c0,&bounds.c1);
        bounds.active=bounds.r0<preview->page.total && bounds.c0<preview->page.column_count;
        if (bounds.r1>=preview->page.total) bounds.r1=preview->page.total-1;
        if (bounds.c1>=preview->page.column_count) bounds.c1=preview->page.column_count-1;
    }
    GridRegion *old=&preview->stats_bounds;
    bool same=bounds.active==old->active && bounds.r0==old->r0 && bounds.r1==old->r1 && bounds.c0==old->c0 && bounds.c1==old->c1;
    if (!same) {
        preview->stats_bounds=bounds; preview->stats_request=0; preview->stats_changed=dx_millis();
        statistics_free(&preview->statistics); free(preview->stats_error); preview->stats_error=NULL; preview->stats_state=PREVIEW_STATS_IDLE;
    }
    if (requested && bounds.active && preview->stats_state==PREVIEW_STATS_IDLE) {
        preview->stats_state=bounds.c1-bounds.c0+1>256 ? PREVIEW_STATS_UNAVAILABLE : PREVIEW_STATS_WAITING; preview->stats_changed=dx_millis();
    }
    if (!requested && preview->stats_state==PREVIEW_STATS_WAITING) preview->stats_state=PREVIEW_STATS_IDLE;
    return request && !same;
}
QueryJob *step_preview_statistics_job(StepPreview *preview,uint64_t request) {
    if (!preview->active || preview->loading || preview->stats_state!=PREVIEW_STATS_WAITING || preview->grid.dragging || preview->grid.resizing>=0 || dx_millis()-preview->stats_changed<140) return NULL;
    GridRegion *bounds=&preview->stats_bounds; QueryJob *job=dx_alloc(sizeof(*job));
    job->kind=JOB_STATISTICS; job->is_preview=true; job->id=request; job->analysis=analysis_copy(&preview->source);
    job->expected_source_stamp=preview->page.records.stamp;
    job->offset=bounds->r0; job->rows=bounds->r1-bounds->r0+1; job->first_col=bounds->c0; job->cols=bounds->c1-bounds->c0+1;
    preview->stats_request=request; preview->stats_state=PREVIEW_STATS_LOADING; return job;
}
bool step_preview_statistics_reply(StepPreview *preview,QueryReply *reply) {
    if (!preview->active || reply->kind!=JOB_STATISTICS || !reply->is_preview || reply->id!=preview->stats_request) return false;
    preview->stats_request=0; statistics_free(&preview->statistics); free(preview->stats_error); preview->stats_error=reply->error; reply->error=NULL;
    preview->statistics=reply->statistics; memset(&reply->statistics,0,sizeof(reply->statistics)); preview->stats_state=preview->stats_error ? PREVIEW_STATS_FAILED : PREVIEW_STATS_READY; return true;
}
