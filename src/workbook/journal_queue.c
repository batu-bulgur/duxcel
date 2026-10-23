#include "journal_queue.h"
#include "../platform/platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct JournalReservation {
    WorkbookJournalQueue *owner;
    JournalCellEdit edit;
    size_t bytes;
    unsigned char data[];
};
struct WorkbookJournalQueue {
    DxThread thread;
    DxMutex mutex;
    DxCond changed;
    char *path;
    WorkbookId document;
    uint64_t base,expected_head;
    JournalQueueOptions options;
    JournalQueueState state;
    JournalReservation *items[DX_JOURNAL_BATCH],*reservation;
    bool drain,joined,attempted;
};
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static void failure_locked(WorkbookJournalQueue *q,const char *message) {
    q->state.failed=true; q->state.opening=q->state.writing=false;
    snprintf(q->state.error,sizeof(q->state.error),"%s",message ? message : "Journal writer failed.");
}
static bool before_commit(void *context) {
    WorkbookJournalQueue *q=context;
    dx_lock(&q->mutex); bool cancel=q->state.stopping && !q->drain; dx_unlock(&q->mutex);
    if (cancel || (q->options.observer.before_commit && !q->options.observer.before_commit(q->options.observer.context))) return false;
    dx_lock(&q->mutex); cancel=q->state.stopping && !q->drain; dx_unlock(&q->mutex);
    return !cancel;
}
static void after_commit(void *context) {
    WorkbookJournalQueue *q=context;
    if (q->options.observer.after_commit) q->options.observer.after_commit(q->options.observer.context);
}
static void *writer(void *context) {
    WorkbookJournalQueue *q=context; WorkbookJournal *journal=NULL;
    for (;;) {
        dx_lock(&q->mutex);
        while (q->state.failed && !q->state.stopping) dx_cond_wait(&q->changed,&q->mutex);
        if (q->state.stopping && (!q->drain || q->state.failed || !q->state.pending)) { dx_unlock(&q->mutex); break; }
        if (!journal) {
            bool already_prepared=q->state.prepared;
            dx_unlock(&q->mutex); char *error=NULL;
            bool prepared=already_prepared || !q->options.setup.prepare || q->options.setup.prepare(q->options.setup.context,&error);
            bool opened=prepared && workbook_journal_open(&journal,q->path,q->document,q->base,&error);
            if (opened && q->options.origin) {
                const JournalOrigin *origin=q->options.origin(q->options.setup.context);
                opened=origin && workbook_journal_bind_origin(journal,origin,&error);
                if (!origin) fail(&error,"Recovery checkpoint origin is not ready.");
            }
            dx_lock(&q->mutex);
            q->state.prepared=prepared;
            uint64_t head=workbook_journal_durable_revision(journal);
            bool matches=opened && (q->attempted ? head>=q->state.durable_revision && head<=q->state.accepted_revision : head==q->expected_head);
            if (!matches) failure_locked(q,error ? error : "Journal head does not match the admitted checkpoint.");
            else q->state.opening=false;
            dx_unlock(&q->mutex); free(error);
            if (!matches) { workbook_journal_close(journal); journal=NULL; }
            continue;
        }
        while (!q->state.pending && !q->state.stopping) dx_cond_wait(&q->changed,&q->mutex);
        if (q->state.stopping && (!q->drain || !q->state.pending)) { dx_unlock(&q->mutex); break; }
        size_t count=q->state.pending;
        if (count>q->options.batch) count=q->options.batch;
        JournalCellEdit batch[DX_JOURNAL_BATCH];
        for (size_t i=0;i<count;i++) batch[i]=q->items[i]->edit;
        q->state.writing=true; q->attempted=true; dx_unlock(&q->mutex);
        char *error=NULL; JournalReceipt receipt={0};
        bool committed=workbook_journal_append(journal,batch,count,(JournalControl){before_commit,after_commit,q},&receipt,&error);
        dx_lock(&q->mutex); q->state.writing=false;
        if (committed) {
            for (size_t i=0;i<count;i++) { q->state.owned_bytes-=q->items[i]->bytes; free(q->items[i]); }
            q->state.pending-=count;
            memmove(q->items,q->items+count,q->state.pending*sizeof(*q->items));
            memset(q->items+q->state.pending,0,count*sizeof(*q->items));
            q->state.durable_revision=receipt.durable_revision;
        } else failure_locked(q,error);
        dx_unlock(&q->mutex); free(error);
        if (!committed) { workbook_journal_close(journal); journal=NULL; }
    }
    workbook_journal_close(journal);
    dx_lock(&q->mutex); q->state.stopped=true; q->state.opening=q->state.writing=false; dx_unlock(&q->mutex);
    return NULL;
}
WorkbookJournalQueue *workbook_journal_queue_start(const char *path,WorkbookId document,uint64_t base,uint64_t head,JournalQueueOptions options,char **error) {
    if (!path || !*path || !strncmp(path,":memory:",8) || (!document.high && !document.low) || head<base ||
        !options.entries || options.entries>DX_JOURNAL_BATCH || !options.bytes || options.bytes>DX_JOURNAL_BATCH_BYTES ||
        !options.batch || options.batch>options.entries) { fail(error,"Choose a bounded journal queue and checkpoint lineage."); return NULL; }
    WorkbookJournalQueue *q=dx_alloc(sizeof(*q)); memset(q,0,sizeof(*q));
    q->path=dx_strdup(path); q->document=document; q->base=base; q->expected_head=head; q->options=options;
    q->state=(JournalQueueState){.accepted_revision=head,.durable_revision=head,.opening=true};
    dx_mutex_init(&q->mutex); dx_cond_init(&q->changed);
    if (!dx_thread_start(&q->thread,writer,q)) {
        dx_cond_destroy(&q->changed); dx_mutex_destroy(&q->mutex); free(q->path); free(q);
        fail(error,"Cannot start the journal writer."); return NULL;
    }
    return q;
}
bool workbook_journal_queue_reserve(WorkbookJournalQueue *q,const JournalCellEdit *e,JournalReservation **out,char **error) {
    if (!q || !e || !out || *out || !workbook_journal_edit_valid(e)) return fail(error,"Choose a valid cell edit and empty reservation.");
    size_t bytes=sizeof(JournalReservation)+e->before_bytes+e->after_bytes;
    dx_lock(&q->mutex);
    bool room=!q->reservation && !q->state.stopping && !q->state.failed && e->previous_revision==q->state.accepted_revision &&
        q->state.pending<q->options.entries && bytes<=q->options.bytes-q->state.owned_bytes;
    for (size_t i=0;room && i<q->state.pending;i++) if (q->items[i]->edit.operation.high==e->operation.high && q->items[i]->edit.operation.low==e->operation.low) room=false;
    if (!room) {
        const char *message=q->state.failed ? "Local backup is paused. Retry it in the status bar. Your draft is kept." :
            e->previous_revision!=q->state.accepted_revision ? "The workbook changed while preparing backup. Your draft is kept; cancel and reopen the cell." :
            "Local backup is busy. Your draft is kept; try again shortly.";
        dx_unlock(&q->mutex); return fail(error,message);
    }
    JournalReservation *r=dx_alloc(bytes); r->owner=q; r->edit=*e; r->bytes=bytes;
    if (e->before_bytes) memcpy(r->data,e->before,e->before_bytes);
    if (e->after_bytes) memcpy(r->data+e->before_bytes,e->after,e->after_bytes);
    r->edit.before=(const char *)r->data; r->edit.after=(const char *)r->data+e->before_bytes;
    q->reservation=r; q->state.reserved=true; q->state.owned_bytes+=bytes;
    dx_unlock(&q->mutex); *out=r; return true;
}
void workbook_journal_queue_cancel(JournalReservation *r) {
    if (!r) return;
    WorkbookJournalQueue *q=r->owner; dx_lock(&q->mutex); assert(q->reservation==r);
    q->reservation=NULL; q->state.reserved=false; q->state.owned_bytes-=r->bytes; dx_unlock(&q->mutex); free(r);
}
void workbook_journal_queue_publish(JournalReservation *r) {
    assert(r); WorkbookJournalQueue *q=r->owner; dx_lock(&q->mutex);
    assert(q->reservation==r && q->state.pending<q->options.entries && !q->state.stopping);
    q->items[q->state.pending++]=r; q->state.accepted_revision=r->edit.revision;
    q->reservation=NULL; q->state.reserved=false; dx_signal(&q->changed); dx_unlock(&q->mutex);
}
JournalQueueState workbook_journal_queue_state(WorkbookJournalQueue *q) {
    if (!q) return (JournalQueueState){0};
    dx_lock(&q->mutex); JournalQueueState state=q->state; dx_unlock(&q->mutex); return state;
}
bool workbook_journal_queue_retry(WorkbookJournalQueue *q,char **error) {
    if (!q) return fail(error,"Choose an active journal queue.");
    dx_lock(&q->mutex);
    if (!q->state.failed || q->state.stopping) { dx_unlock(&q->mutex); return fail(error,"Only a paused active journal queue can retry."); }
    q->state.failed=false; q->state.opening=true; q->state.error[0]=0; dx_signal(&q->changed); dx_unlock(&q->mutex); return true;
}
bool workbook_journal_queue_request_stop(WorkbookJournalQueue *q,bool drain,char **error) {
    if (!q) return true;
    dx_lock(&q->mutex);
    if (q->reservation) { dx_unlock(&q->mutex); return fail(error,"Publish or cancel the reserved edit before stopping the journal."); }
    q->state.stopping=true; q->drain=drain; dx_signal(&q->changed); dx_unlock(&q->mutex); return true;
}
bool workbook_journal_queue_stop(WorkbookJournalQueue *q,bool drain,char **error) {
    if (!q) return true;
    if (!workbook_journal_queue_request_stop(q,drain,error)) return false;
    if (!q->joined) { dx_thread_join(q->thread); q->joined=true; }
    JournalQueueState state=workbook_journal_queue_state(q);
    return (!state.failed && !state.pending) || fail(error,*state.error ? state.error : "Journal stopped with uncommitted edits; preserve the live workbook.");
}
void workbook_journal_queue_free(WorkbookJournalQueue *q) {
    if (!q) return;
    if (q->reservation) workbook_journal_queue_cancel(q->reservation);
    workbook_journal_queue_stop(q,false,NULL);
    for (size_t i=0;i<q->state.pending;i++) free(q->items[i]);
    if (q->options.setup.release) q->options.setup.release(q->options.setup.context);
    dx_cond_destroy(&q->changed); dx_mutex_destroy(&q->mutex); free(q->path); free(q);
}
