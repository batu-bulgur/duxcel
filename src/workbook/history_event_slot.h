#ifndef DUXCEL_HISTORY_EVENT_SLOT_H
#define DUXCEL_HISTORY_EVENT_SLOT_H
#include "history_path.h"
#include "history_stream.h"
typedef struct {
    const WorkbookHistoryPath *path;
    size_t index;
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    const WorkbookHistoryEventInfo *event;
} WorkbookHistoryEventSlot;
typedef enum { HISTORY_EVENT_SLOT_OK,HISTORY_EVENT_SLOT_INVALID,HISTORY_EVENT_SLOT_PATH,HISTORY_EVENT_SLOT_ASSOCIATION } WorkbookHistoryEventSlotStatus;
/* Exactly one caller-assigned readable path slot. Bind the expected runtime
   document/stream owner/session and frame transition/lineage/position, including
   explicit fork-parent ancestry. Path.operation means TRANSITION, not reusable
   action. An opaque first segment cannot prove a fork's missing parent.
   This is structural association, not full frame/payload/profile/dependency
   validation, origin/root/checkpoint closure, omitted-family coverage, cursor
   selection or replay authority. Family consumers keep those separate checks.
   Arrays/frame remain borrowed immutable through the allocation-free call.
   No output owner, callbacks, IO, mutation, IDs or user actions are introduced. */
WorkbookHistoryEventSlotStatus workbook_history_event_slot_check(WorkbookHistoryEventSlot slot);
#endif
