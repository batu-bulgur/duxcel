#ifndef DUXCEL_HISTORY_CELLS_STATE_H
#define DUXCEL_HISTORY_CELLS_STATE_H
#include "history_profile.h"
/* Disposable naturally aligned caller scratch; values borrow the admitted
   profile bytes or actual Workbook. No pointer escapes in the scalar report. */
typedef struct {
    uint64_t sheet,row;
    size_t column,bytes;
    const char *value;
} WorkbookHistoryCellStateEntry;
typedef struct { size_t undo,redo,undo_cells,redo_cells; } WorkbookHistoryCellsStateReport;
typedef enum { HISTORY_CELLS_STATE_OK,HISTORY_CELLS_STATE_INVALID,HISTORY_CELLS_STATE_UNSUPPORTED,
    HISTORY_CELLS_STATE_LIMIT,HISTORY_CELLS_STATE_MISMATCH } WorkbookHistoryCellsStateStatus;
/* Actual current-body applicability for precisely the admitted CURRENT path.
   Every selected slot must be current family1; other families refuse the whole
   interval before scratch writes. Undo walks cursor -> available and Redo walks
   cursor -> tail independently from the same actual body, preserving causal
   overwrites/clears and stable sheet IDs. No omitted event or speculative value
   is invented. The current sparse text codec's empty span means missing/clear.
   Actual Workbook is already model-admitted, immutable and borrowed; physical
   body/profile/cursor association remains the native outer owner's proof.
   Existing profile and live model remain charged separately. Measure allocates
   nothing and returns exact caller scratch bytes. Scratch must be disjoint from
   all inputs/out and naturally aligned for WorkbookHistoryCellStateEntry; it is
   disposable on every outcome. Failure preserves the scalar output and all
   input owners. There are no allocations, callbacks, mutations, session actions,
   IO, source execution or decoded graph clones. This does NOT prove Saved/fork
   paths, checkpoint equality, other families, source readiness or usable retained
   Undo; it grants no publication/activation authority. */
WorkbookHistoryCellsStateStatus workbook_history_cells_state_measure(const WorkbookHistoryProfile *profile,
    const Workbook *actual,size_t *bytes);
WorkbookHistoryCellsStateStatus workbook_history_cells_state_check(const WorkbookHistoryProfile *profile,
    const Workbook *actual,WorkbookHistoryCellStateEntry *scratch,size_t reserved,
    WorkbookHistoryCellsStateReport *out);
typedef struct {
    WorkbookHistoryCellsStateReport current,saved;
    uint64_t shared_position;
    size_t coordinate_checks; /* Includes repeated coordinates; not a unique count. */
} WorkbookHistoryCellsSavedReport;
/* Additionally prove both complete selected family1 intervals and full sparse
   cell equality at their shared durable fork, including cells never mentioned by
   a delta. Actual Saved body must be independently associated with the public
   Saved root by the native owner; Saved's start is its explicit anchor position,
   not an inferred hash or path cursor. Both models remain immutable. Non-cell
   authored graphs, other public checkpoints and physical association remain
   outside this projection proof. Scratch is the sum of both path entry bounds;
   output/input preservation and admission rules match the current-only API. */
WorkbookHistoryCellsStateStatus workbook_history_cells_saved_measure(const WorkbookHistoryProfile *profile,
    const Workbook *current,const Workbook *saved,size_t *bytes);
WorkbookHistoryCellsStateStatus workbook_history_cells_saved_check(const WorkbookHistoryProfile *profile,
    const Workbook *current,const Workbook *saved,WorkbookHistoryCellStateEntry *scratch,size_t reserved,
    WorkbookHistoryCellsSavedReport *out);
#endif
