#include "sheet_topology_replay.h"
#include "history.h"
#include <stdlib.h>
#include <string.h>
struct SheetTopologyReplayPlan {
    Workbook *book;
    const uint64_t *epoch_owner;
    uint64_t epoch,session,revision,branch,order,issued,high_water,sheet_session,cells_revision,format_revision,width_revision;
    WorkbookLifecyclePresence selection;
    const void *packet_data;
    size_t packet_bytes;
    SheetTopologyEvent event;
    SheetPresenceAsset *presence;
    bool redo;
};
static bool add(size_t *n,size_t v) { if (*n>SIZE_MAX-v) return false; *n+=v; return true; }
SheetTopologyReplayStatus sheet_topology_replay_measure(const SheetTopologyPacket *packet,SheetTopologyReplayMeasure *out) {
    if (!packet || !out) return SHEET_REPLAY_INVALID;
    SheetTopologyInfo info; SheetPresenceWireInfo presence;
    if (sheet_topology_preflight(packet->data,packet->bytes,&info)!=SHEET_TOPOLOGY_OK ||
        sheet_presence_preflight((unsigned char *)packet->data+packet->bytes-info.presence_bytes,info.presence_bytes,&presence)!=SHEET_PRESENCE_OK) return SHEET_REPLAY_INVALID;
    SheetTopologyReplayMeasure m={sizeof(SheetTopologyReplayPlan),0};
    if (!add(&m.owned,info.owned) || !add(&m.owned,presence.owned)) return SHEET_REPLAY_LIMIT;
    m.staging=m.owned;
    if (!add(&m.staging,info.validation_scratch) || !add(&m.staging,presence.validation_scratch)) return SHEET_REPLAY_LIMIT;
    *out=m; return SHEET_REPLAY_READY;
}
static bool catalog(const Workbook *b,SheetLifecycleState expected,uint64_t high) {
    SheetLifecycleEntry rows[DX_MAX_SHEETS]; SheetLifecycleState live;
    if (sheet_lifecycle_from_workbook(b,rows,&live)!=SHEET_LIFECYCLE_OK || live.count!=expected.count || live.high_water<high) return false;
    for (size_t i=0;i<live.count;i++) if (rows[i].id!=expected.sheets[i].id || rows[i].hidden!=expected.sheets[i].hidden || strcmp(rows[i].name,expected.sheets[i].name)) return false;
    return true;
}
static SheetTopologyReplayStatus selection(SheetTopologyReplayContext c,const SheetTopologyReplayPlan *p,WorkbookLifecyclePresence *out) {
    if (!c.book || !c.epoch || !c.primary || !workbook_identity_valid(c.book) || !c.book->edit_session_id || !c.book->count || c.book->active>=c.book->count) return SHEET_REPLAY_INVALID;
    if (!c.ready || c.read_only) return SHEET_REPLAY_PROTECTED;
    WorkbookHistoryResult selected=workbook_history_peek(c.book,p->redo,NULL);
    if (selected.status==WORKBOOK_HISTORY_LIMIT) return SHEET_REPLAY_LIMIT;
    if (selected.status==WORKBOOK_HISTORY_INVALID) return SHEET_REPLAY_INVALID;
    if (selected.status!=WORKBOOK_HISTORY_READY || !selected.lifecycle || selected.sheet_id!=p->event.sheet || selected.order!=p->order || c.primary_sheet!=p->event.sheet) return SHEET_REPLAY_STALE;
    if (!workbook_lifecycle_presence(c.book,p->redo,out)) return SHEET_REPLAY_INVALID;
    bool remove=p->event.operation==SHEET_TOPOLOGY_REMOVE;
    if ((remove ? out->operation!=SHEET_DELETE : out->operation!=SHEET_CREATE && out->operation!=SHEET_DUPLICATE) || out->detached!=(remove ? !p->redo : p->redo)) return SHEET_REPLAY_STALE;
    uint64_t latest=workbook_history_latest_record(c.book);
    if (c.book->history_branch!=latest) return SHEET_REPLAY_STALE;
    /* Detached session constituents remain real owners. Validate their counts
       before restoration without copying/mutating them or inventing actions. */
    Workbook one=*c.book; one.sheets=(Worksheet *)out->worksheet; one.count=1; one.active=0; one.lifecycle=NULL;
    if (workbook_history_peek(&one,false,NULL).status==WORKBOOK_HISTORY_INVALID || workbook_history_peek(&one,true,NULL).status==WORKBOOK_HISTORY_INVALID) return SHEET_REPLAY_INVALID;
    return SHEET_REPLAY_READY;
}
SheetTopologyReplayStatus sheet_topology_replay_check(SheetTopologyReplayContext c,const SheetTopologyReplayPlan *p) {
    if (!p) return SHEET_REPLAY_INVALID; WorkbookLifecyclePresence live;
    SheetTopologyReplayStatus status=selection(c,p,&live); if (status!=SHEET_REPLAY_READY) return status;
    if (c.book!=p->book || c.epoch!=p->epoch_owner || *c.epoch!=p->epoch || c.book->edit_session_id!=p->session || c.book->command_revision!=p->revision ||
        c.book->history_branch!=p->branch || worksheet_history_high_water()!=p->issued || c.book->sheet_high_water!=p->high_water || live.owner!=p->selection.owner ||
        live.packet!=p->selection.packet || live.order!=p->selection.order || live.position!=p->selection.position || live.active!=p->selection.active || live.detached!=p->selection.detached ||
        (live.detached && live.worksheet!=p->selection.worksheet)) return SHEET_REPLAY_STALE;
    if (live.packet) {
        if (live.packet->data!=p->packet_data || live.packet->bytes!=p->packet_bytes) return SHEET_REPLAY_STALE;
        bool same=false; SheetTopologyStatus packet=sheet_topology_packet_matches(sheet_topology_view(&p->event),live.packet,&same);
        if (packet!=SHEET_TOPOLOGY_OK) return SHEET_REPLAY_INVALID;
        if (!same) return SHEET_REPLAY_STALE;
    }
    const Worksheet *s=live.worksheet;
    if (s->edit_session_id!=p->sheet_session || s->cells.revision!=p->cells_revision || s->formats.revision!=p->format_revision || s->width_revision!=p->width_revision) return SHEET_REPLAY_STALE;
    if (!catalog(c.book,p->redo ? p->event.before : p->event.after,p->event.after.high_water)) return SHEET_REPLAY_STALE;
    SheetPresenceView expected=sheet_presence_view(p->presence); SheetPresenceRefinement refined={0};
    SheetPresenceView actual={c.book->id,expected.sheet_high_water,s,c.primary};
    if (sheet_presence_matches_refined(p->presence,actual,&refined)!=SHEET_PRESENCE_OK) return SHEET_REPLAY_INVALID;
    if (!refined.matches) return SHEET_REPLAY_STALE;
    actual.primary=&s->analysis;
    if (sheet_presence_matches_refined(p->presence,actual,&refined)!=SHEET_PRESENCE_OK) return SHEET_REPLAY_INVALID;
    return refined.matches ? SHEET_REPLAY_READY : SHEET_REPLAY_PROTECTED;
}
SheetTopologyReplayStatus sheet_topology_replay_prepare(SheetTopologyReplayContext c,const SheetTopologyPacket *packet,bool redo,uint64_t order,SheetTopologyReplayPlan **out,char **error) {
    if (!out || *out || !order) return SHEET_REPLAY_INVALID;
    SheetTopologyReplayMeasure m; SheetTopologyReplayStatus status=sheet_topology_replay_measure(packet,&m); if (status!=SHEET_REPLAY_READY) return status;
    if (m.staging>c.reserved) return SHEET_REPLAY_LIMIT;
    SheetTopologyReplayPlan *p=dx_alloc(sizeof(*p)); p->redo=redo; p->order=order;
    SheetTopologyInfo info; sheet_topology_preflight(packet->data,packet->bytes,&info);
    SheetPresenceWireInfo wire; sheet_presence_preflight((unsigned char *)packet->data+packet->bytes-info.presence_bytes,info.presence_bytes,&wire);
    if (sheet_topology_decode(packet->data,packet->bytes,info.owned+info.validation_scratch,&p->event)!=SHEET_TOPOLOGY_OK ||
        sheet_presence_decode(p->event.presence.data,p->event.presence.bytes,wire.owned+wire.validation_scratch,&p->presence,error)!=SHEET_PRESENCE_OK) { sheet_topology_replay_free(&p); return SHEET_REPLAY_INVALID; }
    WorkbookLifecyclePresence selected; status=selection(c,p,&selected);
    if (status!=SHEET_REPLAY_READY) { sheet_topology_replay_free(&p); return status; }
    p->book=c.book; p->epoch_owner=c.epoch; p->epoch=*c.epoch; p->session=c.book->edit_session_id; p->revision=c.book->command_revision; p->branch=c.book->history_branch; p->issued=worksheet_history_high_water(); p->high_water=c.book->sheet_high_water; p->selection=selected;
    p->packet_data=selected.packet ? selected.packet->data : NULL; p->packet_bytes=selected.packet ? selected.packet->bytes : 0;
    p->sheet_session=selected.worksheet->edit_session_id; p->cells_revision=selected.worksheet->cells.revision; p->format_revision=selected.worksheet->formats.revision; p->width_revision=selected.worksheet->width_revision;
    status=sheet_topology_replay_check(c,p); if (status!=SHEET_REPLAY_READY) { sheet_topology_replay_free(&p); return status; }
    *out=p; return SHEET_REPLAY_READY;
}
static uint64_t target_active(const SheetTopologyReplayPlan *p) {
    SheetLifecycleState target=p->redo ? p->event.after : p->event.before;
    for (size_t i=0;i<target.count;i++) if (target.sheets[i].id==p->selection.active && !target.sheets[i].hidden) return p->selection.active;
    for (size_t i=0;i<target.count;i++) if (!target.sheets[i].hidden) return target.sheets[i].id;
    return 0;
}
static bool navigation_same(const SheetView *a,const SheetView *b) {
    return a->row==b->row && a->anchor_row==b->anchor_row && a->end_row==b->end_row && a->top==b->top &&
        a->col==b->col && a->anchor_col==b->anchor_col && a->end_col==b->end_col && a->selection==b->selection &&
        !memcmp(&a->horizontal,&b->horizontal,sizeof(float)) && !memcmp(&a->fraction,&b->fraction,sizeof(double)) && !memcmp(&a->canvas_zoom,&b->canvas_zoom,sizeof(float));
}
SheetTopologyReplayStatus sheet_topology_replay_apply(SheetTopologyReplayContext c,SheetTopologyReplayPlan **owned,char **error) {
    if (!owned || !*owned) return SHEET_REPLAY_INVALID; SheetTopologyReplayPlan *p=*owned;
    SheetTopologyReplayStatus status=sheet_topology_replay_check(c,p); if (status!=SHEET_REPLAY_READY) return status;
    size_t staging=workbook_lifecycle_presence_staging(c.book,p->redo);
    if (staging==SIZE_MAX || staging>c.adoption_reserved) return SHEET_REPLAY_LIMIT;
    uint64_t active=target_active(p); WorkbookLifecyclePresence current;
    if (!workbook_lifecycle_presence(c.book,p->redo,&current)) return SHEET_REPLAY_STALE;
    SheetView navigation=current.worksheet->view;
    WorkbookHistoryResult result=workbook_history_step(c.book,p->redo,error);
    if (result.status!=WORKBOOK_HISTORY_APPLIED) return result.status==WORKBOOK_HISTORY_LIMIT ? SHEET_REPLAY_LIMIT : SHEET_REPLAY_INVALID;
    /* Synchronous model transfer; all checks here are allocation-free and no
       source activation, callback, OS access or publication intervenes. */
    SheetLifecycleState target=p->redo ? p->event.after : p->event.before; WorkbookLifecyclePresence inverse;
    if (!catalog(c.book,target,p->high_water) || c.book->sheet_high_water!=p->high_water || c.book->command_revision!=p->revision+1 || c.book->edit_session_id!=p->session ||
        c.book->history_branch!=p->branch || worksheet_history_high_water()!=p->issued || c.book->sheets[c.book->active].id!=active ||
        !workbook_lifecycle_presence(c.book,!p->redo,&inverse) || inverse.sheet!=p->event.sheet || inverse.order!=p->order || inverse.detached==p->selection.detached || !navigation_same(&navigation,&inverse.worksheet->view)) abort();
    if (!!inverse.packet!=!!p->selection.packet) abort();
    if (inverse.packet) {
        bool packet_same=false;
        if (inverse.packet->data!=p->packet_data || inverse.packet->bytes!=p->packet_bytes ||
            sheet_topology_packet_matches(sheet_topology_view(&p->event),inverse.packet,&packet_same)!=SHEET_TOPOLOGY_OK || !packet_same) abort();
    }
    SheetPresenceView expected=sheet_presence_view(p->presence); SheetPresenceRefinement refined={0};
    if (sheet_presence_matches_refined(p->presence,(SheetPresenceView){c.book->id,expected.sheet_high_water,inverse.worksheet,&inverse.worksheet->analysis},&refined)!=SHEET_PRESENCE_OK || !refined.matches) abort();
    sheet_topology_replay_free(owned); return SHEET_REPLAY_APPLIED;
}
void sheet_topology_replay_free(SheetTopologyReplayPlan **owned) {
    if (!owned || !*owned) return; SheetTopologyReplayPlan *p=*owned;
    sheet_topology_free(&p->event); sheet_presence_free(&p->presence); free(p); *owned=NULL;
}
