#include "workbook/column_widths.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL width asset:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void bad(const void *data,size_t bytes,ColumnWidthStatus expected,ColumnWidthDelta *owned) {
    ColumnWidthInfo sentinel={7,8,9,10},info=sentinel; ColumnWidthDelta prior=*owned;
    CHECK(column_width_preflight(data,bytes,&info)==expected && !memcmp(&info,&sentinel,sizeof(info)));
    CHECK(column_width_decode(data,bytes,SIZE_MAX,owned)==expected && !memcmp(owned,&prior,sizeof(prior)));
}
int main(void) {
    CHECK(sizeof(float)==4 && column_width_bits(96)==DX_COLUMN_WIDTH_DEFAULT_BITS);
    float dense[104]; for (size_t i=0;i<104;i++) dense[i]=96;
    ColumnWidthState a={0},b={0},shifted={0};
    CHECK(column_width_capture(dense,52,0,&a)==COLUMN_WIDTH_OK && !a.count && !a.entries);
    CHECK(column_width_capture(dense,104,0,&b)==COLUMN_WIDTH_OK && column_width_equal(column_width_view(&a),column_width_view(&b)));
    size_t bytes=91; CHECK(column_width_delta_size(column_width_view(&a),column_width_view(&b),&bytes)==COLUMN_WIDTH_NOOP && bytes==91);
    CHECK(column_width_resize(column_width_view(&a),3,96,0,&b)==COLUMN_WIDTH_NOOP);
    dense[51]=nextafterf(96,1200); CHECK(column_width_capture(dense,52,sizeof(ColumnWidthEntry)-1,&a)==COLUMN_WIDTH_LIMIT && !a.count);
    CHECK(column_width_capture(dense,52,sizeof(ColumnWidthEntry),&a)==COLUMN_WIDTH_OK && a.count==1);
    CHECK(a.entries[0].column==51 && a.entries[0].bits==DX_COLUMN_WIDTH_DEFAULT_BITS+1);
    CHECK(column_width_shift(column_width_view(&a),0,true,SIZE_MAX,&shifted)==COLUMN_WIDTH_OK);
    CHECK(shifted.count==1 && shifted.entries[0].column==52); /* past cached52 is NOT truncated */
    CHECK(column_width_shift(column_width_view(&shifted),0,false,SIZE_MAX,&b)==COLUMN_WIDTH_OK && column_width_equal(column_width_view(&a),column_width_view(&b)));
    CHECK(column_width_resize(column_width_view(&a),16383,1200,SIZE_MAX,&a)==COLUMN_WIDTH_OK);
    CHECK(column_width_shift(column_width_view(&a),0,true,SIZE_MAX,&b)==COLUMN_WIDTH_OK && b.count==1 && column_width_at(column_width_view(&b),16383)==DX_COLUMN_WIDTH_DEFAULT_BITS);
    CHECK(column_width_shift(column_width_view(&b),0,false,SIZE_MAX,&shifted)==COLUMN_WIDTH_OK && !column_width_equal(column_width_view(&a),column_width_view(&shifted)));
    CHECK(column_width_delta_size(column_width_view(&a),column_width_view(&b),&bytes)==COLUMN_WIDTH_OK);
    unsigned char *packet=dx_alloc(bytes+1),*copy=dx_alloc(bytes+1); memset(packet,0xcc,bytes+1); size_t written=777;
    CHECK(column_width_encode(column_width_view(&a),column_width_view(&b),packet,bytes-1,&written)==COLUMN_WIDTH_LIMIT && written==777);
    for (size_t i=0;i<bytes+1;i++) CHECK(packet[i]==0xcc);
    CHECK(column_width_encode(column_width_view(&a),column_width_view(&b),packet,bytes,&written)==COLUMN_WIDTH_OK && written==bytes && packet[bytes]==0xcc);
    unsigned char arena_before[DX_COLUMN_WIDTH_HEADER_BYTES+18]; memcpy(arena_before,packet,bytes);
    CHECK(column_width_encode(column_width_view(&a),column_width_view(&b),packet,bytes,(size_t *)(void *)packet)==COLUMN_WIDTH_INVALID && !memcmp(packet,arena_before,bytes));
    ColumnWidthEntry entry_before=a.entries[0];
    CHECK(column_width_encode(column_width_view(&a),column_width_view(&b),a.entries,SIZE_MAX,&written)==COLUMN_WIDTH_INVALID && a.entries[0].column==entry_before.column && a.entries[0].bits==entry_before.bits);
    CHECK(!memcmp(packet,"DXCW\1\0\0\0\0\100\0\0\2\0\1\0\0\0\0\0",20));
    ColumnWidthInfo info; CHECK(column_width_preflight(packet,bytes,&info)==COLUMN_WIDTH_OK && info.owned==3*sizeof(ColumnWidthEntry));
    ColumnWidthDelta decoded={0}; CHECK(column_width_decode(packet,bytes,info.owned-1,&decoded)==COLUMN_WIDTH_LIMIT && !decoded.before.entries);
    CHECK(column_width_decode(packet,bytes,info.owned,&decoded)==COLUMN_WIDTH_OK && decoded.before.entries!=a.entries && decoded.after.entries!=b.entries);
    CHECK(column_width_equal(column_width_view(&decoded.before),column_width_view(&a)) && column_width_equal(column_width_view(&decoded.after),column_width_view(&b)));
    CHECK(column_width_matches(packet,bytes,false,column_width_view(&a)) && column_width_matches(packet,bytes,true,column_width_view(&b)));
    ColumnWidthPair inverse=column_width_inverse(column_width_delta_view(&decoded));
    CHECK(column_width_encode(inverse.before,inverse.after,copy,bytes,&written)==COLUMN_WIDTH_OK);
    CHECK(column_width_matches(copy,bytes,true,column_width_view(&a)) && column_width_at(inverse.after,16383)==column_width_bits(1200));
    for (size_t n=0;n<bytes;n++) bad(packet,n,COLUMN_WIDTH_INVALID,&decoded);
    memcpy(copy,packet,bytes); bad(copy,bytes+1,COLUMN_WIDTH_INVALID,&decoded);
    const size_t offsets[]={0,6,8,16,21,27,28};
    for (size_t i=0;i<sizeof(offsets)/sizeof(*offsets);i++) { memcpy(copy,packet,bytes); copy[offsets[i]]=0xff; bad(copy,bytes,COLUMN_WIDTH_INVALID,&decoded); }
    memcpy(copy,packet,bytes); copy[4]=2; bad(copy,bytes,COLUMN_WIDTH_UNSUPPORTED,&decoded);
    memcpy(copy,packet,bytes); memcpy(copy+26,copy+20,2); bad(copy,bytes,COLUMN_WIDTH_INVALID,&decoded); /* duplicate */
    memcpy(copy,packet,bytes); uint32_t default_bits=DX_COLUMN_WIDTH_DEFAULT_BITS; for (unsigned i=0;i<4;i++) copy[22+i]=(unsigned char)(default_bits>>(8*i)); bad(copy,bytes,COLUMN_WIDTH_INVALID,&decoded);
    const uint32_t invalid[]={0,UINT32_C(0x80000000),UINT32_C(0x7f800000),UINT32_C(0xff800000),UINT32_C(0x7fc00000),UINT32_C(0x424fffff),UINT32_C(0x44960001)};
    ColumnWidthState prior=a;
    for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        dense[0]=column_width_value(invalid[i]); CHECK(column_width_capture(dense,1,SIZE_MAX,&a)==COLUMN_WIDTH_INVALID && a.entries==prior.entries);
        CHECK(column_width_resize(column_width_view(&a),0,dense[0],SIZE_MAX,&b)==COLUMN_WIDTH_INVALID);
    }
    ColumnWidthEntry malformed[]={{2,column_width_bits(100)},{1,column_width_bits(110)}};
    CHECK(!column_width_valid((ColumnWidthView){malformed,2})); malformed[1].column=2; CHECK(!column_width_valid((ColumnWidthView){malformed,2}));
    CHECK(column_width_resize(column_width_view(&a),DX_COLUMN_WIDTH_COLUMNS,100,SIZE_MAX,&b)==COLUMN_WIDTH_INVALID);
    CHECK(column_width_shift(column_width_view(&a),DX_COLUMN_WIDTH_COLUMNS,true,SIZE_MAX,&b)==COLUMN_WIDTH_INVALID);
    /* Maximum canonical count and exact float-bit bounds, without epsilon. */
    float *full=dx_alloc(DX_COLUMN_WIDTH_COLUMNS*sizeof(float));
    for (size_t i=0;i<DX_COLUMN_WIDTH_COLUMNS;i++) full[i]=i%2 ? 52 : 1200;
    CHECK(column_width_capture(full,DX_COLUMN_WIDTH_COLUMNS,SIZE_MAX,&a)==COLUMN_WIDTH_OK && a.count==DX_COLUMN_WIDTH_COLUMNS);
    full[0]=nextafterf(52,1200); CHECK(column_width_capture(full,DX_COLUMN_WIDTH_COLUMNS,SIZE_MAX,&b)==COLUMN_WIDTH_OK);
    CHECK(column_width_delta_size(column_width_view(&a),column_width_view(&b),&bytes)==COLUMN_WIDTH_OK && bytes==DX_COLUMN_WIDTH_MAX_BYTES);
    free(packet); packet=dx_alloc(bytes); CHECK(column_width_encode(column_width_view(&a),column_width_view(&b),packet,bytes,&written)==COLUMN_WIDTH_OK);
    CHECK(column_width_decode(packet,bytes,2*DX_COLUMN_WIDTH_COLUMNS*sizeof(ColumnWidthEntry),&decoded)==COLUMN_WIDTH_OK);
    CHECK(column_width_equal(column_width_view(&decoded.before),column_width_view(&a)) && column_width_equal(column_width_view(&decoded.after),column_width_view(&b)));
    for (uint32_t bits=UINT32_C(0x42bffff0);bits<UINT32_C(0x42c00010);bits++) {
        float value=column_width_value(bits); ColumnWidthEntry entry={0,bits};
        CHECK(column_width_valid((ColumnWidthView){&entry,1})==(bits!=DX_COLUMN_WIDTH_DEFAULT_BITS));
        CHECK(column_width_bits(value)==bits);
    }
    free(full); free(packet); free(copy); column_width_delta_free(&decoded); column_width_free(&a); column_width_free(&b); column_width_free(&shifted);
    puts("PASS: canonical column width assets, exact binary32 pairs and virtual-edge inverse"); printf("PASS: %u width asset checks\n",checks); return 0;
}
