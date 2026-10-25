#include "workbook/source_versions_admission.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source subset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void row(SourceVersions *v,uint64_t id,uint64_t owner,bool complete,unsigned seed) {
    v->items=dx_realloc(v->items,(v->count+1)*sizeof(*v->items));
    SourceVersion *r=v->items+v->count++; *r=(SourceVersion){owner,id,SOURCE_PARQUET,dx_strdup("/exact.parquet"),{0}};
    if (complete) {
        Column column={.name="Amount",.type=DUCKDB_TYPE_DECIMAL,.type_name="DECIMAL(38,4)",.numeric=true,.number_type={NUM_DECIMAL,38,4}};
        Page schema={.columns=&column,.column_count=1}; Analysis a={.kind=r->kind,.path=r->path}; SourceDigest digest={.bytes=100+seed};
        digest.identity.size=digest.bytes; memset(digest.sha256,(int)seed,sizeof(digest.sha256));
        CHECK(source_scope_raw_build(owner,id,&a,&schema,&digest,"exact-engine",&r->raw,NULL));
    }
    v->capacity=v->count; v->bytes+=sizeof(*r)+strlen(r->path)+1+r->raw.bytes; if (id>v->high_water) v->high_water=id;
    CHECK(source_versions_valid(v,UINT64_MAX,NULL));
}
static SourceVersionsAsset capture(const SourceVersions *v) {
    bool selected[3]={true,false,true}; SourceVersionsAsset selected_asset={0},out={0};
    CHECK(source_versions_asset_select(7,v,UINT64_MAX,selected,3,SIZE_MAX,&selected_asset)==SOURCE_VERSIONS_CODEC_OK);
    size_t bytes=0,written=0; CHECK(source_versions_asset_size(7,&selected_asset.versions,UINT64_MAX,&bytes)==SOURCE_VERSIONS_CODEC_OK);
    void *wire=dx_alloc(bytes); CHECK(source_versions_asset_encode(7,&selected_asset.versions,UINT64_MAX,wire,bytes,&written)==SOURCE_VERSIONS_CODEC_OK && written==bytes);
    CHECK(source_versions_asset_decode(wire,bytes,UINT64_MAX,SIZE_MAX,&out)==SOURCE_VERSIONS_CODEC_OK);
    free(wire); source_versions_asset_free(&selected_asset); return out;
}
static void scenarios(void) {
    for (unsigned mode=0;mode<14;mode++) {
        SourceVersions live={0}; row(&live,2,0,false,1); row(&live,6,3,true,1); row(&live,9,7,true,1);
        SourceVersionsAsset saved=capture(&live); SourceSubsetAdmission out={0}; SourceSubsetStatus expected=SOURCE_SUBSET_EXACT;
        if (mode==1) { row(&live,10,8,false,0); live.high_water=UINT64_MAX; }
        if (mode==2) {
            SourceVersions replacement={0}; row(&replacement,2,0,true,2); row(&replacement,6,3,true,1); row(&replacement,9,7,true,1);
            source_versions_free(&live); live=replacement; expected=SOURCE_SUBSET_REFINED;
        }
        if (mode==3) { live.bytes-=live.items[2].raw.bytes; source_packet_free(&live.items[2].raw); expected=SOURCE_SUBSET_UNAVAILABLE; }
        if (mode==4) { source_versions_free(&live); row(&live,2,0,false,0); live.high_water=9; expected=SOURCE_SUBSET_UNAVAILABLE; }
        if (mode==5) { live.items[0].owner=1; expected=SOURCE_SUBSET_CONFLICT; }
        if (mode==6) { live.items[0].kind=SOURCE_CSV; expected=SOURCE_SUBSET_CONFLICT; }
        if (mode==7) { live.items[0].path[1]='E'; expected=SOURCE_SUBSET_CONFLICT; }
        if (mode==8) {
            SourceVersions replacement={0}; row(&replacement,2,0,false,0); row(&replacement,6,3,true,1); row(&replacement,9,7,true,2);
            source_versions_free(&live); live=replacement; expected=SOURCE_SUBSET_CONFLICT;
        }
        if (mode==9) { saved.versions.high_water=UINT64_MAX; expected=SOURCE_SUBSET_UNAVAILABLE; }
        if (mode==10) expected=SOURCE_SUBSET_UNAVAILABLE;
        if (mode==11) expected=SOURCE_SUBSET_CONFLICT;
        if (mode==12) {
            SourceVersions replacement={0}; row(&replacement,2,0,true,2); row(&replacement,6,3,true,1); row(&replacement,9,7,true,2);
            source_versions_free(&live); live=replacement; expected=SOURCE_SUBSET_CONFLICT;
        }
        if (mode==13) {
            live.items[0].path[1]='E'; live.bytes-=live.items[2].raw.bytes; source_packet_free(&live.items[2].raw); expected=SOURCE_SUBSET_CONFLICT;
        }
        SourceVersions before_live=live; SourceVersionsAsset before_saved=saved;
        CHECK(source_versions_subset_admit(&saved,mode==11 ? 8 : 7,mode==10 ? NULL : &live,mode==4 ? 0 : UINT64_MAX,&out)==expected);
        CHECK(out.captured_pending==1 && out.exact+out.refined+out.unavailable+out.conflicting==2);
        CHECK(!memcmp(&live,&before_live,sizeof(live)) && !memcmp(&saved,&before_saved,sizeof(saved)));
        if (mode==2) CHECK(out.refined==1 && out.captured_pending==1 && !saved.versions.items[0].raw.bytes && saved.versions.items[0].raw.data==NULL);
        if (mode==12) CHECK(out.refined==1 && out.conflicting==1 && out.captured_pending==1);
        if (mode==13) CHECK(out.conflicting==1 && out.unavailable==1);
        if (mode==9 || mode==10) CHECK(out.allocator_unavailable);
        if (mode==0 || mode==1) CHECK(out.exact==2 && !out.allocator_unavailable);
        source_versions_free(&live); source_versions_asset_free(&saved);
    }
}
static void invalid_and_empty(void) {
    SourceVersions live={0}; SourceVersionsAsset saved={.sheet=7}; SourceSubsetAdmission out={1,2,3,4,5,true},before=out;
    CHECK(source_versions_subset_admit(&saved,7,&live,0,&out)==SOURCE_SUBSET_EXACT && !out.exact && !out.captured_pending);
    saved.versions.high_water=UINT64_MAX;
    CHECK(source_versions_subset_admit(&saved,7,&live,0,&out)==SOURCE_SUBSET_UNAVAILABLE && out.allocator_unavailable);
    out=before; saved.sheet=0;
    CHECK(source_versions_subset_admit(&saved,7,&live,0,&out)==SOURCE_SUBSET_INVALID && !memcmp(&out,&before,sizeof(out)));
    saved.sheet=7; row(&live,1,2,false,0);
    CHECK(source_versions_subset_admit(&saved,7,&live,1,&out)==SOURCE_SUBSET_INVALID && !memcmp(&out,&before,sizeof(out)));
    live.items[0].path[1]='*';
    CHECK(source_versions_subset_admit(&saved,7,&live,UINT64_MAX,&out)==SOURCE_SUBSET_INVALID && !memcmp(&out,&before,sizeof(out)));
    CHECK(source_versions_subset_admit(&saved,7,NULL,0,NULL)==SOURCE_SUBSET_INVALID);
    source_versions_free(&live);
}
static void maximum_names(void) {
    size_t count=DX_SOURCE_SCOPE_FIELDS; Column *columns=dx_alloc(count*sizeof(*columns)); char (*names)[4]=dx_alloc(count*sizeof(*names));
    static const char digits[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (size_t i=0;i<count;i++) {
        size_t value=i,n=0; char reverse[3]; do { reverse[n++]=digits[value%36]; value/=36; } while (value);
        for (size_t j=0;j<n;j++) names[i][j]=reverse[n-1-j]; names[i][n]=0;
        columns[i]=(Column){.name=names[i],.type=DUCKDB_TYPE_VARCHAR,.type_name="VARCHAR"};
    }
    Analysis a={.kind=SOURCE_CSV,.path="/full.csv"}; Page page={.columns=columns,.column_count=count}; SourceDigest digest={.bytes=1,.identity={.size=1}};
    SourcePacket raw={0}; CHECK(source_scope_raw_build(0,1,&a,&page,&digest,"test",&raw,NULL));
    SourceVersion entry={.version=1,.kind=SOURCE_CSV,.path=dx_strdup(a.path),.raw=raw};
    SourceVersions live={.high_water=UINT64_MAX,.items=&entry,.count=1,.capacity=1,.bytes=sizeof(entry)+strlen(entry.path)+1+raw.bytes};
    SourceVersionsAsset saved={.sheet=7}; CHECK(source_versions_copy(&saved.versions,&live,NULL));
    SourceSubsetAdmission out={0}; CHECK(source_versions_subset_admit(&saved,7,&live,0,&out)==SOURCE_SUBSET_EXACT && out.exact==1 && !out.captured_pending);
    SourceScopeField last,previous; CHECK(source_scope_field(raw.data,raw.bytes,count-1,&last,NULL) && source_scope_field(raw.data,raw.bytes,count-2,&previous,NULL));
    CHECK(strlen(last.name)==strlen(previous.name)); memcpy((char *)last.name,previous.name,strlen(last.name));
    SourceSubsetAdmission before=out;
    CHECK(source_versions_subset_admit(&saved,7,&live,0,&out)==SOURCE_SUBSET_INVALID && !memcmp(&out,&before,sizeof(out)));
    saved.versions.items[0].owner=1;
    CHECK(source_versions_subset_admit(&saved,7,NULL,0,&out)==SOURCE_SUBSET_INVALID && !memcmp(&out,&before,sizeof(out)));
    source_versions_asset_free(&saved); source_packet_free(&entry.raw); free(entry.path); free(names); free(columns);
}
int main(void) { scenarios(); invalid_and_empty(); maximum_names(); printf("PASS: %u read-only source subset admission checks\n",checks); return 0; }
