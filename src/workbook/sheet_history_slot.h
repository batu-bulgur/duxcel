#ifndef DUXCEL_SHEET_HISTORY_SLOT_H
#define DUXCEL_SHEET_HISTORY_SLOT_H
#include "sheet_history_payload.h"
#include "history_path.h"
typedef struct {
    const WorkbookHistoryPath *path;
    size_t index; /* Exactly one assigned entry in the already readable path. */
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    WorkbookSheetPayloadView payload;
} WorkbookSheetHistorySlot;
typedef enum { SHEET_SLOT_OK,SHEET_SLOT_INVALID,SHEET_SLOT_PATH,SHEET_SLOT_ASSOCIATION,SHEET_SLOT_UNSUPPORTED,SHEET_SLOT_LIMIT } WorkbookSheetSlotStatus;
/* One assigned CURRENT authored family5 slot, not a whole-profile admission.
   Reuse the structural ancestry checker and closed typed payload codecs. Path's
   field named operation is the transition identity: it must equal frame.transition,
   never the reusable user-action identity. Bind exact position/lineage/document,
   runtime stream owner/session, target/metadata effect and dependency absence.
   Forks require an explicit preceding ancestry segment and its parent lineage;
   a fork at an opaque first-segment origin cannot invent missing parent proof.
   A nonfork first segment relies on the caller's declared origin lineage; this
   helper does not manufacture its checkpoint/root/anchor authority.
   Cursor may precede this slot (Redo is still retained). Omitted slots/families,
   anchors/root closure, archive wire, writer and replay remain outer obligations.
   Path arrays, frame and bytes stay borrowed and immutable during each call.
   Preflight allocates nothing; validate/decode use the exact payload grants.
   Decode returns the existing independent payload owner, which stores the matched
   frame/transition but no borrowed path pointer. Old output/input are separate
   coexistence charges. Every failure preserves outputs and all caller owners. */
WorkbookSheetSlotStatus workbook_sheet_slot_preflight(WorkbookSheetHistorySlot slot,WorkbookSheetPayloadInfo *out);
WorkbookSheetSlotStatus workbook_sheet_slot_validate(WorkbookSheetHistorySlot slot,size_t reserved,WorkbookSheetPayloadInfo *out);
WorkbookSheetSlotStatus workbook_sheet_slot_decode(WorkbookSheetHistorySlot slot,size_t reserved,WorkbookSheetPayload **out);
#endif
