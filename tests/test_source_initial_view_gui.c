#include "app/app.h"
#include <stdio.h>
int app_test_source_initial_view_result(void);
unsigned app_test_source_initial_view_stage(void);
int main(void) {
    AppOptions options={.frames=2400,.path="build/tests/generated/source-open-wide.parquet"};
    AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_source_initial_view_result() && app_frame(session)) {}
    int complete=app_test_source_initial_view_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: initial source view unfinished at stage %u\n",app_test_source_initial_view_stage()); return 1; }
    return result;
}
