#include "workbook/objects_asset_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL objects asset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static ObjectsAssetPacket capture(const ObjectStore *s,ObjectsAssetInfo *info) {
    ObjectSnapshot view={s->items,s->count}; ObjectsAssetPacket packet={0};
    CHECK(objects_asset_measure(99,&view,s->next_id,info)==OBJECTS_ASSET_OK);
    CHECK(objects_asset_capture(99,&view,s->next_id,info->measure.bytes+info->measure.validation_scratch,&packet)==OBJECTS_ASSET_OK && packet.bytes==info->measure.bytes);
    ObjectsAssetInfo framed={0}; CHECK(objects_asset_preflight(packet.data,packet.bytes,&framed)==OBJECTS_ASSET_OK && framed.sheet==99 && framed.count==s->count &&
        framed.high_water==s->next_id && framed.measure.owned==info->measure.owned && framed.measure.validation_scratch==info->measure.validation_scratch);
    unsigned char *direct=dx_alloc(packet.bytes+1); memset(direct,0xa5,packet.bytes+1); size_t written=77;
    CHECK(objects_asset_encode_unpublished(99,&view,s->next_id,info->measure.validation_scratch,direct,packet.bytes-1,&written)==OBJECTS_ASSET_LIMIT && written==77);
    CHECK(objects_asset_encode_unpublished(99,&view,s->next_id,info->measure.validation_scratch,direct,packet.bytes,&written)==OBJECTS_ASSET_OK &&
        written==packet.bytes && direct[packet.bytes]==0xa5 && !memcmp(direct,packet.data,packet.bytes));
    free(direct); return packet;
}
static void reject(const void *data,size_t bytes,size_t budget,ObjectsAsset *out,ObjectsAssetStatus expected) {
    ObjectsAsset before=*out; CHECK(objects_asset_decode(data,bytes,budget,out)==expected && !memcmp(&before,out,sizeof(before)));
}
static ObjectStore mixed(void) {
    ObjectStore s={0}; Analysis a={.kind=SOURCE_PARQUET,.path="/Ürün.parquet",.source_version=UINT64_MAX};
    CHECK(objects_add_table(&s,0,0,"Raw data",&a,NULL)==0);
    CHECK(analysis_filter_set(&s.items[0].source,"Amount",FILTER_GREATER,"9007199254740993.0001",NULL)); s.items[0].source.filter_number=(NumberType){NUM_DECIMAL,38,4};
    PivotDefinition p={0}; CHECK(pivot_assign(&p,PIVOT_ROWS,"Region",AGG_SUM,NULL)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_AVG,NULL));
    CHECK(objects_add(&s,20,0,"Regional pivot",NULL)==1); CHECK(objects_update(&s,1,&p,NULL)); CHECK(objects_bind_source(&s,1,s.items[0].id,NULL));
    CHECK(objects_add_chart(&s,40,0,"Regional chart",&p,NULL)==2); CHECK(objects_bind_source(&s,2,s.items[0].id,NULL));
    CHECK(objects_mark(&s,2,MARK_LINE,NULL)); CHECK(objects_chart_measure(&s,2,1,NULL));
    MarkDefinition k={0}; MarkField f={.name="Region"}; CHECK(marks_assign(&k,MARKS_COLOR,&f,NULL)); f=(MarkField){.name="Amount",.aggregate=AGG_AVG,.measure=true}; CHECK(marks_assign(&k,MARKS_SIZE,&f,NULL));
    CHECK(objects_marks(&s,2,&k,NULL)); marks_free(&k); pivot_free(&p);
    SheetObject swap=s.items[1]; s.items[1]=s.items[2]; s.items[2]=swap; /* Preserve authored array/paint order. */
    s.next_id=UINT64_MAX; s.title_revision=s.marks_revision=UINT64_MAX;
    for (size_t i=0;i<s.count;i++) s.items[i].revision=UINT64_MAX;
    CHECK(objects_valid(&s,NULL)); return s;
}
static void oracle(void) {
    ObjectStore s={.next_id=UINT64_MAX}; ObjectsAssetInfo info={0}; ObjectsAssetPacket packet=capture(&s,&info); unsigned char oracle[32]={0};
    memcpy(oracle,"DXOB",4); oracle[4]=1; oracle[8]=99; put(oracle+16,UINT64_MAX,8);
    CHECK(packet.bytes==32 && !memcmp(packet.data,oracle,32) && !info.measure.owned && !info.measure.validation_scratch);
    ObjectsAsset out={0}; CHECK(objects_asset_decode(packet.data,packet.bytes,0,&out)==OBJECTS_ASSET_OK && out.high_water==UINT64_MAX && !out.objects.items && !out.objects.count);
    for (size_t n=0;n<32;n++) reject(packet.data,n,0,&out,OBJECTS_ASSET_INVALID);
    unsigned char bad[33]; memcpy(bad,packet.data,32); bad[32]=0; reject(bad,33,0,&out,OBJECTS_ASSET_INVALID);
    struct { size_t at; unsigned value; ObjectsAssetStatus status; } mutations[]={
        {0,'X',OBJECTS_ASSET_INVALID},{4,2,OBJECTS_ASSET_UNSUPPORTED},{5,1,OBJECTS_ASSET_UNSUPPORTED},{6,1,OBJECTS_ASSET_INVALID},
        {24,33,OBJECTS_ASSET_INVALID},{25,1,OBJECTS_ASSET_INVALID},{26,1,OBJECTS_ASSET_INVALID},{28,1,OBJECTS_ASSET_INVALID}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) { memcpy(bad,packet.data,32); bad[mutations[i].at]=(unsigned char)mutations[i].value; reject(bad,32,0,&out,mutations[i].status); }
    memcpy(bad,packet.data,32); memset(bad+8,0,8); reject(bad,32,0,&out,OBJECTS_ASSET_INVALID);
    reject(NULL,32,0,&out,OBJECTS_ASSET_INVALID); CHECK(objects_asset_decode(packet.data,32,0,NULL)==OBJECTS_ASSET_INVALID);
    ObjectsAssetInfo unchanged=info; CHECK(objects_asset_preflight(packet.data,31,&info)==OBJECTS_ASSET_INVALID && !memcmp(&unchanged,&info,sizeof(info)));
    objects_asset_free(&out); objects_asset_packet_free(&packet); objects_asset_packet_free(NULL); objects_asset_free(NULL);
}
static void ownership_and_corruption(void) {
    ObjectStore s=mixed(); ObjectsAssetInfo info={0}; ObjectsAssetPacket packet=capture(&s,&info); ObjectsAsset out={0}; size_t budget=info.measure.owned+info.measure.validation_scratch;
    CHECK(objects_asset_decode(packet.data,packet.bytes,budget,&out)==OBJECTS_ASSET_OK && out.objects.count==3 && out.high_water==UINT64_MAX);
    CHECK(out.objects.items[0].kind==OBJECT_TABLE && out.objects.items[1].kind==OBJECT_CHART && out.objects.items[2].kind==OBJECT_PIVOT &&
        out.objects.items[1].mark==MARK_LINE && out.objects.items[1].chart_measure==1 && out.objects.items[1].source_id==1 && out.objects.items[2].source_id==1);
    CHECK(!out.objects.items[0].source.filter_number.kind && out.objects.items[0].source.source_version==UINT64_MAX &&
        !strcmp(out.objects.items[0].source.path,"/Ürün.parquet") && !strcmp(out.objects.items[0].source.filter_value,"9007199254740993.0001"));
    CHECK(!out.objects.items[0].revision && !out.objects.items[1].revision && !out.objects.items[2].revision);
    unsigned char *bad=dx_alloc(packet.bytes+1); unsigned char *data=packet.data;
    for (size_t n=32;n<packet.bytes;n++) { memcpy(bad,data,n); put(bad+28,n-32,4); reject(bad,n,budget,&out,OBJECTS_ASSET_INVALID); }
    struct { size_t at; uint64_t value; unsigned width; } mutations[]={
        {32,0,8},{40,DX_SHEET_ROWS,8},{48,0,8},{64,16384,2},{66,0,2},{68,3,1},{69,4,1},{71,1,1},{16,0,8},
        {72,UINT32_MAX,4},{84,0,4},{88,0,1}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) { memcpy(bad,data,packet.bytes); put(bad+mutations[i].at,mutations[i].value,mutations[i].width); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID); }
    unsigned char *canonical=dx_alloc(packet.bytes); memcpy(canonical,data,packet.bytes);
    size_t nested=32+DX_OBJECT_ASSET_HEADER_BYTES+strlen(s.items[0].name);
    memcpy(bad,data,packet.bytes); put(bad+nested+8,100,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); put(bad+nested+16,2,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); bad[nested+4]=2; reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_UNSUPPORTED);
    size_t lengths[3]={0}; for (unsigned f=0;f<3;f++) for (unsigned b=0;b<4;b++) lengths[f]|=(size_t)data[72+f*4+b]<<(8*b);
    size_t node=nested;
    for (unsigned f=0;f<3;f++) {
        memcpy(bad,data,packet.bytes); bad[node+4]=2; reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_UNSUPPORTED);
        memcpy(bad,data,packet.bytes); put(bad+node+16,0,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
        node+=lengths[f];
    }
    size_t second=node;
    memcpy(bad,data,packet.bytes); put(bad+second,1,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); put(bad+second+24,999,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); bad[second+38]=255;
    ObjectsAssetInfo inspection={0}; CHECK(objects_asset_preflight(bad,packet.bytes,&inspection)==OBJECTS_ASSET_OK);
    reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); put(bad+40,DX_SHEET_ROWS-1,8); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); put(bad+64,16383,2); reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID);
    memcpy(bad,data,packet.bytes); bad[nested+32]=SOURCE_NONE;
    ObjectsAssetInfo framed={0}; CHECK(objects_asset_preflight(bad,packet.bytes,&framed)==OBJECTS_ASSET_OK);
    reject(bad,packet.bytes,budget,&out,OBJECTS_ASSET_INVALID); /* Full Table/source semantics still mandatory. */
    reject(data,packet.bytes,budget-1,&out,OBJECTS_ASSET_LIMIT);
    ObjectSnapshot snapshot={s.items,s.count}; ObjectsAssetPacket prior=packet;
    CHECK(objects_asset_capture(99,&snapshot,s.next_id,info.measure.bytes+info.measure.validation_scratch-1,&packet)==OBJECTS_ASSET_LIMIT && packet.data==prior.data && packet.bytes==prior.bytes);
    SourceKind kind=s.items[0].source.kind; s.items[0].source.kind=SOURCE_NONE;
    CHECK(objects_asset_capture(99,&snapshot,s.next_id,SIZE_MAX,&packet)==OBJECTS_ASSET_INVALID && packet.data==prior.data && packet.bytes==prior.bytes);
    unsigned char *unpublished=dx_alloc(packet.bytes); size_t written=77;
    CHECK(objects_asset_encode_unpublished(99,&snapshot,s.next_id,SIZE_MAX,unpublished,packet.bytes,&written)==OBJECTS_ASSET_INVALID && written==77);
    free(unpublished); s.items[0].source.kind=kind;
    uint64_t source=s.items[1].source_id; s.items[1].source_id=999;
    CHECK(objects_asset_capture(99,&snapshot,s.next_id,SIZE_MAX,&packet)==OBJECTS_ASSET_INVALID && packet.data==prior.data); s.items[1].source_id=source;
    uint64_t row=s.items[1].row; s.items[1].row=DX_SHEET_ROWS;
    CHECK(objects_asset_capture(99,&snapshot,s.next_id,SIZE_MAX,&packet)==OBJECTS_ASSET_INVALID && packet.data==prior.data); s.items[1].row=row;
    memset(packet.data,0,packet.bytes); objects_asset_packet_free(&packet); free(bad); objects_free(&s);
    CHECK(!strcmp(out.objects.items[1].name,"Regional chart") && out.objects.items[1].marks.counts[MARKS_SIZE]==1 && out.objects.items[1].pivot.counts[PIVOT_VALUES]==2);
    ObjectStore decoded={.items=out.objects.items,.count=out.objects.count,.next_id=out.high_water}; CHECK(objects_valid(&decoded,NULL));
    ObjectsAssetInfo again={0}; packet=capture(&decoded,&again); CHECK(again.measure.owned==info.measure.owned && packet.bytes==info.measure.bytes && !memcmp(packet.data,canonical,packet.bytes)); free(canonical); objects_asset_packet_free(&packet); objects_asset_free(&out);
}
static void full_vector_and_owned_bound(void) {
    ObjectStore s={0};
    for (size_t i=0;i<DX_MAX_OBJECTS;i++) { char *name=dx_printf("Pivot%zu",i); CHECK(objects_add(&s,i*10,0,name,NULL)==(int)i); free(name); }
    ObjectsAssetInfo info={0}; ObjectsAssetPacket packet=capture(&s,&info); CHECK(info.measure.owned>packet.bytes);
    ObjectsAsset out={0}; reject(packet.data,packet.bytes,packet.bytes,&out,OBJECTS_ASSET_LIMIT);
    CHECK(objects_asset_decode(packet.data,packet.bytes,info.measure.owned,&out)==OBJECTS_ASSET_OK && out.objects.count==32);
    objects_asset_free(&out); objects_asset_packet_free(&packet); objects_free(&s);
    Analysis a={.kind=SOURCE_CSV,.path="/source"}; CHECK(objects_add_table(&s,0,0,"Raw",&a,NULL)==0);
    CHECK(analysis_filter_set(&s.items[0].source,"A",FILTER_CONTAINS,"",NULL)); CHECK(analysis_filter_set(&s.items[0].source,"B",FILTER_CONTAINS,"",NULL));
    ObjectSnapshot snapshot={s.items,s.count}; CHECK(objects_asset_measure(99,&snapshot,s.next_id,&info)==OBJECTS_ASSET_OK);
    size_t fill=DX_OBJECT_CURRENT_BYTES-info.measure.owned;
    free(s.items[0].source.filter_value); s.items[0].source.filter_value=dx_alloc(4u*1024u*1024u+1); memset(s.items[0].source.filter_value,'a',4u*1024u*1024u);
    size_t extra=fill-4u*1024u*1024u; free(s.items[0].source.filters[0].value); s.items[0].source.filters[0].value=dx_alloc(extra+2); memset(s.items[0].source.filters[0].value,'b',extra);
    packet=capture(&s,&info); CHECK(info.measure.owned==DX_OBJECT_CURRENT_BYTES);
    CHECK(objects_asset_decode(packet.data,packet.bytes,info.measure.owned,&out)==OBJECTS_ASSET_OK && strlen(out.objects.items[0].source.filter_value)==4u*1024u*1024u);
    s.items[0].source.filters[0].value[extra]='b'; s.items[0].source.filters[0].value[extra+1]=0;
    CHECK(objects_asset_measure(99,&snapshot,s.next_id,&info)==OBJECTS_ASSET_INVALID);
    objects_asset_free(&out); objects_asset_packet_free(&packet); objects_free(&s);
}
int main(void) { oracle(); ownership_and_corruption(); full_vector_and_owned_bound(); printf("PASS: %u complete object asset ownership checks\n",checks); return 0; }
