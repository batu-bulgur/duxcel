#include "sheet_lifecycle.h"
#include "definition_wire.h"
#include <stdlib.h>
#include <string.h>
typedef struct { uint64_t id; const char *name; size_t bytes; bool hidden; } Row;
typedef struct { WorkbookId document; uint64_t high_water,active; size_t count; Row rows[DX_MAX_SHEETS]; } State;
static bool same_document(WorkbookId a,WorkbookId b) { return a.high==b.high && a.low==b.low; }
static unsigned char fold(unsigned char c) { return c>='A' && c<='Z' ? c+('a'-'A') : c; }
static bool same_name(Row a,Row b) {
    if (a.bytes!=b.bytes) return false;
    for (size_t i=0;i<a.bytes;i++) if (fold((unsigned char)a.name[i])!=fold((unsigned char)b.name[i])) return false;
    return true;
}
static SheetLifecycleStatus state_check(const State *s) {
    if ((!s->document.high && !s->document.low) || !s->count || s->count>DX_MAX_SHEETS) return SHEET_LIFECYCLE_INVALID;
    bool active=false;
    for (size_t i=0;i<s->count;i++) {
        Row r=s->rows[i];
        if (!r.id || r.id>s->high_water || !r.name || !r.bytes || r.bytes>DX_SHEET_LIFECYCLE_NAME_BYTES) return SHEET_LIFECYCLE_INVALID;
        for (size_t n=0;n<r.bytes;n++) if ((unsigned char)r.name[n]<32 || (unsigned char)r.name[n]==127) return SHEET_LIFECYCLE_INVALID;
        for (size_t j=0;j<i;j++) if (r.id==s->rows[j].id || same_name(r,s->rows[j])) return SHEET_LIFECYCLE_INVALID;
        if (r.id==s->active) { if (r.hidden) return SHEET_LIFECYCLE_INVALID; active=true; }
    }
    return active ? SHEET_LIFECYCLE_OK : SHEET_LIFECYCLE_INVALID;
}
static SheetLifecycleStatus state_view(SheetLifecycleState view,State *out,size_t *bytes,size_t *names) {
    if (view.count>DX_MAX_SHEETS) return SHEET_LIFECYCLE_LIMIT;
    if (!view.sheets || !view.count) return SHEET_LIFECYCLE_INVALID;
    State s={.document=view.document,.high_water=view.high_water,.active=view.active,.count=view.count};
    size_t encoded=0,owned=0;
    for (size_t i=0;i<s.count;i++) {
        DefinitionWireMeasure m={0}; size_t length=0;
        if (!view.sheets[i].name || !definition_wire_text_measure(view.sheets[i].name,&m,DX_SHEET_LIFECYCLE_NAME_BYTES+1u,&length)) return SHEET_LIFECYCLE_INVALID;
        s.rows[i]=(Row){view.sheets[i].id,view.sheets[i].name,length,view.sheets[i].hidden};
        encoded+=16+length; owned+=length+1;
    }
    SheetLifecycleStatus status=state_check(&s); if (status) return status;
    *out=s; if (bytes) *bytes=encoded; if (names) *names=owned; return SHEET_LIFECYCLE_OK;
}
static bool same_state(const State *a,const State *b) {
    if (!same_document(a->document,b->document) || a->count!=b->count) return false;
    for (size_t i=0;i<a->count;i++) {
        Row x=a->rows[i],y=b->rows[i];
        if (x.id!=y.id || x.hidden!=y.hidden || x.bytes!=y.bytes || memcmp(x.name,y.name,x.bytes)) return false;
    }
    return true;
}
static SheetLifecycleStatus pair_check(const State *a,const State *b) {
    if (!same_document(a->document,b->document) || a->high_water!=b->high_water || a->count!=b->count) return SHEET_LIFECYCLE_INVALID;
    for (size_t i=0;i<a->count;i++) {
        bool found=false; for (size_t j=0;j<b->count;j++) found|=a->rows[i].id==b->rows[j].id;
        if (!found) return SHEET_LIFECYCLE_INVALID;
    }
    return same_state(a,b) ? SHEET_LIFECYCLE_NOOP : SHEET_LIFECYCLE_OK;
}
static SheetLifecycleStatus measure(SheetLifecycle view,State *a,State *b,SheetLifecycleInfo *out) {
    size_t ab=0,bb=0,an=0,bn=0;
    SheetLifecycleStatus status=state_view(view.before,a,&ab,&an); if (status) return status;
    status=state_view(view.after,b,&bb,&bn); if (status) return status;
    status=pair_check(a,b); if (status) return status;
    *out=(SheetLifecycleInfo){DX_SHEET_LIFECYCLE_HEADER_BYTES+ab+bb,sizeof(SheetLifecycle)+2*a->count*sizeof(SheetLifecycleEntry)+an+bn,a->count};
    return SHEET_LIFECYCLE_OK;
}
SheetLifecycleStatus sheet_lifecycle_measure(SheetLifecycle view,SheetLifecycleInfo *out) {
    if (!out) return SHEET_LIFECYCLE_INVALID;
    State a,b; SheetLifecycleInfo info; SheetLifecycleStatus status=measure(view,&a,&b,&info);
    if (!status) *out=info; return status;
}
static void write_state(DefinitionWireWriter *w,const State *s) {
    for (size_t i=0;i<s->count;i++) {
        Row row=s->rows[i]; definition_wire_put(w,row.id,8); definition_wire_put(w,row.hidden,4);
        definition_wire_put(w,row.bytes,4); memcpy(w->p,row.name,row.bytes); w->p+=row.bytes;
    }
}
SheetLifecycleStatus sheet_lifecycle_capture(SheetLifecycle view,size_t reserved,SheetLifecyclePacket *out) {
    if (!out) return SHEET_LIFECYCLE_INVALID;
    State a,b; SheetLifecycleInfo info; SheetLifecycleStatus status=measure(view,&a,&b,&info); if (status) return status;
    if (info.bytes>reserved) return SHEET_LIFECYCLE_LIMIT;
    unsigned char *data=dx_alloc(info.bytes); DefinitionWireWriter w={data}; memcpy(w.p,"DXSL",4); w.p+=4;
    definition_wire_put(&w,DX_SHEET_LIFECYCLE_ENCODING,2); definition_wire_put(&w,0,2);
    definition_wire_put(&w,a.document.high,8); definition_wire_put(&w,a.document.low,8); definition_wire_put(&w,a.high_water,8);
    definition_wire_put(&w,a.count,4); definition_wire_put(&w,0,4); definition_wire_put(&w,a.active,8); definition_wire_put(&w,b.active,8);
    size_t ab=0,bb=0; for (size_t i=0;i<a.count;i++) { ab+=16+a.rows[i].bytes; bb+=16+b.rows[i].bytes; }
    definition_wire_put(&w,ab,4); definition_wire_put(&w,bb,4); write_state(&w,&a); write_state(&w,&b);
    sheet_lifecycle_packet_free(out); *out=(SheetLifecyclePacket){data,info.bytes}; return SHEET_LIFECYCLE_OK;
}
static SheetLifecycleStatus parse(const void *data,size_t bytes,State *a,State *b,SheetLifecycleInfo *out) {
    if (!data || bytes<6 || bytes>DX_SHEET_LIFECYCLE_MAX_BYTES) return SHEET_LIFECYCLE_INVALID;
    DefinitionWireReader r={.data=data,.bytes=bytes,.at=4,.owned_limit=SIZE_MAX,.ok=true};
    if (memcmp(data,"DXSL",4)) return SHEET_LIFECYCLE_INVALID;
    if (definition_wire_get(&r,2)!=DX_SHEET_LIFECYCLE_ENCODING) return SHEET_LIFECYCLE_UNSUPPORTED;
    if (bytes<DX_SHEET_LIFECYCLE_HEADER_BYTES || definition_wire_get(&r,2)) return SHEET_LIFECYCLE_INVALID;
    State sides[2]={0}; State *first=sides;
    first->document.high=definition_wire_get(&r,8); first->document.low=definition_wire_get(&r,8); first->high_water=definition_wire_get(&r,8);
    first->count=(size_t)definition_wire_get(&r,4); if (!first->count || first->count>DX_MAX_SHEETS || definition_wire_get(&r,4)) return SHEET_LIFECYCLE_INVALID;
    sides[1]=*first; first->active=definition_wire_get(&r,8); sides[1].active=definition_wire_get(&r,8);
    size_t lengths[2]={(size_t)definition_wire_get(&r,4),(size_t)definition_wire_get(&r,4)};
    if (lengths[0]>bytes-DX_SHEET_LIFECYCLE_HEADER_BYTES || lengths[1]!=bytes-DX_SHEET_LIFECYCLE_HEADER_BYTES-lengths[0]) return SHEET_LIFECYCLE_INVALID;
    for (unsigned side=0;side<2;side++) {
        size_t start=r.at; State *s=sides+side;
        for (size_t i=0;i<s->count;i++) {
            Row *row=s->rows+i; row->id=definition_wire_get(&r,8); uint64_t hidden=definition_wire_get(&r,4);
            if (hidden>1) return SHEET_LIFECYCLE_INVALID; row->hidden=hidden!=0;
            size_t name_at=r.at+4;
            if (!definition_wire_text_read(&r,NULL,&row->bytes) || row->bytes>DX_SHEET_LIFECYCLE_NAME_BYTES) return SHEET_LIFECYCLE_INVALID;
            row->name=(const char *)data+name_at;
        }
        if (!r.ok || r.at-start!=lengths[side] || state_check(s)) return SHEET_LIFECYCLE_INVALID;
    }
    if (r.at!=bytes) return SHEET_LIFECYCLE_INVALID;
    SheetLifecycleStatus status=pair_check(sides,sides+1); if (status) return status;
    *a=sides[0]; *b=sides[1];
    *out=(SheetLifecycleInfo){bytes,sizeof(SheetLifecycle)+2*first->count*sizeof(SheetLifecycleEntry)+r.owned,first->count}; return SHEET_LIFECYCLE_OK;
}
SheetLifecycleStatus sheet_lifecycle_preflight(const void *data,size_t bytes,SheetLifecycleInfo *out) {
    if (!out) return SHEET_LIFECYCLE_INVALID;
    State a,b; SheetLifecycleInfo info; SheetLifecycleStatus status=parse(data,bytes,&a,&b,&info);
    if (!status) *out=info; return status;
}
SheetLifecycleStatus sheet_lifecycle_decode(const void *data,size_t bytes,size_t reserved,SheetLifecycle *out) {
    if (!out) return SHEET_LIFECYCLE_INVALID;
    State sides[2]; SheetLifecycleInfo info; SheetLifecycleStatus status=parse(data,bytes,sides,sides+1,&info); if (status) return status;
    if (info.owned>reserved) return SHEET_LIFECYCLE_LIMIT;
    SheetLifecycle next={0}; SheetLifecycleState *states[2]={&next.before,&next.after};
    for (unsigned side=0;side<2;side++) {
        State *s=sides+side; SheetLifecycleEntry *entries=dx_alloc(s->count*sizeof(*entries));
        *states[side]=(SheetLifecycleState){s->document,s->high_water,s->active,entries,s->count};
        for (size_t i=0;i<s->count;i++) {
            Row row=s->rows[i]; char *name=dx_alloc(row.bytes+1); memcpy(name,row.name,row.bytes); name[row.bytes]=0;
            entries[i]=(SheetLifecycleEntry){row.id,name,row.hidden};
        }
    }
    sheet_lifecycle_free(out); *out=next; return SHEET_LIFECYCLE_OK;
}
void sheet_lifecycle_packet_free(SheetLifecyclePacket *p) { if (p) { free(p->data); *p=(SheetLifecyclePacket){0}; } }
void sheet_lifecycle_free(SheetLifecycle *p) {
    if (!p) return;
    SheetLifecycleState *states[2]={&p->before,&p->after};
    for (unsigned side=0;side<2;side++) {
        const SheetLifecycleState *s=states[side];
        for (size_t i=0;i<s->count;i++) free((void *)s->sheets[i].name);
        free((void *)s->sheets);
    }
    *p=(SheetLifecycle){0};
}
SheetLifecycleStatus sheet_lifecycle_from_workbook(const Workbook *book,SheetLifecycleEntry storage[DX_MAX_SHEETS],SheetLifecycleState *out) {
    if (!book || !storage || !out || !book->sheets || !book->count || book->count>DX_MAX_SHEETS || book->active>=book->count) return SHEET_LIFECYCLE_INVALID;
    SheetLifecycleEntry entries[DX_MAX_SHEETS];
    for (size_t i=0;i<book->count;i++) entries[i]=(SheetLifecycleEntry){book->sheets[i].id,book->sheets[i].name,book->sheets[i].hidden};
    SheetLifecycleState view={book->id,book->sheet_high_water,book->sheets[book->active].id,entries,book->count}; State checked;
    SheetLifecycleStatus status=state_view(view,&checked,NULL,NULL); if (status) return status;
    memcpy(storage,entries,book->count*sizeof(*entries)); view.sheets=storage; *out=view; return SHEET_LIFECYCLE_OK;
}
SheetLifecycleStatus sheet_lifecycle_admit(SheetLifecycle view,SheetLifecycleState current,bool redo,SheetLifecycleState *target) {
    if (!target) return SHEET_LIFECYCLE_INVALID;
    State a,b,live; SheetLifecycleInfo info; SheetLifecycleStatus status=measure(view,&a,&b,&info); if (status) return status;
    status=state_view(current,&live,NULL,NULL); if (status) return status;
    if (live.high_water<a.high_water || !same_state(&live,redo ? &a : &b)) return SHEET_LIFECYCLE_STALE;
    SheetLifecycleState next=redo ? view.after : view.before; next.high_water=live.high_water;
    /* Navigation is not an authored edit. Keep the user's live active ID when
       the target still exposes it; a hiding transition uses captured fallback. */
    for (size_t i=0;i<next.count;i++) if (next.sheets[i].id==live.active && !next.sheets[i].hidden) { next.active=live.active; break; }
    *target=next; return SHEET_LIFECYCLE_OK;
}
