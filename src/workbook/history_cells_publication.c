#include "history_cells_publication.h"
WorkbookHistoryCellsStateStatus workbook_history_cells_state_publication(const WorkbookVersionPublication *publication,
    duckdb_connection connection,const WorkbookVersionCoreSet *after,const WorkbookHistoryAfterSave *plan,
    WorkbookHistoryCellStateEntry *scratch,size_t reserved,WorkbookHistoryCellsStateReport *out) {
    if (!publication || !plan || !out) return HISTORY_CELLS_STATE_INVALID;
    if (workbook_version_publication_intent(publication)!=VERSION_PUBLICATION_USER_SAVE) return HISTORY_CELLS_STATE_UNSUPPORTED;
    const Workbook *snapshot=workbook_version_publication_snapshot(publication,connection,after);
    const WorkbookHistoryCapturedPosition *captured=workbook_history_after_save_position(plan);
    const WorkbookVersionPlan *publication_plan=workbook_version_publication_plan(publication);
    if (!snapshot || workbook_history_after_save_roots(plan)!=after || !captured || !publication_plan ||
        captured->owner.session!=publication_plan->owner.session || captured->owner.mode!=publication_plan->owner.mode ||
        captured->revision!=workbook_version_publication_revision(publication) ||
        captured->owner.document_high!=snapshot->id.high || captured->owner.document_low!=snapshot->id.low)
        return HISTORY_CELLS_STATE_INVALID;
    return workbook_history_cells_state_check(workbook_history_after_save_profile(plan),snapshot,scratch,reserved,out);
}
