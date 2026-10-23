/* Private native checkpoint/queue/input fixture; never linked into production. */
#include <unistd.h>
static unsigned journal_gui_stage,journal_gui_wait,journal_gui_checks;
static int journal_gui_result;
static DxMutex journal_gate_mutex;
static bool journal_gate_ready,journal_gate_release,journal_gate_entered,journal_gui_narrow;
static unsigned journal_gate_calls,journal_gate_cancel;
static char *journal_gui_checkpoint,*journal_gui_path;
static WorkbookId journal_gui_document;
static uint64_t journal_gui_sheet;
#define JOURNAL_GUI_CHECK(x) do { journal_gui_checks++; if (!(x)) { fprintf(stderr,"FAIL: journal GUI stage %u line %d: %s (%s)\n",journal_gui_stage,__LINE__,#x,a->message ? a->message : ""); journal_gui_result=-1; return; } } while (0)
static bool journal_gui_before(void *unused) {
    (void)unused; dx_lock(&journal_gate_mutex); unsigned call=++journal_gate_calls; journal_gate_entered=true; dx_unlock(&journal_gate_mutex);
    for (;;) { dx_lock(&journal_gate_mutex); bool release=journal_gate_release; dx_unlock(&journal_gate_mutex); if (release) break; dx_sleep_ms(1); }
    dx_lock(&journal_gate_mutex); bool accept=call!=journal_gate_cancel; dx_unlock(&journal_gate_mutex); return accept;
}
static JournalControl journal_session_test_observer(void) { return (JournalControl){journal_gui_before,NULL,NULL}; }
static size_t journal_session_test_capacity(void) { return 2; }
static void journal_gui_edit(App *a,const char *value) {
    a->chrome_focus=0; grid_select_cell(&a->grid,1,1,false); open_modal(a,MODAL_CELL); input_set(&a->input,value); ui_note_key_press(KEY_ENTER,0);
}
static void journal_gui_click(Rectangle r) {
    float x=r.x+r.width*.5f,y=r.y+r.height*.5f;
    ui_note_pointer_gesture((DxPointerGesture){x,y,x,y,0,false});
}
static void journal_session_test_frame(AppSession *session) {
    App *a=&session->app; Worksheet *s=active_sheet(a);
    if (journal_gui_result || a->busy || a->io_busy || !s || ++journal_gui_wait<12) return;
    journal_gui_wait=0; JournalSessionState backup=workbook_journal_session_state(a->journal,&a->book);
    switch (journal_gui_stage) {
    case 0:
        dx_mutex_init(&journal_gate_mutex); journal_gate_ready=true; journal_gate_cancel=1;
        journal_gui_document=a->book.id; journal_gui_edit(a,"First"); break;
    case 1: {
        dx_lock(&journal_gate_mutex); bool entered=journal_gate_entered; dx_unlock(&journal_gate_mutex); if (!entered) return;
        JOURNAL_GUI_CHECK(backup.active && backup.covered && !backup.queue.opening && backup.queue.pending==1 && !backup.queue.durable_revision);
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"First") && a->book.command_revision==1 && a->book.path==NULL);
        smoke_snapshot("build/tests/results/journal-session/pending.png"); journal_gui_edit(a,"Second"); break;
    }
    case 2:
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Second") && backup.queue.pending==2 && backup.queue.accepted_revision==2 && !backup.queue.durable_revision);
        journal_gui_edit(a,"Third"); break;
    case 3:
        JOURNAL_GUI_CHECK(a->modal==MODAL_CELL && a->cell_edit_error && !strcmp(a->input.text,"Third"));
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Second") && a->book.command_revision==2);
        smoke_snapshot("build/tests/results/journal-session/draft-retained.png");
        dx_lock(&journal_gate_mutex); journal_gate_release=true; dx_unlock(&journal_gate_mutex); ui_note_key_press(KEY_ESCAPE,0); break;
    case 4:
        if (!backup.queue.failed) return;
        JOURNAL_GUI_CHECK(a->modal==MODAL_NONE && backup.queue.pending==2 && !backup.queue.durable_revision);
        a->chrome_focus=10; a->status_focus=5; ui_note_key_press(KEY_SPACE,0); break;
    case 5:
        if (backup.queue.opening || backup.queue.pending) return;
        JOURNAL_GUI_CHECK(!backup.queue.failed && backup.queue.durable_revision==2 && backup.covered);
        dx_lock(&journal_gate_mutex); journal_gate_cancel=journal_gate_calls+1; dx_unlock(&journal_gate_mutex);
        journal_gui_edit(a,"Third"); break;
    case 6:
        if (!backup.queue.failed) return;
        if (!journal_gui_narrow) { ui_set_scale(&a->ui,1.5f); SetWindowSize(640,640); journal_gui_narrow=true; return; }
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Third") && backup.queue.accepted_revision==3 && backup.queue.durable_revision==2 && backup.queue.pending==1);
        smoke_snapshot("build/tests/results/journal-session/retry.png"); journal_gui_click(a->status_layout.context[0]); break;
    case 7:
        if (backup.queue.opening || backup.queue.pending) return;
        JOURNAL_GUI_CHECK(!backup.queue.failed && backup.queue.durable_revision==3 && backup.covered);
        smoke_snapshot("build/tests/results/journal-session/backed-up.png"); ui_set_scale(&a->ui,1); SetWindowSize(1280,820); ui_note_key_press(KEY_Z,DX_MOD_SUPER); break;
    case 8:
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"Second") && !backup.covered && a->book.command_revision==4);
        new_sheet(a); journal_gui_sheet=active_sheet(a)->id; journal_gui_edit(a,"B1"); break;
    case 9: {
        if (backup.queue.opening || backup.queue.pending) return;
        JOURNAL_GUI_CHECK(s->id==journal_gui_sheet && !strcmp(cells_get(&s->cells,1,1),"B1") && backup.covered && backup.queue.durable_revision==6);
        journal_gui_checkpoint=dx_strdup(workbook_journal_session_checkpoint(a->journal)); journal_gui_path=dx_strdup(workbook_journal_session_journal(a->journal));
        workspace_job(a,JOB_SAVE,journal_gui_checkpoint); JOURNAL_GUI_CHECK(!a->io_busy && a->book.path==NULL);
        workspace_job(a,JOB_SAVE,"build/tests/generated/journal-session-demo.dxls");
        /* Inject a later admitted C command after the real Save snapshot. The
           ordinary UI remains IO-guarded; this checks publication fencing. */
        CellEditTarget target; char *error=NULL;
        JOURNAL_GUI_CHECK(workbook_prepare_cell_edit(&a->book,a->book.active,1,1,&target,&error));
        JOURNAL_GUI_CHECK(workbook_journal_session_apply(a->journal,&a->book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"B2",2},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error); break;
    }
    case 10: {
        JOURNAL_GUI_CHECK(a->book.dirty && a->journal && a->book.command_revision==7 && !strcmp(cells_get(&s->cells,1,1),"B2"));
        Workbook disk; workbook_init(&disk); char *error=NULL; bool loaded=workbook_load(&disk,"build/tests/generated/journal-session-demo.dxls",&error);
        bool prior=loaded && disk.command_revision==6 && !strcmp(cells_get(&disk.sheets[disk.active].cells,1,1),"B1"); workbook_free(&disk); free(error); JOURNAL_GUI_CHECK(prior);
        break;
    }
    case 11:
        if (backup.queue.opening || backup.queue.pending) return;
        JOURNAL_GUI_CHECK(backup.queue.durable_revision==7); workspace_job(a,JOB_SAVE,"build/tests/generated/journal-session-demo.dxls"); break;
    case 12:
        JOURNAL_GUI_CHECK(!a->book.dirty && !a->journal && access(journal_gui_checkpoint,F_OK) && access(journal_gui_path,F_OK)); workspace_job(a,JOB_LOAD,"build/tests/generated/journal-session-demo.dxls"); break;
    case 13:
        JOURNAL_GUI_CHECK(!a->book.dirty && !a->journal && a->book.command_revision==7 && a->book.count==2);
        JOURNAL_GUI_CHECK(a->book.id.high==journal_gui_document.high && a->book.id.low==journal_gui_document.low && s->id==journal_gui_sheet);
        JOURNAL_GUI_CHECK(!strcmp(cells_get(&s->cells,1,1),"B2") && !strcmp(cells_get(&a->book.sheets[0].cells,1,1),"Second"));
        JOURNAL_GUI_CHECK(access(journal_gui_checkpoint,F_OK) && access(journal_gui_path,F_OK)); free(journal_gui_checkpoint); free(journal_gui_path);
        journal_gui_checkpoint=journal_gui_path=NULL; journal_gui_edit(a,"B3"); break;
    case 14:
        if (backup.queue.opening || backup.queue.pending) return;
        JOURNAL_GUI_CHECK(backup.covered && backup.queue.durable_revision==8 && !strcmp(cells_get(&s->cells,1,1),"B3"));
        journal_gui_checkpoint=dx_strdup(workbook_journal_session_checkpoint(a->journal)); journal_gui_path=dx_strdup(workbook_journal_session_journal(a->journal));
        request_document_action(a,DOCUMENT_NEW,NULL); break;
    case 15:
        JOURNAL_GUI_CHECK(a->modal==MODAL_UNSAVED && a->book.dirty); journal_gui_click(a->document_actions[1]); break;
    case 16:
        JOURNAL_GUI_CHECK(a->modal==MODAL_NONE && !a->book.dirty && a->book.count==1 && !a->journal);
        JOURNAL_GUI_CHECK(a->book.id.high!=journal_gui_document.high || a->book.id.low!=journal_gui_document.low);
        JOURNAL_GUI_CHECK(access(journal_gui_checkpoint,F_OK) && access(journal_gui_path,F_OK)); break;
    case 17: {
        Workbook disk; workbook_init(&disk); char *error=NULL; bool loaded=workbook_load(&disk,"build/tests/generated/journal-session-demo.dxls",&error);
        bool saved=loaded && disk.command_revision==7 && !strcmp(cells_get(&disk.sheets[disk.active].cells,1,1),"B2"); workbook_free(&disk); free(error); JOURNAL_GUI_CHECK(saved);
        free(journal_gui_checkpoint); free(journal_gui_path); journal_gui_checkpoint=journal_gui_path=NULL;
        smoke_snapshot("build/tests/results/journal-session/final.png"); journal_gui_result=1;
        printf("PASS: native checkpoint journal workflow (18 stages, %u checks)\n",journal_gui_checks); break;
    }
    }
    journal_gui_stage++;
}
int app_test_journal_session_result(void) { return journal_gui_result; }
void app_test_journal_session_cleanup(void) {
    if (journal_gate_ready) { dx_lock(&journal_gate_mutex); journal_gate_release=true; dx_unlock(&journal_gate_mutex); }
}
void app_test_journal_session_finished(void) {
    if (journal_gate_ready) dx_mutex_destroy(&journal_gate_mutex);
    free(journal_gui_checkpoint); free(journal_gui_path);
}
#undef JOURNAL_GUI_CHECK
