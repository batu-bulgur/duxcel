#include "app/app.h"
#include <stdio.h>
int app_test_range_command_result(void);
unsigned app_test_range_command_stage(void);
int main(void) {
    AppOptions options={.frames=2400}; AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_range_command_result() && app_frame(session)) {}
    int complete=app_test_range_command_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: native manual range workflow unfinished at stage %u\n",app_test_range_command_stage()); return 1; } return result;
}
