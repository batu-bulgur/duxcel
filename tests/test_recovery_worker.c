#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/recovery_worker.h"
#include "workbook/journal_session.h"
#include "platform/platform.h"
#include "platform/file_lease.h"
#include "data/source_digest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL recovery worker:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static DxMutex gate_mutex; static DxCond gate_condition;
static unsigned gate_stage; static bool gate_enabled,gate_entered,gate_released;
void dx_test_recovery_worker_checkpoint(WorkbookRecoveryWorker *worker,unsigned stage,WorkbookRecoveryProgress progress) {
    (void)worker; dx_lock(&gate_mutex);
    bool match=stage==gate_stage && (stage!=2 || (progress.phase==RECOVERY_REPLAY && !progress.completed));
    if (gate_enabled && match && !gate_entered) { gate_entered=true; while (!gate_released) dx_cond_wait(&gate_condition,&gate_mutex); }
    dx_unlock(&gate_mutex);
}
static void gate(unsigned stage) { dx_lock(&gate_mutex); gate_stage=stage; gate_enabled=true; gate_entered=gate_released=false; dx_unlock(&gate_mutex); }
static void release(void) { dx_lock(&gate_mutex); gate_enabled=false; gate_released=true; dx_signal(&gate_condition); dx_unlock(&gate_mutex); }
static void wait_gate(void) {
    double deadline=dx_millis()+10000;
    for (;;) { dx_lock(&gate_mutex); bool value=gate_entered; dx_unlock(&gate_mutex); if (value) return; if (dx_millis()>=deadline) { fputs("Recovery barrier did not arrive\n",stderr); exit(1); } dx_sleep_ms(1); }
}
static WorkbookRecoveryWorkerState ready(WorkbookRecoveryWorker *w) {
    double deadline=dx_millis()+10000;
    for (;;) { WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(w); if (state.ready) return state; if (dx_millis()>=deadline) { fputs("Recovery result did not arrive\n",stderr); exit(1); } dx_sleep_ms(1); }
}
static WorkbookRecoveryReply *poll(WorkbookRecoveryWorker *w,uint64_t id) { ready(w); WorkbookRecoveryReply *r=workbook_recovery_worker_poll(w); CHECK(r && r->id==id); return r; }
static void claim(const char *directory,bool busy) {
    char *path=dx_printf("%s/lease.lock",directory),*error=NULL; DxFileLease *lease=NULL;
    CHECK(dx_file_lease_acquire(path,false,&lease,&error)==(busy?DX_FILE_LEASE_BUSY:DX_FILE_LEASE_ACQUIRED) && !error);
    dx_file_lease_release(lease); free(path);
}
static void missing(const char *root) {
    char *error=NULL; WorkbookRecoveryWorker *w=workbook_recovery_worker_new(root,&error); CHECK(w && !error && access(root,F_OK));
    gate(0); uint64_t id=0; CHECK(workbook_recovery_worker_page(w,true,&id,&error)); wait_gate();
    WorkbookRecoveryWorkerState state=workbook_recovery_worker_state(w); CHECK(state.active && state.row_reservation_bytes==DX_RECOVERY_PAGE_BYTES && !state.format_reservation_bytes);
    uint64_t unchanged=99; CHECK(!workbook_recovery_worker_page(w,true,&unchanged,&error) && error && unchanged==99); free(error); error=NULL;
    workbook_recovery_worker_cancel(w); CHECK(workbook_recovery_worker_state(w).cancel_requested); release();
    WorkbookRecoveryReply *r=poll(w,id); CHECK(r->scan_result==RECOVERY_SCAN_CANCELLED && !r->page.count && !r->candidate && !*r->error && access(root,F_OK)); workbook_recovery_reply_free(r);
    CHECK(workbook_recovery_worker_page(w,false,&id,&error)); r=poll(w,id); CHECK(r->page.restarted && r->scan_result==RECOVERY_SCAN_DONE && !r->page.count && !*r->error && access(root,F_OK)); workbook_recovery_reply_free(r);
    CHECK(!workbook_recovery_worker_state(w).row_reservation_bytes); workbook_recovery_worker_free(w);
    w=workbook_recovery_worker_new(root,&error); CHECK(w && workbook_recovery_worker_page(w,true,&id,&error)); workbook_recovery_worker_free(w); CHECK(access(root,F_OK));
}
static char *fixture(const char *root,char **checkpoint,char **journal,char **name) {
    Workbook b; workbook_init(&b); char *error=NULL; CHECK(workbook_add(&b,"A",&(Analysis){0},&error));
    b.path=dx_alloc(65537); b.path[0]='/'; memset(b.path+1,'a',65535); b.path[65536]=0;
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; CHECK(format_apply(&b.sheets[0].formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[0],EDIT_FORMAT);
    WorkbookJournalSession *s=workbook_journal_session_new(root,(JournalQueueOptions){.entries=256,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=256},&error); CHECK(s && !error);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,0,0,&target,&error)); CellEditResult applied=workbook_journal_session_apply(s,&b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Recovered",9},(CellEditPolicy){0,0,true},&error); if (applied.status!=CELL_EDIT_APPLIED) fprintf(stderr,"Fixture admission: %s\n",error ? error : "unknown"); CHECK(applied.status==CELL_EDIT_APPLIED);
    double deadline=dx_millis()+10000; JournalSessionState state;
    do { state=workbook_journal_session_state(s,&b); if (state.queue.failed || dx_millis()>=deadline) { fprintf(stderr,"Backup failed: %s\n",state.queue.error); exit(1); } if (state.queue.opening || state.queue.pending) dx_sleep_ms(1); } while (state.queue.opening || state.queue.pending);
    *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)); *journal=dx_strdup(workbook_journal_session_journal(s)); char *directory=dx_strdup(*checkpoint); *strrchr(directory,'/')=0; *name=dx_strdup(dx_basename(directory));
    CHECK(workbook_journal_session_close(s,false,&error) && !error); workbook_free(&b); return directory;
}
static void *stop_worker(void *context) { workbook_recovery_worker_free(context); return NULL; }
static SourceDigest digest(const char *path) { SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d; }
int main(void) {
    dx_mutex_init(&gate_mutex); dx_cond_init(&gate_condition);
    char parent[]="build/tests/recovery-worker-XXXXXX"; CHECK(mkdtemp(parent)); char *absent=dx_printf("%s/absent",parent); missing(absent); free(absent);
    char *root=dx_printf("%s/catalog",parent); CHECK(!mkdir(root,0700));
    for (unsigned i=0;i<40;i++) { char *path=dx_printf("%s/session-%032u",root,i); CHECK(!mkdir(path,0700)); free(path); }
    char *checkpoint=NULL,*journal=NULL,*name=NULL,*directory=fixture(root,&checkpoint,&journal,&name); SourceDigest before=digest(checkpoint),tail=digest(journal);
    char *error=NULL,*owned_root=dx_strdup(root); WorkbookRecoveryWorker *w=workbook_recovery_worker_new(owned_root,&error); memset(owned_root,'x',strlen(owned_root)); free(owned_root); CHECK(w && !error); uint64_t id;
    gate(1); CHECK(workbook_recovery_worker_page(w,true,&id,&error)); wait_gate();
    CHECK(workbook_recovery_worker_state(w).row_reservation_bytes==DX_RECOVERY_PAGE_BYTES); workbook_recovery_worker_cancel(w); release();
    WorkbookRecoveryReply *cancelled_page=poll(w,id); CHECK(cancelled_page->scan_result==RECOVERY_SCAN_CANCELLED && !cancelled_page->page.count); workbook_recovery_reply_free(cancelled_page);
    CHECK(workbook_recovery_worker_page(w,false,&id,&error)); WorkbookRecoveryWorkerState state=ready(w); CHECK(!state.active && state.ready && state.row_reservation_bytes<=DX_RECOVERY_PAGE_BYTES);
    uint64_t rejected=123; CHECK(!workbook_recovery_worker_page(w,false,&rejected,&error) && error && rejected==123); free(error); error=NULL;
    WorkbookRecoveryReply *first=poll(w,id); CHECK(first->scan_result==RECOVERY_SCAN_MORE && first->page.count==32 && first->page.restarted && first->page.owned_bytes==state.row_reservation_bytes);
    CHECK(workbook_recovery_worker_page(w,false,&id,&error)); WorkbookRecoveryReply *second=poll(w,id);
    CHECK(second->scan_result==RECOVERY_SCAN_DONE && second->page.count==9 && !second->page.restarted && second->page.scan.offered==41 && second->page.scan.legacy==40);
    unsigned bound=0; for (unsigned page=0;page<2;page++) { WorkbookRecoveryReply *r=page?second:first;
        for (size_t i=0;i<r->page.count;i++) if (r->page.rows[i].kind==RECOVERY_ENTRY_BOUND) { bound++; CHECK(strlen(r->page.rows[i].original_path)==65536 && !strcmp(r->page.rows[i].generation_name,name)); }
    }
    CHECK(bound==1 && !workbook_recovery_worker_state(w).row_reservation_bytes);
    gate(2); CHECK(workbook_recovery_worker_prepare(w,name,RECOVERY_REPLAY_TAIL,&id,&error)); wait_gate(); state=workbook_recovery_worker_state(w);
    CHECK(state.active && state.progress.phase==RECOVERY_REPLAY && state.format_reservation_bytes==DX_WORKBOOK_FORMAT_CURRENT_BYTES); claim(directory,true);
    workbook_recovery_worker_cancel(w); release(); WorkbookRecoveryReply *r=poll(w,id); CHECK(r->prepare_result==RECOVERY_CANCELLED && !r->candidate); workbook_recovery_reply_free(r); claim(directory,false);
    gate(1); CHECK(workbook_recovery_worker_prepare(w,name,RECOVERY_REPLAY_TAIL,&id,&error)); wait_gate(); claim(directory,true); workbook_recovery_worker_cancel(w); release();
    r=poll(w,id); CHECK(r->prepare_result==RECOVERY_CANCELLED && !r->candidate && !workbook_recovery_worker_state(w).format_reservation_bytes); workbook_recovery_reply_free(r); claim(directory,false);
    CHECK(workbook_recovery_worker_prepare(w,name,RECOVERY_REPLAY_TAIL,&id,&error)); state=ready(w); CHECK(state.format_reservation_bytes>0); claim(directory,true);
    CHECK(!workbook_recovery_worker_page(w,true,&rejected,&error) && error); free(error); error=NULL;
    r=poll(w,id); CHECK(r->prepare_result==RECOVERY_PREPARED && r->candidate && !*r->error && !workbook_recovery_worker_state(w).format_reservation_bytes);
    const Workbook *book=workbook_recovery_book(r->candidate); CHECK(book && book->dirty && !book->path && !strcmp(cells_get(&book->sheets[0].cells,0,0),"Recovered"));
    WorkbookFormatUsage usage; CHECK(workbook_format_usage(book,NULL,0,&usage)==FORMAT_BUDGET_OK && usage.current==state.format_reservation_bytes);
    WorkbookRecoveryCandidate *candidate=workbook_recovery_reply_take_candidate(r); CHECK(candidate && !r->candidate); workbook_recovery_reply_free(r);
    workbook_recovery_worker_free(w); claim(directory,true);
    bool retained_text=false; for (unsigned page=0;page<2;page++) { WorkbookRecoveryReply *owned=page?second:first;
        for (size_t i=0;i<owned->page.count;i++) if (owned->page.rows[i].kind==RECOVERY_ENTRY_BOUND) { CHECK(strlen(owned->page.rows[i].original_path)==65536); retained_text=true; }
    }
    CHECK(retained_text); workbook_recovery_reply_free(first); workbook_recovery_reply_free(second); workbook_recovery_free(candidate); claim(directory,false);
    w=workbook_recovery_worker_new(root,&error); CHECK(w && workbook_recovery_worker_prepare(w,name,RECOVERY_OPEN_CHECKPOINT,&id,&error)); ready(w); claim(directory,true); workbook_recovery_worker_free(w); claim(directory,false);
    w=workbook_recovery_worker_new(root,&error); gate(2); CHECK(w && workbook_recovery_worker_prepare(w,name,RECOVERY_REPLAY_TAIL,&id,&error)); wait_gate(); claim(directory,true);
    DxThread stopper; CHECK(dx_thread_start(&stopper,stop_worker,w)); release(); dx_thread_join(stopper); claim(directory,false);
    SourceDigest after=digest(checkpoint),after_tail=digest(journal); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,32) && tail.bytes==after_tail.bytes && !memcmp(tail.sha256,after_tail.sha256,32));
    CHECK(!unlink(checkpoint) && !unlink(journal)); char *lease=dx_printf("%s/lease.lock",directory); CHECK(!unlink(lease) && !rmdir(directory)); free(lease);
    for (unsigned i=0;i<40;i++) { char *path=dx_printf("%s/session-%032u",root,i); CHECK(!rmdir(path)); free(path); }
    CHECK(!rmdir(root) && !rmdir(parent)); free(root); free(checkpoint); free(journal); free(name); free(directory);
    dx_cond_destroy(&gate_condition); dx_mutex_destroy(&gate_mutex); CHECK(!error);
    printf("PASS: %u background recovery ownership checks\n",checks); return 0;
}
