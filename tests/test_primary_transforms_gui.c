#include "app/app.h"
#include <stdio.h>
int app_test_primary_transforms_result(void);
int main(void) {
    AppOptions options={.path="tests/fixtures/duplicates.csv",.frames=1800}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_primary_transforms_result() && app_frame(session)) {}
    int complete=app_test_primary_transforms_result(),result=app_finish(session);
    if (complete!=1) { fputs("FAIL: native primary sort/filter workflow unfinished\n",stderr); return 1; }
    return result;
}
