#include "app/app.h"
#include <stdio.h>
int app_test_workbook_save_result(void);
void app_test_workbook_save_cleanup(void);
void app_test_workbook_save_finished(void);
int main(void) {
    AppOptions options={.frames=1600}; AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_workbook_save_result() && app_frame(session)) {}
    int complete=app_test_workbook_save_result(); app_test_workbook_save_cleanup(); int result=app_finish(session); app_test_workbook_save_finished();
    if (complete!=1) { fputs("FAIL: native controlled Save workflow unfinished\n",stderr); return 1; } return result;
}
