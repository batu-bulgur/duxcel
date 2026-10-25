#ifndef DUXCEL_TEST_HISTORY_PROFILE_FIXTURE_H
#define DUXCEL_TEST_HISTORY_PROFILE_FIXTURE_H
/* Six genuine current product-codec payloads; no native file generator.
   The including test supplies CHECK and standard C headers. */
typedef struct {
    WorkbookId doc; WorkbookVersionRoot root; WorkbookVersionTag tag; WorkbookVersionIndex index;
    WorkbookHistoryAnchor anchor; WorkbookHistoryTagAnchor label; WorkbookHistoryAnchors anchors;
    WorkbookHistorySegment segment; WorkbookHistoryEventPosition positions[6]; WorkbookHistoryPath path; size_t map[6];
    WorkbookHistoryEventInfo frames[6]; WorkbookHistoryProfileRecord records[6];
    SourceVersions ledger; SourceDependencyLive live;
    WorkbookHistoryProfileView view;
} Fixture;
static void packet(Fixture *f,size_t i,unsigned family,unsigned encoding,void *data,size_t bytes,void *deps,size_t dependency_bytes,unsigned dependency_encoding) {
    f->frames[i]=(WorkbookHistoryEventInfo){.owner={f->doc.high,f->doc.low,88,VERSION_WORKING_HEAD},.edit_session=9,.revision=i+1,.action=i+1,.transition=i+1,.position=i+1,.previous_position=i,.family=family,.encoding=encoding,.bytes=bytes,.dependency_bytes=dependency_bytes,.dependency_encoding=dependency_encoding};
    f->records[i]=(WorkbookHistoryProfileRecord){.owner=f->frames[i].owner,.edit_session=9,.event=f->frames+i,.data=data,.dependencies=deps,.bytes=bytes,.dependency_bytes=dependency_bytes,.sheet=1,.live=&f->live};
    f->positions[i]=(WorkbookHistoryEventPosition){i+1,0,i+1}; f->map[i]=i;
}
static void fixture(Fixture *f) {
    *f=(Fixture){0}; f->doc=(WorkbookId){11,22}; f->root=(WorkbookVersionRoot){.bytes=12345,.encoding=15}; f->root.key.bytes[0]=71;
    f->tag=(WorkbookVersionTag){{"Current",7},f->root.key,1};
    f->index=(WorkbookVersionIndex){f->doc.high,f->doc.low,1,1,{true,f->root.key},&f->root,1,&f->tag,1};
    f->anchor=(WorkbookHistoryAnchor){{1,4,f->root.key},0}; f->label=(WorkbookHistoryTagAnchor){1,1};
    f->anchors=(WorkbookHistoryAnchors){f->doc.high,f->doc.low,1,1,&f->anchor,1,&f->label,1};
    f->segment=(WorkbookHistorySegment){0,0,6}; f->path=(WorkbookHistoryPath){f->doc.high,f->doc.low,&f->segment,1,f->positions,6,0,6,0,0,4,6};
    f->live=(SourceDependencyLive){1,0,&f->ledger};
    CellDelta cell={1,10,2,"NULL","",4,0}; size_t bytes=0,written=0;
    CHECK(cell_delta_encoded_size(cell,&bytes)==CELL_DELTA_CODEC_OK); void *data=dx_alloc(bytes);
    CHECK(cell_delta_encode(cell,data,bytes,&written)==CELL_DELTA_CODEC_OK); packet(f,0,DX_HISTORY_CELL_FAMILY,DX_CELL_DELTA_ENCODING,data,bytes,NULL,0,0); f->records[0].intent.effects=2;
    FormatStore formats={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_apply(&formats,(FormatRange){1,1,1,1},FORMAT_BOLD,bold,NULL));
    CHECK(format_delta_encoded_size(1,(FormatSnapshotView){0},format_delta_store_view(&formats),&bytes)==FORMAT_DELTA_OK); data=dx_alloc(bytes);
    CHECK(format_delta_encode(1,(FormatSnapshotView){0},format_delta_store_view(&formats),data,bytes,&written)==FORMAT_DELTA_OK);
    size_t source_bytes=0; CHECK(source_versions_asset_size(1,&f->ledger,0,&source_bytes)==SOURCE_VERSIONS_CODEC_OK); void *deps=dx_alloc(source_bytes);
    CHECK(source_versions_asset_encode(1,&f->ledger,0,deps,source_bytes,&written)==SOURCE_VERSIONS_CODEC_OK); packet(f,1,DX_HISTORY_FORMAT_FAMILY,DX_FORMAT_DELTA_ENCODING,data,bytes,deps,source_bytes,DX_SOURCE_VERSIONS_ENCODING); f->records[1].intent.effects=8; format_free(&formats);
    Analysis before={0},after={.kind=SOURCE_CSV,.path="/source"}; DefinitionEventPacket definition={0};
    CHECK(definition_event_capture((DefinitionEventView){.sheet=1,.effects=1,.before={&before,NULL},.after={&after,NULL},.sources=&f->ledger},SIZE_MAX,&definition)==DEFINITION_EVENT_OK);
    packet(f,2,DX_HISTORY_DEFINITION_FAMILY,DX_DEFINITION_EVENT_ENCODING,definition.data,definition.bytes,NULL,0,0); f->records[2].intent.effects=1;
    ColumnWidthEntry width={3,column_width_bits(128)}; RangeEventView range={.sheet=1,.effects=16,.widths={{0},{&width,1}}}; RangeEventInfo ri;
    CHECK(range_event_measure(range,SIZE_MAX,&ri)==RANGE_EVENT_OK); data=dx_alloc(ri.bytes);
    CHECK(range_event_encode(range,SIZE_MAX,data,ri.bytes,&written)==RANGE_EVENT_OK); packet(f,3,DX_HISTORY_RANGE_FAMILY,DX_RANGE_EVENT_ENCODING,data,ri.bytes,NULL,0,0); f->records[3].intent.effects=16;
    SheetLifecycleEntry original[]={{1,"A",false},{2,"B",false}},renamed[]={{1,"Renamed",false},{2,"B",false}};
    SheetLifecyclePacket metadata={0}; CHECK(sheet_lifecycle_capture((SheetLifecycle){{f->doc,2,1,original,2},{f->doc,2,1,renamed,2}},SIZE_MAX,&metadata)==SHEET_LIFECYCLE_OK);
    packet(f,4,DX_HISTORY_SHEET_LIFECYCLE_FAMILY,DX_HISTORY_SHEET_LIFECYCLE_PROFILE,metadata.data,metadata.bytes,NULL,0,0); f->records[4].intent.operation=SHEET_RENAME; f->records[4].live=NULL;
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,"Added",&(Analysis){0},NULL)); b.id=f->doc; b.sheet_high_water=3; b.sheets[0].id=3;
    SheetPresenceAsset *asset=NULL; SheetPresencePacket presence={0}; CHECK(sheet_presence_capture((SheetPresenceView){f->doc,3,b.sheets,&b.sheets[0].analysis},SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
    CHECK(sheet_presence_encode(asset,SIZE_MAX,&presence)==SHEET_PRESENCE_OK); SheetLifecycleEntry added[]={{1,"Renamed",false},{2,"B",false},{3,"Added",false}}; SheetTopologyPacket topology={0};
    CHECK(sheet_topology_capture((SheetTopologyView){SHEET_TOPOLOGY_ADD,3,{f->doc,2,1,renamed,2},{f->doc,3,3,added,3},&presence},SIZE_MAX,&topology)==SHEET_TOPOLOGY_OK);
    packet(f,5,DX_HISTORY_SHEET_LIFECYCLE_FAMILY,DX_HISTORY_SHEET_TOPOLOGY_PROFILE,topology.data,topology.bytes,NULL,0,0); f->records[5].sheet=3; f->records[5].intent.operation=SHEET_CREATE; f->records[5].live=NULL;
    sheet_presence_free(&asset); sheet_presence_packet_free(&presence); workbook_free(&b);
    f->view=(WorkbookHistoryProfileView){f->doc,16,15,HISTORY_PROFILE_RETAINED,&f->index,&f->anchors,{&f->path,f->map,6},{&f->path,f->map,6},f->records,6};
}
static void fixture_free(Fixture *f) { for (size_t i=0;i<6;i++) { free((void *)f->records[i].data); free((void *)f->records[i].dependencies); } }
#endif
