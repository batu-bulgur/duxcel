#include "workbook/analysis_asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL analysis asset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static size_t skip_text(const unsigned char *data,size_t at) {
    uint32_t n=0; for (unsigned i=0;i<4;i++) n|=(uint32_t)data[at+i]<<(8*i);
    return at+4+(n==UINT32_MAX ? 0 : n);
}
static unsigned char *encode(const Analysis *a,size_t *bytes,AnalysisAssetMeasure *measure) {
    CHECK(analysis_asset_measure(UINT64_MAX,7,a,7,measure)==ANALYSIS_ASSET_OK);
    unsigned char *data=dx_alloc(measure->bytes); *bytes=123;
    CHECK(analysis_asset_encode(UINT64_MAX,7,a,7,measure->validation_scratch,data,measure->bytes,bytes)==ANALYSIS_ASSET_OK && *bytes==measure->bytes);
    DefinitionAssetInfo info={0}; CHECK(analysis_asset_preflight(data,*bytes,7,&info)==ANALYSIS_ASSET_OK && info.sheet==UINT64_MAX && info.owner==7 &&
        info.measure.bytes==measure->bytes && info.measure.owned==measure->owned && info.measure.validation_scratch==measure->validation_scratch);
    DefinitionAssetInfo before=info;
    CHECK(analysis_asset_preflight(data,31,7,&info)==ANALYSIS_ASSET_INVALID && !memcmp(&before,&info,sizeof(before)));
    CHECK(analysis_asset_preflight(data,*bytes,7,NULL)==ANALYSIS_ASSET_INVALID);
    return data;
}
static void reject(const void *data,size_t bytes,size_t budget,AnalysisAsset *out,AnalysisAssetStatus expected) {
    AnalysisAsset before=*out; CHECK(analysis_asset_decode(data,bytes,7,budget,out)==expected && !memcmp(&before,out,sizeof(before)));
}
static void roundtrip(const Analysis *a) {
    AnalysisAssetMeasure m={0}; size_t bytes=0; unsigned char *data=encode(a,&bytes,&m); AnalysisAsset out={0};
    CHECK(analysis_asset_decode(data,bytes,7,m.owned+m.validation_scratch,&out)==ANALYSIS_ASSET_OK);
    CHECK(out.sheet==UINT64_MAX && out.owner==7 && !out.definition.filter_number.kind);
    size_t written=0; unsigned char *other=dx_alloc(bytes);
    CHECK(analysis_asset_encode(out.sheet,out.owner,&out.definition,7,m.validation_scratch,other,bytes,&written)==ANALYSIS_ASSET_OK && written==bytes && !memcmp(data,other,bytes));
    free(other); free(data); analysis_asset_free(&out);
}
static void oracle_and_corruption(void) {
    Analysis a={0}; AnalysisAssetMeasure m={0}; size_t bytes=0; unsigned char *data=encode(&a,&bytes,&m);
    unsigned char oracle[84]={0}; memcpy(oracle,"DXAN",4); oracle[4]=1; put(oracle+8,UINT64_MAX,8); oracle[16]=7; oracle[24]=52;
    memset(oracle+48,255,36); CHECK(bytes==sizeof(oracle) && !memcmp(oracle,data,sizeof(oracle)) && m.owned==sizeof(Analysis) && !m.validation_scratch);
    AnalysisAsset out={0}; CHECK(analysis_asset_decode(data,bytes,7,sizeof(Analysis),&out)==ANALYSIS_ASSET_OK);
    for (size_t n=0;n<bytes;n++) reject(data,n,DX_ANALYSIS_ASSET_OWNED_BYTES,&out,ANALYSIS_ASSET_INVALID);
    unsigned char *bad=dx_alloc(bytes+1); memcpy(bad,data,bytes); bad[bytes]=0; reject(bad,bytes+1,DX_ANALYSIS_ASSET_OWNED_BYTES,&out,ANALYSIS_ASSET_INVALID);
    struct { size_t at; unsigned value; AnalysisAssetStatus status; } cases[]={
        {0,'X',ANALYSIS_ASSET_INVALID},{4,2,ANALYSIS_ASSET_UNSUPPORTED},{5,1,ANALYSIS_ASSET_UNSUPPORTED},
        {6,1,ANALYSIS_ASSET_INVALID},{28,1,ANALYSIS_ASSET_INVALID},{32,5,ANALYSIS_ASSET_INVALID},
        {33,4,ANALYSIS_ASSET_INVALID},{34,12,ANALYSIS_ASSET_INVALID},{35,6,ANALYSIS_ASSET_INVALID},
        {44,16,ANALYSIS_ASSET_INVALID},{45,8,ANALYSIS_ASSET_INVALID},{46,1,ANALYSIS_ASSET_INVALID},{47,1,ANALYSIS_ASSET_INVALID}};
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) { memcpy(bad,data,bytes); bad[cases[i].at]=(unsigned char)cases[i].value; reject(bad,bytes,sizeof(Analysis),&out,cases[i].status); }
    memcpy(bad,data,bytes); memset(bad+8,0,8); reject(bad,bytes,sizeof(Analysis),&out,ANALYSIS_ASSET_INVALID);
    memcpy(bad,data,bytes); put(bad+16,8,8); reject(bad,bytes,sizeof(Analysis),&out,ANALYSIS_ASSET_INVALID);
    reject(data,bytes,sizeof(Analysis)-1,&out,ANALYSIS_ASSET_LIMIT); reject(NULL,bytes,sizeof(Analysis),&out,ANALYSIS_ASSET_INVALID);
    CHECK(analysis_asset_decode(data,bytes,7,sizeof(Analysis),NULL)==ANALYSIS_ASSET_INVALID);
    free(bad); free(data); analysis_asset_free(&out); analysis_asset_free(NULL);
}
static Analysis rich(void) {
    Analysis a={.kind=SOURCE_PARQUET,.source_version=UINT64_MAX,.path=dx_strdup("/a.parquet"),.schema=dx_strdup(""),.table=dx_strdup("NULL"),
        .query=dx_strdup("SELECT * FROM input"),.unique_rows=true,.group_column=dx_strdup("Ürün"),.measure_column=dx_strdup("Amount"),.aggregate=AGG_AVG,
        .filter_column=dx_strdup("Id"),.filter_value=dx_strdup("9007199254740993"),.filter_op=FILTER_EQUAL,.filter_number={NUM_I64,0,0},
        .sort_column=dx_strdup("Ürün"),.descending=true,.filter_count=1,.sort_count=1};
    a.filters[0]=(AnalysisFilter){dx_strdup("Region"),dx_strdup(""),FILTER_CONTAINS,{NUM_DECIMAL,38,4}};
    a.sorts[0]=(AnalysisSort){dx_strdup("Amount"),false}; return a;
}
static void definitions_and_ownership(void) {
    Analysis a=rich(); AnalysisAssetMeasure m={0}; size_t bytes=0; unsigned char *data=encode(&a,&bytes,&m); AnalysisAsset out={0};
    CHECK(analysis_asset_decode(data,bytes,7,m.owned,&out)==ANALYSIS_ASSET_OK);
    CHECK(out.definition.path!=a.path && out.definition.schema && !*out.definition.schema && !strcmp(out.definition.table,"NULL") &&
        out.definition.source_version==UINT64_MAX && out.definition.filter_count==1 && out.definition.sort_count==1 && !out.definition.filters[0].number.kind &&
        !strcmp(out.definition.filter_value,"9007199254740993") && out.definition.descending && !out.definition.sorts[0].descending);
    CHECK(out.definition.kind==SOURCE_PARQUET && out.definition.unique_rows && out.definition.aggregate==AGG_AVG &&
        !strcmp(out.definition.measure_column,"Amount") && !strcmp(out.definition.sort_column,"Ürün") &&
        !strcmp(out.definition.sorts[0].column,"Amount") && out.definition.filters[0].op==FILTER_CONTAINS);
    /* Inner framing must reject every prefix even with a consistent outer size. */
    unsigned char *bad=dx_alloc(bytes);
    for (size_t n=32;n<bytes;n++) { memcpy(bad,data,n); put(bad+24,n-32,4); reject(bad,n,m.owned,&out,ANALYSIS_ASSET_INVALID); }
    memcpy(bad,data,bytes); bad[52]=0; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID); /* Embedded NUL in path. */
    memcpy(bad,data,bytes); bad[52]=0xc0; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID); /* Overlong UTF-8. */
    size_t filter_at=48; for (size_t i=0;i<9;i++) filter_at=skip_text(data,filter_at);
    memcpy(bad,data,bytes); bad[filter_at]=255; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID);
    memcpy(bad,data,bytes); bad[filter_at+1]=1; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID);
    size_t sort_at=skip_text(data,skip_text(data,filter_at+4));
    memcpy(bad,data,bytes); bad[sort_at]=2; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID);
    memcpy(bad,data,bytes); bad[sort_at+3]=1; reject(bad,bytes,m.owned,&out,ANALYSIS_ASSET_INVALID);
    memset(data,0,bytes); free(data); free(bad); analysis_free(&a);
    CHECK(!strcmp(out.definition.group_column,"Ürün") && !strcmp(out.definition.query,"SELECT * FROM input") && !out.definition.filters[0].value[0]);
    roundtrip(&out.definition); analysis_asset_free(&out);
    Analysis none={0}; roundtrip(&none);
    for (unsigned kind=SOURCE_CSV;kind<=SOURCE_DEMO;kind++) { Analysis source={.kind=(SourceKind)kind,.path="/source"}; roundtrip(&source); }
    const char *values[]={NULL,"","NULL"};
    for (size_t i=0;i<3;i++) {
        Analysis source={.kind=SOURCE_CSV,.filter_column="Id",.filter_value=(char *)values[i],.filter_op=FILTER_EQUAL};
        roundtrip(&source);
    }
}
static void grammar_budgets_and_limits(void) {
    Analysis a={.kind=SOURCE_CSV,.path="/source",.filter_column="Amount",.filter_value="[\"1.0001\",\"99999999999999999999999999.0002\"]",.filter_op=FILTER_BETWEEN};
    AnalysisAssetMeasure m={0}; size_t bytes=0; unsigned char *data=encode(&a,&bytes,&m),*other=dx_alloc(bytes);
    CHECK(m.validation_scratch==analysis_filter_validation_scratch(FILTER_BETWEEN,strlen(a.filter_value),true));
    CHECK(!analysis_filter_validation_scratch(FILTER_EQUAL,42,true) && !analysis_filter_validation_scratch(FILTER_COMPOSITE,42,false));
    CHECK(analysis_filter_validation_scratch(FILTER_COMPOSITE,DX_ANALYSIS_ASSET_OWNED_BYTES+1u,true)==SIZE_MAX);
    CHECK(m.validation_scratch>0); memset(other,0xa5,bytes); size_t written=77;
    CHECK(analysis_asset_encode(1,0,&a,0,m.validation_scratch-1,other,bytes,&written)==ANALYSIS_ASSET_LIMIT && written==77 && other[0]==0xa5 && other[bytes-1]==0xa5);
    CHECK(analysis_asset_encode(1,0,&a,0,m.validation_scratch,other,bytes-1,&written)==ANALYSIS_ASSET_LIMIT && written==77 && other[0]==0xa5);
    AnalysisAsset out={0}; reject(data,bytes,m.owned+m.validation_scratch-1,&out,ANALYSIS_ASSET_LIMIT);
    CHECK(analysis_asset_decode(data,bytes,7,m.owned+m.validation_scratch,&out)==ANALYSIS_ASSET_OK);
    unsigned char *corrupt=dx_alloc(bytes); memcpy(corrupt,data,bytes);
    size_t value_at=48; for (size_t i=0;i<5;i++) value_at=skip_text(data,value_at);
    corrupt[value_at+4]='x'; DefinitionAssetInfo framed={0};
    CHECK(analysis_asset_preflight(corrupt,bytes,7,&framed)==ANALYSIS_ASSET_OK && framed.measure.owned==m.owned);
    reject(corrupt,bytes,m.owned+m.validation_scratch,&out,ANALYSIS_ASSET_INVALID); free(corrupt);
    char *malformed="[\"one\"]"; a.filter_value=malformed;
    CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_OK);
    CHECK(analysis_asset_encode(1,0,&a,0,m.validation_scratch,other,bytes,&written)==ANALYSIS_ASSET_INVALID && written==77 && other[0]==0xa5);
    char *composite=analysis_filter_pack(FILTER_IN,"[null,\"\",\"NULL\"]",FILTER_BETWEEN,"[\"1\",\"2\"]"); CHECK(composite);
    a.filter_value=composite; a.filter_op=FILTER_COMPOSITE; roundtrip(&a); free(composite);
    free(data); free(other); analysis_asset_free(&out);
    a=(Analysis){.kind=SOURCE_CSV,.query=dx_alloc(DX_QUERY_BYTES+2)};
    memset(a.query,'s',DX_QUERY_BYTES+1); a.query[DX_QUERY_BYTES]=0; roundtrip(&a);
    a.query[DX_QUERY_BYTES]='s'; a.query[DX_QUERY_BYTES+1]=0; AnalysisAssetMeasure sentinel={1,2,3},before=sentinel;
    CHECK(analysis_asset_measure(1,0,&a,0,&sentinel)==ANALYSIS_ASSET_INVALID && !memcmp(&sentinel,&before,sizeof(before))); analysis_free(&a);
    a=(Analysis){.kind=SOURCE_DATABASE}; size_t n=DX_ANALYSIS_ASSET_OWNED_BYTES-sizeof(Analysis)-1; a.schema=dx_alloc(n+2); memset(a.schema,'x',n); a.schema[n]=0;
    CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_OK && m.owned==DX_ANALYSIS_ASSET_OWNED_BYTES); roundtrip(&a);
    a.schema[n]='x'; a.schema[n+1]=0; CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_INVALID); analysis_free(&a);
    a=(Analysis){.kind=(SourceKind)-1}; CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_INVALID);
    a=(Analysis){0}; a.filters[0].column="unused"; CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_INVALID);
    a=(Analysis){0}; a.sorts[0].descending=true; CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_INVALID);
    a=(Analysis){0}; CHECK(analysis_asset_measure(0,0,&a,0,&m)==ANALYSIS_ASSET_INVALID && analysis_asset_measure(1,1,&a,0,&m)==ANALYSIS_ASSET_INVALID);
}
static void full_vectors(void) {
    Analysis a={.kind=SOURCE_CSV};
    for (size_t i=0;i<DX_ANALYSIS_FILTERS;i++) { char *name=dx_printf("field%zu",i); CHECK(analysis_filter_set(&a,name,FILTER_EQUAL,i%2 ? "" : "NULL",NULL)); free(name); }
    for (size_t i=0;i<DX_ANALYSIS_SORTS;i++) { char *name=dx_printf("field%zu",i); CHECK(analysis_sort_set(&a,name,i%2,true,NULL)); free(name); }
    CHECK(a.filter_count==15 && a.sort_count==7); roundtrip(&a);
    AnalysisAssetMeasure m={0}; char *saved=a.sorts[0].column; a.sorts[0].column=a.sort_column;
    CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_INVALID); a.sorts[0].column=saved;
    saved=a.filters[0].column; a.filters[0].column=a.filter_column; size_t bytes=0;
    CHECK(analysis_asset_measure(1,0,&a,0,&m)==ANALYSIS_ASSET_OK); unsigned char *data=dx_alloc(m.bytes);
    CHECK(analysis_asset_encode(1,0,&a,0,m.validation_scratch,data,m.bytes,&bytes)==ANALYSIS_ASSET_INVALID); free(data); a.filters[0].column=saved;
    analysis_free(&a);
}
int main(void) { oracle_and_corruption(); definitions_and_ownership(); grammar_budgets_and_limits(); full_vectors(); printf("PASS: %u closed analysis definition asset checks\n",checks); return 0; }
