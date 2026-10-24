#include "app/app.h"
#include <stdio.h>
int app_test_version_controls_result(void);
void app_test_version_controls_cleanup(void);
void app_test_version_controls_finished(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=2400}); if (!session) return 1;
    while (!app_test_version_controls_result() && app_frame(session)) {}
    int complete=app_test_version_controls_result(); app_test_version_controls_cleanup(); int result=app_finish(session); app_test_version_controls_finished();
    if (complete!=1) { fputs("FAIL: native version controls workflow unfinished\n",stderr); return 1; } return result;
}
