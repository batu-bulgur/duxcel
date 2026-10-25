#ifndef DUXCEL_SHEET_LIFECYCLE_H
#define DUXCEL_SHEET_LIFECYCLE_H
#include "workbook.h"
#define DX_SHEET_LIFECYCLE_ENCODING 1u
#define DX_SHEET_LIFECYCLE_HEADER_BYTES 64u
#define DX_SHEET_LIFECYCLE_NAME_BYTES 4096u
#define DX_SHEET_LIFECYCLE_MAX_BYTES (DX_SHEET_LIFECYCLE_HEADER_BYTES+2u*DX_MAX_SHEETS*(16u+DX_SHEET_LIFECYCLE_NAME_BYTES))
typedef struct { uint64_t id; const char *name; bool hidden; } SheetLifecycleEntry;
typedef struct {
    WorkbookId document;
    uint64_t high_water,active; /* Stable active sheet ID, never array position. */
    const SheetLifecycleEntry *sheets;
    size_t count;
} SheetLifecycleState;
typedef struct { SheetLifecycleState before,after; } SheetLifecycle;
typedef struct { size_t bytes,owned,count; } SheetLifecycleInfo;
typedef struct SheetLifecyclePacket { void *data; size_t bytes; } SheetLifecyclePacket;
typedef enum { SHEET_LIFECYCLE_OK,SHEET_LIFECYCLE_NOOP,SHEET_LIFECYCLE_INVALID,
    SHEET_LIFECYCLE_LIMIT,SHEET_LIFECYCLE_UNSUPPORTED,SHEET_LIFECYCLE_STALE } SheetLifecycleStatus;
/* Metadata-only rename/reorder/hide foundation. Both sides contain the SAME
   document, allocator bound and complete stable ID set; no detached sheet data,
   source/results, runtime IDs, event envelope, user-action clock or App adoption.
   Names retain exact bytes, including existing surrounding spaces; case folding
   for uniqueness is ASCII-only, matching workbook naming. Persisted text uses
   the shared UTF-8 contract, 1..4096 bytes and no ASCII control characters.
   Each side has a visible active sheet and at least one visible sheet. */
/* Active-only navigation is a semantic NOOP, not an authored metadata event. */
/* One complete catalog, independent of a same-ID metadata pair. bytes is the
   exact ordered row encoding WITHOUT a DXSL/DXTP header; owned is a decoded
   entry array and terminated names. Valid unchanged catalogs return OK.
   This shares the real codec state validation/counting and allocates nothing.
   Failure preserves out; it does not change pair NOOP semantics. */
SheetLifecycleStatus sheet_lifecycle_state_measure(SheetLifecycleState state,SheetLifecycleInfo *out);
SheetLifecycleStatus sheet_lifecycle_measure(SheetLifecycle view,SheetLifecycleInfo *out);
/* DXSL/u16 encoding/u16 zero/document high+low/u64 allocator/u32 count/u32 zero,
   before+after active IDs/u32 before+after lengths, then ordered rows on each
   side: u64 ID/u32 hidden(0|1)/u32 name length/exact name bytes. Little-endian,
   no padding or trailing bytes. Capture reserved covers NEW packet bytes; decode
   reserved covers NEW wrapper/arrays/terminated names. Existing inputs/output
   and packets remain separately charged. All failure/NOOP preserves outputs. */
SheetLifecycleStatus sheet_lifecycle_capture(SheetLifecycle view,size_t reserved,SheetLifecyclePacket *out);
SheetLifecycleStatus sheet_lifecycle_preflight(const void *data,size_t bytes,SheetLifecycleInfo *out);
SheetLifecycleStatus sheet_lifecycle_decode(const void *data,size_t bytes,size_t reserved,SheetLifecycle *out);
void sheet_lifecycle_packet_free(SheetLifecyclePacket *packet);
/* Only call free on an independently decoded owner, never a borrowed view. */
void sheet_lifecycle_free(SheetLifecycle *owned);
/* Borrow actual workbook metadata into caller storage. No mutation/allocation;
   storage and Workbook outlive the returned view. Failure preserves both. */
SheetLifecycleStatus sheet_lifecycle_from_workbook(const Workbook *book,SheetLifecycleEntry storage[DX_MAX_SHEETS],SheetLifecycleState *out);
/* Pure authored-metadata admission, not replay. redo matches current against
   before; Undo matches after, independently of live active-sheet navigation.
   Live high-water may exceed captured but never regress. Success returns a
   BORROWED opposite side retaining live high-water and active ID when visible;
   otherwise its validated captured active ID is the fallback. target must be
   separate scalar storage, never an owning decoded state. Failure preserves it. Publication, revision/session fences,
   owned model preparation and durable event activation remain separate. */
SheetLifecycleStatus sheet_lifecycle_admit(SheetLifecycle view,SheetLifecycleState current,bool redo,SheetLifecycleState *target);
#endif
