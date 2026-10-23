#include "app/app.h"
#include <stdio.h>
int app_test_journal_session_result(void);
void app_test_journal_session_cleanup(void);
void app_test_journal_session_finished(void);
int main(void) {
    AppOptions options={.frames=1200}; AppSession *session=app_start(&options);
    if (!session) return 1;
    while (!app_test_journal_session_result() && app_frame(session)) {}
    int complete=app_test_journal_session_result(); app_test_journal_session_cleanup();
    int result=app_finish(session); app_test_journal_session_finished();
    if (complete!=1) { fputs("FAIL: native checkpoint journal workflow unfinished\n",stderr); return 1; }
    return result;
}
