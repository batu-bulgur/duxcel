#include "sheet_topology_event.h"
#include "definition_wire.h"
#include <stdlib.h>
#include <string.h>
static bool document_equal(WorkbookId a,WorkbookId b) { return a.high==b.high && a.low==b.low; }
static bool state_valid(SheetLifecycleState state) { SheetLifecycleInfo info; return sheet_lifecycle_measure((SheetLifecycle){state,state},&info)==SHEET_LIFECYCLE_NOOP; }
static bool entry_equal(SheetLifecycleEntry a,SheetLifecycleEntry b) { return a.id==b.id && a.hidden==b.hidden && !strcmp(a.name,b.name); }
static size_t locate(SheetLifecycleState state,uint64_t id) { for (size_t i=0;i<state.count;i++) if (state.sheets[i].id==id) return i; return state.count; }
static size_t catalog_bytes(SheetLifecycleState state) { size_t bytes=0; for (size_t i=0;i<state.count;i++) bytes+=16+strlen(state.sheets[i].name); return bytes; }
static size_t catalog_owned(SheetLifecycleState state) { return catalog_bytes(state)-16*state.count+state.count*(sizeof(SheetLifecycleEntry)+1); }
static SheetTopologyStatus model(SheetTopologyView v,SheetTopologyInfo *out,SheetPresenceWireInfo *presence) {
    if (!out || !presence || !v.presence || !v.sheet || !state_valid(v.before) || !state_valid(v.after) || !document_equal(v.before.document,v.after.document)) return SHEET_TOPOLOGY_INVALID;
    SheetLifecycleState full,small;
    if (v.operation==SHEET_TOPOLOGY_ADD) {
        if (v.before.high_water==UINT64_MAX) return SHEET_TOPOLOGY_LIMIT;
        if (v.after.count!=v.before.count+1 || v.sheet!=v.before.high_water+1 || v.after.high_water!=v.sheet) return SHEET_TOPOLOGY_INVALID;
        full=v.after; small=v.before;
    } else if (v.operation==SHEET_TOPOLOGY_REMOVE) {
        if (v.before.count!=v.after.count+1 || v.before.high_water!=v.after.high_water) return SHEET_TOPOLOGY_INVALID;
        full=v.before; small=v.after;
    } else return SHEET_TOPOLOGY_INVALID;
    size_t at=locate(full,v.sheet); if (at==full.count || locate(small,v.sheet)!=small.count) return SHEET_TOPOLOGY_INVALID;
    for (size_t i=0,j=0;i<full.count;i++) if (i!=at && !entry_equal(full.sheets[i],small.sheets[j++])) return SHEET_TOPOLOGY_INVALID;
    SheetPresenceStatus status=sheet_presence_preflight(v.presence->data,v.presence->bytes,presence);
    if (status==SHEET_PRESENCE_UNSUPPORTED) return SHEET_TOPOLOGY_UNSUPPORTED;
    if (status!=SHEET_PRESENCE_OK || !document_equal(presence->document,full.document) || presence->sheet!=v.sheet || presence->sheet_high_water!=full.high_water) return SHEET_TOPOLOGY_INVALID;
    size_t bytes=DX_SHEET_TOPOLOGY_HEADER_BYTES+catalog_bytes(v.before)+catalog_bytes(v.after)+presence->bytes;
    if (bytes>DX_SHEET_TOPOLOGY_MAX_BYTES || presence->owned>SIZE_MAX-presence->validation_scratch) return SHEET_TOPOLOGY_LIMIT;
    *out=(SheetTopologyInfo){v.before.document,v.operation,v.sheet,bytes,catalog_owned(v.before)+catalog_owned(v.after)+presence->bytes,
        presence->owned+presence->validation_scratch,v.before.count,v.after.count,presence->bytes}; return SHEET_TOPOLOGY_OK;
}
SheetTopologyStatus sheet_topology_measure(SheetTopologyView view,SheetTopologyInfo *out) { SheetTopologyInfo info; SheetPresenceWireInfo presence; SheetTopologyStatus status=model(view,&info,&presence); if (!status && out) *out=info; return out ? status : SHEET_TOPOLOGY_INVALID; }
static bool presence_valid(SheetTopologyView view,size_t reserved) {
    SheetPresenceAsset *asset=NULL; if (sheet_presence_decode(view.presence->data,view.presence->bytes,reserved,&asset,NULL)!=SHEET_PRESENCE_OK) return false;
    SheetPresenceView owned=sheet_presence_view(asset); SheetLifecycleState full=view.operation==SHEET_TOPOLOGY_ADD ? view.after : view.before;
    size_t at=locate(full,view.sheet); bool matches=at<full.count && !strcmp(owned.sheet->name,full.sheets[at].name) && owned.sheet->hidden==full.sheets[at].hidden;
    sheet_presence_free(&asset); return matches;
}
static void write_row_prefix(DefinitionWireWriter *w,SheetLifecycleEntry row) {
    definition_wire_put(w,row.id,8); definition_wire_put(w,row.hidden,4); definition_wire_put(w,strlen(row.name),4);
}
static void write_catalog(DefinitionWireWriter *w,SheetLifecycleState state) {
    for (size_t i=0;i<state.count;i++) {
        write_row_prefix(w,state.sheets[i]); size_t n=strlen(state.sheets[i].name); memcpy(w->p,state.sheets[i].name,n); w->p+=n;
    }
}
static void write_header(DefinitionWireWriter *w,SheetTopologyView view) {
    memcpy(w->p,"DXTP",4); w->p+=4;
    definition_wire_put(w,DX_SHEET_TOPOLOGY_ENCODING,2); definition_wire_put(w,view.operation,2); definition_wire_put(w,view.before.document.high,8); definition_wire_put(w,view.before.document.low,8);
    definition_wire_put(w,view.sheet,8); definition_wire_put(w,view.before.high_water,8); definition_wire_put(w,view.after.high_water,8); definition_wire_put(w,view.before.active,8); definition_wire_put(w,view.after.active,8);
    definition_wire_put(w,view.before.count,2); definition_wire_put(w,view.after.count,2); definition_wire_put(w,catalog_bytes(view.before),4); definition_wire_put(w,catalog_bytes(view.after),4); definition_wire_put(w,view.presence->bytes,4);
}
void sheet_topology_packet_free(SheetTopologyPacket *packet) { if (packet) { free(packet->data); *packet=(SheetTopologyPacket){0}; } }
SheetTopologyStatus sheet_topology_capture(SheetTopologyView view,size_t reserved,SheetTopologyPacket *out) {
    if (!out) return SHEET_TOPOLOGY_INVALID; SheetTopologyInfo info; SheetPresenceWireInfo presence; SheetTopologyStatus status=model(view,&info,&presence); if (status) return status;
    if (info.bytes>reserved || info.validation_scratch>reserved-info.bytes) return SHEET_TOPOLOGY_LIMIT;
    if (!presence_valid(view,info.validation_scratch)) return SHEET_TOPOLOGY_INVALID;
    SheetTopologyPacket next={dx_alloc(info.bytes),info.bytes}; DefinitionWireWriter w={next.data}; write_header(&w,view);
    write_catalog(&w,view.before); write_catalog(&w,view.after); memcpy(w.p,view.presence->data,view.presence->bytes); w.p+=view.presence->bytes;
    if ((size_t)(w.p-(unsigned char *)next.data)!=next.bytes) { sheet_topology_packet_free(&next); return SHEET_TOPOLOGY_INVALID; }
    sheet_topology_packet_free(out); *out=next; return SHEET_TOPOLOGY_OK;
}
typedef struct { SheetLifecycleEntry entries[DX_MAX_SHEETS]; char names[DX_MAX_SHEETS][DX_SHEET_LIFECYCLE_NAME_BYTES+1]; } CatalogStorage;
static bool read_catalog(DefinitionWireReader *r,size_t bytes,SheetLifecycleState *state,CatalogStorage *storage) {
    if (bytes>r->bytes-r->at) return false; size_t end=r->at+bytes;
    for (size_t i=0;i<state->count;i++) {
        uint64_t id=definition_wire_get(r,8),hidden=definition_wire_get(r,4); size_t n=0;
        size_t prefix=r->at; bool text=definition_wire_text_read(r,NULL,&n);
        if (!r->ok || !text || !n || n>DX_SHEET_LIFECYCLE_NAME_BYTES || hidden>1 || r->at>end) return false;
        memcpy(storage->names[i],r->data+prefix+4,n); storage->names[i][n]=0; storage->entries[i]=(SheetLifecycleEntry){id,storage->names[i],hidden!=0};
    }
    state->sheets=storage->entries; return r->at==end;
}
static SheetTopologyStatus inspect(const void *data,size_t bytes,SheetTopologyView *view,SheetPresencePacket *presence,CatalogStorage storage[2],SheetTopologyInfo *out) {
    if (!data || !out || bytes<6 || bytes>DX_SHEET_TOPOLOGY_MAX_BYTES || memcmp(data,"DXTP",4)) return SHEET_TOPOLOGY_INVALID;
    DefinitionWireReader r={data,bytes,4,0,0,SIZE_MAX,true}; if (definition_wire_get(&r,2)!=DX_SHEET_TOPOLOGY_ENCODING) return SHEET_TOPOLOGY_UNSUPPORTED;
    if (bytes<DX_SHEET_TOPOLOGY_HEADER_BYTES) return SHEET_TOPOLOGY_INVALID;
    SheetTopologyView v={0}; v.operation=(SheetTopologyOperation)definition_wire_get(&r,2); WorkbookId document;
    document.high=definition_wire_get(&r,8); document.low=definition_wire_get(&r,8); v.sheet=definition_wire_get(&r,8);
    v.before.document=v.after.document=document; v.before.high_water=definition_wire_get(&r,8); v.after.high_water=definition_wire_get(&r,8); v.before.active=definition_wire_get(&r,8); v.after.active=definition_wire_get(&r,8);
    v.before.count=(size_t)definition_wire_get(&r,2); v.after.count=(size_t)definition_wire_get(&r,2);
    size_t a=(size_t)definition_wire_get(&r,4),b=(size_t)definition_wire_get(&r,4),n=(size_t)definition_wire_get(&r,4);
    if (!r.ok || v.before.count>DX_MAX_SHEETS || v.after.count>DX_MAX_SHEETS || !read_catalog(&r,a,&v.before,storage) || !read_catalog(&r,b,&v.after,storage+1) || n!=r.bytes-r.at) return SHEET_TOPOLOGY_INVALID;
    *presence=(SheetPresencePacket){(void *)(r.data+r.at),n}; v.presence=presence; SheetPresenceWireInfo info;
    SheetTopologyStatus status=model(v,out,&info); if (!status) *view=v; return status;
}
SheetTopologyStatus sheet_topology_preflight(const void *data,size_t bytes,SheetTopologyInfo *out) {
    if (!out) return SHEET_TOPOLOGY_INVALID; CatalogStorage storage[2]; SheetTopologyView view; SheetPresencePacket presence; SheetTopologyInfo info;
    SheetTopologyStatus status=inspect(data,bytes,&view,&presence,storage,&info); if (!status) *out=info; return status;
}
SheetTopologyStatus sheet_topology_validate(const void *data,size_t bytes,size_t reserved,SheetTopologyInfo *out) {
    if (!out) return SHEET_TOPOLOGY_INVALID; CatalogStorage storage[2]; SheetTopologyView view; SheetPresencePacket presence; SheetTopologyInfo info;
    SheetTopologyStatus status=inspect(data,bytes,&view,&presence,storage,&info); if (status) return status;
    if (info.validation_scratch>reserved) return SHEET_TOPOLOGY_LIMIT;
    if (!presence_valid(view,info.validation_scratch)) return SHEET_TOPOLOGY_INVALID;
    *out=info; return SHEET_TOPOLOGY_OK;
}
static void free_catalog(SheetLifecycleState *state) { if (state->sheets) { for (size_t i=0;i<state->count;i++) free((void *)state->sheets[i].name); free((void *)state->sheets); } *state=(SheetLifecycleState){0}; }
void sheet_topology_free(SheetTopologyEvent *event) { if (event) { free_catalog(&event->before); free_catalog(&event->after); sheet_presence_packet_free(&event->presence); *event=(SheetTopologyEvent){0}; } }
static SheetLifecycleState copy_catalog(SheetLifecycleState state) {
    SheetLifecycleEntry *entries=dx_alloc(state.count*sizeof(*entries)); for (size_t i=0;i<state.count;i++) { entries[i]=state.sheets[i]; entries[i].name=dx_strdup(entries[i].name); }
    state.sheets=entries; return state;
}
SheetTopologyStatus sheet_topology_decode(const void *data,size_t bytes,size_t reserved,SheetTopologyEvent *out) {
    if (!out) return SHEET_TOPOLOGY_INVALID; CatalogStorage storage[2]; SheetTopologyView view; SheetPresencePacket presence; SheetTopologyInfo info;
    SheetTopologyStatus status=inspect(data,bytes,&view,&presence,storage,&info); if (status) return status;
    if (info.owned>reserved || info.validation_scratch>reserved-info.owned) return SHEET_TOPOLOGY_LIMIT;
    if (!presence_valid(view,info.validation_scratch)) return SHEET_TOPOLOGY_INVALID;
    SheetTopologyEvent next={view.operation,view.sheet,copy_catalog(view.before),copy_catalog(view.after),{dx_alloc(presence.bytes),presence.bytes}};
    memcpy(next.presence.data,presence.data,presence.bytes); sheet_topology_free(out); *out=next; return SHEET_TOPOLOGY_OK;
}
SheetTopologyView sheet_topology_view(const SheetTopologyEvent *event) { return event ? (SheetTopologyView){event->operation,event->sheet,event->before,event->after,&event->presence} : (SheetTopologyView){0}; }
SheetTopologyStatus sheet_topology_packet_matches(SheetTopologyView view,const SheetTopologyPacket *packet,bool *out) {
    if (!packet || !out) return SHEET_TOPOLOGY_INVALID; SheetTopologyInfo expected,actual; SheetPresenceWireInfo presence;
    SheetTopologyStatus status=model(view,&expected,&presence); if (status) return status;
    status=sheet_topology_preflight(packet->data,packet->bytes,&actual); if (status) return status;
    if (packet->bytes!=expected.bytes) { *out=false; return SHEET_TOPOLOGY_OK; }
    unsigned char header[DX_SHEET_TOPOLOGY_HEADER_BYTES]; DefinitionWireWriter writer={header}; write_header(&writer,view);
    const unsigned char *data=packet->data; bool same=!memcmp(header,data,sizeof(header)); size_t at=sizeof(header);
    SheetLifecycleState states[2]={view.before,view.after};
    for (unsigned side=0;side<2;side++) for (size_t i=0;i<states[side].count;i++) {
        SheetLifecycleEntry row=states[side].sheets[i]; unsigned char prefix[16]; DefinitionWireWriter field={prefix}; write_row_prefix(&field,row);
        size_t n=strlen(row.name); same=(!memcmp(prefix,data+at,sizeof(prefix)) && !memcmp(row.name,data+at+sizeof(prefix),n)) && same; at+=sizeof(prefix)+n;
    }
    /* Model bounds and the equal complete size prove every comparison range.
       Compare the original immutable DXSP bytes, not a refined live projection. */
    same=(!memcmp(view.presence->data,data+at,view.presence->bytes)) && same;
    *out=same; return SHEET_TOPOLOGY_OK;
}
static bool authored_equal(SheetLifecycleState a,SheetLifecycleState b) { if (!document_equal(a.document,b.document) || a.count!=b.count) return false; for (size_t i=0;i<a.count;i++) if (!entry_equal(a.sheets[i],b.sheets[i])) return false; return true; }
SheetTopologyStatus sheet_topology_admit(SheetTopologyView view,SheetLifecycleState current,bool redo,SheetLifecycleState *out) {
    if (!out) return SHEET_TOPOLOGY_INVALID; SheetTopologyInfo info; SheetPresenceWireInfo presence; SheetTopologyStatus status=model(view,&info,&presence); if (status) return status;
    if (!state_valid(current)) return SHEET_TOPOLOGY_INVALID;
    SheetLifecycleState expected=redo ? view.before : view.after,target=redo ? view.after : view.before;
    if (current.high_water<expected.high_water || !authored_equal(current,expected)) return SHEET_TOPOLOGY_STALE;
    if (target.high_water<current.high_water) target.high_water=current.high_water;
    size_t active=locate(target,current.active); if (active<target.count && !target.sheets[active].hidden) target.active=current.active;
    *out=target; return SHEET_TOPOLOGY_OK;
}
