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
