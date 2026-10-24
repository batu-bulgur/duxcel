#include "workbook/history_format.h"
#include "workbook/cell_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL history format:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static WorkbookHistoryStream *create(size_t entries,size_t bytes,bool forked) {
    WorkbookHistoryStreamOrigin o={.owner={1,2,99,VERSION_WORKING_HEAD},.edit_session=44,.command_revision=100,
        .cursor=5,.tail=forked ? 7 : 5,.lineage=2,.action_high_water=40,.transition_high_water=60,.lineage_high_water=7};
    WorkbookHistoryStream *s=NULL; CHECK(workbook_history_stream_new(o,(WorkbookHistoryStreamOptions){entries,bytes},&s)==HISTORY_STREAM_OK); return s;
}
static WorkbookHistoryFormatRecord record(WorkbookHistoryStream *s,const FormatStore *before,const FormatSnapshot *after,const SourceVersions *v) {
    WorkbookHistoryStreamOrigin o=workbook_history_stream_state(s).current;
    return (WorkbookHistoryFormatRecord){.workbook={1,2},.edit_session=44,.sheet_session=88,.sheet=9,
        .expected_revision=o.command_revision,.revision=o.command_revision+1,.effects=8,
        .before=format_delta_store_view(before),.after=format_delta_snapshot_view(after),.sources=v,.object_high_water=10};
}
static void reject(WorkbookHistoryStream *s,WorkbookHistoryFormatRecord r) {
    WorkbookHistoryStreamState before=workbook_history_stream_state(s); WorkbookHistoryFormatRecorder a=workbook_history_stream_format_recorder(s);
    void *ticket=NULL; char *error=NULL; CHECK(!a.reserve(a.context,&r,&ticket,&error) && !ticket && error); free(error);
    WorkbookHistoryStreamState after=workbook_history_stream_state(s);
    CHECK(!memcmp(&before.current,&after.current,sizeof(before.current)) && before.pending==after.pending && before.owned_bytes==after.owned_bytes &&
        before.reserved==after.reserved && before.in_flight==after.in_flight);
}
static SourceVersions source(void) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&column,.column_count=1}; Analysis a={.kind=SOURCE_PARQUET,.path="/source.parquet"};
    SourceDigest digest={.bytes=100,.observed_stamp=11}; digest.identity.size=100; digest.sha256[0]=42;
    SourceVersions v={.high_water=UINT64_MAX,.count=1,.capacity=1,.items=dx_alloc(sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){.version=3,.owner=10,.kind=SOURCE_PARQUET,.path=dx_strdup("/source.parquet")};
    CHECK(source_scope_raw_build(10,3,&a,&schema,&digest,"engine",&v.items[0].raw,NULL));
    v.bytes=sizeof(SourceVersion)+strlen(v.items[0].path)+1+v.items[0].raw.bytes; return v;
}
static void test_source_assets_and_fences(void) {
    WorkbookHistoryStream *s=create(2,DX_HISTORY_STREAM_BYTES,true); WorkbookHistoryFormatRecorder a=workbook_history_stream_format_recorder(s);
    SourceVersions v=source(); FormatStore f={0}; FormatSnapshot next={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    FormatScopeView scope={10,v.items[0].raw.data,v.items[0].raw.bytes}; uint64_t ids[]={1,9007199254740993ULL,UINT64_MAX}; size_t field=0;
    CHECK(format_plan_records(&f,&scope,ids,3,&field,1,FORMAT_BOLD,bold,&next,NULL));
    WorkbookHistoryFormatRecord good=record(s,&f,&next,&v),bad=good;
    bad.workbook.low++; reject(s,bad); bad=good; bad.edit_session++; reject(s,bad); bad=good; bad.expected_revision++; reject(s,bad);
    bad=good; bad.revision++; reject(s,bad); bad=good; bad.sheet=0; reject(s,bad); bad=good; bad.sheet_session=0; reject(s,bad);
    bad=good; bad.object_high_water=9; reject(s,bad); bad=good; bad.sources=NULL; reject(s,bad);
    unsigned effects[]={0,1,4,9,10,12,14,255};
    for (size_t i=0;i<sizeof(effects)/sizeof(*effects);i++) { bad=good; bad.effects=effects[i]; reject(s,bad); }
    bad=good; bad.after=bad.before; reject(s,bad);
    SourcePacket raw=v.items[0].raw; v.items[0].raw=(SourcePacket){0}; v.bytes-=raw.bytes; reject(s,good);
    v.items[0].raw=raw; v.bytes+=raw.bytes;
    void *ticket=NULL; char *error=NULL;
    CHECK(a.reserve(a.context,&good,&ticket,&error) && ticket && !error); size_t charge=workbook_history_stream_state(s).owned_bytes;
    CHECK(charge>raw.bytes && workbook_history_stream_state(s).reserved); reject(s,good);
    CHECK(workbook_history_stream_free(&s,true)==HISTORY_STREAM_BUSY);
    a.cancel(a.context,ticket); ticket=NULL; CHECK(!workbook_history_stream_state(s).owned_bytes && workbook_history_stream_state(s).current.tail==7);
    CHECK(a.reserve(a.context,&good,&ticket,&error) && ticket && !error);
    CHECK(format_commit(&f,&next)); CHECK(f.undo_count==1 && f.source_aware);
    /* Model/source lifetimes may end before the writer dispatches. */
    format_free(&f); source_versions_free(&v); a.publish(a.context,ticket);
    WorkbookHistoryStreamState state=workbook_history_stream_state(s);
    CHECK(state.pending==1 && state.current.cursor==6 && state.current.tail==6 && state.current.lineage==8 && state.current.lineage_high_water==8);
    WorkbookHistoryEvent *e=NULL; CHECK(workbook_history_stream_dispatch(s,&e)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(e);
    CHECK(info->family==DX_HISTORY_FORMAT_FAMILY && info->encoding==DX_FORMAT_DELTA_ENCODING && info->dependency_encoding==DX_SOURCE_VERSIONS_ENCODING &&
        info->forked && info->action==41 && info->transition==61 && info->parent_lineage==2 && !info->inactive_scopes);
    FormatDelta delta={0}; SourceVersionsAsset versions={0};
    CHECK(format_delta_decode(workbook_history_event_payload(e),info->bytes,2*DX_FORMAT_CURRENT_BYTES+DX_FORMAT_VALIDATION_BYTES,&delta)==FORMAT_DELTA_OK);
    CHECK(source_versions_asset_decode(workbook_history_event_dependencies(e),info->dependency_bytes,10,DX_SOURCE_VERSIONS_BYTES,&versions)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(delta.sheet==9 && !delta.before.count && delta.after.count==1 && delta.after.items[0].record_ids[1]==9007199254740993ULL && delta.after.items[0].record_ids[2]==UINT64_MAX);
    CHECK(versions.sheet==9 && versions.versions.high_water==UINT64_MAX && versions.versions.count==1 && !strcmp(versions.versions.items[0].path,"/source.parquet"));
    FormatDependencies closed={0}; CHECK(format_dependencies_build(9,format_delta_snapshot_view(&delta.before),format_delta_snapshot_view(&delta.after),
        &versions.versions,10,DX_SOURCE_VERSIONS_BYTES,&closed)==FORMAT_DEPENDENCIES_OK && closed.sources.versions.count==1);
    format_dependencies_free(&closed); format_delta_free(&delta); source_versions_asset_free(&versions);
    const WorkbookHistoryEvent *identity=e; CHECK(workbook_history_stream_retry(s,&e)==HISTORY_STREAM_OK);
    CHECK(workbook_history_stream_dispatch(s,&e)==HISTORY_STREAM_OK && e==identity && workbook_history_stream_state(s).owned_bytes==charge);
    CHECK(workbook_history_stream_complete(s,&e)==HISTORY_STREAM_OK && !workbook_history_stream_state(s).owned_bytes);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK);
    CHECK(!workbook_history_event_dependencies(NULL));
}
static void test_shared_queue_and_eviction(void) {
    WorkbookHistoryStream *s=create(64,DX_HISTORY_STREAM_BYTES,false); WorkbookHistoryFormatRecorder a=workbook_history_stream_format_recorder(s);
    SourceVersions v={.high_water=20}; FormatStore f={0}; WorkbookHistoryEvent *held=NULL;
    for (size_t i=0;i<40;i++) {
        FormatSnapshot next={0}; CellFormat value=format_default(); value.flags=i%2 ? FORMAT_ITALIC : FORMAT_BOLD;
        CHECK(format_plan_spatial(&f,(FormatRange){0,0,0,0},FORMAT_BOLD|FORMAT_ITALIC,value,&next,NULL));
        WorkbookHistoryFormatRecord r=record(s,&f,&next,&v); void *ticket=NULL;
        CHECK(a.reserve(a.context,&r,&ticket,NULL) && ticket); CHECK(format_commit(&f,&next)); a.publish(a.context,ticket);
        if (!i) CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(f.undo_count==DX_FORMAT_HISTORY && workbook_history_stream_state(s).pending==39);
    const WorkbookHistoryEventInfo *info=workbook_history_event_info(held); FormatDelta first={0};
    CHECK(format_delta_decode(workbook_history_event_payload(held),info->bytes,2*DX_FORMAT_CURRENT_BYTES+DX_FORMAT_VALIDATION_BYTES,&first)==FORMAT_DELTA_OK &&
        first.before.count==0 && first.after.items[0].value.flags==FORMAT_BOLD); format_delta_free(&first);
    CellEditRecorder cell=workbook_history_stream_cell_recorder(s); WorkbookHistoryStreamOrigin o=workbook_history_stream_state(s).current;
    CellEditRecord r={.target={.workbook={1,2},.document=44,.sheet=88,.sheet_id=9,.expected_document_revision=o.command_revision},
        .revision=o.command_revision+1,.delta={9,0,0,"","cell",0,4}}; void *ticket=NULL;
    CHECK(cell.reserve(cell.context,&r,&ticket,NULL) && ticket); cell.publish(cell.context,ticket);
    CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK);
    for (size_t i=1;i<=40;i++) {
        CHECK(workbook_history_stream_dispatch(s,&held)==HISTORY_STREAM_OK); info=workbook_history_event_info(held);
        CHECK(info->action==41+i && info->revision==101+i && info->position==6+i);
        CHECK(info->family==(i==40 ? DX_HISTORY_CELL_FAMILY : DX_HISTORY_FORMAT_FAMILY));
        if (i==40) CHECK(!info->dependency_bytes && !info->dependency_encoding && !workbook_history_event_dependencies(held));
        else {
            SourceVersionsAsset empty={0}; CHECK(source_versions_asset_decode(workbook_history_event_dependencies(held),info->dependency_bytes,0,0,&empty)==SOURCE_VERSIONS_CODEC_OK);
            CHECK(empty.versions.high_water==20 && !empty.versions.count); source_versions_asset_free(&empty);
        }
        CHECK(workbook_history_stream_complete(s,&held)==HISTORY_STREAM_OK);
    }
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); format_free(&f);
}
static void test_exact_budget_and_legacy(void) {
    WorkbookHistoryStream *s=create(1,DX_HISTORY_STREAM_BYTES,false); SourceVersions v={0}; FormatStore f={0}; FormatSnapshot next={0};
    CellFormat value=format_default(); value.flags=FORMAT_BOLD; const unsigned char opaque[]={0,1,2,0}; FormatScopeView scope={0,opaque,sizeof(opaque)};
    CHECK(format_plan_scoped(&f,(FormatRange){0,1,0,0},FORMAT_BOLD,value,&scope,1,false,&next,NULL));
    WorkbookHistoryFormatRecord r=record(s,&f,&next,&v); WorkbookHistoryFormatRecorder a=workbook_history_stream_format_recorder(s); void *ticket=NULL;
    CHECK(a.reserve(a.context,&r,&ticket,NULL)); size_t charge=workbook_history_stream_state(s).owned_bytes; a.cancel(a.context,ticket);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); s=create(1,charge-1,false); reject(s,r);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); s=create(1,charge,false); a=workbook_history_stream_format_recorder(s); ticket=NULL;
    CHECK(a.reserve(a.context,&r,&ticket,NULL)); a.publish(a.context,ticket); reject(s,r);
    WorkbookHistoryEvent *e=NULL; CHECK(workbook_history_stream_dispatch(s,&e)==HISTORY_STREAM_OK && workbook_history_event_info(e)->inactive_scopes==1);
    r.expected_revision++; r.revision++; reject(s,r); CHECK(workbook_history_stream_complete(s,&e)==HISTORY_STREAM_OK);
    ticket=NULL; CHECK(a.reserve(a.context,&r,&ticket,NULL)); a.cancel(a.context,ticket);
    CHECK(workbook_history_stream_free(&s,false)==HISTORY_STREAM_OK); format_snapshot_free(&next); format_free(&f);
}
int main(void) {
    test_source_assets_and_fences(); test_shared_queue_and_eviction(); test_exact_budget_and_legacy();
    printf("PASS: %u immutable format event ownership checks\n",checks); return 0;
}
