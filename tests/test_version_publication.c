#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/version_save.h"
#include "workbook/cell_command.h"
#include "workbook/history_after_save.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL real Save publication:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { unsigned captures,prepared,cancelled; bool reject; WorkbookVersionPublicationIntent intent; uintptr_t capture; uint64_t revision; unsigned char selected[32]; const WorkbookHistoryProfile *before; WorkbookHistoryAfterSave *after; } Trace;
static bool capture(void *context,WorkbookSaveProgress p) { Trace *t=context; if (p.phase==SAVE_PHASE_VALIDATE && !p.completed) t->captures++; return true; }
static bool prepare(void *context,const WorkbookVersionPublication *publication,duckdb_connection connection,const WorkbookVersionCoreSet *after,char **error) {
    (void)error; Trace *t=context; t->prepared++;
    CHECK(connection && workbook_version_publication_intent(publication)==t->intent);
    const WorkbookCorePayload *actual=workbook_version_publication_capture(publication),*selected=workbook_version_publication_selected(publication);
    const WorkbookCorePayloadInfo *head=workbook_core_payload_info(actual),*chosen=workbook_core_payload_info(selected);
    const WorkbookVersionIndex *base=workbook_version_core_set_index(workbook_version_publication_base(publication)),*next=workbook_version_core_set_index(after);
    const WorkbookVersionPlan *plan=workbook_version_publication_plan(publication);
    const Workbook *snapshot=workbook_version_publication_snapshot(publication,connection,after);
    CHECK(!workbook_version_publication_snapshot(publication,NULL,after) && !workbook_version_publication_snapshot(publication,connection,workbook_version_publication_base(publication)));
    CHECK(head && snapshot && snapshot->id.high==head->document.high && snapshot->id.low==head->document.low && snapshot->command_revision==workbook_version_publication_revision(publication));
    CHECK(head && chosen && base && next && plan && chosen->encoding==15 && head->document.high==next->document_high && head->document.low==next->document_low);
    CHECK(workbook_version_can_publish(base,plan->owner,plan) && next->saved.present && !memcmp(next->saved.key.bytes,chosen->sha256,32));
    t->capture=(uintptr_t)actual; t->revision=workbook_version_publication_revision(publication); memcpy(t->selected,chosen->sha256,32);
    CHECK(access(workbook_core_payload_path(actual),F_OK)==0);
    if (t->before) {
        WorkbookHistoryNativeStatus adapted=workbook_history_after_save_prepare_publication(t->before,publication,connection,after,
            (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},
            (WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u},(WorkbookHistoryAfterSaveControl){0},&t->after,error);
        if (t->intent==VERSION_PUBLICATION_USER_SAVE) {
            CHECK(adapted==HISTORY_NATIVE_READY && t->after);
            CHECK(!memcmp(workbook_core_payload_info(workbook_history_after_save_selected(t->after))->sha256,t->selected,32));
        } else CHECK(adapted==HISTORY_NATIVE_INVALID && !t->after);
    }
    return !t->reject;
}
static void cancel(void *context,const WorkbookVersionPublication *publication) {
    Trace *t=context; t->cancelled++;
    const WorkbookCorePayload *actual=workbook_version_publication_capture(publication);
    CHECK((uintptr_t)actual==t->capture && access(workbook_core_payload_path(actual),F_OK)==0);
    workbook_history_after_save_free(&t->after);
}
static bool no_publish(void *context,WorkbookSaveProgress progress) { (void)context; return progress.phase!=SAVE_PHASE_PUBLISH; }
static WorkbookVersionSaveControl control(Trace *t) { return (WorkbookVersionSaveControl){.capture={.save={capture,t}},.publication={prepare,cancel,t}}; }
static void committed(WorkbookVersionReceipt *receipt,Trace *t) {
    const WorkbookVersionCommittedPublication *p=workbook_version_receipt_publication(receipt);
    CHECK(p && workbook_version_committed_intent(p)==t->intent && workbook_version_committed_plan(p));
    CHECK((uintptr_t)workbook_version_committed_capture(p)==t->capture && workbook_version_committed_revision(p)==t->revision);
    const WorkbookCorePayloadInfo *selected=workbook_core_payload_info(workbook_version_committed_selected(p));
    CHECK(selected && !memcmp(selected->sha256,t->selected,32) && t->captures==1 && t->prepared==1 && !t->cancelled);
}
static size_t children(const char *path) {
    DxDirectory *directory=NULL; CHECK(dx_directory_open(path,&directory,NULL)==DX_DIRECTORY_OPEN); DxDirectoryEntry entry={0}; DxDirectoryNext next; size_t n=0;
    while ((next=dx_directory_next(directory,&entry,NULL))==DX_DIRECTORY_ENTRY) n++;
    CHECK(next==DX_DIRECTORY_END); dx_directory_close(directory); return n;
}
int main(void) {
    char parent[]="build/tests/version-publication-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/current.dxls",parent),*failed=dx_printf("%s/failed.dxls",parent),*export=dx_printf("%s/export.dxls",parent),*error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Current",&(Analysis){0},&error));
    WorkbookVersionOwner owner={book.id.high,book.id.low,71,VERSION_WORKING_HEAD};
    WorkbookVersionSaveRequest request={.head=&book,.owner=owner,.purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=128u*1024u*1024u};
    WorkbookVersionIndex empty={.document_high=book.id.high,.document_low=book.id.low};
    WorkbookHistoryAnchors anchors={.document_high=book.id.high,.document_low=book.id.low}; WorkbookHistorySegment segment={0,0,0};
    WorkbookHistoryPath empty_path={.document_high=book.id.high,.document_low=book.id.low,.segments=&segment,.segment_count=1};
    WorkbookHistoryProfileView empty_view={.document=book.id,.container_version=16,.core_version=15,.kind=HISTORY_PROFILE_RETAINED,.versions=&empty,.anchors=&anchors,.current={&empty_path,NULL,0}};
    WorkbookHistoryProfile *before=NULL; CHECK(workbook_history_profile_prepare(empty_view,(WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},&before)==HISTORY_PROFILE_OK);
    WorkbookVersionReceipt *receipt=NULL; Trace first={.intent=VERSION_PUBLICATION_USER_SAVE,.before=before};
    CHECK(workbook_version_save(request,control(&first),&receipt,&error)==WORKBOOK_SAVE_COMPLETE); committed(receipt,&first); CHECK(first.revision==0);
    const WorkbookVersionCommittedPublication *p=workbook_version_receipt_publication(receipt); WorkbookVersionCoreSet *base=workbook_version_receipt_take_versions(receipt); CHECK(base && !workbook_version_committed_selected(p));
    WorkbookHistoryProfile *saved_before=NULL;
    CHECK(workbook_history_profile_prepare(*workbook_history_profile_view(workbook_history_after_save_profile(first.after)),(WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},&saved_before)==HISTORY_PROFILE_OK);
    CHECK(workbook_core_payload_info(workbook_version_committed_capture(p))); workbook_history_after_save_free(&first.after); workbook_version_receipt_free(receipt); receipt=NULL;
    CHECK(children(parent)==2); /* Named file + one actually owned root lease. */
    request.base=base; Trace reuse={.intent=VERSION_PUBLICATION_USER_SAVE,.before=saved_before}; CHECK(workbook_version_save(request,control(&reuse),&receipt,&error)==WORKBOOK_SAVE_COMPLETE); committed(receipt,&reuse);
    CHECK(workbook_version_receipt_reused(receipt)); workbook_history_after_save_free(&reuse.after); workbook_version_receipt_free(receipt); receipt=NULL;
    Trace rejected={.intent=VERSION_PUBLICATION_USER_SAVE,.reject=true,.before=saved_before}; request.path=failed;
    CHECK(workbook_version_save(request,control(&rejected),&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && rejected.captures==1 && rejected.prepared==1 && rejected.cancelled==1 && access(failed,F_OK)!=0);
    free(error); error=NULL;
    Trace late={.intent=VERSION_PUBLICATION_USER_SAVE,.before=saved_before}; WorkbookVersionSaveControl cancelled=control(&late); cancelled.archive.core.update=no_publish;
    CHECK(workbook_version_save(request,cancelled,&receipt,&error)==WORKBOOK_SAVE_CANCELLED && !receipt && late.captures==1 && late.prepared==1 && late.cancelled==1 && access(failed,F_OK)!=0);
    free(error); error=NULL;
    Trace tag={.intent=VERSION_PUBLICATION_TAG_SAVE,.before=saved_before}; request.path=path;
    CHECK(workbook_version_tag_save(request,(WorkbookVersionText){"Explicit",8},control(&tag),&receipt,&error)==WORKBOOK_SAVE_COMPLETE); committed(receipt,&tag);
    WorkbookVersionCoreSet *tagged=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); receipt=NULL; workbook_version_core_set_free(base); base=tagged;
    /* Checkpoint has no captured User Save witness and may not request one by
       attaching an arbitrary callback. Existing Saved index remains distinct. */
    request.base=base; request.purpose=VERSION_RECOVERY_CHECKPOINT; request.path=failed; Trace checkpoint={.intent=VERSION_PUBLICATION_CHECKPOINT};
    CHECK(workbook_version_save(request,control(&checkpoint),&receipt,&error)==WORKBOOK_SAVE_FAILED && !checkpoint.prepared && !checkpoint.captures && !receipt);
    free(error); error=NULL;
    CHECK(workbook_version_save(request,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE && !workbook_version_receipt_publication(receipt)); workbook_version_receipt_free(receipt); receipt=NULL;
    Workbook identity; workbook_init(&identity); CHECK(workbook_add(&identity,"Identity",&(Analysis){0},&error)); WorkbookId independent=identity.id; workbook_free(&identity);
    Workbook *copy=workbook_snapshot(&book); CHECK(copy); Workbook fresh=*copy; free(copy); fresh.id=independent; fresh.origin.present=true; fresh.origin.document=book.id;
    const WorkbookVersionIndex *index=workbook_version_core_set_index(base); memcpy(fresh.origin.root,index->saved.key.bytes,32);
    WorkbookVersionSaveRequest outgoing={.head=&fresh,.owner={fresh.id.high,fresh.id.low,99,VERSION_WORKING_HEAD},.purpose=VERSION_USER_SAVE,.path=export,.parent=parent,.reserved_bytes=128u*1024u*1024u};
    Trace exported={.intent=VERSION_PUBLICATION_EXPORT_SAVE,.before=saved_before}; CHECK(workbook_version_export_save(outgoing,control(&exported),&receipt,&error)==WORKBOOK_SAVE_COMPLETE); committed(receipt,&exported); workbook_version_receipt_free(receipt); receipt=NULL;
    outgoing.base=base; CHECK(workbook_version_export_save(outgoing,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt); free(error); error=NULL;
    CHECK(workbook_version_publication_intent(NULL)==VERSION_PUBLICATION_INVALID && workbook_version_committed_intent(NULL)==VERSION_PUBLICATION_INVALID && !workbook_version_receipt_publication(NULL));
    workbook_history_profile_free(&before); workbook_history_profile_free(&saved_before);
    workbook_free(&fresh); workbook_free(&book); workbook_version_core_set_free(base); CHECK(remove(path)==0 && remove(failed)==0 && remove(export)==0 && rmdir(parent)==0); free(path); free(failed); free(export); free(error);
    printf("PASS: %u genuine controller publication ownership checks\n",checks); return 0;
}
