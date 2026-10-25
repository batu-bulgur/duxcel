#ifndef DUXCEL_SOURCE_HISTORY_PAYLOAD_H
#define DUXCEL_SOURCE_HISTORY_PAYLOAD_H
#include "history_event_frame.h"
#include "history_format.h"
#include "history_definition.h"
#include "source_dependency_admission.h"
typedef struct {
    const WorkbookHistoryEventInfo *event;
    const void *data,*dependencies;
    size_t bytes,dependency_bytes;
    uint64_t sheet;
    unsigned effects; /* Explicit outer expectation, not an invented frame field. */
} WorkbookSourcePayloadView;
typedef struct { uint64_t sheet; unsigned effects; size_t owned,validation_scratch,staging; } WorkbookSourcePayloadInfo;
typedef enum { SOURCE_PAYLOAD_OK,SOURCE_PAYLOAD_INVALID,SOURCE_PAYLOAD_UNSUPPORTED,SOURCE_PAYLOAD_LIMIT } WorkbookSourcePayloadStatus;
/* Current FAMILY2/DXFD+DXSV or FAMILY3/DXDE; no alternate profile or file codec.
   Bind actual borrowed frame lengths/profiles and explicit sheet/effect to the
   existing codecs. Preflight is allocation-free framing/cost association only.
   Admit staging before temporary existing decoded owners/parser workspace.
   Admission fully decodes the SAME actual bytes, checks exact source selection
   and inactive observation associations, returns only scalar live metadata
   compatibility, and frees all temporary owners. No stored payload/dependency
   owner, synthetic event, merge, replay, writer or activation decision.
   Input bytes/frame/ledger remain immutable throughout; out is separate from
   inputs and preserved on invalid/limit/unsupported outcomes. Compatibility
   results report observations only, including still-pending captured metadata.
   Runtime/path/checkpoint/document admission beyond this frame is external. */
WorkbookSourcePayloadStatus workbook_source_payload_preflight(WorkbookSourcePayloadView view,WorkbookSourcePayloadInfo *out);
SourceDependencyStatus workbook_source_payload_admit(WorkbookSourcePayloadView view,SourceDependencyLive live,
    size_t reserved,SourceDependencyAdmission *out);
#endif
