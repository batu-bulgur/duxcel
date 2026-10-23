#include "app/app.h"
#include <stdio.h>
int app_test_workbook_lifecycle_result(void);
int main(void) {
    AppOptions options={.frames=1000}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_workbook_lifecycle_result() && app_frame(session)) {}
    int complete=app_test_workbook_lifecycle_result(),result=app_finish(session);
    if (complete!=1) { fputs("FAIL: native workbook lifecycle workflow unfinished\n",stderr); return 1; }
    return result;
}
