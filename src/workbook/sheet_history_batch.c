#include "sheet_history_batch.h"
#include <stdlib.h>
typedef struct { size_t index; WorkbookSheetPayload *payload; } Entry;
struct WorkbookSheetHistoryBatch { Entry *entries; size_t count,owned; };
static WorkbookSheetBatchStatus status(WorkbookSheetSlotStatus s) {
    return s==SHEET_SLOT_OK ? SHEET_BATCH_OK : s==SHEET_SLOT_LIMIT ? SHEET_BATCH_LIMIT : s==SHEET_SLOT_UNSUPPORTED ? SHEET_BATCH_UNSUPPORTED :
        s==SHEET_SLOT_PATH || s==SHEET_SLOT_ASSOCIATION ? SHEET_BATCH_ASSOCIATION : SHEET_BATCH_INVALID;
}
static WorkbookSheetHistorySlot slot(WorkbookSheetBatchView view,size_t i) {
    const WorkbookSheetHistoryAssignment *a=view.assignments+i;
    return (WorkbookSheetHistorySlot){view.path,a->index,view.owner,view.edit_session,a->payload};
}
WorkbookSheetBatchStatus workbook_sheet_batch_measure(WorkbookSheetBatchView view,const WorkbookSheetHistoryBatch *prior,WorkbookSheetBatchInfo *out) {
    if (!out || !view.count || !view.assignments || !view.path) return SHEET_BATCH_INVALID;
    if (view.count>DX_SHEET_HISTORY_BATCH_ENTRIES) return SHEET_BATCH_LIMIT;
    WorkbookSheetBatchInfo info={.count=view.count,.owned=sizeof(WorkbookSheetHistoryBatch)+view.count*sizeof(Entry)};
    for (size_t i=0;i<view.count;i++) {
        if (i && view.assignments[i].index<=view.assignments[i-1].index) return SHEET_BATCH_SEQUENCE;
        WorkbookSheetPayloadInfo payload; WorkbookSheetBatchStatus result=status(workbook_sheet_slot_preflight(slot(view,i),&payload)); if (result) return result;
        if (payload.owned>DX_SHEET_HISTORY_BATCH_OWNED_BYTES-info.owned) return SHEET_BATCH_LIMIT;
        info.owned+=payload.owned;
        if (payload.validation_scratch>info.validation_scratch) info.validation_scratch=payload.validation_scratch;
    }
    info.prior_owned=prior ? prior->owned : 0;
    if (info.prior_owned>DX_SHEET_HISTORY_BATCH_STAGING_BYTES-info.owned || info.validation_scratch>DX_SHEET_HISTORY_BATCH_STAGING_BYTES-info.owned-info.prior_owned) return SHEET_BATCH_LIMIT;
    info.staging=info.owned+info.prior_owned+info.validation_scratch; *out=info; return SHEET_BATCH_OK;
}
WorkbookSheetBatchStatus workbook_sheet_batch_decode(WorkbookSheetBatchView view,size_t reserved,WorkbookSheetHistoryBatch **out) {
    if (!out) return SHEET_BATCH_INVALID; WorkbookSheetBatchInfo info;
    WorkbookSheetBatchStatus result=workbook_sheet_batch_measure(view,*out,&info); if (result) return result;
    if (info.staging>reserved) return SHEET_BATCH_LIMIT;
    /* Reject semantic closure failures before constructing any new batch graph.
       Each temporary validator releases its one arena before the next starts. */
    for (size_t i=0;i<view.count;i++) {
        WorkbookSheetPayloadInfo payload; result=status(workbook_sheet_slot_validate(slot(view,i),info.validation_scratch,&payload)); if (result) return result;
    }
    WorkbookSheetHistoryBatch *next=dx_alloc(sizeof(*next)); next->count=view.count; next->owned=info.owned; next->entries=dx_alloc(view.count*sizeof(*next->entries));
    for (size_t i=0;i<view.count;i++) {
        WorkbookSheetPayloadInfo payload; result=status(workbook_sheet_slot_preflight(slot(view,i),&payload));
        if (!result) result=status(workbook_sheet_slot_decode(slot(view,i),payload.staging,&next->entries[i].payload));
        if (result) { workbook_sheet_batch_free(&next); return result; }
        next->entries[i].index=view.assignments[i].index;
    }
    workbook_sheet_batch_free(out); *out=next; return SHEET_BATCH_OK;
}
void workbook_sheet_batch_free(WorkbookSheetHistoryBatch **owned) {
    if (!owned || !*owned) return; WorkbookSheetHistoryBatch *batch=*owned;
    for (size_t i=0;i<batch->count;i++) workbook_sheet_payload_free(&batch->entries[i].payload);
    free(batch->entries); free(batch); *owned=NULL;
}
size_t workbook_sheet_batch_count(const WorkbookSheetHistoryBatch *batch) { return batch ? batch->count : 0; }
const WorkbookSheetPayload *workbook_sheet_batch_at(const WorkbookSheetHistoryBatch *batch,size_t index) { return batch && index<batch->count ? batch->entries[index].payload : NULL; }
bool workbook_sheet_batch_index(const WorkbookSheetHistoryBatch *batch,size_t index,size_t *out) { if (!batch || index>=batch->count || !out) return false; *out=batch->entries[index].index; return true; }
