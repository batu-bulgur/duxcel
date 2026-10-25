#ifndef DUXCEL_SHEET_TOPOLOGY_REMOVE_H
#define DUXCEL_SHEET_TOPOLOGY_REMOVE_H
#include "history_sheet_topology.h"
typedef struct {
    Workbook *book;
    const uint64_t *epoch;
    const Analysis *primary; /* Actual primary of primary_sheet, never guessed from active. */
    uint64_t primary_sheet;
    size_t reserved;
    bool ready,read_only;
} SheetTopologyRemoveContext;
typedef struct SheetTopologyRemovePlan SheetTopologyRemovePlan;
typedef struct { size_t owned,staging,reservation; } SheetTopologyRemoveMeasure;
typedef enum { SHEET_REMOVE_READY,SHEET_REMOVE_INVALID,SHEET_REMOVE_STALE,SHEET_REMOVE_PROTECTED,SHEET_REMOVE_LIMIT,SHEET_REMOVE_BUSY } SheetTopologyRemoveStatus;
/* Genuine existing document/target sessions are required; caller may establish
   them through workbook_prepare_command_session without creating a user action.
   Measure is allocation-free. Plan owns before/after catalog names, actual full
   presence preimage and immutable remove event. staging conservatively covers
   coexisting capture/serialization/temporary validation owners; owned is the
   final resident plan. reservation covers the separate wrapper/validation only;
   stream payload and caller live graphs stay separately charged. No IO, model
   adoption, session assignment, lifecycle eviction, dirty/action clocks or App.
   Pending DATABASE defaults must first resolve through their actual owner. */
SheetTopologyRemoveStatus sheet_topology_remove_measure(SheetTopologyRemoveContext context,uint64_t sheet,SheetTopologyRemoveMeasure *out);
SheetTopologyRemoveStatus sheet_topology_remove_prepare(SheetTopologyRemoveContext context,uint64_t sheet,SheetTopologyRemovePlan **out,char **error);
/* Stable-ID resolution tolerates Worksheet relocation and unrelated navigation.
   Exact document/session/revision/epoch/history and target store fences, owned
   authored data and stored preferences are checked without allocation or old
   live-string dereferences. False/invalid/read-only preserves every owner. */
SheetTopologyRemoveStatus sheet_topology_remove_check(SheetTopologyRemoveContext context,const SheetTopologyRemovePlan *plan);
/* Recheck before and after reserve; stale/budget failure cancels any new ticket.
   Already-reserved plans reject duplicate reservation. No publication API. The
   stream must outlive the plan/reservation; free cancels it safely. */
SheetTopologyRemoveStatus sheet_topology_remove_reserve(SheetTopologyRemoveContext context,SheetTopologyRemovePlan *plan,
    WorkbookHistoryStream *stream,size_t reserved,char **error);
void sheet_topology_remove_cancel(SheetTopologyRemovePlan *plan);
void sheet_topology_remove_free(SheetTopologyRemovePlan **plan);
const SheetTopologyPacket *sheet_topology_remove_packet(const SheetTopologyRemovePlan *plan);
SheetPresenceView sheet_topology_remove_presence(const SheetTopologyRemovePlan *plan);
SheetTopologyRemoveMeasure sheet_topology_remove_usage(const SheetTopologyRemovePlan *plan);
bool sheet_topology_remove_reserved(const SheetTopologyRemovePlan *plan);
#endif
