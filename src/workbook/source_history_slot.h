#ifndef DUXCEL_SOURCE_HISTORY_SLOT_H
#define DUXCEL_SOURCE_HISTORY_SLOT_H
#include "history_event_slot.h"
#include "source_history_payload.h"
typedef struct {
    const WorkbookHistoryPath *path;
    size_t index;
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    WorkbookSourcePayloadView payload;
} WorkbookSourceHistorySlot;
typedef enum { SOURCE_SLOT_OK,SOURCE_SLOT_INVALID,SOURCE_SLOT_PATH,SOURCE_SLOT_ASSOCIATION,SOURCE_SLOT_UNSUPPORTED,SOURCE_SLOT_LIMIT } WorkbookSourceSlotStatus;
typedef struct { SourceDependencyStatus compatibility; SourceDependencyAdmission observations; } WorkbookSourceSlotAdmission;
/* Exactly one assigned family2/3 slot. Reuse shared structural path/runtime/
   TRANSITION association, then actual-byte payload/effect/dependency admission.
   Frame.action is reusable and must never substitute for the path transition.
   Cursor may precede this slot: retained Redo is not omitted. Arrays/bytes/frame/
   ledger remain borrowed immutable. Preflight is allocation-free framing only;
   admission uses complete existing temporary decode/parser grants and stores no
   owner or borrowed pointers. Old outputs survive any validation/budget failure.
   OK reports a valid slot plus scalar compatibility; UNAVAILABLE/CONFLICT and
   captured pending/unbound/opaque observations do not grant readiness. No whole
   profile/omitted-family, root/anchor/checkpoint, replay, writer or activation
   decision follows. The path's origin authority remains an outer obligation. */
WorkbookSourceSlotStatus workbook_source_slot_preflight(WorkbookSourceHistorySlot slot,WorkbookSourcePayloadInfo *out);
WorkbookSourceSlotStatus workbook_source_slot_admit(WorkbookSourceHistorySlot slot,SourceDependencyLive live,
    size_t reserved,WorkbookSourceSlotAdmission *out);
#endif
