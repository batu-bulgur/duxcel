#ifndef DUXCEL_HISTORY_STREAM_INTERNAL_H
#define DUXCEL_HISTORY_STREAM_INTERNAL_H
#include "history_stream.h"
/* Private typed-producer boundary. No arbitrary public payload admission. */
typedef struct {
    WorkbookId workbook;
    uint64_t edit_session,expected_revision,revision;
} WorkbookHistoryEditFence;
bool workbook_history_stream_reserve(WorkbookHistoryStream *stream,WorkbookHistoryEditFence fence,
    unsigned family,unsigned encoding,size_t bytes,unsigned dependency_encoding,size_t dependency_bytes,
    size_t inactive_scopes,void **ticket,char **error);
void *workbook_history_reservation_payload(void *ticket);
void *workbook_history_reservation_dependencies(void *ticket);
void workbook_history_stream_publish(void *context,void *ticket);
void workbook_history_stream_cancel(void *context,void *ticket);
#endif
