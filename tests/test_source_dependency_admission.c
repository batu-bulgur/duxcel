#include "workbook/source_dependency_admission.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL dependency admission:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SourcePacket raw(uint64_t owner,uint64_t version,SourceKind kind,unsigned salt) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Analysis a={.kind=kind,.path="/source"}; Page p={.columns=&column,.column_count=1}; SourceDigest d={.bytes=42,.identity={.size=42}}; d.sha256[0]=(unsigned char)salt;
    SourcePacket packet={0}; CHECK(source_scope_raw_build(owner,version,&a,&p,&d,"engine",&packet,NULL)); return packet;
}
static SourceVersions ledger(void) {
    SourceVersions v={.high_water=20,.count=3,.capacity=3,.items=dx_alloc(3*sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){0,1,SOURCE_CSV,dx_strdup("/one.csv"),raw(0,1,SOURCE_CSV,1)};
    v.items[1]=(SourceVersion){9,2,SOURCE_PARQUET,dx_strdup("/two.parquet"),{0}};
    v.items[2]=(SourceVersion){10,3,SOURCE_CSV,dx_strdup("/unused.csv"),{0}};
    v.bytes=3*sizeof(SourceVersion); for (size_t i=0;i<3;i++) v.bytes+=strlen(v.items[i].path)+1+v.items[i].raw.bytes;
    CHECK(source_versions_valid(&v,10,NULL)); return v;
}
static void decoded(SourceVersionsAsset *asset) {
    size_t bytes=0,written=0; CHECK(source_versions_asset_size(asset->sheet,&asset->versions,UINT64_MAX,&bytes)==SOURCE_VERSIONS_CODEC_OK);
    void *wire=dx_alloc(bytes); CHECK(source_versions_asset_encode(asset->sheet,&asset->versions,UINT64_MAX,wire,bytes,&written)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(source_versions_asset_decode(wire,written,UINT64_MAX,SIZE_MAX,asset)==SOURCE_VERSIONS_CODEC_OK); free(wire);
}
static void definitions(void) {
    SourceVersions live=ledger(); Analysis before={.kind=SOURCE_CSV,.path="/one.csv",.source_version=1},after=before; after.source_version=0;
    SheetObject table={.id=9,.kind=OBJECT_TABLE,.name="Table",.rows=5,.columns=4,.source={.kind=SOURCE_PARQUET,.path="/two.parquet",.source_version=2}};
    ObjectSnapshot objects={&table,1}; DefinitionDependencyView a={&before,&objects},b={&after,&objects}; DefinitionDependencies captured={0};
    CHECK(definition_dependencies_build(7,a,b,&live,10,SIZE_MAX,&captured)==DEFINITION_DEPENDENCIES_OK); decoded(&captured.sources);
    SourceDependencyLive current={7,10,&live}; SourceDependencyAdmission out={0};
    CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_EXACT && out.sources.exact==2 && out.sources.captured_pending==1 && out.unbound_sources==1 && !out.opaque_scopes);
    live.items[1].raw=raw(9,2,SOURCE_PARQUET,1); live.bytes+=live.items[1].raw.bytes;
    CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_REFINED && out.sources.refined==1 && out.sources.captured_pending==1 && out.unbound_sources==1);
    CHECK(!captured.sources.versions.items[1].raw.bytes && captured.unbound_sources==1 && live.high_water==20);
    SourceDependencyAdmission prior=out; captured.unbound_sources++;
    CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&prior,&out,sizeof(out))); captured.unbound_sources--;
    table.id=8; CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&prior,&out,sizeof(out))); table.id=9;
    before.source_version=99; CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&prior,&out,sizeof(out))); before.source_version=1;
    bool all[]={true,true,true}; DefinitionDependencies extra={.unbound_sources=captured.unbound_sources};
    CHECK(source_versions_asset_select(7,&live,10,all,3,SIZE_MAX,&extra.sources)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(source_definition_dependencies_admit(a,b,&extra,current,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&prior,&out,sizeof(out))); definition_dependencies_free(&extra);
    SourceDependencyLive wrong=current; wrong.sheet=8;
    CHECK(source_definition_dependencies_admit(a,b,&captured,wrong,&out)==SOURCE_DEPENDENCY_CONFLICT && out.sources.conflicting==2);
    live.items[0].path[1]='X'; CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_CONFLICT && out.sources.conflicting==1); live.items[0].path[1]='o';
    captured.sources.sheet=0; prior=out;
    CHECK(source_definition_dependencies_admit(a,b,&captured,current,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&prior,&out,sizeof(out))); captured.sources.sheet=7;
    source_versions_free(&live); definition_dependencies_free(&captured);
}
static void formats(void) {
    SourceVersions live=ledger(); FormatStore before={0},after={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD;
    uint64_t record=1; size_t field=0; FormatScopeView scope={0,live.items[0].raw.data,live.items[0].raw.bytes};
    CHECK(format_apply_records(&before,&scope,&record,1,&field,1,FORMAT_BOLD,bold,NULL)); format_copy(&after,&before);
    FormatSnapshotView a=format_delta_store_view(&before),b=format_delta_store_view(&after); FormatDependencies captured={0};
    CHECK(format_dependencies_build(7,a,b,&live,10,SIZE_MAX,&captured)==FORMAT_DEPENDENCIES_OK); decoded(&captured.sources);
    size_t scratch=0; CHECK(source_format_dependencies_scratch(a,b,&scratch)==SOURCE_DEPENDENCY_EXACT && scratch);
    SourceDependencyLive current={7,10,&live}; SourceDependencyAdmission out={0};
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_EXACT && out.sources.exact==1 && !out.sources.captured_pending && !out.opaque_scopes);
    SourceDependencyAdmission prior=out;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch-1,&out)==SOURCE_DEPENDENCY_LIMIT && !memcmp(&out,&prior,sizeof(out)));
    SourcePacket saved=live.items[0].raw; live.items[0].raw=(SourcePacket){0}; live.bytes-=saved.bytes;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_UNAVAILABLE && out.sources.unavailable==1 && !out.sources.captured_pending);
    live.items[0].raw=raw(0,1,SOURCE_CSV,2); live.bytes+=live.items[0].raw.bytes;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_CONFLICT && out.sources.conflicting==1);
    source_packet_free(&live.items[0].raw); live.items[0].raw=saved;
    SourceDependencyLive wrong=current; wrong.sheet=8;
    CHECK(source_format_dependencies_admit(a,b,&captured,wrong,scratch,&out)==SOURCE_DEPENDENCY_CONFLICT);
    prior=out; before.scopes[0].owner=9;
    CHECK(source_format_dependencies_admit(format_delta_store_view(&before),b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out))); before.scopes[0].owner=0;
    before.items[0].fields[0]=1;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out))); before.items[0].fields[0]=0;
    captured.legacy_scopes=1;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out))); captured.legacy_scopes=0;
    SourcePacket captured_raw=captured.sources.versions.items[0].raw; captured.sources.versions.items[0].raw=(SourcePacket){0}; captured.sources.versions.bytes-=captured_raw.bytes;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out)));
    captured.sources.versions.items[0].raw=captured_raw; captured.sources.versions.bytes+=captured_raw.bytes;
    SourcePacket mismatched=raw(0,1,SOURCE_CSV,9);
    captured.sources.versions.items[0].raw=mismatched;
    captured.sources.versions.bytes=captured.sources.versions.bytes-captured_raw.bytes+mismatched.bytes;
    CHECK(source_format_dependencies_admit(a,b,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out)));
    captured.sources.versions.items[0].raw=captured_raw;
    captured.sources.versions.bytes=captured.sources.versions.bytes-mismatched.bytes+captured_raw.bytes; source_packet_free(&mismatched);
    FormatStore wrong_version={0}; SourcePacket second=raw(0,2,SOURCE_CSV,1); FormatScopeView second_scope={0,second.data,second.bytes};
    CHECK(format_apply_records(&wrong_version,&second_scope,&record,1,&field,1,FORMAT_BOLD,bold,NULL));
    CHECK(source_format_dependencies_admit(format_delta_store_view(&wrong_version),b,&captured,current,SIZE_MAX,&out)==SOURCE_DEPENDENCY_INVALID && !memcmp(&out,&prior,sizeof(out)));
    format_free(&wrong_version); source_packet_free(&second);
    format_dependencies_free(&captured); format_free(&before); format_free(&after); source_versions_free(&live);
}
static void inactive(void) {
    SourceVersions live={.high_water=41}; Analysis a={.kind=SOURCE_DATABASE,.path="/offline.duckdb"}; DefinitionDependencies definitions={0};
    CHECK(definition_dependencies_build(7,(DefinitionDependencyView){&a,NULL},(DefinitionDependencyView){&a,NULL},&live,0,0,&definitions)==DEFINITION_DEPENDENCIES_OK);
    SourceDependencyAdmission out={0}; SourceDependencyLive current={7,0,&live};
    CHECK(source_definition_dependencies_admit((DefinitionDependencyView){&a,NULL},(DefinitionDependencyView){&a,NULL},&definitions,current,&out)==SOURCE_DEPENDENCY_EXACT && out.unbound_sources==2 && !out.sources.exact);
    FormatStore store={0}; CellFormat bold=format_default(); bold.flags=FORMAT_BOLD; FormatScopeView opaque={0,"opaque",6};
    CHECK(format_apply_scoped(&store,(FormatRange){0},FORMAT_BOLD,bold,&opaque,1,true,NULL));
    FormatSnapshotView view=format_delta_store_view(&store); FormatDependencies captured={0};
    CHECK(format_dependencies_build(7,view,view,&live,0,0,&captured)==FORMAT_DEPENDENCIES_OK && captured.legacy_scopes==2);
    size_t scratch=0; CHECK(source_format_dependencies_scratch(view,view,&scratch)==SOURCE_DEPENDENCY_EXACT);
    CHECK(source_format_dependencies_admit(view,view,&captured,current,scratch,&out)==SOURCE_DEPENDENCY_EXACT && out.opaque_scopes==2 && !out.sources.exact && !out.sources.captured_pending && !out.unbound_sources);
    SourceDependencyAdmission prior=out; FormatStore record_store={0}; uint64_t id=1; size_t field=0;
    CHECK(format_apply_records(&record_store,&opaque,&id,1,&field,1,FORMAT_BOLD,bold,NULL));
    CHECK(source_format_dependencies_admit(format_delta_store_view(&record_store),view,&captured,current,SIZE_MAX,&out)==SOURCE_DEPENDENCY_UNSUPPORTED && !memcmp(&prior,&out,sizeof(out)));
    format_free(&record_store); format_free(&store); format_dependencies_free(&captured); definition_dependencies_free(&definitions);
}
int main(void) { definitions(); formats(); inactive(); printf("PASS: %u inactive dependency consumer admission checks\n",checks); return 0; }
