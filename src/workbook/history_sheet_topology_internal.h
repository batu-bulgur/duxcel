#ifndef DUXCEL_HISTORY_SHEET_TOPOLOGY_INTERNAL_H
#define DUXCEL_HISTORY_SHEET_TOPOLOGY_INTERNAL_H
#include "history_sheet_topology.h"
/* Private closed-coordinator boundary only. Caller has synchronously proved
   actual typed catalog/action adoption; this independently rechecks document,
   genuine session and the actual adopted revision before nonfallible publish.
   Never expose an unchecked caller coherence flag or this boundary to App. */
bool workbook_history_topology_belongs(const WorkbookHistoryTopologyTicket *ticket,const WorkbookHistoryStream *stream);
void workbook_history_topology_publish_adopted(WorkbookHistoryTopologyTicket **ticket,const Workbook *book);
#endif
