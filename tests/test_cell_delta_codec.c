#include "workbook/cell_delta_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell delta codec:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void reject(const void *data,size_t bytes,CellDeltaCodecStatus status) {
    CellDelta out; memset(&out,0xa5,sizeof(out)); CellDelta before=out;
    CHECK(cell_delta_decode(data,bytes,&out)==status && !memcmp(&out,&before,sizeof(out)));
}
static void encode_rejected(CellDelta in,size_t capacity,CellDeltaCodecStatus status) {
    unsigned char arena[128],before[128]; memset(arena,0xa5,sizeof(arena)); memcpy(before,arena,sizeof(arena));
    size_t written=123; CHECK(cell_delta_encode(in,arena,capacity,&written)==status && written==123 && !memcmp(arena,before,sizeof(arena)));
}
int main(void) {
    /* Authored canonical byte oracle: independent of host endian/struct size. */
    static const unsigned char expected[]={
        'D','X','C','D',1,0,0,0,
        8,7,6,5,4,3,2,1,
        5,4,3,2,1,0,0,0,
        0x34,0x12, 1,0,0,0, 4,0,0,0, 'A','N','U','L','L'};
    CellDelta in={.sheet=UINT64_C(0x0102030405060708),.row=UINT64_C(0x0102030405),.column=0x1234,
        .before="A",.after="NULL",.before_bytes=1,.after_bytes=4},out={0};
    unsigned char wire[128]; memset(wire,0xa5,sizeof(wire)); size_t bytes=0,written=0;
    CHECK(cell_delta_encoded_size(in,&bytes)==CELL_DELTA_CODEC_OK && bytes==sizeof(expected));
    CHECK(cell_delta_encode(in,wire,sizeof(wire),&written)==CELL_DELTA_CODEC_OK && written==sizeof(expected) && !memcmp(wire,expected,sizeof(expected)));
    CHECK(wire[written]==0xa5 && wire[sizeof(wire)-1]==0xa5);
    CHECK(cell_delta_decode(wire,written,&out)==CELL_DELTA_CODEC_OK && out.sheet==in.sheet && out.row==in.row && out.column==in.column);
    CHECK(out.before==(const char *)wire+DX_CELL_DELTA_HEADER_BYTES && out.after==out.before+1 && out.before_bytes==1 && out.after_bytes==4 && !memcmp(out.after,"NULL",4));
    for (size_t n=0;n<written;n++) reject(wire,n,CELL_DELTA_CODEC_INVALID);
    reject(wire,written+1,CELL_DELTA_CODEC_INVALID);
    reject(NULL,written,CELL_DELTA_CODEC_INVALID);
    CHECK(cell_delta_decode(wire,written,NULL)==CELL_DELTA_CODEC_INVALID);
    unsigned char corrupt[128]; memcpy(corrupt,wire,sizeof(corrupt));
    for (unsigned i=0;i<4;i++) { corrupt[i]^=1; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[i]^=1; }
    corrupt[4]=2; reject(corrupt,written,CELL_DELTA_CODEC_UNSUPPORTED); reject(corrupt,6,CELL_DELTA_CODEC_UNSUPPORTED); corrupt[4]=1;
    corrupt[5]=1; reject(corrupt,written,CELL_DELTA_CODEC_UNSUPPORTED); corrupt[5]=0;
    corrupt[6]=1; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[6]=0;
    corrupt[7]=128; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[7]=0;
    memset(corrupt+8,0,8); reject(corrupt,written,CELL_DELTA_CODEC_INVALID); memcpy(corrupt+8,wire+8,8);
    corrupt[21]=1; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[21]=0;
    corrupt[25]=0x40; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[25]=wire[25];
    corrupt[29]=0xff; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[29]=0;
    corrupt[33]=0xff; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[33]=0;
    corrupt[30]=3; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[30]=4;
    corrupt[DX_CELL_DELTA_HEADER_BYTES]=0; reject(corrupt,written,CELL_DELTA_CODEC_INVALID); corrupt[DX_CELL_DELTA_HEADER_BYTES]='A';
    corrupt[DX_CELL_DELTA_HEADER_BYTES+2]=0; reject(corrupt,written,CELL_DELTA_CODEC_INVALID);
    encode_rejected(in,written-1,CELL_DELTA_CODEC_LIMIT);
    CellDelta bad=in; bad.before=bad.after; bad.before_bytes=bad.after_bytes;
    encode_rejected(bad,sizeof(wire),CELL_DELTA_CODEC_NOOP); bytes=123;
    CHECK(cell_delta_encoded_size(bad,&bytes)==CELL_DELTA_CODEC_NOOP && bytes==123);
    bad=in; bad.sheet=0; encode_rejected(bad,sizeof(wire),CELL_DELTA_CODEC_INVALID);
    written=123; CHECK(cell_delta_encode(in,NULL,sizeof(wire),&written)==CELL_DELTA_CODEC_INVALID && written==123);
    CHECK(cell_delta_encode(in,wire,sizeof(wire),NULL)==CELL_DELTA_CODEC_INVALID);
    /* A crafted clear->clear or equal-value record is never an event. */
    memcpy(corrupt,wire,DX_CELL_DELTA_HEADER_BYTES); memset(corrupt+26,0,8);
    reject(corrupt,DX_CELL_DELTA_HEADER_BYTES,CELL_DELTA_CODEC_INVALID);
    corrupt[26]=corrupt[30]=1; corrupt[DX_CELL_DELTA_HEADER_BYTES]=corrupt[DX_CELL_DELTA_HEADER_BYTES+1]='A';
    reject(corrupt,DX_CELL_DELTA_HEADER_BYTES+2,CELL_DELTA_CODEC_INVALID);
    char raw[]={'\t','\n',(char)0xc4,(char)0xb0};
    in=(CellDelta){.sheet=UINT64_MAX,.row=DX_SHEET_ROWS-1,.column=16383,.after=raw,.after_bytes=sizeof(raw)};
    CHECK(cell_delta_encode(in,wire,sizeof(wire),&written)==CELL_DELTA_CODEC_OK && cell_delta_decode(wire,written,&out)==CELL_DELTA_CODEC_OK);
    CHECK(!out.before_bytes && out.after_bytes==sizeof(raw) && out.sheet==UINT64_MAX && out.row==DX_SHEET_ROWS-1 && out.column==16383 && !memcmp(out.after,raw,sizeof(raw)));
    in=cell_delta_inverse(in);
    CHECK(cell_delta_encode(in,wire,sizeof(wire),&written)==CELL_DELTA_CODEC_OK && cell_delta_decode(wire,written,&out)==CELL_DELTA_CODEC_OK && !out.after_bytes && out.before_bytes==sizeof(raw));
    size_t n=DX_CELL_DELTA_TEXT_BYTES,total=DX_CELL_DELTA_HEADER_BYTES+2*n;
    char *a=malloc(n),*b=malloc(n); unsigned char *large=malloc(total+1); CHECK(a && b && large);
    memset(a,'A',n); memset(b,'B',n); memset(large,0xa5,total+1);
    in=(CellDelta){.sheet=1,.before=a,.after=b,.before_bytes=n,.after_bytes=n};
    CHECK(cell_delta_encoded_size(in,&bytes)==CELL_DELTA_CODEC_OK && bytes==total);
    CHECK(cell_delta_encode(in,large,total,&written)==CELL_DELTA_CODEC_OK && written==total && large[total]==0xa5);
    CHECK(cell_delta_decode(large,total,&out)==CELL_DELTA_CODEC_OK && out.before_bytes==n && out.after_bytes==n && !memcmp(out.before,a,n) && !memcmp(out.after,b,n));
    reject(large,total-1,CELL_DELTA_CODEC_INVALID); reject(large,total+1,CELL_DELTA_CODEC_INVALID);
    large[26]=1; reject(large,total,CELL_DELTA_CODEC_INVALID); large[26]=0; /* 65,537 bytes. */
    free(a); free(b); free(large);
    printf("PASS: %u canonical sparse-cell payload codec checks\n",checks); return 0;
}
