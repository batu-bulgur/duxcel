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
/* Conservative additional allocation needed by an actual metadata command:
   first lifecycle ring plus rename normalization scratch. Existing live/history
   allocations and the separate canonical command plan remain caller charges.
   Only rename/reorder/hide/show are admitted; invalid input returns SIZE_MAX.
   No mutation/allocation. Recheck the same owner before applying the command. */
size_t workbook_lifecycle_metadata_staging(const Workbook *book,WorkbookSheetOperation operation);
/* Additional CREATE adoption peak from the shared real workbook_add allocation
   plan, the actual normalized-name bound and first lifecycle ring. Caller still
   charges live old arrays/definitions, candidate/draft/presence/retained packets
   and App/cache copies. No new ID/action, entropy, parsing or allocation occurs.
   This is NOT creation admission: normalization/uniqueness, closed grammar,
   owner/readiness and complete after-sheet/event fences remain separate.
   SIZE_MAX means absent/exhausted owner or invalid/overflowing bounded inputs. */
size_t workbook_lifecycle_create_staging(const Workbook *book,const char *name,const Analysis *analysis);
/* Conservative real DUPLICATE peak: shared producer clone/array/name bound
   plus the first lifecycle ring. Existing graph, plan and stream are separate.
   Invalid bounds/capacity/identity returns SIZE_MAX; no allocation/mutation. */
size_t workbook_lifecycle_duplicate_staging(const Workbook *book,size_t sheet);
/* Exact additional DELETE allocation: one detached Worksheet wrapper and, if
   absent, the first lifecycle ring. All owned contents move without a clone;
   existing live arrays/graphs, prepared payloads and streams are outer charges.
   Invalid target/last-visible removal returns SIZE_MAX. No mutation/allocation. */
size_t workbook_lifecycle_remove_staging(const Workbook *book,size_t sheet);
/* Borrow only the globally selected genuine presence action. The worksheet is
   live or owned by the lifecycle ring; no graph is cloned. Output is unchanged
   for non-presence, malformed or unavailable selections. A selected exhausted
   revision remains inspectable but is not replay admission. */
struct SheetTopologyPacket;
typedef struct {
    const void *owner;
    const Worksheet *worksheet;
    WorkbookSheetOperation operation;
    uint64_t sheet,order,active;
    size_t position;
    bool detached;
    const struct SheetTopologyPacket *packet; /* Borrowed original; NULL for plain session events. */
} WorkbookLifecyclePresence;
bool workbook_lifecycle_presence(const Workbook *book,bool redo,WorkbookLifecyclePresence *out);
/* Actual presence transfer allocation bound: replacement array + serial global
   format-validation scratch for restoration, one wrapper for detachment. */
size_t workbook_lifecycle_presence_staging(const Workbook *book,bool redo);
/* Coordinator internals: session-only, never copied into a Save snapshot. */
void workbook_lifecycle_free(Workbook *book);
void workbook_lifecycle_clear_redo(Workbook *book);
uint64_t workbook_lifecycle_latest(const Workbook *book);
/* Branch high-water also retains constituent history while a Worksheet is
   detached. This is not the newest lifecycle action clock above. */
uint64_t workbook_lifecycle_retained_latest(const Workbook *book);
FormatBudgetStatus workbook_lifecycle_format_usage(const Workbook *book,size_t *retained);
uint64_t workbook_lifecycle_floor(const Workbook *book);
bool workbook_lifecycle_peek(const Workbook *book,bool redo,WorkbookHistoryResult *result,char **error);
WorkbookHistoryResult workbook_lifecycle_step(Workbook *book,bool redo,char **error);
#endif
