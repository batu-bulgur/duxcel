#include "workbook/sheet_lifecycle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet lifecycle:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void put(unsigned char *p,uint64_t value,unsigned n) { for (unsigned i=0;i<n;i++) { p[i]=(unsigned char)value; value>>=8; } }
static void invalid(SheetLifecycle view) {
    SheetLifecycleInfo info={71,72,73},old=info; SheetLifecyclePacket packet={dx_strdup("keep"),5}; void *previous=packet.data;
    CHECK(sheet_lifecycle_measure(view,&info)!=SHEET_LIFECYCLE_OK && !memcmp(&info,&old,sizeof(info)));
    CHECK(sheet_lifecycle_capture(view,SIZE_MAX,&packet)!=SHEET_LIFECYCLE_OK && packet.data==previous && packet.bytes==5);
    sheet_lifecycle_packet_free(&packet);
}
int main(void) {
    SheetLifecycleEntry before[]={{9,"İstanbul",false},{2,"Raw data",false},{7,"Hidden",true}};
    SheetLifecycleEntry after[]={{7,"Hidden",true},{9,"Renamed",false},{2,"Raw data",true}};
    SheetLifecycle view={{{UINT64_C(9007199254740993),UINT64_MAX},12,2,before,3},{{UINT64_C(9007199254740993),UINT64_MAX},12,9,after,3}};
    SheetLifecycleInfo info={0}; CHECK(sheet_lifecycle_measure(view,&info)==SHEET_LIFECYCLE_OK && info.count==3);
    SheetLifecyclePacket packet={0},second={0};
    CHECK(sheet_lifecycle_capture(view,info.bytes-1,&packet)==SHEET_LIFECYCLE_LIMIT && !packet.data);
    CHECK(sheet_lifecycle_capture(view,info.bytes,&packet)==SHEET_LIFECYCLE_OK && packet.bytes==info.bytes);
    unsigned char *bytes=packet.data; CHECK(!memcmp(bytes,"DXSL\1\0\0\0",8));
    CHECK(bytes[8]==1 && bytes[14]==32 && bytes[24]==12 && bytes[32]==3 && bytes[40]==2 && bytes[48]==9 && bytes[64]==9);
    SheetLifecycleInfo inspected={0}; CHECK(sheet_lifecycle_preflight(bytes,packet.bytes,&inspected)==SHEET_LIFECYCLE_OK && !memcmp(&info,&inspected,sizeof(info)));
    SheetLifecycle owned={0}; CHECK(sheet_lifecycle_decode(bytes,packet.bytes,info.owned-1,&owned)==SHEET_LIFECYCLE_LIMIT && !owned.before.sheets);
    CHECK(sheet_lifecycle_decode(bytes,packet.bytes,info.owned,&owned)==SHEET_LIFECYCLE_OK);
    CHECK(owned.before.sheets!=before && owned.after.sheets!=after && owned.before.sheets[0].name!=before[0].name);
    CHECK(!strcmp(owned.before.sheets[0].name,"İstanbul") && owned.before.active==2 && owned.after.active==9 && owned.after.sheets[2].hidden);
    CHECK(sheet_lifecycle_capture(owned,info.bytes,&second)==SHEET_LIFECYCLE_OK && second.bytes==packet.bytes && !memcmp(second.data,packet.data,packet.bytes));
    SheetLifecycleState target={0}; CHECK(sheet_lifecycle_admit(owned,view.before,true,&target)==SHEET_LIFECYCLE_OK && target.sheets==owned.after.sheets && target.active==9);
    SheetLifecycleState live=view.after; live.high_water=UINT64_MAX;
    CHECK(sheet_lifecycle_admit(owned,live,false,&target)==SHEET_LIFECYCLE_OK && target.sheets==owned.before.sheets && target.high_water==UINT64_MAX);
    SheetLifecycleState unchanged=target; live.high_water=11;
    CHECK(sheet_lifecycle_admit(owned,live,false,&target)==SHEET_LIFECYCLE_STALE && !memcmp(&target,&unchanged,sizeof(target)));
    live=view.before; live.active=9; CHECK(sheet_lifecycle_admit(owned,live,true,&target)==SHEET_LIFECYCLE_OK && target.active==9);
    /* A later navigation does not stale rename Undo or manufacture an event. */
    SheetLifecycleEntry renamed[3]; memcpy(renamed,before,sizeof(before)); renamed[0].name="New title";
    SheetLifecycle rename={view.before,view.before}; rename.after.sheets=renamed;
    live=rename.after; live.active=9; live.high_water=UINT64_MAX;
    CHECK(sheet_lifecycle_admit(rename,live,false,&target)==SHEET_LIFECYCLE_OK && target.active==9 && target.high_water==UINT64_MAX && !strcmp(target.sheets[0].name,"İstanbul"));
    SheetLifecycle navigation={view.before,view.before}; navigation.after.active=9;
    CHECK(sheet_lifecycle_measure(navigation,&inspected)==SHEET_LIFECYCLE_NOOP);
    CHECK(sheet_lifecycle_admit(navigation,view.before,true,&target)==SHEET_LIFECYCLE_NOOP);
    live=view.before; live.active=2;
    CHECK(sheet_lifecycle_admit(owned,live,true,&target)==SHEET_LIFECYCLE_OK && target.active==9); /* hiding active2 falls back */
    live=view.after; live.active=9;
    CHECK(sheet_lifecycle_admit(owned,live,false,&target)==SHEET_LIFECYCLE_OK && target.active==9); /* Undo keeps navigation */
    live=view.before; live.document.low--; CHECK(sheet_lifecycle_admit(owned,live,true,&target)==SHEET_LIFECYCLE_STALE);
    SheetLifecycle prior=owned;
    for (size_t i=0;i<packet.bytes;i++) {
        SheetLifecycleInfo sentinel={71,72,73},old=sentinel;
        CHECK(sheet_lifecycle_preflight(bytes,i,&sentinel)!=SHEET_LIFECYCLE_OK && !memcmp(&sentinel,&old,sizeof(old)));
        CHECK(sheet_lifecycle_decode(bytes,i,SIZE_MAX,&owned)!=SHEET_LIFECYCLE_OK && !memcmp(&owned,&prior,sizeof(prior)));
    }
    unsigned char *mutant=dx_alloc(packet.bytes+2); memcpy(mutant+1,bytes,packet.bytes);
    CHECK(sheet_lifecycle_preflight(mutant+1,packet.bytes,&inspected)==SHEET_LIFECYCLE_OK);
    memcpy(mutant,bytes,packet.bytes); mutant[packet.bytes]=0;
    CHECK(sheet_lifecycle_decode(mutant,packet.bytes+1,SIZE_MAX,&owned)==SHEET_LIFECYCLE_INVALID && !memcmp(&owned,&prior,sizeof(prior)));
    size_t offsets[]={0,6,36,64+8,64+12,64+16};
    for (size_t i=0;i<sizeof(offsets)/sizeof(*offsets);i++) {
        memcpy(mutant,bytes,packet.bytes); mutant[offsets[i]]=0xff;
        CHECK(sheet_lifecycle_decode(mutant,packet.bytes,SIZE_MAX,&owned)==SHEET_LIFECYCLE_INVALID && !memcmp(&owned,&prior,sizeof(prior)));
    }
    memcpy(mutant,bytes,packet.bytes); mutant[4]=2;
    CHECK(sheet_lifecycle_decode(mutant,packet.bytes,SIZE_MAX,&owned)==SHEET_LIFECYCLE_UNSUPPORTED && !memcmp(&owned,&prior,sizeof(prior)));
    memcpy(mutant,bytes,packet.bytes); put(mutant+56,UINT32_MAX,4);
    CHECK(sheet_lifecycle_preflight(mutant,packet.bytes,&inspected)==SHEET_LIFECYCLE_INVALID);
    memcpy(mutant,bytes,packet.bytes); put(mutant+32,DX_MAX_SHEETS+1,4);
    CHECK(sheet_lifecycle_preflight(mutant,packet.bytes,&inspected)==SHEET_LIFECYCLE_INVALID);
    memcpy(mutant,bytes,packet.bytes); put(mutant+40,7,8);
    CHECK(sheet_lifecycle_preflight(mutant,packet.bytes,&inspected)==SHEET_LIFECYCLE_INVALID); /* hidden active */
    memcpy(mutant,bytes,packet.bytes); mutant[64+16]=0; CHECK(sheet_lifecycle_preflight(mutant,packet.bytes,&inspected)==SHEET_LIFECYCLE_INVALID);
    free(mutant);
    SheetLifecycle bad=view; bad.after=bad.before; invalid(bad);
    CHECK(sheet_lifecycle_measure(bad,&inspected)==SHEET_LIFECYCLE_NOOP);
    bad=view; bad.after.high_water++; invalid(bad);
    bad=view; bad.after.document.high++; invalid(bad);
    bad=view; bad.after.count=2; invalid(bad);
    bad=view; after[0].id=8; invalid(bad); after[0].id=7;
    bad=view; after[0].id=9; invalid(bad); after[0].id=7;
    bad=view; after[0].name="RENAMED"; invalid(bad); after[0].name="Hidden";
    const char *names[]={NULL,"","bad\nname","bad\177name","\xc0\xaf","\xed\xa0\x80"};
    for (size_t i=0;i<sizeof(names)/sizeof(*names);i++) { after[0].name=names[i]; invalid(view); }
    after[0].name="Hidden";
    after[1].hidden=true; invalid(view); after[1].hidden=false;
    bad=view; bad.before.active=0; invalid(bad);
    bad=view; bad.before.count=DX_MAX_SHEETS+1; invalid(bad);
    bad=view; bad.before.high_water=8; invalid(bad);
    /* Separate actual Workbook metadata view: order uses stable IDs and the
       active ordinal is translated, with zero authoring/history side effects. */
    Worksheet sheets[3]={{.id=9,.name="İstanbul"},{.id=2,.name="Raw data"},{.id=7,.name="Hidden",.hidden=true}};
    Workbook book={.sheets=sheets,.id=view.before.document,.sheet_high_water=12,.count=3,.active=1,.dirty=true,.command_revision=81}; Workbook original=book;
    SheetLifecycleEntry storage[DX_MAX_SHEETS]={0}; SheetLifecycleState captured={0};
    CHECK(sheet_lifecycle_from_workbook(&book,storage,&captured)==SHEET_LIFECYCLE_OK && captured.active==2 && captured.sheets==storage && captured.sheets[0].name==sheets[0].name);
    CHECK(!memcmp(&book,&original,sizeof(book)));
    SheetLifecycleEntry old_storage[DX_MAX_SHEETS]; memcpy(old_storage,storage,sizeof(storage)); SheetLifecycleState old_state=captured;
    book.active=2; CHECK(sheet_lifecycle_from_workbook(&book,storage,&captured)==SHEET_LIFECYCLE_INVALID && !memcmp(storage,old_storage,sizeof(storage)) && !memcmp(&captured,&old_state,sizeof(captured)));
    /* Full finite limits, exhausted allocator, exact surrounding whitespace,
       case-only rename and canonical ownership replacement. */
    SheetLifecycleEntry many[DX_MAX_SHEETS],changed[DX_MAX_SHEETS]; char large[DX_MAX_SHEETS][DX_SHEET_LIFECYCLE_NAME_BYTES+1];
    for (size_t i=0;i<DX_MAX_SHEETS;i++) {
        memset(large[i],'x',DX_SHEET_LIFECYCLE_NAME_BYTES); large[i][0]=(char)('A'+i/26); large[i][1]=(char)('A'+i%26); large[i][DX_SHEET_LIFECYCLE_NAME_BYTES]=0;
        many[i]=(SheetLifecycleEntry){i+1,large[i],false}; changed[i]=many[i];
    }
    changed[0].hidden=true;
    SheetLifecycle maximum={{{1,2},UINT64_MAX,2,many,DX_MAX_SHEETS},{{1,2},UINT64_MAX,2,changed,DX_MAX_SHEETS}};
    CHECK(sheet_lifecycle_measure(maximum,&inspected)==SHEET_LIFECYCLE_OK && inspected.bytes==DX_SHEET_LIFECYCLE_MAX_BYTES);
    CHECK(sheet_lifecycle_capture(maximum,inspected.bytes,&second)==SHEET_LIFECYCLE_OK);
    CHECK(sheet_lifecycle_decode(second.data,second.bytes,inspected.owned,&owned)==SHEET_LIFECYCLE_OK && owned.before.count==DX_MAX_SHEETS && owned.before.high_water==UINT64_MAX);
    after[1].name=" renamed "; CHECK(sheet_lifecycle_measure(view,&inspected)==SHEET_LIFECYCLE_OK);
    after[1].name="İSTANBUL"; CHECK(sheet_lifecycle_measure(view,&inspected)==SHEET_LIFECYCLE_OK);
    after[1].name="Renamed";
    sheet_lifecycle_free(&owned); sheet_lifecycle_free(&owned); sheet_lifecycle_packet_free(&packet); sheet_lifecycle_packet_free(&second);
    sheet_lifecycle_free(NULL); sheet_lifecycle_packet_free(NULL);
    printf("PASS: %u canonical sheet lifecycle metadata checks; no model replay or durable activation\n",checks); return 0;
}
