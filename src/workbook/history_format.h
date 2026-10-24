#ifndef DUXCEL_HISTORY_FORMAT_H
#define DUXCEL_HISTORY_FORMAT_H
#include "history_stream.h"
#include "format_dependencies.h"
#define DX_HISTORY_FORMAT_FAMILY 2u
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,sheet_session,sheet,expected_revision,revision;
    unsigned effects; /* Exactly 8: a format-only edit. Other composites reject. */
    FormatSnapshotView before,after;
    const SourceVersions *sources;
    uint64_t object_high_water;
} WorkbookHistoryFormatRecord;
typedef struct {
    bool (*reserve)(void *context,const WorkbookHistoryFormatRecord *record,void **ticket,char **error);
    void (*publish)(void *context,void *ticket);
    void (*cancel)(void *context,void *ticket);
    void *context;
} WorkbookHistoryFormatRecorder;
/* One mutation owner. Before reserve, the producer proves workbook/sheet/session,
   format/source/definition fences and source activation/readiness, after any
   baseline normalization. Before adoption it rechecks those exact fences.
   Reserve copies canonical format and closed source metadata before returning;
   publish follows the COMPLETE coherent model/history/dirty/revision change.
   Cancel on failure/no-op/stale; callbacks consume one ticket and perform no IO.
   This format-only adapter rejects analysis/object/range/lifecycle composites;
   those require complete constituent admission, never a partial format event.
   Source rows/locators/packets encode directly without an intermediate ledger
   clone. Bounded source-scope grammar/format-validation scratch and one scope
   marker are reserved separately; final assets share the stream
   bound with cell events. No App activation or durable file receipt is implied. */
WorkbookHistoryFormatRecorder workbook_history_stream_format_recorder(WorkbookHistoryStream *stream);
#endif
