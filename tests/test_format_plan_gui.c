#include "app/app.h"
#include <stdio.h>
int app_test_format_plan_result(void);
int main(void) {
    AppOptions options={.frames=1800}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_format_plan_result() && app_frame(session)) {}
    int complete=app_test_format_plan_result(),result=app_finish(session);
    if (complete!=1) { fputs("FAIL: native cancellable format workflow unfinished\n",stderr); return 1; }
    return result;
}
