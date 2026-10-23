#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/journal_queue.h"
#include "platform/platform.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL journal queue:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const WorkbookId document={UINT64_MAX,71};
typedef struct { DxMutex mutex; DxCond changed; bool entered,release,cancel_first; size_t calls; } Gate;
static void gate_init(Gate *g,bool release,bool cancel) { memset(g,0,sizeof(*g)); dx_mutex_init(&g->mutex); dx_cond_init(&g->changed); g->release=release; g->cancel_first=cancel; }
static bool gate_before(void *context) {
    Gate *g=context; dx_lock(&g->mutex); size_t call=++g->calls; g->entered=true;
    while (!g->release) dx_cond_wait(&g->changed,&g->mutex);
    bool result=!(g->cancel_first && call==1); dx_unlock(&g->mutex); return result;
}
static void gate_release(Gate *g) { dx_lock(&g->mutex); g->release=true; dx_signal(&g->changed); dx_unlock(&g->mutex); }
static void gate_wait(Gate *g) {
    double limit=dx_millis()+5000;
    for (;;) { dx_lock(&g->mutex); bool entered=g->entered; dx_unlock(&g->mutex); if (entered) break; if (dx_millis()>=limit) { fputs("Timed out waiting for journal commit barrier\n",stderr); exit(1); } dx_sleep_ms(1); }
}
static void gate_free(Gate *g) { dx_cond_destroy(&g->changed); dx_mutex_destroy(&g->mutex); }
static JournalCellEdit edit(uint64_t revision,const char *before,const char *after) {
    return (JournalCellEdit){.operation={93,revision},.previous_revision=revision-1,.revision=revision,.sheet=UINT64_MAX,.row=revision,.column=3,
        .before=before,.after=after,.before_bytes=strlen(before),.after_bytes=strlen(after)};
}
static JournalQueueOptions options(Gate *gate,size_t entries,size_t batch) {
    return (JournalQueueOptions){.entries=entries,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=batch,.observer={gate_before,NULL,gate}};
}
static JournalQueueState wait_state(WorkbookJournalQueue *q,uint64_t head,bool failure) {
    double limit=dx_millis()+5000; JournalQueueState state;
    do {
        state=workbook_journal_queue_state(q);
        if (failure ? state.failed : !state.opening && !state.failed && !state.pending && state.durable_revision==head) return state;
        if (dx_millis()>=limit) { fprintf(stderr,"Timed out: accepted=%llu durable=%llu pending=%zu opening=%d failed=%d %s\n",(unsigned long long)state.accepted_revision,(unsigned long long)state.durable_revision,state.pending,state.opening,state.failed,state.error); exit(1); }
        dx_sleep_ms(1);
    } while (true);
}
static void publish(WorkbookJournalQueue *q,JournalCellEdit e) {
    char *error=NULL; JournalReservation *reservation=NULL;
    CHECK(workbook_journal_queue_reserve(q,&e,&reservation,&error)); CHECK(reservation && !error); workbook_journal_queue_publish(reservation);
}
typedef struct { uint64_t next; const char *first,*second; bool numbered; } Read;
static bool verify(void *context,const JournalCellEdit *e,char **error) {
    (void)error; Read *read=context; CHECK(e->revision==++read->next && e->previous_revision==e->revision-1 && e->operation.high==93 && e->operation.low==e->revision);
    CHECK(e->sheet==UINT64_MAX && e->row==e->revision && e->column==3);
    char value[32]; snprintf(value,sizeof(value),"Value %llu",(unsigned long long)e->revision);
    const char *expected=read->numbered ? value : e->revision==1 ? read->first : read->second;
    CHECK(e->after_bytes==strlen(expected) && !memcmp(e->after,expected,e->after_bytes)); return true;
}
static void inspect(const char *path,uint64_t head,const char *first,const char *second,bool numbered) {
    WorkbookJournal *j=NULL; char *error=NULL; CHECK(workbook_journal_open(&j,path,document,0,&error)); CHECK(!error && workbook_journal_durable_revision(j)==head);
    Read read={0,first,second,numbered};
    while (read.next<head) { size_t visited=0; CHECK(workbook_journal_read(j,read.next,256,verify,&read,&visited,&error)); CHECK(visited && visited<=DX_JOURNAL_READ_BATCH); }
    CHECK(read.next==head); workbook_journal_close(j);
}
static void reservation_and_backpressure(const char *path) {
    Gate gate; gate_init(&gate,false,false); char *error=NULL;
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,options(&gate,2,1),&error); CHECK(q && !error);
    char value[]="Original"; JournalCellEdit e=edit(1,"",value); JournalReservation *r=NULL,*other=NULL;
    CHECK(workbook_journal_queue_reserve(q,&e,&r,&error)); JournalQueueState state=workbook_journal_queue_state(q);
    CHECK(state.reserved && !state.pending && !state.accepted_revision && !state.durable_revision && state.owned_bytes>e.after_bytes);
    CHECK(!workbook_journal_queue_reserve(q,&e,&other,&error) && !other); free(error); error=NULL;
    CHECK(!workbook_journal_queue_stop(q,true,&error)); CHECK(error && !workbook_journal_queue_state(q).stopping); free(error); error=NULL;
    workbook_journal_queue_cancel(r); r=NULL; state=workbook_journal_queue_state(q); CHECK(!state.reserved && !state.owned_bytes && !state.accepted_revision);
    CHECK(workbook_journal_queue_reserve(q,&e,&r,&error)); memcpy(value,"Changed!",8); workbook_journal_queue_publish(r); gate_wait(&gate);
    state=workbook_journal_queue_state(q); CHECK(state.writing && state.pending==1 && state.accepted_revision==1 && !state.durable_revision);
    JournalCellEdit conflict=edit(2,"Original","Second"); conflict.operation=e.operation;
    r=NULL; CHECK(!workbook_journal_queue_reserve(q,&conflict,&r,&error)); CHECK(error && !r); free(error); error=NULL;
    publish(q,edit(2,"Original","Second")); state=workbook_journal_queue_state(q); size_t owned=state.owned_bytes;
    e=edit(3,"Second","Third"); r=NULL; CHECK(!workbook_journal_queue_reserve(q,&e,&r,&error));
    CHECK(error && !r && workbook_journal_queue_state(q).owned_bytes==owned); free(error); error=NULL;
    gate_release(&gate); state=wait_state(q,2,false); CHECK(state.accepted_revision==2 && !state.owned_bytes && !state.failed);
    CHECK(workbook_journal_queue_stop(q,true,&error)); workbook_journal_queue_free(q); gate_free(&gate);
    inspect(path,2,"Original","Second",false);
}
static void failure_during_reservation(const char *path) {
    Gate gate; gate_init(&gate,false,true); char *error=NULL;
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,options(&gate,4,4),&error); CHECK(q && !error);
    publish(q,edit(1,"","First")); gate_wait(&gate);
    JournalReservation *r=NULL; JournalCellEdit e=edit(2,"First","Second"); CHECK(workbook_journal_queue_reserve(q,&e,&r,&error));
    gate_release(&gate); JournalQueueState failed=wait_state(q,0,true);
    CHECK(failed.reserved && failed.pending==1 && failed.accepted_revision==1 && !failed.durable_revision && *failed.error);
    /* Applying the model after reservation may race the writer error. Publication
       must still retain the second immutable record and never fail here. */
    workbook_journal_queue_publish(r); failed=workbook_journal_queue_state(q);
    CHECK(failed.failed && failed.pending==2 && !failed.reserved && failed.accepted_revision==2 && !failed.durable_revision);
    r=NULL; e=edit(3,"Second","Third"); CHECK(!workbook_journal_queue_reserve(q,&e,&r,&error)); CHECK(!r && error); free(error); error=NULL;
    CHECK(workbook_journal_queue_retry(q,&error)); JournalQueueState committed=wait_state(q,2,false);
    CHECK(!committed.failed && !*committed.error && !committed.owned_bytes && committed.accepted_revision==2);
    CHECK(workbook_journal_queue_stop(q,true,&error)); workbook_journal_queue_free(q); gate_free(&gate); inspect(path,2,"First","Second",false);
}
static void batching(const char *path) {
    Gate gate; gate_init(&gate,false,false); char *error=NULL;
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,options(&gate,256,256),&error); CHECK(q && !error);
    publish(q,edit(1,"","Value 1")); gate_wait(&gate);
    for (uint64_t i=2;i<=256;i++) { char value[32]; snprintf(value,sizeof(value),"Value %llu",(unsigned long long)i); publish(q,edit(i,"",value)); memset(value,'x',strlen(value)); }
    JournalQueueState pending=workbook_journal_queue_state(q); CHECK(pending.pending==256 && pending.accepted_revision==256 && !pending.durable_revision && pending.owned_bytes<=DX_JOURNAL_BATCH_BYTES);
    JournalReservation *r=NULL; JournalCellEdit e=edit(257,"","Overflow"); CHECK(!workbook_journal_queue_reserve(q,&e,&r,&error)); CHECK(!r && error); free(error); error=NULL;
    gate_release(&gate); JournalQueueState done=wait_state(q,256,false); CHECK(!done.owned_bytes && !done.pending);
    CHECK(workbook_journal_queue_stop(q,true,&error)); CHECK(gate.calls==2); workbook_journal_queue_free(q); gate_free(&gate); inspect(path,256,NULL,NULL,true);
}
static void byte_budget(const char *path) {
    Gate gate; gate_init(&gate,false,false); char *error=NULL; JournalQueueOptions opts=options(&gate,8,8); opts.bytes=70000;
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,opts,&error); CHECK(q && !error);
    char *large=dx_alloc(65537); memset(large,'a',65536); large[65536]=0;
    publish(q,edit(1,"",large)); gate_wait(&gate); size_t bytes=workbook_journal_queue_state(q).owned_bytes;
    JournalCellEdit e=edit(2,large,"Small"); JournalReservation *r=NULL;
    CHECK(!workbook_journal_queue_reserve(q,&e,&r,&error)); CHECK(error && !r && bytes==workbook_journal_queue_state(q).owned_bytes && bytes<=70000); free(error); error=NULL; free(large);
    gate_release(&gate); wait_state(q,1,false); CHECK(workbook_journal_queue_stop(q,true,&error)); workbook_journal_queue_free(q); gate_free(&gate);
}
static void *release_later(void *context) { dx_sleep_ms(20); gate_release(context); return NULL; }
static void shutdown_case(const char *path,bool drain) {
    Gate gate; gate_init(&gate,false,false); char *error=NULL;
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,options(&gate,2,2),&error); CHECK(q && !error);
    publish(q,edit(1,"","First")); gate_wait(&gate);
    DxThread release; CHECK(dx_thread_start(&release,release_later,&gate));
    CHECK(workbook_journal_queue_stop(q,drain,&error)==drain); dx_thread_join(release);
    JournalQueueState state=workbook_journal_queue_state(q); CHECK(state.stopped && !state.writing && state.pending==(drain ? 0u : 1u) && state.durable_revision==(drain ? 1u : 0u));
    CHECK(drain ? !error : error!=NULL); free(error); error=NULL;
    CHECK(!workbook_journal_queue_retry(q,&error)); CHECK(error); free(error);
    workbook_journal_queue_free(q); gate_free(&gate); inspect(path,drain ? 1 : 0,"First",NULL,false);
}
typedef struct { size_t calls; bool released; } Setup;
static bool setup_prepare(void *context,char **error) {
    Setup *setup=context;
    if (++setup->calls==1) { if (error) *error=dx_strdup("Checkpoint preparation failed."); return false; }
    return true;
}
static void setup_release(void *context) { ((Setup *)context)->released=true; }
static void preparation(const char *path) {
    Gate gate; gate_init(&gate,true,false); Setup setup={0}; char *error=NULL;
    JournalQueueOptions opts=options(&gate,2,2); opts.setup=(JournalQueueSetup){setup_prepare,setup_release,&setup};
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,opts,&error); CHECK(q && !error);
    JournalQueueState failed=wait_state(q,0,true); CHECK(!failed.prepared && !failed.durable_revision && *failed.error && !setup.released);
    CHECK(workbook_journal_queue_retry(q,&error)); publish(q,edit(1,"","Prepared"));
    JournalQueueState done=wait_state(q,1,false); CHECK(done.prepared && !done.failed);
    CHECK(workbook_journal_queue_request_stop(q,true,&error)); CHECK(workbook_journal_queue_stop(q,true,&error));
    workbook_journal_queue_free(q); CHECK(setup.calls==2 && setup.released); gate_free(&gate); inspect(path,1,"Prepared",NULL,false);
}
static void lineage_and_options(const char *path,const char *missing) {
    Gate gate; gate_init(&gate,true,false); char *error=NULL; JournalQueueOptions opts=options(&gate,2,2);
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,opts,&error); CHECK(q && !error);
    JournalQueueState state=wait_state(q,0,true); CHECK(!state.pending && !state.durable_revision && *state.error);
    CHECK(!workbook_journal_queue_stop(q,true,&error)); CHECK(error); free(error); error=NULL; workbook_journal_queue_free(q);
    q=workbook_journal_queue_start(path,document,0,2,opts,&error); CHECK(q && !error); publish(q,edit(3,"Second","Third")); wait_state(q,3,false);
    CHECK(workbook_journal_queue_stop(q,true,&error)); workbook_journal_queue_free(q);
    q=workbook_journal_queue_start(missing,document,0,0,opts,&error); CHECK(q && !error); state=wait_state(q,0,true); CHECK(*state.error && !state.pending);
    char *parent=dx_strdup(missing); char *slash=strrchr(parent,'/'); CHECK(slash); *slash=0; CHECK(!mkdir(parent,0700));
    CHECK(workbook_journal_queue_retry(q,&error)); publish(q,edit(1,"","Recovered writer")); wait_state(q,1,false);
    CHECK(workbook_journal_queue_stop(q,true,&error)); workbook_journal_queue_free(q); inspect(missing,1,"Recovered writer",NULL,false);
    CHECK(!unlink(missing) && !rmdir(parent)); free(parent);
    opts.entries=0; CHECK(!workbook_journal_queue_start(path,document,0,0,opts,&error)); CHECK(error); free(error); error=NULL;
    opts=options(&gate,2,2); opts.bytes=DX_JOURNAL_BATCH_BYTES+1; CHECK(!workbook_journal_queue_start(path,document,0,0,opts,&error)); CHECK(error); free(error); error=NULL;
    opts=options(&gate,2,2); CHECK(!workbook_journal_queue_start(":memory:",document,0,0,opts,&error)); CHECK(error); free(error);
    gate_free(&gate);
}
static bool process_prepare(void *context,char **error) { (void)error; return gate_before(context); }
static int process_mode(int argc,char **argv) {
    if (argc!=3) return 2;
    const char *path=argv[2]; char *error=NULL;
    if (!strcmp(argv[1],"--init")) {
        WorkbookJournal *j=NULL; CHECK(workbook_journal_open(&j,path,document,0,&error)); CHECK(!error); workbook_journal_close(j);
        puts("PASS: queue journal initialized"); return 0;
    }
    if (!strcmp(argv[1],"--inspect")) { inspect(path,2,"First","Second",false); puts("PASS: queue journal complete"); return 0; }
    if (strcmp(argv[1],"--fault")) return 2;
    Gate gate; gate_init(&gate,false,false); JournalQueueOptions opts={.entries=2,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=2};
    opts.setup=(JournalQueueSetup){.prepare=process_prepare,.context=&gate};
    /* Setup pauses before opening the existing file, so both records are ready
       before the actual failing two-record transaction begins. */
    WorkbookJournalQueue *q=workbook_journal_queue_start(path,document,0,0,opts,&error); CHECK(q && !error);
    publish(q,edit(1,"","First")); publish(q,edit(2,"","Second")); gate_release(&gate);
    JournalQueueState failed=wait_state(q,0,true);
    CHECK(failed.failed && failed.pending==2 && failed.owned_bytes>0 && failed.accepted_revision==2 && !failed.durable_revision && *failed.error);
    printf("EXPECTED queue I/O failure: %.400s\n",failed.error);
    void *symbol=dlsym(RTLD_DEFAULT,"duxcel_journal_fault_disable"); unsigned (*disable)(void)=NULL;
    CHECK(symbol && sizeof(symbol)==sizeof(disable)); memcpy(&disable,&symbol,sizeof(disable)); CHECK(disable()>0);
    CHECK(workbook_journal_queue_retry(q,&error)); JournalQueueState done=wait_state(q,2,false);
    CHECK(done.durable_revision==2 && !done.pending && !done.owned_bytes && !done.failed);
    CHECK(workbook_journal_queue_stop(q,true,&error)); CHECK(!error); workbook_journal_queue_free(q); gate_free(&gate);
    inspect(path,2,"First","Second",false); puts("PASS: queue I/O retry retained exact records"); return 0;
}
int main(int argc,char **argv) {
    if (argc>1) return process_mode(argc,argv);
    char directory[]="build/tests/journal-queue-XXXXXX"; CHECK(mkdtemp(directory));
    const char *names[]={"bounds","retry","batch","bytes","abort","drain","prepare"}; char *paths[7];
    for (unsigned i=0;i<7;i++) paths[i]=dx_printf("%s/%s.duckdb",directory,names[i]);
    reservation_and_backpressure(paths[0]); failure_during_reservation(paths[1]); batching(paths[2]); byte_budget(paths[3]); shutdown_case(paths[4],false); shutdown_case(paths[5],true); preparation(paths[6]);
    char *missing=dx_printf("%s/missing/edits.duckdb",directory); lineage_and_options(paths[0],missing); free(missing);
    for (unsigned i=0;i<7;i++) { unlink(paths[i]); free(paths[i]); } CHECK(!rmdir(directory));
    printf("PASS: %u journal queue checks\n",checks); return 0;
}
