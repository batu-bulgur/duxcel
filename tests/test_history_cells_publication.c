#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "workbook/history_cells_publication.h"
#include "workbook/history_stream.h"
#include "platform/directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL actual cell publication:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { const WorkbookHistoryProfile *before; WorkbookHistoryAfterSave *after; WorkbookHistoryCellsStateStatus expected; unsigned calls,cancels,captures; } Probe;
static bool capture(void *context,WorkbookSaveProgress progress) { Probe *p=context; if (progress.phase==SAVE_PHASE_VALIDATE && !progress.completed) p->captures++; return true; }
static void cancel(void *context,const WorkbookVersionPublication *publication) { (void)publication; Probe *p=context; p->cancels++; workbook_history_after_save_free(&p->after); }
static bool prepare(void *context,const WorkbookVersionPublication *publication,duckdb_connection c,const WorkbookVersionCoreSet *after,char **error) {
    Probe *p=context; p->calls++;
    CHECK(workbook_history_after_save_prepare_publication(p->before,publication,c,after,
        (WorkbookHistoryProfileCaps){4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES},
        (WorkbookHistoryNativeCaps){256u*1024u*1024u,1024u*1024u},(WorkbookHistoryAfterSaveControl){0},&p->after,error)==HISTORY_NATIVE_READY);
    WorkbookHistoryCellStateEntry scratch[1]; WorkbookHistoryCellsStateReport report={9,8,7,6},old=report;
    CHECK(workbook_history_cells_state_publication(publication,NULL,after,p->after,scratch,sizeof(scratch),&report)==HISTORY_CELLS_STATE_INVALID && !memcmp(&report,&old,sizeof(report)));
    CHECK(workbook_history_cells_state_publication(publication,c,workbook_version_publication_base(publication),p->after,scratch,sizeof(scratch),&report)==HISTORY_CELLS_STATE_INVALID);
    CHECK(workbook_history_cells_state_publication(publication,c,after,p->after,scratch,sizeof(scratch)-1,&report)==HISTORY_CELLS_STATE_LIMIT && !memcmp(&report,&old,sizeof(report)));
    WorkbookHistoryCellsStateStatus status=workbook_history_cells_state_publication(publication,c,after,p->after,scratch,sizeof(scratch),&report);
    CHECK(status==p->expected);
    if (status!=HISTORY_CELLS_STATE_OK) {
        CHECK(!memcmp(&report,&old,sizeof(report)));
        if (error && !*error) *error=dx_strdup("Retained current cell state does not match the actual frozen publication.");
        return false;
    }
    CHECK(report.undo==1 && !report.redo && report.undo_cells==1); return true;
}
int main(void) {
    char parent[]="build/tests/history-cells-publication-XXXXXX"; CHECK(mkdtemp(parent));
    char *basepath=dx_printf("%s/base.dxls",parent),*path=dx_printf("%s/current.dxls",parent),*rejected=dx_printf("%s/rejected.dxls",parent),*error=NULL;
    Workbook book; workbook_init(&book); CHECK(workbook_add(&book,"Current",&(Analysis){0},&error));
    WorkbookVersionOwner owner={book.id.high,book.id.low,81,VERSION_WORKING_HEAD}; WorkbookVersionReceipt *receipt=NULL;
    WorkbookVersionSaveRequest save={.head=&book,.owner=owner,.purpose=VERSION_USER_SAVE,.path=basepath,.parent=parent,.reserved_bytes=256u*1024u*1024u};
    CHECK(workbook_version_save(save,(WorkbookVersionSaveControl){0},&receipt,&error)==WORKBOOK_SAVE_COMPLETE);
    WorkbookVersionCoreSet *roots=workbook_version_receipt_take_versions(receipt); workbook_version_receipt_free(receipt); receipt=NULL;
    CellEditTarget target; CHECK(workbook_prepare_cell_edit(&book,0,2,3,&target,&error));
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=owner,.edit_session=book.edit_session_id},(WorkbookHistoryStreamOptions){4,4096},&stream)==HISTORY_STREAM_OK);
    CellEditRecorder recorder=workbook_history_stream_cell_recorder(stream); CellEditCommand command={CELL_EDIT_COMMAND_VERSION,target,"Captured R",10};
    CHECK(workbook_apply_cell_edit_recorded(&book,&command,(CellEditPolicy){.source_ready=true},&recorder,&error).status==CELL_EDIT_APPLIED);
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK); WorkbookHistoryEventInfo frame=*workbook_history_event_info(event);
    WorkbookHistoryEventPosition position={frame.transition,frame.lineage,frame.position}; WorkbookHistorySegment current_segment={0,0,1},saved_segment={0};
    WorkbookHistoryPath current={book.id.high,book.id.low,&current_segment,1,&position,1,0,frame.transition,0,0,1,1},saved={book.id.high,book.id.low,&saved_segment,1,NULL,0,0,0,0,0,0,0};
    const WorkbookVersionIndex *index=workbook_version_core_set_index(roots); WorkbookHistoryAnchor anchor={{0,0,index->saved.key},0}; WorkbookHistoryAnchors anchors={book.id.high,book.id.low,0,0,&anchor,1,NULL,0}; size_t ordinal=0;
    WorkbookHistoryProfileRecord record={.owner=owner,.edit_session=book.edit_session_id,.event=&frame,.data=workbook_history_event_payload(event),.bytes=frame.bytes,.sheet=book.sheets[0].id,.intent.effects=2};
    WorkbookHistoryProfileView view={book.id,16,15,HISTORY_PROFILE_RETAINED,index,&anchors,{&current,&ordinal,1},{&saved,NULL,0},&record,1}; WorkbookHistoryProfile *before=NULL;
    WorkbookHistoryProfileCaps caps={4096,4u*1024u*1024u,DX_HISTORY_PROFILE_OWNED_BYTES,DX_HISTORY_PROFILE_STAGING_BYTES};
    CHECK(workbook_history_profile_prepare(view,caps,&before)==HISTORY_PROFILE_OK);
    save.base=roots; save.path=path; Probe p={.before=before,.expected=HISTORY_CELLS_STATE_OK};
    WorkbookVersionSaveControl control={.capture={.save={capture,&p}},.publication={prepare,cancel,&p}};
    CHECK(workbook_version_save(save,control,&receipt,&error)==WORKBOOK_SAVE_COMPLETE && receipt && !error && p.calls==1 && p.captures==1 && !p.cancels);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,2,3),"Captured R") && book.command_revision==1);
    workbook_history_after_save_free(&p.after); workbook_version_receipt_free(receipt); receipt=NULL;
    /* Valid current framing, physical closure and after-Save metadata alone do
       not establish applicability: wrong latest-after must reject pre-COMMIT. */
    CellDelta wrong={book.sheets[0].id,2,3,"","Wrong body",0,10}; size_t n=0,written=0; CHECK(cell_delta_encoded_size(wrong,&n)==CELL_DELTA_CODEC_OK); void *data=dx_alloc(n);
    CHECK(cell_delta_encode(wrong,data,n,&written)==CELL_DELTA_CODEC_OK); frame.bytes=n; record.data=data; record.bytes=n;
    WorkbookHistoryProfile *bad=NULL; CHECK(workbook_history_profile_prepare(view,caps,&bad)==HISTORY_PROFILE_OK);
    p=(Probe){.before=bad,.expected=HISTORY_CELLS_STATE_MISMATCH}; save.path=rejected;
    CHECK(workbook_version_save(save,control,&receipt,&error)==WORKBOOK_SAVE_FAILED && !receipt && error && p.calls==1 && p.captures==1 && p.cancels==1 && !p.after && access(rejected,F_OK)!=0);
    CHECK(!strcmp(cells_get(&book.sheets[0].cells,2,3),"Captured R") && book.command_revision==1);
    free(error); error=NULL; free(data); workbook_history_profile_free(&bad); workbook_history_profile_free(&before);
    CHECK(workbook_history_stream_retry(stream,&event)==HISTORY_STREAM_OK && workbook_history_stream_free(&stream,true)==HISTORY_STREAM_OK);
    workbook_version_core_set_free(roots); workbook_free(&book); CHECK(!unlink(basepath) && !unlink(path) && !rmdir(parent)); free(basepath); free(path); free(rejected);
    printf("PASS: %u actual frozen-R cell publication checks\n",checks); return 0;
}
