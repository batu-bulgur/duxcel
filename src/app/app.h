#ifndef DUXCEL_APP_H
#define DUXCEL_APP_H
typedef struct { const char *path, *screenshot; int frames, smoke, workspace_smoke, filter_smoke, pivot_smoke, export_smoke, count_smoke, clipboard_smoke, sort_smoke,layout_smoke,navigation_smoke,nyc_smoke,document_smoke,chart_smoke,statistics_smoke,table_smoke,icon_smoke,query_smoke,unique_smoke,demo; } AppOptions;
int app_run(const AppOptions *options);
#endif
