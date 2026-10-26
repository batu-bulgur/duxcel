#ifndef DUXCEL_HISTORY_AFTER_SAVE_H
#define DUXCEL_HISTORY_AFTER_SAVE_H
#include "history_native_capture.h"
#include "core_compare.h"
typedef struct WorkbookHistoryAfterSave WorkbookHistoryAfterSave;
typedef struct { WorkbookHistoryNativeControl verify; WorkbookCoreCompareControl compare; } WorkbookHistoryAfterSaveControl;
/* Independent CURRENT after-USER_SAVE preparation. The source capsule is a true
   immutable BORROW: its caller must keep it owned/alive until plan cancellation
   or the future genuine publication handoff. Source is never consumed here.
   The prepared after-CoreSet is a real existing root-stage/read owner; it can be
   admitted BEFORE the component is staged because root admission does not claim
   complete-container admission. The plan retains that owner and independently
   owns the after-profile; all failures preserve source/input/output ownership.
   The canonical existing User Save plan, exact after-index/tag/root preservation,
   original sealed R and ACTUALLY selected Saved leaf are checked together.
   A new root must be original R bytes. Existing-root reuse requires exact bytes
   or existing complete-core comparison, never doc/revision alone. This proves
   capture-to-selected-core equality, not retained event applicability.
   First Saved allocates anchor0. Changed Saved allocates the next high-water ID;
   unchanged Saved reuses its exact boundary. All tag bindings remain unchanged.
   Saved/current use the entire current path including Redo, with unchanged
   origin/available/counters and exact unique transition bytes. Previous Saved
   branches remain independently retained public checkpoints; current history
   is not narrowed or evicted. No roots/tags are removed by this preparation.
   profile_caps.staging_bytes charges old/new profile coexistence, descriptor
   staging, this wrapper and max sequential decoder scratch. NativeCaps bounds
   old/new physical extents conservatively including shared roles, and additional
   wrapper/descriptor staging. Engine compare residency remains separately bounded
   by its existing owner. No SQL writes, normal registry activation or serializer.
   There is deliberately NO generic-receipt finalizer: a generic archive receipt
   does not prove USER_SAVE. Final consumption/publication awaits a typed owner
   minted INSIDE the real User Save controller and actual-state applicability
   proof. Stale UI adoption must never relabel a committed Save as file failure. */
WorkbookHistoryNativeStatus workbook_history_after_save_prepare(const WorkbookHistoryNativeCapture *source,
    const WorkbookVersionCoreSet *prepared_after,const WorkbookVersionPlan *save,
    WorkbookHistoryProfileCaps profile_caps,WorkbookHistoryNativeCaps native_caps,
    WorkbookHistoryAfterSaveControl control,WorkbookHistoryAfterSave **out,char **error);
const WorkbookHistoryProfile *workbook_history_after_save_profile(const WorkbookHistoryAfterSave *plan);
const WorkbookVersionCoreSet *workbook_history_after_save_roots(const WorkbookHistoryAfterSave *plan);
const WorkbookCorePayload *workbook_history_after_save_selected(const WorkbookHistoryAfterSave *plan);
const WorkbookHistoryCapturedPosition *workbook_history_after_save_position(const WorkbookHistoryAfterSave *plan);
bool workbook_history_after_save_semantic_reuse(const WorkbookHistoryAfterSave *plan);
void workbook_history_after_save_free(WorkbookHistoryAfterSave **plan);
#endif
