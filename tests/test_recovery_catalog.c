#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/recovery_catalog.h"
#include "workbook/journal_session.h"
#include "platform/platform.h"
#include "platform/file_lease.h"
#include "data/source_digest.h"
#include <duckdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL recovery catalog:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { WorkbookJournalSession *session; char *directory,*checkpoint,*journal; WorkbookId generation; } Generation;
static Generation generation(const char *root,Workbook *b,const char *value,bool keep_active) {
    char *error=NULL; Generation g={0}; g.session=workbook_journal_session_new(root,(JournalQueueOptions){.entries=256,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=256},&error); CHECK(g.session && !error);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(b,0,0,0,&target,&error));
    CHECK(workbook_journal_session_apply(g.session,b,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,value,strlen(value)},(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED && !error);
    double deadline=dx_millis()+10000; JournalSessionState state;
    do { state=workbook_journal_session_state(g.session,b); if (state.queue.failed || dx_millis()>=deadline) { fprintf(stderr,"Backup failed: %s\n",state.queue.error); exit(1); } if (state.queue.opening || state.queue.pending) dx_sleep_ms(1); } while (state.queue.opening || state.queue.pending);
    CHECK(state.queue.durable_revision==1); g.generation=state.generation; g.checkpoint=dx_strdup(workbook_journal_session_checkpoint(g.session)); g.journal=dx_strdup(workbook_journal_session_journal(g.session));
    g.directory=dx_strdup(g.checkpoint); *strrchr(g.directory,'/')=0;
    if (!keep_active) { CHECK(workbook_journal_session_close(g.session,false,&error) && !error); g.session=NULL; }
    return g;
}
static void cleanup_generation(Generation *g) {
    char *error=NULL;
    if (g->session) CHECK(workbook_journal_session_close(g->session,true,&error) && !error);
    else { char *lease=dx_printf("%s/lease.lock",g->directory); CHECK(!unlink(g->checkpoint) && !unlink(g->journal) && !unlink(lease) && !rmdir(g->directory)); free(lease); }
    free(g->directory); free(g->checkpoint); free(g->journal);
}
static SourceDigest digest(const char *path) { SourceDigest d={0}; char error[DX_SOURCE_DIGEST_ERROR_BYTES]={0}; CHECK(source_digest_file(path,NULL,NULL,&d,error)==SOURCE_DIGEST_OK); return d; }
static void unchanged(const char *path,SourceDigest before) { SourceDigest after=digest(path); CHECK(before.bytes==after.bytes && !memcmp(before.sha256,after.sha256,32)); }
static void sql(const char *path,const char *text) {
    duckdb_database db=NULL; duckdb_connection c=NULL; duckdb_result r={0}; CHECK(duckdb_open(path,&db)==DuckDBSuccess && duckdb_connect(db,&c)==DuckDBSuccess);
    CHECK(duckdb_query(c,text,&r)==DuckDBSuccess); duckdb_destroy_result(&r); duckdb_disconnect(&c); duckdb_close(&db);
}
static char *folder(const char *root,unsigned id,bool lease,bool bad) {
    char *path=dx_printf("%s/session-%032u",root,id); CHECK(!mkdir(path,0700));
    if (lease) {
        char *name=dx_printf("%s/lease.lock",path); DxFileLease *owned=NULL; char *error=NULL;
        CHECK(dx_file_lease_acquire(name,true,&owned,&error)==DX_FILE_LEASE_ACQUIRED && !error); dx_file_lease_release(owned);
        if (bad) { FILE *f=fopen(name,"wb"); CHECK(f && fputs("preserve",f)>=0 && !fclose(f)); } free(name);
    }
    return path;
}
typedef struct { WorkbookId document; Generation *first,*second; unsigned kinds[7],bound; bool cancel; char *name[16],*original[16]; size_t count; } Read;
static bool visit(void *context,const WorkbookRecoveryEntry *entry) {
    Read *r=context; CHECK(r->count<16 && (unsigned)entry->kind<7); r->kinds[entry->kind]++;
    r->name[r->count]=dx_strdup(entry->generation_name); r->original[r->count++]=dx_strdup(entry->original_path);
    if (entry->kind==RECOVERY_ENTRY_BOUND) {
        CHECK(entry->document.high==r->document.high && entry->document.low==r->document.low && !entry->baseline_revision && entry->declared_head_revision==1);
        CHECK(!entry->parent.high && !entry->parent.low && strstr(entry->original_path,"original.dxls"));
        bool first=entry->generation.high==r->first->generation.high && entry->generation.low==r->first->generation.low;
        bool second=entry->generation.high==r->second->generation.high && entry->generation.low==r->second->generation.low;
        CHECK(first || second); r->bound|=first ? 1u : 2u;
    }
    return false; /* One row per page exercises pause/resume ownership. */
}
static bool cancelled(void *context) { return ((Read *)context)->cancel; }
int main(void) {
    char root[]="build/tests/recovery-catalog-XXXXXX"; CHECK(mkdtemp(root)); char *missing=dx_printf("%s/missing",root); char *error=NULL; WorkbookRecoveryScan *scan=NULL;
    CHECK(workbook_recovery_scan_open(missing,&scan,&error) && scan && !error && workbook_recovery_scan_state(scan).done && access(missing,F_OK));
    CHECK(workbook_recovery_scan_step(scan,1,(WorkbookRecoveryScanControl){0},&error)==RECOVERY_SCAN_DONE); workbook_recovery_scan_free(scan); scan=NULL; free(missing);
    Workbook a; workbook_init(&a); CHECK(workbook_add(&a,"A",&(Analysis){0},&error)); char *base=dx_absolute_path("build/tests"); a.path=dx_printf("%s/original.dxls",base); free(base);
    Workbook *b=workbook_snapshot(&a),*c=workbook_snapshot(&a); CHECK(b && c);
    Generation first=generation(root,&a,"Branch A",false),second=generation(root,b,"Branch B",false),active=generation(root,c,"Active",true);
    CHECK(first.generation.high!=second.generation.high || first.generation.low!=second.generation.low);
    SourceDigest checkpoint=digest(first.checkpoint),journal=digest(first.journal);
    char *legacy=folder(root,1,false,false),*incomplete=folder(root,2,true,false),*invalid=folder(root,3,true,true),*only=folder(root,4,true,false);
    char *only_checkpoint=dx_printf("%s/checkpoint.dxls",only); CHECK(workbook_save(&a,only_checkpoint,&error));
    char *note=dx_printf("%s/notes",root); CHECK(!mkdir(note,0700)); char *plain=dx_printf("%s/plain",root); FILE *f=fopen(plain,"wb"); CHECK(f && fputs("preserve",f)>=0 && !fclose(f));
    char *alias=dx_printf("%s/session-00000000000000000000000000000005",root); CHECK(!symlink(dx_basename(first.directory),alias));
    CHECK(workbook_recovery_scan_open(root,&scan,&error) && scan && !error); Read read={.document=a.id,.first=&first,.second=&second,.cancel=true};
    CHECK(workbook_recovery_scan_step(scan,1,(WorkbookRecoveryScanControl){visit,cancelled,&read},&error)==RECOVERY_SCAN_CANCELLED && !workbook_recovery_scan_state(scan).visited);
    read.cancel=false; CHECK(workbook_recovery_scan_step(scan,128,(WorkbookRecoveryScanControl){visit,cancelled,&read},&error)==RECOVERY_SCAN_MORE);
    CHECK(read.count==1 && workbook_recovery_scan_state(scan).offered==1);
    uint64_t visited=workbook_recovery_scan_state(scan).visited; read.cancel=true;
    CHECK(workbook_recovery_scan_step(scan,128,(WorkbookRecoveryScanControl){visit,cancelled,&read},&error)==RECOVERY_SCAN_CANCELLED && read.count==1 && workbook_recovery_scan_state(scan).visited==visited);
    read.cancel=false; WorkbookRecoveryScanResult result;
    do {
        result=workbook_recovery_scan_step(scan,1,(WorkbookRecoveryScanControl){visit,cancelled,&read},&error);
        WorkbookRecoveryScanState state=workbook_recovery_scan_state(scan); CHECK(state.visited>=visited && state.visited-visited<=1); visited=state.visited;
    } while (result==RECOVERY_SCAN_MORE);
    WorkbookRecoveryScanState state=workbook_recovery_scan_state(scan);
    CHECK(result==RECOVERY_SCAN_DONE && !error && state.done && state.visited==10 && state.offered==7 && state.active==1 && state.legacy==1 && state.invalid==2);
    CHECK(read.count==7 && read.bound==3 && read.kinds[RECOVERY_ENTRY_BOUND]==2 && read.kinds[RECOVERY_ENTRY_ACTIVE]==1 && read.kinds[RECOVERY_ENTRY_LEGACY]==1 && read.kinds[RECOVERY_ENTRY_CHECKPOINT_ONLY]==1 && read.kinds[RECOVERY_ENTRY_INCOMPLETE]==1 && read.kinds[RECOVERY_ENTRY_INVALID]==1);
    CHECK(workbook_recovery_scan_step(scan,0,(WorkbookRecoveryScanControl){0},&error)==RECOVERY_SCAN_FAILED && error); free(error); error=NULL;
    CHECK(workbook_recovery_scan_step(scan,129,(WorkbookRecoveryScanControl){0},&error)==RECOVERY_SCAN_FAILED && error); free(error); error=NULL;
    CHECK(workbook_recovery_scan_state(scan).visited==visited); workbook_recovery_scan_free(scan);
    for (size_t i=0;i<read.count;i++) { free(read.name[i]); free(read.original[i]); }
    unchanged(first.checkpoint,checkpoint); unchanged(first.journal,journal);
    sql(second.journal,"DROP TABLE duxcel_journal_origin; UPDATE duxcel_journal_meta SET version=1");
    char *wal=dx_printf("%s.wal",only_checkpoint); f=fopen(wal,"wb"); CHECK(f && !fclose(f));
    scan=NULL; CHECK(workbook_recovery_scan_open(root,&scan,&error)); Read legacy_read={.document=a.id,.first=&first,.second=&second};
    do { result=workbook_recovery_scan_step(scan,128,(WorkbookRecoveryScanControl){visit,cancelled,&legacy_read},&error); } while (result==RECOVERY_SCAN_MORE);
    CHECK(result==RECOVERY_SCAN_DONE && !error && legacy_read.count==7 && legacy_read.kinds[RECOVERY_ENTRY_UNBOUND]==1 && legacy_read.kinds[RECOVERY_ENTRY_BOUND]==1 && legacy_read.kinds[RECOVERY_ENTRY_INCOMPLETE]==2 && !legacy_read.kinds[RECOVERY_ENTRY_CHECKPOINT_ONLY]);
    workbook_recovery_scan_free(scan); for (size_t i=0;i<legacy_read.count;i++) { free(legacy_read.name[i]); free(legacy_read.original[i]); }
    CHECK(!unlink(wal)); free(wal); unchanged(first.checkpoint,checkpoint); unchanged(first.journal,journal);
    CHECK(!unlink(alias) && !unlink(plain) && !rmdir(note)); free(alias); free(plain); free(note);
    CHECK(!rmdir(legacy)); free(legacy);
    char *lease=dx_printf("%s/lease.lock",incomplete); CHECK(!unlink(lease) && !rmdir(incomplete)); free(lease); free(incomplete);
    lease=dx_printf("%s/lease.lock",invalid); f=fopen(lease,"rb"); CHECK(f); char text[9]={0}; CHECK(fread(text,1,8,f)==8 && !strcmp(text,"preserve") && !fclose(f)); CHECK(!unlink(lease) && !rmdir(invalid)); free(lease); free(invalid);
    lease=dx_printf("%s/lease.lock",only); CHECK(!unlink(lease) && !unlink(only_checkpoint) && !rmdir(only)); free(lease); free(only_checkpoint); free(only);
    cleanup_generation(&first); cleanup_generation(&second); cleanup_generation(&active); workbook_free(&a); workbook_free(b); free(b); workbook_free(c); free(c);
    CHECK(!rmdir(root)); printf("PASS: %u bounded recovery catalog checks\n",checks); return 0;
}
