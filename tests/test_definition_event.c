#include "workbook/definition_event_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL definition event:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static SourcePacket raw(uint64_t owner,uint64_t version,SourceKind kind) {
    Column col={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&col,.column_count=1}; Analysis a={.kind=kind,.path="/source"}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1;
    SourcePacket p={0}; CHECK(source_scope_raw_build(owner,version,&a,&schema,&digest,"engine",&p,NULL)); return p;
}
static SourceVersions ledger(void) {
    SourceVersions v={.high_water=20,.count=3,.capacity=3,.items=dx_alloc(3*sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){.version=1,.kind=SOURCE_CSV,.path=dx_strdup("/one.csv"),.raw=raw(0,1,SOURCE_CSV)};
    v.items[1]=(SourceVersion){.version=2,.owner=9,.kind=SOURCE_PARQUET,.path=dx_strdup("/two.parquet"),.raw=raw(9,2,SOURCE_PARQUET)};
    v.items[2]=(SourceVersion){.version=3,.owner=10,.kind=SOURCE_CSV,.path=dx_strdup("/unused.csv")};
    v.bytes=3*sizeof(SourceVersion); for (size_t i=0;i<3;i++) v.bytes+=strlen(v.items[i].path)+1+v.items[i].raw.bytes;
    CHECK(source_versions_valid(&v,10,NULL)); return v;
}
static DefinitionEventView decoded_view(const DefinitionEvent *e) {
    DefinitionEventView v={.sheet=e->sheet,.object_high_water=e->object_high_water,.effects=e->effects,.sources=&e->sources.versions};
    if (e->effects&1) { v.before.primary=&e->before_primary; v.after.primary=&e->after_primary; }
    if (e->effects&4) { v.before.objects=&e->before_objects.objects; v.after.objects=&e->after_objects.objects; }
    if (e->effects&8) { v.before_formats=format_delta_snapshot_view(&e->formats.before); v.after_formats=format_delta_snapshot_view(&e->formats.after); } return v;
}
static DefinitionEventPacket capture(DefinitionEventView v,DefinitionEventInfo *info) {
    CHECK(definition_event_measure(v,SIZE_MAX,info)==DEFINITION_EVENT_OK);
    DefinitionEventPacket p={0}; CHECK(definition_event_capture(v,info->bytes+info->validation_scratch,&p)==DEFINITION_EVENT_OK && p.bytes==info->bytes);
    DefinitionEventInfo framed={0}; CHECK(definition_event_preflight(p.data,p.bytes,&framed)==DEFINITION_EVENT_OK && framed.sheet==info->sheet && framed.effects==info->effects &&
        framed.owned==info->owned && framed.validation_scratch==info->validation_scratch && framed.source_high_water==info->source_high_water);
    DefinitionEventPlan plan={0}; CHECK(definition_event_prepare(v,info->validation_scratch,&plan)==DEFINITION_EVENT_OK);
    unsigned char *direct=dx_alloc(p.bytes+1); memset(direct,0xa5,p.bytes+1); size_t written=77;
    CHECK(definition_event_encode_unpublished(&plan,info->validation_scratch,direct,p.bytes-1,&written)==DEFINITION_EVENT_LIMIT && written==77);
    CHECK(definition_event_encode_unpublished(&plan,info->validation_scratch,direct,p.bytes,&written)==DEFINITION_EVENT_OK && written==p.bytes && direct[p.bytes]==0xa5 && !memcmp(direct,p.data,p.bytes));
    free(direct); return p;
}
static void reject(const void *data,size_t bytes,size_t budget,DefinitionEvent *out,DefinitionEventStatus expected) {
    DefinitionEvent before=*out; DefinitionEventStatus got=definition_event_decode(data,bytes,budget,out);
    if (got!=expected) fprintf(stderr,"definition event expected%d got%d\n",expected,got);
    CHECK(got==expected && !memcmp(&before,out,sizeof(before)));
}
static void primary_and_wire(void) {
    SourceVersions sources=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.source_version=1},b=a; b.query="SELECT Id FROM input";
    DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=1,.before={&a,NULL},.after={&b,NULL},.sources=&sources}; DefinitionEventInfo info={0}; DefinitionEventPacket p=capture(v,&info);
    unsigned char expected[64]={0}; memcpy(expected,"DXDE",4); expected[4]=1; expected[8]=99; expected[16]=10; expected[24]=1;
    size_t before=0,after=0; AnalysisAssetMeasure am={0}; CHECK(analysis_asset_measure(99,0,&a,10,&am)==ANALYSIS_ASSET_OK); before=am.bytes;
    CHECK(analysis_asset_measure(99,0,&b,10,&am)==ANALYSIS_ASSET_OK); after=am.bytes;
    put(expected+28,before,4); put(expected+32,after,4); put(expected+48,p.bytes-64-before-after,4);
    CHECK(!memcmp(expected,p.data,64));
    DefinitionEvent out={0}; CHECK(definition_event_decode(p.data,p.bytes,info.owned+info.validation_scratch,&out)==DEFINITION_EVENT_OK);
    CHECK(out.effects==1 && out.sheet==99 && !strcmp(out.after_primary.query,b.query) && !out.before_primary.query && out.sources.versions.count==1 && out.sources.versions.items[0].version==1);
    unsigned char *bad=dx_alloc(p.bytes+1),*data=p.data;
    for (size_t n=0;n<p.bytes;n++) reject(data,n,info.owned+info.validation_scratch,&out,DEFINITION_EVENT_INVALID);
    memcpy(bad,data,p.bytes); bad[p.bytes]=0; reject(bad,p.bytes+1,SIZE_MAX,&out,DEFINITION_EVENT_INVALID);
    struct { size_t at; unsigned value; DefinitionEventStatus status; } changes[]={
        {0,'X',DEFINITION_EVENT_INVALID},{4,2,DEFINITION_EVENT_UNSUPPORTED},{5,1,DEFINITION_EVENT_UNSUPPORTED},{6,1,DEFINITION_EVENT_INVALID},
        {24,0,DEFINITION_EVENT_INVALID},{24,8,DEFINITION_EVENT_INVALID},{24,5,DEFINITION_EVENT_INVALID},{24,14,DEFINITION_EVENT_INVALID},
        {60,1,DEFINITION_EVENT_INVALID},{28,0,DEFINITION_EVENT_INVALID},{36,1,DEFINITION_EVENT_INVALID},{52,1,DEFINITION_EVENT_INVALID},{56,1,DEFINITION_EVENT_INVALID}};
    for (size_t i=0;i<sizeof(changes)/sizeof(*changes);i++) { memcpy(bad,data,p.bytes); bad[changes[i].at]=(unsigned char)changes[i].value; reject(bad,p.bytes,SIZE_MAX,&out,changes[i].status); }
    for (size_t i=0;i<2;i++) {
        size_t at=64+(i ? before : 0);
        memcpy(bad,data,p.bytes); put(bad+at+8,100,8); reject(bad,p.bytes,SIZE_MAX,&out,DEFINITION_EVENT_INVALID);
        memcpy(bad,data,p.bytes); put(bad+at+16,9,8); reject(bad,p.bytes,SIZE_MAX,&out,DEFINITION_EVENT_INVALID);
        memcpy(bad,data,p.bytes); bad[at+4]=2; reject(bad,p.bytes,SIZE_MAX,&out,DEFINITION_EVENT_UNSUPPORTED);
    }
    reject(data,p.bytes,info.owned-1,&out,DEFINITION_EVENT_LIMIT); reject(NULL,p.bytes,SIZE_MAX,&out,DEFINITION_EVENT_INVALID);
    CHECK(definition_event_decode(data,p.bytes,SIZE_MAX,NULL)==DEFINITION_EVENT_INVALID);
    DefinitionEventPacket old=p; CHECK(definition_event_capture(v,p.bytes-1,&p)==DEFINITION_EVENT_LIMIT && p.data==old.data && p.bytes==old.bytes);
    b.source_version=4; CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_INVALID && p.data==old.data); b.source_version=1;
    b.query=a.query; CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_NOOP && p.data==old.data); b.query="SELECT Id FROM input";
    v.effects=9; CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_INVALID && p.data==old.data); v.effects=1;
    CHECK(definition_event_capture(v,SIZE_MAX,NULL)==DEFINITION_EVENT_INVALID);
    memset(data,0,p.bytes); definition_event_packet_free(&p); source_versions_free(&sources); free(bad);
    CHECK(!strcmp(out.before_primary.path,"/one.csv") && !strcmp(out.sources.versions.items[0].path,"/one.csv"));
    DefinitionEventInfo again={0}; p=capture(decoded_view(&out),&again); CHECK(again.owned==info.owned); definition_event_packet_free(&p); definition_event_free(&out);
}
static void composites(void) {
    for (unsigned mask=9;mask<=12;mask+=3) {
        SourceVersions sources=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv"},b=a; b.source_version=1;
        SheetObject x={.id=9,.name="Raw",.kind=OBJECT_TABLE,.rows=5,.columns=4,.source={.kind=SOURCE_PARQUET,.path="/two.parquet"}},y=x; y.source.source_version=2;
        ObjectSnapshot before={&x,1},after={&y,1}; FormatStore f={0}; FormatSnapshot next={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
        const SourceVersion *entry=sources.items+(mask==9 ? 0 : 1); FormatScopeView scope={entry->owner,entry->raw.data,entry->raw.bytes}; uint64_t id=9007199254740993ULL; size_t field=0;
        CHECK(format_plan_records(&f,&scope,&id,1,&field,1,FORMAT_BOLD,bold,&next,NULL));
        DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=mask,.before_formats=format_delta_store_view(&f),.after_formats=format_delta_snapshot_view(&next),.sources=&sources};
        if (mask==9) { v.before.primary=&a; v.after.primary=&b; } else { v.before.objects=&before; v.after.objects=&after; }
        DefinitionEventInfo info={0}; DefinitionEventPacket p=capture(v,&info); DefinitionEvent out={0};
        CHECK(info.unbound_sources==1 && !info.inactive_scopes && info.validation_scratch>0);
        CHECK(definition_event_decode(p.data,p.bytes,info.owned+info.validation_scratch,&out)==DEFINITION_EVENT_OK && out.effects==mask && out.sources.versions.count==1 &&
            out.formats.after.items[0].record_ids[0]==id && out.sources.versions.items[0].version==entry->version);
        reject(p.data,p.bytes,info.owned+info.validation_scratch-1,&out,DEFINITION_EVENT_LIMIT);
        DefinitionEventPacket prior=p; CHECK(definition_event_capture(v,p.bytes+info.validation_scratch-1,&p)==DEFINITION_EVENT_LIMIT && p.data==prior.data);
        SourcePacket raw_saved=sources.items[mask==9 ? 0 : 1].raw; SourceVersion *mutable=sources.items+(mask==9 ? 0 : 1); mutable->raw=(SourcePacket){0}; sources.bytes-=raw_saved.bytes;
        CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_INVALID && p.data==prior.data); mutable->raw=raw_saved; sources.bytes+=raw_saved.bytes;
        unsigned char *bad=dx_alloc(p.bytes); memcpy(bad,p.data,p.bytes); put(bad+56,0,4); reject(bad,p.bytes,SIZE_MAX,&out,DEFINITION_EVENT_INVALID); free(bad);
        format_snapshot_free(&next); format_free(&f); source_versions_free(&sources); definition_event_packet_free(&p);
        CHECK(out.unbound_sources==1 && out.formats.after.source_aware); definition_event_free(&out);
    }
}
static void object_projection_and_grammar(void) {
    SourceVersions sources={.high_water=20}; SheetObject a={.id=9,.name="Pivot",.rows=5,.columns=4},b=a;
    ObjectSnapshot before={&a,1},after={&b,1}; DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&sources};
    DefinitionEventPacket p={0}; b.rows=100; b.columns=10; b.revision=UINT64_MAX;
    CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_NOOP && !p.data);
    b.name="Renamed"; DefinitionEventInfo info={0}; p=capture(v,&info); DefinitionEvent out={0}; CHECK(definition_event_decode(p.data,p.bytes,info.owned,&out)==DEFINITION_EVENT_OK && !out.sources.versions.count && !out.after_objects.objects.items[0].revision);
    DefinitionEventPacket prior=p; v.effects=12; CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_INVALID && p.data==prior.data); v.effects=4;
    a.kind=b.kind=OBJECT_CHART; a.mark=b.mark=MARK_LINE; b.name=a.name; b.rows=a.rows+1;
    CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_OK); definition_event_packet_free(&p);
    Analysis x={.kind=SOURCE_CSV,.path="/one",.filter_column="A",.filter_value="[\"1\",\"2\"]",.filter_op=FILTER_BETWEEN},y=x; y.filter_value="[\"1\",\"3\"]";
    v=(DefinitionEventView){.sheet=99,.effects=1,.before={&x,NULL},.after={&y,NULL},.sources=&sources}; p=capture(v,&info);
    CHECK(info.validation_scratch>0); prior=p; y.filter_value="[\"only-one\"]";
    CHECK(definition_event_capture(v,SIZE_MAX,&p)==DEFINITION_EVENT_INVALID && p.data==prior.data); y.filter_value="[\"1\",\"3\"]";
    CHECK(definition_event_measure(v,info.validation_scratch-1,&info)==DEFINITION_EVENT_LIMIT);
    definition_event_free(&out); definition_event_packet_free(&p);
}
static void source_union_and_forgery(void) {
    SourceVersions sources=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.source_version=1},b=a; b.query="SELECT Id FROM input";
    DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=1,.before={&a,NULL},.after={&b,NULL},.sources=&sources}; DefinitionEventInfo info={0}; DefinitionEventPacket p=capture(v,&info);
    /* An otherwise-valid source asset with extra unreferenced rows is not a closed event. */
    size_t full=0; CHECK(source_versions_asset_size(99,&sources,10,&full)==SOURCE_VERSIONS_CODEC_OK);
    size_t old_source=0; unsigned char *original=p.data; for (unsigned i=0;i<4;i++) old_source|=(size_t)original[48+i]<<(8*i);
    size_t prefix=p.bytes-old_source,new_bytes=prefix+full; unsigned char *bad=dx_alloc(new_bytes); memcpy(bad,original,prefix); put(bad+48,full,4); size_t written=0;
    CHECK(source_versions_asset_encode(99,&sources,10,bad+prefix,full,&written)==SOURCE_VERSIONS_CODEC_OK);
    DefinitionEvent out={0}; CHECK(definition_event_preflight(bad,new_bytes,&info)==DEFINITION_EVENT_OK); reject(bad,new_bytes,SIZE_MAX,&out,DEFINITION_EVENT_INVALID);
    free(bad); definition_event_packet_free(&p); source_versions_free(&sources); definition_event_free(&out);
}
static void distinct_union_and_opaque_scopes(void) {
    SourceVersions sources=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.source_version=1},b=a; b.query="SELECT Id FROM input";
    FormatStore f={0}; FormatSnapshot next={0}; CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    SourceVersion *other=sources.items+1; FormatScopeView scope={other->owner,other->raw.data,other->raw.bytes}; uint64_t id=1; size_t field=0;
    CHECK(format_plan_records(&f,&scope,&id,1,&field,1,FORMAT_BOLD,value,&next,NULL));
    DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=9,.before={&a,NULL},.after={&b,NULL},
        .before_formats=format_delta_store_view(&f),.after_formats=format_delta_snapshot_view(&next),.sources=&sources};
    DefinitionEventInfo info={0}; DefinitionEventPacket p=capture(v,&info); DefinitionEvent out={0};
    CHECK(definition_event_decode(p.data,p.bytes,info.owned+info.validation_scratch,&out)==DEFINITION_EVENT_OK && out.sources.versions.count==2 &&
        out.sources.versions.items[0].version==1 && out.sources.versions.items[1].version==2 && !out.inactive_scopes && !out.unbound_sources);
    definition_event_free(&out); definition_event_packet_free(&p); format_snapshot_free(&next);
    const unsigned char legacy[]={'a',0,'b'}; scope=(FormatScopeView){0,legacy,sizeof(legacy)};
    CHECK(format_plan_scoped(&f,(FormatRange){0,1,0,0},FORMAT_BOLD,value,&scope,1,true,&next,NULL));
    a.source_version=b.source_version=0; v.after_formats=format_delta_snapshot_view(&next); p=capture(v,&info);
    CHECK(info.inactive_scopes==1 && info.unbound_sources==2);
    CHECK(definition_event_decode(p.data,p.bytes,info.owned+info.validation_scratch,&out)==DEFINITION_EVENT_OK && out.inactive_scopes==1 && out.unbound_sources==2 &&
        !out.sources.versions.count && out.formats.after.scopes[0].bytes==3 && !memcmp(out.formats.after.scopes[0].definition,legacy,3));
    definition_event_free(&out); definition_event_packet_free(&p); format_snapshot_free(&next); format_free(&f); source_versions_free(&sources);
}
static void large_compound_budget(void) {
    SourceVersions sources={.high_water=20};
    SheetObject a={.id=9,.name="Raw",.kind=OBJECT_TABLE,.rows=5,.columns=4,
        .source={.kind=SOURCE_CSV,.path="/one",.filter_column="A",.filter_value="",.filter_op=FILTER_CONTAINS,.filter_count=1}};
    a.source.filters[0]=(AnalysisFilter){.column="B",.value="",.op=FILTER_CONTAINS};
    ObjectSnapshot before={&a,1}; ObjectsAssetInfo initial={0}; CHECK(objects_asset_measure(99,&before,10,&initial)==OBJECTS_ASSET_OK);
    size_t total=DX_OBJECT_CURRENT_BYTES-initial.measure.owned,first=4u*1024u*1024u,second=total-first;
    char *x=dx_alloc(first+1),*y=dx_alloc(second+1); memset(x,'a',first); memset(y,'b',second);
    a.source.filter_value=x; a.source.filters[0].value=y; SheetObject b=a; b.name="New"; ObjectSnapshot after={&b,1};
    DefinitionEventView v={.sheet=99,.object_high_water=10,.effects=4,.before={NULL,&before},.after={NULL,&after},.sources=&sources};
    DefinitionEventInfo info={0}; CHECK(definition_event_measure(v,0,&info)==DEFINITION_EVENT_OK && info.bytes>8u*1024u*1024u && !info.validation_scratch);
    DefinitionEventPacket p={0}; CHECK(definition_event_capture(v,info.bytes,&p)==DEFINITION_EVENT_OK);
    DefinitionEvent out={0}; reject(p.data,p.bytes,info.owned-1,&out,DEFINITION_EVENT_LIMIT);
    CHECK(definition_event_decode(p.data,p.bytes,info.owned,&out)==DEFINITION_EVENT_OK && out.unbound_sources==2 &&
        !strcmp(out.before_objects.objects.items[0].name,"Raw") && !strcmp(out.after_objects.objects.items[0].name,"New") &&
        strlen(out.before_objects.objects.items[0].source.filter_value)==first && strlen(out.after_objects.objects.items[0].source.filters[0].value)==second);
    free(x); free(y); definition_event_packet_free(&p); definition_event_free(&out);
}
int main(void) { primary_and_wire(); composites(); object_projection_and_grammar(); source_union_and_forgery(); distinct_union_and_opaque_scopes(); large_compound_budget(); definition_event_packet_free(NULL); definition_event_free(NULL); printf("PASS: %u closed compound definition event checks\n",checks); return 0; }
