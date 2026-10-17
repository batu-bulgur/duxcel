#include "app/step_preview.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL preview:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static QueryReply *wait_reply(QueryWorker *worker) {
    double until=dx_millis()+10000;
    while (dx_millis()<until) { QueryReply *reply=worker_poll(worker); if (reply) return reply; dx_sleep_ms(1); }
    CHECK(!"Preview worker timeout"); return NULL;
}
int main(void) {
    const float scales[]={.8f,1,1.5f,2.5f};
    for (size_t i=0;i<4;i++) for (int width=180;width<=1440;width+=70) {
        Rectangle canvas={23,31,width*scales[i],360*scales[i]}; UIPreviewLayout layout=ui_preview_layout(canvas,scales[i]);
        CHECK(layout.valid && layout.banner.height==29*scales[i]);
        CHECK(layout.grid.y==canvas.y+layout.banner.height+layout.context.height && layout.grid.y+layout.grid.height==canvas.y+canvas.height);
        CHECK(layout.return_button.x>=canvas.x && layout.return_button.x+layout.return_button.width<=canvas.x+canvas.width);
    }
    CHECK(!ui_preview_layout((Rectangle){0,0,100,100},1).valid); CHECK(!ui_preview_layout((Rectangle){0,0,800,600},NAN).valid);
    QueryWorker worker; CHECK(worker_start(&worker)); char *error=NULL;
    Analysis live={.kind=SOURCE_CSV,.path=dx_strdup("tests/fixtures/transactions.csv")};
    CHECK(analysis_filter_set(&live,"City",FILTER_EQUAL,"İstanbul",&error)); CHECK(analysis_sort_set(&live,"Amount",true,false,&error));
    Page original={0}; Database db; CHECK(database_open(&db,&error)); CHECK(database_fetch_table(&db,&live,0,-1,&original,&error));
    Grid grid; grid_init(&grid); grid_schema(&grid,&original,true); grid_set_width(&grid,1,207); grid.zoom=.9f; grid.freeze=true; grid.elastic_enabled=false;
    grid_select_cell(&grid,1,2,false); Cell *cached=original.cells; StepPreview preview={0};
    CHECK(step_preview_start(&preview,&live,0,74,9,&grid)); CHECK(preview.active && !preview.source.filter_column && !preview.source.sort_column && preview.source.path!=live.path);
    FormatStore styles={0}; CellFormat accounting=format_default(); accounting.number=FORMAT_ACCOUNTING;
    CHECK(format_apply(&styles,(FormatRange){0,DX_SHEET_ROWS-1,2,2},FORMAT_NUMERIC,accounting,&error));
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&styles,(FormatRange){0,0,1,1},FORMAT_BOLD,bold,&error));
    step_preview_presentation(&preview,&original,&grid,&styles,0); CHECK(preview.formats.count==1 && preview.formats.items!=styles.items);
    CHECK(preview.grid.zoom==.9f && preview.grid.freeze && !preview.grid.elastic_enabled);
    CHECK(!step_preview_start(&preview,&live,99,74,9,&grid)); CHECK(preview.active && preview.step==0);
    QueryJob *job=step_preview_job(&preview,1,false); CHECK(job && job->kind==JOB_STEP_PREVIEW && job->is_preview && job->known_total==-1 && job->analysis.path!=preview.source.path);
    CHECK(!step_preview_job(&preview,2,false)); worker_submit(&worker,job); QueryReply *reply=wait_reply(&worker);
    CHECK(reply->is_preview && !reply->error && reply->page.total==5 && reply->page.row_count==5 && reply->page.column_count==6);
    uint64_t request=preview.request; preview.request=99; CHECK(!step_preview_reply(&preview,reply) && reply->page.cells); preview.request=request;
    CHECK(step_preview_reply(&preview,reply) && !reply->page.cells && !preview.loading && preview.page.cells); reply_free(reply);
    CHECK(preview.grid.widths[1]==207 && !preview.width_count && !preview.grid.elastic_enabled);
    CHECK(preview.grid.formats==&preview.formats && format_at(&preview.formats,4,2).number==FORMAT_ACCOUNTING && !(format_at(&preview.formats,0,1).flags&FORMAT_BOLD)); format_free(&styles);
    grid_layout(&preview.grid,preview.grid.zoom,&preview.page,(Rectangle){0,0,1000,500}); CHECK(!step_preview_job(&preview,2,false));
    grid_set_width(&preview.grid,1,310); CHECK(grid.widths[1]==207 && grid.row==1 && grid.col==2 && original.cells==cached && original.total==2 && live.filter_column && live.sort_column);
    CHECK(step_preview_start(&preview,&live,1,74,9,&grid)); job=step_preview_job(&preview,2,false); worker_submit(&worker,job); reply=wait_reply(&worker);
    CHECK(!reply->error && reply->page.total==2 && !reply->analysis.sort_column); CHECK(step_preview_reply(&preview,reply)); reply_free(reply);
    CHECK(preview.page.cells[0].number==1 && preview.page.cells[6].number==3);
    grid_select_cell(&preview.grid,0,2,false); preview.grid.end_row=1;
    CHECK(!step_preview_watch_statistics(&preview,true) && preview.stats_state==PREVIEW_STATS_WAITING); preview.stats_changed=dx_millis()-200;
    job=step_preview_statistics_job(&preview,20); CHECK(job && job->is_preview && job->kind==JOB_STATISTICS && job->rows==2 && job->cols==1 && job->analysis.filter_column && !job->analysis.sort_column);
    worker_submit(&worker,job); reply=wait_reply(&worker); CHECK(!reply->error && reply->statistics.numeric==2 && !strcmp(reply->statistics.sum,"19.75"));
    preview.stats_request=21; CHECK(!step_preview_statistics_reply(&preview,reply) && reply->statistics.sum); preview.stats_request=20;
    CHECK(step_preview_statistics_reply(&preview,reply) && !reply->statistics.sum && preview.stats_state==PREVIEW_STATS_READY); reply_free(reply);
    char *sum=preview.statistics.sum; preview.grid.horizontal=150; preview.grid.zoom=1.5f;
    CHECK(!step_preview_watch_statistics(&preview,true) && preview.statistics.sum==sum && !step_preview_statistics_job(&preview,21));
    grid_select_cell(&preview.grid,0,1,false); CHECK(!step_preview_watch_statistics(&preview,true) && !preview.statistics.sum && preview.stats_state==PREVIEW_STATS_WAITING); preview.stats_changed=dx_millis()-200;
    job=step_preview_statistics_job(&preview,21); worker_submit(&worker,job);
    grid_select_cell(&preview.grid,0,2,false); CHECK(step_preview_watch_statistics(&preview,true) && !preview.stats_request); reply=wait_reply(&worker);
    CHECK(!step_preview_statistics_reply(&preview,reply) && reply->statistics.distinct==1); reply_free(reply);
    CHECK(step_preview_start(&preview,&live,2,74,9,&grid)); job=step_preview_job(&preview,3,false); worker_submit(&worker,job); reply=wait_reply(&worker);
    CHECK(!reply->error && reply->page.total==2 && reply->analysis.sort_column); CHECK(step_preview_reply(&preview,reply)); reply_free(reply);
    CHECK(preview.page.cells[0].number==1);
    step_preview_free(&preview); CHECK(!preview.active && !preview.page.cells && !preview.grid.widths && !preview.width_names && !preview.statistics.sum);
    Analysis big={.kind=SOURCE_DEMO,.path=dx_strdup("")}; CHECK(step_preview_start(&preview,&big,0,0,0,NULL));
    job=step_preview_job(&preview,4,false); worker_submit(&worker,job); reply=wait_reply(&worker); CHECK(!reply->error && reply->page.total==1000000 && reply->page.row_count==256); CHECK(step_preview_reply(&preview,reply)); reply_free(reply);
    grid_layout(&preview.grid,1,&preview.page,(Rectangle){0,0,800,500}); preview.grid.top=500; Cell *first_cache=preview.page.cells;
    job=step_preview_job(&preview,5,false); CHECK(job && job->offset==436 && job->known_total==1000000); worker_submit(&worker,job); reply=wait_reply(&worker);
    CHECK(!reply->error && reply->page.offset==436 && reply->page.row_count==256); CHECK(step_preview_reply(&preview,reply)); reply_free(reply);
    CHECK(preview.spare.cells==first_cache && preview.spare.offset==0 && preview.spare.row_count==256);
    preview.grid.dragging=true; CHECK(!step_preview_job(&preview,6,true)); preview.grid.dragging=false;
    job=step_preview_job(&preview,6,true); worker_submit(&worker,job); worker_cancel(&worker); step_preview_free(&preview);
    CHECK(step_preview_start(&preview,&live,1,74,9,NULL)); job=step_preview_job(&preview,7,false); worker_submit(&worker,job); reply=wait_reply(&worker);
    CHECK(reply->id==7 && !reply->error && reply->page.total==2); CHECK(step_preview_reply(&preview,reply)); reply_free(reply);
    preview.loading=true; preview.request=8; QueryReply failure={.kind=JOB_STEP_PREVIEW,.id=8,.error=dx_strdup("Test query failure")};
    CHECK(step_preview_reply(&preview,&failure) && !failure.error && preview.error && preview.page.total==2); CHECK(!step_preview_job(&preview,8,true));
    free(preview.error); preview.error=NULL; preview.loading=true; preview.request=9;
    preview.page.records.source_key=dx_strdup("reader-v1"); preview.page.records.stamp=11;
    preview.spare.records.source_key=dx_strdup("reader-v1"); preview.spare.records.stamp=11;
    Cell *retained_page=preview.page.cells; Page *retained_spare=&preview.spare; char *spare_key=retained_spare->records.source_key;
    QueryReply changed={.kind=JOB_STEP_PREVIEW,.id=9}; changed.page.records.source_key=dx_strdup("reader-v2"); changed.page.records.stamp=12;
    CHECK(step_preview_reply(&preview,&changed) && preview.error && strstr(preview.error,"source changed between pages"));
    CHECK(preview.page.cells==retained_page && preview.page.total==2 && preview.spare.records.source_key==spare_key && changed.page.records.source_key);
    page_free(&changed.page); CHECK(!step_preview_job(&preview,10,true));
    step_preview_free(&preview); analysis_free(&big); analysis_free(&live); page_free(&original); grid_destroy(&grid); database_close(&db); worker_stop(&worker); free(error);
    printf("PASS: %u historical preview checks (geometry, real worker prefixes/counts/paging, ownership, stale/cancelled replies, widths and unchanged live source)\n",checks); return 0;
}
