#ifndef DUXCEL_TEST_RANGE_PAYLOAD_FIXTURE_H
#define DUXCEL_TEST_RANGE_PAYLOAD_FIXTURE_H
/* Shared finite actual range-producer fixture; caller defines CHECK. */
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
static void fixture_origin(Fixture *f,unsigned profile,WorkbookHistoryStreamOrigin origin) {
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
    CHECK(workbook_history_stream_new(origin,(WorkbookHistoryStreamOptions){1,DX_HISTORY_STREAM_DEFAULT_BYTES},&f->stream)==HISTORY_STREAM_OK);
    WorkbookHistoryRangeRecorder recorder=workbook_history_stream_range_recorder(f->stream); void *ticket=NULL;
    WorkbookHistoryRangeRecord record={.workbook={origin.owner.document_high,origin.owner.document_low},.edit_session=origin.edit_session,.sheet_session=4,.expected_revision=origin.command_revision,.revision=origin.command_revision+1,.validation_reserved=SIZE_MAX,.view=view};
    CHECK(recorder.reserve(recorder.context,&record,&ticket,NULL)); recorder.publish(recorder.context,ticket);
    CHECK(workbook_history_stream_dispatch(f->stream,&f->held)==HISTORY_STREAM_OK);
    const WorkbookHistoryEventInfo *event=workbook_history_event_info(f->held);
    f->view=(WorkbookRangePayloadView){.event=event,.data=workbook_history_event_payload(f->held),.bytes=event->bytes,.sheet=6,.effects=view.effects};
}
static void finish(Fixture *f) {
    CHECK(workbook_history_stream_complete(f->stream,&f->held)==HISTORY_STREAM_OK && workbook_history_stream_free(&f->stream,false)==HISTORY_STREAM_OK);
    objects_free(&f->objects); source_versions_free(&f->sources); format_free(&f->before); format_free(&f->after);
}
#endif
