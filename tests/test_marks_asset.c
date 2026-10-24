#include "workbook/marks_asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL marks asset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static unsigned char *encode(const MarkDefinition *d,size_t *bytes,MarksAssetMeasure *m) {
    CHECK(marks_asset_measure(UINT64_MAX,7,d,7,m)==MARKS_ASSET_OK); unsigned char *data=dx_alloc(m->bytes);
    CHECK(marks_asset_encode(UINT64_MAX,7,d,7,data,m->bytes,bytes)==MARKS_ASSET_OK && *bytes==m->bytes);
    DefinitionAssetInfo info={0}; CHECK(marks_asset_preflight(data,*bytes,7,&info)==MARKS_ASSET_OK && info.sheet==UINT64_MAX && info.owner==7 &&
        info.measure.bytes==m->bytes && info.measure.owned==m->owned && info.measure.validation_scratch==m->validation_scratch);
    DefinitionAssetInfo before=info;
    CHECK(marks_asset_preflight(data,31,7,&info)==MARKS_ASSET_INVALID && !memcmp(&before,&info,sizeof(before)));
    CHECK(marks_asset_preflight(data,*bytes,7,NULL)==MARKS_ASSET_INVALID);
    return data;
}
static void reject(const void *data,size_t bytes,size_t budget,MarksAsset *out,MarksAssetStatus status) {
    MarksAsset before=*out; CHECK(marks_asset_decode(data,bytes,7,budget,out)==status && !memcmp(&before,out,sizeof(before)));
}
static void roundtrip(const MarkDefinition *d) {
    size_t bytes=0; MarksAssetMeasure m={0}; unsigned char *data=encode(d,&bytes,&m); MarksAsset out={0};
    CHECK(marks_asset_decode(data,bytes,7,m.owned,&out)==MARKS_ASSET_OK && out.sheet==UINT64_MAX && out.owner==7 && marks_equal(d,&out.definition));
    for (unsigned role=0;role<DX_MARK_ROLES;role++) for (size_t i=0;i<d->counts[role];i++) {
        const MarkField *from=&d->fields[role][i],*to=&out.definition.fields[role][i];
        CHECK(from->aggregate==to->aggregate && from->grain==to->grain && from->measure==to->measure && from->name!=to->name && !strcmp(from->name,to->name));
    }
    size_t written=0; unsigned char *other=dx_alloc(bytes);
    CHECK(marks_asset_encode(out.sheet,out.owner,&out.definition,7,other,bytes,&written)==MARKS_ASSET_OK && written==bytes && !memcmp(other,data,bytes));
    free(other); free(data); marks_asset_free(&out);
}
static void oracle(void) {
    MarkDefinition d={0}; size_t bytes=0; MarksAssetMeasure m={0}; unsigned char *data=encode(&d,&bytes,&m),oracle[40]={0};
    memcpy(oracle,"DXMK",4); oracle[4]=1; put(oracle+8,UINT64_MAX,8); oracle[16]=7; oracle[24]=8;
    CHECK(bytes==40 && !memcmp(data,oracle,40) && m.owned==sizeof(MarkDefinition) && !m.validation_scratch);
    MarksAsset out={0}; CHECK(marks_asset_decode(data,bytes,7,m.owned,&out)==MARKS_ASSET_OK);
    for (size_t n=0;n<40;n++) reject(data,n,m.owned,&out,MARKS_ASSET_INVALID);
    unsigned char bad[41]; memcpy(bad,data,40); bad[40]=0; reject(bad,41,m.owned,&out,MARKS_ASSET_INVALID);
    struct { size_t at; unsigned value; MarksAssetStatus status; } mutations[]={
        {0,'X',MARKS_ASSET_INVALID},{4,2,MARKS_ASSET_UNSUPPORTED},{5,1,MARKS_ASSET_UNSUPPORTED},{6,1,MARKS_ASSET_INVALID},
        {28,1,MARKS_ASSET_INVALID},{32,9,MARKS_ASSET_INVALID},{33,2,MARKS_ASSET_INVALID},{34,9,MARKS_ASSET_INVALID},
        {35,9,MARKS_ASSET_INVALID},{36,9,MARKS_ASSET_INVALID},{37,2,MARKS_ASSET_INVALID},{38,1,MARKS_ASSET_INVALID}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) { memcpy(bad,data,40); bad[mutations[i].at]=(unsigned char)mutations[i].value; reject(bad,40,m.owned,&out,mutations[i].status); }
    memcpy(bad,data,40); memset(bad+8,0,8); reject(bad,40,m.owned,&out,MARKS_ASSET_INVALID);
    memcpy(bad,data,40); memset(bad+16,0,8); reject(bad,40,m.owned,&out,MARKS_ASSET_INVALID);
    memcpy(bad,data,40); put(bad+16,8,8); reject(bad,40,m.owned,&out,MARKS_ASSET_INVALID);
    reject(data,40,m.owned-1,&out,MARKS_ASSET_LIMIT); reject(NULL,40,m.owned,&out,MARKS_ASSET_INVALID);
    CHECK(marks_asset_decode(data,40,7,m.owned,NULL)==MARKS_ASSET_INVALID); free(data); marks_asset_free(&out); marks_asset_free(NULL);
}
static void fields_and_corruption(void) {
    MarkDefinition d={0};
    for (unsigned role=0;role<DX_MARK_ROLES;role++) {
        MarkField field={.name=role%2 ? "Amount" : "Ürün",.measure=role%2!=0,.aggregate=AGG_AVG,.grain=role%2 ? PIVOT_DATE_EXACT : PIVOT_DATE_MONTH};
        CHECK(marks_assign(&d,(MarkRole)role,&field,NULL));
    }
    MarkField more={.name="Tarih",.grain=PIVOT_DATE_YEAR}; CHECK(marks_assign(&d,MARKS_COLOR,&more,NULL)); roundtrip(&d);
    size_t bytes=0; MarksAssetMeasure m={0}; unsigned char *data=encode(&d,&bytes,&m),*bad=dx_alloc(bytes); MarksAsset out={0};
    CHECK(marks_asset_decode(data,bytes,7,m.owned,&out)==MARKS_ASSET_OK);
    for (size_t n=32;n<bytes;n++) { memcpy(bad,data,n); put(bad+24,n-32,4); reject(bad,n,m.owned,&out,MARKS_ASSET_INVALID); }
    unsigned at[]={40,41,42,43}; unsigned values[]={6,4,2,1};
    for (size_t i=0;i<4;i++) { memcpy(bad,data,bytes); bad[at[i]]=(unsigned char)values[i]; reject(bad,bytes,m.owned,&out,MARKS_ASSET_INVALID); }
    memcpy(bad,data,bytes); bad[42]=1; reject(bad,bytes,m.owned,&out,MARKS_ASSET_INVALID); /* A measure cannot carry Month grain. */
    memcpy(bad,data,bytes); bad[48]=0; reject(bad,bytes,m.owned,&out,MARKS_ASSET_INVALID);
    memcpy(bad,data,bytes); bad[48]=0xf4; bad[49]=0x90; bad[50]=0x80; bad[51]=0x80; reject(bad,bytes,m.owned,&out,MARKS_ASSET_INVALID);
    memset(data,0,bytes); free(data); free(bad); marks_free(&d);
    CHECK(out.definition.counts[MARKS_COLOR]==2 && !strcmp(out.definition.fields[MARKS_COLOR][0].name,"Ürün") && out.definition.fields[MARKS_COLOR][0].aggregate==AGG_AVG);
    roundtrip(&out.definition); marks_asset_free(&out);
}
static void budgets_and_limits(void) {
    MarkDefinition d={0}; MarksAssetMeasure m={0}; MarkField field={.name="Region",.aggregate=AGG_MAX}; CHECK(marks_assign(&d,MARKS_COLOR,&field,NULL));
    size_t bytes=0; unsigned char *data=encode(&d,&bytes,&m),*other=dx_alloc(bytes); memset(other,0xa5,bytes); size_t written=77;
    CHECK(marks_asset_encode(1,1,&d,1,other,bytes-1,&written)==MARKS_ASSET_LIMIT && written==77 && other[0]==0xa5 && other[bytes-1]==0xa5);
    MarksAsset out={0}; reject(data,bytes,m.owned-1,&out,MARKS_ASSET_LIMIT); CHECK(marks_asset_decode(data,bytes,7,m.owned,&out)==MARKS_ASSET_OK);
    free(data); free(other); marks_asset_free(&out);
    d.counts[MARKS_COLOR]=2; d.fields[MARKS_COLOR][1]=(MarkField){.name=dx_strdup("Region"),.aggregate=AGG_SUM};
    CHECK(marks_asset_measure(1,1,&d,1,&m)==MARKS_ASSET_INVALID); /* Dimension duplicates ignore latent aggregate. */
    marks_free(&d); d=(MarkDefinition){0}; d.fields[0][0].name="unused"; CHECK(marks_asset_measure(1,1,&d,1,&m)==MARKS_ASSET_INVALID);
    d=(MarkDefinition){0}; d.counts[MARKS_SIZE]=2; CHECK(marks_asset_measure(1,1,&d,1,&m)==MARKS_ASSET_INVALID);
    d=(MarkDefinition){0}; CHECK(marks_asset_measure(0,1,&d,1,&m)==MARKS_ASSET_INVALID && marks_asset_measure(1,0,&d,1,&m)==MARKS_ASSET_INVALID && marks_asset_measure(1,2,&d,1,&m)==MARKS_ASSET_INVALID);
    for (unsigned role=0;role<DX_MARK_ROLES;role++) for (size_t i=0;i<(role==MARKS_SIZE || role==MARKS_SHAPE ? 1u : DX_MARK_FIELDS);i++) {
        char *name=dx_alloc(4098); memset(name,'x',4096); name[4096]=0; char prefix[32]; snprintf(prefix,sizeof(prefix),"field%u_%zu_",role,i); memcpy(name,prefix,strlen(prefix));
        field=(MarkField){.name=name,.measure=i%2!=0,.aggregate=(Aggregate)(i%6),.grain=i%2 ? PIVOT_DATE_EXACT : PIVOT_DATE_QUARTER};
        CHECK(marks_assign(&d,(MarkRole)role,&field,NULL)); free(name);
    }
    CHECK(marks_asset_measure(1,1,&d,1,&m)==MARKS_ASSET_OK && m.owned==DX_MARKS_ASSET_OWNED_BYTES && m.bytes==DX_MARKS_ASSET_MAX_BYTES); roundtrip(&d);
    char *name=d.fields[0][0].name; d.fields[0][0].name=dx_alloc(4098); memset(d.fields[0][0].name,'x',4097); d.fields[0][0].name[4097]=0;
    CHECK(marks_asset_measure(1,1,&d,1,&m)==MARKS_ASSET_INVALID); free(d.fields[0][0].name); d.fields[0][0].name=name; marks_free(&d);
}
int main(void) { oracle(); fields_and_corruption(); budgets_and_limits(); printf("PASS: %u closed Marks binding asset checks\n",checks); return 0; }
