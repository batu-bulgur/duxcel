#ifndef DUXCEL_HISTORY_ANCHORS_H
#define DUXCEL_HISTORY_ANCHORS_H
#include "history_window.h"
#define DX_HISTORY_PUBLIC_ANCHORS (DX_VERSION_TAGS+1u)
typedef struct {
    WorkbookHistoryBoundary boundary;
    /* Opaque operation-path identity; event/path admission proves membership
       and position bounds separately. This is not a filesystem locator. */
    uint64_t lineage;
} WorkbookHistoryAnchor;
typedef struct {
    uint64_t tag_created; /* Existing immutable tag publication identity. */
    uint64_t anchor;
} WorkbookHistoryTagAnchor;
typedef struct {
    uint64_t document_high,document_low,high_water,saved_anchor;
    const WorkbookHistoryAnchor *items;
    size_t count;
    const WorkbookHistoryTagAnchor *tags;
    size_t tag_count;
} WorkbookHistoryAnchors;
typedef enum { HISTORY_ANCHORS_OK,HISTORY_ANCHORS_INDEX,HISTORY_ANCHORS_INVALID,HISTORY_ANCHORS_LIMIT } WorkbookHistoryAnchorsStatus;
/* Pure closed binding proof for a proposed durable-history metadata projection.
   Every tag and Saved resolves to exactly one matching root/anchor; all public
   anchors are referenced and IDs fit the separately retained high-water mark.
   Different anchors may share content or an operation position. Aliased labels
   explicitly share an anchor, never infer one from a hash or label spelling.
   Borrowed arrays only, no allocation/IO/model mutation. This is not a file
   format declaration, event/path proof, payload decoder, replay or GC authority.
   An admitted v16 index alone contains none of this history metadata. */
WorkbookHistoryAnchorsStatus workbook_history_anchors_check(const WorkbookVersionIndex *versions,const WorkbookHistoryAnchors *anchors);
#endif
