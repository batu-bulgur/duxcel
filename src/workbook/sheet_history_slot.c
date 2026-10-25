#include "sheet_history_slot.h"
static bool owner_equal(WorkbookVersionOwner a,WorkbookVersionOwner b) {
    return a.document_high==b.document_high && a.document_low==b.document_low && a.session==b.session && a.mode==b.mode;
}
static WorkbookSheetSlotStatus payload_status(WorkbookSheetPayloadStatus status) {
    return status==SHEET_PAYLOAD_OK ? SHEET_SLOT_OK : status==SHEET_PAYLOAD_UNSUPPORTED ? SHEET_SLOT_UNSUPPORTED : status==SHEET_PAYLOAD_LIMIT ? SHEET_SLOT_LIMIT : SHEET_SLOT_INVALID;
}
static WorkbookSheetSlotStatus association(WorkbookSheetHistorySlot slot) {
    if (!slot.path || !slot.payload.event || !slot.edit_session || !slot.owner.session || slot.owner.mode!=VERSION_WORKING_HEAD) return SHEET_SLOT_INVALID;
    if (workbook_history_path_check(slot.path)!=HISTORY_PATH_OK) return SHEET_SLOT_PATH;
    if (slot.index>=slot.path->event_count) return SHEET_SLOT_ASSOCIATION;
    const WorkbookHistoryEventInfo *frame=slot.payload.event;
    const WorkbookHistoryEventPosition *event=slot.path->events+slot.index;
    if (slot.owner.document_high!=slot.path->document_high || slot.owner.document_low!=slot.path->document_low ||
        !owner_equal(slot.owner,frame->owner) || slot.edit_session!=frame->edit_session || frame->transition!=event->operation ||
        frame->lineage!=event->lineage || frame->position!=event->position || !event->position || frame->previous_position!=event->position-1) return SHEET_SLOT_ASSOCIATION;
    size_t segment=0;
    while (segment+1<slot.path->segment_count && slot.path->segments[segment].through<event->position) segment++;
    const WorkbookHistorySegment *current=slot.path->segments+segment;
    bool first=frame->previous_position==current->from;
    if (frame->forked) {
        if (!segment || !first || frame->parent_lineage!=slot.path->segments[segment-1].lineage) return SHEET_SLOT_ASSOCIATION;
    } else if (segment && first) return SHEET_SLOT_ASSOCIATION;
    return SHEET_SLOT_OK;
}
WorkbookSheetSlotStatus workbook_sheet_slot_preflight(WorkbookSheetHistorySlot slot,WorkbookSheetPayloadInfo *out) {
    if (!out) return SHEET_SLOT_INVALID; WorkbookSheetSlotStatus status=association(slot); if (status) return status;
    return payload_status(workbook_sheet_payload_preflight(slot.payload,out));
}
WorkbookSheetSlotStatus workbook_sheet_slot_validate(WorkbookSheetHistorySlot slot,size_t reserved,WorkbookSheetPayloadInfo *out) {
    if (!out) return SHEET_SLOT_INVALID; WorkbookSheetSlotStatus status=association(slot); if (status) return status;
    return payload_status(workbook_sheet_payload_validate(slot.payload,reserved,out));
}
WorkbookSheetSlotStatus workbook_sheet_slot_decode(WorkbookSheetHistorySlot slot,size_t reserved,WorkbookSheetPayload **out) {
    if (!out) return SHEET_SLOT_INVALID; WorkbookSheetSlotStatus status=association(slot); if (status) return status;
    return payload_status(workbook_sheet_payload_decode(slot.payload,reserved,out));
}
