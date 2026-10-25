#include "workbook/manual_value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL manual value:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ManualValueInput text(const char *raw,size_t bytes) { return (ManualValueInput){true,MANUAL_VALUE_TEXT,{0},raw,bytes}; }
static ManualValueInput number(NumberKind kind,unsigned precision,unsigned scale,const char *raw) {
    return (ManualValueInput){true,MANUAL_VALUE_NUMBER,{kind,(unsigned char)precision,(unsigned char)scale},raw,strlen(raw)};
}
static ManualValue *build(ManualValueInput input,const char *canonical) {
    ManualValueInfo info={0}; CHECK(manual_value_measure(input,&info)==MANUAL_VALUE_OK);
    ManualValue *out=NULL; CHECK(manual_value_build(input,info.owned,&out)==MANUAL_VALUE_OK);
    ManualValueView view=manual_value_view(out);
    CHECK(view.present==input.present && view.kind==input.kind && view.number.kind==input.number.kind && view.number.precision==input.number.precision && view.number.scale==input.number.scale);
    CHECK(view.owned==info.owned && view.raw_bytes==input.raw_bytes && view.canonical_bytes==info.canonical_bytes);
    if (canonical) CHECK(view.raw && view.canonical && !memcmp(view.raw,input.raw,input.raw_bytes) && !view.raw[input.raw_bytes] && !strcmp(view.canonical,canonical));
    else CHECK(!view.raw && !view.canonical && !view.raw_bytes && !view.canonical_bytes);
    ManualValue *clone=NULL; CHECK(manual_value_clone(out,view.owned,&clone)==MANUAL_VALUE_OK && manual_value_equal(out,clone));
    if (out) CHECK(out!=clone);
    manual_value_free(&clone); CHECK(!clone); return out;
}
static void reject(ManualValueInput input,ManualValueStatus status) {
    ManualValueInfo info={31,37,41},before=info;
    ManualValue *out=build(text("Retained",8),"Retained"),*old=out;
    ManualValueView view=manual_value_view(out);
    CHECK(manual_value_measure(input,&info)==status && !memcmp(&info,&before,sizeof(info)));
    CHECK(manual_value_build(input,SIZE_MAX,&out)==status && out==old && manual_value_view(out).raw==view.raw && !strcmp(view.raw,"Retained"));
    manual_value_free(&out);
}
static void presence_and_text(void) {
    ManualValue *missing=build((ManualValueInput){0},NULL); CHECK(!missing);
    ManualValue *null=build((ManualValueInput){.present=true},NULL);
    ManualValue *empty=build(text(NULL,0),""),*literal=build(text("NULL",4),"NULL");
    CHECK(null && empty && literal && !manual_value_equal(missing,null) && !manual_value_equal(null,empty) && !manual_value_equal(empty,literal));
    ManualValueView zero=manual_value_view(empty); CHECK(zero.present && zero.raw==zero.canonical && zero.raw && !zero.raw_bytes);
    const char utf8[]="İstanbul 🦆\tline\nNULL\r\"quoted\"";
    ManualValue *value=build(text(utf8,sizeof(utf8)-1),utf8); manual_value_free(&value);
    value=build(text("=A1+1",5),"=A1+1"); CHECK(manual_value_view(value).kind==MANUAL_VALUE_TEXT); manual_value_free(&value);
    const char raw[]={'A','B'}; value=build(text(raw,sizeof(raw)),"AB"); manual_value_free(&value);
    unsigned char invalid[][4]={{0},{0xc0,0x80},{0xc2},{0xe0,0x80,0x80},{0xed,0xa0,0x80},{0xf0,0x80,0x80,0x80},{0xf4,0x90,0x80,0x80},{0x80},{0xf5,0x80,0x80,0x80}};
    size_t sizes[]={1,2,1,3,3,4,4,1,4};
    for (size_t i=0;i<sizeof(sizes)/sizeof(*sizes);i++) reject(text((const char *)invalid[i],sizes[i]),MANUAL_VALUE_INVALID);
    reject((ManualValueInput){false,MANUAL_VALUE_TEXT,{0},NULL,0},MANUAL_VALUE_INVALID);
    reject((ManualValueInput){.raw="",.raw_bytes=0},MANUAL_VALUE_INVALID);
    reject((ManualValueInput){.present=true,.raw="NULL",.raw_bytes=4},MANUAL_VALUE_INVALID);
    reject((ManualValueInput){true,MANUAL_VALUE_SQL_NULL,{NUM_I64,0,0},NULL,0},MANUAL_VALUE_INVALID);
    reject((ManualValueInput){true,MANUAL_VALUE_TEXT,{NUM_NONE,1,0},"x",1},MANUAL_VALUE_INVALID);
    reject(text(NULL,1),MANUAL_VALUE_INVALID);
    reject((ManualValueInput){true,(ManualValueKind)99,{0},NULL,0},MANUAL_VALUE_INVALID);
    manual_value_free(&null); manual_value_free(&empty); manual_value_free(&literal); manual_value_free(&missing); manual_value_free(NULL);
}
static void integer_limits(void) {
    struct { NumberKind kind; const char *min,*max,*below,*above; } cases[]={
        {NUM_I8,"-128","127","-129","128"},{NUM_I16,"-32768","32767","-32769","32768"},
        {NUM_I32,"-2147483648","2147483647","-2147483649","2147483648"},
        {NUM_I64,"-9223372036854775808","9223372036854775807","-9223372036854775809","9223372036854775808"},
        {NUM_I128,"-170141183460469231731687303715884105728","170141183460469231731687303715884105727","-170141183460469231731687303715884105729","170141183460469231731687303715884105728"},
        {NUM_U8,"0","255",NULL,"256"},{NUM_U16,"0","65535",NULL,"65536"},
        {NUM_U32,"0","4294967295",NULL,"4294967296"},{NUM_U64,"0","18446744073709551615",NULL,"18446744073709551616"},
        {NUM_U128,"0","340282366920938463463374607431768211455",NULL,"340282366920938463463374607431768211456"}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        ManualValue *low=build(number(cases[i].kind,0,0,cases[i].min),cases[i].min),*high=build(number(cases[i].kind,0,0,cases[i].max),cases[i].max);
        reject(number(cases[i].kind,0,0,cases[i].above),MANUAL_VALUE_RANGE);
        if (cases[i].below) reject(number(cases[i].kind,0,0,cases[i].below),MANUAL_VALUE_RANGE);
        else reject(number(cases[i].kind,0,0,"-0"),MANUAL_VALUE_INVALID);
        manual_value_free(&low); manual_value_free(&high);
    }
    ManualValue *v=build(number(NUM_I64,0,0,"9007199254740993"),"9007199254740993"); manual_value_free(&v);
    v=build(number(NUM_I8,0,0,"+000127"),"127"); manual_value_free(&v);
    v=build(number(NUM_I128,0,0,"-000000"),"0"); manual_value_free(&v);
    for (int i=-128;i<=127;i++) { char input[32]; snprintf(input,sizeof(input),"%d",i); v=build(number(NUM_I8,0,0,input),input); manual_value_free(&v); }
    for (unsigned i=0;i<=255;i++) { char input[32]; snprintf(input,sizeof(input),"%u",i); v=build(number(NUM_U8,0,0,input),input); manual_value_free(&v); }
    reject(number(NUM_I64,1,0,"1"),MANUAL_VALUE_INVALID); reject(number(NUM_U64,0,1,"1"),MANUAL_VALUE_INVALID);
    reject(number(NUM_NONE,0,0,"1"),MANUAL_VALUE_INVALID); reject(number(NUM_FLOAT,0,0,"1"),MANUAL_VALUE_UNSUPPORTED);
    reject(number((NumberKind)99,0,0,"1"),MANUAL_VALUE_INVALID);
}
static void decimal_limits(void) {
    struct { unsigned precision,scale; const char *raw,*canonical; } cases[]={
        {5,2,"+0012.3","12.30"},{38,2,"-000.0","0.00"},{2,2,"0.99","0.99"},{2,2,"-0.99","-0.99"},
        {38,38,"-0.99999999999999999999999999999999999999","-0.99999999999999999999999999999999999999"},
        {38,0,"99999999999999999999999999999999999999","99999999999999999999999999999999999999"},
        {38,2,"-123456789012345678901234567890123456.78","-123456789012345678901234567890123456.78"},
        {1,0,"+0","0"},{38,38,"0","0.00000000000000000000000000000000000000"}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) { ManualValue *v=build(number(NUM_DECIMAL,cases[i].precision,cases[i].scale,cases[i].raw),cases[i].canonical); manual_value_free(&v); }
    reject(number(NUM_DECIMAL,2,2,"1.00"),MANUAL_VALUE_RANGE);
    reject(number(NUM_DECIMAL,3,2,"10"),MANUAL_VALUE_RANGE);
    reject(number(NUM_DECIMAL,38,0,"100000000000000000000000000000000000000"),MANUAL_VALUE_RANGE);
    reject(number(NUM_DECIMAL,5,2,"1.230"),MANUAL_VALUE_RANGE); reject(number(NUM_DECIMAL,5,2,"0.000"),MANUAL_VALUE_RANGE);
    reject(number(NUM_DECIMAL,1,0,"1.0"),MANUAL_VALUE_RANGE);
    reject(number(NUM_DECIMAL,0,0,"0"),MANUAL_VALUE_INVALID); reject(number(NUM_DECIMAL,39,2,"0"),MANUAL_VALUE_INVALID); reject(number(NUM_DECIMAL,2,3,"0"),MANUAL_VALUE_INVALID);
    for (unsigned p=1;p<=38;p++) for (unsigned scale=0;scale<=p;scale++) {
        ManualValue *v=NULL; ManualValueInfo info; ManualValueInput input=number(NUM_DECIMAL,p,scale,"0");
        CHECK(manual_value_measure(input,&info)==MANUAL_VALUE_OK && manual_value_build(input,info.owned,&v)==MANUAL_VALUE_OK);
        ManualValueView view=manual_value_view(v); CHECK(view.canonical_bytes==(scale ? scale+2u : 1u) && view.raw_bytes==1 && view.canonical[view.canonical_bytes]==0);
        manual_value_free(&v);
    }
}
static void strict_grammar(void) {
    const char *bad[]={""," "," 1","1 ","\t1","1\n","+","-",".5","-.5","1.","1e3","1E+3","NaN","Infinity","1,000","1,5","١٢","−1","0x10","1_000","--1","+-1","1..2","NULL"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        reject(number(NUM_I128,0,0,bad[i]),MANUAL_VALUE_INVALID);
        reject(number(NUM_DECIMAL,38,4,bad[i]),MANUAL_VALUE_INVALID);
    }
    reject((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I64,0,0},NULL,1},MANUAL_VALUE_INVALID);
    reject((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I64,0,0},NULL,DX_MANUAL_VALUE_NUMBER_BYTES+1},MANUAL_VALUE_INVALID);
    const char embedded[]={'1',0,'2'};
    reject((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I64,0,0},embedded,sizeof(embedded)},MANUAL_VALUE_INVALID);
    const char span[]={'1','2'}; ManualValue *v=build((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I8,0,0},span,sizeof(span)},"12"); manual_value_free(&v);
}
static void lifetime_and_budget(void) {
    ManualValueInput input=number(NUM_DECIMAL,5,2,"+0012.3"); ManualValueInfo info;
    CHECK(manual_value_measure(input,&info)==MANUAL_VALUE_OK);
    ManualValue *value=build(text("Old",3),"Old"),*prior=value;
    CHECK(manual_value_build(input,info.owned-1,&value)==MANUAL_VALUE_LIMIT && value==prior && !strcmp(manual_value_view(value).raw,"Old"));
    CHECK(manual_value_build(input,info.owned,&value)==MANUAL_VALUE_OK);
    ManualValueView view=manual_value_view(value); CHECK(!strcmp(view.raw,"+0012.3") && !strcmp(view.canonical,"12.30") && view.raw!=view.canonical);
    ManualValue *clone=NULL; CHECK(manual_value_clone(value,view.owned-1,&clone)==MANUAL_VALUE_LIMIT && !clone);
    CHECK(manual_value_clone(value,view.owned,&clone)==MANUAL_VALUE_OK && manual_value_equal(value,clone));
    prior=value; CHECK(manual_value_clone(value,view.owned-1,&value)==MANUAL_VALUE_LIMIT && value==prior);
    CHECK(manual_value_clone(value,view.owned,&value)==MANUAL_VALUE_OK && manual_value_equal(value,clone));
    view=manual_value_view(value); input=(ManualValueInput){view.present,view.kind,view.number,view.raw,view.raw_bytes};
    CHECK(manual_value_build(input,view.owned,&value)==MANUAL_VALUE_OK && manual_value_equal(value,clone));
    ManualValue *same=build(number(NUM_DECIMAL,5,2,"12.30"),"12.30"),*different_type=build(number(NUM_DECIMAL,4,2,"+0012.3"),"12.30");
    CHECK(!manual_value_equal(value,same) && !manual_value_equal(value,different_type));
    manual_value_free(&same); manual_value_free(&different_type); manual_value_free(&clone);
    CHECK(manual_value_build((ManualValueInput){0},0,&value)==MANUAL_VALUE_OK && !value);
    value=build(text("Owned",5),"Owned"); CHECK(manual_value_clone(NULL,0,&value)==MANUAL_VALUE_OK && !value);
    char *borrowed=malloc(7); CHECK(borrowed); memcpy(borrowed,"+00012",7); input=number(NUM_I64,0,0,borrowed); value=build(input,"12");
    memset(borrowed,'x',6); free(borrowed); view=manual_value_view(value); CHECK(!strcmp(view.raw,"+00012") && !strcmp(view.canonical,"12")); manual_value_free(&value);
    CHECK(manual_value_measure((ManualValueInput){0},NULL)==MANUAL_VALUE_INVALID && manual_value_build((ManualValueInput){0},0,NULL)==MANUAL_VALUE_INVALID && manual_value_clone(NULL,0,NULL)==MANUAL_VALUE_INVALID);
}
static void byte_bounds(void) {
    char *raw=malloc(DX_MANUAL_VALUE_TEXT_BYTES+1); CHECK(raw); memset(raw,'x',DX_MANUAL_VALUE_TEXT_BYTES); raw[DX_MANUAL_VALUE_TEXT_BYTES]=0;
    ManualValue *v=build(text(raw,DX_MANUAL_VALUE_TEXT_BYTES),raw); CHECK(manual_value_view(v).raw_bytes==DX_MANUAL_VALUE_TEXT_BYTES); manual_value_free(&v); free(raw);
    reject(text((const char *)1,DX_MANUAL_VALUE_TEXT_BYTES+1),MANUAL_VALUE_LIMIT);
    raw=malloc(DX_MANUAL_VALUE_NUMBER_BYTES+1); CHECK(raw); memset(raw,'0',DX_MANUAL_VALUE_NUMBER_BYTES); raw[DX_MANUAL_VALUE_NUMBER_BYTES-1]='1'; raw[DX_MANUAL_VALUE_NUMBER_BYTES]=0;
    v=build(number(NUM_I8,0,0,raw),"1"); CHECK(manual_value_view(v).raw_bytes==DX_MANUAL_VALUE_NUMBER_BYTES); manual_value_free(&v); free(raw);
    reject((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I128,0,0},(const char *)1,DX_MANUAL_VALUE_NUMBER_BYTES+1},MANUAL_VALUE_LIMIT);
    v=build(number(NUM_DECIMAL,38,38,"-0.99999999999999999999999999999999999999"),"-0.99999999999999999999999999999999999999");
    CHECK(manual_value_view(v).canonical_bytes==DX_MANUAL_VALUE_CANONICAL_BYTES); manual_value_free(&v);
}
static void locale_independence(void) {
    const char *names[]={"C","tr_TR.UTF-8","de_DE.UTF-8","fr_FR.UTF-8"};
    unsigned available=0;
    for (size_t i=0;i<sizeof(names)/sizeof(*names);i++) {
        if (!setlocale(LC_NUMERIC,names[i])) continue;
        available++;
        ManualValue *v=build(number(NUM_DECIMAL,8,3,"+0012.5"),"12.500"); manual_value_free(&v);
        reject(number(NUM_DECIMAL,8,3,"12,5"),MANUAL_VALUE_INVALID);
        v=build(text("12,5",4),"12,5"); manual_value_free(&v);
    }
    CHECK(available && setlocale(LC_NUMERIC,"C"));
}
int main(void) {
    presence_and_text(); integer_limits(); decimal_limits(); strict_grammar(); lifetime_and_budget(); byte_bounds(); locale_independence();
    printf("PASS: %u typed manual value ownership checks\n",checks); return 0;
}
