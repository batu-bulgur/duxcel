#ifndef DUXCEL_COLUMN_WIDTH_COMMAND_H
#define DUXCEL_COLUMN_WIDTH_COMMAND_H
#include "workbook.h"
#include "column_width_history.h"
/* Prepared dense-authority constituent only. The worksheet coordinator owns
   durable/session fences, chronology and ALL compound event reservations. */
typedef struct {
    SheetView *view;
    ColumnWidthHistory **history;
    uint64_t *revision;
    size_t reserved;
} ColumnWidthCommandContext;
typedef struct ColumnWidthCommand ColumnWidthCommand;
/* Conservative staging bound, including a lazy journal, canonical operation,
   full dense arrays and independently owned empty extension names. NOOP is
   detected before budget/revision checks and never clears Redo. */
ColumnWidthStatus column_width_command_measure(ColumnWidthCommandContext context,ColumnWidthView after,size_t *owned);
ColumnWidthStatus column_width_command_prepare(ColumnWidthCommandContext context,ColumnWidthView after,ColumnWidthCommand **out);
ColumnWidthStatus column_width_command_replay(ColumnWidthCommandContext context,bool redo,ColumnWidthCommand **out);
ColumnWidthStatus column_width_command_check(ColumnWidthCommandContext context,const ColumnWidthCommand *plan);
ColumnWidthStatus column_width_command_history(const ColumnWidthCommand *plan,ColumnWidthHistoryInfo *out);
ColumnWidthStatus column_width_command_retain_undo(ColumnWidthCommand *plan,size_t keep);
const ColumnWidthPacket *column_width_command_packet(const ColumnWidthCommand *plan);
/* Independent arrays are prepared before eviction. Existing column-name strings
   move with adoption; newly materialized columns own empty names. Navigation is
   not fenced or changed. Revision increments once, never restores an old value.
   Commit does not record any worksheet action or publish a durable event. */
ColumnWidthStatus column_width_command_adopt(ColumnWidthCommandContext context,ColumnWidthCommand **plan);
void column_width_command_cancel(ColumnWidthCommand **plan);
#endif
