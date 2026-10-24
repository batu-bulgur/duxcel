#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/journal_session.h"
#include "platform/platform.h"
#include "workbook/version_save.h"
#include "workbook/archive_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL version checkpoint:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void ready(WorkbookJournalSession *session,const Workbook *book) {
    double deadline=dx_millis()+15000;
    for (;;) {
        workbook_journal_session_poll(session); JournalSessionState state=workbook_journal_session_state(session,book);
        CHECK(!state.queue.failed);
        if (state.queue.prepared && !state.queue.opening && !state.queue.pending && state.queue.durable_revision==book->command_revision) return;
        CHECK(dx_millis()<deadline); dx_sleep_ms(1);
    }
}
static void inspect(const char *path,const char *parent,const char *head,const char *saved,uint64_t generation) {
    char *error=NULL; WorkbookArchiveCandidate *candidate=NULL;
    CHECK(workbook_archive_read(path,parent,UINT64_C(128)*1024*1024,(WorkbookArchiveReadControl){0},&candidate,&error)==ARCHIVE_READ_READY && !error);
    const Workbook *book=workbook_archive_book(candidate); CHECK(!strcmp(cells_get(&book->sheets[0].cells,0,0),head));
    const WorkbookVersionCoreSet *set=workbook_archive_versions(candidate); const WorkbookVersionIndex *index=workbook_version_core_set_index(set);
    CHECK(index && index->saved.present && index->generation==generation && index->saved_generation==generation);
    size_t i=0; while (i<index->root_count && memcmp(index->roots[i].key.bytes,index->saved.key.bytes,DX_VERSION_KEY_BYTES)) i++;
    CHECK(i<index->root_count); Workbook b; workbook_init(&b);
    CHECK(workbook_load_core_payload(&b,workbook_core_payload_path(workbook_version_core_set_leaf(set,i)),&error) && !error);
    CHECK(!strcmp(cells_get(&b.sheets[0].cells,0,0),saved)); workbook_free(&b); workbook_archive_candidate_free(candidate);
}
int main(void) {
    char directory[]="build/tests/version-checkpoint-XXXXXX"; CHECK(mkdtemp(directory));
    char *parent=dx_printf("%s/versions",directory),*root=dx_printf("%s/recovery",directory),*path=dx_printf("%s/named.dxls",directory);
    CHECK(dx_make_directories(parent)); Workbook book; workbook_init(&book); char *error=NULL;
    CHECK(workbook_add(&book,"Checkpoint",&(Analysis){0},&error)); CHECK(cells_set(&book.sheets[0].cells,0,0,"Saved R",&error)); worksheet_record(&book.sheets[0],EDIT_CELLS);
    WorkbookVersionOwner owner={book.id.high,book.id.low,482,VERSION_WORKING_HEAD};
    WorkbookVersionSaveRequest request={&book,NULL,owner,VERSION_USER_SAVE,path,parent,UINT64_C(128)*1024*1024};
    WorkbookVersionReceipt *receipt=NULL; CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    WorkbookVersionCoreSet *base=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Working R+1",&error)); worksheet_record(&book.sheets[0],EDIT_CELLS); book.command_revision=1; book.dirty=true;
    WorkbookJournalSession *session=workbook_journal_session_new(root,(JournalQueueOptions){.entries=32,.bytes=DX_JOURNAL_BATCH_BYTES,.batch=32},&error); CHECK(session && !error);
    JournalCheckpointVersions binding={.base=base,.owner=owner,.parent=parent,.reserved_bytes=request.reserved_bytes};
    JournalCheckpointVersions invalid=binding; invalid.owner.session=0;
    CHECK(!workbook_journal_session_capture_versions(session,&book,invalid,&error) && error && access(root,F_OK)); free(error); error=NULL;
    CHECK(workbook_journal_session_capture_versions(session,&book,binding,&error));
    ready(session,&book); JournalSessionState before=workbook_journal_session_state(session,&book);
    CHECK(workbook_journal_session_capture_versions(session,&book,binding,&error));
    JournalSessionState after=workbook_journal_session_state(session,&book); CHECK(before.generation.high==after.generation.high && before.generation.low==after.generation.low);
    char *checkpoint=dx_strdup(workbook_journal_session_checkpoint(session)); inspect(checkpoint,parent,"Working R+1","Saved R",1);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,0,0,&target,&error));
    CellEditResult applied=workbook_journal_session_apply(session,&book,&(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Working R+2",11},(CellEditPolicy){0,0,true},&error);
    if (applied.status!=CELL_EDIT_APPLIED) fprintf(stderr,"Checkpoint edit status=%d: %s\n",applied.status,error ? error : "no error");
    CHECK(applied.status==CELL_EDIT_APPLIED);
    ready(session,&book); inspect(checkpoint,parent,"Working R+1","Saved R",1);
    request.base=base; receipt=NULL; CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !error);
    WorkbookVersionCoreSet *next=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt);
    CHECK(cells_set(&book.sheets[0].cells,0,0,"Newer R+3",&error)); worksheet_record(&book.sheets[0],EDIT_CELLS); book.command_revision++;
    binding.base=next; CHECK(workbook_journal_session_capture_versions(session,&book,binding,&error));
    workbook_version_core_set_free(base); workbook_version_core_set_free(next); base=next=NULL;
    ready(session,&book); inspect(workbook_journal_session_checkpoint(session),parent,"Newer R+3","Working R+2",2);
    CHECK(workbook_journal_session_close(session,true,&error) && !error); inspect(path,parent,"Working R+2","Working R+2",2);
    CHECK(access(checkpoint,F_OK)); free(checkpoint); CHECK(!unlink(path) && !rmdir(parent) && !rmdir(root) && !rmdir(directory));
    workbook_free(&book); free(path); free(parent); free(root);
    printf("PASS: %u versioned recovery checkpoint checks\n",checks); return 0;
}
