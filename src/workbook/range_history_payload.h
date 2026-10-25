#ifndef DUXCEL_RANGE_HISTORY_PAYLOAD_H
#define DUXCEL_RANGE_HISTORY_PAYLOAD_H
#include "history_event_frame.h"
#include "history_range.h"
#include "source_dependency_admission.h"
typedef struct {
    const WorkbookHistoryEventInfo *event;
    const void *data,*dependencies;
    size_t bytes,dependency_bytes;
    uint64_t sheet;
    unsigned effects; /* Explicit caller expectation; not an invented frame field. */
} WorkbookRangePayloadView;
typedef struct {
    uint64_t sheet,object_high_water;
    unsigned effects;
    size_t unbound_sources,owned,validation_scratch,staging;
} WorkbookRangePayloadInfo;
typedef enum { RANGE_PAYLOAD_OK,RANGE_PAYLOAD_INVALID,RANGE_PAYLOAD_UNSUPPORTED,RANGE_PAYLOAD_LIMIT } WorkbookRangePayloadStatus;
/* Current FAMILY4/DXRE4 only. Preflight associates actual borrowed frame/byte
   lengths, exact profile/sheet/effects, zero external dependencies/inactive
   scopes, and bounded staging costs without allocating. Semantic decode and
   source-union proof follow admission of the complete staging reservation.
   Admission frees every temporary existing decoder owner and returns only the
   shared scalar source metadata report. The exact decoded DXDE/DXSV union is
   compared once; a constituent is never made to reference its partner's rows.
   Unbound/pending observations remain inactive even when live metadata refines
   them. Cells/width-only events carry no source allocator/dependency observation
   and do not require or inspect a ledger. No live target-graph, path/root/anchor,
   omitted-family, checkpoint, cursor, replay, writer or activation proof follows.
   Input frame/bytes/ledger are borrowed immutable, outputs must be separate.
   Invalid/unsupported/limit outcomes preserve old outputs. No IO or callbacks. */
WorkbookRangePayloadStatus workbook_range_payload_preflight(WorkbookRangePayloadView view,WorkbookRangePayloadInfo *out);
SourceDependencyStatus workbook_range_payload_admit(WorkbookRangePayloadView view,SourceDependencyLive live,
    size_t reserved,SourceDependencyAdmission *out);
#endif
