#include "history_window.h"
#include <string.h>
static bool same_boundary(WorkbookHistoryBoundary a,WorkbookHistoryBoundary b) {
    return a.anchor==b.anchor && a.position==b.position && !memcmp(a.root.bytes,b.root.bytes,DX_VERSION_KEY_BYTES);
}
WorkbookHistoryWindowStatus workbook_history_window(WorkbookHistoryWindowInput in,WorkbookHistoryWindow *out) {
    if (!out || in.origin>in.available || in.available>in.cursor || in.cursor>in.tail || (in.count && !in.named) || (!in.has_saved && in.count)) return HISTORY_WINDOW_INVALID;
    if (in.count>DX_VERSION_TAGS) return HISTORY_WINDOW_LIMIT;
    if (in.has_saved && (in.saved.position<in.origin || in.saved.position>in.tail)) return HISTORY_WINDOW_INVALID;
    for (size_t i=0;i<in.count;i++) {
        const WorkbookHistoryBoundary *b=in.named+i;
        if (b->position<in.origin || b->position>in.tail || b->anchor>in.saved.anchor || (b->anchor==in.saved.anchor && !same_boundary(*b,in.saved))) return HISTORY_WINDOW_INVALID;
        for (size_t j=0;j<i;j++) if (b->anchor==in.named[j].anchor && !same_boundary(*b,in.named[j])) return HISTORY_WINDOW_INVALID;
    }
    WorkbookHistoryWindow next={.from=in.origin,.cursor=in.cursor,.through=in.tail};
    if (in.has_saved) {
        next.states[0]=in.saved; next.count=1;
        /* At most two predecessor searches. Version-boundary chronology is
           separate from the reversible operation position: Save after Undo
           can move backwards on that path while retaining current Redo. */
        while (next.count<DX_HISTORY_WINDOW_STATES) {
            const WorkbookHistoryBoundary *previous=NULL; uint64_t before=next.states[0].anchor;
            for (size_t i=0;i<in.count;i++) if (in.named[i].anchor<before && (!previous || in.named[i].anchor>previous->anchor)) previous=in.named+i;
            if (!previous) break;
            memmove(next.states+1,next.states,next.count*sizeof(*next.states)); next.states[0]=*previous; next.count++;
        }
        if (next.count==DX_HISTORY_WINDOW_STATES) {
            next.from=next.states[0].position;
            for (size_t i=1;i<next.count;i++) if (next.states[i].position<next.from) next.from=next.states[i].position;
        }
    }
    if (in.cursor<next.from) next.from=in.cursor;
    if (next.from<in.available) return HISTORY_WINDOW_GAP;
    *out=next; return HISTORY_WINDOW_OK;
}
