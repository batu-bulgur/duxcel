#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "support/current_checkpoint.h"
#include "workbook/recovery.h"
#include "workbook/journal_session.h"
#include "workbook/history.h"
#include "platform/platform.h"
#include "data/source_digest.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL recovery candidate:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { Workbook live; char *root,*directory,*name,*checkpoint,*journal; WorkbookId generation; } Fixture;
static void apply(WorkbookJournalSession *s,Workbook *b,size_t sheet,const char *text) {
    CellEditTarget target; char *error=NULL; CHECK(workbook_prepare_cell_edit(b,sheet,0,0,&target,&error));
    CHECK(workbook_journal_session_apply(s,b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,text,strlen(text)},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error);
}
static Fixture fixture(const char *root) {
    Fixture f={0}; f.root=dx_strdup(root); workbook_init(&f.live); char *error=NULL;
    CHECK(workbook_add(&f.live,"A",&(Analysis){0},&error)); CHECK(workbook_add(&f.live,"B",&(Analysis){0},&error));
    CHECK(cells_set(&f.live.sheets[0].cells,0,0,"Baseline",&error)); worksheet_record(&f.live.sheets[0],EDIT_CELLS);
    CHECK(cells_set(&f.live.sheets[1].cells,1,0,"NULL",&error)); worksheet_record(&f.live.sheets[1],EDIT_CELLS);
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&f.live.sheets[1].formats,(FormatRange){1,1,0,0},FORMAT_BOLD,bold,&error)); worksheet_record(&f.live.sheets[1],EDIT_FORMAT);
    f.live.sheets[0].view.canvas_zoom=1.25f;
    char *base=dx_absolute_path("build/tests"); CHECK(base); f.live.path=dx_printf("%s/original İstanbul.dxls",base); free(base);
    WorkbookJournalSession *session=workbook_journal_session_new(root,(JournalQueueOptions){.entries=256,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=256},&error); CHECK(session && !error);
    apply(session,&f.live,0,"9007199254740993.0123456789"); apply(session,&f.live,1,"İstanbul\tline\ntwo"); apply(session,&f.live,0,"Final");
    double deadline=dx_millis()+10000; JournalSessionState state;
    do {
        state=workbook_journal_session_state(session,&f.live);
        if (state.queue.failed || dx_millis()>=deadline) { fprintf(stderr,"Checkpoint did not complete: %s\n",state.queue.error); exit(1); }
        if (state.queue.pending || state.queue.opening) dx_sleep_ms(1);
    } while (state.queue.pending || state.queue.opening);
    CHECK(state.queue.durable_revision==f.live.command_revision && !state.queue.pending && !state.queue.opening);
    f.generation=state.generation; f.checkpoint=dx_strdup(workbook_journal_session_checkpoint(session)); f.journal=dx_strdup(workbook_journal_session_journal(session));
    f.directory=dx_strdup(f.checkpoint); char *slash=strrchr(f.directory,'/'); CHECK(slash); *slash=0; f.name=dx_strdup(dx_basename(f.directory));
    CHECK(workbook_journal_session_close(session,false,&error) && !error); return f;
}
static void cleanup(Fixture *f) {
    CHECK(!unlink(f->checkpoint) && !unlink(f->journal)); char *lease=dx_printf("%s/lease.lock",f->directory); CHECK(!unlink(lease)); free(lease);
    CHECK(!rmdir(f->directory) && !rmdir(f->root)); workbook_free(&f->live); free(f->root); free(f->directory); free(f->name); free(f->checkpoint); free(f->journal);
}
static SourceDigest digest(const char *path) { SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d; }
static void unchanged(const char *path,SourceDigest before) { SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,32)); }
static void sql(const char *path,const char *text) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0}; CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,text,&r)==DuckDBSuccess); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
typedef struct { WorkbookRecoveryPhase phase; uint64_t at; bool seen; } Cancel;
static bool cancel(void *context,WorkbookRecoveryProgress p) { Cancel *c=context; if (p.phase==c->phase && p.completed>=c->at) { c->seen=true; return false; } return true; }
static void normal(Fixture *f) {
    char *error=NULL; SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal); WorkbookRecoveryCandidate *candidate=NULL,*other=NULL;
    uint64_t issued=worksheet_history_issue_order();
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && candidate && !error);
    CHECK(worksheet_history_issue_order()==issued+1);
    const Workbook *book=workbook_recovery_book(candidate); const WorkbookRecoveryInfo *info=workbook_recovery_info(candidate);
    CHECK(book && !book->path && book->dirty && book->command_revision==3 && book->count==2 && !book->edit_session_id);
    CHECK(!book->sheets[0].edit_session_id && !book->sheets[1].edit_session_id && !book->sheets[0].cells.undo_count && !book->sheets[1].cells.undo_count && !book->sheets[0].undo_actions_count);
    CHECK(book->id.high==f->live.id.high && book->id.low==f->live.id.low && book->sheets[1].id==f->live.sheets[1].id);
    CHECK(!strcmp(cells_get(&book->sheets[0].cells,0,0),"Final") && !strcmp(cells_get(&book->sheets[1].cells,0,0),"İstanbul\tline\ntwo") && !strcmp(cells_get(&book->sheets[1].cells,1,0),"NULL"));
    CHECK(info->bound && !info->tail_deferred && !info->tail_unknown && info->baseline_revision==0 && info->head_revision==3 && info->generation.high==f->generation.high && info->generation.low==f->generation.low);
    CHECK(!strcmp(info->original_path,f->live.path));
    CHECK(book->sheets[0].view.canvas_zoom==1.25f && format_at(&book->sheets[1].formats,1,0).flags==FORMAT_BOLD);
    WorkbookRecoveryCandidate *retained=candidate;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && candidate==retained && error); free(error); error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&other,&error)==RECOVERY_BUSY && !other && error); free(error); error=NULL;
    Workbook *owned=workbook_recovery_take_book(candidate); CHECK(owned==book && !workbook_recovery_book(candidate)); workbook_free(owned); free(owned); workbook_recovery_free(candidate); candidate=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED);
    book=workbook_recovery_book(candidate); info=workbook_recovery_info(candidate);
    CHECK(info->bound && info->tail_deferred && !info->tail_unknown && book->command_revision==0 && !strcmp(cells_get(&book->sheets[0].cells,0,0),"Baseline")); workbook_recovery_free(candidate); candidate=NULL;
    const struct { WorkbookRecoveryPhase phase; uint64_t at; } stops[]={{RECOVERY_CLAIM,0},{RECOVERY_VERIFY,1},{RECOVERY_LOAD,0},{RECOVERY_REPLAY,1},{RECOVERY_READY,1}};
    for (size_t i=0;i<sizeof(stops)/sizeof(*stops);i++) {
        Cancel stop={stops[i].phase,stops[i].at,false}; CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){cancel,&stop},&candidate,&error)==RECOVERY_CANCELLED);
        CHECK(stop.seen && !candidate && error && f->live.command_revision==3 && !strcmp(cells_get(&f->live.sheets[0].cells,0,0),"Final")); free(error); error=NULL;
    }
    CHECK(workbook_recovery_prepare(f->root,"../escape",RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL;
    char *alias=dx_printf("%s/session-00000000000000000000000000000000",f->root); CHECK(!symlink(f->name,alias));
    CHECK(workbook_recovery_prepare(f->root,dx_basename(alias),RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL; CHECK(!unlink(alias)); free(alias);
    char *wal=dx_printf("%s.wal",f->checkpoint); FILE *file=fopen(wal,"wb"); CHECK(file && !fclose(file));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL; CHECK(!unlink(wal)); free(wal);
    char *saved=dx_printf("%s.real",f->journal); CHECK(!rename(f->journal,saved) && !symlink(dx_basename(saved),f->journal));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_UNBOUND && !candidate && error); free(error); error=NULL;
    CHECK(!unlink(f->journal) && !rename(saved,f->journal)); free(saved);
    char *linked=dx_printf("%s/hardlink",f->directory); CHECK(!link(f->journal,linked));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_UNBOUND && !candidate && error); free(error); error=NULL; CHECK(!unlink(linked));
    file=fopen(linked,"wb"); CHECK(file && !fclose(file)); char *journal_wal=dx_printf("%s.wal",f->journal); CHECK(!symlink("hardlink",journal_wal));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_UNBOUND && !candidate && error); free(error); error=NULL;
    CHECK(!unlink(journal_wal) && !unlink(linked));
    CHECK(!symlink("missing-sidecar",journal_wal));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_UNBOUND && !candidate && error); free(error); error=NULL; CHECK(!unlink(journal_wal));
    char *baseline_wal=dx_printf("%s.wal",f->checkpoint); CHECK(!symlink("missing-sidecar",baseline_wal));
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL; CHECK(!unlink(baseline_wal));
    free(baseline_wal); free(journal_wal); free(linked);
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
}
static void bad_tail(Fixture *f) {
    sql(f->journal,"UPDATE duxcel_journal_cells SET before_value='wrong'::BLOB WHERE revision=2");
    SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal); WorkbookRecoveryCandidate *candidate=NULL; char *error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && !error);
    CHECK(workbook_recovery_info(candidate)->tail_deferred && workbook_recovery_info(candidate)->bound); workbook_recovery_free(candidate); candidate=NULL;
    unchanged(f->journal,tail);
    sql(f->journal,"DELETE FROM duxcel_journal_cells WHERE revision=2");
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && error && !candidate); free(error); error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED); workbook_recovery_free(candidate);
    unchanged(f->checkpoint,baseline);
}
static void unbound(Fixture *f) {
    sql(f->journal,"DROP TABLE duxcel_journal_origin; UPDATE duxcel_journal_meta SET version=1");
    SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal); WorkbookRecoveryCandidate *candidate=NULL; char *error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_UNBOUND && error && !candidate); free(error); error=NULL;
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && !error);
    const WorkbookRecoveryInfo *info=workbook_recovery_info(candidate); CHECK(!info->bound && info->tail_deferred && !info->tail_unknown); workbook_recovery_free(candidate); candidate=NULL;
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
    sql(f->journal,"DROP TABLE duxcel_journal_cells");
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && !error);
    CHECK(workbook_recovery_info(candidate)->tail_unknown && workbook_recovery_info(candidate)->tail_deferred); workbook_recovery_free(candidate);
}
static void wrong_baseline(Fixture *f) {
    Workbook wrong; workbook_init(&wrong); char *error=NULL; CHECK(test_current_checkpoint_load(&wrong,f->checkpoint,&error));
    CHECK(cells_set(&wrong.sheets[0].cells,0,0,"Other baseline",&error)); CHECK(test_current_checkpoint_save(&wrong,f->checkpoint,&error)); workbook_free(&wrong);
    SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal); WorkbookRecoveryCandidate *candidate=NULL;
    for (unsigned mode=0;mode<2;mode++) {
        CHECK(workbook_recovery_prepare(f->root,f->name,(WorkbookRecoveryMode)mode,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error); free(error); error=NULL;
    }
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,NULL)==RECOVERY_FAILED && !candidate);
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
}
static void wrong_digest(Fixture *f) {
    sql(f->journal,"UPDATE duxcel_journal_origin SET baseline_sha256=repeat('z',32)::BLOB");
    SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal); WorkbookRecoveryCandidate *candidate=NULL; char *error=NULL;
    for (unsigned mode=0;mode<2;mode++) {
        CHECK(workbook_recovery_prepare(f->root,f->name,(WorkbookRecoveryMode)mode,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_FAILED && !candidate && error && strstr(error,"digest")); free(error); error=NULL;
    }
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
}
typedef struct {
    Fixture *fixture;
    DxMutex mutex; DxCond condition;
    bool entered,released;
    WorkbookRecoveryCandidate *candidate;
    WorkbookRecoveryResult result;
    char *error;
} Async;
static bool parallel_progress(void *context,WorkbookRecoveryProgress p) {
    Async *a=context;
    if (p.phase==RECOVERY_REPLAY && !p.completed) {
        dx_lock(&a->mutex); a->entered=true;
        while (!a->released) dx_cond_wait(&a->condition,&a->mutex);
        dx_unlock(&a->mutex);
    }
    return true;
}
static void *parallel_prepare(void *context) {
    Async *a=context; a->result=workbook_recovery_prepare(a->fixture->root,a->fixture->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){parallel_progress,a},&a->candidate,&a->error); return NULL;
}
static void parallel_live_edit(Fixture *f) {
    Async a={.fixture=f}; dx_mutex_init(&a.mutex); dx_cond_init(&a.condition); DxThread thread;
    CHECK(dx_thread_start(&thread,parallel_prepare,&a)); double deadline=dx_millis()+10000;
    for (;;) { dx_lock(&a.mutex); bool entered=a.entered; dx_unlock(&a.mutex); if (entered) break; if (dx_millis()>=deadline) { fputs("Recovery worker did not reach its barrier\n",stderr); exit(1); } dx_sleep_ms(1); }
    char *error=NULL; CellEditTarget target; CHECK(workbook_prepare_cell_edit(&f->live,0,0,0,&target,&error));
    CHECK(workbook_apply_cell_edit(&f->live,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Live edit",9},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error);
    uint64_t issued=worksheet_history_issue_order(); dx_lock(&a.mutex); a.released=true; dx_signal(&a.condition); dx_unlock(&a.mutex); dx_thread_join(thread);
    CHECK(a.result==RECOVERY_PREPARED && a.candidate && !a.error && worksheet_history_issue_order()==issued+1);
    const Workbook *book=workbook_recovery_book(a.candidate);
    CHECK(book->command_revision==3 && !strcmp(cells_get(&book->sheets[0].cells,0,0),"Final") && !book->edit_session_id);
    CHECK(f->live.command_revision==4 && !strcmp(cells_get(&f->live.sheets[0].cells,0,0),"Live edit"));
    CHECK(workbook_history_step(&f->live,false,&error).status==WORKBOOK_HISTORY_APPLIED && !error && !strcmp(cells_get(&f->live.sheets[0].cells,0,0),"Final"));
    workbook_recovery_free(a.candidate); dx_cond_destroy(&a.condition); dx_mutex_destroy(&a.mutex);
}
static void retirement(Fixture *f) {
    char *error=NULL; WorkbookRecoveryCandidate *candidate=NULL;
    SourceDigest baseline=digest(f->checkpoint),tail=digest(f->journal);
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_OPEN_CHECKPOINT,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && !error);
    CHECK(!workbook_recovery_retire(candidate,&error) && error); free(error); error=NULL; workbook_recovery_free(candidate); candidate=NULL;
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
    CHECK(workbook_recovery_prepare(f->root,f->name,RECOVERY_REPLAY_TAIL,(WorkbookRecoveryControl){0},&candidate,&error)==RECOVERY_PREPARED && !error);
    char *extra=dx_printf("%s/keep.txt",f->directory); FILE *file=fopen(extra,"wb"); CHECK(file && fputs("private note",file)>=0 && !fclose(file));
    CHECK(!workbook_recovery_retire(candidate,&error) && error && !access(extra,F_OK)); free(error); error=NULL;
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail); CHECK(!unlink(extra)); free(extra);
    char *wal=dx_printf("%s.wal",f->journal); file=fopen(wal,"wb"); CHECK(file && !fclose(file));
    CHECK(!workbook_recovery_retire(candidate,&error) && error); free(error); error=NULL; CHECK(!unlink(wal)); free(wal);
    unchanged(f->checkpoint,baseline); unchanged(f->journal,tail);
    Workbook *book=workbook_recovery_take_book(candidate); CHECK(book && !book->path && book->dirty);
    CHECK(workbook_recovery_retire(candidate,&error) && !error && access(f->directory,F_OK));
    CHECK(workbook_recovery_retire(candidate,NULL) && !strcmp(cells_get(&book->sheets[0].cells,0,0),"Final"));
    workbook_recovery_free(candidate); workbook_free(book); free(book);
    CHECK(!rmdir(f->root)); workbook_free(&f->live); free(f->root); free(f->directory); free(f->name); free(f->checkpoint); free(f->journal);
}
int main(void) {
    char directory[]="build/tests/recovery-candidate-XXXXXX"; CHECK(mkdtemp(directory));
    const char *names[]={"normal","bad-tail","legacy","wrong-base","parallel","wrong-digest"};
    for (unsigned i=0;i<6;i++) {
        char *root=dx_printf("%s/%s",directory,names[i]); Fixture f=fixture(root); free(root);
        if (!i) normal(&f); else if (i==1) bad_tail(&f); else if (i==2) unbound(&f); else if (i==3) wrong_baseline(&f); else if (i==4) parallel_live_edit(&f); else wrong_digest(&f);
        cleanup(&f);
    }
    char *root=dx_printf("%s/retirement",directory); Fixture retired=fixture(root); free(root); retirement(&retired);
    CHECK(!rmdir(directory)); printf("PASS: %u isolated recovery candidate checks\n",checks); return 0;
}
