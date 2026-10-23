#ifndef DUXCEL_WORKBOOK_LIFECYCLE_H
#define DUXCEL_WORKBOOK_LIFECYCLE_H
#include "history.h"
typedef enum { SHEET_CREATE,SHEET_DUPLICATE,SHEET_DELETE,SHEET_RENAME,SHEET_REORDER,SHEET_HIDE,SHEET_SHOW } WorkbookSheetOperation;
typedef struct {
    WorkbookSheetOperation operation;
    size_t sheet,destination;
    const char *name;
    const Analysis *analysis;
} WorkbookSheetCommand;
/* Owned-model command boundary. Callers capture live Analysis/view first.
   No-op/rejection preserves revision and Redo; success joins workbook chronology.
   Delete/create replay moves an entire sheet without cloning its data or IDs.
   At least one visible sheet remains. Initial document construction uses the
   low-level workbook_add API; this command requires an existing document. */
bool workbook_sheet_command(Workbook *book,WorkbookSheetCommand command,bool *changed,char **error);
/* Coordinator internals: session-only, never copied into a Save snapshot. */
void workbook_lifecycle_free(Workbook *book);
void workbook_lifecycle_clear_redo(Workbook *book);
uint64_t workbook_lifecycle_latest(const Workbook *book);
uint64_t workbook_lifecycle_floor(const Workbook *book);
bool workbook_lifecycle_peek(const Workbook *book,bool redo,WorkbookHistoryResult *result,char **error);
WorkbookHistoryResult workbook_lifecycle_step(Workbook *book,bool redo,char **error);
#endif
