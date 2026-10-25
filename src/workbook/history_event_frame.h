#ifndef DUXCEL_HISTORY_EVENT_FRAME_H
#define DUXCEL_HISTORY_EVENT_FRAME_H
#include "history_stream.h"
/* Existing in-memory stream frame identity/clock invariant, shared by current
   family dispatchers. This is not path membership, wire decoding, target/effect,
   dependency closure, publication or activation proof. No mutation/allocation. */
bool workbook_history_event_frame_valid(const WorkbookHistoryEventInfo *event);
#endif
