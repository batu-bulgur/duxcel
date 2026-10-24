#ifndef DUXCEL_SHEET_LIFECYCLE_COMMAND_H
#define DUXCEL_SHEET_LIFECYCLE_COMMAND_H
#include "history_sheet_lifecycle.h"
typedef enum { SHEET_METADATA_RENAME,SHEET_METADATA_REORDER,SHEET_METADATA_HIDE,SHEET_METADATA_SHOW } SheetMetadataOperation;
typedef struct { SheetMetadataOperation operation; uint64_t sheet,destination; const char *name; } SheetMetadataCommand;
typedef struct {
    const Workbook *book;
    const uint64_t *epoch; /* Runtime owner/request epoch, never serialized. */
    size_t plan_reserved;
    bool ready,read_only;
} SheetLifecycleCommandContext;
typedef struct SheetLifecycleCommandPlan SheetLifecycleCommandPlan;
typedef enum { SHEET_METADATA_READY,SHEET_METADATA_NOOP,SHEET_METADATA_INVALID,
    SHEET_METADATA_STALE,SHEET_METADATA_PROTECTED,SHEET_METADATA_LIMIT,SHEET_METADATA_BUSY } SheetMetadataStatus;
/* Existing genuine workbook/target sheet sessions must already be established.
   No lazy session assignment via a fake cell target/action. Prepare normalizes
   rename with the ACTUAL workbook naming helper, resolves stable reorder IDs,
   projects hide fallback and owns full before/after graphs plus canonical bytes.
   reserved covers the new plan/graphs/packet and conservative4097-byte naming
   scratch; outer ownership accounts live/current/other jobs separately.
   Failure/NOOP preserves out, book, history, allocator and dirty/revision fields. */
SheetMetadataStatus sheet_lifecycle_command_prepare(SheetLifecycleCommandContext context,SheetMetadataCommand command,SheetLifecycleCommandPlan **out,char **error);
/* Exact durable/session/model/chronology/epoch fences, without treating active
   navigation as an authored change. Raw pointer/scalar fences are runtime only;
   captured name bytes are owned. Callbacks cannot reenter/mutate the owner. */
SheetMetadataStatus sheet_lifecycle_command_check(SheetLifecycleCommandContext context,const SheetLifecycleCommandPlan *plan,char **error);
/* Reserve immutable family5 bytes, then recheck every live fence. Any callback
   mutation cancels the new ticket. A pending ticket stays charged until cancel
   or plan disposal; duplicate reserve never replaces or leaks it. No publish,
   model adoption, Undo eviction, dirty/revision/action update or IO occurs. */
SheetMetadataStatus sheet_lifecycle_command_reserve(SheetLifecycleCommandContext context,SheetLifecycleCommandPlan *plan,const WorkbookHistorySheetLifecycleRecorder *recorder,char **error);
void sheet_lifecycle_command_cancel(SheetLifecycleCommandPlan *plan);
void sheet_lifecycle_command_free(SheetLifecycleCommandPlan **plan);
const SheetLifecycle *sheet_lifecycle_command_view(const SheetLifecycleCommandPlan *plan);
const SheetLifecyclePacket *sheet_lifecycle_command_packet(const SheetLifecycleCommandPlan *plan);
size_t sheet_lifecycle_command_owned(const SheetLifecycleCommandPlan *plan);
bool sheet_lifecycle_command_reserved(const SheetLifecycleCommandPlan *plan);
#endif
