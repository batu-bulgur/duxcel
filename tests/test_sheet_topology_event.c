#include "workbook/sheet_topology_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL sheet topology:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static SheetPresencePacket presence(WorkbookId document,uint64_t id,uint64_t high,const char *name,bool hidden) {
    Workbook b; workbook_init(&b); CHECK(workbook_add(&b,name,&(Analysis){0},NULL)); b.id=document; b.sheet_high_water=high; b.sheets[0].id=id; b.sheets[0].hidden=hidden;
    char *error=NULL; CHECK(cells_set(&b.sheets[0].cells,50,7,"NULL",&error) && !error);
    SheetPresenceAsset *asset=NULL; CHECK(sheet_presence_capture((SheetPresenceView){document,high,b.sheets,&b.sheets[0].analysis},SIZE_MAX,&asset,NULL)==SHEET_PRESENCE_OK);
    SheetPresencePacket packet={0}; CHECK(sheet_presence_encode(asset,SIZE_MAX,&packet)==SHEET_PRESENCE_OK); sheet_presence_free(&asset); workbook_free(&b); return packet;
}
static void put(unsigned char *data,uint64_t value,unsigned count) { for (unsigned i=0;i<count;i++) { data[i]=(unsigned char)value; value>>=8; } }
static void add_remove(void) {
    WorkbookId document={UINT64_C(9007199254740993),77}; SheetLifecycleEntry before[]={{1,"Base",false},{7,"Other",false}};
    SheetLifecycleEntry after[]={{1,"Base",false},{8,"Added İ",false},{7,"Other",false}};
    SheetPresencePacket asset=presence(document,8,8,"Added İ",false);
    SheetTopologyView view={SHEET_TOPOLOGY_ADD,8,{document,7,1,before,2},{document,8,8,after,3},&asset}; SheetTopologyInfo info={0};
    CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_OK && info.operation==SHEET_TOPOLOGY_ADD && info.sheet==8 && info.before_count==2 && info.after_count==3 && info.presence_bytes==asset.bytes);
    SheetTopologyPacket packet={0}; CHECK(sheet_topology_capture(view,info.bytes+info.validation_scratch-1,&packet)==SHEET_TOPOLOGY_LIMIT && !packet.data);
    CHECK(sheet_topology_capture(view,info.bytes+info.validation_scratch,&packet)==SHEET_TOPOLOGY_OK && packet.bytes==info.bytes);
    SheetTopologyInfo inspected={0}; CHECK(sheet_topology_preflight(packet.data,packet.bytes,&inspected)==SHEET_TOPOLOGY_OK && inspected.owned==info.owned && inspected.validation_scratch==info.validation_scratch);
    CHECK(sheet_topology_validate(packet.data,packet.bytes,info.validation_scratch-1,&inspected)==SHEET_TOPOLOGY_LIMIT);
    CHECK(sheet_topology_validate(packet.data,packet.bytes,info.validation_scratch,&inspected)==SHEET_TOPOLOGY_OK);
    SheetTopologyEvent owned={0}; CHECK(sheet_topology_decode(packet.data,packet.bytes,info.owned+info.validation_scratch-1,&owned)==SHEET_TOPOLOGY_LIMIT && !owned.presence.data);
    CHECK(sheet_topology_decode(packet.data,packet.bytes,info.owned+info.validation_scratch,&owned)==SHEET_TOPOLOGY_OK && owned.before.sheets!=before && owned.before.sheets[0].name!=before[0].name && owned.presence.data!=asset.data);
    CHECK(!strcmp(owned.after.sheets[1].name,"Added İ") && owned.after.active==8 && !memcmp(owned.presence.data,asset.data,asset.bytes));
    SheetLifecycleState target={0},current=owned.before;
    current.high_water=100; current.active=7;
    CHECK(sheet_topology_admit(sheet_topology_view(&owned),current,true,&target)==SHEET_TOPOLOGY_OK && target.count==3 && target.high_water==100 && target.active==7);
    current=owned.after; current.high_water=100; current.active=8;
    CHECK(sheet_topology_admit(sheet_topology_view(&owned),current,false,&target)==SHEET_TOPOLOGY_OK && target.count==2 && target.high_water==100 && target.active==1);
    SheetLifecycleEntry drift[3]; memcpy(drift,owned.after.sheets,sizeof(drift)); drift[0].name="Changed"; current=owned.after; current.sheets=drift;
    SheetLifecycleState unchanged=target; CHECK(sheet_topology_admit(sheet_topology_view(&owned),current,false,&target)==SHEET_TOPOLOGY_STALE && !memcmp(&target,&unchanged,sizeof(target)));
    current=owned.after;
    SheetLifecycleState sentinel=target; current.high_water=6;
    CHECK(sheet_topology_admit(sheet_topology_view(&owned),current,false,&target)==SHEET_TOPOLOGY_INVALID && !memcmp(&target,&sentinel,sizeof(target)));
    current=owned.after; current.high_water=7; CHECK(sheet_topology_admit(sheet_topology_view(&owned),current,false,&target)==SHEET_TOPOLOGY_INVALID);
    SheetTopologyPacket canonical={0}; CHECK(sheet_topology_capture(sheet_topology_view(&owned),SIZE_MAX,&canonical)==SHEET_TOPOLOGY_OK && canonical.bytes==packet.bytes && !memcmp(canonical.data,packet.data,packet.bytes)); sheet_topology_packet_free(&canonical);
    unsigned char snapshot[sizeof(owned)]; memcpy(snapshot,&owned,sizeof(owned));
    for (size_t n=0;n<DX_SHEET_TOPOLOGY_HEADER_BYTES;n++) CHECK(sheet_topology_decode(packet.data,n,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID && !memcmp(snapshot,&owned,sizeof(owned)));
    CHECK(sheet_topology_decode(packet.data,packet.bytes-1,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID && !memcmp(snapshot,&owned,sizeof(owned)));
    unsigned char *bytes=packet.data; bytes[4]=2; CHECK(sheet_topology_decode(bytes,packet.bytes,SIZE_MAX,&owned)==SHEET_TOPOLOGY_UNSUPPORTED && !memcmp(snapshot,&owned,sizeof(owned))); bytes[4]=1;
    bytes[6]=3; CHECK(sheet_topology_decode(bytes,packet.bytes,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID && !memcmp(snapshot,&owned,sizeof(owned))); bytes[6]=1;
    put(bytes+32,8,8); CHECK(sheet_topology_decode(bytes,packet.bytes,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID && !memcmp(snapshot,&owned,sizeof(owned))); put(bytes+32,7,8);
    unsigned char *extended=dx_alloc(packet.bytes+1); memcpy(extended,packet.data,packet.bytes); CHECK(sheet_topology_decode(extended,packet.bytes+1,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID); free(extended);
    size_t presence_at=packet.bytes-asset.bytes; unsigned char initial=bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES]; bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES]='X';
    CHECK(sheet_topology_preflight(bytes,packet.bytes,&inspected)==SHEET_TOPOLOGY_OK);
    CHECK(sheet_topology_validate(bytes,packet.bytes,SIZE_MAX,&inspected)==SHEET_TOPOLOGY_INVALID);
    CHECK(sheet_topology_decode(bytes,packet.bytes,SIZE_MAX,&owned)==SHEET_TOPOLOGY_INVALID && !memcmp(snapshot,&owned,sizeof(owned))); bytes[presence_at+DX_SHEET_PRESENCE_HEADER_BYTES]=initial;
    SheetTopologyInfo prior=info;
    after[0].name="Changed"; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID && !memcmp(&prior,&info,sizeof(info))); after[0].name="Base";
    after[0].hidden=true; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); after[0].hidden=false;
    after[0]=before[1]; after[2]=before[0]; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); after[0]=before[0]; after[2]=before[1];
    view.after.active=99; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); view.after.active=8;
    /* Presence framing alone does not establish its exact name/hidden owner. */
    after[1].name="Wrong"; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_OK);
    CHECK(sheet_topology_capture(view,SIZE_MAX,&canonical)==SHEET_TOPOLOGY_INVALID && !canonical.data); after[1].name="Added İ";
    view.operation=SHEET_TOPOLOGY_REMOVE; view.before=(SheetLifecycleState){document,8,8,after,3}; view.after=(SheetLifecycleState){document,8,1,before,2};
    CHECK(sheet_topology_capture(view,SIZE_MAX,&canonical)==SHEET_TOPOLOGY_OK);
    SheetTopologyEvent removed={0}; CHECK(sheet_topology_decode(canonical.data,canonical.bytes,SIZE_MAX,&removed)==SHEET_TOPOLOGY_OK);
    current=removed.before; CHECK(sheet_topology_admit(sheet_topology_view(&removed),current,true,&target)==SHEET_TOPOLOGY_OK && target.count==2 && target.active==1 && target.high_water==8);
    current=removed.after; current.active=7; current.high_water=200; CHECK(sheet_topology_admit(sheet_topology_view(&removed),current,false,&target)==SHEET_TOPOLOGY_OK && target.count==3 && target.active==7 && target.high_water==200);
    current=removed.after; current.high_water=7; CHECK(sheet_topology_admit(sheet_topology_view(&removed),current,false,&target)==SHEET_TOPOLOGY_STALE);
    view.after.high_water=7; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID);
    sheet_topology_packet_free(&canonical); sheet_topology_free(&removed); sheet_topology_packet_free(&packet); sheet_presence_packet_free(&asset);
    CHECK(!strcmp(owned.after.sheets[1].name,"Added İ") && sheet_topology_measure(sheet_topology_view(&owned),&info)==SHEET_TOPOLOGY_OK);
    sheet_topology_free(&owned); sheet_topology_free(&owned); sheet_topology_free(NULL);
}
static void guards(void) {
    WorkbookId doc={1,2}; SheetLifecycleEntry before[]={{1,"Base",false}},after[]={{1,"Base",false},{UINT64_MAX,"Last",false}};
    SheetPresencePacket packet=presence(doc,UINT64_MAX,UINT64_MAX,"Last",false);
    SheetTopologyView view={SHEET_TOPOLOGY_ADD,UINT64_MAX,{doc,UINT64_MAX-1,1,before,1},{doc,UINT64_MAX,UINT64_MAX,after,2},&packet};
    SheetTopologyPacket out={0}; SheetTopologyInfo info; CHECK(sheet_topology_capture(view,SIZE_MAX,&out)==SHEET_TOPOLOGY_OK);
    view.before.high_water=UINT64_MAX; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_LIMIT); view.before.high_water=UINT64_MAX-1;
    view.after.count=1; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); view.after.count=2;
    after[1].hidden=true; view.after.active=1; CHECK(sheet_topology_capture(view,SIZE_MAX,&out)==SHEET_TOPOLOGY_INVALID); after[1].hidden=false; view.after.active=UINT64_MAX;
    view.before.document.low++; CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); view.before.document.low--;
    view.operation=SHEET_TOPOLOGY_REMOVE; view.before=view.after; view.after=(SheetLifecycleState){doc,UINT64_MAX,1,before,1}; before[0].hidden=true;
    CHECK(sheet_topology_measure(view,&info)==SHEET_TOPOLOGY_INVALID); before[0].hidden=false;
    CHECK(sheet_topology_capture(view,SIZE_MAX,&out)==SHEET_TOPOLOGY_OK);
    sheet_topology_packet_free(&out); sheet_presence_packet_free(&packet);
}
int main(void) { add_remove(); guards(); printf("PASS: %u canonical one-ID sheet topology checks\n",checks); return 0; }
