#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/history_native_capture.h"
#include "workbook/version_save.h"
#include "workbook/archive_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL native history capture:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryProfileCaps profile_caps(void) { return (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES}; }
static WorkbookHistoryNativeCaps caps(void) { return (WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u}; }
static bool cancel(void *context,size_t root,uint64_t bytes) { (void)bytes; return root==*(size_t *)context; }
static void unchanged(WorkbookHistoryProfile *profile,WorkbookVersionCoreSet *roots,WorkbookCorePayload *head,WorkbookHistoryCapturedPosition position,
    WorkbookHistoryNativeCaps budget,WorkbookHistoryNativeControl control,WorkbookHistoryNativeCapture *old,WorkbookHistoryNativeStatus expected) {
    WorkbookHistoryProfile *p=profile; WorkbookVersionCoreSet *r=roots; WorkbookCorePayload *h=head; WorkbookHistoryNativeCapture *out=old; char *error=NULL;
    CHECK(workbook_history_native_take(&p,&r,&h,position,budget,control,&out,&error)==expected);
    CHECK(p==profile && r==roots && h==head && out==old); free(error);
}
static void first_save_input(void) {
    char parent[]="build/tests/history-new-document-XXXXXX"; CHECK(mkdtemp(parent));
    Workbook book; workbook_init(&book); char *error=NULL; CHECK(workbook_add(&book,"Unsaved",&(Analysis){0},&error));
    /* AppVersionState.store is NULL before first Save. Build an explicitly
       owned CURRENT empty index, never a default root or a file-format fallback. */
    WorkbookVersionIndex empty={.document_high=book.id.high,.document_low=book.id.low}; WorkbookVersionMetadata *metadata=NULL;
    CHECK(workbook_version_metadata_copy(&empty,0,&metadata,&error)); WorkbookVersionCoreSet *roots=NULL;
    CHECK(workbook_version_core_set_take(&metadata,NULL,0,0,(WorkbookVersionRestoreControl){0},&roots,&error)==VERSION_RESTORE_READY && !metadata && roots);
    WorkbookHistoryAnchors anchors={.document_high=book.id.high,.document_low=book.id.low}; WorkbookHistorySegment segment={0,0,0};
    WorkbookHistoryPath current={.document_high=book.id.high,.document_low=book.id.low,.segments=&segment,.segment_count=1};
    WorkbookHistoryProfileView view={.document=book.id,.container_version=16,.core_version=15,.kind=HISTORY_PROFILE_RETAINED,.versions=workbook_version_core_set_index(roots),.anchors=&anchors,.current={&current,NULL,0}};
    WorkbookHistoryProfile *profile=NULL; CHECK(workbook_history_profile_prepare(view,profile_caps(),&profile)==HISTORY_PROFILE_OK);
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot); WorkbookCorePayload *head=NULL;
    CHECK(workbook_core_payload_capture(snapshot,parent,(WorkbookCorePayloadControl){0},&head,&error)==CORE_PAYLOAD_READY); workbook_free(snapshot); free(snapshot);
    uint64_t revision=77; CHECK(workbook_core_payload_revision(head,&revision) && revision==0);
    WorkbookHistoryCapturedPosition position={{book.id.high,book.id.low,101,VERSION_WORKING_HEAD},0}; WorkbookHistoryNativeMeasure measure;
    CHECK(workbook_history_native_measure(profile,roots,head,position,caps(),NULL,&measure)==HISTORY_NATIVE_READY && !measure.root_bytes && measure.physical_bytes==measure.head_bytes);
    WorkbookHistoryNativeCapture *out=NULL; CHECK(workbook_history_native_take(&profile,&roots,&head,position,caps(),(WorkbookHistoryNativeControl){0},&out,&error)==HISTORY_NATIVE_READY);
    const WorkbookHistoryProtection *protected=workbook_history_profile_protection(workbook_history_native_profile(out));
    const WorkbookVersionIndex *index=workbook_version_core_set_index(workbook_history_native_roots(out));
    CHECK(index && !index->root_count && !index->tag_count && !index->saved.present && !index->generation && !index->saved_generation);
    CHECK(protected && !protected->has_saved && !protected->current_from && !protected->current_cursor && !protected->current_through && !book.path && !book.command_revision);
    /* A real authored first edit before any Save has the same empty base, plus
       its genuine transition. No Save/index/anchor publication is fabricated. */
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,1,1,&target,&error)); WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=position.owner,.edit_session=book.edit_session_id},(WorkbookHistoryStreamOptions){4,4096},&stream)==HISTORY_STREAM_OK);
    CellEditRecorder recorder=workbook_history_stream_cell_recorder(stream); CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"First",5};
    CHECK(workbook_apply_cell_edit_recorded(&book,&command,(CellEditPolicy){.source_ready=true},&recorder,&error).status==CELL_EDIT_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event);
    WorkbookHistoryEventPosition transition={frame->transition,frame->lineage,frame->position}; size_t ordinal=0; segment.through=1;
    current.events=&transition; current.event_count=1; current.operation_high_water=frame->transition; current.cursor=current.tail=1;
    WorkbookHistoryProfileRecord record={.owner=position.owner,.edit_session=book.edit_session_id,.event=frame,.data=workbook_history_event_payload(event),.bytes=frame->bytes,.sheet=book.sheets[0].id,.intent.effects=2};
    roots=workbook_version_core_set_retain(workbook_history_native_roots(out)); CHECK(roots);
    view.versions=workbook_version_core_set_index(roots); view.current=(WorkbookHistoryProfilePath){&current,&ordinal,1}; view.records=&record; view.count=1;
    CHECK(workbook_history_profile_prepare(view,profile_caps(),&profile)==HISTORY_PROFILE_OK);
    CHECK(workbook_history_stream_retry(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,true)==HISTORY_STREAM_OK);
    snapshot=workbook_snapshot(&book); CHECK(snapshot); CHECK(workbook_core_payload_capture(snapshot,parent,(WorkbookCorePayloadControl){0},&head,&error)==CORE_PAYLOAD_READY); workbook_free(snapshot); free(snapshot);
    position.revision=1; CHECK(workbook_history_native_take(&profile,&roots,&head,position,caps(),(WorkbookHistoryNativeControl){0},&out,&error)==HISTORY_NATIVE_READY && !profile && !roots && !head);
    protected=workbook_history_profile_protection(workbook_history_native_profile(out)); index=workbook_version_core_set_index(workbook_history_native_roots(out));
    CHECK(!index->saved.present && !index->root_count && !index->tag_count && !protected->has_saved && protected->current_through==1 && !book.path && book.command_revision==1 && book.dirty);
    CHECK(workbook_core_payload_revision(workbook_history_native_head(out),&revision) && revision==1);
    workbook_history_native_free(&out); workbook_free(&book); free(error); CHECK(rmdir(parent)==0);
}
int main(void) {
    first_save_input();
    char parent[]="build/tests/history-native-XXXXXX"; CHECK(mkdtemp(parent)); char *path=dx_printf("%s/current.dxls",parent),*error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Current",&(Analysis){0},&error));
    WorkbookVersionOwner owner={book.id.high,book.id.low,81,VERSION_WORKING_HEAD}; WorkbookVersionReceipt *receipt=NULL;
    WorkbookVersionSaveRequest save={.head=&book,.owner=owner,.purpose=VERSION_USER_SAVE,.path=path,.parent=parent,.reserved_bytes=256u*1024u*1024u};
    CHECK(workbook_version_save(save,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE);
    /* An actual current16 artifact is admitted by its existing reader. That
       reader owns chunks/hash/current15/document closure; this test does not
       seed a descriptor-only CoreSet or create a second archive decoder. */
    WorkbookArchiveCandidate *archive=NULL;
    CHECK(workbook_archive_read(path,parent,256u*1024u*1024u,(WorkbookArchiveReadControl){0},&archive,&error)==ARCHIVE_READ_READY && workbook_archive_file_version(archive)==16);
    WorkbookVersionCoreSet *roots=workbook_archive_take_versions(archive); workbook_archive_candidate_free(archive); workbook_version_receipt_free(receipt);
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots); CHECK(index && index->root_count==1);
    uint64_t actual=99; CHECK(workbook_core_payload_revision(workbook_version_core_set_leaf(roots,0),&actual) && actual==0);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,2,3,&target,&error));
    WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=owner,.edit_session=book.edit_session_id},(WorkbookHistoryStreamOptions){4,4096},&stream)==HISTORY_STREAM_OK);
    CellEditRecorder recorder=workbook_history_stream_cell_recorder(stream);
    CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"Captured R",10};
    CHECK(workbook_apply_cell_edit_recorded(&book,&command,(CellEditPolicy){.source_ready=true},&recorder,&error).status==CELL_EDIT_APPLIED && book.command_revision==1);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event);
    WorkbookHistoryEventPosition event_position={frame->transition,frame->lineage,frame->position};
    WorkbookHistorySegment current_segment={0,0,1},saved_segment={0,0,0};
    WorkbookHistoryPath current={book.id.high,book.id.low,&current_segment,1,&event_position,1,0,frame->transition,0,0,1,1};
    WorkbookHistoryPath saved={book.id.high,book.id.low,&saved_segment,1,NULL,0,0,0,0,0,0,0};
    WorkbookHistoryAnchor anchor={{0,0,index->saved.key},0};
    WorkbookHistoryAnchors anchors={book.id.high,book.id.low,0,0,&anchor,1,NULL,0}; size_t ordinal=0;
    WorkbookHistoryProfileRecord record={.owner=owner,.edit_session=book.edit_session_id,.event=frame,.data=workbook_history_event_payload(event),.bytes=frame->bytes,.sheet=book.sheets[0].id,.intent.effects=2};
    WorkbookHistoryProfileView view={book.id,16,15,HISTORY_PROFILE_RETAINED,index,&anchors,{&current,&ordinal,1},{&saved,NULL,0},&record,1};
    WorkbookHistoryProfile *profile=NULL; CHECK(workbook_history_profile_prepare(view,profile_caps(),&profile)==HISTORY_PROFILE_OK);
    /* Independent candidate ownership permits explicit test staging discard;
       no retained writer completion/publication is fabricated. */
    CHECK(workbook_history_stream_retry(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,true)==HISTORY_STREAM_OK);
    Workbook *snapshot=workbook_snapshot(&book); CHECK(snapshot && !snapshot->edit_session_id && snapshot->command_revision==1);
    WorkbookCorePayload *head=NULL; CHECK(workbook_core_payload_capture(snapshot,parent,(WorkbookCorePayloadControl){0},&head,&error)==CORE_PAYLOAD_READY);
    workbook_free(snapshot); free(snapshot); CHECK(workbook_core_payload_revision(head,&actual) && actual==1);
    WorkbookHistoryCapturedPosition position={owner,1}; WorkbookHistoryNativeMeasure measured;
    CHECK(workbook_history_native_measure(profile,roots,head,position,caps(),NULL,&measured)==HISTORY_NATIVE_READY && measured.head_bytes && measured.root_bytes && measured.physical_bytes==measured.head_bytes+measured.root_bytes);
    WorkbookHistoryNativeCapture *out=NULL; WorkbookHistoryNativeCaps short_budget=caps(); short_budget.additional_bytes=measured.additional_bytes-1;
    unchanged(profile,roots,head,position,short_budget,(WorkbookHistoryNativeControl){0},NULL,HISTORY_NATIVE_LIMIT);
    short_budget=caps(); short_budget.physical_bytes=measured.physical_bytes-1; unchanged(profile,roots,head,position,short_budget,(WorkbookHistoryNativeControl){0},NULL,HISTORY_NATIVE_LIMIT);
    WorkbookHistoryCapturedPosition wrong=position; wrong.revision=0; unchanged(profile,roots,head,wrong,caps(),(WorkbookHistoryNativeControl){0},NULL,HISTORY_NATIVE_ASSOCIATION);
    wrong=position; wrong.owner.document_low++; unchanged(profile,roots,head,wrong,caps(),(WorkbookHistoryNativeControl){0},NULL,HISTORY_NATIVE_ASSOCIATION);
    wrong=position; wrong.owner.mode=VERSION_HISTORICAL_VIEW; unchanged(profile,roots,head,wrong,caps(),(WorkbookHistoryNativeControl){0},NULL,HISTORY_NATIVE_INVALID);
    size_t phase=SIZE_MAX; unchanged(profile,roots,head,position,caps(),(WorkbookHistoryNativeControl){cancel,&phase},NULL,HISTORY_NATIVE_CANCELLED);
    phase=0; unchanged(profile,roots,head,position,caps(),(WorkbookHistoryNativeControl){cancel,&phase},NULL,HISTORY_NATIVE_CANCELLED);
    CHECK(workbook_history_native_take(&profile,&roots,&head,position,caps(),(WorkbookHistoryNativeControl){0},&out,&error)==HISTORY_NATIVE_READY && !profile && !roots && !head);
    CHECK(workbook_history_native_position(out)->revision==1 && workbook_history_native_position(out)->owner.session==owner.session);
    CHECK(workbook_history_profile_view(workbook_history_native_profile(out))->records[0].event->transition==1);
    /* Live R+1 cannot mutate the owned frozen-R bytes or their revision. */
    CHECK(workbook_prepare_cell_edit(&book,0,2,3,&target,&error)); command=(CellEditCommand){CELL_EDIT_COMMAND_VERSION,target,"Live R+1",8};
    CHECK(workbook_apply_cell_edit(&book,&command,(CellEditPolicy){.source_ready=true},&error).status==CELL_EDIT_APPLIED && book.command_revision==2);
    Workbook restored; workbook_init(&restored); CHECK(workbook_load_core_payload(&restored,workbook_core_payload_path(workbook_history_native_head(out)),&error));
    CHECK(restored.command_revision==1 && !strcmp(cells_get(&restored.sheets[0].cells,2,3),"Captured R")); workbook_free(&restored);
    /* Reuse the genuinely retained CoreSet reference, with a new independently
       owned head and profile. Prior/new wrapper and extent peaks are explicit. */
    roots=workbook_version_core_set_retain(workbook_history_native_roots(out)); CHECK(roots);
    WorkbookHistoryProfileView checkpoint=*workbook_history_profile_view(workbook_history_native_profile(out));
    checkpoint.kind=HISTORY_PROFILE_CHECKPOINT_ONLY; checkpoint.anchors=NULL; checkpoint.current=(WorkbookHistoryProfilePath){0}; checkpoint.saved=(WorkbookHistoryProfilePath){0}; checkpoint.records=NULL; checkpoint.count=0;
    CHECK(workbook_history_profile_prepare(checkpoint,profile_caps(),&profile)==HISTORY_PROFILE_OK);
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&head,&error)==CORE_PAYLOAD_READY); position.revision=2;
    CHECK(workbook_history_native_measure(profile,roots,head,position,caps(),out,&measured)==HISTORY_NATIVE_READY && measured.prior_physical_bytes && measured.additional_bytes>0);
    short_budget=caps(); short_budget.additional_bytes=measured.additional_bytes-1; unchanged(profile,roots,head,position,short_budget,(WorkbookHistoryNativeControl){0},out,HISTORY_NATIVE_LIMIT);
    short_budget=caps(); short_budget.physical_bytes=measured.physical_bytes-1; unchanged(profile,roots,head,position,short_budget,(WorkbookHistoryNativeControl){0},out,HISTORY_NATIVE_LIMIT);
    phase=0; unchanged(profile,roots,head,position,caps(),(WorkbookHistoryNativeControl){cancel,&phase},out,HISTORY_NATIVE_CANCELLED);
    /* A structurally valid advertised descriptor is not physical body proof. */
    WorkbookVersionIndex wrong_index=*checkpoint.versions; WorkbookVersionRoot wrong_root=wrong_index.roots[0]; wrong_root.bytes++;
    wrong_index.roots=&wrong_root; WorkbookHistoryProfileView mismatched=checkpoint; mismatched.versions=&wrong_index;
    WorkbookHistoryProfile *bad_profile=NULL; CHECK(workbook_history_profile_prepare(mismatched,profile_caps(),&bad_profile)==HISTORY_PROFILE_OK);
    unchanged(bad_profile,roots,head,position,caps(),(WorkbookHistoryNativeControl){0},out,HISTORY_NATIVE_ASSOCIATION); workbook_history_profile_free(&bad_profile);
    CHECK(workbook_history_native_take(&profile,&roots,&head,position,(WorkbookHistoryNativeCaps){measured.physical_bytes,measured.additional_bytes},(WorkbookHistoryNativeControl){0},&out,&error)==HISTORY_NATIVE_READY && !profile && !roots && !head);
    CHECK(workbook_history_native_position(out)->revision==2 && !workbook_history_profile_protection(workbook_history_native_profile(out)));
    /* Private sealed bytes are checked again at handoff, not trusted merely
       because their still-owned descriptor previously passed admission. */
    roots=workbook_version_core_set_retain(workbook_history_native_roots(out)); CHECK(roots);
    checkpoint=*workbook_history_profile_view(workbook_history_native_profile(out));
    CHECK(workbook_history_profile_prepare(checkpoint,profile_caps(),&profile)==HISTORY_PROFILE_OK);
    CHECK(workbook_core_payload_capture(&book,parent,(WorkbookCorePayloadControl){0},&head,&error)==CORE_PAYLOAD_READY);
    char *changed=dx_strdup(workbook_core_payload_path(head)),*directory=dx_strdup(workbook_core_payload_directory(head)),*lease=dx_printf("%s/lease.lock",directory);
    FILE *file=fopen(changed,"ab"); CHECK(file && fputc(0,file)!=EOF && fclose(file)==0);
    unchanged(profile,roots,head,position,caps(),(WorkbookHistoryNativeControl){0},out,HISTORY_NATIVE_FAILED);
    workbook_history_profile_free(&profile); workbook_version_core_set_free(roots); roots=NULL;
    /* Existing cleanup refuses externally changed files. Explicitly remove
       only this test's three known private paths after releasing its owner. */
    workbook_core_payload_free(head); head=NULL; CHECK(remove(changed)==0 && remove(lease)==0 && rmdir(directory)==0); free(changed); free(lease); free(directory);
    actual=77; CHECK(!workbook_core_payload_revision(NULL,&actual) && actual==77 && !workbook_core_payload_revision(workbook_history_native_head(out),NULL));
    CHECK(!workbook_history_native_head(NULL) && !workbook_history_native_roots(NULL) && !workbook_history_native_profile(NULL) && !workbook_history_native_position(NULL));
    workbook_history_native_free(&out); CHECK(!out); workbook_history_native_free(&out); workbook_history_native_free(NULL);
    workbook_free(&book); free(error); CHECK(remove(path)==0 && rmdir(parent)==0); free(path);
    printf("PASS: %u native frozen history capture checks\n",checks); return 0;
}
