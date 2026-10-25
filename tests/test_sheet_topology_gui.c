#include "app/app.h"
#include <stdio.h>
int app_test_sheet_topology_result(void);
int main(void) {
    AppOptions options={.path="tests/fixtures/null-text.csv",.frames=2400}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_sheet_topology_result() && app_frame(session)) {}
    int complete=app_test_sheet_topology_result(),result=app_finish(session);
    if (complete!=1) { fputs("FAIL: native sheet topology workflow unfinished\n",stderr); return 1; }
    return result;
}
