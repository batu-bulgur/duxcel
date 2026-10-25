#include "cell_history_payload.h"
WorkbookCellPayloadStatus workbook_cell_payload_check(WorkbookCellPayloadView v,size_t cap,WorkbookCellPayloadInfo *out) {
    if (!out || !v.event) return CELL_PAYLOAD_INVALID;
    const WorkbookHistoryEventInfo *e=v.event;
    if (e->family!=DX_HISTORY_CELL_FAMILY || e->encoding!=DX_CELL_DELTA_ENCODING) return CELL_PAYLOAD_UNSUPPORTED;
    if (!workbook_history_event_frame_valid(e) || !v.data || !v.sheet || v.effects!=2 || v.bytes!=e->bytes ||
        e->dependency_encoding || e->dependency_bytes || e->inactive_scopes || v.dependencies || v.dependency_bytes || v.bytes>DX_CELL_HISTORY_PAYLOAD_BYTES) return CELL_PAYLOAD_INVALID;
    if (v.bytes>cap) return CELL_PAYLOAD_LIMIT;
    CellDelta delta={0}; CellDeltaCodecStatus s=cell_delta_decode(v.data,v.bytes,&delta);
    if (s!=CELL_DELTA_CODEC_OK) return s==CELL_DELTA_CODEC_UNSUPPORTED ? CELL_PAYLOAD_UNSUPPORTED : CELL_PAYLOAD_INVALID;
    if (delta.sheet!=v.sheet) return CELL_PAYLOAD_INVALID;
    *out=(WorkbookCellPayloadInfo){delta.sheet,delta.row,delta.column,v.bytes,delta.before_bytes,delta.after_bytes,2}; return CELL_PAYLOAD_OK;
}
WorkbookCellSlotStatus workbook_cell_slot_check(WorkbookCellHistorySlot slot,size_t cap,WorkbookCellPayloadInfo *out) {
    if (!out) return CELL_SLOT_INVALID;
    switch (workbook_history_event_slot_check((WorkbookHistoryEventSlot){slot.path,slot.index,slot.owner,slot.edit_session,slot.payload.event})) {
    case HISTORY_EVENT_SLOT_INVALID: return CELL_SLOT_INVALID;
    case HISTORY_EVENT_SLOT_PATH: return CELL_SLOT_PATH;
    case HISTORY_EVENT_SLOT_ASSOCIATION: return CELL_SLOT_ASSOCIATION;
    case HISTORY_EVENT_SLOT_OK: break;
    default: return CELL_SLOT_INVALID;
    }
    switch (workbook_cell_payload_check(slot.payload,cap,out)) {
    case CELL_PAYLOAD_OK: return CELL_SLOT_OK;
    case CELL_PAYLOAD_UNSUPPORTED: return CELL_SLOT_UNSUPPORTED;
    case CELL_PAYLOAD_LIMIT: return CELL_SLOT_LIMIT;
    default: return CELL_SLOT_INVALID;
    }
}
