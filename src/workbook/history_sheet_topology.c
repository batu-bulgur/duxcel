#include "history_sheet_topology.h"
#include "history_stream_internal.h"
#include <stdlib.h>
#include <string.h>
struct WorkbookHistoryTopologyTicket { WorkbookHistoryStream *stream; void *reservation; };
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
size_t workbook_history_topology_ticket_allocation(void) { return sizeof(WorkbookHistoryTopologyTicket); }
bool workbook_history_topology_reserve(WorkbookHistoryStream *stream,const WorkbookHistoryTopologyRecord *record,size_t reserved,WorkbookHistoryTopologyTicket **out,char **error) {
    if (!stream || !record || !record->packet || !out || *out) return fail(error,"Choose an empty owned topology reservation.");
    WorkbookHistoryStreamState state=workbook_history_stream_state(stream); const WorkbookHistoryStreamOrigin *owner=&state.current;
    if (record->workbook.high!=owner->owner.document_high || record->workbook.low!=owner->owner.document_low || record->edit_session!=owner->edit_session ||
        record->expected_revision!=owner->command_revision || owner->command_revision==UINT64_MAX || record->revision!=owner->command_revision+1)
        return fail(error,"The topology stream document session or revision changed.");
    if (state.reserved) return fail(error,"Another history reservation is still owned.");
    SheetTopologyInfo info;
    if (sheet_topology_preflight(record->packet->data,record->packet->bytes,&info)!=SHEET_TOPOLOGY_OK ||
        info.document.high!=record->workbook.high || info.document.low!=record->workbook.low)
        return fail(error,"Choose a current topology component for this document.");
    if (reserved<sizeof(WorkbookHistoryTopologyTicket) || info.validation_scratch>reserved-sizeof(WorkbookHistoryTopologyTicket))
        return fail(error,"The complete topology validation budget is unavailable.");
    if (sheet_topology_validate(record->packet->data,record->packet->bytes,info.validation_scratch,&info)!=SHEET_TOPOLOGY_OK)
        return fail(error,"The topology component has incomplete sheet ownership.");
    void *raw=NULL; WorkbookHistoryEditFence fence={record->workbook,record->edit_session,record->expected_revision,record->revision};
    if (!workbook_history_stream_reserve(stream,fence,DX_HISTORY_SHEET_TOPOLOGY_FAMILY,DX_HISTORY_SHEET_TOPOLOGY_PROFILE,info.bytes,0,0,0,&raw,error)) return false;
    memcpy(workbook_history_reservation_payload(raw),record->packet->data,info.bytes);
    WorkbookHistoryTopologyTicket *ticket=dx_alloc(sizeof(*ticket)); *ticket=(WorkbookHistoryTopologyTicket){stream,raw}; *out=ticket; return true;
}
const WorkbookHistoryEventInfo *workbook_history_topology_info(const WorkbookHistoryTopologyTicket *ticket) { return ticket ? workbook_history_event_info(ticket->reservation) : NULL; }
const void *workbook_history_topology_payload(const WorkbookHistoryTopologyTicket *ticket) { return ticket ? workbook_history_event_payload(ticket->reservation) : NULL; }
void workbook_history_topology_cancel(WorkbookHistoryTopologyTicket **owned) {
    if (!owned || !*owned) return; WorkbookHistoryTopologyTicket *ticket=*owned;
    workbook_history_stream_cancel(ticket->stream,ticket->reservation); free(ticket); *owned=NULL;
}
