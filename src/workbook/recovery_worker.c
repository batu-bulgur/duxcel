#include "recovery_worker.h"
#include "../platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { WorkbookRecoveryWork kind; uint64_t id; bool restart; char generation[41]; WorkbookRecoveryMode mode; } Request;
struct WorkbookRecoveryWorker {
    char *root;
    DxThread thread; DxMutex mutex; DxCond changed;
    bool stop,cancel;
    uint64_t issued;
    Request request;
    WorkbookRecoveryWorkerState state;
    WorkbookRecoveryReply *reply;
    WorkbookRecoveryScan *scan; /* Reader thread only. */
};
#ifdef DUXCEL_TEST_RECOVERY_WORKER
extern void dx_test_recovery_worker_checkpoint(WorkbookRecoveryWorker *worker,unsigned stage,WorkbookRecoveryProgress progress);
#define CHECKPOINT(w,stage,p) dx_test_recovery_worker_checkpoint(w,stage,p)
#else
#define CHECKPOINT(w,stage,p) ((void)0)
#endif
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool cancelled(void *context) {
    WorkbookRecoveryWorker *w=context; dx_lock(&w->mutex); bool value=w->cancel || w->stop; dx_unlock(&w->mutex); return value;
}
static bool update(void *context,WorkbookRecoveryProgress progress) {
    WorkbookRecoveryWorker *w=context; dx_lock(&w->mutex); w->state.progress=progress; bool proceed=!w->cancel && !w->stop; dx_unlock(&w->mutex);
    CHECKPOINT(w,2,progress); return proceed && !cancelled(w);
}
static void page_clear(WorkbookRecoveryReply *r) {
    for (size_t i=0;i<r->page.count;i++) { free((void *)r->page.rows[i].generation_name); free((void *)r->page.rows[i].original_path); free((void *)r->page.rows[i].detail); }
    memset(&r->page,0,sizeof(r->page));
}
WorkbookRecoveryCandidate *workbook_recovery_reply_take_candidate(WorkbookRecoveryReply *r) {
    if (!r) return NULL; WorkbookRecoveryCandidate *candidate=r->candidate; r->candidate=NULL; return candidate;
}
void workbook_recovery_reply_free(WorkbookRecoveryReply *r) { if (!r) return; page_clear(r); workbook_recovery_free(r->candidate); free(r); }
typedef struct { WorkbookRecoveryWorker *worker; WorkbookRecoveryReply *reply; bool failed; } PageBuilder;
static bool row(void *context,const WorkbookRecoveryEntry *entry) {
    PageBuilder *b=context; WorkbookRecoveryPage *p=&b->reply->page;
    if (entry->kind==RECOVERY_ENTRY_ACTIVE) return !cancelled(b->worker);
    size_t name=strlen(entry->generation_name),original=strlen(entry->original_path),detail=strlen(entry->detail);
    size_t bytes=name+original+detail+3;
    if (p->count>=DX_RECOVERY_PAGE_ROWS || name>40 || original>65536 || detail>511 || bytes>DX_RECOVERY_PAGE_BYTES-p->owned_bytes) { b->failed=true; return false; }
    WorkbookRecoveryEntry *copy=p->rows+p->count++; *copy=*entry;
    copy->generation_name=dx_strdup(entry->generation_name); copy->original_path=dx_strdup(entry->original_path); copy->detail=dx_strdup(entry->detail); p->owned_bytes+=bytes;
    return p->count<DX_RECOVERY_PAGE_ROWS && !cancelled(b->worker);
}
static bool page_cancelled(void *context) { return cancelled(((PageBuilder *)context)->worker); }
static void close_scan(WorkbookRecoveryWorker *w) { workbook_recovery_scan_free(w->scan); w->scan=NULL; }
static void run_page(WorkbookRecoveryWorker *w,Request request,WorkbookRecoveryReply *r,char **error) {
    if (request.restart) close_scan(w);
    r->page.restarted=w->scan==NULL; r->page.owned_bytes=sizeof(*r);
    if (!w->scan && !workbook_recovery_scan_open(w->root,&w->scan,error)) { r->scan_result=RECOVERY_SCAN_FAILED; return; }
    PageBuilder builder={w,r,false};
    do {
        r->scan_result=workbook_recovery_scan_step(w->scan,128,(WorkbookRecoveryScanControl){row,page_cancelled,&builder},error);
        r->page.scan=workbook_recovery_scan_state(w->scan);
        dx_lock(&w->mutex); w->state.scan=r->page.scan; dx_unlock(&w->mutex);
        if (builder.failed) { fail(error,"Recovery rows exceed the bounded page budget."); r->scan_result=RECOVERY_SCAN_FAILED; }
    } while (r->scan_result==RECOVERY_SCAN_MORE && r->page.count<DX_RECOVERY_PAGE_ROWS && !builder.failed);
    if (r->scan_result==RECOVERY_SCAN_FAILED || r->scan_result==RECOVERY_SCAN_CANCELLED) { close_scan(w); page_clear(r); }
}
static void cancel_reply(WorkbookRecoveryWorker *w,WorkbookRecoveryReply *r) {
    workbook_recovery_free(r->candidate); r->candidate=NULL; page_clear(r); close_scan(w);
    r->prepare_result=RECOVERY_CANCELLED; r->scan_result=RECOVERY_SCAN_CANCELLED; r->error[0]=0;
}
static void *reader(void *context) {
    WorkbookRecoveryWorker *w=context;
    for (;;) {
        dx_lock(&w->mutex); while (!w->stop && w->request.kind==RECOVERY_WORK_NONE) dx_cond_wait(&w->changed,&w->mutex);
        if (w->stop) { dx_unlock(&w->mutex); break; }
        Request request=w->request; w->request=(Request){0}; dx_unlock(&w->mutex);
        CHECKPOINT(w,0,((WorkbookRecoveryProgress){0}));
        WorkbookRecoveryReply *r=dx_alloc(sizeof(*r)); r->id=request.id; r->kind=request.kind; r->scan_result=RECOVERY_SCAN_FAILED; r->prepare_result=RECOVERY_FAILED; char *error=NULL;
        if (cancelled(w)) { r->prepare_result=RECOVERY_CANCELLED; r->scan_result=RECOVERY_SCAN_CANCELLED; }
        else if (request.kind==RECOVERY_WORK_PAGE) run_page(w,request,r,&error);
        else {
            close_scan(w);
            r->prepare_result=workbook_recovery_prepare(w->root,request.generation,request.mode,(WorkbookRecoveryControl){update,w},&r->candidate,&error);
        }
        CHECKPOINT(w,1,((WorkbookRecoveryProgress){0}));
        if (cancelled(w)) {
            cancel_reply(w,r); free(error); error=NULL;
        }
        if (error) { snprintf(r->error,sizeof(r->error),"%s",error); free(error); }
        size_t formats=0;
        if (r->candidate) {
            WorkbookFormatUsage usage;
            if (workbook_format_usage(workbook_recovery_book(r->candidate),NULL,0,&usage)==FORMAT_BUDGET_OK) formats=usage.current;
            else { workbook_recovery_free(r->candidate); r->candidate=NULL; r->prepare_result=RECOVERY_FAILED; snprintf(r->error,sizeof(r->error),"Recovered format ownership could not be accounted safely."); }
        }
        dx_lock(&w->mutex);
        if (w->cancel || w->stop) {
            /* Keep reservations while retiring late owned results outside the
               mutex. The publication decision shares the cancellation lock. */
            dx_unlock(&w->mutex); cancel_reply(w,r); formats=0; dx_lock(&w->mutex);
        }
        bool stopping=w->stop;
        if (!stopping) {
            w->reply=r; w->state.active=false; w->state.ready=true;
            w->state.row_reservation_bytes=r->kind==RECOVERY_WORK_PAGE ? r->page.owned_bytes : 0; w->state.format_reservation_bytes=formats;
            r=NULL;
        }
        dx_unlock(&w->mutex); workbook_recovery_reply_free(r);
    }
    close_scan(w);
    dx_lock(&w->mutex); WorkbookRecoveryReply *reply=w->reply; w->reply=NULL; dx_unlock(&w->mutex); workbook_recovery_reply_free(reply);
    dx_lock(&w->mutex); w->state.active=w->state.ready=false; w->state.row_reservation_bytes=w->state.format_reservation_bytes=0; dx_unlock(&w->mutex); return NULL;
}
WorkbookRecoveryWorker *workbook_recovery_worker_new(const char *root,char **error) {
    if (!root || !*root || strlen(root)>65536) { fail(error,"Choose a bounded private recovery root."); return NULL; }
    WorkbookRecoveryWorker *w=dx_alloc(sizeof(*w)); w->root=dx_strdup(root); dx_mutex_init(&w->mutex); dx_cond_init(&w->changed);
    if (!dx_thread_start(&w->thread,reader,w)) { dx_cond_destroy(&w->changed); dx_mutex_destroy(&w->mutex); free(w->root); free(w); fail(error,"Cannot start the native recovery reader."); return NULL; }
    return w;
}
static bool submit(WorkbookRecoveryWorker *w,Request request,uint64_t *id,char **error) {
    if (!w || !id) return fail(error,"Choose an active recovery reader and request output.");
    dx_lock(&w->mutex);
    if (w->stop || w->state.active || w->reply || w->issued==UINT64_MAX) { dx_unlock(&w->mutex); return fail(error,"Finish or consume the current recovery request before starting another."); }
    request.id=++w->issued; w->request=request; w->cancel=false;
    w->state=(WorkbookRecoveryWorkerState){.id=request.id,.kind=request.kind,.active=true,.row_reservation_bytes=request.kind==RECOVERY_WORK_PAGE ? DX_RECOVERY_PAGE_BYTES : 0,.format_reservation_bytes=request.kind==RECOVERY_WORK_PREPARE ? DX_WORKBOOK_FORMAT_CURRENT_BYTES : 0};
    *id=request.id; dx_signal(&w->changed); dx_unlock(&w->mutex); return true;
}
bool workbook_recovery_worker_page(WorkbookRecoveryWorker *w,bool restart,uint64_t *id,char **error) { return submit(w,(Request){.kind=RECOVERY_WORK_PAGE,.restart=restart},id,error); }
bool workbook_recovery_worker_prepare(WorkbookRecoveryWorker *w,const char *generation,WorkbookRecoveryMode mode,uint64_t *id,char **error) {
    if (!generation || strlen(generation)!=40 || strncmp(generation,"session-",8) || (unsigned)mode>RECOVERY_OPEN_CHECKPOINT) return fail(error,"Choose a bounded generation and recovery mode.");
    Request request={.kind=RECOVERY_WORK_PREPARE,.mode=mode}; memcpy(request.generation,generation,41); return submit(w,request,id,error);
}
void workbook_recovery_worker_cancel(WorkbookRecoveryWorker *w) { if (!w) return; dx_lock(&w->mutex); if (w->state.active) { w->cancel=true; w->state.cancel_requested=true; } dx_unlock(&w->mutex); }
WorkbookRecoveryWorkerState workbook_recovery_worker_state(WorkbookRecoveryWorker *w) { if (!w) return (WorkbookRecoveryWorkerState){0}; dx_lock(&w->mutex); WorkbookRecoveryWorkerState state=w->state; dx_unlock(&w->mutex); return state; }
WorkbookRecoveryReply *workbook_recovery_worker_poll(WorkbookRecoveryWorker *w) {
    if (!w) return NULL; dx_lock(&w->mutex); WorkbookRecoveryReply *r=w->reply; w->reply=NULL;
    if (r) { w->state.ready=false; w->state.row_reservation_bytes=w->state.format_reservation_bytes=0; }
    dx_unlock(&w->mutex); return r;
}
void workbook_recovery_worker_free(WorkbookRecoveryWorker *w) {
    if (!w) return; dx_lock(&w->mutex); w->stop=w->cancel=true; dx_signal(&w->changed); dx_unlock(&w->mutex); dx_thread_join(w->thread);
    dx_cond_destroy(&w->changed); dx_mutex_destroy(&w->mutex); free(w->root); free(w);
}
