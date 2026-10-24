#include "workbook/format_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL format delta codec:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const size_t budget=2u*DX_FORMAT_CURRENT_BYTES+DX_FORMAT_VALIDATION_BYTES;
static void put(unsigned char *to,uint64_t value,unsigned bytes) { for (unsigned i=0;i<bytes;i++) { to[i]=(unsigned char)(value&255u); value>>=8; } }
static FormatStore view(const FormatSnapshot *s) { return (FormatStore){.items=s->items,.count=s->count,.scopes=s->scopes,.scope_count=s->scope_count,.source_aware=s->source_aware}; }
static void rejected(const void *wire,size_t bytes,size_t reserved,FormatDelta *out,FormatDeltaStatus expected) {
    unsigned char before[sizeof(*out)]; memcpy(before,out,sizeof(before));
    CHECK(format_delta_decode(wire,bytes,reserved,out)==expected && !memcmp(before,out,sizeof(before)));
}
static unsigned char *encode(uint64_t sheet,const FormatStore *a,const FormatStore *b,size_t *bytes) {
    CHECK(format_delta_encoded_size(sheet,format_delta_store_view(a),format_delta_store_view(b),bytes)==FORMAT_DELTA_OK);
    unsigned char *wire=dx_alloc(*bytes+1); memset(wire,0xa5,*bytes+1); size_t written=0;
    CHECK(format_delta_encode(sheet,format_delta_store_view(a),format_delta_store_view(b),wire,*bytes,&written)==FORMAT_DELTA_OK && written==*bytes && wire[*bytes]==0xa5);
    return wire;
}
static void test_oracle(void) {
    static const unsigned char expected[]={
        'D','X','F','D',1,0,0,0, 8,7,6,5,4,3,2,1, 8,0,0,0, 42,0,0,0,
        0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,
        1,1,0,0,13,0,2,0,0,0,0,0,0,0};
    FormatStore before={0},after={0}; CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    CHECK(format_apply(&after,(FormatRange){0},FORMAT_BOLD,value,NULL)); after.items[0].value.specified=255;
    size_t bytes=0; unsigned char *wire=encode(UINT64_C(0x0102030405060708),&before,&after,&bytes);
    CHECK(bytes==sizeof(expected) && !memcmp(wire,expected,sizeof(expected)));
    FormatDelta out={0}; CHECK(format_delta_decode(wire,bytes,budget,&out)==FORMAT_DELTA_OK && out.sheet==UINT64_C(0x0102030405060708));
    CHECK(!out.before.count && out.after.count==1 && out.after.items[0].value.flags==FORMAT_BOLD && !out.after.items[0].value.specified && out.after.items!=after.items);
    for (size_t n=0;n<bytes;n++) rejected(wire,n,budget,&out,FORMAT_DELTA_INVALID);
    rejected(wire,bytes+1,budget,&out,FORMAT_DELTA_INVALID);
    rejected(wire,bytes,0,&out,FORMAT_DELTA_LIMIT);
    rejected(NULL,bytes,budget,&out,FORMAT_DELTA_INVALID);
    CHECK(format_delta_decode(wire,bytes,budget,NULL)==FORMAT_DELTA_INVALID);
    unsigned char *bad=dx_alloc(bytes+1); memcpy(bad,wire,bytes+1);
    for (unsigned i=0;i<4;i++) { bad[i]^=1; rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); bad[i]^=1; }
    bad[4]=2; rejected(bad,bytes,budget,&out,FORMAT_DELTA_UNSUPPORTED); rejected(bad,6,budget,&out,FORMAT_DELTA_UNSUPPORTED); bad[4]=1;
    bad[5]=1; rejected(bad,bytes,budget,&out,FORMAT_DELTA_UNSUPPORTED); bad[5]=0;
    bad[6]=1; rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); bad[6]=0;
    memset(bad+8,0,8); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad+8,wire+8,8);
    bad[16]=7; rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); bad[16]=8;
    const size_t snapshot=32,patch=40;
    const struct { size_t at; unsigned char value; } mutations[]={
        {snapshot,2},{snapshot+1,1},{snapshot+3,3},{snapshot+6,1},
        {patch+5,1},{patch+17,64},{patch+20,0},{patch+21,8},{patch+22,4},
        {patch+23,4},{patch+24,0},{patch+25,3},{patch+26,5},{patch+27,4},
        {patch+28,34},{patch+29,1},{patch+31,128},{patch+33,128}};
    for (size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++) {
        unsigned char old=bad[mutations[i].at]; bad[mutations[i].at]=mutations[i].value;
        rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); bad[mutations[i].at]=old;
    }
    for (size_t n=40;n<bytes;n++) {
        put(bad+20,n-32,4); rejected(bad,n,budget,&out,FORMAT_DELTA_INVALID);
    }
    memcpy(bad,wire,bytes); put(bad+20,8,4); memset(bad+32,0,8);
    rejected(bad,40,budget,&out,FORMAT_DELTA_INVALID); /* Empty -> empty is not an event. */
    unsigned char arena[128],copy[128]; memset(arena,0xa5,sizeof(arena)); memcpy(copy,arena,sizeof(copy)); size_t written=123,measured=456;
    CHECK(format_delta_encode(1,format_delta_store_view(&before),format_delta_store_view(&before),arena,sizeof(arena),&written)==FORMAT_DELTA_NOOP && written==123 && !memcmp(arena,copy,sizeof(arena)));
    CHECK(format_delta_encoded_size(1,format_delta_store_view(&before),format_delta_store_view(&before),&measured)==FORMAT_DELTA_NOOP && measured==456);
    CHECK(format_delta_encode(1,format_delta_store_view(&before),format_delta_store_view(&after),arena,bytes-1,&written)==FORMAT_DELTA_LIMIT && written==123 && !memcmp(arena,copy,sizeof(arena)));
    CHECK(format_delta_encode(0,format_delta_store_view(&before),format_delta_store_view(&after),arena,sizeof(arena),&written)==FORMAT_DELTA_INVALID && written==123 && !memcmp(arena,copy,sizeof(arena)));
    CHECK(format_delta_encode(1,format_delta_store_view(&before),format_delta_store_view(&after),NULL,bytes,&written)==FORMAT_DELTA_INVALID && written==123);
    CHECK(format_delta_encode(1,format_delta_store_view(&before),format_delta_store_view(&after),arena,sizeof(arena),NULL)==FORMAT_DELTA_INVALID && !memcmp(arena,copy,sizeof(arena)));
    format_delta_free(&out); format_free(&after); free(wire); free(bad);
    after=(FormatStore){.source_aware=true}; wire=encode(1,&before,&after,&bytes);
    CHECK(format_delta_decode(wire,bytes,0,&out)==FORMAT_DELTA_OK && !out.before.source_aware && out.after.source_aware && !out.after.count && !out.after.scope_count);
    format_delta_free(&out); free(wire);
}
static void test_scoped_membership(void) {
    FormatStore before={0},after={0}; CellFormat value=format_default(); value.color=1;
    CHECK(format_apply(&before,(FormatRange){0,DX_SHEET_ROWS-1,0,16383},FORMAT_COLOR,value,NULL)); format_copy(&after,&before);
    const unsigned char query[]={'q',0,'s'},raw[]={'r',0,'w'};
    FormatScopeView result={0,query,sizeof(query)},records={0,raw,sizeof(raw)};
    value.flags=FORMAT_UNDERLINE; CHECK(format_apply_scoped(&after,(FormatRange){5,7,2,4},FORMAT_UNDERLINE,value,&result,1,false,NULL));
    uint64_t ids[]={1,UINT64_C(9007199254740993),UINT64_MAX}; size_t fields[]={0,16383}; value.flags=FORMAT_BOLD;
    CHECK(format_apply_records(&after,&records,ids,3,fields,2,FORMAT_BOLD,value,NULL));
    FormatScopeView mixed[]={result,{UINT64_MAX,"other",5}}; value.color=2;
    CHECK(format_apply_scoped(&after,(FormatRange){20,21,1,2},FORMAT_COLOR,value,mixed,2,true,NULL));
    size_t bytes=0; unsigned char *wire=encode(UINT64_MAX,&before,&after,&bytes); FormatDelta out={0};
    CHECK(format_delta_decode(wire,bytes,budget,&out)==FORMAT_DELTA_OK && out.sheet==UINT64_MAX && out.after.source_aware && out.after.count==4 && out.after.scope_count==3);
    size_t after_at=24+8+34; /* Before has one spatial patch. */
    size_t scope_data[3],cursor=after_at+8;
    for (size_t i=0;i<3;i++) { scope_data[i]=cursor+12; cursor+=12+after.scopes[i].bytes; }
    size_t result_patch=cursor+34,record_patch=result_patch+36,guard_patch=record_patch+64;
    unsigned char *bad=dx_alloc(bytes); memcpy(bad,wire,bytes);
    memcpy(bad+scope_data[1],bad+scope_data[0],3); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    put(bad+result_patch+34,3,2); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    put(bad+record_patch+34+2+8,1,8); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    put(bad+record_patch+34+2,0,8); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    put(bad+record_patch+34+2+24+2,0,2); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    put(bad+guard_patch+34+2,0,2); rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    bad[after_at]=0; rejected(bad,bytes,budget,&out,FORMAT_DELTA_INVALID); memcpy(bad,wire,bytes);
    for (size_t n=after_at+8;n<bytes;n++) { put(bad+20,n-after_at,4); rejected(bad,n,budget,&out,FORMAT_DELTA_INVALID); }
    format_free(&before); format_free(&after); free(wire); free(bad);
    FormatStore restored=view(&out.after); CHECK(format_valid(&restored,NULL));
    size_t qt=format_scope_token(&restored,&result),rt=format_scope_token(&restored,&records);
    CellFormat resolved=format_resolve_record_token(&restored,6,3,qt,rt,UINT64_MAX,16383);
    CHECK(qt && rt && qt!=rt && resolved.flags==(FORMAT_BOLD|FORMAT_UNDERLINE) && resolved.color==1);
    CHECK(out.after.scopes[2].owner==UINT64_MAX && out.after.scopes[0].bytes==3 && !memcmp(out.after.scopes[0].definition,query,3));
    CHECK(out.after.items[2].record_ids[1]==UINT64_C(9007199254740993) && out.after.items[2].record_ids[2]==UINT64_MAX && out.after.items[2].fields[1]==16383);
    format_delta_free(&out);
}
static void test_memory_and_membership_limits(void) {
    FormatStore before={0},after={.source_aware=true}; const size_t count=34;
    after.count=after.scope_count=count; after.items=dx_alloc(count*sizeof(*after.items)); after.scopes=dx_alloc(count*sizeof(*after.scopes));
    CellFormat value=format_default(); value.flags=FORMAT_BOLD;
    for (size_t i=0;i<count;i++) {
        after.scopes[i]=(FormatScope){i,dx_strdup("x"),1};
        after.items[i]=(FormatPatch){.range={i,i,0,0},.mask=FORMAT_BOLD,.value=value,.selector=FORMAT_RESULT,.guard_count=1,.guard_ids=dx_alloc(sizeof(size_t))};
        after.items[i].guard_ids[0]=i;
    }
    CHECK(format_valid(&after,NULL)); size_t bytes=0; unsigned char *wire=encode(1,&before,&after,&bytes); FormatDelta out={0};
    size_t owned=format_owned_bytes(&after,false),scratch=count*sizeof(const FormatScope *);
    rejected(wire,bytes,owned,&out,FORMAT_DELTA_LIMIT); rejected(wire,bytes,owned+scratch-1,&out,FORMAT_DELTA_LIMIT);
    CHECK(format_delta_decode(wire,bytes,owned+scratch,&out)==FORMAT_DELTA_OK && out.after.scope_count==count && out.after.items[count-1].guard_ids[0]==count-1);
    format_delta_free(&out); format_free(&after); free(wire);
    uint64_t *ids=dx_alloc(DX_FORMAT_RECORDS*sizeof(*ids)); size_t *fields=dx_alloc(DX_FORMAT_FIELDS*sizeof(*fields));
    for (size_t i=0;i<DX_FORMAT_RECORDS;i++) ids[i]=i+1;
    for (size_t i=0;i<DX_FORMAT_FIELDS;i++) fields[i]=i;
    FormatScopeView raw={0,"raw",3};
    CHECK(format_apply_records(&after,&raw,ids,DX_FORMAT_RECORDS,fields,DX_FORMAT_FIELDS,FORMAT_BOLD,value,NULL));
    wire=encode(1,&before,&after,&bytes);
    CHECK(format_delta_decode(wire,bytes,budget,&out)==FORMAT_DELTA_OK && out.after.items[0].record_count==DX_FORMAT_RECORDS && out.after.items[0].field_count==DX_FORMAT_FIELDS);
    CHECK(out.after.items[0].record_ids[DX_FORMAT_RECORDS-1]==DX_FORMAT_RECORDS && out.after.items[0].fields[DX_FORMAT_FIELDS-1]==DX_FORMAT_FIELDS-1);
    rejected(wire,bytes-1,budget,&out,FORMAT_DELTA_INVALID);
    free(ids); free(fields); free(wire); format_free(&after); format_delta_free(&out);
}
int main(void) {
    test_oracle(); test_scoped_membership(); test_memory_and_membership_limits();
    printf("PASS: %u closed owned scoped-format payload codec checks\n",checks); return 0;
}
