#ifndef DUXCEL_TEST_HISTORY_NATIVE_FIXTURE_H
#define DUXCEL_TEST_HISTORY_NATIVE_FIXTURE_H
/* Genuine runtime producers and independent owners for native controller
   assertions. No standalone workbook/SQL fixture generation binary. */
static WorkbookHistoryNativeCapture *native_fixture(const char *parent,Workbook *book,char **saved_path,char **error) {
    workbook_init(book); CHECK(workbook_add(book,"Current",&(Analysis){0},error));
    WorkbookVersionOwner owner={book->id.high,book->id.low,81,VERSION_WORKING_HEAD};
    *saved_path=dx_printf("%s/current.dxls",parent); WorkbookVersionReceipt *receipt=NULL;
    WorkbookVersionSaveRequest save={.head=book,.owner=owner,.purpose=VERSION_USER_SAVE,.path=*saved_path,.parent=parent,.reserved_bytes=256u*1024u*1024u};
    CHECK(workbook_version_save(save,(WorkbookVersionSaveControl){0},&receipt,error)==WORKBOOK_SAVE_COMPLETE); workbook_version_receipt_free(receipt);
    WorkbookArchiveCandidate *archive=NULL; CHECK(workbook_archive_read(*saved_path,parent,256u*1024u*1024u,(WorkbookArchiveReadControl){0},&archive,error)==ARCHIVE_READ_READY);
    WorkbookVersionCoreSet *roots=workbook_archive_take_versions(archive); workbook_archive_candidate_free(archive);
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(book,0,2,3,&target,error));
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=owner,.edit_session=book->edit_session_id},(WorkbookHistoryStreamOptions){4,4096},&stream)==HISTORY_STREAM_OK);
    CellEditRecorder recorder=workbook_history_stream_cell_recorder(stream); CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"Captured R",10};
    CHECK(workbook_apply_cell_edit_recorded(book,&command,(CellEditPolicy){.source_ready=true},&recorder,error).status==CELL_EDIT_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK); const WorkbookHistoryEventInfo *frame=workbook_history_event_info(event);
    WorkbookHistoryEventPosition event_position={frame->transition,frame->lineage,frame->position}; WorkbookHistorySegment current_segment={0,0,1},saved_segment={0,0,0};
    WorkbookHistoryPath current={book->id.high,book->id.low,&current_segment,1,&event_position,1,0,frame->transition,0,0,1,1},saved={book->id.high,book->id.low,&saved_segment,1,NULL,0,0,0,0,0,0,0};
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots); WorkbookHistoryAnchor anchor={{0,0,index->saved.key},0}; WorkbookHistoryAnchors anchors={book->id.high,book->id.low,0,0,&anchor,1,NULL,0}; size_t ordinal=0;
    WorkbookHistoryProfileRecord record={.owner=owner,.edit_session=book->edit_session_id,.event=frame,.data=workbook_history_event_payload(event),.bytes=frame->bytes,.sheet=book->sheets[0].id,.intent.effects=2};
    WorkbookHistoryProfileView view={book->id,16,15,HISTORY_PROFILE_RETAINED,index,&anchors,{&current,&ordinal,1},{&saved,NULL,0},&record,1}; WorkbookHistoryProfile *profile=NULL;
    CHECK(workbook_history_profile_prepare(view,(WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},&profile)==HISTORY_PROFILE_OK);
    CHECK(workbook_history_stream_retry(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,true)==HISTORY_STREAM_OK);
    Workbook *snapshot=workbook_snapshot(book); CHECK(snapshot); WorkbookCorePayload *head=NULL;
    CHECK(workbook_core_payload_capture(snapshot,parent,(WorkbookCorePayloadControl){0},&head,error)==CORE_PAYLOAD_READY); workbook_free(snapshot); free(snapshot);
    WorkbookHistoryNativeCapture *capture=NULL;
    CHECK(workbook_history_native_take(&profile,&roots,&head,(WorkbookHistoryCapturedPosition){owner,1},(WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u},(WorkbookHistoryNativeControl){0},&capture,error)==HISTORY_NATIVE_READY);
    return capture;
}
#endif
