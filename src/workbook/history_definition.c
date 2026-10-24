#include "history_definition.h"
#include "history_stream_internal.h"
#include "definition_event_internal.h"
#include "definition_baseline.h"
#include <stdlib.h>
typedef char DefinitionPayloadFitsStream[(DX_DEFINITION_EVENT_MAX_BYTES<DX_HISTORY_STREAM_BYTES) ? 1 : -1];
static bool fail(char **error,const char *message) {
    if (error) { free(*error); *error=dx_strdup(message); } return false;
}
static bool reserve_definition(void *context,const WorkbookHistoryDefinitionRecord *r,void **ticket,char **error) {
    if (!context || !r || !ticket || *ticket || !r->sheet_session) return fail(error,"Choose a complete definition history reservation.");
    if (definition_baseline_status(r->view.before)!=DEFINITION_BASELINE_READY || definition_baseline_status(r->view.after)!=DEFINITION_BASELINE_READY)
        return fail(error,"Resolve the source relation before admitting definition history; the prepared edit is kept.");
    DefinitionEventPlan plan={0}; DefinitionEventStatus status=definition_event_prepare(r->view,r->validation_reserved,&plan);
    if (status!=DEFINITION_EVENT_OK) return fail(error,status==DEFINITION_EVENT_LIMIT ? "Definition validation exceeds the admitted memory budget; the prepared edit is kept." :
        status==DEFINITION_EVENT_NOOP ? "The prepared definitions are unchanged; no history event was reserved." : "Definition history is incomplete or inconsistent; the prepared edit is kept.");
    WorkbookHistoryEditFence fence={r->workbook,r->edit_session,r->expected_revision,r->revision};
    if (!workbook_history_stream_reserve(context,fence,DX_HISTORY_DEFINITION_FAMILY,DX_DEFINITION_EVENT_ENCODING,plan.info.bytes,0,0,plan.info.inactive_scopes,ticket,error)) return false;
    size_t written=0;
    status=definition_event_encode_unpublished(&plan,r->validation_reserved,workbook_history_reservation_payload(*ticket),plan.info.bytes,&written);
    if (status!=DEFINITION_EVENT_OK || written!=plan.info.bytes) {
        workbook_history_stream_cancel(context,*ticket); *ticket=NULL;
        return fail(error,"The definition payload changed before admission; the prepared edit is kept.");
    }
    return true;
}
WorkbookHistoryDefinitionRecorder workbook_history_stream_definition_recorder(WorkbookHistoryStream *s) {
    return (WorkbookHistoryDefinitionRecorder){reserve_definition,workbook_history_stream_publish,workbook_history_stream_cancel,s};
}
