#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/app.h"
#include "workbook/journal_session.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void app_test_recovery_fixture(const char *checkpoint,const char *journal,WorkbookId generation);
int app_test_recovery_result(void);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL recovery GUI setup:%d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(void) {
    char root[]="build/tests/recovery-ui-XXXXXX"; CHECK(mkdtemp(root));
    CHECK(!setenv("DUXCEL_TEST_RECOVERY_ROOT",root,1) && !setenv("DUXCEL_TEST_RECOVERY_STARTUP","1",1));
    Workbook b; workbook_init(&b); char *error=NULL;
    CHECK(workbook_add(&b,"Recovery",&(Analysis){0},&error) && !error);
    CHECK(cells_set(&b.sheets[0].cells,0,0,"Baseline",&error)); worksheet_record(b.sheets,EDIT_CELLS);
    WorkbookJournalSession *journal=workbook_journal_session_new(root,(JournalQueueOptions){.entries=256,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=256},&error); CHECK(journal && !error);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,0,0,&target,&error));
    CHECK(workbook_journal_session_apply(journal,&b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Recovered",9},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error);
    JournalSessionState state; double deadline=dx_millis()+10000;
    do { state=workbook_journal_session_state(journal,&b); CHECK(!state.queue.failed && dx_millis()<deadline); dx_sleep_ms(1); } while (state.queue.opening || state.queue.pending);
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(journal)),*tail=dx_strdup(workbook_journal_session_journal(journal));
    CHECK(workbook_journal_session_close(journal,false,&error) && !error); workbook_free(&b);
    app_test_recovery_fixture(checkpoint,tail,state.generation);
    AppSession *session=app_start(&(AppOptions){.frames=1800}); CHECK(session);
    while (!app_test_recovery_result() && app_frame(session)) {}
    int complete=app_test_recovery_result(),result=app_finish(session);
    free(checkpoint); free(tail); CHECK(!rmdir(root));
    if (complete!=1) { fputs("FAIL: native recovery choice workflow unfinished\n",stderr); return 1; }
    return result;
}
