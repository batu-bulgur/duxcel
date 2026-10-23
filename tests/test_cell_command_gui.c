#include "app/app.h"
#include <stdio.h>
/* Only the feature App object exports this fixture boundary. */
int app_test_cell_command_result(void);
int main(void) {
    AppOptions options={.frames=600}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_cell_command_result() && app_frame(session)) {}
    int complete=app_test_cell_command_result();
    int result=app_finish(session);
    if (complete!=1) { fputs("FAIL: native cell command workflow unfinished\n",stderr); return 1; }
    return result;
}
