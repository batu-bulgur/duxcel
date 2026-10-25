#ifndef DUXCEL_SHEET_HISTORY_BATCH_H
#define DUXCEL_SHEET_HISTORY_BATCH_H
#include "sheet_history_slot.h"
#define DX_SHEET_HISTORY_BATCH_ENTRIES 256u
#define DX_SHEET_HISTORY_BATCH_OWNED_BYTES (32u*1024u*1024u)
#define DX_SHEET_HISTORY_BATCH_STAGING_BYTES (64u*1024u*1024u)
typedef struct { size_t index; WorkbookSheetPayloadView payload; } WorkbookSheetHistoryAssignment;
typedef struct {
    const WorkbookHistoryPath *path;
    WorkbookVersionOwner owner;
    uint64_t edit_session;
    const WorkbookSheetHistoryAssignment *assignments;
    size_t count;
} WorkbookSheetBatchView;
typedef struct { size_t count,owned,prior_owned,validation_scratch,staging; } WorkbookSheetBatchInfo;
typedef struct WorkbookSheetHistoryBatch WorkbookSheetHistoryBatch;
typedef enum { SHEET_BATCH_OK,SHEET_BATCH_INVALID,SHEET_BATCH_SEQUENCE,SHEET_BATCH_ASSOCIATION,SHEET_BATCH_UNSUPPORTED,SHEET_BATCH_LIMIT } WorkbookSheetBatchStatus;
/* Positive caller-supplied assigned family5 subset only. Index order is strict;
   duplicate/out-of-order slots reject. Never infer omitted families/slots or
   silently truncate to the staging cap. This bound is an unpublished decoded
   BATCH resource policy, not a retained-history/file-profile retention window.
   Reuse exact per-slot proof, then aggregate NEW independent owners/array plus
   one maximum sequential validation scratch, including prior batch residency
   during replacement. Input frames/path and live graphs stay separately charged. Measure allocates nothing; decode first
   validates ALL components under the complete grant, then owns them atomically.
   Caller inputs remain immutable through every pass; no callbacks/IO/replay or
   publication are performed. Free is cancellation of unpublished owned state.
   Existing codec charges are bounded ownership measures, not allocator/RSS claims. */
WorkbookSheetBatchStatus workbook_sheet_batch_measure(WorkbookSheetBatchView view,const WorkbookSheetHistoryBatch *prior,WorkbookSheetBatchInfo *out);
WorkbookSheetBatchStatus workbook_sheet_batch_decode(WorkbookSheetBatchView view,size_t reserved,WorkbookSheetHistoryBatch **out);
void workbook_sheet_batch_free(WorkbookSheetHistoryBatch **batch);
size_t workbook_sheet_batch_count(const WorkbookSheetHistoryBatch *batch);
const WorkbookSheetPayload *workbook_sheet_batch_at(const WorkbookSheetHistoryBatch *batch,size_t index);
bool workbook_sheet_batch_index(const WorkbookSheetHistoryBatch *batch,size_t index,size_t *path_index);
#endif
