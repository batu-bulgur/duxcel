#include "sheet_history_slot.h"
#include "history_event_slot.h"
static WorkbookSheetSlotStatus payload_status(WorkbookSheetPayloadStatus status) {
    return status==SHEET_PAYLOAD_OK ? SHEET_SLOT_OK : status==SHEET_PAYLOAD_UNSUPPORTED ? SHEET_SLOT_UNSUPPORTED : status==SHEET_PAYLOAD_LIMIT ? SHEET_SLOT_LIMIT : SHEET_SLOT_INVALID;
}
static WorkbookSheetSlotStatus association(WorkbookSheetHistorySlot slot) {
    WorkbookHistoryEventSlotStatus status=workbook_history_event_slot_check((WorkbookHistoryEventSlot){slot.path,slot.index,slot.owner,slot.edit_session,slot.payload.event});
    return status==HISTORY_EVENT_SLOT_OK ? SHEET_SLOT_OK : status==HISTORY_EVENT_SLOT_INVALID ? SHEET_SLOT_INVALID :
        status==HISTORY_EVENT_SLOT_PATH ? SHEET_SLOT_PATH : SHEET_SLOT_ASSOCIATION;
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
