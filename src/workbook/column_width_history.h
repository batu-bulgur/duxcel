#ifndef DUXCEL_COLUMN_WIDTH_HISTORY_H
#define DUXCEL_COLUMN_WIDTH_HISTORY_H
#include "column_widths.h"
#define DX_COLUMN_WIDTH_HISTORY_ENTRIES 32u
#define DX_COLUMN_WIDTH_HISTORY_BYTES (8u*1024u*1024u)
typedef struct ColumnWidthHistory ColumnWidthHistory;
typedef struct ColumnWidthPlan ColumnWidthPlan;
typedef struct { size_t entries,bytes; } ColumnWidthHistoryOptions;
typedef struct { size_t undo,redo,owned_bytes; uint64_t generation; } ColumnWidthHistoryInfo;
typedef struct {
    ColumnWidthState *current; /* Independently owned canonical current state. */
    uint64_t *revision; /* Committed width authority; never a historical value. */
    ColumnWidthHistory *history;
    size_t staging_reserved;
} ColumnWidthContext;
/* Journal owns immutable canonical pairs only; current state and revision are
   separate caller-owned objects, borrowed for each single-owner operation.
   Byte cap covers retained packets, entry cap is1..32. No silent capacity growth.
   Constructor reserved covers the journal wrapper/slots; options are explicit. */
size_t column_width_history_allocation(void);
ColumnWidthStatus column_width_history_new(ColumnWidthHistoryOptions options,size_t reserved,ColumnWidthHistory **out);
ColumnWidthHistoryInfo column_width_history_info(const ColumnWidthHistory *history);
void column_width_history_free(ColumnWidthHistory **history);
/* Prepare owns complete immutable pair plus independent target before any
   branch discard/oldest eviction. New allocations only are charged to staging;
   old current, journal and simultaneous plans stay separately charged. NOOP
   preserves Redo/revision even with zero staging or exhausted revision.
   Failure preserves current/journal/out; out must be NULL. Cancel by freeing.
   Current/revision/journal object owners outlive the plan; no callback or IO. */
ColumnWidthStatus column_width_history_prepare(ColumnWidthContext context,ColumnWidthView after,ColumnWidthPlan **out);
ColumnWidthStatus column_width_history_replay(ColumnWidthContext context,bool redo,ColumnWidthPlan **out);
size_t column_width_plan_owned(const ColumnWidthPlan *plan);
ColumnWidthView column_width_plan_target(const ColumnWidthPlan *plan);
const ColumnWidthPacket *column_width_plan_packet(const ColumnWidthPlan *plan);
void column_width_plan_free(ColumnWidthPlan **plan);
/* Recheck exact current/revision/owner/journal generation before a nonfallible
   owned swap. Success consumes plan, clears Redo only for a new edit, moves one
   immutable event for Undo/Redo and INCREMENTS live revision; it never rewinds.
   Divergent current bytes reject even if the caller forgot its revision bump.
   A future producer reserves ALL compound effects/stream bytes before this
   commit; there is no worksheet action, Grid adapter or durable event here. */
ColumnWidthStatus column_width_history_commit(ColumnWidthContext context,ColumnWidthPlan **plan);
#endif
