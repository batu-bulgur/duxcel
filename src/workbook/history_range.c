#include "history_range.h"
#include "history_stream_internal.h"
#include <stdlib.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static bool reserve_range(void *context,const WorkbookHistoryRangeRecord *record,void **ticket,char **error) {
    if (!context || !record || !ticket || *ticket || !record->sheet_session) return fail(error,"Choose a complete manual range reservation.");
    RangeEventInfo info={0}; RangeEventStatus status=range_event_measure(record->view,record->validation_reserved,&info);
    if (status!=RANGE_EVENT_OK) return fail(error,"The complete range event or validation reservation is invalid.");
    WorkbookHistoryEditFence fence={record->workbook,record->edit_session,record->expected_revision,record->revision};
    if (!workbook_history_stream_reserve(context,fence,DX_HISTORY_RANGE_FAMILY,DX_RANGE_EVENT_ENCODING,info.bytes,0,0,0,ticket,error)) return false;
    size_t written=0;
    if (range_event_encode(record->view,record->validation_reserved,workbook_history_reservation_payload(*ticket),info.bytes,&written)!=RANGE_EVENT_OK || written!=info.bytes) {
        workbook_history_stream_cancel(context,*ticket); *ticket=NULL; return fail(error,"The complete range payload changed before admission.");
    }
    return true;
}
WorkbookHistoryRangeRecorder workbook_history_stream_range_recorder(WorkbookHistoryStream *stream) { return (WorkbookHistoryRangeRecorder){reserve_range,workbook_history_stream_publish,workbook_history_stream_cancel,stream}; }
