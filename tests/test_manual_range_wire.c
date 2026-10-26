#include "workbook/manual_range_wire.h"
#include "workbook/cells_tsv_scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL manual range:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static ManualValue *text(const char *s) {
    ManualValue *out=NULL; CHECK(manual_value_build((ManualValueInput){true,MANUAL_VALUE_TEXT,{0},s,strlen(s)},SIZE_MAX,&out)==MANUAL_VALUE_OK); return out;
}
static ManualValue *number(NumberKind kind,unsigned precision,unsigned scale,const char *s) {
    ManualValue *out=NULL; CHECK(manual_value_build((ManualValueInput){true,MANUAL_VALUE_NUMBER,{kind,(unsigned char)precision,(unsigned char)scale},s,strlen(s)},SIZE_MAX,&out)==MANUAL_VALUE_OK); return out;
}
static ManualRangePacket packet(ManualRangeView view) {
    ManualRangeInfo info; CHECK(manual_range_measure(view,&info)==MANUAL_RANGE_OK && !info.heap_scratch);
    ManualRangePacket out={0}; CHECK(manual_range_capture(view,info.bytes,&out)==MANUAL_RANGE_OK && out.bytes==info.bytes); return out;
}
static ManualRange *decode(ManualRangePacket packet) {
    ManualRangeInfo info; CHECK(manual_range_preflight(packet.data,packet.bytes,&info)==MANUAL_RANGE_OK && !info.heap_scratch);
    ManualRange *out=NULL; CHECK(manual_range_decode(packet.data,packet.bytes,info.owned,&out)==MANUAL_RANGE_OK && manual_range_owned(out)==info.owned); return out;
}
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) p[i]=(unsigned char)(value>>(8*i)); }
static void reject(const void *data,size_t bytes,ManualRangeStatus expected) {
    ManualRangeInfo info={11,13,17,19,23,29},before=info;
    CHECK(manual_range_preflight(data,bytes,&info)==expected && !memcmp(&info,&before,sizeof(info)));
    const ManualValue *one[]={NULL}; ManualRangePacket old_packet=packet((ManualRangeView){1,1,1,one}); ManualRange *out=decode(old_packet),*prior=out;
    CHECK(manual_range_decode(data,bytes,SIZE_MAX,&out)==expected && out==prior && manual_range_view(out).count==1);
    manual_range_free(&out); manual_range_packet_free(&old_packet);
}
static void presence_roundtrip(void) {
    ManualValue *null=NULL; CHECK(manual_value_build((ManualValueInput){.present=true},SIZE_MAX,&null)==MANUAL_VALUE_OK);
    ManualValue *empty=text(""),*literal=text("NULL"),*integer=number(NUM_U128,0,0,"340282366920938463463374607431768211455"),*decimal=number(NUM_DECIMAL,5,2,"+0012.3");
    const ManualValue *values[]={NULL,null,empty,literal,integer,decimal}; ManualRangeView view={2,3,6,values}; ManualRangePacket encoded=packet(view);
    const unsigned char header[32]={'D','X','M','R',1,0,0,0,2,0,0,0,3,0,0,0,6};
    CHECK(!memcmp(encoded.data,header,24));
    ManualRange *out=decode(encoded); ManualRangeView actual=manual_range_view(out); CHECK(actual.rows==2 && actual.columns==3 && actual.count==6);
    for (size_t i=0;i<6;i++) CHECK(manual_value_equal(values[i],actual.values[i]));
    const ManualValue *result=literal; CHECK(manual_range_at(out,0,0,&result) && !result);
    result=literal; CHECK(!manual_range_at(out,2,0,&result) && result==literal && !manual_range_at(out,0,3,&result) && result==literal);
    CHECK(manual_range_at(out,0,1,&result) && manual_value_view(result).present && manual_value_view(result).kind==MANUAL_VALUE_SQL_NULL);
    CHECK(manual_range_at(out,0,2,&result) && manual_value_view(result).present && manual_value_view(result).kind==MANUAL_VALUE_TEXT && !manual_value_view(result).raw_bytes);
    ManualRangePacket again=packet(actual); CHECK(again.bytes==encoded.bytes && !memcmp(again.data,encoded.data,encoded.bytes));
    manual_range_packet_free(&encoded); manual_value_free(&null); manual_value_free(&empty); manual_value_free(&literal); manual_value_free(&integer); manual_value_free(&decimal);
    CHECK(!strcmp(manual_value_view(actual.values[5]).raw,"+0012.3") && !strcmp(manual_value_view(actual.values[5]).canonical,"12.30"));
    manual_range_free(&out); manual_range_free(&out); manual_range_free(NULL); manual_range_packet_free(&again); manual_range_packet_free(NULL);
}
static void malformed(void) {
    ManualValue *a=text("A"),*b=number(NUM_I8,0,0,"127"); const ManualValue *values[]={a,NULL,b}; ManualRangePacket good=packet((ManualRangeView){1,3,3,values});
    for (size_t n=0;n<good.bytes;n++) reject(good.data,n,MANUAL_RANGE_INVALID);
    unsigned char *bytes=malloc(good.bytes+29); CHECK(bytes); memcpy(bytes,good.data,good.bytes); bytes[good.bytes]=0;
    reject(bytes,good.bytes+1,MANUAL_RANGE_INVALID);
    for (size_t i=0;i<4;i++) { bytes[i]^=1; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[i]^=1; }
    bytes[4]=0; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[4]=2; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[4]=1;
    const size_t reserved[]={5,6,7,20,21,22,23};
    for (size_t i=0;i<sizeof(reserved)/sizeof(*reserved);i++) { bytes[reserved[i]]=1; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[reserved[i]]=0; }
    bytes[8]=0; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[8]=1;
    put(bytes+12,16385,4); reject(bytes,good.bytes,MANUAL_RANGE_LIMIT); put(bytes+12,3,4);
    bytes[16]=2; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[16]=3;
    bytes[24]++; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[24]--;
    put(bytes+32,0,4); reject(bytes,good.bytes,MANUAL_RANGE_INVALID); put(bytes+32,25,4);
    put(bytes+32,UINT32_MAX,4); reject(bytes,good.bytes,MANUAL_RANGE_INVALID); put(bytes+32,25,4);
    bytes[40]=2; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[40]=1; /* Nested DXMV version. */
    bytes[good.bytes-1]='8'; reject(bytes,good.bytes,MANUAL_RANGE_INVALID); bytes[good.bytes-1]='7'; /* Last cell128 overflows I8. */
    memcpy(bytes+good.bytes,bytes+32,29); put(bytes+24,good.bytes+29-32,8); reject(bytes,good.bytes+29,MANUAL_RANGE_INVALID);
    free(bytes); manual_range_packet_free(&good); manual_value_free(&a); manual_value_free(&b);
    reject(NULL,0,MANUAL_RANGE_INVALID); reject((const void *)1,DX_CLIPBOARD_BYTES+1,MANUAL_RANGE_LIMIT);
}
static void shape_and_maximum(void) {
    const ManualValue *one[]={NULL}; ManualRangeInfo info={1,2,3,4,5,6},old=info;
    CHECK(manual_range_measure((ManualRangeView){0,1,0,one},&info)==MANUAL_RANGE_INVALID && !memcmp(&info,&old,sizeof(info)));
    CHECK(manual_range_measure((ManualRangeView){1,0,0,one},&info)==MANUAL_RANGE_INVALID);
    CHECK(manual_range_measure((ManualRangeView){1,1,0,one},&info)==MANUAL_RANGE_INVALID);
    CHECK(manual_range_measure((ManualRangeView){1,1,1,NULL},&info)==MANUAL_RANGE_INVALID);
    CHECK(manual_range_measure((ManualRangeView){1,16385,16385,one},&info)==MANUAL_RANGE_LIMIT);
    CHECK(manual_range_measure((ManualRangeView){SIZE_MAX,1,SIZE_MAX,one},&info)==MANUAL_RANGE_LIMIT);
    const ManualValue **values=calloc(DX_CLIPBOARD_CELLS,sizeof(*values)); CHECK(values);
    ManualRangeView full={100,200,DX_CLIPBOARD_CELLS,values}; ManualRangePacket encoded=packet(full); ManualRange *out=decode(encoded);
    CHECK(encoded.bytes==32u+DX_CLIPBOARD_CELLS*28u && manual_range_view(out).count==DX_CLIPBOARD_CELLS);
    for (size_t i=0;i<DX_CLIPBOARD_CELLS;i++) CHECK(!manual_range_view(out).values[i]);
    manual_range_free(&out); manual_range_packet_free(&encoded); free(values);
    CHECK(manual_range_measure((ManualRangeView){1,1,1,one},NULL)==MANUAL_RANGE_INVALID && manual_range_preflight(NULL,0,NULL)==MANUAL_RANGE_INVALID);
    CHECK(!manual_range_owned(NULL) && !manual_range_view(NULL).count && !manual_range_at(NULL,0,0,NULL));
}
static void budgets_and_lifetimes(void) {
    ManualValue *a=text("Owned"),*b=number(NUM_DECIMAL,5,2,"+0012.3"); const ManualValue *values[]={a,b}; ManualRangeView view={1,2,2,values};
    ManualRangeInfo info; CHECK(manual_range_measure(view,&info)==MANUAL_RANGE_OK);
    const ManualValue *empty[]={NULL}; ManualRangePacket out=packet((ManualRangeView){1,1,1,empty}),prior=out;
    CHECK(manual_range_capture(view,out.bytes+info.bytes-1,&out)==MANUAL_RANGE_LIMIT && out.data==prior.data && out.bytes==prior.bytes);
    CHECK(manual_range_capture(view,out.bytes+info.bytes,&out)==MANUAL_RANGE_OK);
    ManualRange *old=decode(out),*before=old; size_t resident=manual_range_owned(old);
    CHECK(manual_range_decode(out.data,out.bytes,resident+info.owned-1,&old)==MANUAL_RANGE_LIMIT && old==before);
    CHECK(manual_range_decode(out.data,out.bytes,resident+info.owned,&old)==MANUAL_RANGE_OK && manual_range_owned(old)==info.owned);
    ManualRangeView copied=manual_range_view(old); prior=out;
    CHECK(manual_range_capture(copied,2*out.bytes-1,&out)==MANUAL_RANGE_LIMIT && out.data==prior.data);
    CHECK(manual_range_capture(copied,2*out.bytes,&out)==MANUAL_RANGE_OK);
    before=old; CHECK(manual_range_decode(out.data,out.bytes,2*info.owned-1,&old)==MANUAL_RANGE_LIMIT && old==before);
    CHECK(manual_range_decode(out.data,out.bytes,2*info.owned,&old)==MANUAL_RANGE_OK);
    manual_value_free(&a); manual_value_free(&b); manual_range_packet_free(&out);
    copied=manual_range_view(old); CHECK(!strcmp(manual_value_view(copied.values[0]).raw,"Owned") && !strcmp(manual_value_view(copied.values[1]).raw,"+0012.3") && !strcmp(manual_value_view(copied.values[1]).canonical,"12.30"));
    manual_range_free(&old);
    CHECK(manual_range_decode(NULL,0,0,NULL)==MANUAL_RANGE_INVALID && manual_range_capture(view,0,NULL)==MANUAL_RANGE_INVALID);
}
typedef struct { ManualValue *values[8]; size_t width,calls; } Scan;
static bool visit(void *context,uint64_t row,size_t column,const char *raw,size_t bytes,char **error) {
    (void)error; Scan *scan=context; size_t index=(size_t)row*scan->width+column;
    if (index>=8) return false;
    scan->calls++; return manual_value_build((ManualValueInput){true,MANUAL_VALUE_TEXT,{0},raw,bytes},SIZE_MAX,scan->values+index)==MANUAL_VALUE_OK;
}
static void scan_free(Scan *scan) { for (size_t i=0;i<8;i++) manual_value_free(scan->values+i); }
static void public_projection(void) {
    ManualValue *null=NULL; CHECK(manual_value_build((ManualValueInput){.present=true},SIZE_MAX,&null)==MANUAL_VALUE_OK);
    ManualValue *empty=text(""),*literal=text("NULL"),*integer=number(NUM_U64,0,0,"18446744073709551615"),*decimal=number(NUM_DECIMAL,5,2,"+0012.3");
    const ManualValue *values[]={NULL,null,empty,literal,integer,decimal}; ManualRangeView view={2,3,6,values};
    const char *expected="\t\t\"\"\nNULL\t18446744073709551615\t12.30";
    size_t bytes=0; CHECK(manual_range_tsv_measure(view,&bytes)==MANUAL_RANGE_OK && bytes==strlen(expected)+1);
    char *out=dx_strdup("Old"),*prior=out;
    CHECK(manual_range_tsv(view,4+bytes-1,&out)==MANUAL_RANGE_LIMIT && out==prior && !strcmp(out,"Old"));
    CHECK(manual_range_tsv(view,4+bytes,&out)==MANUAL_RANGE_OK && !strcmp(out,expected) && !strcmp(manual_value_view(decimal).raw,"+0012.3"));
    CellClipboard legacy={0}; CHECK(clipboard_init(&legacy,2,3,NULL));
    for (size_t i=0;i<6;i++) { ManualValueView v=manual_value_view(values[i]); CHECK(clipboard_set(&legacy,i/3,i%3,!v.present || v.kind==MANUAL_VALUE_SQL_NULL ? NULL : v.canonical,NULL)); }
    char *old=clipboard_tsv(&legacy,NULL); CHECK(old && !strcmp(old,out)); free(old); clipboard_free(&legacy);
    Scan scan={.width=3}; size_t rows=0,columns=0;
    CHECK(cells_tsv_scan(0,0,out,visit,&scan,&rows,&columns,NULL) && rows==2 && columns==3 && scan.calls==6);
    CHECK(manual_value_view(scan.values[0]).present && manual_value_view(scan.values[0]).kind==MANUAL_VALUE_TEXT && !manual_value_view(scan.values[0]).raw_bytes);
    CHECK(manual_value_view(scan.values[4]).kind==MANUAL_VALUE_TEXT && !strcmp(manual_value_view(scan.values[4]).raw,"18446744073709551615"));
    scan_free(&scan); free(out);
    ManualValue *quoted=text("İstanbul 🦆\tline\n\"quoted\"\r"); const ManualValue *one[]={quoted}; view=(ManualRangeView){1,1,1,one};
    out=NULL; CHECK(manual_range_tsv(view,SIZE_MAX,&out)==MANUAL_RANGE_OK); scan=(Scan){.width=1};
    CHECK(cells_tsv_scan(0,0,out,visit,&scan,&rows,&columns,NULL) && scan.calls==1 && manual_value_equal(quoted,scan.values[0])); scan_free(&scan); free(out); manual_value_free(&quoted);
    manual_value_free(&null); manual_value_free(&empty); manual_value_free(&literal); manual_value_free(&integer); manual_value_free(&decimal);
}
static void external_shape_contract(void) {
    Scan scan={.width=2}; size_t rows=91,columns=93;
    CHECK(cells_tsv_scan(0,0,"A\tB\nC",visit,&scan,&rows,&columns,NULL) && rows==2 && columns==2 && scan.calls==3 && !scan.values[3]);
    ManualRangeView view={2,2,4,(const ManualValue *const *)scan.values}; ManualRangePacket encoded=packet(view); ManualRange *out=decode(encoded);
    const ManualValue *hole=scan.values[0]; CHECK(manual_range_at(out,1,1,&hole) && !hole);
    char *public=NULL; CHECK(manual_range_tsv(manual_range_view(out),SIZE_MAX,&public)==MANUAL_RANGE_OK && !strcmp(public,"A\tB\nC\t"));
    Scan again={.width=2}; CHECK(cells_tsv_scan(0,0,public,visit,&again,&rows,&columns,NULL) && again.calls==4 && manual_value_view(again.values[3]).present);
    scan_free(&again); free(public); manual_range_free(&out); manual_range_packet_free(&encoded);
    const ManualValue *trailing[]={scan.values[0],NULL}; public=NULL;
    CHECK(manual_range_tsv((ManualRangeView){2,1,2,trailing},SIZE_MAX,&public)==MANUAL_RANGE_OK && !strcmp(public,"A\n")); again=(Scan){.width=1};
    CHECK(cells_tsv_scan(0,0,public,visit,&again,&rows,&columns,NULL) && rows==1 && columns==1); scan_free(&again); free(public); scan_free(&scan);
    char *large=malloc(65538); CHECK(large); memset(large,'x',65537); large[65537]=0; ManualValue *v=text(large); free(large);
    const ManualValue *one[]={v}; encoded=packet((ManualRangeView){1,1,1,one}); public=NULL;
    CHECK(manual_range_tsv((ManualRangeView){1,1,1,one},SIZE_MAX,&public)==MANUAL_RANGE_OK); again=(Scan){.width=1}; rows=91; columns=93;
    CHECK(!cells_tsv_scan(0,0,public,visit,&again,&rows,&columns,NULL) && rows==91 && columns==93 && !again.calls);
    free(public); manual_range_packet_free(&encoded); manual_value_free(&v);
}
static void byte_caps_and_arenas(void) {
    char *raw=malloc(DX_MANUAL_VALUE_TEXT_BYTES+1); CHECK(raw); memset(raw,'"',DX_MANUAL_VALUE_TEXT_BYTES); raw[DX_MANUAL_VALUE_TEXT_BYTES]=0;
    ManualValue *v=text(raw); const ManualValue *one[]={v},*two[]={v,v}; ManualRangeInfo info={1,2,3,4,5,6},old=info;
    CHECK(manual_range_measure((ManualRangeView){1,2,2,two},&info)==MANUAL_RANGE_LIMIT && !memcmp(&info,&old,sizeof(info)));
    ManualRangePacket good=packet((ManualRangeView){1,1,1,one}); manual_range_packet_free(&good);
    char *out=dx_strdup("Prior"),*prior=out; size_t owned=27;
    CHECK(manual_range_tsv_measure((ManualRangeView){1,1,1,one},&owned)==MANUAL_RANGE_LIMIT && owned==27);
    CHECK(manual_range_tsv((ManualRangeView){1,1,1,one},SIZE_MAX,&out)==MANUAL_RANGE_LIMIT && out==prior && !strcmp(out,"Prior")); manual_value_free(&v);
    raw[DX_MANUAL_VALUE_TEXT_BYTES-1]=0; v=text(raw); one[0]=v;
    CHECK(manual_range_tsv_measure((ManualRangeView){1,1,1,one},&owned)==MANUAL_RANGE_OK && owned==DX_CLIPBOARD_BYTES+1u);
    CHECK(manual_range_tsv((ManualRangeView){1,1,1,one},6+owned,&out)==MANUAL_RANGE_OK && strlen(out)==DX_CLIPBOARD_BYTES);
    free(out); free(raw); manual_value_free(&v);
    ManualValue *word=text("x"); const ManualValue *words[]={word}; ManualRangeView view={1,1,1,words};
    char arena[8]; memset(arena,0x5a,sizeof(arena)); char before[8]; memcpy(before,arena,sizeof(arena)); size_t written=71;
    ClipboardTextView invalid={0}; CHECK(!clipboard_tsv_encode_view(invalid,arena,sizeof(arena),&written,NULL) && written==71 && !memcmp(arena,before,sizeof(arena)));
    CHECK(manual_range_tsv_measure(view,NULL)==MANUAL_RANGE_INVALID && manual_range_tsv(view,0,NULL)==MANUAL_RANGE_INVALID);
    manual_value_free(&word);
}
static const char *borrowed_text(const void *context,size_t index) { return ((const char *const *)context)[index]; }
static void borrowed_alias_guards(void) {
    union { size_t aligned; unsigned char bytes[96]; } storage;
    unsigned char before[96]; const char *values[2]; char *error=NULL; size_t written=719;
    memset(storage.bytes,0xa5,sizeof(storage.bytes)); strcpy((char *)storage.bytes+16,"Borrowed");
    memcpy(before,storage.bytes,sizeof(before)); values[0]=(const char *)storage.bytes+16;
    CHECK(!clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes,sizeof(storage.bytes),&written,&error) && error && written==719 && !memcmp(before,storage.bytes,sizeof(before))); free(error); error=NULL;
    values[0]="Safe first field"; values[1]=(const char *)storage.bytes+16;
    CHECK(!clipboard_tsv_encode_view((ClipboardTextView){1,2,borrowed_text,values},(char *)storage.bytes,sizeof(storage.bytes),&written,NULL) && written==719 && !memcmp(before,storage.bytes,sizeof(before)));
    /* A source NUL alone overlaps the output; the nonempty byte is outside it. */
    memset(storage.bytes,0xa5,sizeof(storage.bytes)); strcpy((char *)storage.bytes,"X"); values[0]=(const char *)storage.bytes; memcpy(before,storage.bytes,sizeof(before));
    CHECK(!clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes+1,8,&written,NULL) && written==719 && !memcmp(before,storage.bytes,sizeof(before)));
    CHECK(clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes+2,8,&written,NULL) && written==1 && !strcmp((char *)storage.bytes,"X") && !strcmp((char *)storage.bytes+2,"X") && storage.bytes[4]==0xa5);
    memset(storage.bytes,0xa5,sizeof(storage.bytes)); values[0]="Independent"; memcpy(before,storage.bytes,sizeof(before));
    CHECK(!clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes,sizeof(storage.bytes),(size_t *)storage.bytes,NULL) && !memcmp(before,storage.bytes,sizeof(before)));
    union { size_t aligned; unsigned char bytes[32]; } text; memset(text.bytes,0xa5,sizeof(text.bytes)); strcpy((char *)text.bytes,"Text"); unsigned char old_text[32]; memcpy(old_text,text.bytes,sizeof(old_text)); values[0]=(const char *)text.bytes;
    CHECK(!clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes,sizeof(storage.bytes),(size_t *)text.bytes,NULL) && !memcmp(old_text,text.bytes,sizeof(old_text)) && !memcmp(before,storage.bytes,sizeof(before)));
    values[0]="OK"; written=719;
    CHECK(clipboard_tsv_encode_view((ClipboardTextView){1,1,borrowed_text,values},(char *)storage.bytes,sizeof(storage.bytes),&written,NULL) && written==2 && !strcmp((char *)storage.bytes,"OK") && storage.bytes[3]==0xa5);
}
int main(void) {
    borrowed_alias_guards(); presence_roundtrip(); malformed(); shape_and_maximum(); budgets_and_lifetimes(); public_projection(); external_shape_contract(); byte_caps_and_arenas();
    printf("PASS: %u typed manual range and clipboard checks\n",checks); return 0;
}
