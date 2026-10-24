#include "workbook/pivot_asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL pivot asset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static unsigned char *encode(const PivotDefinition *p,size_t *bytes,PivotAssetMeasure *m) {
    CHECK(pivot_asset_measure(UINT64_MAX,3,p,3,m)==PIVOT_ASSET_OK); unsigned char *data=dx_alloc(m->bytes);
    CHECK(pivot_asset_encode(UINT64_MAX,3,p,3,m->validation_scratch,data,m->bytes,bytes)==PIVOT_ASSET_OK && *bytes==m->bytes);
    DefinitionAssetInfo info={0}; CHECK(pivot_asset_preflight(data,*bytes,3,&info)==PIVOT_ASSET_OK && info.sheet==UINT64_MAX && info.owner==3 &&
        info.measure.bytes==m->bytes && info.measure.owned==m->owned && info.measure.validation_scratch==m->validation_scratch);
    DefinitionAssetInfo before=info;
    CHECK(pivot_asset_preflight(data,31,3,&info)==PIVOT_ASSET_INVALID && !memcmp(&before,&info,sizeof(before)));
    CHECK(pivot_asset_preflight(data,*bytes,3,NULL)==PIVOT_ASSET_INVALID);
    return data;
}
static void reject(const void *data,size_t bytes,size_t budget,PivotAsset *out,PivotAssetStatus status) {
    PivotAsset before=*out; CHECK(pivot_asset_decode(data,bytes,3,budget,out)==status && !memcmp(&before,out,sizeof(before)));
}
static void roundtrip(const PivotDefinition *p) {
    size_t bytes=0; PivotAssetMeasure m={0}; unsigned char *data=encode(p,&bytes,&m); PivotAsset out={0};
    CHECK(pivot_asset_decode(data,bytes,3,m.owned+m.validation_scratch,&out)==PIVOT_ASSET_OK);
    CHECK(out.sheet==UINT64_MAX && out.owner==3);
    for (unsigned role=0;role<4;role++) for (size_t i=0;i<p->counts[role];i++) {
        const PivotField *from=&p->fields[role][i],*to=&out.definition.fields[role][i];
        CHECK(out.definition.counts[role]==p->counts[role] && !strcmp(from->name,to->name) && from->aggregate==to->aggregate &&
            from->filter==to->filter && from->grain==to->grain && !to->number.kind &&
            (from->value ? to->value && !strcmp(from->value,to->value) : !to->value));
    }
    unsigned char *other=dx_alloc(bytes); size_t written=0;
    CHECK(pivot_asset_encode(out.sheet,out.owner,&out.definition,3,m.validation_scratch,other,bytes,&written)==PIVOT_ASSET_OK && written==bytes && !memcmp(data,other,bytes));
    free(other); free(data); pivot_asset_free(&out);
}
static void oracle(void) {
    PivotDefinition p={0}; size_t bytes=0; PivotAssetMeasure m={0}; unsigned char *data=encode(&p,&bytes,&m),oracle[40]={0};
    memcpy(oracle,"DXPV",4); oracle[4]=1; put(oracle+8,UINT64_MAX,8); oracle[16]=3; oracle[24]=8;
    CHECK(bytes==40 && !memcmp(oracle,data,40) && m.owned==sizeof(PivotDefinition) && !m.validation_scratch);
    PivotAsset out={0}; CHECK(pivot_asset_decode(data,bytes,3,m.owned,&out)==PIVOT_ASSET_OK);
    for (size_t n=0;n<bytes;n++) reject(data,n,m.owned,&out,PIVOT_ASSET_INVALID);
    unsigned char bad[41]; memcpy(bad,data,40); bad[40]=0; reject(bad,41,m.owned,&out,PIVOT_ASSET_INVALID);
    struct { size_t at; unsigned value; PivotAssetStatus status; } mutations[]={
        {0,'X',PIVOT_ASSET_INVALID},{4,2,PIVOT_ASSET_UNSUPPORTED},{5,1,PIVOT_ASSET_UNSUPPORTED},{6,1,PIVOT_ASSET_INVALID},
        {28,1,PIVOT_ASSET_INVALID},{32,9,PIVOT_ASSET_INVALID},{33,9,PIVOT_ASSET_INVALID},{34,9,PIVOT_ASSET_INVALID},{35,9,PIVOT_ASSET_INVALID},{36,1,PIVOT_ASSET_INVALID}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) { memcpy(bad,data,40); bad[mutations[i].at]=(unsigned char)mutations[i].value; reject(bad,40,m.owned,&out,mutations[i].status); }
    memcpy(bad,data,40); memset(bad+8,0,8); reject(bad,40,m.owned,&out,PIVOT_ASSET_INVALID);
    memcpy(bad,data,40); memset(bad+16,0,8); reject(bad,40,m.owned,&out,PIVOT_ASSET_INVALID);
    memcpy(bad,data,40); put(bad+16,4,8); reject(bad,40,m.owned,&out,PIVOT_ASSET_INVALID);
    reject(data,40,m.owned-1,&out,PIVOT_ASSET_LIMIT); reject(NULL,40,m.owned,&out,PIVOT_ASSET_INVALID);
    CHECK(pivot_asset_decode(data,40,3,m.owned,NULL)==PIVOT_ASSET_INVALID);
    pivot_asset_free(&out); pivot_asset_free(NULL); free(data);
}
static void fields_and_corruption(void) {
    PivotDefinition p={0}; CHECK(pivot_assign_grain(&p,PIVOT_ROWS,"Tarih",AGG_SUM,PIVOT_DATE_YEAR,NULL));
    CHECK(pivot_assign_grain(&p,PIVOT_ROWS,"Tarih",AGG_SUM,PIVOT_DATE_MONTH,NULL));
    CHECK(pivot_assign_grain(&p,PIVOT_COLUMNS,"Tarih",AGG_SUM,PIVOT_DATE_QUARTER,NULL));
    CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_DISTINCT,NULL));
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Ürün",AGG_SUM,NULL)); p.fields[PIVOT_FILTERS][0].value=dx_strdup("[null,\"\",\"NULL\"]");
    p.fields[PIVOT_FILTERS][0].filter=FILTER_IN; p.fields[PIVOT_FILTERS][0].number=(NumberType){NUM_DECIMAL,38,4}; roundtrip(&p);
    size_t bytes=0; PivotAssetMeasure m={0}; unsigned char *data=encode(&p,&bytes,&m),*bad=dx_alloc(bytes); PivotAsset out={0};
    CHECK(pivot_asset_decode(data,bytes,3,m.owned,&out)==PIVOT_ASSET_OK);
    for (size_t n=32;n<bytes;n++) { memcpy(bad,data,n); put(bad+24,n-32,4); reject(bad,n,m.owned,&out,PIVOT_ASSET_INVALID); }
    unsigned offsets[]={40,41,42,43}; unsigned values[]={6,12,4,1};
    for (size_t i=0;i<4;i++) { memcpy(bad,data,bytes); bad[offsets[i]]=(unsigned char)values[i]; reject(bad,bytes,m.owned,&out,PIVOT_ASSET_INVALID); }
    memcpy(bad,data,bytes); bad[48]=0; reject(bad,bytes,m.owned,&out,PIVOT_ASSET_INVALID);
    memcpy(bad,data,bytes); bad[48]=0xed; bad[49]=0xa0; bad[50]=0x80; reject(bad,bytes,m.owned,&out,PIVOT_ASSET_INVALID);
    memset(data,0,bytes); free(data); free(bad); pivot_free(&p);
    CHECK(!strcmp(out.definition.fields[PIVOT_FILTERS][0].name,"Ürün") && !strcmp(out.definition.fields[PIVOT_VALUES][1].name,"Amount"));
    roundtrip(&out.definition); pivot_asset_free(&out);
}
static void invalid_and_budgets(void) {
    PivotDefinition p={0}; PivotAssetMeasure m={0}; CHECK(pivot_assign(&p,PIVOT_FILTERS,"Value",AGG_SUM,NULL));
    PivotField *f=&p.fields[PIVOT_FILTERS][0]; f->filter=FILTER_BETWEEN; f->value=dx_strdup("[\"1.0001\",\"9007199254740993.0002\"]");
    size_t bytes=0; unsigned char *data=encode(&p,&bytes,&m),*other=dx_alloc(bytes); memset(other,0xa5,bytes); size_t written=77;
    CHECK(pivot_asset_encode(1,1,&p,1,m.validation_scratch-1,other,bytes,&written)==PIVOT_ASSET_LIMIT && written==77 && other[0]==0xa5);
    CHECK(pivot_asset_encode(1,1,&p,1,m.validation_scratch,other,bytes-1,&written)==PIVOT_ASSET_LIMIT && written==77 && other[0]==0xa5);
    PivotAsset out={0}; reject(data,bytes,m.owned+m.validation_scratch-1,&out,PIVOT_ASSET_LIMIT);
    CHECK(pivot_asset_decode(data,bytes,3,m.owned+m.validation_scratch,&out)==PIVOT_ASSET_OK);
    free(f->value); f->value=dx_strdup("[\"only-one\"]"); CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_OK);
    CHECK(pivot_asset_encode(1,1,&p,1,m.validation_scratch,other,bytes,&written)==PIVOT_ASSET_INVALID && written==77 && other[0]==0xa5);
    CHECK(!pivot_valid(&p,NULL)); /* Invalid definitions must not require diagnostics. */
    free(f->value); f->value=analysis_filter_pack(FILTER_IN,"[null,\"\",\"NULL\"]",FILTER_BETWEEN,"[\"1\",\"2\"]"); CHECK(f->value); f->filter=FILTER_COMPOSITE; roundtrip(&p);
    f->grain=PIVOT_DATE_YEAR; CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_INVALID); f->grain=PIVOT_DATE_EXACT;
    free(data); free(other); pivot_asset_free(&out); pivot_free(&p);
    p=(PivotDefinition){0}; p.fields[PIVOT_ROWS][0].name="unused"; CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_INVALID);
    p=(PivotDefinition){0}; CHECK(pivot_asset_measure(0,1,&p,1,&m)==PIVOT_ASSET_INVALID && pivot_asset_measure(1,0,&p,1,&m)==PIVOT_ASSET_INVALID && pivot_asset_measure(1,2,&p,1,&m)==PIVOT_ASSET_INVALID);
    p.counts[0]=9; CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_INVALID);
    p=(PivotDefinition){0}; CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_SUM,NULL)); CHECK(pivot_assign(&p,PIVOT_VALUES,"Amount",AGG_COUNT,NULL));
    p.fields[PIVOT_VALUES][1].aggregate=AGG_SUM; CHECK(!pivot_valid(&p,NULL));
    CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_OK); other=dx_alloc(m.bytes);
    CHECK(pivot_asset_encode(1,1,&p,1,0,other,m.bytes,&written)==PIVOT_ASSET_INVALID); free(other); pivot_free(&p);
}
static void full_vectors_and_boundary(void) {
    PivotDefinition p={0};
    for (unsigned role=0;role<4;role++) for (size_t i=0;i<DX_PIVOT_FIELDS;i++) {
        char *name=dx_printf("field%u_%zu",role,i); CHECK(pivot_assign(&p,(PivotRole)role,name,(Aggregate)(i%6),NULL)); free(name);
    }
    roundtrip(&p); pivot_free(&p);
    CHECK(pivot_assign(&p,PIVOT_FILTERS,"Value",AGG_SUM,NULL)); PivotField *f=&p.fields[PIVOT_FILTERS][0];
    const char *values[]={NULL,"","NULL"};
    for (size_t i=0;i<3;i++) { f->value=values[i] ? dx_strdup(values[i]) : NULL; roundtrip(&p); free(f->value); f->value=NULL; }
    size_t n=DX_DEFINITION_OWNED_BYTES-sizeof(PivotDefinition)-strlen(f->name)-2; f->value=dx_alloc(n+2); memset(f->value,'x',n); f->value[n]=0;
    PivotAssetMeasure m={0}; CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_OK && m.owned==DX_DEFINITION_OWNED_BYTES); roundtrip(&p);
    f->value[n]='x'; f->value[n+1]=0; CHECK(pivot_asset_measure(1,1,&p,1,&m)==PIVOT_ASSET_INVALID); pivot_free(&p);
}
int main(void) { oracle(); fields_and_corruption(); invalid_and_budgets(); full_vectors_and_boundary(); printf("PASS: %u closed Pivot/Chart definition asset checks\n",checks); return 0; }
