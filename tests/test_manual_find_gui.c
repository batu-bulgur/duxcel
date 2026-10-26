#include "app/app.h"
#include <stdio.h>
int app_test_manual_find_result(void);
unsigned app_test_manual_find_stage(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=2400}); if (!session) return 1;
    while (!app_test_manual_find_result() && app_frame(session)) {}
    int complete=app_test_manual_find_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: manual Find unfinished at stage %u\n",app_test_manual_find_stage()); return 1; }
    return result;
}
