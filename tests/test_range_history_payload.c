#include "workbook/range_history_payload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static uint64_t hash_bytes(const void *data,size_t bytes) { const unsigned char *p=data; uint64_t h=UINT64_C(1469598103934665603); for (size_t i=0;i<bytes;i++) { h^=p[i]; h*=UINT64_C(1099511628211); } return h; }
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range payload:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    ObjectStore objects; SourceVersions sources; FormatStore before,after;
    WorkbookHistoryStream *stream; WorkbookHistoryEvent *held;
    WorkbookRangePayloadView view;
} Fixture;
static SourcePacket raw(uint64_t owner,uint64_t version,const char *path,unsigned seed) {
    Column column={.name="Amount",.type=DUCKDB_TYPE_DECIMAL,.type_name="DECIMAL(38,4)",.numeric=true,.number_type={NUM_DECIMAL,38,4}};
    Page schema={.columns=&column,.column_count=1}; Analysis source={.kind=SOURCE_PARQUET,.path=(char *)path}; SourceDigest digest={.bytes=100+seed};
    digest.identity.size=digest.bytes; memset(digest.sha256,(int)seed,sizeof(digest.sha256)); SourcePacket packet={0};
    CHECK(source_scope_raw_build(owner,version,&source,&schema,&digest,"exact-engine",&packet,NULL)); return packet;
}
static void row(SourceVersions *v,uint64_t owner,uint64_t version,const char *path,bool complete) {
    v->items=dx_realloc(v->items,(v->count+1)*sizeof(*v->items)); SourceVersion *entry=v->items+v->count++;
    *entry=(SourceVersion){.owner=owner,.version=version,.kind=SOURCE_PARQUET,.path=dx_strdup(path)};
    if (complete) entry->raw=raw(owner,version,path,1);
    v->capacity=v->count; v->bytes+=sizeof(*entry)+strlen(path)+1+entry->raw.bytes; if (v->high_water<version) v->high_water=version;
    CHECK(source_versions_valid(v,UINT64_MAX,NULL));
}
static void fixture(Fixture *f,unsigned profile) {
    *f=(Fixture){0}; RangeEventView view={.sheet=6};
    CellDelta cell={.sheet=6,.row=4,.column=2,.before="NULL",.before_bytes=4,.after="9007199254740993.0123456789",.after_bytes=25}; cell.after_bytes=strlen(cell.after);
    ColumnWidthEntry width={3,column_width_bits(123)}; ObjectSnapshot before={0},after={0}; SheetObject moved={0};
    if (profile==0 || profile==2 || profile==6) { view.effects|=2; view.cells=&cell; view.count=1; }
    if (profile==1 || profile==2 || profile==6) { view.effects|=16; view.widths=(ColumnWidthPair){{0},{&width,1}}; }
    if (profile>=3) f->sources.high_water=19;
    if (profile>=4) {
        Analysis source={.kind=profile==4 ? SOURCE_DATABASE : SOURCE_PARQUET,.path=profile==4 ? "/source.duckdb" : "/table.parquet",
            .table=profile==4 ? "Orders" : NULL,.source_version=profile==4 ? 0 : 2};
        CHECK(objects_add_table(&f->objects,2,4,"Table",&source,NULL)==0); before=(ObjectSnapshot){f->objects.items,f->objects.count}; moved=f->objects.items[0]; moved.row=20; after=(ObjectSnapshot){&moved,1};
        view.effects|=4; view.object_high_water=f->objects.next_id;
        if (profile==6) { f->sources.high_water=0; row(&f->sources,0,1,"/primary.parquet",true); }
        if (profile>=5) row(&f->sources,1,2,"/table.parquet",false);
        f->sources.high_water=19;
    }
    if (profile==3 || profile==6) {
        view.effects|=8; f->after.source_aware=true;
        if (profile==6) {
            f->before.source_aware=true; uint64_t record=2; size_t field=0; CellFormat italic=format_default(); italic.flags=FORMAT_ITALIC;
            FormatScopeView scope={0,f->sources.items[0].raw.data,f->sources.items[0].raw.bytes};
            CHECK(format_apply_records(&f->after,&scope,&record,1,&field,1,FORMAT_ITALIC,italic,NULL));
        }
        if (!(view.effects&4)) { view.sources=&f->sources; view.object_high_water=1; view.before_formats=format_delta_store_view(&f->before); view.after_formats=format_delta_store_view(&f->after); }
    }
    if (view.effects&4) view.definitions=(DefinitionEventView){.sheet=6,.effects=view.effects&12u,.object_high_water=f->objects.next_id,
        .before={NULL,&before},.after={NULL,&after},.sources=&f->sources,.before_formats=format_delta_store_view(&f->before),.after_formats=format_delta_store_view(&f->after)};
    WorkbookHistoryStreamOrigin origin={.owner={1,2,9,VERSION_WORKING_HEAD},.edit_session=3}; CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&f->stream)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(f->stream); void *ticket=NULL;
    WorkbookHistoryRangeRecord record={.workbook={1,2},.edit_session=3,.sheet_session=4,.revision=1,.validation_reserved=SIZE_MAX,.view=view};
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); recorder.publish(recorder.context,ticket);
    CHECK(workbook_history_stream_dispatch(f->stream,&f->held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *event=workbook_history_event_info(f->held);
    f->view=(WorkbookRangePayloadView){.event=event,.data=workbook_history_event_payload(f->held),.bytes=event->bytes,.sheet=6,.effects=view.effects};
}
static void finish(Fixture *f) {
    CHECK(workbook_history_stream_complete(f->stream,&f->held)==HISTORY_STREAM_OK && workbook_history_stream_free(&f->stream,false)==HISTORY_STREAM_OK);
    objects_free(&f->objects); source_versions_free(&f->sources); format_free(&f->before); format_free(&f->after);
}
static void rejected(WorkbookRangePayloadView view,SourceDependencyLive live,WorkbookRangePayloadStatus expected,SourceDependencyStatus admission) {
    WorkbookRangePayloadInfo info={.sheet=91,.owned=92},before=info; SourceDependencyAdmission report={.unbound_sources=93,.opaque_scopes=94},held=report;
    CHECK(workbook_range_payload_preflight(view,&info)==expected && !memcmp(&before,&info,sizeof(info)));
    CHECK(workbook_range_payload_admit(view,live,SIZE_MAX,&report)==admission && !memcmp(&held,&report,sizeof(report)));
}
static void profiles(void) {
    for (unsigned profile=0;profile<7;profile++) {
        Fixture f; fixture(&f,profile); WorkbookHistoryStreamState state=workbook_history_stream_state(f.stream); WorkbookHistoryEventInfo frame=*f.view.event;
        uint64_t original_hash=hash_bytes(f.view.data,f.view.bytes); void *copy=dx_alloc(f.view.bytes); memcpy(copy,f.view.data,f.view.bytes); SourceVersions source_header=f.sources;
        SourceDependencyLive live={6,1,profile>=3 ? &f.sources : NULL}; WorkbookRangePayloadInfo info={0}; SourceDependencyAdmission report={.unbound_sources=91},held=report;
        CHECK(workbook_range_payload_preflight(f.view,&info)==RANGE_PAYLOAD_OK && info.sheet==6 && info.effects==f.view.effects && info.staging>=info.owned && info.owned>=f.view.bytes+sizeof(RangeEvent));
        CHECK(workbook_range_payload_admit(f.view,live,info.staging-1,&report)==SOURCE_DEPENDENCY_LIMIT && !memcmp(&held,&report,sizeof(report)));
        CHECK(workbook_range_payload_admit(f.view,live,info.staging,&report)==SOURCE_DEPENDENCY_EXACT && report.unbound_sources==(profile==4 ? 2u : 0u) && !report.opaque_scopes);
        CHECK(report.sources.exact==(profile==5 ? 1u : profile==6 ? 2u : 0u) && report.sources.captured_pending==(profile>=5 ? 1u : 0u) && !report.sources.refined && !report.sources.allocator_unavailable);
        if (profile<3) {
            SourceVersions malformed={.count=SIZE_MAX}; live.versions=&malformed;
            CHECK(workbook_range_payload_admit(f.view,live,info.staging,&report)==SOURCE_DEPENDENCY_EXACT && !report.sources.allocator_unavailable); live.versions=NULL;
        } else {
            live.versions=NULL; CHECK(workbook_range_payload_admit(f.view,live,info.staging,&report)==SOURCE_DEPENDENCY_UNAVAILABLE && report.sources.allocator_unavailable && report.unbound_sources==(profile==4 ? 2u : 0u)); live.versions=&f.sources;
        }
        live.sheet=7; CHECK(workbook_range_payload_admit(f.view,live,info.staging,&report)==SOURCE_DEPENDENCY_CONFLICT); live.sheet=6;
        for (unsigned fault=0;fault<12;fault++) {
            WorkbookHistoryEventInfo bad=frame; WorkbookRangePayloadView view=f.view; view.event=&bad;
            switch (fault) {
            case 0: bad.family=3; break; case 1: bad.encoding=3; break;
            case 2: bad.bytes--; break; case 3: bad.dependency_encoding=1; break; case 4: bad.dependency_bytes=1; break;
            case 5: bad.inactive_scopes=1; break; case 6: view.dependencies=copy; break; case 7: view.dependency_bytes=1; break;
            case 8: view.sheet++; break; case 9: view.effects=0; break; case 10: bad.position++; break; case 11: bad.owner.mode=VERSION_HISTORICAL_VIEW; break;
            }
            rejected(view,live,fault<2 ? RANGE_PAYLOAD_UNSUPPORTED : RANGE_PAYLOAD_INVALID,fault<2 ? SOURCE_DEPENDENCY_UNSUPPORTED : SOURCE_DEPENDENCY_INVALID);
        }
        for (size_t n=0;n<f.view.bytes;n++) { WorkbookRangePayloadView view=f.view; view.bytes=n; rejected(view,live,RANGE_PAYLOAD_INVALID,SOURCE_DEPENDENCY_INVALID); }
        unsigned char *data=copy; data[4]=3; WorkbookRangePayloadView malformed=f.view; malformed.data=data; rejected(malformed,live,RANGE_PAYLOAD_UNSUPPORTED,SOURCE_DEPENDENCY_UNSUPPORTED); memcpy(data,f.view.data,f.view.bytes);
        if (profile==4) {
            data[DX_RANGE_EVENT_HEADER_BYTES+56]^=1; report=held;
            CHECK(workbook_range_payload_admit(malformed,live,SIZE_MAX,&report)==SOURCE_DEPENDENCY_INVALID && !memcmp(&held,&report,sizeof(report)));
        }
        CHECK(!memcmp(&frame,f.view.event,sizeof(frame)) && !memcmp(&source_header,&f.sources,sizeof(source_header)) && hash_bytes(f.view.data,f.view.bytes)==original_hash);
        WorkbookHistoryStreamState after=workbook_history_stream_state(f.stream); CHECK(!memcmp(&state,&after,sizeof(state)));
        free(copy); finish(&f);
        CHECK(report.sources.captured_pending<=1); /* Reports own no retired decoder or stream pointer. */
    }
}
static void source_compatibility(void) {
    Fixture f; fixture(&f,6); WorkbookRangePayloadInfo info={0}; CHECK(workbook_range_payload_preflight(f.view,&info)==RANGE_PAYLOAD_OK);
    SourceVersions live={0}; CHECK(source_versions_copy(&live,&f.sources,NULL)); SourceDependencyLive context={6,1,&live}; SourceDependencyAdmission report={0};
    row(&live,0,20,"/extra.parquet",false);
    CHECK(workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_EXACT && report.sources.exact==2 && report.sources.captured_pending==1);
    SourceVersion complete=live.items[1]; complete.raw=raw(1,2,complete.path,1); SourceVersions refined={0};
    CHECK(source_versions_plan(&live,&complete,false,&refined,NULL)); source_packet_free(&complete.raw); source_versions_free(&live); live=refined;
    CHECK(workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_REFINED && report.sources.refined==1 && report.sources.exact==1 && report.sources.captured_pending==1);
    source_versions_free(&live); CHECK(source_versions_copy(&live,&f.sources,NULL)); live.high_water=2;
    CHECK(workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_UNAVAILABLE && report.sources.allocator_unavailable && report.sources.captured_pending==1);
    live.high_water=19; size_t bytes=live.items[0].raw.bytes; source_packet_free(&live.items[0].raw); live.bytes-=bytes;
    CHECK(workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_UNAVAILABLE && report.sources.unavailable==1 && report.sources.captured_pending==1);
    source_versions_free(&live); CHECK(source_versions_copy(&live,&f.sources,NULL)); source_packet_free(&live.items[0].raw); live.items[0].raw=raw(0,1,live.items[0].path,2);
    CHECK(source_versions_valid(&live,1,NULL) && workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_CONFLICT && report.sources.conflicting==1);
    source_versions_free(&live); CHECK(source_versions_copy(&live,&f.sources,NULL)); free(live.items[1].path); live.items[1].path=dx_strdup("/other.parquet");
    CHECK(source_versions_valid(&live,1,NULL) && workbook_range_payload_admit(f.view,context,info.staging,&report)==SOURCE_DEPENDENCY_CONFLICT && report.sources.conflicting==1);
    source_versions_free(&live); finish(&f);
}
int main(void) { profiles(); source_compatibility(); printf("PASS: %u actual family4 range payload checks\n",checks); return 0; }
