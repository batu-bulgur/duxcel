#ifndef DUXCEL_HISTORY_SHEET_TOPOLOGY_H
#define DUXCEL_HISTORY_SHEET_TOPOLOGY_H
#include "history_stream.h"
#include "sheet_topology_event.h"
#define DX_HISTORY_SHEET_TOPOLOGY_FAMILY 5u
#define DX_HISTORY_SHEET_TOPOLOGY_PROFILE 2u
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,expected_revision,revision;
    const SheetTopologyPacket *packet;
} WorkbookHistoryTopologyRecord;
typedef struct WorkbookHistoryTopologyTicket WorkbookHistoryTopologyTicket;
/* Current lifecycle family profiles are different payload kinds: profile1 is
   same-ID DXSL metadata; profile2 is one-ID DXTP topology. DXTP and its DXSP
   component each use their current internal encoding1. No historical reader,
   upgrades, profile fallback or App retained-history activation is provided.
   ADD has no existing target-sheet runtime session; the genuine document
   session/revision fences the stream. The future command owner must prove all
   live/remove/prospective-sheet ownership before actual adoption/publication. */
size_t workbook_history_topology_ticket_allocation(void);
/* Full immutable closure validation BEFORE stream reservation/copy. reserved
   admits this wrapper plus topology validation_scratch; stream independently
   charges its bounded event allocation/payload. Packet/old models/tickets stay
   separately charged. Success owns one reservation and advances NO path/action/
   revision clock. Failure preserves NULL out, input and stream. Source packet
   can be released after success. Stream outlives the ticket; free refuses it.
   No publication API: reserve/cancel is the only current prerequisite. */
bool workbook_history_topology_reserve(WorkbookHistoryStream *stream,const WorkbookHistoryTopologyRecord *record,
    size_t reserved,WorkbookHistoryTopologyTicket **out,char **error);
/* Borrowed actual stream envelope/copied bytes, immutable until cancel. */
const WorkbookHistoryEventInfo *workbook_history_topology_info(const WorkbookHistoryTopologyTicket *ticket);
const void *workbook_history_topology_payload(const WorkbookHistoryTopologyTicket *ticket);
void workbook_history_topology_cancel(WorkbookHistoryTopologyTicket **ticket);
#endif
