#include "history_event_frame.h"
bool workbook_history_event_frame_valid(const WorkbookHistoryEventInfo *e) {
    return e && (e->owner.document_high || e->owner.document_low) && e->owner.session && e->owner.mode==VERSION_WORKING_HEAD &&
        e->edit_session && e->revision && e->action && e->transition && e->previous_position<UINT64_MAX &&
        e->position==e->previous_position+1 && (e->forked ? e->parent_lineage<e->lineage : e->parent_lineage==e->lineage);
}
