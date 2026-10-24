#ifndef DUXCEL_HISTORY_PATH_H
#define DUXCEL_HISTORY_PATH_H
#include "history_anchors.h"
/* Root-to-current ancestry spine. Each later segment inherits the preceding
   prefix at from, then owns transitions (from,through]. IDs are issued from a
   document-local monotonic lineage clock, never the session Undo order. */
typedef struct { uint64_t lineage,from,through; } WorkbookHistorySegment;
/* Structural projection only: position is the state AFTER this transition.
   Complete reversible payload decoding/closure is a separate admission step. */
typedef struct { uint64_t operation,lineage,position; } WorkbookHistoryEventPosition;
typedef struct {
    uint64_t document_high,document_low;
    const WorkbookHistorySegment *segments;
    size_t segment_count;
    const WorkbookHistoryEventPosition *events;
    size_t event_count;
    uint64_t lineage_high_water,operation_high_water;
    uint64_t origin,available,cursor,tail;
} WorkbookHistoryPath;
typedef enum {
    HISTORY_PATH_OK,HISTORY_PATH_INVALID,HISTORY_PATH_SEQUENCE,HISTORY_PATH_GAP,
    HISTORY_PATH_ANCHORS,HISTORY_PATH_LIMIT,HISTORY_PATH_FORKED_SAVED
} WorkbookHistoryPathStatus;
/* Borrowed readable arrays, no allocation/IO/mutation. Linear in supplied
   segments/events. The adapter preflights decoded array/byte budgets. Events
   exactly cover (available,tail], including Redo. Operation IDs increase along
   this path but may skip abandoned branches; zero/exhausted IDs remain valid.
   No root/payload/replay or checkpoint-only legacy-history claim. */
WorkbookHistoryPathStatus workbook_history_path_check(const WorkbookHistoryPath *path);
/* Join admitted public bindings to a structurally proved path, then run the
   existing three-boundary planner. Ancestor anchors up to a fork are on-path;
   abandoned suffixes/other branches stay independently retained checkpoints.
   Saved on an abandoned branch returns FORKED_SAVED, never an empty successful
   window: publication must separately protect that Saved path and this draft's
   branch. This function alone cannot admit either to a durable archive.
   Failure preserves out, all inputs and every checkpoint. */
WorkbookHistoryPathStatus workbook_history_path_window(const WorkbookVersionIndex *versions,
    const WorkbookHistoryAnchors *anchors,const WorkbookHistoryPath *path,WorkbookHistoryWindow *out);
#endif
