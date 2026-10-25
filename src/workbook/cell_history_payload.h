#ifndef DUXCEL_CELL_HISTORY_PAYLOAD_H
#define DUXCEL_CELL_HISTORY_PAYLOAD_H
#include "cell_delta_codec.h"
#include "history_event_frame.h"
#include "history_event_slot.h"
#define DX_CELL_HISTORY_PAYLOAD_BYTES (DX_CELL_DELTA_HEADER_BYTES+2u*DX_CELL_DELTA_TEXT_BYTES)
typedef struct {
    const WorkbookHistoryEventInfo *event;
    const void *data,*dependencies;
    size_t bytes,dependency_bytes;
    uint64_t sheet;
    unsigned effects; /* Explicit expected 2, not an invented envelope field. */
} WorkbookCellPayloadView;
typedef struct { uint64_t sheet,row; size_t column,bytes,before_bytes,after_bytes; unsigned effects; } WorkbookCellPayloadInfo;
typedef enum { CELL_PAYLOAD_OK,CELL_PAYLOAD_INVALID,CELL_PAYLOAD_UNSUPPORTED,CELL_PAYLOAD_LIMIT } WorkbookCellPayloadStatus;
/* Current family1/DXCD only. Decode actual immutable bytes with the existing
   sparse-manual codec after the shared in-memory frame check. Bind expected
   sheet/effects2; reject every external dependency/inactive-source claim.
   max_payload_bytes is an explicit borrowed-input inspection cap, NOT heap
   reservation. The strict decoder owns nothing and uses no parser heap scratch;
   zero cap refuses nonempty input before reading it. Return scalar target/value
   lengths only, never borrowed text, a decoded owner or SQL NULL semantics.
   Failure preserves old out. No source activation, path/checkpoint closure,
   replay, writer, new codec or .dxls profile is introduced. */
WorkbookCellPayloadStatus workbook_cell_payload_check(WorkbookCellPayloadView view,size_t max_payload_bytes,WorkbookCellPayloadInfo *out);
typedef struct {
    const WorkbookHistoryPath *path;
    size_t index;
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    WorkbookCellPayloadView payload;
} WorkbookCellHistorySlot;
typedef enum { CELL_SLOT_OK,CELL_SLOT_INVALID,CELL_SLOT_PATH,CELL_SLOT_ASSOCIATION,CELL_SLOT_UNSUPPORTED,CELL_SLOT_LIMIT } WorkbookCellSlotStatus;
/* Exactly one caller-assigned retained slot, including Redo. Reuse generic
   transition/runtime/fork association, then the actual-byte cell check above.
   No parallel lineage checker, omitted-family/whole-profile claim, owner or
   activation decision. Arrays/frame/bytes remain borrowed and immutable. */
WorkbookCellSlotStatus workbook_cell_slot_check(WorkbookCellHistorySlot slot,size_t max_payload_bytes,WorkbookCellPayloadInfo *out);
#endif
