#ifndef DUXCEL_HISTORY_CELLS_PUBLICATION_H
#define DUXCEL_HISTORY_CELLS_PUBLICATION_H
#include "history_cells_state.h"
#include "history_after_save.h"
/* Actual frozen-R adapter for the active genuine USER_SAVE prepare callback.
   The SAME controller snapshot, exact after-CoreSet and independently prepared
   after-Save profile are required. No recapture, model load, allocation, SQL or
   generic receipt authorization occurs. Caller keeps all borrowed owners alive
   and charges existing snapshot/profile plus the cell-state overlay separately.
   Failure preserves out. Success proves ONLY the current family1 interval;
   Saved/forks/checkpoints/other families and usable retained Undo remain outside
   this child. It does not stage/advertise history or grant COMMIT authority. */
WorkbookHistoryCellsStateStatus workbook_history_cells_state_publication(
    const WorkbookVersionPublication *publication,duckdb_connection connection,
    const WorkbookVersionCoreSet *after,const WorkbookHistoryAfterSave *plan,
    WorkbookHistoryCellStateEntry *scratch,size_t reserved,WorkbookHistoryCellsStateReport *out);
#endif
