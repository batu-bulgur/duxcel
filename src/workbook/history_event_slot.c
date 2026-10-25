#include "history_event_slot.h"
static bool owner_equal(WorkbookVersionOwner a,WorkbookVersionOwner b) {
    return a.document_high==b.document_high && a.document_low==b.document_low && a.session==b.session && a.mode==b.mode;
}
WorkbookHistoryEventSlotStatus workbook_history_event_slot_check(WorkbookHistoryEventSlot slot) {
    if (!slot.path || !slot.event || !slot.edit_session || !slot.owner.session || slot.owner.mode!=VERSION_WORKING_HEAD) return HISTORY_EVENT_SLOT_INVALID;
    if (workbook_history_path_check(slot.path)!=HISTORY_PATH_OK) return HISTORY_EVENT_SLOT_PATH;
    if (slot.index>=slot.path->event_count) return HISTORY_EVENT_SLOT_ASSOCIATION;
    const WorkbookHistoryEventInfo *frame=slot.event;
    const WorkbookHistoryEventPosition *event=slot.path->events+slot.index;
    if (slot.owner.document_high!=slot.path->document_high || slot.owner.document_low!=slot.path->document_low ||
        !owner_equal(slot.owner,frame->owner) || slot.edit_session!=frame->edit_session || frame->transition!=event->operation ||
        frame->lineage!=event->lineage || frame->position!=event->position || !event->position || frame->previous_position!=event->position-1) return HISTORY_EVENT_SLOT_ASSOCIATION;
    size_t segment=0;
    while (segment+1<slot.path->segment_count && slot.path->segments[segment].through<event->position) segment++;
    const WorkbookHistorySegment *current=slot.path->segments+segment;
    bool first=frame->previous_position==current->from;
    if (frame->forked) {
        if (!segment || !first || frame->parent_lineage!=slot.path->segments[segment-1].lineage) return HISTORY_EVENT_SLOT_ASSOCIATION;
    } else if (segment && first) return HISTORY_EVENT_SLOT_ASSOCIATION;
    return HISTORY_EVENT_SLOT_OK;
}
