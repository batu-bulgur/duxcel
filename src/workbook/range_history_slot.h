#ifndef DUXCEL_RANGE_HISTORY_SLOT_H
#define DUXCEL_RANGE_HISTORY_SLOT_H
#include "history_event_slot.h"
#include "range_history_payload.h"
typedef struct {
    const WorkbookHistoryPath *path;
    size_t index;
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    WorkbookRangePayloadView payload;
} WorkbookRangeHistorySlot;
typedef enum { RANGE_SLOT_OK,RANGE_SLOT_INVALID,RANGE_SLOT_PATH,RANGE_SLOT_ASSOCIATION,RANGE_SLOT_UNSUPPORTED,RANGE_SLOT_LIMIT } WorkbookRangeSlotStatus;
typedef struct { SourceDependencyStatus compatibility; SourceDependencyAdmission observations; } WorkbookRangeSlotAdmission;
/* Exactly one caller-assigned FAMILY4 slot. Shared path/runtime/TRANSITION
   association precedes actual DXRE4 byte/profile/effect/dependency admission.
   A retained Redo slot is checked even when cursor precedes its position.
   Borrowed arrays/bytes/frame/ledger remain immutable and readable. Preflight
   allocates nothing; admission grants complete existing temporary decoding.
   Outputs are separate and preserved on path/association/profile/budget errors.
   OK plus UNAVAILABLE/CONFLICT or pending/unbound observations is metadata only,
   never result readiness. No omitted-family/profile, origin/root/checkpoint,
   replay, writer or activation authority follows; outer admission owns those. */
WorkbookRangeSlotStatus workbook_range_slot_preflight(WorkbookRangeHistorySlot slot,WorkbookRangePayloadInfo *out);
WorkbookRangeSlotStatus workbook_range_slot_admit(WorkbookRangeHistorySlot slot,SourceDependencyLive live,
    size_t reserved,WorkbookRangeSlotAdmission *out);
#endif
