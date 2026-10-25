#include "workbook/range_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL range event:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void compound(void) {
    ColumnWidthEntry before[]={{0,0},{16383,0}},after[]={{1,0}};
    before[0].bits=column_width_bits(100); before[1].bits=column_width_bits(1200); after[0].bits=column_width_bits(100);
    CellDelta cell={.sheet=7,.row=4,.column=2,.before="NULL",.after="9007199254740993.0123456789",.before_bytes=4,.after_bytes=25};
    cell.after_bytes=strlen(cell.after);
    SourceVersions ledger={0}; FormatStore store={0}; FormatSnapshot style={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_spatial(&store,(FormatRange){4,4,2,2},FORMAT_BOLD,bold,&style,NULL));
    const unsigned effects[]={2,8,10,16,18,24,26};
    for (size_t k=0;k<sizeof(effects)/sizeof(*effects);k++) {
        unsigned effect=effects[k];
        RangeEventView v={.sheet=7,.effects=effect};
        if (effect&2) { v.cells=&cell; v.count=1; }
        if (effect&8) { v.sources=&ledger; v.before_formats=format_delta_store_view(&store); v.after_formats=format_delta_snapshot_view(&style); }
        if (effect&16) v.widths=(ColumnWidthPair){{before,2},{after,1}};
        RangeEventInfo measured={0},info={0}; CHECK(range_event_measure(v,SIZE_MAX,&measured)==RANGE_EVENT_OK);
        unsigned char *data=dx_alloc(measured.bytes+1); memset(data,0x77,measured.bytes+1); size_t written=91;
        CHECK(range_event_encode(v,SIZE_MAX,data,measured.bytes,&written)==RANGE_EVENT_OK && written==measured.bytes && data[written]==0x77 && data[4]==DX_RANGE_EVENT_ENCODING);
        CHECK(range_event_preflight(data,written,&info)==RANGE_EVENT_OK && info.effects==effect && info.owned<=measured.owned && info.width_bytes==measured.width_bytes);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,info.owned+info.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK);
        if (effect&16) {
            CHECK(event.widths.before.count==2 && event.widths.after.count==1 && event.widths.before.entries!=before && event.widths.after.entries!=after);
            CHECK(event.widths.before.entries[1].column==16383 && event.widths.before.entries[1].bits==column_width_bits(1200));
            ColumnWidthPair inverse=column_width_inverse(column_width_delta_view(&event.widths));
            CHECK(column_width_equal(inverse.after,(ColumnWidthView){before,2}) && column_width_equal(inverse.before,(ColumnWidthView){after,1}));
        }
        if (effect&2) CHECK(event.cells[0].before!=cell.before && !memcmp(event.cells[0].after,cell.after,cell.after_bytes));
        if (effect&8) CHECK(event.formats.after.items!=style.items && event.formats.after.count==1);
        RangeEvent held=event; RangeEventInfo sentinel=info;
        for (size_t n=0;n<written;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&held,&event,sizeof(held)));
        data[written]=0; CHECK(range_event_preflight(data,written+1,&info)==RANGE_EVENT_INVALID && !memcmp(&sentinel,&info,sizeof(info)));
        data[4]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[4]=DX_RANGE_EVENT_ENCODING;
        if (effect&16) {
            size_t at=written-measured.source_bytes-measured.width_bytes;
            data[at+4]=2; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[at+4]=1;
            data[at+16]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[at+16]=0;
            data[at+20]=0xff; data[at+21]=0xff;
            CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held)));
            /* Restore the exact known-good packet before testing input lifetime. */
            CHECK(range_event_encode(v,SIZE_MAX,data,written,&written)==RANGE_EVENT_OK);
        }
        memset(data,0,written); free(data);
        if (effect&16) CHECK(event.widths.before.entries[1].bits==column_width_bits(1200));
        range_event_free(&event);
    }
    RangeEventView v={.sheet=7,.effects=16,.widths={{before,2},{after,1}}}; RangeEventInfo info={0};
    CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_OK); size_t bytes=info.bytes,written=17; unsigned char *data=dx_alloc(bytes); memset(data,0x66,bytes);
    CHECK(range_event_encode(v,0,data,bytes,(size_t *)data)==RANGE_EVENT_INVALID && data[0]==0x66);
    CHECK(range_event_encode(v,0,before,bytes,&written)==RANGE_EVENT_INVALID && before[0].bits==column_width_bits(100) && written==17);
    CHECK(range_event_encode(v,0,data,bytes,(size_t *)before)==RANGE_EVENT_INVALID && before[0].bits==column_width_bits(100));
    v.widths.after=v.widths.before; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    v.widths.after=(ColumnWidthView){after,1}; v.effects=2; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    v.effects=17; CHECK(range_event_measure(v,0,&info)==RANGE_EVENT_INVALID);
    free(data); format_snapshot_free(&style); format_free(&store);
}
static size_t u32(const unsigned char *p) { return (size_t)p[0]|((size_t)p[1]<<8)|((size_t)p[2]<<16)|((size_t)p[3]<<24); }
static void objects_compound(void) {
    ObjectStore objects={0}; CHECK(objects_add(&objects,2,2,"İstanbul template",NULL)==0);
    ObjectSnapshot before={objects.items,objects.count}; SheetObject moved=objects.items[0]; moved.row=20; moved.col=12; ObjectSnapshot after={&moved,1};
    SourceVersions sources={.high_water=17}; FormatStore formats={0}; FormatSnapshot styling={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    CHECK(format_plan_spatial(&formats,(FormatRange){2,2,2,2},FORMAT_BOLD,bold,&styling,NULL));
    CellDelta cell={.sheet=9,.row=1,.column=1,.before="NULL",.before_bytes=4,.after="Exact",.after_bytes=5}; ColumnWidthEntry width={3,column_width_bits(120)};
    const unsigned effects[]={4,6,12,14,20,22,28,30};
    for (size_t k=0;k<sizeof(effects)/sizeof(*effects);k++) {
        unsigned effect=effects[k]; RangeEventView view={.sheet=9,.effects=effect,.object_high_water=objects.next_id,.definitions={.sheet=9,.object_high_water=objects.next_id,.effects=effect&12u,.before={NULL,&before},.after={NULL,&after},.sources=&sources}};
        if (effect&2) { view.cells=&cell; view.count=1; }
        if (effect&8) { view.definitions.before_formats=format_delta_store_view(&formats); view.definitions.after_formats=format_delta_snapshot_view(&styling); }
        if (effect&16) view.widths=(ColumnWidthPair){{0},{&width,1}};
        RangeEventInfo measured={0},info={0}; CHECK(range_event_measure(view,SIZE_MAX,&measured)==RANGE_EVENT_OK && measured.definition_bytes && !measured.format_bytes);
        unsigned char *data=dx_alloc(measured.bytes+1); size_t written=71; memset(data,0x77,measured.bytes+1);
        CHECK(range_event_encode(view,SIZE_MAX,data,measured.bytes,&written)==RANGE_EVENT_OK && data[written]==0x77);
        CHECK(range_event_preflight(data,written,&info)==RANGE_EVENT_OK && info.effects==effect && info.owned<=measured.owned);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,info.owned+info.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK && event.definitions.effects==(effect&12u) && event.definitions.before_objects.objects.count==1);
        CHECK(event.definitions.before_objects.objects.items[0].name!=objects.items[0].name && event.definitions.after_objects.objects.items[0].row==20 && event.definitions.sources.versions.high_water==17 && !event.definitions.sources.versions.count);
        if (effect&8) CHECK(event.definitions.formats.after.count==1 && !event.formats.after.count);
        RangeEvent held=event;
        for (size_t n=0;n<written;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&held,&event,sizeof(held)));
        data[4]=2; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(held))); data[4]=DX_RANGE_EVENT_ENCODING;
        size_t nested=written-measured.definition_bytes;
        data[nested+8]=8; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[nested+8]=9;
        data[36]=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held))); data[36]=0;
        size_t old_bytes=u32(data+nested+36),new_bytes=u32(data+nested+40); CHECK(old_bytes==new_bytes);
        memcpy(data+nested+DX_DEFINITION_EVENT_HEADER_BYTES+old_bytes,data+nested+DX_DEFINITION_EVENT_HEADER_BYTES,old_bytes);
        CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(held)));
        CHECK(range_event_encode(view,SIZE_MAX,data,written,&written)==RANGE_EVENT_OK);
        CHECK(range_event_encode(view,SIZE_MAX,objects.items,measured.bytes,&written)==RANGE_EVENT_INVALID && objects.items[0].row==2);
        CHECK(range_event_encode(view,SIZE_MAX,data,measured.bytes,(size_t *)objects.items[0].name)==RANGE_EVENT_INVALID && !strcmp(objects.items[0].name,"İstanbul template"));
        free(data); range_event_free(&event);
        RangeEventView invalid=view; invalid.definitions.before.primary=&(Analysis){0}; CHECK(range_event_measure(invalid,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED);
        moved.source_id=1; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_INVALID); moved.source_id=0;
        moved.kind=OBJECT_TABLE; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_INVALID); moved.kind=OBJECT_PIVOT;
    }
    format_snapshot_free(&styling); format_free(&formats); objects_free(&objects);
}
static void wire_put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static void source_objects(void) {
    for (unsigned bound=0;bound<2;bound++) {
        Analysis source={.kind=SOURCE_CSV,.path="/owned.csv",.source_version=bound ? 1 : 0}; ObjectStore objects={0};
        CHECK(objects_add_table(&objects,2,4,"Imported Table",&source,NULL)==0);
        ObjectSnapshot before={objects.items,objects.count}; SheetObject moved=objects.items[0]; moved.row=20; ObjectSnapshot after={&moved,1};
        SourceVersions sources={.high_water=bound ? 0 : 17};
        if (bound) { SourceVersions next={0}; SourceVersion row={.owner=objects.items[0].id,.version=1,.kind=SOURCE_CSV,.path="/owned.csv"}; CHECK(source_versions_plan(&sources,&row,true,&next,NULL)); sources=next; }
        RangeEventView v={.sheet=8,.effects=4,.object_high_water=objects.next_id,.definitions={.sheet=8,.object_high_water=objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&sources}};
        RangeEventInfo info={0}; CHECK(range_event_measure(v,SIZE_MAX,&info)==RANGE_EVENT_OK && info.unbound_sources==(bound ? 0u : 2u) && !info.source_bytes);
        unsigned char *data=dx_alloc(info.bytes); size_t written=0; CHECK(range_event_encode(v,SIZE_MAX,data,info.bytes,&written)==RANGE_EVENT_OK);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_OK && event.unbound_sources==info.unbound_sources && event.object_high_water==objects.next_id && event.definitions.before_objects.objects.items[0].source.source_version==(uint64_t)bound && event.definitions.after_objects.objects.items[0].row==20);
        CHECK(event.definitions.before_objects.objects.items[0].source.path!=objects.items[0].source.path && !strcmp(event.definitions.before_objects.objects.items[0].source.path,"/owned.csv"));
        if (bound) CHECK(event.definitions.sources.versions.count==1 && !event.definitions.sources.versions.items[0].raw.bytes);
        else CHECK(!event.definitions.sources.versions.count && event.definitions.sources.versions.high_water==17);
        RangeEvent held=event; size_t child=DX_RANGE_EVENT_HEADER_BYTES;
        data[child+56]^=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); data[child+56]^=1;
        data[40]^=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); data[40]^=1;
        data[child+8]^=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); data[child+8]^=1;
        data[4]=3; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED && !memcmp(&held,&event,sizeof(event))); data[4]=DX_RANGE_EVENT_ENCODING;
        memset(data,0,written); free(data); objects_free(&objects); source_versions_free(&sources);
        CHECK(!strcmp(event.definitions.after_objects.objects.items[0].source.path,"/owned.csv")); range_event_free(&event);
    }
}
/* Metadata preservation does not assert an actual result/default relation. */
static bool nullable_equal(const char *a,const char *b) { return a && b ? !strcmp(a,b) : a==b; }
static void source_profiles(void) {
    const Analysis profiles[]={
        {.kind=SOURCE_DATABASE,.path="/owned database.duckdb",.table="Orders"},
        {.kind=SOURCE_DATABASE,.path="/owned database.duckdb",.schema="main",.table="Orders"},
        {.kind=SOURCE_DATABASE,.path="/owned database.duckdb",.schema="sales data",.table="Order\"items",.query="SELECT * FROM input WHERE 9007199254740993 > 0"},
        {.kind=SOURCE_DATABASE,.path="/owned database.duckdb"}, /* Unresolved, inactive metadata only. */
        {.kind=SOURCE_PARQUET,.path="/owned parquet.parquet"},
        {.kind=SOURCE_PARQUET,.path="/owned parquet.parquet",.schema="",.table="",.query="SELECT * FROM input",.source_version=1}
    };
    ObjectStore rejected={0}; Analysis empty_query={.kind=SOURCE_DATABASE,.path="/source.duckdb",.table="Orders",.query=""};
    CHECK(objects_add_table(&rejected,2,4,"Invalid query",&empty_query,NULL)<0 && !rejected.count && !rejected.items);
    for (size_t k=0;k<sizeof(profiles)/sizeof(*profiles);k++) {
        const Analysis *profile=profiles+k; ObjectStore objects={0};
        CHECK(objects_add_table(&objects,2,4,"Profile Table",profile,NULL)==0);
        ObjectSnapshot before={objects.items,objects.count}; SheetObject moved=objects.items[0]; moved.row=20; ObjectSnapshot after={&moved,1};
        SourceVersions sources={.high_water=17};
        if (profile->source_version) {
            SourceVersion row={.owner=objects.items[0].id,.version=18,.kind=SOURCE_PARQUET,.path=profile->path}; SourceVersions next={0};
            CHECK(source_versions_plan(&sources,&row,true,&next,NULL)); sources=next;
            objects.items[0].source.source_version=moved.source.source_version=18;
        }
        RangeEventView view={.sheet=8,.effects=4,.object_high_water=objects.next_id,.definitions={.sheet=8,.object_high_water=objects.next_id,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&sources}};
        RangeEventInfo info={0}; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_OK);
        unsigned char *data=dx_alloc(info.bytes),*copy=dx_alloc(info.bytes); size_t written=0;
        CHECK(range_event_encode(view,SIZE_MAX,data,info.bytes,&written)==RANGE_EVENT_OK); memcpy(copy,data,written);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,info.owned+info.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_OK);
        const Analysis *decoded=&event.definitions.after_objects.objects.items[0].source;
        CHECK(decoded->kind==profile->kind && nullable_equal(decoded->path,profile->path) && nullable_equal(decoded->schema,profile->schema) && nullable_equal(decoded->table,profile->table) && nullable_equal(decoded->query,profile->query));
        CHECK(decoded->path!=objects.items[0].source.path && event.unbound_sources==(profile->source_version ? 0u : 2u));
        CHECK(event.definitions.sources.versions.high_water==(profile->source_version ? 18u : 17u) && event.definitions.sources.versions.count==(profile->source_version ? 1u : 0u));
        if (profile->source_version) CHECK(!event.definitions.sources.versions.items[0].raw.bytes && event.definitions.sources.versions.items[0].kind==SOURCE_PARQUET);
        RangeEvent held=event; data[40]^=1;
        CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); memcpy(data,copy,written);
        if (profile->kind==SOURCE_DATABASE) {
            objects.items[0].source.source_version=moved.source.source_version=17; RangeEventInfo untouched=info; size_t sentinel=91;
            DefinitionEventInfo unsupported={0}; CHECK(definition_event_measure(view.definitions,SIZE_MAX,&unsupported)==DEFINITION_EVENT_UNSUPPORTED);
            CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_INVALID && !memcmp(&untouched,&info,sizeof(info)));
            CHECK(range_event_encode(view,SIZE_MAX,data,written,&sentinel)==RANGE_EVENT_INVALID && sentinel==91 && !memcmp(data,copy,written));
        }
        objects_free(&objects); source_versions_free(&sources); memset(data,0,written); free(data); free(copy);
        CHECK(nullable_equal(decoded->path,profile->path) && nullable_equal(decoded->schema,profile->schema) && nullable_equal(decoded->table,profile->table) && nullable_equal(decoded->query,profile->query));
        range_event_free(&event);
    }
}
static SourceVersions format_sources(void) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&column,.column_count=1}; Analysis source={.kind=SOURCE_CSV,.path="/source"}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1;
    SourcePacket raw={0}; CHECK(source_scope_raw_build(0,1,&source,&schema,&digest,"engine",&raw,NULL));
    SourceVersions rows={.high_water=9,.count=2,.capacity=2,.items=dx_alloc(2*sizeof(SourceVersion))};
    rows.items[0]=(SourceVersion){.version=1,.kind=SOURCE_CSV,.path=dx_strdup("/source"),.raw=raw};
    rows.items[1]=(SourceVersion){.owner=1,.version=2,.kind=SOURCE_CSV,.path=dx_strdup("/unused")};
    rows.bytes=2*sizeof(SourceVersion)+strlen(rows.items[0].path)+1+strlen(rows.items[1].path)+1+raw.bytes; CHECK(source_versions_valid(&rows,1,NULL)); return rows;
}
static void source_formats(void) {
    SourceVersions sources=format_sources(); FormatStore before={.source_aware=true},after={.source_aware=true};
    uint64_t id=UINT64_C(9007199254740993); size_t field=0; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    FormatScopeView raw={0,sources.items[0].raw.data,sources.items[0].raw.bytes}; CHECK(format_apply_records(&after,&raw,&id,1,&field,1,FORMAT_BOLD,bold,NULL));
    CellDelta cell={.sheet=11,.row=20,.column=4,.after="NULL",.after_bytes=4}; ColumnWidthEntry width={3,column_width_bits(123)};
    const unsigned effects[]={8,10,24,26};
    for (size_t k=0;k<sizeof(effects)/sizeof(*effects);k++) {
        unsigned effect=effects[k]; RangeEventView v={.sheet=11,.effects=effect,.before_formats=format_delta_store_view(&before),.after_formats=format_delta_store_view(&after),.object_high_water=1,.sources=&sources};
        if (effect&2) { v.cells=&cell; v.count=1; } if (effect&16) v.widths=(ColumnWidthPair){{0},{&width,1}};
        RangeEventInfo info={0}; CHECK(range_event_measure(v,SIZE_MAX,&info)==RANGE_EVENT_OK && info.format_bytes && info.source_bytes && !info.definition_bytes);
        unsigned char *data=dx_alloc(info.bytes); size_t written=17; CHECK(range_event_encode(v,SIZE_MAX,data,info.bytes,&written)==RANGE_EVENT_OK);
        RangeEventInfo framed={0}; CHECK(range_event_preflight(data,written,&framed)==RANGE_EVENT_OK && framed.owned<=info.owned && framed.validation_scratch<=info.validation_scratch);
        RangeEvent event={0}; CHECK(range_event_decode(data,written,framed.owned+framed.validation_scratch-1,&event)==RANGE_EVENT_LIMIT && !event.data);
        CHECK(range_event_decode(data,written,framed.owned+framed.validation_scratch,&event)==RANGE_EVENT_OK && event.effects==effect && event.sources.versions.count==1 && event.sources.versions.high_water==9 && event.sources.versions.items[0].version==1 && event.formats.after.items[0].record_ids[0]==id);
        CHECK(event.sources.versions.items[0].raw.data!=sources.items[0].raw.data && event.formats.after.scopes[0].definition!=after.scopes[0].definition);
        RangeEvent held=event; size_t prefix=written-info.source_bytes;
        data[prefix+8]^=1; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); data[prefix+8]^=1;
        data[48]=0; CHECK(range_event_decode(data,written,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); wire_put(data+48,info.source_bytes,4);
        size_t full=0,n=0; CHECK(source_versions_asset_size(11,&sources,1,&full)==SOURCE_VERSIONS_CODEC_OK); unsigned char *extra=dx_alloc(prefix+full); memcpy(extra,data,prefix); wire_put(extra+48,full,4);
        CHECK(source_versions_asset_encode(11,&sources,1,extra+prefix,full,&n)==SOURCE_VERSIONS_CODEC_OK && n==full);
        CHECK(range_event_decode(extra,prefix+full,SIZE_MAX,&event)==RANGE_EVENT_INVALID && !memcmp(&held,&event,sizeof(event))); free(extra);
        for (size_t cut=0;cut<written;cut++) CHECK(range_event_decode(data,cut,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&held,&event,sizeof(event)));
        size_t sentinel=73; unsigned char first=((unsigned char *)after.scopes[0].definition)[0]; CHECK(range_event_encode(v,SIZE_MAX,after.scopes[0].definition,info.bytes,&sentinel)==RANGE_EVENT_INVALID && sentinel==73 && ((unsigned char *)after.scopes[0].definition)[0]==first);
        free(data); range_event_free(&event);
    }
    FormatStore opaque={0}; const char text[]="opaque-result"; FormatScopeView unknown={0,text,sizeof(text)-1}; CHECK(format_apply_scoped(&opaque,(FormatRange){1,2,1,2},FORMAT_BOLD,bold,&unknown,1,false,NULL));
    RangeEventView bad={.sheet=11,.effects=8,.before_formats=format_delta_store_view(&before),.after_formats=format_delta_store_view(&opaque),.object_high_water=1,.sources=&sources}; RangeEventInfo info={0};
    CHECK(range_event_measure(bad,SIZE_MAX,&info)==RANGE_EVENT_UNSUPPORTED);
    bad.after_formats=format_delta_store_view(&after); SourcePacket saved=sources.items[0].raw; sources.items[0].raw=(SourcePacket){0}; sources.bytes-=saved.bytes;
    CHECK(range_event_measure(bad,SIZE_MAX,&info)==RANGE_EVENT_INVALID); sources.items[0].raw=saved; sources.bytes+=saved.bytes;
    format_free(&opaque); format_free(&before); format_free(&after); source_versions_free(&sources);
}
int main(void) {
    CellDelta cells[]={{1,1,1,"NULL","",4,0},{1,10,10,"","12345678901234567890.123456789",0,30}};
    cells[1].after_bytes=strlen(cells[1].after); RangeEventView view={.sheet=1,.effects=2,.cells=cells,.count=2}; RangeEventInfo info={0};
    CHECK(range_event_measure(view,0,&info)==RANGE_EVENT_OK && info.count==2); size_t bytes=info.bytes,written=99;
    unsigned char *data=dx_alloc(bytes+2); memset(data,0x55,bytes+2);
    CHECK(range_event_encode(view,0,data,bytes-1,&written)==RANGE_EVENT_LIMIT && written==99 && data[0]==0x55);
    CHECK(range_event_encode(view,0,data,bytes,&written)==RANGE_EVENT_OK && written==bytes && data[bytes]==0x55);
    RangeEvent event={0}; CHECK(range_event_preflight(data,bytes,&info)==RANGE_EVENT_OK);
    CHECK(range_event_decode(data,bytes,info.owned-1,&event)==RANGE_EVENT_LIMIT && !event.data);
    CHECK(range_event_decode(data,bytes,info.owned,&event)==RANGE_EVENT_OK && event.data!=data && event.cells[0].before_bytes==4 && !event.cells[0].after_bytes && !memcmp(event.cells[0].before,"NULL",4));
    CellDelta inverse=cell_delta_inverse(event.cells[1]); CHECK(inverse.before_bytes==cells[1].after_bytes && !inverse.after_bytes);
    RangeEvent before=event;
    for (size_t n=0;n<bytes;n++) CHECK(range_event_decode(data,n,SIZE_MAX,&event)!=RANGE_EVENT_OK && !memcmp(&before,&event,sizeof(event)));
    data[6]=1; CHECK(range_event_decode(data,bytes,SIZE_MAX,&event)==RANGE_EVENT_INVALID); data[6]=0;
    data[4]=1; CHECK(range_event_decode(data,bytes,SIZE_MAX,&event)==RANGE_EVENT_UNSUPPORTED); data[4]=DX_RANGE_EVENT_ENCODING;
    CellDelta duplicate[2]={cells[0],cells[0]}; RangeEventView invalid=view; invalid.cells=duplicate;
    CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_INVALID); invalid=view; invalid.count=0; CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_INVALID);
    SourceVersions ledger={0}; invalid=view; invalid.effects=10; invalid.sources=&ledger; CHECK(range_event_measure(invalid,0,&info)==RANGE_EVENT_LIMIT);
    FormatStore formats={0}; FormatSnapshot after={0}; CellFormat style=format_default(); style.flags=1;
    CHECK(format_plan_spatial(&formats,(FormatRange){1,1,1,1},FORMAT_BOLD,style,&after,NULL)); view.effects=10; view.sources=&ledger; view.before_formats=format_delta_store_view(&formats); view.after_formats=format_delta_snapshot_view(&after);
    CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_OK); free(data); data=dx_alloc(info.bytes); bytes=info.bytes;
    CHECK(range_event_encode(view,DX_FORMAT_VALIDATION_BYTES,data,bytes,&written)==RANGE_EVENT_OK && range_event_preflight(data,bytes,&info)==RANGE_EVENT_OK);
    CHECK(range_event_decode(data,bytes,info.owned+info.validation_scratch,&event)==RANGE_EVENT_OK && event.effects==10 && event.formats.after.count==1 && !event.formats.before.count);
    CHECK(event.formats.after.items!=after.items); view.effects=8; view.cells=NULL; view.count=0;
    CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_OK); view.after_formats=view.before_formats; CHECK(range_event_measure(view,DX_FORMAT_VALIDATION_BYTES,&info)==RANGE_EVENT_INVALID);
    view.after_formats=format_delta_snapshot_view(&after); view.after_formats.source_aware=true; CHECK(range_event_measure(view,SIZE_MAX,&info)==RANGE_EVENT_OK);
    compound(); objects_compound(); source_objects(); source_profiles(); source_formats(); range_event_free(&event); free(data); format_snapshot_free(&after); format_free(&formats); printf("PASS: %u complete range event checks\n",checks); return 0;
}
