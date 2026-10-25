#include "app/app.h"
#include <stdio.h>
int app_test_source_details_result(void);
unsigned app_test_source_details_stage(void);
int app_test_source_details_cleanup(void);
int main(void) {
    AppSession *session=app_start(&(AppOptions){.frames=3000}); if (!session) return 1;
    while (!app_test_source_details_result() && app_frame(session)) {}
    int complete=app_test_source_details_result(),restored=app_test_source_details_cleanup(); int result=app_finish(session);
    if (complete!=1 || !restored) { fprintf(stderr,"FAIL: source provenance unfinished at stage %u\n",app_test_source_details_stage()); return 1; }
    return result;
}
