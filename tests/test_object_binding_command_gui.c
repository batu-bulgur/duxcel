#include "app/app.h"
#include <stdio.h>
int app_test_object_binding_command_result(void);
unsigned app_test_object_binding_command_stage(void);
int main(void) {
    AppOptions options={.frames=2800,.path="build/tests/generated/source-connection-a.duckdb"}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_object_binding_command_result() && app_frame(session)) {}
    int complete=app_test_object_binding_command_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: native object binding workflow unfinished at stage %u\n",app_test_object_binding_command_stage()); return 1; }
    return result;
}
