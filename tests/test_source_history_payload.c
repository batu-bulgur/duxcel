#include "workbook/source_history_payload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source payload:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SourcePacket raw(uint64_t owner,uint64_t version,SourceKind kind,unsigned salt) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page p={.columns=&column,.column_count=1}; Analysis a={.kind=kind,.path="/source"}; SourceDigest d={.bytes=42,.identity={.size=42}}; d.sha256[0]=(unsigned char)salt;
    SourcePacket packet={0}; CHECK(source_scope_raw_build(owner,version,&a,&p,&d,"engine",&packet,NULL)); return packet;
}
static SourceVersions ledger(void) {
    SourceVersions v={.high_water=20,.count=3,.capacity=3,.items=dx_alloc(3*sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){0,1,SOURCE_CSV,dx_strdup("/one.csv"),{0}};
    v.items[1]=(SourceVersion){9,2,SOURCE_PARQUET,dx_strdup("/two.parquet"),raw(9,2,SOURCE_PARQUET,1)};
    v.items[2]=(SourceVersion){10,3,SOURCE_CSV,dx_strdup("/style.csv"),raw(10,3,SOURCE_CSV,1)};
    v.bytes=3*sizeof(SourceVersion); for (size_t i=0;i<3;i++) v.bytes+=strlen(v.items[i].path)+1+v.items[i].raw.bytes;
    CHECK(source_versions_valid(&v,10,NULL)); return v;
}
static WorkbookHistoryStream *stream(void) {
    WorkbookHistoryStreamOrigin origin={.owner={1,2,99,VERSION_WORKING_HEAD},.edit_session=44,.command_revision=100,.cursor=5,.tail=7,.lineage=2,
        .action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){2,DX_HISTORY_STREAM_BYTES},&s)==HISTORY_STREAM_OK); return s;
}
static WorkbookSourcePayloadView dispatched(WorkbookHistoryStream *s,WorkbookHistoryEvent **event,unsigned effects) {
    CHECK(workbook_history_stream_dispatch(s,event)==HISTORY_STREAM_OK && *event);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(*event);
    return (WorkbookSourcePayloadView){info,workbook_history_event_payload(*event),workbook_history_event_dependencies(*event),info->bytes,info->dependency_bytes,7,effects};
}
static void finished(WorkbookHistoryStream **s,WorkbookHistoryEvent **event) {
    CHECK(workbook_history_stream_complete(*s,event)==HISTORY_STREAM_OK && !*event);
    CHECK(workbook_history_stream_free(s,false)==HISTORY_STREAM_OK && !*s);
}
static void denied(WorkbookSourcePayloadView v,SourceDependencyLive live,size_t reserve,SourceDependencyStatus expected,SourceDependencyAdmission *out) {
    SourceDependencyAdmission prior=*out;
    CHECK(workbook_source_payload_admit(v,live,reserve,out)==expected && !memcmp(&prior,out,sizeof(prior)));
}
static void common_rejections(WorkbookSourcePayloadView good,SourceDependencyLive live,const WorkbookSourcePayloadInfo *info,SourceDependencyAdmission *out) {
    denied(good,live,info->staging-1,SOURCE_DEPENDENCY_LIMIT,out);
    WorkbookSourcePayloadView bad=good; bad.sheet++;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out); bad=good; bad.effects^=4;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out); bad=good; bad.bytes--;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out); bad=good; bad.sheet=0;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out);
    WorkbookHistoryEventInfo frame=*good.event; bad=good; bad.event=&frame; frame.transition=0;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out); frame=*good.event; frame.encoding++;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_UNSUPPORTED,out); frame=*good.event; frame.family=99;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_UNSUPPORTED,out);
    unsigned char *copy=dx_alloc(good.bytes); memcpy(copy,good.data,good.bytes); bad=good; bad.data=copy; copy[0]^=1;
    denied(bad,live,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,out); free(copy);
    WorkbookSourcePayloadInfo prior=*info,measure=prior; bad=good; bad.bytes--;
    CHECK(workbook_source_payload_preflight(bad,&measure)==SOURCE_PAYLOAD_INVALID && !memcmp(&measure,&prior,sizeof(prior)));
}
static void format_frame(void) {
    SourceVersions live=ledger(); FormatStore before={0},after={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    FormatScopeView scope={10,live.items[2].raw.data,live.items[2].raw.bytes}; uint64_t id=1; size_t field=0;
    CHECK(format_apply_records(&after,&scope,&id,1,&field,1,FORMAT_BOLD,bold,NULL));
    WorkbookHistoryStream *s=stream(); WorkbookHistoryFormatRecorder recorder=workbook_history_stream_format_recorder(s);
    WorkbookHistoryFormatRecord record={.workbook={1,2},.edit_session=44,.sheet_session=88,.sheet=7,.expected_revision=100,.revision=101,.effects=8,
        .before=format_delta_store_view(&before),.after=format_delta_store_view(&after),.sources=&live,.object_high_water=10};
    void *ticket=NULL; CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL) && ticket); recorder.publish(recorder.context,ticket);
    WorkbookHistoryEvent *event=NULL; WorkbookSourcePayloadView v=dispatched(s,&event,8); WorkbookSourcePayloadInfo info={0};
    CHECK(workbook_source_payload_preflight(v,&info)==SOURCE_PAYLOAD_OK && info.staging==info.owned+info.validation_scratch && info.validation_scratch);
    SourceDependencyLive current={7,10,&live}; SourceDependencyAdmission out={0};
    CHECK(workbook_source_payload_admit(v,current,info.staging,&out)==SOURCE_DEPENDENCY_EXACT && out.sources.exact==1 && !out.sources.captured_pending);
    common_rejections(v,current,&info,&out);
    WorkbookHistoryEventInfo wrong=*v.event; wrong.inactive_scopes++; WorkbookSourcePayloadView bad=v; bad.event=&wrong;
    denied(bad,current,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,&out);
    bad=v; bad.dependencies=NULL; denied(bad,current,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,&out);
    /* A different valid RAW in DXSV passes framing but cannot close this
       actual record scope. This tests full admission, not header coincidence. */
    SourceVersionsAsset changed={0}; CHECK(source_versions_asset_decode(v.dependencies,v.dependency_bytes,10,SIZE_MAX,&changed)==SOURCE_VERSIONS_CODEC_OK);
    size_t old_bytes=changed.versions.items[0].raw.bytes; source_packet_free(&changed.versions.items[0].raw);
    changed.versions.items[0].raw=raw(10,3,SOURCE_CSV,8);
    changed.versions.bytes=changed.versions.bytes-old_bytes+changed.versions.items[0].raw.bytes;
    size_t changed_bytes=0,written=0; CHECK(source_versions_asset_size(7,&changed.versions,10,&changed_bytes)==SOURCE_VERSIONS_CODEC_OK && changed_bytes==v.dependency_bytes);
    void *changed_wire=dx_alloc(changed_bytes); CHECK(source_versions_asset_encode(7,&changed.versions,10,changed_wire,changed_bytes,&written)==SOURCE_VERSIONS_CODEC_OK);
    bad=v; bad.dependencies=changed_wire; WorkbookSourcePayloadInfo framing={0};
    CHECK(workbook_source_payload_preflight(bad,&framing)==SOURCE_PAYLOAD_OK);
    denied(bad,current,framing.staging,SOURCE_DEPENDENCY_INVALID,&out);
    free(changed_wire); source_versions_asset_free(&changed);
    SourcePacket proof=live.items[2].raw; live.items[2].raw=(SourcePacket){0}; live.bytes-=proof.bytes;
    CHECK(workbook_source_payload_admit(v,current,info.staging,&out)==SOURCE_DEPENDENCY_UNAVAILABLE && out.sources.unavailable==1);
    live.items[2].raw=raw(10,3,SOURCE_CSV,2); live.bytes+=live.items[2].raw.bytes;
    CHECK(workbook_source_payload_admit(v,current,info.staging,&out)==SOURCE_DEPENDENCY_CONFLICT && out.sources.conflicting==1);
    source_packet_free(&live.items[2].raw); live.items[2].raw=proof;
    SourceDependencyLive other=current; other.sheet=8;
    CHECK(workbook_source_payload_admit(v,other,info.staging,&out)==SOURCE_DEPENDENCY_CONFLICT);
    finished(&s,&event); format_free(&before); format_free(&after); source_versions_free(&live);
}
static void definition_frame(unsigned effects,bool unbound) {
    SourceVersions live=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.source_version=unbound ? 0 : 1},b=a; b.query="SELECT Id FROM input";
    SheetObject old={.id=9,.kind=OBJECT_TABLE,.name="Table",.row=20,.rows=5,.columns=4,.source={.kind=SOURCE_PARQUET,.path="/two.parquet",.source_version=2}},next=old; next.name="Renamed Table";
    ObjectSnapshot objects_a={&old,1},objects_b={&next,1}; FormatStore before={0},after={0};
    if (effects&8) { CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; FormatScopeView scope={10,live.items[2].raw.data,live.items[2].raw.bytes}; uint64_t id=1; size_t field=0; CHECK(format_apply_records(&after,&scope,&id,1,&field,1,FORMAT_BOLD,bold,NULL)); }
    DefinitionEventView view={.sheet=7,.object_high_water=10,.effects=effects,
        .before={(effects&1) ? &a : NULL,(effects&4) ? &objects_a : NULL},.after={(effects&1) ? &b : NULL,(effects&4) ? &objects_b : NULL},
        .before_formats=format_delta_store_view(&before),.after_formats=format_delta_store_view(&after),.sources=&live};
    WorkbookHistoryStream *s=stream(); WorkbookHistoryDefinitionRecorder recorder=workbook_history_stream_definition_recorder(s);
    WorkbookHistoryDefinitionRecord record={.workbook={1,2},.edit_session=44,.sheet_session=88,.expected_revision=100,.revision=101,.view=view,.validation_reserved=SIZE_MAX};
    void *ticket=NULL; CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL) && ticket); recorder.publish(recorder.context,ticket);
    WorkbookHistoryEvent *event=NULL; WorkbookSourcePayloadView v=dispatched(s,&event,effects); WorkbookSourcePayloadInfo info={0};
    CHECK(workbook_source_payload_preflight(v,&info)==SOURCE_PAYLOAD_OK && info.effects==effects && info.sheet==7);
    SourceDependencyLive current={7,10,&live}; SourceDependencyAdmission out={0};
    CHECK(workbook_source_payload_admit(v,current,info.staging,&out)==SOURCE_DEPENDENCY_EXACT && out.unbound_sources==(unbound ? 2u : 0u));
    CHECK(out.sources.exact==((effects&4) ? 1u : !unbound ? 1u : 0u)+((effects&8) ? 1u : 0u) && out.sources.captured_pending==((effects&1) && !unbound ? 1u : 0u));
    common_rejections(v,current,&info,&out);
    WorkbookHistoryEventInfo wrong=*v.event; wrong.inactive_scopes++; WorkbookSourcePayloadView bad=v; bad.event=&wrong;
    denied(bad,current,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,&out);
    wrong=*v.event; wrong.dependency_encoding=1; denied(bad,current,SIZE_MAX,SOURCE_DEPENDENCY_INVALID,&out);
    /* Header observation counts are readable framing, not semantic proof. */
    unsigned char *wrong_observation=dx_alloc(v.bytes); memcpy(wrong_observation,v.data,v.bytes); wrong_observation[56]^=1;
    bad=v; bad.data=wrong_observation; WorkbookSourcePayloadInfo framing={0};
    CHECK(workbook_source_payload_preflight(bad,&framing)==SOURCE_PAYLOAD_OK);
    denied(bad,current,framing.staging,SOURCE_DEPENDENCY_INVALID,&out); free(wrong_observation);
    live.items[0].raw=raw(0,1,SOURCE_CSV,1); live.bytes+=live.items[0].raw.bytes;
    CHECK(workbook_source_payload_admit(v,current,info.staging,&out)==((effects&1) && !unbound ? SOURCE_DEPENDENCY_REFINED : SOURCE_DEPENDENCY_EXACT));
    CHECK(out.sources.captured_pending==((effects&1) && !unbound ? 1u : 0u) && out.unbound_sources==(unbound ? 2u : 0u));
    finished(&s,&event); format_free(&before); format_free(&after); source_versions_free(&live);
}
int main(void) { format_frame(); definition_frame(1,false); definition_frame(4,false); definition_frame(9,false); definition_frame(12,false); definition_frame(1,true); printf("PASS: %u actual-frame source payload admission checks\n",checks); return 0; }
