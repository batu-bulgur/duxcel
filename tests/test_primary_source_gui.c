#include "app/app.h"
#include <stdio.h>
int app_test_primary_source_result(void);
unsigned app_test_primary_source_stage(void);
int main(void) {
    AppOptions options={.frames=2200,.path="build/tests/generated/source-connection-a.duckdb"};
    AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_primary_source_result() && app_frame(session)) {}
    int complete=app_test_primary_source_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: pending primary unfinished at stage %u\n",app_test_primary_source_stage()); return 1; }
    return result;
}
