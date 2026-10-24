#include "workbook/format_dependencies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL format dependencies:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SourcePacket packet(uint64_t owner,uint64_t version,SourceKind kind,unsigned salt) {
    Column cols[2]={{.name="Id",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}},
        {.name="Amount",.type=DUCKDB_TYPE_DECIMAL,.type_name="DECIMAL(38,4)",.numeric=true,.number_type={NUM_DECIMAL,38,4}}};
    Page schema={.columns=cols,.column_count=2}; Analysis a={.kind=kind,.path="/source"};
    SourceDigest d={.bytes=100,.observed_stamp=11}; d.identity.size=100; d.sha256[0]=(unsigned char)salt;
    SourcePacket out={0}; CHECK(source_scope_raw_build(owner,version,&a,&schema,&d,"engine",&out,NULL)); return out;
}
static SourceVersions ledger(void) {
    SourceVersions v={.high_water=20,.count=3,.capacity=3,.items=dx_alloc(3*sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){.owner=0,.version=1,.kind=SOURCE_CSV,.path=dx_strdup("/one.csv"),.raw=packet(0,1,SOURCE_CSV,1)};
    v.items[1]=(SourceVersion){.owner=9,.version=2,.kind=SOURCE_PARQUET,.path=dx_strdup("/two.parquet"),.raw=packet(9,2,SOURCE_PARQUET,2)};
    v.items[2]=(SourceVersion){.owner=10,.version=3,.kind=SOURCE_CSV,.path=dx_strdup("/pending.csv")};
    v.bytes=3*sizeof(SourceVersion);
    for (size_t i=0;i<3;i++) v.bytes+=strlen(v.items[i].path)+1+v.items[i].raw.bytes;
    CHECK(source_versions_valid(&v,10,NULL)); return v;
}
static void rejected(FormatSnapshotView a,FormatSnapshotView b,const SourceVersions *v,size_t budget,FormatDependencies *out,FormatDependenciesStatus status) {
    unsigned char before[sizeof(*out)]; memcpy(before,out,sizeof(before));
    CHECK(format_dependencies_build(99,a,b,v,10,budget,out)==status && !memcmp(before,out,sizeof(before)));
}
int main(void) {
    SourceVersions v=ledger(); FormatStore before={0},after={0}; CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    uint64_t ids[]={1,3}; size_t fields[]={0,1};
    FormatScopeView raw1={0,v.items[0].raw.data,v.items[0].raw.bytes},raw2={9,v.items[1].raw.data,v.items[1].raw.bytes};
    CHECK(format_apply_records(&before,&raw1,ids,2,fields,2,FORMAT_BOLD,value,NULL)); format_copy(&after,&before);
    SourcePacket result={0}; Analysis a={.kind=SOURCE_CSV,.path="/one.csv",.sort_column="Id",.descending=true};
    CHECK(source_scope_result_build(&v.items[0].raw,&a,&result,NULL)); FormatScopeView transformed={0,result.data,result.bytes};
    value.flags=FORMAT_ITALIC; CHECK(format_apply_scoped(&after,(FormatRange){0,3,0,1},FORMAT_ITALIC,value,&transformed,1,false,NULL));
    value.flags=FORMAT_UNDERLINE; CHECK(format_apply_records(&after,&raw2,ids,2,fields,2,FORMAT_UNDERLINE,value,NULL));
    FormatSnapshotView bv=format_delta_store_view(&before),av=format_delta_store_view(&after); FormatDependencies out={0};
    CHECK(format_dependencies_build(99,bv,av,&v,10,DX_SOURCE_VERSIONS_BYTES,&out)==FORMAT_DEPENDENCIES_OK);
    CHECK(out.sources.sheet==99 && out.sources.versions.high_water==20 && out.sources.versions.count==2 && !out.legacy_scopes && out.sources.versions.items[0].version==1 && out.sources.versions.items[1].version==2);
    CHECK(out.sources.versions.items[0].path!=v.items[0].path && out.sources.versions.items[1].raw.data!=v.items[1].raw.data && source_versions_valid(&out.sources.versions,10,NULL));
    rejected(bv,av,&v,out.sources.versions.bytes-1,&out,FORMAT_DEPENDENCIES_LIMIT);
    SourcePacket saved=v.items[0].raw; v.items[0].raw=(SourcePacket){0}; v.bytes-=saved.bytes;
    rejected(bv,av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_MISSING); v.items[0].raw=saved; v.bytes+=saved.bytes;
    SourceVersions missing={0}; SourceVersions tail={.high_water=20,.count=2,.capacity=2,.items=v.items+1,.bytes=2*sizeof(SourceVersion)+strlen(v.items[1].path)+1+v.items[1].raw.bytes+strlen(v.items[2].path)+1};
    CHECK(source_versions_copy(&missing,&tail,NULL)); rejected(bv,av,&missing,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_MISSING); source_versions_free(&missing);
    SourcePacket different=packet(0,1,SOURCE_CSV,77); v.items[0].raw=different; v.bytes=v.bytes-saved.bytes+different.bytes;
    rejected(bv,av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_CONFLICT); v.items[0].raw=saved; v.bytes=v.bytes-different.bytes+saved.bytes; source_packet_free(&different);
    before.scopes[0].owner=8; rejected(format_delta_store_view(&before),av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_INVALID); before.scopes[0].owner=0;
    before.items[0].fields[1]=2; rejected(bv,av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_INVALID); before.items[0].fields[1]=1;
    before.items[0].guard_ids[0]=before.scope_count; rejected(bv,av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_INVALID); before.items[0].guard_ids[0]=0;
    unsigned char *definition=before.scopes[0].definition; unsigned char old=definition[4]; definition[4]=2;
    rejected(bv,av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_INVALID); definition[4]=old;
    FormatStore wrong={0}; value.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&wrong,&transformed,ids,2,fields,2,FORMAT_BOLD,value,NULL));
    rejected(format_delta_store_view(&wrong),av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_INVALID); format_free(&wrong);
    unsigned char prior[sizeof(out)]; memcpy(prior,&out,sizeof(out));
    CHECK(format_dependencies_build(0,bv,av,&v,10,DX_SOURCE_VERSIONS_BYTES,&out)==FORMAT_DEPENDENCIES_INVALID && !memcmp(prior,&out,sizeof(out)));
    CHECK(format_dependencies_build(99,bv,av,&v,9,DX_SOURCE_VERSIONS_BYTES,&out)==FORMAT_DEPENDENCIES_INVALID && !memcmp(prior,&out,sizeof(out)));
    CHECK(format_dependencies_build(99,bv,av,&v,10,DX_SOURCE_VERSIONS_BYTES,NULL)==FORMAT_DEPENDENCIES_INVALID);
    CHECK(format_dependencies_build(99,bv,av,&out.sources.versions,10,DX_SOURCE_VERSIONS_BYTES,&out)==FORMAT_DEPENDENCIES_INVALID && !memcmp(prior,&out,sizeof(out)));
    FormatStore legacy={0}; FormatScopeView opaque={7,"legacy",6};
    CHECK(format_apply_scoped(&legacy,(FormatRange){0},FORMAT_BOLD,value,&opaque,1,true,NULL));
    FormatDependencies old_meta={0}; CHECK(format_dependencies_build(99,format_delta_store_view(&legacy),format_delta_store_view(&legacy),&v,10,0,&old_meta)==FORMAT_DEPENDENCIES_OK);
    CHECK(old_meta.legacy_scopes==2 && !old_meta.sources.versions.count && old_meta.sources.versions.high_water==20);
    CHECK(format_apply_records(&wrong,&opaque,ids,2,fields,2,FORMAT_BOLD,value,NULL));
    rejected(format_delta_store_view(&wrong),av,&v,DX_SOURCE_VERSIONS_BYTES,&out,FORMAT_DEPENDENCIES_UNSUPPORTED); format_free(&wrong); format_free(&legacy); format_dependencies_free(&old_meta);
    /* An orphaned scope never activates a record or invents a metadata edge. */
    FormatScope unused={123,(void *)"DXFS",4}; FormatStore orphan={.source_aware=true,.scopes=&unused,.scope_count=1};
    CHECK(format_dependencies_build(99,format_delta_store_view(&orphan),(FormatSnapshotView){0},&v,10,0,&old_meta)==FORMAT_DEPENDENCIES_OK && !old_meta.legacy_scopes && !old_meta.sources.versions.count);
    format_dependencies_free(&old_meta);
    source_packet_free(&result); format_free(&before); format_free(&after); source_versions_free(&v);
    CHECK(!strcmp(out.sources.versions.items[0].path,"/one.csv") && !strcmp(out.sources.versions.items[1].path,"/two.parquet"));
    SourceScopeInfo info={0}; CHECK(source_scope_validate(out.sources.versions.items[1].raw.data,out.sources.versions.items[1].raw.bytes,&info,NULL) && info.owner==9 && info.version==2 && info.source_kind==SOURCE_PARQUET);
    format_dependencies_free(&out); format_dependencies_free(NULL);
    printf("PASS: %u exact format/source dependency closure checks\n",checks); return 0;
}
