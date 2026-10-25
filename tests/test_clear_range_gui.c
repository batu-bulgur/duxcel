#include "app/app.h"
#include <stdio.h>
int app_test_clear_range_result(void);
unsigned app_test_clear_range_stage(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=1800}); if (!session) return 1;
    while (!app_test_clear_range_result() && app_frame(session)) {}
    int complete=app_test_clear_range_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: native manual clear unfinished at stage %u\n",app_test_clear_range_stage()); return 1; }
    return result;
}
