#include "workbook/manual_value_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL manual value wire:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ManualValueInput text(const char *raw,size_t bytes) { return (ManualValueInput){true,MANUAL_VALUE_TEXT,{0},raw,bytes}; }
static ManualValueInput number(NumberKind kind,unsigned precision,unsigned scale,const char *raw) {
    return (ManualValueInput){true,MANUAL_VALUE_NUMBER,{kind,(unsigned char)precision,(unsigned char)scale},raw,strlen(raw)};
}
static ManualValue *value(ManualValueInput input) {
    ManualValue *out=NULL; CHECK(manual_value_build(input,SIZE_MAX,&out)==MANUAL_VALUE_OK); return out;
}
static ManualValuePacket packet(ManualValueInput input) {
    ManualValueWireInfo info={0}; CHECK(manual_value_wire_measure(input,&info)==MANUAL_VALUE_WIRE_OK);
    ManualValuePacket out={0}; CHECK(manual_value_wire_capture(input,info.bytes,&out)==MANUAL_VALUE_WIRE_OK && out.bytes==info.bytes); return out;
}
static void roundtrip(ManualValueInput input) {
    ManualValuePacket encoded=packet(input); ManualValueWireInfo info={0};
    CHECK(manual_value_wire_preflight(encoded.data,encoded.bytes,&info)==MANUAL_VALUE_WIRE_OK && info.bytes==encoded.bytes && info.raw_bytes==input.raw_bytes && info.present==input.present && info.kind==input.kind);
    CHECK(encoded.bytes==DX_MANUAL_VALUE_WIRE_HEADER_BYTES+input.raw_bytes);
    if (input.raw_bytes) CHECK(!memcmp((char *)encoded.data+DX_MANUAL_VALUE_WIRE_HEADER_BYTES,input.raw,input.raw_bytes));
    ManualValue *out=NULL,*original=value(input);
    CHECK(manual_value_wire_decode(encoded.data,encoded.bytes,info.value_owned,&out)==MANUAL_VALUE_WIRE_OK && manual_value_equal(out,original));
    ManualValueView view=manual_value_view(out);
    ManualValueInput after={view.present,view.kind,view.number,view.raw,view.raw_bytes}; ManualValuePacket again=packet(after);
    CHECK(again.bytes==encoded.bytes && !memcmp(again.data,encoded.data,encoded.bytes));
    manual_value_free(&out); manual_value_free(&original); manual_value_packet_free(&again); manual_value_packet_free(&encoded);
}
static void preserved_rejection(const void *data,size_t bytes,ManualValueWireStatus expected) {
    ManualValue *out=value(text("Prior",5)),*prior=out; ManualValueWireInfo info={.bytes=11,.value_owned=13,.raw_bytes=17,.canonical_bytes=19,.present=true,.kind=MANUAL_VALUE_TEXT},old=info;
    CHECK(manual_value_wire_preflight(data,bytes,&info)==expected && !memcmp(&info,&old,sizeof(info)));
    CHECK(manual_value_wire_decode(data,bytes,SIZE_MAX,&out)==expected && out==prior && !strcmp(manual_value_view(out).raw,"Prior")); manual_value_free(&out);
}
static void goldens(void) {
    unsigned char missing[24]={'D','X','M','V',1},null[24]={'D','X','M','V',1,1},empty[24]={'D','X','M','V',1,1,1};
    ManualValueInput states[]={{0},{.present=true},{true,MANUAL_VALUE_TEXT,{0},NULL,0}};
    const unsigned char *expected[]={missing,null,empty};
    for (size_t i=0;i<3;i++) { ManualValuePacket out=packet(states[i]); CHECK(out.bytes==24 && !memcmp(out.data,expected[i],24)); manual_value_packet_free(&out); roundtrip(states[i]); }
    ManualValuePacket decimal=packet(number(NUM_DECIMAL,5,2,"+0012.3"));
    const unsigned char fixed[24]={'D','X','M','V',1,1,2,11,5,2,0,0,0,0,0,0,7};
    CHECK(decimal.bytes==31 && !memcmp(decimal.data,fixed,24) && !memcmp((char *)decimal.data+24,"+0012.3",7));
    ManualValue *out=NULL; CHECK(manual_value_wire_decode(decimal.data,decimal.bytes,SIZE_MAX,&out)==MANUAL_VALUE_WIRE_OK);
    CHECK(!strcmp(manual_value_view(out).raw,"+0012.3") && !strcmp(manual_value_view(out).canonical,"12.30"));
    manual_value_free(&out); manual_value_packet_free(&decimal);
    roundtrip(text("NULL",4)); roundtrip(text("İstanbul 🦆\t\n\"quoted\"",strlen("İstanbul 🦆\t\n\"quoted\"")));
    const char raw[]={'A','B'}; roundtrip(text(raw,sizeof(raw)));
}
static void numeric_types(void) {
    struct { NumberKind kind; const char *raw; } types[]={
        {NUM_I8,"-128"},{NUM_I16,"-32768"},{NUM_I32,"-2147483648"},{NUM_I64,"-9223372036854775808"},{NUM_I128,"-170141183460469231731687303715884105728"},
        {NUM_U8,"255"},{NUM_U16,"65535"},{NUM_U32,"4294967295"},{NUM_U64,"18446744073709551615"},{NUM_U128,"340282366920938463463374607431768211455"}
    };
    for (size_t i=0;i<sizeof(types)/sizeof(*types);i++) roundtrip(number(types[i].kind,0,0,types[i].raw));
    roundtrip(number(NUM_I64,0,0,"+0009007199254740993"));
    for (unsigned p=1;p<=38;p++) for (unsigned s=0;s<=p;s++) roundtrip(number(NUM_DECIMAL,p,s,"-0"));
    roundtrip(number(NUM_DECIMAL,38,38,"-0.99999999999999999999999999999999999999"));
    roundtrip(number(NUM_DECIMAL,38,0,"99999999999999999999999999999999999999"));
}
static void malformed(void) {
    ManualValuePacket good=packet(number(NUM_DECIMAL,5,2,"12.30"));
    for (size_t size=0;size<good.bytes;size++) preserved_rejection(good.data,size,MANUAL_VALUE_WIRE_INVALID);
    unsigned char *bytes=malloc(good.bytes+1); CHECK(bytes); memcpy(bytes,good.data,good.bytes); bytes[good.bytes]=0;
    preserved_rejection(bytes,good.bytes+1,MANUAL_VALUE_WIRE_INVALID);
    for (size_t i=0;i<4;i++) { bytes[i]^=1; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[i]^=1; }
    unsigned bad[]={0,2,255};
    for (size_t i=0;i<3;i++) { bytes[4]=(unsigned char)bad[i]; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); } bytes[4]=1;
    for (unsigned tag=2;tag<=255;tag++) { bytes[5]=(unsigned char)tag; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); } bytes[5]=1;
    for (unsigned tag=3;tag<=255;tag++) { bytes[6]=(unsigned char)tag; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); } bytes[6]=2;
    for (unsigned tag=12;tag<=255;tag++) { bytes[7]=(unsigned char)tag; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); } bytes[7]=11;
    for (size_t i=10;i<16;i++) { bytes[i]=1; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[i]=0; }
    bytes[8]=0; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[8]=5;
    bytes[9]=6; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[9]=2;
    bytes[16]--; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[16]++;
    bytes[23]=1; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_LIMIT); bytes[23]=0;
    memcpy(bytes+24,"12,30",5); preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID);
    memcpy(bytes+24,"12.31",5); bytes[8]=2; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[8]=5;
    memcpy(bytes+24,"12.30",5); bytes[28]=0; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID);
    free(bytes); manual_value_packet_free(&good);
    good=packet(text("NULL",4)); bytes=good.data; bytes[5]=0; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[5]=1;
    bytes[6]=0; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[6]=1;
    bytes[7]=1; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[7]=0;
    bytes[8]=1; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID); bytes[8]=0;
    bytes[24]=0xc0; bytes[25]=0x80; preserved_rejection(bytes,good.bytes,MANUAL_VALUE_WIRE_INVALID);
    manual_value_packet_free(&good);
    preserved_rejection(NULL,0,MANUAL_VALUE_WIRE_INVALID);
    preserved_rejection((const void *)1,DX_MANUAL_VALUE_WIRE_MAX_BYTES+1,MANUAL_VALUE_WIRE_LIMIT);
}
static void budgets_and_aliases(void) {
    ManualValuePacket out=packet(text("Old",3)); ManualValuePacket prior=out;
    ManualValueInput input=number(NUM_I128,0,0,"+0009007199254740993"); ManualValueWireInfo info;
    CHECK(manual_value_wire_measure(input,&info)==MANUAL_VALUE_WIRE_OK);
    CHECK(manual_value_wire_capture(input,prior.bytes+info.bytes-1,&out)==MANUAL_VALUE_WIRE_LIMIT && out.data==prior.data && out.bytes==prior.bytes);
    CHECK(manual_value_wire_capture(input,prior.bytes+info.bytes,&out)==MANUAL_VALUE_WIRE_OK && out.bytes==info.bytes);
    ManualValue *old=value(text("Retained",8)),*value_prior=old; size_t old_bytes=manual_value_view(old).owned;
    CHECK(manual_value_wire_decode(out.data,out.bytes,old_bytes+info.value_owned-1,&old)==MANUAL_VALUE_WIRE_LIMIT && old==value_prior && !strcmp(manual_value_view(old).raw,"Retained"));
    CHECK(manual_value_wire_decode(out.data,out.bytes,old_bytes+info.value_owned,&old)==MANUAL_VALUE_WIRE_OK && !strcmp(manual_value_view(old).raw,"+0009007199254740993") && !strcmp(manual_value_view(old).canonical,"9007199254740993"));
    /* In-place packet replacement borrows the old frame's raw subspan. */
    input=(ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I128,0,0},(char *)out.data+24,out.bytes-24}; prior=out;
    CHECK(manual_value_wire_capture(input,2*out.bytes-1,&out)==MANUAL_VALUE_WIRE_LIMIT && out.data==prior.data);
    CHECK(manual_value_wire_capture(input,2*out.bytes,&out)==MANUAL_VALUE_WIRE_OK && !memcmp((char *)out.data+24,"+0009007199254740993",strlen("+0009007199254740993")));
    /* Repeated decode replaces an independent prior owner only after admission. */
    CHECK(manual_value_wire_decode(out.data,out.bytes,2*manual_value_view(old).owned,&old)==MANUAL_VALUE_WIRE_OK);
    ManualValueView view=manual_value_view(old);
    CHECK(manual_value_wire_decode(view.raw,view.raw_bytes,SIZE_MAX,&old)==MANUAL_VALUE_WIRE_INVALID && !strcmp(view.raw,"+0009007199254740993"));
    ManualValuePacket absent=packet((ManualValueInput){0}); old_bytes=manual_value_view(old).owned;
    CHECK(manual_value_wire_decode(absent.data,absent.bytes,old_bytes-1,&old)==MANUAL_VALUE_WIRE_LIMIT && old);
    CHECK(manual_value_wire_decode(absent.data,absent.bytes,old_bytes,&old)==MANUAL_VALUE_WIRE_OK && !old);
    manual_value_packet_free(&absent); manual_value_packet_free(&out); manual_value_free(&old);
    out=packet(text("Stable",6)); prior=out;
    CHECK(manual_value_wire_capture(number(NUM_I8,0,0,"128"),SIZE_MAX,&out)==MANUAL_VALUE_WIRE_INVALID && out.data==prior.data && out.bytes==prior.bytes);
    size_t saved=out.bytes; out.bytes=SIZE_MAX-1;
    CHECK(manual_value_wire_capture((ManualValueInput){0},SIZE_MAX,&out)==MANUAL_VALUE_WIRE_LIMIT && out.bytes==SIZE_MAX-1); out.bytes=saved;
    manual_value_packet_free(&out); manual_value_packet_free(NULL);
    CHECK(manual_value_wire_capture((ManualValueInput){0},0,NULL)==MANUAL_VALUE_WIRE_INVALID && manual_value_wire_decode(NULL,0,0,NULL)==MANUAL_VALUE_WIRE_INVALID && manual_value_wire_measure((ManualValueInput){0},NULL)==MANUAL_VALUE_WIRE_INVALID);
}
static void max_bytes(void) {
    char *raw=malloc(DX_MANUAL_VALUE_TEXT_BYTES); CHECK(raw); memset(raw,'x',DX_MANUAL_VALUE_TEXT_BYTES);
    roundtrip(text(raw,DX_MANUAL_VALUE_TEXT_BYTES)); free(raw);
    raw=malloc(DX_MANUAL_VALUE_NUMBER_BYTES); CHECK(raw); memset(raw,'0',DX_MANUAL_VALUE_NUMBER_BYTES); raw[DX_MANUAL_VALUE_NUMBER_BYTES-1]='1';
    roundtrip((ManualValueInput){true,MANUAL_VALUE_NUMBER,{NUM_I8,0,0},raw,DX_MANUAL_VALUE_NUMBER_BYTES}); free(raw);
    ManualValuePacket valid=packet(text("0",1)); ManualValueWireInfo old={.bytes=1},info=old;
    CHECK(manual_value_wire_measure(text((const char *)1,DX_MANUAL_VALUE_TEXT_BYTES+1),&info)==MANUAL_VALUE_WIRE_LIMIT && !memcmp(&info,&old,sizeof(info))); manual_value_packet_free(&valid);
}
static void admitted_arena(void) {
    ManualValueInput input=number(NUM_DECIMAL,5,2,"+0012.3"); ManualValueWireInfo info;
    CHECK(manual_value_wire_measure(input,&info)==MANUAL_VALUE_WIRE_OK);
    unsigned char arena[64],before[64]; memset(arena,0xa5,sizeof(arena)); memcpy(before,arena,sizeof(arena)); size_t written=73;
    CHECK(manual_value_wire_encode(input,arena,info.bytes-1,&written)==MANUAL_VALUE_WIRE_LIMIT && written==73 && !memcmp(arena,before,sizeof(arena)));
    CHECK(manual_value_wire_encode(number(NUM_I8,0,0,"128"),arena,sizeof(arena),&written)==MANUAL_VALUE_WIRE_INVALID && written==73 && !memcmp(arena,before,sizeof(arena)));
    CHECK(manual_value_wire_encode(input,arena,info.bytes,&written)==MANUAL_VALUE_WIRE_OK && written==info.bytes && arena[info.bytes]==0xa5);
    ManualValuePacket exact=packet(input); CHECK(exact.bytes==written && !memcmp(exact.data,arena,written)); manual_value_packet_free(&exact);
    CHECK(manual_value_wire_encode(input,NULL,0,&written)==MANUAL_VALUE_WIRE_INVALID && manual_value_wire_encode(input,arena,sizeof(arena),NULL)==MANUAL_VALUE_WIRE_INVALID);
}
int main(void) {
    admitted_arena(); goldens(); numeric_types(); malformed(); budgets_and_aliases(); max_bytes();
    printf("PASS: %u current typed manual wire checks\n",checks); return 0;
}
