#ifndef DUXCEL_HISTORY_SHEET_LIFECYCLE_H
#define DUXCEL_HISTORY_SHEET_LIFECYCLE_H
#include "history_stream.h"
#include "sheet_lifecycle.h"
#define DX_HISTORY_SHEET_LIFECYCLE_FAMILY 5u
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,sheet_session,sheet,expected_revision,revision;
    const SheetLifecyclePacket *packet; /* Borrowed immutable during reserve. */
} WorkbookHistorySheetLifecycleRecord;
typedef struct {
    bool (*reserve)(void *context,const WorkbookHistorySheetLifecycleRecord *record,void **ticket,char **error);
    void (*publish)(void *context,void *ticket);
    void (*cancel)(void *context,void *ticket);
    void *context;
} WorkbookHistorySheetLifecycleRecorder;
/* Family5 uses the existing stream FIFO/clock/entry/byte owner. A complete
   canonical metadata pair is copied before reserve returns. No dependency asset,
   sheet data, dummy worksheet action or durable publication is fabricated.
   The recorded metadata producer invokes publish only after actual matching
   workbook-level model/history/revision adoption. No caller-supplied coherent
   flag authorizes publication. Stream outlives all tickets. */
WorkbookHistorySheetLifecycleRecorder workbook_history_stream_sheet_lifecycle_recorder(WorkbookHistoryStream *stream);
#endif
