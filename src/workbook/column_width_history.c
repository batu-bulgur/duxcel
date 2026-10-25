#include "column_width_history.h"
#include <stdlib.h>
#include <string.h>
struct ColumnWidthHistory {
    ColumnWidthPacket undo[DX_COLUMN_WIDTH_HISTORY_ENTRIES],redo[DX_COLUMN_WIDTH_HISTORY_ENTRIES];
    size_t undo_count,redo_count,bytes;
    uint64_t generation;
    ColumnWidthHistoryOptions limits;
};
struct ColumnWidthPlan {
    ColumnWidthState *owner;
    uint64_t *revision_owner;
    ColumnWidthHistory *history;
    uint64_t revision,generation;
    size_t evict,owned;
    unsigned mode; /* 0=new,1=Undo,2=Redo. */
    ColumnWidthPacket packet;
    ColumnWidthState target;
};
size_t column_width_history_allocation(void) { return sizeof(ColumnWidthHistory); }
ColumnWidthStatus column_width_history_new(ColumnWidthHistoryOptions options,size_t reserved,ColumnWidthHistory **out) {
    if (!out || *out || !options.entries || options.entries>DX_COLUMN_WIDTH_HISTORY_ENTRIES || !options.bytes || options.bytes>DX_COLUMN_WIDTH_HISTORY_BYTES) return COLUMN_WIDTH_INVALID;
    if (reserved<sizeof(ColumnWidthHistory)) return COLUMN_WIDTH_LIMIT;
    ColumnWidthHistory *h=dx_alloc(sizeof(*h)); *h=(ColumnWidthHistory){.limits=options}; *out=h; return COLUMN_WIDTH_OK;
}
ColumnWidthHistoryInfo column_width_history_info(const ColumnWidthHistory *h) { return h ? (ColumnWidthHistoryInfo){h->undo_count,h->redo_count,h->bytes,h->generation} : (ColumnWidthHistoryInfo){0}; }
static void packet_free(ColumnWidthPacket *p) { free(p->data); *p=(ColumnWidthPacket){0}; }
void column_width_history_free(ColumnWidthHistory **owned) {
    if (!owned || !*owned) return; ColumnWidthHistory *h=*owned;
    for (size_t i=0;i<h->undo_count;i++) packet_free(h->undo+i);
    for (size_t i=0;i<h->redo_count;i++) packet_free(h->redo+i);
    free(h); *owned=NULL;
}
static ColumnWidthStatus prune_measure(const ColumnWidthHistory *h,size_t undo,size_t redo,size_t *retained) {
    if (!h || undo>h->undo_count || redo>h->redo_count) return COLUMN_WIDTH_INVALID;
    if (undo==h->undo_count && redo==h->redo_count) return COLUMN_WIDTH_NOOP;
    size_t bytes=0;
    for (size_t i=h->undo_count-undo;i<h->undo_count;i++) bytes+=h->undo[i].bytes;
    for (size_t i=h->redo_count-redo;i<h->redo_count;i++) bytes+=h->redo[i].bytes;
    *retained=bytes; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_history_prune_prepare(const ColumnWidthHistory *h,size_t undo,size_t redo,ColumnWidthPrune *out) {
    if (!out) return COLUMN_WIDTH_INVALID;
    size_t retained=0; ColumnWidthStatus status=prune_measure(h,undo,redo,&retained); if (status) return status;
    *out=(ColumnWidthPrune){h,column_width_history_info(h),undo,redo,h->bytes-retained,retained}; return COLUMN_WIDTH_OK;
}
static void prune_stack(ColumnWidthPacket *stack,size_t *count,size_t keep) {
    size_t dropped=*count-keep;
    for (size_t i=0;i<dropped;i++) packet_free(stack+i);
    memmove(stack,stack+dropped,keep*sizeof(*stack)); memset(stack+keep,0,dropped*sizeof(*stack)); *count=keep;
}
ColumnWidthStatus column_width_history_prune_commit(ColumnWidthHistory *h,const ColumnWidthPrune *p) {
    if (!h || !p) return COLUMN_WIDTH_INVALID;
    if (p->owner!=h || p->before.generation!=h->generation || p->before.undo!=h->undo_count || p->before.redo!=h->redo_count || p->before.owned_bytes!=h->bytes) return COLUMN_WIDTH_STALE;
    size_t retained=0; ColumnWidthStatus status=prune_measure(h,p->undo_keep,p->redo_keep,&retained); if (status) return status;
    if (p->retained_bytes!=retained || p->released_bytes!=h->bytes-retained) return COLUMN_WIDTH_INVALID;
    /* All bounds, clocks and accounting are proved before any release. */
    prune_stack(h->undo,&h->undo_count,p->undo_keep); prune_stack(h->redo,&h->redo_count,p->redo_keep);
    /* Retention releases ownership even when edit generation is exhausted.
       No edit/replay plan can be prepared at UINT64_MAX. Older plans mismatch
       that clock; prune plans additionally fence exact counts/bytes, and every
       changed prune strictly reduces at least one count. Saturation cannot
       revive a ticket, wrap a clock or leave abandoned Redo owned. */
    h->bytes=retained; if (h->generation<UINT64_MAX) ++h->generation; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_history_trim(ColumnWidthHistory *h,size_t undo,size_t redo) {
    ColumnWidthPrune plan; ColumnWidthStatus status=column_width_history_prune_prepare(h,undo,redo,&plan);
    return status ? status : column_width_history_prune_commit(h,&plan);
}
ColumnWidthStatus column_width_history_clear_redo(ColumnWidthHistory *h) { return h ? column_width_history_trim(h,h->undo_count,0) : COLUMN_WIDTH_INVALID; }
static bool context_valid(ColumnWidthContext c) { return c.current && c.revision && c.history && column_width_valid(column_width_view(c.current)); }
static ColumnWidthStatus room(ColumnWidthContext c,size_t packet,size_t target,size_t *owned) {
    if (*c.revision==UINT64_MAX || c.history->generation==UINT64_MAX) return COLUMN_WIDTH_LIMIT;
    *owned=sizeof(ColumnWidthPlan)+packet+target*sizeof(ColumnWidthEntry);
    return *owned>c.staging_reserved ? COLUMN_WIDTH_LIMIT : COLUMN_WIDTH_OK;
}
static ColumnWidthPlan *plan_new(ColumnWidthContext c,size_t bytes,size_t owned,unsigned mode) {
    ColumnWidthPlan *p=dx_alloc(sizeof(*p)); *p=(ColumnWidthPlan){.owner=c.current,.revision_owner=c.revision,.history=c.history,
        .revision=*c.revision,.generation=c.history->generation,.owned=owned,.mode=mode,.packet={.data=dx_alloc(bytes),.bytes=bytes}};
    return p;
}
ColumnWidthStatus column_width_history_prepare(ColumnWidthContext c,ColumnWidthView after,ColumnWidthPlan **out) {
    if (!out || *out || !context_valid(c)) return COLUMN_WIDTH_INVALID;
    ColumnWidthView before=column_width_view(c.current); size_t bytes=0;
    ColumnWidthStatus status=column_width_delta_size(before,after,&bytes); if (status) return status;
    ColumnWidthHistory *h=c.history;
    const ColumnWidthPacket *prior=h->undo_count ? h->undo+h->undo_count-1 : h->redo_count ? h->redo+h->redo_count-1 : NULL;
    if (prior && !column_width_matches(prior->data,prior->bytes,h->undo_count!=0,before)) return COLUMN_WIDTH_STALE;
    if (bytes>h->limits.bytes) return COLUMN_WIDTH_LIMIT;
    size_t owned=0; status=room(c,bytes,after.count,&owned); if (status) return status;
    /* Determine prospective eviction without touching either retained branch. */
    size_t retained=0,evict=0; for (size_t i=0;i<h->undo_count;i++) retained+=h->undo[i].bytes;
    while (h->undo_count-evict>=h->limits.entries || retained>h->limits.bytes-bytes) retained-=h->undo[evict++].bytes;
    ColumnWidthPlan *p=plan_new(c,bytes,owned,0); p->evict=evict; size_t written=0;
    status=column_width_encode(before,after,p->packet.data,bytes,&written);
    if (!status) status=column_width_copy(after,after.count*sizeof(ColumnWidthEntry),&p->target);
    if (status) { column_width_plan_free(&p); return status; }
    *out=p; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_history_replay(ColumnWidthContext c,bool redo,ColumnWidthPlan **out) {
    if (!out || *out || !context_valid(c)) return COLUMN_WIDTH_INVALID;
    ColumnWidthHistory *h=c.history; size_t count=redo ? h->redo_count : h->undo_count;
    if (!count) return COLUMN_WIDTH_EMPTY;
    const ColumnWidthPacket *event=(redo ? h->redo : h->undo)+count-1; ColumnWidthInfo info;
    ColumnWidthStatus status=column_width_preflight(event->data,event->bytes,&info); if (status) return status;
    if (!column_width_matches(event->data,event->bytes,!redo,column_width_view(c.current))) return COLUMN_WIDTH_STALE;
    size_t owned=0; status=room(c,event->bytes,redo ? info.after_count : info.before_count,&owned); if (status) return status;
    ColumnWidthPlan *p=plan_new(c,event->bytes,owned,redo ? 2 : 1); memcpy(p->packet.data,event->data,event->bytes);
    status=column_width_extract(event->data,event->bytes,redo,c.staging_reserved,&p->target);
    if (status) { column_width_plan_free(&p); return status; }
    *out=p; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_plan_history(const ColumnWidthPlan *p,ColumnWidthHistoryInfo *out) {
    if (!p || !out) return COLUMN_WIDTH_INVALID;
    const ColumnWidthHistory *h=p->history;
    if (p->generation!=h->generation || p->revision!=*p->revision_owner ||
        !column_width_matches(p->packet.data,p->packet.bytes,p->mode==1,column_width_view(p->owner))) return COLUMN_WIDTH_STALE;
    ColumnWidthHistoryInfo next=column_width_history_info(h); ++next.generation;
    if (!p->mode) {
        next.undo=h->undo_count-p->evict+1; next.redo=0; next.owned_bytes=p->packet.bytes;
        for (size_t i=p->evict;i<h->undo_count;i++) next.owned_bytes+=h->undo[i].bytes;
    } else if (p->mode==1) { --next.undo; ++next.redo; }
    else { ++next.undo; --next.redo; }
    *out=next; return COLUMN_WIDTH_OK;
}
ColumnWidthStatus column_width_plan_retain_undo(ColumnWidthPlan *p,size_t keep) {
    if (!p || p->mode) return COLUMN_WIDTH_INVALID;
    ColumnWidthHistoryInfo next; ColumnWidthStatus status=column_width_plan_history(p,&next); if (status) return status;
    if (!keep || keep>next.undo) return COLUMN_WIDTH_INVALID;
    if (keep==next.undo) return COLUMN_WIDTH_NOOP;
    p->evict+=next.undo-keep; return COLUMN_WIDTH_OK;
}
size_t column_width_plan_owned(const ColumnWidthPlan *p) { return p ? p->owned : 0; }
ColumnWidthView column_width_plan_target(const ColumnWidthPlan *p) { return p ? column_width_view(&p->target) : (ColumnWidthView){0}; }
const ColumnWidthPacket *column_width_plan_packet(const ColumnWidthPlan *p) { return p ? &p->packet : NULL; }
void column_width_plan_free(ColumnWidthPlan **owned) {
    if (!owned || !*owned) return; ColumnWidthPlan *p=*owned; packet_free(&p->packet); column_width_free(&p->target); free(p); *owned=NULL;
}
ColumnWidthStatus column_width_history_commit(ColumnWidthContext c,ColumnWidthPlan **owned) {
    if (!owned || !*owned || !context_valid(c)) return COLUMN_WIDTH_INVALID;
    ColumnWidthPlan *p=*owned; ColumnWidthHistory *h=c.history;
    if (p->owner!=c.current || p->revision_owner!=c.revision || p->history!=h || p->generation!=h->generation || p->revision!=*c.revision ||
        !column_width_matches(p->packet.data,p->packet.bytes,p->mode==1,column_width_view(c.current))) return COLUMN_WIDTH_STALE;
    if (*c.revision==UINT64_MAX || h->generation==UINT64_MAX) return COLUMN_WIDTH_LIMIT;
    /* All validation and allocation are complete. No failure/callback below. */
    if (!p->mode) {
        for (size_t i=0;i<h->redo_count;i++) { h->bytes-=h->redo[i].bytes; packet_free(h->redo+i); } h->redo_count=0;
        for (size_t i=0;i<p->evict;i++) { h->bytes-=h->undo[i].bytes; packet_free(h->undo+i); }
        if (p->evict) { memmove(h->undo,h->undo+p->evict,(h->undo_count-p->evict)*sizeof(*h->undo)); h->undo_count-=p->evict; }
        h->undo[h->undo_count++]=p->packet; h->bytes+=p->packet.bytes; p->packet=(ColumnWidthPacket){0};
    } else if (p->mode==1) { h->redo[h->redo_count++]=h->undo[--h->undo_count]; h->undo[h->undo_count]=(ColumnWidthPacket){0}; }
    else { h->undo[h->undo_count++]=h->redo[--h->redo_count]; h->redo[h->redo_count]=(ColumnWidthPacket){0}; }
    column_width_free(c.current); *c.current=p->target; p->target=(ColumnWidthState){0};
    ++*c.revision; ++h->generation; column_width_plan_free(owned); return COLUMN_WIDTH_OK;
}

#ifdef DUXCEL_COLUMN_WIDTH_HISTORY_TEST
bool column_width_history_test_generation(ColumnWidthHistory *h,uint64_t generation) {
    if (!h || generation<h->generation) return false; h->generation=generation; return true;
}
#endif
