#include "source_history_slot.h"
static WorkbookSourceSlotStatus association(WorkbookSourceHistorySlot slot) {
    WorkbookHistoryEventSlotStatus s=workbook_history_event_slot_check((WorkbookHistoryEventSlot){slot.path,slot.index,slot.owner,slot.edit_session,slot.payload.event});
    switch (s) {
    case HISTORY_EVENT_SLOT_OK: return SOURCE_SLOT_OK;
    case HISTORY_EVENT_SLOT_PATH: return SOURCE_SLOT_PATH;
    case HISTORY_EVENT_SLOT_ASSOCIATION: return SOURCE_SLOT_ASSOCIATION;
    default: return SOURCE_SLOT_INVALID;
    }
}
WorkbookSourceSlotStatus workbook_source_slot_preflight(WorkbookSourceHistorySlot slot,WorkbookSourcePayloadInfo *out) {
    if (!out) return SOURCE_SLOT_INVALID;
    WorkbookSourceSlotStatus s=association(slot); if (s) return s;
    switch (workbook_source_payload_preflight(slot.payload,out)) {
    case SOURCE_PAYLOAD_OK: return SOURCE_SLOT_OK;
    case SOURCE_PAYLOAD_UNSUPPORTED: return SOURCE_SLOT_UNSUPPORTED;
    case SOURCE_PAYLOAD_LIMIT: return SOURCE_SLOT_LIMIT;
    default: return SOURCE_SLOT_INVALID;
    }
}
WorkbookSourceSlotStatus workbook_source_slot_admit(WorkbookSourceHistorySlot slot,SourceDependencyLive live,size_t reserved,WorkbookSourceSlotAdmission *out) {
    if (!out) return SOURCE_SLOT_INVALID;
    WorkbookSourceSlotStatus s=association(slot); if (s) return s;
    WorkbookSourceSlotAdmission next={0}; next.compatibility=workbook_source_payload_admit(slot.payload,live,reserved,&next.observations);
    if (next.compatibility==SOURCE_DEPENDENCY_INVALID) return SOURCE_SLOT_INVALID;
    if (next.compatibility==SOURCE_DEPENDENCY_LIMIT) return SOURCE_SLOT_LIMIT;
    if (next.compatibility==SOURCE_DEPENDENCY_UNSUPPORTED) return SOURCE_SLOT_UNSUPPORTED;
    *out=next; return SOURCE_SLOT_OK;
}
