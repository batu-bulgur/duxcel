#ifndef DUXCEL_CELL_DELTA_H
#define DUXCEL_CELL_DELTA_H
#include "../common.h"
#define DX_CELL_DELTA_TEXT_BYTES 65536u
typedef struct {
    uint64_t sheet,row;
    size_t column;
    const char *before,*after;
    size_t before_bytes,after_bytes;
} CellDelta;
typedef enum { CELL_DELTA_READY,CELL_DELTA_NOOP,CELL_DELTA_INVALID,CELL_DELTA_LIMIT } CellDeltaStatus;
/* Exact sparse manual-text/clear payload, independent of session/recovery clocks.
   Empty span means clear, not typed SQL NULL or a zero-length source value.
   The outer document/event envelope owns document identity and operation ID.
   Spans are borrowed/readable; embedded NUL rejects, other bytes are preserved.
   On READY bytes receives the exact required arena length; otherwise unchanged.
   No allocation, model/history mutation, file-format or durable Undo claim. */
CellDeltaStatus cell_delta_check(CellDelta input,size_t *bytes);
/* Caller owns a fresh, disjoint writable arena of capacity bytes. Copy both
   values before mutation/eviction and return a view borrowing ONLY that arena.
   Failure/NOOP preserves out and arena. No terminators are appended. The owner
   keeps the arena alive through all readers, then releases it exactly once. */
CellDeltaStatus cell_delta_capture(CellDelta input,void *arena,size_t capacity,CellDelta *out);
/* Borrowed inverse; target identity and exact bytes are unchanged. */
CellDelta cell_delta_inverse(CellDelta delta);
#endif
