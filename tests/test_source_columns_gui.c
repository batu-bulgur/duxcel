#include "app/app.h"
#include <stdio.h>
int app_test_source_results_result(void);
unsigned app_test_source_results_stage(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=3600}); if (!session) return 1;
    while (!app_test_source_results_result() && app_frame(session)) {}
    int complete=app_test_source_results_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: native source columns unfinished at stage %u\n",app_test_source_results_stage()); return 1; }
    return result;
}
