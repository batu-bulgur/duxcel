#include "workbook/cell_delta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL cell delta:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void rejected(CellDelta in,CellDeltaStatus status,size_t capacity) {
    unsigned char arena[64],before[64]; memset(arena,0xa5,sizeof(arena)); memcpy(before,arena,sizeof(arena));
    CellDelta out; memset(&out,0xa5,sizeof(out)); CellDelta old=out;
    CHECK(cell_delta_capture(in,arena,capacity,&out)==status && !memcmp(&old,&out,sizeof(out)) && !memcmp(arena,before,sizeof(arena)));
    size_t bytes=123;
    if (status!=CELL_DELTA_LIMIT) CHECK(cell_delta_check(in,&bytes)==status && bytes==123);
}
int main(void) {
    char before[]="9007199254740993.12",after[]="NULL",arena[128];
    CellDelta in={.sheet=UINT64_MAX,.row=DX_SHEET_ROWS-1,.column=16383,.before=before,.after=after,
        .before_bytes=sizeof(before)-1,.after_bytes=sizeof(after)-1},owned={0};
    size_t bytes=0;
    CHECK(cell_delta_check(in,&bytes)==CELL_DELTA_READY && bytes==sizeof(before)+sizeof(after)-2);
    CHECK(cell_delta_capture(in,arena,bytes,&owned)==CELL_DELTA_READY && owned.before==arena && owned.after==arena+in.before_bytes);
    CHECK(owned.sheet==in.sheet && owned.row==in.row && owned.column==in.column && owned.before_bytes==in.before_bytes && owned.after_bytes==4);
    memset(before,'x',sizeof(before)-1); memset(after,'y',sizeof(after)-1);
    CHECK(!memcmp(owned.before,"9007199254740993.12",owned.before_bytes) && !memcmp(owned.after,"NULL",4));
    CellDelta inverse=cell_delta_inverse(owned),again=cell_delta_inverse(inverse);
    CHECK(inverse.before==owned.after && inverse.after==owned.before && inverse.before_bytes==4 && inverse.after_bytes==owned.before_bytes && inverse.sheet==owned.sheet && inverse.row==owned.row && inverse.column==owned.column);
    CHECK(again.before==owned.before && again.after==owned.after && again.before_bytes==owned.before_bytes && again.after_bytes==owned.after_bytes);
    /* Input spans need no terminator. Clearing remains distinct from literal
       NULL; this sparse-manual payload does not represent typed source NULL. */
    char raw[]={'a','\t','\n',(char)0xc4,(char)0xb0};
    in=(CellDelta){.sheet=1,.before=raw,.before_bytes=sizeof(raw)};
    CHECK(cell_delta_capture(in,arena,sizeof(raw),&owned)==CELL_DELTA_READY && owned.before_bytes==sizeof(raw) && !owned.after_bytes && !memcmp(owned.before,raw,sizeof(raw)));
    in=cell_delta_inverse(in);
    CHECK(cell_delta_capture(in,arena,sizeof(raw),&owned)==CELL_DELTA_READY && !owned.before_bytes && owned.after_bytes==sizeof(raw) && !memcmp(owned.after,raw,sizeof(raw)));
    in=(CellDelta){.sheet=1,.after="NULL",.after_bytes=4};
    CHECK(cell_delta_capture(in,arena,4,&owned)==CELL_DELTA_READY && !owned.before_bytes && owned.after_bytes==4);
    rejected(in,CELL_DELTA_LIMIT,3);
    CellDelta bad=in; bad.sheet=0; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.row=DX_SHEET_ROWS; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.column=16384; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.after=NULL; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.before_bytes=1; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.after_bytes=DX_CELL_DELTA_TEXT_BYTES+1; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.before_bytes=DX_CELL_DELTA_TEXT_BYTES+1; rejected(bad,CELL_DELTA_INVALID,64);
    char nul[]={'a',0,'b'}; bad=in; bad.before=nul; bad.before_bytes=3; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.after=nul; bad.after_bytes=3; rejected(bad,CELL_DELTA_INVALID,64);
    bad=in; bad.before="NULL"; bad.before_bytes=4; rejected(bad,CELL_DELTA_NOOP,64);
    bad=(CellDelta){.sheet=1}; rejected(bad,CELL_DELTA_NOOP,64);
    bad.before=""; bad.after=""; rejected(bad,CELL_DELTA_NOOP,64);
    CellDelta saved=owned;
    CHECK(cell_delta_capture(in,NULL,4,&owned)==CELL_DELTA_INVALID && !memcmp(&saved,&owned,sizeof(owned)));
    memset(arena,0xa5,sizeof(arena)); CHECK(cell_delta_capture(in,arena,4,NULL)==CELL_DELTA_INVALID && (unsigned char)arena[0]==0xa5);
    /* Maximum two-sided payload has exact accounting and no appended bytes. */
    size_t n=DX_CELL_DELTA_TEXT_BYTES; char *a=malloc(n),*b=malloc(n),*storage=malloc(2*n+2); CHECK(a && b && storage);
    memset(a,'A',n); memset(b,'B',n); memset(storage,0xa5,2*n+2);
    in=(CellDelta){.sheet=1,.before=a,.after=b,.before_bytes=n,.after_bytes=n}; bytes=0;
    CHECK(cell_delta_check(in,&bytes)==CELL_DELTA_READY && bytes==2*n);
    CHECK(cell_delta_capture(in,storage+1,2*n,&owned)==CELL_DELTA_READY && (unsigned char)storage[0]==0xa5 && (unsigned char)storage[2*n+1]==0xa5);
    CHECK(!memcmp(owned.before,a,n) && !memcmp(owned.after,b,n));
    memset(a,'x',n); memset(b,'x',n); CHECK(owned.before[0]=='A' && owned.after[n-1]=='B');
    CHECK(cell_delta_check(in,NULL)==CELL_DELTA_NOOP);
    b[n-1]='z'; CHECK(cell_delta_check(in,NULL)==CELL_DELTA_READY);
    free(a); free(b); free(storage);
    printf("PASS: %u exact cell-delta validation, ownership and inverse checks\n",checks); return 0;
}
