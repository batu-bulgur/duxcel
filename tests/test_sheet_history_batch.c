#include "workbook/sheet_history_batch.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/sheet_topology_duplicate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet batch:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void reject(WorkbookSheetBatchView v,WorkbookSheetHistoryBatch *old,WorkbookSheetBatchStatus expected) {
    WorkbookSheetBatchInfo out={.count=17},before=out; WorkbookSheetHistoryBatch *next=old;
    CHECK(workbook_sheet_batch_measure(v,old,&out)==expected && !memcmp(&out,&before,sizeof(out)));
    CHECK(workbook_sheet_batch_decode(v,SIZE_MAX,&next)==expected && next==old);
}
int main(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL));
    CHECK(workbook_prepare_command_session(&b,0,NULL)); uint64_t epoch=9,id=b.sheets[0].id;
    WorkbookVersionOwner owner={b.id.high,b.id.low,44,VERSION_WORKING_HEAD}; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner=owner,.edit_session=b.edit_session_id},(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    WorkbookHistoryEventInfo frames[3]; void *bytes[3]; uint64_t ids[3]; WorkbookHistoryEventPosition positions[3];
    for (size_t i=0;i<3;i++) {
        if (!i) {
            SheetLifecycleCommandContext c={&b,&epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetLifecycleCommandPlan *plan=NULL;
            CHECK(sheet_lifecycle_command_prepare(c,(SheetMetadataCommand){SHEET_METADATA_RENAME,id,0,"Renamed"},&plan,NULL)==SHEET_METADATA_READY);
            WorkbookHistorySheetLifecycleRecorder recorder=workbook_history_stream_sheet_lifecycle_recorder(stream);
            CHECK(sheet_lifecycle_command_apply(c,plan,&recorder,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&plan); ids[i]=id;
        } else {
            SheetTopologyDuplicateContext c={&b,&epoch,&b.sheets[0].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *plan=NULL;
            CHECK(sheet_topology_duplicate_prepare(c,id,&plan,NULL)==SHEET_DUPLICATE_READY); ids[i]=sheet_topology_duplicate_presence(plan).sheet->id;
            CHECK(sheet_topology_duplicate_apply(c,&plan,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED);
        }
        WorkbookHistoryEvent *held=NULL; CHECK(workbook_history_stream_dispatch(stream,&held)==HISTORY_STREAM_OK);
        frames[i]=*workbook_history_event_info(held); bytes[i]=dx_alloc(frames[i].bytes); memcpy(bytes[i],workbook_history_event_payload(held),frames[i].bytes);
        positions[i]=(WorkbookHistoryEventPosition){frames[i].transition,frames[i].lineage,frames[i].position};
        CHECK(workbook_history_stream_complete(stream,&held)==HISTORY_STREAM_OK);
    }
    WorkbookHistorySegment segment={0,0,3}; WorkbookHistoryPath path={.document_high=b.id.high,.document_low=b.id.low,.segments=&segment,.segment_count=1,.events=positions,.event_count=3,.operation_high_water=3,.cursor=1,.tail=3};
    WorkbookSheetHistoryAssignment assignments[3];
    for (size_t i=0;i<3;i++) assignments[i]=(WorkbookSheetHistoryAssignment){i,{frames+i,bytes[i],frames[i].bytes,ids[i],i ? SHEET_DUPLICATE : SHEET_RENAME}};
    WorkbookSheetBatchView view={&path,owner,b.edit_session_id,assignments,3}; WorkbookSheetBatchInfo info;
    CHECK(workbook_sheet_batch_measure(view,NULL,&info)==SHEET_BATCH_OK && info.count==3 && !info.prior_owned);
    WorkbookSheetPayloadInfo parts[3]; size_t sum=0,max=0;
    for (size_t i=0;i<3;i++) { CHECK(workbook_sheet_payload_preflight(assignments[i].payload,parts+i)==SHEET_PAYLOAD_OK); sum+=parts[i].owned; if (parts[i].validation_scratch>max) max=parts[i].validation_scratch; }
    CHECK(info.owned>sum && info.validation_scratch==max && info.staging==info.owned+max);
    WorkbookSheetHistoryBatch *out=NULL;
    CHECK(workbook_sheet_batch_decode(view,info.staging-1,&out)==SHEET_BATCH_LIMIT && !out);
    CHECK(workbook_sheet_batch_decode(view,info.staging,&out)==SHEET_BATCH_OK && workbook_sheet_batch_count(out)==3);
    WorkbookSheetHistoryBatch *old=out; WorkbookSheetBatchInfo replacement;
    CHECK(workbook_sheet_batch_measure(view,out,&replacement)==SHEET_BATCH_OK && replacement.prior_owned==info.owned && replacement.staging==info.staging+info.owned);
    CHECK(workbook_sheet_batch_decode(view,replacement.staging-1,&out)==SHEET_BATCH_LIMIT && out==old);
    CHECK(workbook_sheet_batch_decode(view,info.staging,&out)==SHEET_BATCH_LIMIT && out==old);
    size_t index=99; CHECK(workbook_sheet_batch_index(out,2,&index) && index==2);
    CHECK(!workbook_sheet_batch_index(out,3,&index) && index==2 && !workbook_sheet_batch_at(out,3));
    WorkbookSheetBatchView changed=view; changed.count=0; reject(changed,out,SHEET_BATCH_INVALID);
    changed.count=SIZE_MAX; reject(changed,out,SHEET_BATCH_LIMIT); changed=view;
    assignments[2].index=1; reject(changed,out,SHEET_BATCH_SEQUENCE); assignments[2].index=0; reject(changed,out,SHEET_BATCH_SEQUENCE); assignments[2].index=2;
    path.document_low++; reject(view,out,SHEET_BATCH_ASSOCIATION); path.document_low--;
    frames[2].transition=UINT64_MAX; reject(view,out,SHEET_BATCH_ASSOCIATION); frames[2].transition=3;
    frames[2].encoding=99; reject(view,out,SHEET_BATCH_UNSUPPORTED); frames[2].encoding=2;
    /* Later DXSP name corruption passes framing but fails full semantic closure.
       Earlier owners are never replaced, even after earlier entries validate. */
    SheetTopologyInfo topology; CHECK(sheet_topology_preflight(bytes[2],frames[2].bytes,&topology)==SHEET_TOPOLOGY_OK);
    unsigned char *bad=bytes[2]; size_t at=frames[2].bytes-topology.presence_bytes+DX_SHEET_PRESENCE_HEADER_BYTES; bad[at]^=1;
    CHECK(workbook_sheet_batch_measure(view,out,&replacement)==SHEET_BATCH_OK);
    CHECK(workbook_sheet_batch_decode(view,replacement.staging,&out)==SHEET_BATCH_INVALID && out==old); bad[at]^=1;
    /* A noncontiguous supplied subset is valid; no omitted-slot claim is made. */
    WorkbookSheetHistoryAssignment subset[2]={assignments[0],assignments[2]}; changed=view; changed.assignments=subset; changed.count=2;
    CHECK(workbook_sheet_batch_measure(changed,out,&replacement)==SHEET_BATCH_OK);
    CHECK(workbook_sheet_batch_decode(changed,replacement.staging,&out)==SHEET_BATCH_OK && out!=old && workbook_sheet_batch_count(out)==2);
    CHECK(workbook_sheet_batch_index(out,1,&index) && index==2);
    /* The count policy is explicit, inclusive and never silently truncates. */
    WorkbookSheetHistoryAssignment many[DX_SHEET_HISTORY_BATCH_ENTRIES+1];
    WorkbookHistoryEventInfo many_frames[DX_SHEET_HISTORY_BATCH_ENTRIES+1];
    WorkbookHistoryEventPosition many_positions[DX_SHEET_HISTORY_BATCH_ENTRIES+1];
    for (size_t i=0;i<DX_SHEET_HISTORY_BATCH_ENTRIES+1;i++) {
        many_frames[i]=frames[0]; many_frames[i].previous_position=i; many_frames[i].position=i+1;
        many_frames[i].transition=i+1; many_frames[i].action=i+1; many_frames[i].revision=i+1;
        many_positions[i]=(WorkbookHistoryEventPosition){i+1,0,i+1};
        many[i]=(WorkbookSheetHistoryAssignment){i,assignments[0].payload}; many[i].payload.event=many_frames+i;
    }
    WorkbookHistorySegment bounded_segment={0,0,DX_SHEET_HISTORY_BATCH_ENTRIES+1};
    WorkbookHistoryPath bounded=path; bounded.segments=&bounded_segment; bounded.events=many_positions;
    bounded.event_count=bounded.tail=bounded.operation_high_water=DX_SHEET_HISTORY_BATCH_ENTRIES+1;
    changed=view; changed.path=&bounded; changed.assignments=many; changed.count=DX_SHEET_HISTORY_BATCH_ENTRIES;
    CHECK(workbook_sheet_batch_measure(changed,out,&replacement)==SHEET_BATCH_OK && replacement.count==DX_SHEET_HISTORY_BATCH_ENTRIES);
    CHECK(workbook_sheet_batch_decode(changed,replacement.staging-1,&out)==SHEET_BATCH_LIMIT);
    changed.count++; reject(changed,out,SHEET_BATCH_LIMIT);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); workbook_free(&b);
    for (size_t i=0;i<3;i++) free(bytes[i]); memset(&path,0,sizeof(path)); memset(frames,0,sizeof(frames));
    const WorkbookSheetPayload *metadata=workbook_sheet_batch_at(out,0),*added=workbook_sheet_batch_at(out,1);
    CHECK(!strcmp(workbook_sheet_payload_metadata(metadata)->after.sheets[0].name,"Renamed"));
    CHECK(workbook_sheet_payload_topology(added)->sheet==ids[2] && workbook_sheet_payload_event(added)->transition==3);
    workbook_sheet_batch_free(&out); CHECK(!out && !workbook_sheet_batch_count(out)); workbook_sheet_batch_free(&out); workbook_sheet_batch_free(NULL);
    printf("PASS: %u assigned family5 batch checks\n",checks); return 0;
}
