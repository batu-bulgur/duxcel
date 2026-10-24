#include "workbook/source_versions_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL source versions codec:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *data,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) { data[i]=(unsigned char)(value&255u); value>>=8; } }
static SourceVersions ledger(size_t count) {
    SourceVersions v={.high_water=count,.count=count,.capacity=count};
    if (count) v.items=dx_alloc(count*sizeof(*v.items)); v.bytes=count*sizeof(*v.items);
    for (size_t i=0;i<count;i++) { v.items[i]=(SourceVersion){.owner=i,.version=i+1,.kind=SOURCE_CSV,.path=dx_strdup("/a.csv")}; v.bytes+=7; }
    CHECK(source_versions_valid(&v,UINT64_MAX,NULL)); return v;
}
static unsigned char *encode(uint64_t sheet,const SourceVersions *v,size_t *bytes) {
    CHECK(source_versions_asset_size(sheet,v,UINT64_MAX,bytes)==SOURCE_VERSIONS_CODEC_OK);
    unsigned char *data=dx_alloc(*bytes+1); memset(data,0xa5,*bytes+1); size_t written=0;
    CHECK(source_versions_asset_encode(sheet,v,UINT64_MAX,data,*bytes,&written)==SOURCE_VERSIONS_CODEC_OK && written==*bytes && data[*bytes]==0xa5); return data;
}
static void rejected_at(int line,const void *data,size_t bytes,uint64_t owners,size_t reserved,SourceVersionsAsset *out,SourceVersionsCodecStatus status) {
    unsigned char before[sizeof(*out)]; memcpy(before,out,sizeof(before));
    SourceVersionsCodecStatus got=source_versions_asset_decode(data,bytes,owners,reserved,out);
    if (got!=status) fprintf(stderr,"Source asset case:%d: expected %d, got %d\n",line,status,got);
    CHECK(got==status && !memcmp(before,out,sizeof(before)));
}
#define rejected(...) rejected_at(__LINE__,__VA_ARGS__)
static void test_pending_oracle(void) {
    static const unsigned char expected[]={
        'D','X','S','V',1,0,0,0, 8,7,6,5,4,3,2,1,
        255,255,255,255,255,255,255,255, 1,0,0,0,0,0,0,0,
        1,0,0,0,0,0,32,0, 255,255,255,255,255,255,255,255,
        1,0,0,0, 6,0,0,0, 0,0,0,0, '/','a','.','c','s','v'};
    SourceVersions v=ledger(1); v.high_water=UINT64_MAX; v.items[0].version=UINT64_C(9007199254740993); v.items[0].owner=UINT64_MAX;
    size_t bytes=0; unsigned char *wire=encode(UINT64_C(0x0102030405060708),&v,&bytes);
    CHECK(bytes==sizeof(expected) && !memcmp(wire,expected,sizeof(expected)));
    SourceVersions padded={0}; CHECK(source_versions_copy(&padded,&v,NULL));
    padded.items=dx_realloc(padded.items,2*sizeof(*padded.items)); padded.items[1]=(SourceVersion){0}; padded.capacity=2; padded.bytes+=sizeof(SourceVersion);
    size_t padded_bytes=0; unsigned char *canonical=encode(UINT64_C(0x0102030405060708),&padded,&padded_bytes);
    CHECK(padded_bytes==bytes && !memcmp(canonical,wire,bytes)); free(canonical); source_versions_free(&padded);
    SourceVersionsAsset out={0}; CHECK(source_versions_asset_decode(wire,bytes,UINT64_MAX,v.bytes,&out)==SOURCE_VERSIONS_CODEC_OK);
    CHECK(out.sheet==UINT64_C(0x0102030405060708) && out.versions.high_water==UINT64_MAX && out.versions.count==1 && out.versions.capacity==1 && out.versions.bytes==v.bytes && out.versions.items[0].version==UINT64_C(9007199254740993) && out.versions.items[0].owner==UINT64_MAX && !out.versions.items[0].raw.data && !out.versions.items[0].raw.bytes);
    for (size_t n=0;n<bytes;n++) rejected(wire,n,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    rejected(wire,bytes+1,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    rejected(wire,bytes,UINT64_MAX,v.bytes-1,&out,SOURCE_VERSIONS_CODEC_LIMIT);
    rejected(wire,bytes,UINT64_MAX-1,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    rejected(NULL,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    CHECK(source_versions_asset_decode(wire,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,NULL)==SOURCE_VERSIONS_CODEC_INVALID);
    unsigned char *bad=dx_alloc(bytes+1); memcpy(bad,wire,bytes+1);
    for (unsigned i=0;i<4;i++) { bad[i]^=1; rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); bad[i]^=1; }
    bad[4]=2; rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_UNSUPPORTED); rejected(bad,6,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_UNSUPPORTED); bad[4]=1;
    bad[5]=1; rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_UNSUPPORTED); bad[5]=0;
    const struct { size_t at; unsigned char value; } mutations[]={
        {6,1},{7,1},{24,2},{25,5},{26,1},{28,1},{48,0},{48,3},{49,1},{50,1},
        {52,0},{54,1},{59,4},{60,0},{61,'*'},{61,255}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) {
        unsigned char old=bad[mutations[i].at]; bad[mutations[i].at]=mutations[i].value;
        rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); bad[mutations[i].at]=old;
    }
    memset(bad+8,0,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); memcpy(bad+8,wire+8,8);
    memset(bad+16,0,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); memcpy(bad+16,wire+16,8);
    memset(bad+32,0,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    unsigned char arena[128],copy[128]; memset(arena,0xa5,sizeof(arena)); memcpy(copy,arena,sizeof(copy)); size_t written=123,measured=456;
    CHECK(source_versions_asset_encode(1,&v,UINT64_MAX,arena,bytes-1,&written)==SOURCE_VERSIONS_CODEC_LIMIT && written==123 && !memcmp(arena,copy,sizeof(arena)));
    CHECK(source_versions_asset_size(0,&v,UINT64_MAX,&measured)==SOURCE_VERSIONS_CODEC_INVALID && measured==456);
    SourceVersions invalid=v; invalid.high_water=0;
    CHECK(source_versions_asset_encode(1,&invalid,UINT64_MAX,arena,sizeof(arena),&written)==SOURCE_VERSIONS_CODEC_INVALID && written==123 && !memcmp(arena,copy,sizeof(arena)));
    invalid=v; invalid.bytes++;
    CHECK(source_versions_asset_size(1,&invalid,UINT64_MAX,&measured)==SOURCE_VERSIONS_CODEC_INVALID && measured==456);
    CHECK(source_versions_asset_encode(1,&v,UINT64_MAX,NULL,bytes,&written)==SOURCE_VERSIONS_CODEC_INVALID && written==123);
    CHECK(source_versions_asset_encode(1,&v,UINT64_MAX,arena,sizeof(arena),NULL)==SOURCE_VERSIONS_CODEC_INVALID && !memcmp(arena,copy,sizeof(arena)));
    CHECK(out.versions.items[0].path!=v.items[0].path); source_versions_free(&v); free(wire); free(bad);
    CHECK(!strcmp(out.versions.items[0].path,"/a.csv") && source_versions_valid(&out.versions,UINT64_MAX,NULL)); source_versions_asset_free(&out);
    v=(SourceVersions){.high_water=UINT64_MAX}; wire=encode(1,&v,&bytes);
    CHECK(bytes==32 && source_versions_asset_decode(wire,bytes,0,0,&out)==SOURCE_VERSIONS_CODEC_OK && out.versions.high_water==UINT64_MAX && !out.versions.count && !out.versions.bytes);
    source_versions_asset_free(&out); free(wire); source_versions_asset_free(NULL);
}
static void test_complete_packet(void) {
    Column columns[2]={{.name="İd",.type=DUCKDB_TYPE_BIGINT,.type_name="BIGINT",.numeric=true,.number_type={NUM_I64,0,0}},
        {.name="Amount",.type=DUCKDB_TYPE_DECIMAL,.type_name="DECIMAL(38,4)",.numeric=true,.number_type={NUM_DECIMAL,38,4}}};
    Page schema={.columns=columns,.column_count=2}; Analysis a={.kind=SOURCE_PARQUET,.path="/private/source İ.parquet"};
    SourceDigest digest={.bytes=UINT64_C(9007199254740993),.observed_stamp=UINT64_MAX}; digest.identity.size=digest.bytes;
    for (size_t i=0;i<DX_SOURCE_DIGEST_BYTES;i++) digest.sha256[i]=(unsigned char)(i+1);
    SourcePacket packet={0}; CHECK(source_scope_raw_build(UINT64_MAX,UINT64_MAX,&a,&schema,&digest,"test-engine",&packet,NULL));
    SourceVersions v={.high_water=UINT64_MAX,.count=1,.capacity=1,.items=dx_alloc(sizeof(SourceVersion))};
    v.items[0]=(SourceVersion){.owner=UINT64_MAX,.version=UINT64_MAX,.kind=SOURCE_PARQUET,.path=dx_strdup(a.path),.raw=packet};
    v.bytes=sizeof(SourceVersion)+strlen(a.path)+1+packet.bytes;
    size_t bytes=0; unsigned char *wire=encode(9,&v,&bytes); SourceVersionsAsset out={0};
    CHECK(wire[48]==2 && source_versions_asset_decode(wire,bytes,UINT64_MAX,v.bytes,&out)==SOURCE_VERSIONS_CODEC_OK && out.sheet==9);
    CHECK(out.versions.items[0].raw.data!=packet.data && out.versions.items[0].raw.bytes==packet.bytes && !memcmp(out.versions.items[0].raw.data,packet.data,packet.bytes));
    size_t raw_at=32+28+strlen(a.path); unsigned char *bad=dx_alloc(bytes); memcpy(bad,wire,bytes);
    bad[48]=1; rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); bad[48]=2;
    put(bad+32,UINT64_MAX-1,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); put(bad+32,UINT64_MAX,8);
    put(bad+40,0,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); put(bad+40,UINT64_MAX,8);
    bad[raw_at]^=1; rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); bad[raw_at]^=1;
    for (size_t n=raw_at+1;n<bytes;n++) { put(bad+56,n-raw_at,4); rejected(bad,n,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID); }
    source_versions_free(&v); free(wire); free(bad);
    SourceScopeInfo info={0}; CHECK(source_scope_validate(out.versions.items[0].raw.data,out.versions.items[0].raw.bytes,&info,NULL));
    CHECK(info.owner==UINT64_MAX && info.version==UINT64_MAX && info.file_bytes==UINT64_C(9007199254740993) && info.source_kind==SOURCE_PARQUET && !strcmp(out.versions.items[0].path,a.path));
    SourceScopeField field={0}; CHECK(source_scope_field(out.versions.items[0].raw.data,out.versions.items[0].raw.bytes,1,&field,NULL) && field.number.kind==NUM_DECIMAL && field.number.precision==38 && field.number.scale==4);
    source_versions_asset_free(&out);
}
static void test_ledger_limits(void) {
    SourceVersions v=ledger(DX_SOURCE_VERSIONS); size_t bytes=0; unsigned char *wire=encode(UINT64_MAX,&v,&bytes); SourceVersionsAsset out={0};
    CHECK(source_versions_asset_decode(wire,bytes,DX_SOURCE_VERSIONS-1,v.bytes,&out)==SOURCE_VERSIONS_CODEC_OK && out.versions.count==DX_SOURCE_VERSIONS && out.versions.items[DX_SOURCE_VERSIONS-1].version==DX_SOURCE_VERSIONS);
    unsigned char *bad=dx_alloc(bytes); memcpy(bad,wire,bytes); const size_t second=32+28+6;
    put(bad+second,1,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    put(bad+second,0,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    put(bad+second,UINT64_MAX,8); rejected(bad,bytes,UINT64_MAX,DX_SOURCE_VERSIONS_BYTES,&out,SOURCE_VERSIONS_CODEC_INVALID);
    source_versions_free(&v); source_versions_asset_free(&out); free(wire); free(bad);
    v=ledger(1); free(v.items[0].path); v.items[0].path=dx_alloc(DX_SOURCE_SCOPE_TEXT_BYTES+1); memset(v.items[0].path,'a',DX_SOURCE_SCOPE_TEXT_BYTES);
    v.bytes=sizeof(SourceVersion)+DX_SOURCE_SCOPE_TEXT_BYTES+1; wire=encode(1,&v,&bytes);
    CHECK(source_versions_asset_decode(wire,bytes,0,v.bytes,&out)==SOURCE_VERSIONS_CODEC_OK && strlen(out.versions.items[0].path)==DX_SOURCE_SCOPE_TEXT_BYTES);
    source_versions_free(&v); source_versions_asset_free(&out); free(wire);
}
int main(void) {
    test_pending_oracle(); test_complete_packet(); test_ledger_limits();
    printf("PASS: %u closed source-version metadata asset checks\n",checks); return 0;
}
