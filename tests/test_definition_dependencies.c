#include "workbook/definition_dependencies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL definition dependencies:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SourcePacket packet(uint64_t owner,uint64_t version,SourceKind kind) {
    Column column={.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}};
    Page schema={.columns=&column,.column_count=1}; Analysis a={.kind=kind,.path="/source"}; SourceDigest digest={.bytes=42}; digest.identity.size=42; digest.sha256[0]=1;
    SourcePacket p={0}; CHECK(source_scope_raw_build(owner,version,&a,&schema,&digest,"engine",&p,NULL)); return p;
}
static SourceVersions ledger(void) {
    SourceVersions v={.high_water=UINT64_MAX,.count=4,.capacity=4,.items=dx_alloc(4*sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){.owner=0,.version=1,.kind=SOURCE_CSV,.path=dx_strdup("/one.csv"),.raw=packet(0,1,SOURCE_CSV)};
    v.items[1]=(SourceVersion){.owner=9,.version=2,.kind=SOURCE_PARQUET,.path=dx_strdup("/two.parquet")};
    v.items[2]=(SourceVersion){.owner=10,.version=3,.kind=SOURCE_CSV,.path=dx_strdup("/unused.csv"),.raw=packet(10,3,SOURCE_CSV)};
    v.items[3]=(SourceVersion){.owner=0,.version=4,.kind=SOURCE_CSV,.path=dx_strdup("/new.csv"),.raw=packet(0,4,SOURCE_CSV)};
    v.bytes=4*sizeof(SourceVersion); for (size_t i=0;i<4;i++) v.bytes+=strlen(v.items[i].path)+1+v.items[i].raw.bytes;
    CHECK(source_versions_valid(&v,10,NULL)); return v;
}
static void reject(DefinitionDependencyView before,DefinitionDependencyView after,const SourceVersions *v,size_t budget,DefinitionDependencies *out,DefinitionDependenciesStatus expected) {
    DefinitionDependencies prior=*out; CHECK(definition_dependencies_build(99,before,after,v,10,budget,out)==expected && !memcmp(&prior,out,sizeof(prior)));
}
static void subsets(void) {
    SourceVersions v=ledger(); CHECK(!source_versions_find(NULL,1) && !source_versions_find(&v,0) && !source_versions_find(&v,5));
    CHECK(source_versions_find(&v,1)==v.items && source_versions_find(&v,2)==v.items+1 && source_versions_find(&v,3)==v.items+2);
    bool selected[]={true,true,false,false}; SourceVersionsAsset out={0};
    CHECK(source_versions_asset_select(99,&v,10,selected,4,DX_SOURCE_VERSIONS_BYTES,&out)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(out.versions.count==2 && out.versions.capacity==2 && out.versions.high_water==UINT64_MAX && !out.versions.items[1].raw.data && !out.versions.items[1].raw.bytes);
    CHECK(out.versions.items[0].raw.data!=v.items[0].raw.data && out.versions.items[1].path!=v.items[1].path && source_versions_valid(&out.versions,10,NULL));
    SourceVersionsSelectionMeasure measured={0};
    CHECK(source_versions_selection_measure(99,&v,10,selected,4,&measured)==SOURCE_VERSIONS_CODEC_OK && measured.count==2 && measured.owned==out.versions.bytes);
    unsigned char *direct=dx_alloc(measured.bytes),*owned=dx_alloc(measured.bytes); size_t written=77,encoded=0;
    memset(direct,0xa5,measured.bytes);
    CHECK(source_versions_selection_encode(99,&v,10,selected,4,direct,measured.bytes-1,&written)==SOURCE_VERSIONS_CODEC_LIMIT && written==77 && direct[0]==0xa5);
    CHECK(source_versions_selection_encode(99,&v,10,selected,4,direct,measured.bytes,&written)==SOURCE_VERSIONS_CODEC_OK && written==measured.bytes);
    CHECK(source_versions_asset_encode(99,&out.versions,10,owned,measured.bytes,&encoded)==SOURCE_VERSIONS_CODEC_OK && encoded==written && !memcmp(direct,owned,written));
    free(direct); free(owned);
    SourceVersionsAsset prior=out;
    CHECK(source_versions_asset_select(99,&v,10,selected,4,out.versions.bytes-1,&out)==SOURCE_VERSIONS_CODEC_LIMIT && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(source_versions_asset_select(99,&v,10,selected,2,SIZE_MAX,&out)==SOURCE_VERSIONS_CODEC_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(source_versions_asset_select(99,&v,10,NULL,4,SIZE_MAX,&out)==SOURCE_VERSIONS_CODEC_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(source_versions_asset_select(99,&out.versions,10,selected,2,SIZE_MAX,&out)==SOURCE_VERSIONS_CODEC_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(source_versions_asset_select(99,&v,10,selected,4,SIZE_MAX,NULL)==SOURCE_VERSIONS_CODEC_INVALID);
    source_versions_free(&v); SourceScopeInfo copied={0};
    CHECK(!strcmp(out.versions.items[1].path,"/two.parquet") && source_scope_validate(out.versions.items[0].raw.data,out.versions.items[0].raw.bytes,&copied,NULL) && copied.version==1 && copied.owner==0);
    source_versions_asset_free(&out);
    SourceVersions empty={.high_water=UINT64_MAX}; CHECK(source_versions_asset_select(99,&empty,0,NULL,0,0,&out)==SOURCE_VERSIONS_CODEC_OK && !out.versions.items && out.versions.high_water==UINT64_MAX);
    source_versions_asset_free(&out);
}
static void definitions(void) {
    SourceVersions v=ledger(); Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.source_version=1},b=a; b.query="SELECT * FROM input";
    SheetObject old={.id=9,.name="Raw",.rows=5,.columns=4,.kind=OBJECT_TABLE,.source={.kind=SOURCE_PARQUET,.path="/two.parquet",.source_version=2}},next=old;
    next.source.query="SELECT Id FROM input"; ObjectSnapshot o={&old,1},n={&next,1};
    DefinitionDependencyView before={&a,&o},after={&b,&n}; DefinitionDependencies out={0};
    CHECK(definition_dependencies_build(99,before,after,&v,10,DX_SOURCE_VERSIONS_BYTES,&out)==DEFINITION_DEPENDENCIES_OK);
    CHECK(out.sources.versions.count==2 && out.sources.versions.high_water==UINT64_MAX && !out.unbound_sources &&
        out.sources.versions.items[0].version==1 && out.sources.versions.items[1].version==2 && !out.sources.versions.items[1].raw.bytes);
    CHECK(!memcmp(out.sources.versions.items[0].raw.data,v.items[0].raw.data,v.items[0].raw.bytes));
    DefinitionDependencySelection selected={0};
    CHECK(definition_dependencies_select(99,before,after,&v,10,&selected)==DEFINITION_DEPENDENCIES_OK && selected.rows[0] && selected.rows[1] && !selected.rows[2] && !selected.rows[3] && !selected.unbound_sources);
    DefinitionDependencySelection single={0};
    CHECK(definition_dependencies_select_state(99,before,&v,10,&single)==DEFINITION_DEPENDENCIES_OK && single.rows[0] && single.rows[1] && !single.rows[2] && !single.unbound_sources);
    b.source_version=0;
    CHECK(definition_dependencies_select_state(99,after,&v,10,&single)==DEFINITION_DEPENDENCIES_OK && !single.rows[0] && single.rows[1] && !single.rows[2] && single.unbound_sources==1);
    b.source_version=1; DefinitionDependencySelection single_prior=single;
    CHECK(definition_dependencies_select_state(0,before,&v,10,&single)==DEFINITION_DEPENDENCIES_INVALID && !memcmp(&single,&single_prior,sizeof(single)));
    old.id=8;
    CHECK(definition_dependencies_select_state(99,before,&v,10,&single)==DEFINITION_DEPENDENCIES_CONFLICT && !memcmp(&single,&single_prior,sizeof(single))); old.id=9;
    CHECK(definition_dependencies_select_state(99,before,&v,8,&single)==DEFINITION_DEPENDENCIES_INVALID && !memcmp(&single,&single_prior,sizeof(single)));
    DefinitionDependencySelection original_selection=selected; b.source_version=5;
    CHECK(definition_dependencies_select(99,before,after,&v,10,&selected)==DEFINITION_DEPENDENCIES_MISSING && !memcmp(&selected,&original_selection,sizeof(selected))); b.source_version=1;

    reject(before,after,&v,out.sources.versions.bytes-1,&out,DEFINITION_DEPENDENCIES_LIMIT);
    b.source_version=5; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_MISSING); b.source_version=1;
    b.path="/moved.csv"; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_CONFLICT); b.path="/one.csv";
    b.kind=SOURCE_PARQUET; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_CONFLICT); b.kind=SOURCE_CSV;
    next.id=8; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_CONFLICT); next.id=9;
    b.query=NULL; b.kind=SOURCE_DATABASE; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_UNSUPPORTED);
    b.kind=SOURCE_NONE; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_INVALID); b.kind=SOURCE_CSV;
    b.path=NULL; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_INVALID); b.path="/one.csv";
    b.filter_count=16; reject(before,after,&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_INVALID); b.filter_count=0;
    reject(before,(DefinitionDependencyView){NULL,&n},&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_INVALID);
    reject(before,(DefinitionDependencyView){&b,NULL},&v,SIZE_MAX,&out,DEFINITION_DEPENDENCIES_INVALID);
    DefinitionDependencies prior=out;
    CHECK(definition_dependencies_build(0,before,after,&v,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(definition_dependencies_build(99,before,after,&out.sources.versions,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(definition_dependencies_build(99,before,after,&v,8,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_INVALID && !memcmp(&prior,&out,sizeof(prior)));
    CHECK(definition_dependencies_build(99,before,after,&v,10,SIZE_MAX,NULL)==DEFINITION_DEPENDENCIES_INVALID);
    b.source_version=4; b.path="/new.csv";
    CHECK(definition_dependencies_build(99,before,after,&v,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_OK && out.sources.versions.count==3 &&
        out.sources.versions.items[0].version==1 && out.sources.versions.items[1].version==2 && out.sources.versions.items[2].version==4);
    b.path="/one.csv";
    b.source_version=0;
    CHECK(definition_dependencies_build(99,before,after,&v,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_OK && out.unbound_sources==1 && out.sources.versions.count==2);
    a.source_version=0;
    CHECK(definition_dependencies_build(99,before,after,&v,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_OK && out.unbound_sources==2 && out.sources.versions.count==1 && out.sources.versions.items[0].version==2);
    a.kind=b.kind=SOURCE_DEMO; a.path=b.path=NULL;
    CHECK(definition_dependencies_build(99,before,after,&v,10,SIZE_MAX,&out)==DEFINITION_DEPENDENCIES_OK && out.unbound_sources==2 && out.sources.versions.count==1);
    source_versions_free(&v); CHECK(!strcmp(out.sources.versions.items[0].path,"/two.parquet") && !out.sources.versions.items[0].raw.data);
    definition_dependencies_free(&out); definition_dependencies_free(NULL);
    SourceVersions empty={.high_water=50};
    CHECK(definition_dependencies_build(99,(DefinitionDependencyView){0},(DefinitionDependencyView){0},&empty,0,0,&out)==DEFINITION_DEPENDENCIES_OK &&
        out.sources.versions.high_water==50 && !out.sources.versions.count && !out.unbound_sources);
    definition_dependencies_free(&out);
}
int main(void) { subsets(); definitions(); printf("PASS: %u exact definition/source dependency checks\n",checks); return 0; }
