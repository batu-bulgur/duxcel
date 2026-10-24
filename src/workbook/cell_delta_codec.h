#ifndef DUXCEL_CELL_DELTA_CODEC_H
#define DUXCEL_CELL_DELTA_CODEC_H
#include "cell_delta.h"
#define DX_CELL_DELTA_ENCODING 1u
#define DX_CELL_DELTA_HEADER_BYTES 34u
typedef enum {
    CELL_DELTA_CODEC_OK,CELL_DELTA_CODEC_NOOP,CELL_DELTA_CODEC_INVALID,
    CELL_DELTA_CODEC_LIMIT,CELL_DELTA_CODEC_UNSUPPORTED
} CellDeltaCodecStatus;
/* Closed sparse-manual payload family, not a .dxls version or native struct.
   Bytes: DXCD magic; little-endian u16 encoding/u16 zero flags; u64 sheet/row;
   u16 column; u32 before/after lengths; exact nonterminated before/after bytes.
   No document/operation/session clocks, terminators, normalization or padding.
   Document/event binding and content digest verification belong to the outer
   storage adapter. Typed source NULL/empty values are outside this family.
   No allocation/IO/model mutation. NOOP/invalid preserves *bytes. */
CellDeltaCodecStatus cell_delta_encoded_size(CellDelta input,size_t *bytes);
/* Caller-owned writable arena is disjoint from inputs/out. Write exactly the
   measured length; larger capacity is untouched beyond it. Failure/NOOP leaves
   arena and initialized *written unchanged. */
CellDeltaCodecStatus cell_delta_encode(CellDelta input,void *arena,size_t capacity,size_t *written);
/* Strict closed decoder. All reserved bits, exact length and sparse value/target
   constraints are checked; no-op records reject. out borrows immutable bytes,
   never transient DuckDB pages after they are released. The owner must retain
   the complete input asset for every reader, or copy it before adoption.
   Unknown encoding rejects distinctly; failure leaves out unchanged. */
CellDeltaCodecStatus cell_delta_decode(const void *bytes,size_t length,CellDelta *out);
#endif
