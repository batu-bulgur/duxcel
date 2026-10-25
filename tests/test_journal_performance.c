#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "support/current_checkpoint.h"
#include "workbook/journal_session.h"
#include "platform/platform.h"
#include <duckdb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL journal performance:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { DxMutex mutex; DxCond changed; unsigned commits; bool blocked; } Counter;
static bool before_commit(void *context) { Counter *c=context; dx_lock(&c->mutex); while (c->blocked) dx_cond_wait(&c->changed,&c->mutex); dx_unlock(&c->mutex); return true; }
static void committed(void *context) { Counter *c=context; dx_lock(&c->mutex); c->commits++; dx_unlock(&c->mutex); }
static unsigned count(Counter *c) { dx_lock(&c->mutex); unsigned n=c->commits; dx_unlock(&c->mutex); return n; }
static double apply(WorkbookJournalSession *s,Workbook *b,const char *value) {
    CellEditTarget target; char *error=NULL;
    CHECK(workbook_prepare_cell_edit(b,0,0,0,&target,&error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,value,strlen(value)};
    double start=dx_millis();
    CellEditResult result=workbook_journal_session_apply(s,b,&command,(CellEditPolicy){0,0,true},&error);
    double elapsed=dx_millis()-start;
    if (error) fprintf(stderr,"Admission: %s\n",error);
    CHECK(result.status==CELL_EDIT_APPLIED && !error); return elapsed;
}
static void ready(WorkbookJournalSession *s,Workbook *b) {
    double deadline=dx_millis()+60000;
    for (;;) {
        workbook_journal_session_poll(s); JournalSessionState state=workbook_journal_session_state(s,b);
        if (state.queue.failed) { fprintf(stderr,"Journal failed: %s\n",state.queue.error); exit(1); }
        CHECK(state.queue.pending<=DX_JOURNAL_BATCH && state.queue.owned_bytes<=DX_JOURNAL_BATCH_BYTES);
        if (state.active && !state.queue.opening && !state.queue.pending && state.queue.durable_revision==b->command_revision) return;
        CHECK(dx_millis()<deadline); dx_sleep_ms(1);
    }
}
static int compare(const void *a,const void *b) { double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y); }
static double percentile(double *values,size_t n,unsigned percent) { qsort(values,n,sizeof(*values),compare); return values[(n-1)*percent/100]; }
static char *read_input(const char *path) {
    FILE *file=fopen(path,"rb"); CHECK(file); CHECK(!fseek(file,0,SEEK_END)); long bytes=ftell(file); CHECK(bytes>0 && bytes<DX_CELL_BYTES);
    rewind(file); char *text=dx_alloc((size_t)bytes+1); CHECK(fread(text,1,(size_t)bytes,file)==(size_t)bytes); CHECK(!fclose(file)); text[bytes]=0; return text;
}
static bool replay(void *context,const JournalCellEdit *e,char **error) {
    Workbook *b=context; CHECK(e->sheet==b->sheets[0].id && e->previous_revision==b->command_revision);
    CHECK(e->row==0 && e->column==0);
    const char *before=cells_get(&b->sheets[0].cells,e->row,e->column);
    CHECK(strlen(before)==e->before_bytes && !memcmp(before,e->before,e->before_bytes));
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(b,0,e->row,e->column,&target,error));
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,e->after,e->after_bytes};
    CHECK(workbook_apply_cell_edit(b,&command,(CellEditPolicy){0,0,true},error).status==CELL_EDIT_APPLIED);
    return true;
}
int main(int argc,char **argv) {
    CHECK(argc==2); Dl_info library; CHECK(dladdr((void *)duckdb_library_version,&library) && library.dli_fname);
    printf("ENGINE_VERSION %s\nENGINE_LIBRARY %s\n",duckdb_library_version(),library.dli_fname);
    char directory[]="build/tests/journal-performance-XXXXXX"; CHECK(mkdtemp(directory));
    Workbook b,loaded; workbook_init(&b); workbook_init(&loaded); char *error=NULL;
    CHECK(workbook_add(&b,"Bench",&(Analysis){0},&error)); Counter counter={0}; dx_mutex_init(&counter.mutex); dx_cond_init(&counter.changed);
    JournalQueueOptions options={.entries=DX_JOURNAL_BATCH,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=DX_JOURNAL_BATCH,
        .observer={.before_commit=before_commit,.after_commit=committed,.context=&counter}};
    WorkbookJournalSession *s=workbook_journal_session_new(directory,options,&error); CHECK(s && !error);
    double start=dx_millis(),bootstrap=apply(s,&b,"seed"); ready(s,&b); double bootstrap_durable=dx_millis()-start;
    double producer[64],durable[64];
    for (unsigned i=0;i<64;i++) { start=dx_millis(); producer[i]=apply(s,&b,i%2?"B":"A"); ready(s,&b); durable[i]=dx_millis()-start; }
    unsigned commits=count(&counter); start=dx_millis(); double burst[256];
    for (unsigned i=0;i<256;i++) burst[i]=apply(s,&b,i%2?"B":"A");
    double burst_admission=dx_millis()-start; ready(s,&b); double burst_durable=dx_millis()-start; unsigned burst_commits=count(&counter)-commits;
    CHECK(b.command_revision==321 && burst_commits>0 && burst_commits<=256);
    /* A deterministic blocked writer measures rejection, not disk throughput. */
    dx_lock(&counter.mutex); counter.blocked=true; dx_unlock(&counter.mutex);
    for (unsigned i=0;i<256;i++) apply(s,&b,i%2?"B":"A");
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&b,0,0,0,&target,&error));
    CellEditCommand overflow={CELL_EDIT_COMMAND_VERSION,target,"overflow",8};
    start=dx_millis(); CellEditResult rejected=workbook_journal_session_apply(s,&b,&overflow,(CellEditPolicy){0,0,true},&error);
    double rejection=dx_millis()-start;
    CHECK(rejected.status==CELL_EDIT_LIMIT && error && b.command_revision==577);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,0,0),"B")); free(error); error=NULL;
    dx_lock(&counter.mutex); counter.blocked=false; dx_signal(&counter.changed); dx_unlock(&counter.mutex); ready(s,&b);
    char *tsv=read_input(argv[1]); size_t rows=0,columns=0; start=dx_millis();
    CHECK(cells_paste(&b.sheets[0].cells,0,0,tsv,&rows,&columns,&error));
    worksheet_record(&b.sheets[0],EDIT_CELLS); b.dirty=true; double paste=dx_millis()-start; free(tsv);
    CHECK(rows==20000 && columns==1 && b.sheets[0].cells.count==20000 && b.command_revision==577);
    CHECK(!workbook_journal_session_state(s,&b).covered);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,0,0),"9007199254740993.0123456789"));
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,1,0),"NULL"));
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,2,0),"İstanbul\tline\ntwo"));
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,19999,0),"last-row"));
    start=dx_millis(); double checkpoint_admission=apply(s,&b,"after-paste"); ready(s,&b); double checkpoint_durable=dx_millis()-start;
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*journal=dx_strdup(workbook_journal_session_journal(s));
    CHECK(workbook_journal_session_close(s,false,&error)); CHECK(test_current_checkpoint_load(&loaded,checkpoint,&error));
    CHECK(loaded.command_revision==577 && loaded.sheets[0].cells.count==20000);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,0,0),"9007199254740993.0123456789"));
    WorkbookJournal *j=NULL; CHECK(workbook_journal_open(&j,journal,loaded.id,loaded.command_revision,&error)); size_t visited=0;
    CHECK(workbook_journal_read(j,loaded.command_revision,256,replay,&loaded,&visited,&error)); CHECK(visited==1 && !error);
    CHECK(loaded.command_revision==578 && loaded.sheets[0].cells.count==b.sheets[0].cells.count);
    bool equal=true; for (size_t i=0;i<b.sheets[0].cells.count;i++) { const SheetCell *a=b.sheets[0].cells.cells+i,*v=loaded.sheets[0].cells.cells+i;
        if (a->row!=v->row || a->col!=v->col || strcmp(a->text,v->text)) equal=false; } CHECK(equal);
    workbook_journal_close(j); workbook_free(&loaded); workbook_free(&b); dx_cond_destroy(&counter.changed); dx_mutex_destroy(&counter.mutex);
    CHECK(!unlink(checkpoint) && !unlink(journal)); char *slash=strrchr(checkpoint,'/'); CHECK(slash); *slash=0;
    char *lease=dx_printf("%s/lease.lock",checkpoint); CHECK(!unlink(lease)); free(lease);
    CHECK(!rmdir(checkpoint) && !rmdir(directory)); free(checkpoint); free(journal);
    struct rusage usage; CHECK(!getrusage(RUSAGE_SELF,&usage));
#ifdef __APPLE__
    double rss=usage.ru_maxrss/1048576.0;
#else
    double rss=usage.ru_maxrss/1024.0;
#endif
    printf("METRICS {\"bootstrap_producer_ms\":%.3f,\"bootstrap_durable_ms\":%.3f,\"warm_producer_p50_ms\":%.3f,\"warm_producer_p95_ms\":%.3f,\"warm_durable_p50_ms\":%.3f,\"warm_durable_p95_ms\":%.3f,\"burst_producer_ms\":%.3f,\"burst_producer_p95_ms\":%.3f,\"burst_durable_ms\":%.3f,\"burst_commits\":%u,\"backpressure_rejection_ms\":%.3f,\"paste_20000_ms\":%.3f,\"checkpoint_producer_ms\":%.3f,\"checkpoint_durable_ms\":%.3f,\"process_peak_mib\":%.3f}\n",
        bootstrap,bootstrap_durable,percentile(producer,64,50),percentile(producer,64,95),percentile(durable,64,50),percentile(durable,64,95),
        burst_admission,percentile(burst,256,95),burst_durable,burst_commits,rejection,paste,checkpoint_admission,checkpoint_durable,rss);
    printf("PASS: journal performance (%u checks)\n",checks); return 0;
}
