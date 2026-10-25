#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "support/current_checkpoint.h"
#include "workbook/journal_session.h"
#include "workbook/format_command.h"
#include "workbook/analysis_command.h"
#include "workbook/table_command.h"
#include "workbook/object_name_command.h"
#include "workbook/lifecycle.h"
#include "platform/platform.h"
#include "data/source_digest.h"
#include "platform/file_lease.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL journal session:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void base(Workbook *b) { workbook_init(b); char *error=NULL; CHECK(workbook_add(b,"A",&(Analysis){0},&error)); CHECK(workbook_add(b,"B",&(Analysis){0},&error)); CHECK(!error); }
static CellEditCommand command(Workbook *b,size_t sheet,const char *text) {
    CellEditTarget target; char *error=NULL; CHECK(workbook_prepare_cell_edit(b,sheet,3,2,&target,&error)); CHECK(!error);
    return (CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,text,strlen(text)};
}
static JournalQueueOptions options(void) { return (JournalQueueOptions){.entries=256,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=256}; }
static WorkbookJournalSession *session(const char *root,JournalQueueOptions opts) { char *error=NULL; WorkbookJournalSession *s=workbook_journal_session_new(root,opts,&error); CHECK(s && !error); return s; }
static void apply(WorkbookJournalSession *s,Workbook *b,size_t sheet,const char *text) {
    char *error=NULL; CellEditCommand c=command(b,sheet,text); uint64_t revision=b->command_revision;
    CHECK(workbook_journal_session_apply(s,b,&c,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    CHECK(!error && b->command_revision==revision+1 && b->dirty);
}
static JournalSessionState ready(WorkbookJournalSession *s,Workbook *b) {
    double limit=dx_millis()+10000;
    for (;;) {
        workbook_journal_session_poll(s); JournalSessionState state=workbook_journal_session_state(s,b);
        if (state.queue.failed) { fprintf(stderr,"Backup failed: %s\n",state.queue.error); exit(1); }
        if (state.active && !state.queue.opening && !state.queue.pending && state.queue.durable_revision==b->command_revision) return state;
        if (dx_millis()>=limit) { fputs("Backup did not complete\n",stderr); exit(1); } dx_sleep_ms(1);
    }
}
static char *parent(const char *path) { char *directory=dx_strdup(path),*slash=strrchr(directory,'/'); CHECK(slash); *slash=0; return directory; }
static void cleanup_generation(const char *checkpoint,const char *journal) { char *directory=parent(checkpoint),*lease=dx_printf("%s/lease.lock",directory); CHECK(!unlink(lease)); free(lease); CHECK(!unlink(checkpoint)); CHECK(!unlink(journal)); CHECK(!rmdir(directory)); free(directory); }
typedef struct { Workbook *book; } Replay;
static bool replay(void *context,const JournalCellEdit *e,char **error) {
    Replay *r=context; Workbook *b=r->book; CHECK(e->previous_revision==b->command_revision);
    Worksheet *sheet=NULL; for (size_t i=0;i<b->count;i++) if (b->sheets[i].id==e->sheet) sheet=b->sheets+i;
    CHECK(sheet); const char *before=cells_get(&sheet->cells,e->row,e->column);
    CHECK(strlen(before)==e->before_bytes && (!e->before_bytes || !memcmp(before,e->before,e->before_bytes)));
    char *after=dx_alloc(e->after_bytes+1); if (e->after_bytes) memcpy(after,e->after,e->after_bytes); after[e->after_bytes]=0;
    bool changed=cells_set(&sheet->cells,e->row,e->column,after,error); free(after); CHECK(changed);
    worksheet_record(sheet,EDIT_CELLS); workbook_history_note_edit(b); b->command_revision=e->revision; return true;
}
static void recover(const char *checkpoint,const char *journal,Workbook *out,uint64_t head,const char *original) {
    char *error=NULL; CHECK(test_current_checkpoint_load(out,checkpoint,&error)); WorkbookJournal *j=NULL;
    CHECK(workbook_journal_open(&j,journal,out->id,out->command_revision,&error)); CHECK(workbook_journal_durable_revision(j)==head);
#ifdef __APPLE__
    const JournalOrigin *origin=workbook_journal_origin(j); CHECK(origin && (origin->generation.high || origin->generation.low));
    SourceDigest digest={0}; char detail[DX_SOURCE_DIGEST_ERROR_BYTES]={0};
    CHECK(source_digest_file(checkpoint,NULL,NULL,&digest,detail)==SOURCE_DIGEST_OK);
    CHECK(origin->baseline_bytes==digest.bytes && !memcmp(origin->baseline_sha256,digest.sha256,32));
    CHECK(!strcmp(origin->original_path,original ? original : ""));
#else
    (void)original;
#endif
    Replay context={out};
    while (out->command_revision<head) { size_t visited=0; CHECK(workbook_journal_read(j,out->command_revision,256,replay,&context,&visited,&error)); CHECK(visited); }
    CHECK(!error); workbook_journal_close(j);
}
static void initial_checkpoint(const char *root) {
    Workbook b,loaded; base(&b); workbook_init(&loaded); char *error=NULL;
    char *locator=dx_absolute_path("build/tests"); CHECK(locator); b.path=dx_printf("%s/original named İstanbul.dxls",locator); free(locator);
    CHECK(cells_set(&b.sheets[0].cells,0,0,"9007199254740993.0123456789",&error)); worksheet_record(&b.sheets[0],EDIT_CELLS);
    WorkbookJournalSession *s=session(root,options()); CellEditCommand same=command(&b,0,"");
    CHECK(workbook_journal_session_apply(s,&b,&same,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_NOOP);
    CHECK(!workbook_journal_session_state(s,&b).active && access(root,F_OK));
    CellEditCommand protected=command(&b,0,"Blocked");
    CHECK(workbook_journal_session_apply(s,&b,&protected,(CellEditPolicy){20,4,true},&error).status==CELL_EDIT_PROTECTED); CHECK(error && access(root,F_OK)); free(error); error=NULL;
    char value[]="First"; apply(s,&b,0,value); memset(value,'x',5); apply(s,&b,1,"NULL");
    JournalSessionState state=ready(s,&b); CHECK(state.covered && !state.checkpoint_format_bytes && state.queue.accepted_revision==2);
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*journal=dx_strdup(workbook_journal_session_journal(s));
    CHECK(workbook_journal_session_owns_path(s,checkpoint) && workbook_journal_session_owns_path(s,journal));
    char *directory=parent(checkpoint),*inside=dx_printf("%s/new-workbook.dxls",directory); free(directory);
    CHECK(workbook_journal_session_owns_path(s,inside) && !workbook_journal_session_owns_path(s,"build/tests/generated/named.dxls")); free(inside);
    char *lease_directory=parent(checkpoint),*lease_path=dx_printf("%s/lease.lock",lease_directory); free(lease_directory); DxFileLease *lease=NULL;
    CHECK(dx_file_lease_acquire(lease_path,false,&lease,&error)==DX_FILE_LEASE_BUSY && !lease && !error);
    CHECK(workbook_journal_session_close(s,false,&error)); CHECK(!error);
    CHECK(dx_file_lease_acquire(lease_path,false,&lease,&error)==DX_FILE_LEASE_ACQUIRED && lease && !error); dx_file_lease_release(lease); free(lease_path);
    CHECK(test_current_checkpoint_load(&loaded,checkpoint,&error)); CHECK(loaded.command_revision==0 && !*cells_get(&loaded.sheets[0].cells,3,2));
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,0,0),"9007199254740993.0123456789"));
    recover(checkpoint,journal,&loaded,2,b.path);
    CHECK(!strcmp(cells_get(&loaded.sheets[0].cells,3,2),"First") && !strcmp(cells_get(&loaded.sheets[1].cells,3,2),"NULL"));
    CHECK(loaded.id.high==b.id.high && loaded.id.low==b.id.low && loaded.sheets[0].id==b.sheets[0].id && loaded.sheets[1].id==b.sheets[1].id);
    cleanup_generation(checkpoint,journal); CHECK(!rmdir(root)); free(checkpoint); free(journal); workbook_free(&loaded); workbook_free(&b);
}
static void rotation_and_history(const char *root) {
    Workbook b,loaded; base(&b); workbook_init(&loaded); char *error=NULL;
    WorkbookJournalSession *s=session(root,options()); apply(s,&b,0,"First"); JournalSessionState first=ready(s,&b);
    CHECK((first.generation.high || first.generation.low) && !first.parent.high && !first.parent.low);
    char *old=parent(workbook_journal_session_checkpoint(s));
    CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&b.sheets[1].formats,(FormatRange){3,3,2,2},FORMAT_BOLD,bold,&error)); worksheet_record(&b.sheets[1],EDIT_FORMAT);
    CHECK(!workbook_journal_session_state(s,&b).covered && workbook_journal_session_needs_checkpoint(s,&b));
    CellEditCommand noop=command(&b,0,"First"); CHECK(workbook_journal_session_apply(s,&b,&noop,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_NOOP);
    CHECK(!workbook_journal_session_state(s,&b).covered);
    apply(s,&b,1,"Second"); JournalSessionState second=ready(s,&b);
    CHECK(second.parent.high==first.generation.high && second.parent.low==first.generation.low);
    CHECK(second.generation.high!=first.generation.high || second.generation.low!=first.generation.low);
    for (unsigned i=0;i<100 && !access(old,F_OK);i++) { workbook_journal_session_poll(s); dx_sleep_ms(1); }
    CHECK(access(old,F_OK)); free(old);
    ManualFormatTarget format_target={0}; FormatSnapshot format_next={0}; CellFormat italic=format_default(); italic.flags=FORMAT_ITALIC;
    uint64_t format_revision=b.command_revision;
    CHECK(workbook_prepare_manual_format(&b,0,&format_target,&error));
    CHECK(format_plan_spatial(&b.sheets[0].formats,(FormatRange){0,0,0,0},FORMAT_ITALIC,italic,&format_next,&error));
    CHECK(workbook_apply_manual_format_recorded(&b,&format_target,&format_next,NULL,&error).status==MANUAL_FORMAT_APPLIED && b.command_revision==format_revision+1 && !format_next.items);
    CHECK(!workbook_journal_session_state(s,&b).covered && workbook_journal_session_needs_checkpoint(s,&b));
    CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_RENAME,.sheet=1,.name="Renamed B"},NULL,&error));
    CHECK(workbook_sheet_command(&b,(WorkbookSheetCommand){.operation=SHEET_REORDER,.sheet=1,.destination=0},NULL,&error));
    apply(s,&b,0,"Third"); ready(s,&b);
    CHECK(workbook_history_step(&b,false,&error).status==WORKBOOK_HISTORY_APPLIED);
    CHECK(workbook_history_available(&b,true) && !workbook_journal_session_state(s,&b).covered);
    noop=command(&b,0,"Second"); CHECK(workbook_journal_session_apply(s,&b,&noop,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_NOOP);
    CHECK(workbook_history_available(&b,true)); apply(s,&b,0,"Branch"); ready(s,&b); CHECK(!workbook_history_available(&b,true));
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*journal=dx_strdup(workbook_journal_session_journal(s)); uint64_t head=b.command_revision;
    CHECK(workbook_journal_session_close(s,false,&error)); CHECK(!error);
    recover(checkpoint,journal,&loaded,head,b.path);
    CHECK(!strcmp(loaded.sheets[0].name,"Renamed B") && !strcmp(cells_get(&loaded.sheets[0].cells,3,2),"Branch"));
    CHECK(format_at(&loaded.sheets[0].formats,3,2).flags==FORMAT_BOLD && !strcmp(cells_get(&loaded.sheets[1].cells,3,2),"First"));
    CHECK(format_at(&loaded.sheets[1].formats,0,0).flags==FORMAT_ITALIC);
    cleanup_generation(checkpoint,journal); CHECK(!rmdir(root)); free(checkpoint); free(journal); workbook_free(&loaded); workbook_free(&b);
}
static void analysis_rotation(const char *root) {
    Workbook b,loaded; base(&b); workbook_init(&loaded); char *error=NULL;
    b.sheets[0].analysis=(Analysis){.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/duplicates.csv")};
    CHECK(b.sheets[0].analysis.path);
    WorkbookJournalSession *s=session(root,options()); apply(s,&b,1,"Before transform"); JournalSessionState first=ready(s,&b);
    uint64_t epoch=1,revision=b.command_revision; AnalysisCommandContext context={&b,&b.sheets[0].analysis,&epoch,SIZE_MAX,true,false};
    AnalysisTransformTarget target={0}; Analysis next=analysis_copy(context.live); next.unique_rows=true;
    CHECK(workbook_prepare_analysis_transform(&context,0,&target,&error));
    CHECK(workbook_apply_analysis_transform_recorded(&context,&target,&next,NULL,&error).status==ANALYSIS_TRANSFORM_APPLIED && !next.path && b.command_revision==revision+1);
    CHECK(workbook_journal_session_needs_checkpoint(s,&b) && !workbook_journal_session_state(s,&b).covered);
    /* A new primary definition must be in the next checkpoint; it cannot be
       skipped as if the document revision advanced through a cell-tail entry. */
    apply(s,&b,1,"After transform"); JournalSessionState second=ready(s,&b);
    CHECK(second.parent.high==first.generation.high && second.parent.low==first.generation.low);
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*journal=dx_strdup(workbook_journal_session_journal(s)); uint64_t head=b.command_revision;
    CHECK(workbook_journal_session_close(s,false,&error) && !error);
    recover(checkpoint,journal,&loaded,head,b.path);
    CHECK(loaded.sheets[0].analysis.unique_rows && loaded.sheets[0].analysis.kind==SOURCE_CSV && !strcmp(loaded.sheets[0].analysis.path,b.sheets[0].analysis.path) &&
        !strcmp(cells_get(&loaded.sheets[1].cells,3,2),"After transform"));
    cleanup_generation(checkpoint,journal); CHECK(!rmdir(root)); free(checkpoint); free(journal); workbook_free(&loaded); workbook_free(&b);
}
static void table_rotation(const char *root) {
    Workbook b,loaded; base(&b); workbook_init(&loaded); char *error=NULL;
    Analysis source={.kind=SOURCE_CSV,.path=dx_absolute_path("tests/fixtures/duplicates.csv")}; CHECK(source.path);
    CHECK(objects_add_table(&b.sheets[0].objects,10,10,"Raw",&source,&error)==0); analysis_free(&source); worksheet_record(b.sheets,EDIT_OBJECTS);
    WorkbookJournalSession *s=session(root,options()); apply(s,&b,1,"Before Table transform"); JournalSessionState first=ready(s,&b);
    uint64_t epoch=1,revision=b.command_revision; TableCommandContext context={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; TableTransformTarget target;
    Analysis next=analysis_copy(&b.sheets[0].objects.items[0].source); next.unique_rows=true;
    CHECK(workbook_prepare_table_transform(&context,0,b.sheets[0].objects.items[0].id,&target,&error));
    CHECK(workbook_apply_table_transform_recorded(&context,&target,&next,NULL,&error).status==TABLE_TRANSFORM_APPLIED && b.command_revision==revision+1); analysis_free(&next);
    CHECK(workbook_journal_session_needs_checkpoint(s,&b) && !workbook_journal_session_state(s,&b).covered);
    ObjectCommandContext rename={&b,&epoch,SIZE_MAX,SIZE_MAX,true,false}; ObjectNameTarget name;
    CHECK(workbook_prepare_object_name(&rename,0,b.sheets[0].objects.items[0].id,&name,&error));
    CHECK(workbook_apply_object_name_recorded(&rename,&name,"Renamed Raw",NULL,&error).status==OBJECT_NAME_APPLIED && b.command_revision==revision+2);
    CHECK(workbook_journal_session_needs_checkpoint(s,&b) && !workbook_journal_session_state(s,&b).covered);
    apply(s,&b,1,"After Table transform"); JournalSessionState second=ready(s,&b);
    CHECK(second.parent.high==first.generation.high && second.parent.low==first.generation.low);
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*journal=dx_strdup(workbook_journal_session_journal(s)); uint64_t head=b.command_revision;
    CHECK(workbook_journal_session_close(s,false,&error) && !error); recover(checkpoint,journal,&loaded,head,b.path);
    CHECK(loaded.sheets[0].objects.count==1 && loaded.sheets[0].objects.items[0].source.unique_rows && !strcmp(loaded.sheets[0].objects.items[0].name,"Renamed Raw") && loaded.sheets[0].objects.items[0].id==b.sheets[0].objects.items[0].id &&
        !strcmp(cells_get(&loaded.sheets[1].cells,3,2),"After Table transform"));
    cleanup_generation(checkpoint,journal); CHECK(!rmdir(root)); free(checkpoint); free(journal); workbook_free(&loaded); workbook_free(&b);
}
static void root_failure_and_discard(const char *root) {
    Workbook b; base(&b); char *error=NULL; FILE *file=fopen(root,"wb"); CHECK(file); CHECK(fputs("preserve",file)>=0 && !fclose(file));
    WorkbookJournalSession *s=session(root,options()); CellEditCommand c=command(&b,0,"First"); Workbook before=b;
    CHECK(workbook_journal_session_apply(s,&b,&c,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_LIMIT);
    CHECK(error && !memcmp(&before,&b,sizeof(b)) && !workbook_journal_session_state(s,&b).active); free(error); error=NULL;
    CHECK(!unlink(root)); CHECK(workbook_journal_session_apply(s,&b,&c,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_APPLIED);
    ready(s,&b); char *directory=parent(workbook_journal_session_checkpoint(s)); CHECK(workbook_journal_session_close(s,true,&error)); CHECK(!error && access(directory,F_OK));
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,3,2),"First")); CHECK(!rmdir(root)); free(directory); workbook_free(&b);
}
static void inherited_lineage(const char *root) {
    Workbook b,wrong; base(&b); base(&wrong); char *error=NULL;
    WorkbookJournalSession *s=session(root,options());
    JournalSessionLineage lineage={b.id,{71,92},"relative.dxls"};
    CHECK(!workbook_journal_session_lineage(s,lineage,&error) && error && access(root,F_OK)); free(error); error=NULL;
    char *location=dx_absolute_path("build/tests"); CHECK(location); char *original=dx_printf("%s/recovered original.dxls",location);
    lineage.original_path=original; CHECK(workbook_journal_session_lineage(s,lineage,&error) && !error); memset(original,'x',strlen(original)); free(original);
    CellEditCommand c=command(&wrong,0,"Wrong document"); uint64_t revision=wrong.command_revision;
    CHECK(workbook_journal_session_apply(s,&wrong,&c,(CellEditPolicy){0,0,true},&error).status==CELL_EDIT_LIMIT && error);
    CHECK(wrong.command_revision==revision && !*cells_get(&wrong.sheets[0].cells,3,2) && access(root,F_OK)); free(error); error=NULL;
    apply(s,&b,0,"Recovered edit"); JournalSessionState state=ready(s,&b);
    CHECK(state.parent.high==71 && state.parent.low==92 && !b.path);
    CHECK(!workbook_journal_session_lineage(s,lineage,&error) && error); free(error); error=NULL;
    WorkbookJournal *journal=NULL;
    /* Inspect only after the writer has joined; its exact origin is immutable. */
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(s)),*path=dx_strdup(workbook_journal_session_journal(s));
    CHECK(workbook_journal_session_close(s,false,&error) && !error);
    CHECK(workbook_journal_inspect(&journal,path,&error) && !error);
    const JournalOrigin *origin=workbook_journal_origin(journal);
    original=dx_printf("%s/recovered original.dxls",location); free(location);
    CHECK(origin && origin->parent.high==71 && origin->parent.low==92 && !strcmp(origin->original_path,original)); free(original); workbook_journal_close(journal);
    cleanup_generation(checkpoint,path); CHECK(!rmdir(root)); free(checkpoint); free(path); workbook_free(&wrong); workbook_free(&b);
}
int main(void) {
    char directory[]="build/tests/journal-session-XXXXXX"; CHECK(mkdtemp(directory));
    char *first=dx_printf("%s/first",directory),*rotation=dx_printf("%s/rotation",directory),*failure=dx_printf("%s/failure",directory);
    initial_checkpoint(first); rotation_and_history(rotation); analysis_rotation(rotation); table_rotation(rotation); root_failure_and_discard(failure); inherited_lineage(first);
    CHECK(!rmdir(directory)); free(first); free(rotation); free(failure);
    printf("PASS: %u journal session checks\n",checks); return 0;
}
