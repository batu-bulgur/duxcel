#include "app/app.h"
#include <stdio.h>
int app_test_source_import_format_result(void);
unsigned app_test_source_import_format_stage(void);
int main(void) {
    AppOptions options={.frames=2600,.path="tests/fixtures/transactions.csv"};
    AppSession *session=app_start(&options); if (!session) return 1;
    while (!app_test_source_import_format_result() && app_frame(session)) {}
    int complete=app_test_source_import_format_result(),result=app_finish(session);
    if (complete!=1) { fprintf(stderr,"FAIL: source import formatting unfinished at stage %u\n",app_test_source_import_format_stage()); return 1; }
    return result;
}
