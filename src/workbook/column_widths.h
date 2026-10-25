#ifndef DUXCEL_COLUMN_WIDTHS_H
#define DUXCEL_COLUMN_WIDTHS_H
#include "../common.h"
#define DX_COLUMN_WIDTH_COLUMNS 16384u
#define DX_COLUMN_WIDTH_DEFAULT_BITS UINT32_C(0x42c00000)
#define DX_COLUMN_WIDTH_ENCODING 1u
#define DX_COLUMN_WIDTH_HEADER_BYTES 20u
#define DX_COLUMN_WIDTH_MAX_BYTES (DX_COLUMN_WIDTH_HEADER_BYTES+12u*DX_COLUMN_WIDTH_COLUMNS)
typedef struct { uint16_t column; uint32_t bits; } ColumnWidthEntry;
typedef struct { const ColumnWidthEntry *entries; size_t count; } ColumnWidthView;
typedef struct { ColumnWidthEntry *entries; size_t count; } ColumnWidthState;
typedef struct { ColumnWidthState before,after; } ColumnWidthDelta;
typedef struct { ColumnWidthView before,after; } ColumnWidthPair;
typedef struct { void *data; size_t bytes; } ColumnWidthPacket;
typedef struct { size_t bytes,owned,before_count,after_count; } ColumnWidthInfo;
typedef enum { COLUMN_WIDTH_OK,COLUMN_WIDTH_NOOP,COLUMN_WIDTH_INVALID,COLUMN_WIDTH_LIMIT,
    COLUMN_WIDTH_UNSUPPORTED,COLUMN_WIDTH_STALE,COLUMN_WIDTH_EMPTY } ColumnWidthStatus;
/* Fixed virtual domain, not a rendered/materialized prefix. Entries are sorted,
   unique, finite binary32 widths in52..1200, excluding implicit default96.
   All ownership here is independent of Grid, sources, cells and event clocks. */
ColumnWidthView column_width_view(const ColumnWidthState *state);
bool column_width_valid(ColumnWidthView view);
bool column_width_equal(ColumnWidthView a,ColumnWidthView b);
uint32_t column_width_bits(float width);
float column_width_value(uint32_t bits);
uint32_t column_width_at(ColumnWidthView view,size_t column);
size_t column_width_owned(ColumnWidthView view);
/* A dense prefix is only a representation: its missing virtual tail is96.
   Caller retains input; success owns a canonical independent array. reserved
   bounds newly allocated array bytes; existing out is separately charged.
   All failures/NOOP preserve out. Free only independent owned states. */
ColumnWidthStatus column_width_capture(const float *widths,size_t count,size_t reserved,ColumnWidthState *out);
ColumnWidthStatus column_width_copy(ColumnWidthView view,size_t reserved,ColumnWidthState *out);
ColumnWidthStatus column_width_resize(ColumnWidthView before,size_t column,float width,size_t reserved,ColumnWidthState *out);
/* Pure structural projection; the complete before asset retains any width
   deleted or truly truncated at virtual column16383. Cached-prefix edges do
   not truncate. No cells/formats/worksheet action or producer is implied. */
ColumnWidthStatus column_width_shift(ColumnWidthView before,size_t column,bool insert,size_t reserved,ColumnWidthState *out);
void column_width_free(ColumnWidthState *state);
/* DXCW/u16 encoding/u16 zero/u32 virtual16384/u16 before+after counts/u32 zero,
   then both complete states as u16 column/u32 exact IEEE754 bits, little-endian.
   No default entries, padding, epsilon, historical revision or trailing bytes.
   All APIs reject unchanged pairs; two-pass decode owns independent arrays.
   Encode arena prefix/written scalar must be disjoint from both input arrays
   and each other; overlap rejects before any output/input byte is changed.
   Failure preserves output and encode arena; inverse is borrowed side swapping. */
ColumnWidthStatus column_width_delta_size(ColumnWidthView before,ColumnWidthView after,size_t *bytes);
ColumnWidthStatus column_width_encode(ColumnWidthView before,ColumnWidthView after,void *arena,size_t capacity,size_t *written);
ColumnWidthStatus column_width_preflight(const void *data,size_t bytes,ColumnWidthInfo *out);
ColumnWidthPair column_width_delta_view(const ColumnWidthDelta *delta);
ColumnWidthPair column_width_inverse(ColumnWidthPair pair);
ColumnWidthStatus column_width_extract(const void *data,size_t bytes,bool after,size_t reserved,ColumnWidthState *out);
ColumnWidthStatus column_width_decode(const void *data,size_t bytes,size_t reserved,ColumnWidthDelta *out);
void column_width_delta_free(ColumnWidthDelta *delta);
/* Exact allocation-free side guard for already canonical immutable packets. */
bool column_width_matches(const void *data,size_t bytes,bool after,ColumnWidthView current);
#endif
