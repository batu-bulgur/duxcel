#ifndef DUXCEL_LIFECYCLE_TOPOLOGY_INTERNAL_H
#define DUXCEL_LIFECYCLE_TOPOLOGY_INTERNAL_H
#include "lifecycle.h"
#include "history_sheet_topology.h"
#include "history_sheet_lifecycle.h"
/* Closed producer boundary, never an App/coherence-flag API. Before mutation,
   charge the entire prospective packet and coexisting plan/adoption staging.
   The current ring's packets and detached graphs already count in live usage. */
FormatBudgetStatus workbook_lifecycle_topology_preflight(const Workbook *book,size_t packet_bytes,size_t staging);
/* After actual matching presence action/model proof, move the original packet
   into that exact genuine lifecycle event without allocation. Caller has already
   validated its complete current payload. Independent identity/header/session
   invariants abort only on an impossible closed-coordinator violation. */
void workbook_lifecycle_topology_adopt(Workbook *book,uint64_t order,uint64_t revision,uint64_t sheet,WorkbookSheetOperation operation,SheetTopologyPacket *packet);
/* Same single lifecycle owner for current metadata profile1. The existing
   topology profile2 API above remains stable for source CREATE coordinators. */
FormatBudgetStatus workbook_lifecycle_metadata_preflight(const Workbook *book,size_t packet_bytes,size_t staging);
void workbook_lifecycle_metadata_adopt(Workbook *book,uint64_t order,uint64_t revision,uint64_t sheet,WorkbookSheetOperation operation,SheetLifecyclePacket *packet);
#endif
