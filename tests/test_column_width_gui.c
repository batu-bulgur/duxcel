#include "app/app.h"
#include <stdio.h>
int app_test_column_width_result(void);
unsigned app_test_column_width_stage(void);
int main(void) {
    AppOptions options={.frames=3000}; AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_column_width_result() && app_frame(session)) {}
    int complete=app_test_column_width_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: native column width workflow unfinished at stage %u\n",app_test_column_width_stage()); return 1; }
    return result;
}
