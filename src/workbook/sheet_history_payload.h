#ifndef DUXCEL_SHEET_HISTORY_PAYLOAD_H
#define DUXCEL_SHEET_HISTORY_PAYLOAD_H
#include "history_sheet_lifecycle.h"
#include "history_sheet_topology.h"
#include "lifecycle.h"
typedef enum { SHEET_PAYLOAD_METADATA=1,SHEET_PAYLOAD_TOPOLOGY=2 } WorkbookSheetPayloadKind;
typedef struct {
    const WorkbookHistoryEventInfo *event;
    const void *data;
    size_t bytes;
    uint64_t sheet;
    WorkbookSheetOperation operation;
} WorkbookSheetPayloadView;
typedef struct {
    WorkbookSheetPayloadKind kind;
    WorkbookId document;
    uint64_t sheet;
    WorkbookSheetOperation operation;
    size_t bytes,owned,validation_scratch,staging;
} WorkbookSheetPayloadInfo;
typedef struct WorkbookSheetPayload WorkbookSheetPayload;
typedef enum { SHEET_PAYLOAD_OK,SHEET_PAYLOAD_INVALID,SHEET_PAYLOAD_UNSUPPORTED,SHEET_PAYLOAD_LIMIT } WorkbookSheetPayloadStatus;
/* Dispatch current FAMILY5 kinds, not historical format versions: metadata
   profile1/DXSL and topology profile2/DXTP, each with its current internal codec.
   Bind the existing in-memory stream frame to explicit target/operation and its
   exact document, byte count and dependency absence. Verify current frame clock
   structure, not ancestry/path continuity, publication or IO receipt authority.
   Metadata must contain only its declared rename/reorder/hide/show effect;
   topology must truthfully ADD or REMOVE the declared ID. CREATE and DUPLICATE
   intentionally share ADD; proof of actual copying belongs to their producer.
   No target session is fabricated for an absent/new sheet. The frame does not
   encode a sheet-session identity; source/runtime activation remains external.
   Inputs stay immutable. Preflight allocates nothing and proves framing/effect
   association; validate additionally proves full topology presence/source closure
   under validation_scratch. Decode reserves complete NEW owner + staging before
   allocation and atomically replaces old output only on success. Existing input,
   prior output and live graphs remain separate coexistence charges.
   No new envelope wire, schema fallback, writer, replay or App activation. */
WorkbookSheetPayloadStatus workbook_sheet_payload_preflight(WorkbookSheetPayloadView view,WorkbookSheetPayloadInfo *out);
WorkbookSheetPayloadStatus workbook_sheet_payload_validate(WorkbookSheetPayloadView view,size_t reserved,WorkbookSheetPayloadInfo *out);
WorkbookSheetPayloadStatus workbook_sheet_payload_decode(WorkbookSheetPayloadView view,size_t reserved,WorkbookSheetPayload **out);
void workbook_sheet_payload_free(WorkbookSheetPayload **payload);
const WorkbookSheetPayloadInfo *workbook_sheet_payload_info(const WorkbookSheetPayload *payload);
const WorkbookHistoryEventInfo *workbook_sheet_payload_event(const WorkbookSheetPayload *payload);
const SheetLifecycle *workbook_sheet_payload_metadata(const WorkbookSheetPayload *payload);
const SheetTopologyEvent *workbook_sheet_payload_topology(const WorkbookSheetPayload *payload);
#endif
