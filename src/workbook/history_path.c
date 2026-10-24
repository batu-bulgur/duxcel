#include "history_path.h"
WorkbookHistoryPathStatus workbook_history_path_check(const WorkbookHistoryPath *p) {
    if (!p || !(p->document_high|p->document_low) || !p->segment_count || !p->segments || (p->event_count && !p->events) ||
        p->origin>p->available || p->available>p->cursor || p->cursor>p->tail) return HISTORY_PATH_INVALID;
    uint64_t from=p->origin;
    for (size_t i=0;i<p->segment_count;i++) {
        const WorkbookHistorySegment *s=p->segments+i;
        if (s->lineage>p->lineage_high_water || s->from!=from || s->from>s->through || s->through>p->tail ||
            (i && s->lineage<=p->segments[i-1].lineage)) return HISTORY_PATH_SEQUENCE;
        from=s->through;
    }
    if (from!=p->tail) return HISTORY_PATH_SEQUENCE;
    if (p->event_count!=p->tail-p->available) return HISTORY_PATH_GAP;
    size_t segment=0; uint64_t position=p->available;
    for (size_t i=0;i<p->event_count;i++) {
        const WorkbookHistoryEventPosition *e=p->events+i;
        /* count equality above proves position < tail before incrementing. */
        position++;
        while (segment+1<p->segment_count && p->segments[segment].through<position) segment++;
        if (e->position!=position || e->lineage!=p->segments[segment].lineage ||
            e->operation>p->operation_high_water || (i && e->operation<=p->events[i-1].operation)) return HISTORY_PATH_SEQUENCE;
    }
    return HISTORY_PATH_OK;
}
/* Called only after path/binding validation. 1=on path, 0=checkpoint only,
   -1=impossible current path metadata. Does not infer identity from content. */
static int membership(const WorkbookHistoryPath *p,const WorkbookHistoryAnchor *a) {
    if (a->lineage>p->lineage_high_water) return -1;
    for (size_t i=0;i<p->segment_count;i++) if (a->lineage==p->segments[i].lineage) {
        if (a->boundary.position<p->segments[i].from) return -1;
        if (a->boundary.position<=p->segments[i].through) return 1;
        return i+1==p->segment_count ? -1 : 0;
    }
    return 0;
}
WorkbookHistoryPathStatus workbook_history_path_window(const WorkbookVersionIndex *v,
    const WorkbookHistoryAnchors *h,const WorkbookHistoryPath *p,WorkbookHistoryWindow *out) {
    if (!out) return HISTORY_PATH_INVALID;
    if (workbook_history_anchors_check(v,h)!=HISTORY_ANCHORS_OK) return HISTORY_PATH_ANCHORS;
    WorkbookHistoryPathStatus status=workbook_history_path_check(p);
    if (status!=HISTORY_PATH_OK) return status;
    if (p->document_high!=h->document_high || p->document_low!=h->document_low) return HISTORY_PATH_INVALID;
    WorkbookHistoryBoundary named[DX_VERSION_TAGS]; size_t count=0;
    WorkbookHistoryBoundary saved={0}; bool found=false;
    for (size_t i=0;i<h->count;i++) {
        int on=membership(p,h->items+i);
        if (on<0) return HISTORY_PATH_INVALID;
        if (h->items[i].boundary.anchor==h->saved_anchor && v->saved.present) {
            if (!on) return HISTORY_PATH_FORKED_SAVED;
            saved=h->items[i].boundary; found=true;
        } else if (on) named[count++]=h->items[i].boundary;
    }
    WorkbookHistoryWindowInput input={.named=named,.count=count,.has_saved=found,.saved=saved,
        .origin=p->origin,.available=p->available,.cursor=p->cursor,.tail=p->tail};
    switch (workbook_history_window(input,out)) {
    case HISTORY_WINDOW_OK: return HISTORY_PATH_OK;
    case HISTORY_WINDOW_GAP: return HISTORY_PATH_GAP;
    case HISTORY_WINDOW_LIMIT: return HISTORY_PATH_LIMIT;
    default: return HISTORY_PATH_INVALID;
    }
}
static uint64_t lesser(uint64_t a,uint64_t b) { return a<b ? a : b; }
static uint64_t greater(uint64_t a,uint64_t b) { return a>b ? a : b; }
static bool shared_prefix(const WorkbookHistoryPath *s,const WorkbookHistoryPath *c,uint64_t *fork) {
    size_t common=0;
    while (common<s->segment_count && common<c->segment_count && s->segments[common].lineage==c->segments[common].lineage) {
        if (s->segments[common].from!=c->segments[common].from) return false;
        uint64_t end=lesser(s->segments[common].through,c->segments[common].through);
        bool split=s->segments[common].through!=c->segments[common].through;
        *fork=end; common++; if (split) break;
    }
    if (!common) return false;
    size_t i=common,j=common;
    /* Monotonic IDs permit a linear intersection check. A branch cannot
       silently rejoin a later shared lineage with different ancestry. */
    while (i<s->segment_count && j<c->segment_count) {
        uint64_t a=s->segments[i].lineage,b=c->segments[j].lineage;
        if (a==b) return false;
        if (a<b) i++; else j++;
    }
    uint64_t start=greater(s->available,c->available);
    if (start<*fork) for (uint64_t at=start;at<*fork;at++) {
        const WorkbookHistoryEventPosition *a=s->events+(size_t)(at-s->available),*b=c->events+(size_t)(at-c->available);
        if (a->operation!=b->operation || a->lineage!=b->lineage) return false;
    }
    i=j=0;
    while (i<s->event_count && j<c->event_count) {
        const WorkbookHistoryEventPosition *a=s->events+i,*b=c->events+j;
        if (a->operation==b->operation) {
            if (a->position!=b->position || a->position>*fork || a->lineage!=b->lineage) return false;
            i++; j++;
        } else if (a->operation<b->operation) i++; else j++;
    }
    return true;
}
WorkbookHistoryPathStatus workbook_history_protect(const WorkbookVersionIndex *v,
    const WorkbookHistoryAnchors *h,const WorkbookHistoryPath *s,const WorkbookHistoryPath *c,WorkbookHistoryProtection *out) {
    if (!out) return HISTORY_PATH_INVALID;
    if (workbook_history_anchors_check(v,h)!=HISTORY_ANCHORS_OK) return HISTORY_PATH_ANCHORS;
    WorkbookHistoryProtection next={.has_saved=v->saved.present};
    if (!v->saved.present) {
        if (s) return HISTORY_PATH_INVALID;
        WorkbookHistoryWindow draft;
        WorkbookHistoryPathStatus status=workbook_history_path_window(v,h,c,&draft);
        if (status!=HISTORY_PATH_OK) return status;
        next.current_from=draft.from; next.current_cursor=draft.cursor; next.current_through=draft.through; next.shared_through=c->origin;
    } else {
        WorkbookHistoryPathStatus status=workbook_history_path_window(v,h,s,&next.saved);
        if (status!=HISTORY_PATH_OK) return status;
        status=workbook_history_path_check(c);
        if (status!=HISTORY_PATH_OK) return status;
        if (c->document_high!=h->document_high || c->document_low!=h->document_low || c->origin!=s->origin ||
            c->lineage_high_water<s->lineage_high_water || c->operation_high_water<s->operation_high_water) return HISTORY_PATH_INVALID;
        for (size_t i=0;i<h->count;i++) if (membership(c,h->items+i)<0) return HISTORY_PATH_INVALID;
        if (!shared_prefix(s,c,&next.shared_through)) return HISTORY_PATH_SEQUENCE;
        uint64_t from=lesser(next.saved.from,lesser(next.shared_through,c->cursor));
        if (from<s->available || from<c->available) return HISTORY_PATH_GAP;
        next.saved.from=from;
        next.current_from=from; next.current_cursor=c->cursor; next.current_through=c->tail;
    }
    *out=next; return HISTORY_PATH_OK;
}
