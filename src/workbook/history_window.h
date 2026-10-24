#ifndef DUXCEL_HISTORY_WINDOW_H
#define DUXCEL_HISTORY_WINDOW_H
#include "version_index.h"
#define DX_HISTORY_WINDOW_STATES 3u
typedef struct {
    /* Ordered durable version-boundary identity, separate from tag publication/index
       generation. Same-state aliases share it; presentation-only states may
       have distinct anchors at one operation position. Issuance must observe
       history/presentation changes even when content is reused; unchanged Save
       or tag aliases can share the existing anchor. Zero is valid. */
    uint64_t anchor;
    /* Stable position on the proved current operation path, not a process
       command revision, label or hash-derived identity. Zero is valid. */
    uint64_t position;
    WorkbookVersionKey root;
} WorkbookHistoryBoundary;
typedef struct {
    /* The storage adapter proves path membership and root closure before this
       pure planner. Include named boundaries on this path only, in any order.
       Anchors cannot be newer than the Saved boundary. Other branches' tags
       remain independently protected/openable checkpoints. */
    const WorkbookHistoryBoundary *named;
    size_t count;
    bool has_saved;
    WorkbookHistoryBoundary saved;
    uint64_t origin,available,cursor,tail;
} WorkbookHistoryWindowInput;
typedef struct {
    WorkbookHistoryBoundary states[DX_HISTORY_WINDOW_STATES];
    size_t count;
    /* Required reversible interval, inclusive endpoints. It can extend before
       the third boundary to preserve a cursor already undone further back.
       through includes current Redo and the unsaved suffix, never just Saved. */
    uint64_t from,cursor,through;
} WorkbookHistoryWindow;
typedef enum { HISTORY_WINDOW_OK,HISTORY_WINDOW_INVALID,HISTORY_WINDOW_LIMIT,HISTORY_WINDOW_GAP } WorkbookHistoryWindowStatus;
/* Three explicit version boundaries ending at Latest Saved, not three
   intervals. Labels at the same explicit anchor share one boundary; identical
   content at different anchors does not erase intervening history.
   Fewer than three states retain all history from the declared path origin.
   A missing required interval reports GAP; no budget silently narrows it.
   No allocation, IO, model mutation, pruning or durable-history availability
   claim. On failure out is unchanged. Caller retains every named root/reader
   and proves actual event/payload continuity separately before publication. */
WorkbookHistoryWindowStatus workbook_history_window(WorkbookHistoryWindowInput input,WorkbookHistoryWindow *out);
#endif
