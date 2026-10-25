#include "history_sheet_lifecycle.h"
#include "history_stream_internal.h"
#include <stdlib.h>
#include <string.h>
static bool fail(char **error,const char *message) { if (error) { free(*error); *error=dx_strdup(message); } return false; }
static uint64_t get(const unsigned char *p,unsigned n) { uint64_t v=0; for (unsigned i=0;i<n;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
static bool reserve(void *context,const WorkbookHistorySheetLifecycleRecord *r,void **ticket,char **error) {
    if (!context || !r || !ticket || *ticket || !r->sheet || !r->sheet_session || !r->packet)
        return fail(error,"Choose a complete workbook sheet metadata reservation.");
    SheetLifecycleInfo info;
    if (sheet_lifecycle_preflight(r->packet->data,r->packet->bytes,&info)!=SHEET_LIFECYCLE_OK)
        return fail(error,"Sheet metadata must contain a complete changed canonical pair.");
    const unsigned char *data=r->packet->data;
    if (get(data+8,8)!=r->workbook.high || get(data+16,8)!=r->workbook.low)
        return fail(error,"The sheet payload belongs to another document.");
    bool found=false; size_t at=DX_SHEET_LIFECYCLE_HEADER_BYTES;
    for (size_t i=0;i<info.count;i++) { found|=get(data+at,8)==r->sheet; at+=16+(size_t)get(data+at+12,4); }
    if (!found) return fail(error,"The sheet payload omits its command target.");
    WorkbookHistoryEditFence fence={r->workbook,r->edit_session,r->expected_revision,r->revision};
    if (!workbook_history_stream_reserve(context,fence,DX_HISTORY_SHEET_LIFECYCLE_FAMILY,DX_HISTORY_SHEET_LIFECYCLE_PROFILE,info.bytes,0,0,0,ticket,error)) return false;
    memcpy(workbook_history_reservation_payload(*ticket),data,info.bytes); return true;
}
WorkbookHistorySheetLifecycleRecorder workbook_history_stream_sheet_lifecycle_recorder(WorkbookHistoryStream *stream) {
    return (WorkbookHistorySheetLifecycleRecorder){reserve,workbook_history_stream_publish,workbook_history_stream_cancel,stream};
}
