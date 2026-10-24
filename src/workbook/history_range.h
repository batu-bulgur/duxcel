#ifndef DUXCEL_HISTORY_RANGE_H
#define DUXCEL_HISTORY_RANGE_H
#include "history_stream.h"
#include "range_event.h"
#define DX_HISTORY_RANGE_FAMILY 4u
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,sheet_session,expected_revision,revision;
    size_t validation_reserved;
    RangeEventView view;
} WorkbookHistoryRangeRecord;
typedef struct {
    bool (*reserve)(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error);
    void (*publish)(void *context,void *ticket);
    void (*cancel)(void *context,void *ticket);
    void *context;
} WorkbookHistoryRangeRecorder;
/* One complete immutable source/object-free range packet owns every changed
   value/clear and optional exact Format pair. No partial constituent record.
   Reserve before history eviction, recheck owners, then publish once after
   coherent action/revision adoption. Cancellation consumes no event clocks or
   Redo. Same stream bounds/FIFO/retry owner as families1-3; no App IO activation. */
WorkbookHistoryRangeRecorder workbook_history_stream_range_recorder(WorkbookHistoryStream *stream);
#endif
