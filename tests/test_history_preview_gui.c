#include "app/app.h"
#include <stdio.h>
int app_test_history_preview_result(void);
void app_test_history_preview_cleanup(void);
void app_test_history_preview_finished(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=1600}); if (!session) return 1;
    while (!app_test_history_preview_result() && app_frame(session)) {}
    int complete=app_test_history_preview_result(); app_test_history_preview_cleanup();
    int result=app_finish(session); app_test_history_preview_finished();
    if (complete!=1) { fputs("FAIL: native historical preview workflow unfinished\n",stderr); return 1; } return result;
}
