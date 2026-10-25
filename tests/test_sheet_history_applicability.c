#include "workbook/sheet_history_applicability.h"
#include "workbook/sheet_lifecycle_command.h"
#include "workbook/sheet_topology_duplicate.h"
#include "workbook/sheet_topology_remove.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet applicability:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void rejects(WorkbookSheetApplicability *plan,SheetLifecycleState state,const SheetPresenceView *actual,bool redo,WorkbookSheetApplicabilityStatus expected) {
    WorkbookSheetApplicabilityResult out={.target.high_water=77},saved=out;
    CHECK(workbook_sheet_applicability_check(plan,state,actual,redo,&out)==expected && !memcmp(&out,&saved,sizeof(out)));
}
static WorkbookSheetPayload *decode(WorkbookHistoryStream *stream,uint64_t id,WorkbookSheetOperation operation) {
    WorkbookHistoryEvent *event=NULL; CHECK(workbook_history_stream_dispatch(stream,&event)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(event); WorkbookSheetPayload *payload=NULL;
    CHECK(workbook_sheet_payload_decode((WorkbookSheetPayloadView){info,workbook_history_event_payload(event),info->bytes,id,operation},SIZE_MAX,&payload)==SHEET_PAYLOAD_OK);
    CHECK(workbook_history_stream_complete(stream,&event)==HISTORY_STREAM_OK); return payload;
}
static void pending_observation(void) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Base",&(Analysis){0},NULL));
    CHECK(workbook_add(&b,"CSV",&(Analysis){.kind=SOURCE_CSV,.path="/source",.source_version=1},NULL));
    Worksheet *source=b.sheets+1; SourceVersions next={0}; SourceVersion row={.version=1,.kind=SOURCE_CSV,.path="/source"};
    CHECK(source_versions_plan(&source->source_versions,&row,true,&next,NULL)); source->source_versions=next;
    CHECK(workbook_prepare_command_session(&b,1,NULL)); uint64_t epoch=6,id=source->id; WorkbookHistoryStream *stream=NULL;
    CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,60,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id},(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    SheetTopologyDuplicateContext dc={&b,&epoch,&source->analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *duplicate=NULL;
    CHECK(sheet_topology_duplicate_prepare(dc,id,&duplicate,NULL)==SHEET_DUPLICATE_READY); id=sheet_topology_duplicate_presence(duplicate).sheet->id;
    CHECK(sheet_topology_duplicate_apply(dc,&duplicate,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED);
    WorkbookSheetPayload *payload=decode(stream,id,SHEET_DUPLICATE); WorkbookSheetApplicability *plan=NULL;
    CHECK(workbook_sheet_applicability_prepare(payload,SIZE_MAX,&plan)==SHEET_APPLICABILITY_OK);
    Worksheet *sheet=b.sheets+b.active; SheetPresenceView actual={b.id,b.sheet_high_water,sheet,&sheet->analysis}; SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState state;
    CHECK(sheet_lifecycle_from_workbook(&b,rows,&state)==SHEET_LIFECYCLE_OK); WorkbookSheetApplicabilityResult result;
    CHECK(workbook_sheet_applicability_check(plan,state,&actual,false,&result)==SHEET_APPLICABILITY_OK && result.presence.matches && result.presence.sources.captured_pending==1 && result.presence.sources.exact==1);
    Column column={.name="Amount",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&column,.column_count=1}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1; SourcePacket raw={0};
    CHECK(source_scope_raw_build(0,1,&sheet->analysis,&schema,&digest,"engine",&raw,NULL)); row.raw=raw; next=(SourceVersions){0};
    CHECK(source_versions_plan(&sheet->source_versions,&row,false,&next,NULL)); source_versions_free(&sheet->source_versions); sheet->source_versions=next; source_packet_free(&raw);
    CHECK(workbook_sheet_applicability_check(plan,state,&actual,false,&result)==SHEET_APPLICABILITY_OK && result.presence.sources.refined==1 && result.presence.sources.captured_pending==1);
    /* Compatible current completion never changes the captured observation. */
    const SheetTopologyEvent *event=workbook_sheet_payload_topology(payload); SheetPresenceAsset *captured=NULL;
    CHECK(sheet_presence_decode(event->presence.data,event->presence.bytes,SIZE_MAX,&captured,NULL)==SHEET_PRESENCE_OK && !sheet_presence_view(captured).sheet->source_versions.items[0].raw.bytes);
    CHECK(sheet->analysis.source_version==1 && sheet->source_versions.items[0].raw.bytes);
    /* REMOVE uses the caller's actual detached owner; applicability never
       invents an absent sheet or performs the lifecycle transfer itself. */
    CHECK(workbook_prepare_command_session(&b,b.active,NULL));
    SheetTopologyRemoveContext rc={&b,&epoch,&sheet->analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyRemovePlan *remove=NULL;
    CHECK(sheet_topology_remove_prepare(rc,id,&remove,NULL)==SHEET_REMOVE_READY);
    CHECK(sheet_topology_remove_apply(rc,&remove,stream,SIZE_MAX,NULL)==SHEET_REMOVE_APPLIED);
    WorkbookSheetPayload *removed=decode(stream,id,SHEET_DELETE);
    CHECK(workbook_sheet_applicability_prepare(removed,SIZE_MAX,&plan)==SHEET_APPLICABILITY_OK);
    WorkbookLifecyclePresence selected; CHECK(workbook_lifecycle_presence(&b,false,&selected) && selected.detached && selected.sheet==id);
    actual=(SheetPresenceView){b.id,b.sheet_high_water,selected.worksheet,&selected.worksheet->analysis};
    CHECK(sheet_lifecycle_from_workbook(&b,rows,&state)==SHEET_LIFECYCLE_OK);
    CHECK(workbook_sheet_applicability_check(plan,state,&actual,false,&result)==SHEET_APPLICABILITY_OK && result.target.count==3 && result.presence.sources.exact==1 && !result.presence.sources.captured_pending);
    CHECK(workbook_sheet_applicability_check(plan,result.target,&actual,true,&result)==SHEET_APPLICABILITY_OK && result.target.count==2);
    workbook_sheet_applicability_free(&plan); workbook_sheet_payload_free(&removed); workbook_sheet_payload_free(&payload); sheet_presence_free(&captured);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); workbook_free(&b);
}
int main(void) {
    pending_observation();
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"A",&(Analysis){0},NULL)); CHECK(workbook_add(&b,"B",&(Analysis){0},NULL));
    CHECK(workbook_prepare_command_session(&b,0,NULL)); uint64_t epoch=8,id=b.sheets[0].id;
    WorkbookHistoryStream *stream=NULL; CHECK(workbook_history_stream_new((WorkbookHistoryStreamOrigin){.owner={b.id.high,b.id.low,20,VERSION_WORKING_HEAD},.edit_session=b.edit_session_id},(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_DEFAULT_BYTES},&stream)==HISTORY_STREAM_OK);
    SheetLifecycleCommandContext context={&b,&epoch,SIZE_MAX,true,false,SIZE_MAX}; SheetLifecycleCommandPlan *command=NULL;
    CHECK(sheet_lifecycle_command_prepare(context,(SheetMetadataCommand){SHEET_METADATA_RENAME,id,0,"Renamed"},&command,NULL)==SHEET_METADATA_READY);
    WorkbookHistorySheetLifecycleRecorder recorder=workbook_history_stream_sheet_lifecycle_recorder(stream);
    CHECK(sheet_lifecycle_command_apply(context,command,&recorder,NULL)==SHEET_METADATA_APPLIED); sheet_lifecycle_command_free(&command);
    WorkbookSheetPayload *metadata=decode(stream,id,SHEET_RENAME); WorkbookSheetApplicability *plan=NULL; WorkbookSheetApplicabilityMeasure measure;
    CHECK(workbook_sheet_applicability_measure(metadata,NULL,&measure)==SHEET_APPLICABILITY_OK && !measure.validation_scratch && !measure.prior_owned && measure.owned==measure.staging);
    CHECK(workbook_sheet_applicability_prepare(metadata,measure.staging-1,&plan)==SHEET_APPLICABILITY_LIMIT && !plan);
    CHECK(workbook_sheet_applicability_prepare(metadata,measure.staging,&plan)==SHEET_APPLICABILITY_OK);
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState current; CHECK(sheet_lifecycle_from_workbook(&b,rows,&current)==SHEET_LIFECYCLE_OK);
    current.active=b.sheets[1].id; current.high_water=90; WorkbookSheetApplicabilityResult result;
    CHECK(workbook_sheet_applicability_check(plan,current,NULL,false,&result)==SHEET_APPLICABILITY_OK && result.target.active==current.active && result.target.high_water==90 && !strcmp(result.target.sheets[0].name,"A"));
    CHECK(workbook_sheet_applicability_check(plan,result.target,NULL,true,&result)==SHEET_APPLICABILITY_OK && !strcmp(result.target.sheets[0].name,"Renamed"));
    rejects(plan,current,NULL,true,SHEET_APPLICABILITY_MISMATCH); rows[0].name="Different"; rejects(plan,current,NULL,false,SHEET_APPLICABILITY_MISMATCH); rows[0].name=b.sheets[0].name;
    current.document.low++; rejects(plan,current,NULL,false,SHEET_APPLICABILITY_MISMATCH);
    SheetTopologyDuplicateContext dc={&b,&epoch,&b.sheets[0].analysis,id,SIZE_MAX,true,false,SIZE_MAX}; SheetTopologyDuplicatePlan *duplicate=NULL;
    CHECK(sheet_topology_duplicate_prepare(dc,id,&duplicate,NULL)==SHEET_DUPLICATE_READY); uint64_t added=sheet_topology_duplicate_presence(duplicate).sheet->id;
    CHECK(sheet_topology_duplicate_apply(dc,&duplicate,stream,SIZE_MAX,NULL)==SHEET_DUPLICATE_APPLIED); WorkbookSheetPayload *topology=decode(stream,added,SHEET_DUPLICATE);
    WorkbookSheetApplicability *old=plan; CHECK(workbook_sheet_applicability_measure(topology,plan,&measure)==SHEET_APPLICABILITY_OK && measure.prior_owned && measure.validation_scratch && measure.staging==measure.owned+measure.prior_owned+measure.validation_scratch);
    CHECK(workbook_sheet_applicability_prepare(topology,measure.staging-1,&plan)==SHEET_APPLICABILITY_LIMIT && plan==old);
    CHECK(workbook_sheet_applicability_prepare(topology,measure.staging,&plan)==SHEET_APPLICABILITY_OK && plan!=old);
    Worksheet *sheet=b.sheets+b.active; SheetPresenceView actual={b.id,b.sheet_high_water,sheet,&sheet->analysis};
    CHECK(sheet_lifecycle_from_workbook(&b,rows,&current)==SHEET_LIFECYCLE_OK);
    rejects(plan,current,NULL,false,SHEET_APPLICABILITY_INVALID); SheetPresenceView invalid=actual; invalid.sheet_high_water++; rejects(plan,current,&invalid,false,SHEET_APPLICABILITY_INVALID);
    invalid=actual; invalid.document.low++; rejects(plan,current,&invalid,false,SHEET_APPLICABILITY_INVALID);
    Workbook before=b; Worksheet unchanged=*sheet;
    CHECK(workbook_sheet_applicability_check(plan,current,&actual,false,&result)==SHEET_APPLICABILITY_OK && result.target.count==2 && !result.presence.sources.captured_pending && !memcmp(&before,&b,sizeof(b)) && !memcmp(&unchanged,sheet,sizeof(*sheet)));
    SheetLifecycleState opposite=result.target; CHECK(workbook_sheet_applicability_check(plan,opposite,&actual,true,&result)==SHEET_APPLICABILITY_OK && result.target.count==3);
    sheet->view.row=9000; sheet->view.horizontal=100; sheet->view.canvas_zoom=1.5f; sheet->objects.next_id=UINT64_MAX;
    current.high_water=actual.sheet_high_water=UINT64_MAX;
    CHECK(workbook_sheet_applicability_check(plan,current,&actual,false,&result)==SHEET_APPLICABILITY_OK && result.target.high_water==UINT64_MAX && sheet->objects.next_id==UINT64_MAX);
    CHECK(cells_set(&sheet->cells,1,1,"Changed",NULL)); rejects(plan,current,&actual,false,SHEET_APPLICABILITY_MISMATCH); CHECK(cells_set(&sheet->cells,1,1,"",NULL));
    sheet->view.col=16384; rejects(plan,current,&actual,false,SHEET_APPLICABILITY_INVALID); sheet->view.col=0;
    CHECK(workbook_sheet_applicability_check(plan,current,&actual,false,&result)==SHEET_APPLICABILITY_OK);
    CHECK(workbook_history_stream_free(&stream,false)==HISTORY_STREAM_OK); workbook_free(&b);
    CHECK(workbook_sheet_applicability_check(plan,result.target,NULL,false,&result)==SHEET_APPLICABILITY_INVALID);
    workbook_sheet_applicability_free(&plan); CHECK(!plan); workbook_sheet_applicability_free(&plan); workbook_sheet_applicability_free(NULL);
    workbook_sheet_payload_free(&metadata); workbook_sheet_payload_free(&topology);
    printf("PASS: %u read-only sheet payload applicability checks\n",checks); return 0;
}
