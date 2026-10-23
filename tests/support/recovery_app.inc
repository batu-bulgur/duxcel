/* Private real-App scenario. Physical key/pointer paths exercise the shared UI;
   setup and observations use the actual workbook/journal/recovery controllers. */
#include <unistd.h>
static unsigned recovery_gui_stage,recovery_gui_wait,recovery_gui_checks;
static int recovery_gui_result;
static bool recovery_gui_modal_painted;
static const char *recovery_gui_checkpoint,*recovery_gui_journal;
static WorkbookId recovery_gui_parent;
static const char *recovery_gui_copy="build/tests/generated/recovery-checkpoint-copy.dxls",*recovery_gui_full="build/tests/generated/recovery-full.dxls";
void app_test_recovery_fixture(const char *checkpoint,const char *journal,WorkbookId generation) { recovery_gui_checkpoint=checkpoint; recovery_gui_journal=journal; recovery_gui_parent=generation; }
int app_test_recovery_result(void) { return recovery_gui_result; }
#define RECOVERY_GUI_CHECK(x) do { recovery_gui_checks++; if (!(x)) { fprintf(stderr,"FAIL recovery GUI stage %u:%d: %s (%s)\n",recovery_gui_stage,__LINE__,#x,a->message ? a->message : ""); recovery_gui_result=-1; return; } } while (0)
static void recovery_gui_click(Rectangle r) { Vector2 p={r.x+r.width/2,r.y+r.height/2}; ui_note_pointer_gesture((DxPointerGesture){p.x,p.y,p.x,p.y,0,false}); }
static void recovery_gui_edit(App *a,const char *text) { a->chrome_focus=0; grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,text); ui_note_key_press(KEY_ENTER,0); }
static bool recovery_gui_idle(App *a) { WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(a->recovery_worker); return !state.active && !state.ready && !a->io_busy && !a->busy; }
static void recovery_process_frame(App *a,const char *role) {
    if (++recovery_gui_wait<8) return; recovery_gui_wait=0;
    JournalSessionState backup=workbook_journal_session_state(a->journal,&a->book);
    if (!strcmp(role,"writer")) {
        if (!recovery_gui_stage) { a->chrome_focus=0; grid_select_cell(&a->grid,0,0,false); open_modal(a,MODAL_CELL); input_set(&a->input,"Crash value"); ui_note_key_press(KEY_ENTER,0); recovery_gui_stage=1; }
        else if (recovery_gui_stage==1 && backup.covered && !backup.queue.opening && !backup.queue.pending) {
            RECOVERY_GUI_CHECK(backup.queue.durable_revision==1 && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Crash value"));
            printf("RECOVERY_WRITER_READY %016" PRIx64 "%016" PRIx64 " %" PRIu64 "\n",a->book.id.high,a->book.id.low,a->book.command_revision); fflush(stdout); recovery_gui_stage=2;
        }
    } else if (!strcmp(role,"observer")) {
        if (!a->recovery_page || a->recovery_startup || !recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->recovery_page->page.scan.done && a->recovery_page->page.scan.active==1 && !a->recovery_page->page.count);
        RECOVERY_GUI_CHECK(a->modal==MODAL_NONE && !a->book.dirty && !a->recovery_owner);
        puts("RECOVERY_ACTIVE_EXCLUDED"); fflush(stdout); recovery_gui_result=1;
    } else if (!strcmp(role,"restore")) {
        if (!recovery_gui_stage) {
            if (!recovery_gui_idle(a) || a->modal!=MODAL_RECOVERY) return;
            RECOVERY_GUI_CHECK(a->recovery_page && a->recovery_page->page.count==1 && a->recovery_page->page.rows[0].kind==RECOVERY_ENTRY_BOUND);
            recovery_gui_click(a->recovery_view.layout.buttons[0]); recovery_gui_stage=1;
        } else {
            if (!recovery_gui_idle(a)) return;
            RECOVERY_GUI_CHECK(a->recovery_owner && !a->book.path && a->book.dirty && a->modal==MODAL_NONE);
            RECOVERY_GUI_CHECK(a->book.command_revision==1 && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Crash value"));
            printf("RECOVERY_RESTORED %016" PRIx64 "%016" PRIx64 " %" PRIu64 "\n",a->book.id.high,a->book.id.low,a->book.command_revision); fflush(stdout); recovery_gui_result=1;
        }
    } else RECOVERY_GUI_CHECK(false);
}
static void recovery_test_frame(AppSession *session) {
    App *a=&session->app; const char *role=getenv("DUXCEL_TEST_RECOVERY_PROCESS_ROLE");
    if (role && !recovery_gui_result) { recovery_process_frame(a,role); return; }
    if (recovery_gui_result || ++recovery_gui_wait<8) return; recovery_gui_wait=0;
    JournalSessionState backup=workbook_journal_session_state(a->journal,&a->book);
    switch (recovery_gui_stage) {
    case 0: {
        if (!recovery_gui_idle(a) || a->modal!=MODAL_RECOVERY) return;
        RECOVERY_GUI_CHECK(a->recovery_page && a->recovery_page->page.count==1 && !a->book.dirty);
        RECOVERY_GUI_CHECK(a->recovery_view.layout.valid); char *error=NULL;
        RECOVERY_GUI_CHECK(workbook_recovery_worker_prepare(a->recovery_worker,a->recovery_page->page.rows[0].generation_name,RECOVERY_REPLAY_TAIL,&a->recovery_request,&error) && !error);
        recovery_dismiss(a); break;
    }
    case 1:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_NONE && !a->recovery_owner && !a->recovery_staged && !a->book.dirty);
        RECOVERY_GUI_CHECK(!access(recovery_gui_checkpoint,F_OK)); recovery_open(a); break;
    case 2:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_RECOVERY && a->recovery_page->page.count==1); recovery_gui_click(a->recovery_view.layout.buttons[1]); break;
    case 3:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->recovery_owner && !a->book.path && a->book.dirty && a->modal==MODAL_NONE);
        RECOVERY_GUI_CHECK(workbook_recovery_info(a->recovery_owner)->tail_deferred && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Baseline"));
        workspace_job(a,JOB_SAVE,recovery_gui_checkpoint); RECOVERY_GUI_CHECK(!a->io_busy && !a->book.path);
        recovery_gui_edit(a,"Copy edit"); break;
    case 4:
        if (backup.queue.opening || backup.queue.pending) return;
        RECOVERY_GUI_CHECK(backup.covered && backup.parent.high==recovery_gui_parent.high && backup.parent.low==recovery_gui_parent.low);
        RECOVERY_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Copy edit")); workspace_job(a,JOB_SAVE,recovery_gui_copy); break;
    case 5:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(!a->book.dirty && !a->recovery_owner && !a->journal && !access(recovery_gui_checkpoint,F_OK) && !access(recovery_gui_journal,F_OK));
        recovery_gui_edit(a,"Current draft"); break;
    case 6:
        if (backup.queue.opening || backup.queue.pending) return;
        RECOVERY_GUI_CHECK(a->book.dirty && !strcmp(cells_get(&active_sheet(a)->cells,1,1),"Current draft")); file_action(a,UI_KEYTIP_FILE_RECOVER); break;
    case 7:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_RECOVERY && a->recovery_page->page.count==1); recovery_gui_click(a->recovery_view.layout.buttons[0]); break;
    case 8:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_UNSAVED && a->recovery_staged && a->book.dirty);
        if (!recovery_gui_modal_painted) { recovery_gui_modal_painted=true; return; }
        recovery_gui_modal_painted=false; recovery_gui_click(a->document_actions[2]); break;
    case 9:
        RECOVERY_GUI_CHECK(a->modal==MODAL_NONE && !a->recovery_staged && !a->recovery_owner && a->book.dirty);
        RECOVERY_GUI_CHECK(!strcmp(cells_get(&active_sheet(a)->cells,1,1),"Current draft") && !access(recovery_gui_checkpoint,F_OK)); recovery_open(a); break;
    case 10:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_RECOVERY && a->recovery_page->page.count==1); recovery_gui_click(a->recovery_view.layout.buttons[0]); break;
    case 11:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_UNSAVED && a->recovery_staged);
        if (!recovery_gui_modal_painted) { recovery_gui_modal_painted=true; return; }
        recovery_gui_modal_painted=false; recovery_gui_click(a->document_actions[1]); break;
    case 12:
        RECOVERY_GUI_CHECK(a->recovery_owner && !a->book.path && a->book.dirty && !a->journal);
        RECOVERY_GUI_CHECK(!workbook_recovery_info(a->recovery_owner)->tail_deferred && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Recovered") && !*cells_get(&active_sheet(a)->cells,1,1));
        workspace_job(a,JOB_SAVE,recovery_gui_full); break;
    case 13:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(!a->book.dirty && !a->recovery_owner && access(recovery_gui_checkpoint,F_OK) && access(recovery_gui_journal,F_OK));
        recovery_open(a); SetWindowSize(640,640); ui_set_scale(&a->ui,1.5f); break;
    case 14: {
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(a->modal==MODAL_RECOVERY && a->recovery_page && !a->recovery_page->page.count);
        UIRecoveryLayout *layout=&a->recovery_view.layout;
        RECOVERY_GUI_CHECK(layout->valid && layout->panel.x>=0 && layout->panel.y>=0 && layout->panel.x+layout->panel.width<=640 && layout->panel.y+layout->panel.height<=640);
        for (int i=0;i<5;i++) RECOVERY_GUI_CHECK(layout->buttons[i].x>=layout->panel.x && layout->buttons[i].x+layout->buttons[i].width<=layout->panel.x+layout->panel.width);
        smoke_snapshot("build/tests/results/recovery/narrow.png"); ui_note_key_press(KEY_ESCAPE,0); break;
    }
    case 15:
        RECOVERY_GUI_CHECK(a->modal==MODAL_NONE && !a->book.dirty && !a->recovery_staged);
        workspace_job(a,JOB_LOAD,recovery_gui_copy); break;
    case 16:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(!a->book.dirty && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Baseline") && !strcmp(cells_get(&active_sheet(a)->cells,1,1),"Copy edit"));
        workspace_job(a,JOB_LOAD,recovery_gui_full); break;
    case 17:
        if (!recovery_gui_idle(a)) return;
        RECOVERY_GUI_CHECK(!a->book.dirty && !strcmp(cells_get(&active_sheet(a)->cells,0,0),"Recovered") && !*cells_get(&active_sheet(a)->cells,1,1));
        recovery_gui_result=1; printf("PASS: native recovery choice workflow (18 stages, %u checks)\n",recovery_gui_checks); break;
    }
    recovery_gui_stage++;
}
#undef RECOVERY_GUI_CHECK
