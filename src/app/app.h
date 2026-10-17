#ifndef DUXCEL_APP_H
#define DUXCEL_APP_H
#include <stdbool.h>
typedef struct { const char *path, *screenshot; int frames, smoke, workspace_smoke, filter_smoke, pivot_smoke, export_smoke, count_smoke, clipboard_smoke, sort_smoke,layout_smoke,navigation_smoke,nyc_smoke,document_smoke,chart_smoke,statistics_smoke,table_smoke,icon_smoke,query_smoke,unique_smoke,demo;
#ifdef DUXCEL_DEV_REVIEW
    int review_smoke;
#endif
} AppOptions;
/* One graphics-thread session at a time. Start copies all option strings.
   Each frame returns to the caller; false remains false until finish.
   Finish joins owned workers and releases the session, including on an early
   host shutdown. Neither options nor a caller stack frame must stay alive.
   Embedded native scenario runs remain one per process (their test injectors
   retain function-static state); ordinary frame sessions can restart. */
typedef struct AppSession AppSession;
AppSession *app_start(const AppOptions *options);
bool app_frame(AppSession *session);
int app_finish(AppSession *session);
int app_run(const AppOptions *options);
#endif
