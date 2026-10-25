#include "range_history_slot.h"
static WorkbookRangeSlotStatus associated(WorkbookRangeHistorySlot slot) {
    WorkbookHistoryEventSlot view={slot.path,slot.index,slot.owner,slot.edit_session,slot.payload.event};
    WorkbookHistoryEventSlotStatus status=workbook_history_event_slot_check(view);
    return status==HISTORY_EVENT_SLOT_OK ? RANGE_SLOT_OK : status==HISTORY_EVENT_SLOT_PATH ? RANGE_SLOT_PATH :
        status==HISTORY_EVENT_SLOT_ASSOCIATION ? RANGE_SLOT_ASSOCIATION : RANGE_SLOT_INVALID;
}
WorkbookRangeSlotStatus workbook_range_slot_preflight(WorkbookRangeHistorySlot slot,WorkbookRangePayloadInfo *out) {
    if (!out) return RANGE_SLOT_INVALID;
    WorkbookRangeSlotStatus status=associated(slot); if (status!=RANGE_SLOT_OK) return status;
    WorkbookRangePayloadStatus payload=workbook_range_payload_preflight(slot.payload,out);
    return payload==RANGE_PAYLOAD_OK ? RANGE_SLOT_OK : payload==RANGE_PAYLOAD_UNSUPPORTED ? RANGE_SLOT_UNSUPPORTED : payload==RANGE_PAYLOAD_LIMIT ? RANGE_SLOT_LIMIT : RANGE_SLOT_INVALID;
}
WorkbookRangeSlotStatus workbook_range_slot_admit(WorkbookRangeHistorySlot slot,SourceDependencyLive live,size_t reserved,WorkbookRangeSlotAdmission *out) {
    if (!out) return RANGE_SLOT_INVALID;
    WorkbookRangeSlotStatus status=associated(slot); if (status!=RANGE_SLOT_OK) return status;
    WorkbookRangeSlotAdmission report={0}; report.compatibility=workbook_range_payload_admit(slot.payload,live,reserved,&report.observations);
    if (report.compatibility==SOURCE_DEPENDENCY_INVALID) return RANGE_SLOT_INVALID;
    if (report.compatibility==SOURCE_DEPENDENCY_UNSUPPORTED) return RANGE_SLOT_UNSUPPORTED;
    if (report.compatibility==SOURCE_DEPENDENCY_LIMIT) return RANGE_SLOT_LIMIT;
    *out=report; return RANGE_SLOT_OK;
}
