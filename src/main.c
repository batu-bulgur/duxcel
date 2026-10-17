#include "app/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DUXCEL_BROWSER_HOST
#include <emscripten/emscripten.h>
/* Implemented by the version-pinned web raylib platform adapter. */
void dx_web_begin_frame(void);
EM_JS(void, browser_first_frame, (void), {
    if (!Module.dxFirstFrame) { Module.dxFirstFrame=true; performance.mark('duxcel:first-frame'); }
});
static void browser_frame(void *context) {
    static bool rendered;
    AppSession *session=context;
    dx_web_begin_frame();
    bool running=app_frame(session);
    if (!rendered) { browser_first_frame(); rendered=true; }
    if (!running) { emscripten_cancel_main_loop(); app_finish(session); }
}
#endif
int main(int argc, char **argv) {
    AppOptions options = {0};
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--demo")) options.demo=1;
        else if (!strcmp(argv[i],"--frames") && i+1<argc) options.frames=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--screenshot") && i+1<argc) options.screenshot=argv[++i];
        else if (!strcmp(argv[i],"--smoke")) options.smoke=1;
        else if (!strcmp(argv[i],"--workspace-smoke")) options.workspace_smoke=1;
        else if (!strcmp(argv[i],"--filter-smoke")) options.filter_smoke=1;
        else if (!strcmp(argv[i],"--pivot-smoke")) options.pivot_smoke=1;
        else if (!strcmp(argv[i],"--export-smoke")) options.export_smoke=1;
        else if (!strcmp(argv[i],"--count-smoke")) options.count_smoke=1;
        else if (!strcmp(argv[i],"--clipboard-smoke")) options.clipboard_smoke=1;
        else if (!strcmp(argv[i],"--sort-smoke")) options.sort_smoke=1;
        else if (!strcmp(argv[i],"--layout-smoke")) options.layout_smoke=1;
        else if (!strcmp(argv[i],"--navigation-smoke")) options.navigation_smoke=1;
        else if (!strcmp(argv[i],"--nyc-smoke")) options.nyc_smoke=1;
        else if (!strcmp(argv[i],"--document-smoke")) options.document_smoke=1;
        else if (!strcmp(argv[i],"--chart-smoke")) options.chart_smoke=1;
        else if (!strcmp(argv[i],"--statistics-smoke")) options.statistics_smoke=1;
        else if (!strcmp(argv[i],"--table-smoke")) options.table_smoke=1;
        else if (!strcmp(argv[i],"--icon-smoke")) options.icon_smoke=1;
        else if (!strcmp(argv[i],"--unique-smoke")) options.unique_smoke=1;
        else if (!strcmp(argv[i],"--query-smoke")) options.query_smoke=1;
#ifdef DUXCEL_DEV_REVIEW
        else if (!strcmp(argv[i],"--review-smoke")) options.review_smoke=1;
#endif
        else if (!strcmp(argv[i],"--help")) {
            puts("Duxcel: ./duxcel [file.csv|file.parquet|file.duckdb|book.dxls] [--demo]\nDevelopment: --frames N --screenshot path.png --smoke --workspace-smoke --filter-smoke --pivot-smoke --export-smoke --count-smoke --clipboard-smoke --sort-smoke --layout-smoke --navigation-smoke --nyc-smoke --document-smoke --chart-smoke --statistics-smoke --table-smoke --icon-smoke --query-smoke --unique-smoke");
#ifdef DUXCEL_DEV_REVIEW
            puts("Native review: Cmd/Ctrl+Shift+. or Review > Report UI issue. --review-smoke tests the isolated reporting workflow.");
#endif
            return 0;
        } else if (argv[i][0]=='-') { fprintf(stderr,"Unknown option: %s\n",argv[i]); return 1; }
        else options.path=argv[i];
    }
#ifdef DUXCEL_BROWSER_HOST
    AppSession *session=app_start(&options);
    if (!session) return 1;
    emscripten_set_main_loop_arg(browser_frame,session,0,true);
    return 0;
#else
    return app_run(&options);
#endif
}
