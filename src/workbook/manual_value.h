#ifndef DUXCEL_MANUAL_VALUE_H
#define DUXCEL_MANUAL_VALUE_H
#include "../transforms/number.h"
/* Bounds count authored/canonical bytes, excluding the terminating NUL. */
#define DX_MANUAL_VALUE_TEXT_BYTES (4u*1024u*1024u)
#define DX_MANUAL_VALUE_NUMBER_BYTES 4096u
#define DX_MANUAL_VALUE_CANONICAL_BYTES 41u
typedef enum { MANUAL_VALUE_SQL_NULL=0,MANUAL_VALUE_TEXT,MANUAL_VALUE_NUMBER } ManualValueKind;
typedef struct ManualValue ManualValue;
typedef struct {
    bool present; /* false is sparse absence, not a stored blank value kind. */
    ManualValueKind kind;
    NumberType number;
    const char *raw; size_t raw_bytes; /* Borrowed span; no terminator required. */
} ManualValueInput;
typedef struct {
    bool present;
    ManualValueKind kind;
    NumberType number;
    const char *raw,*canonical;
    size_t raw_bytes,canonical_bytes,owned;
} ManualValueView;
typedef struct { size_t owned,raw_bytes,canonical_bytes; } ManualValueInfo;
typedef enum { MANUAL_VALUE_OK,MANUAL_VALUE_INVALID,MANUAL_VALUE_RANGE,
    MANUAL_VALUE_UNSUPPORTED,MANUAL_VALUE_LIMIT } ManualValueStatus;
/* Explicit typed input only: text is never inferred as a number/NULL/formula.
   Missing input is the zero shape; explicit NULL has present=true and otherwise
   the same zero shape. Text requires NUM_NONE and strict NUL-free UTF-8, including
   present zero-length text. SQL NULL never carries authored text.
   Integer types use existing signed/unsigned8..128 widths with precision/scale0;
   DECIMAL uses precision1..38/scale0..precision. NUM_FLOAT is unsupported.
   Numeric grammar is ASCII [+-]?[0-9]+ (unsigned forbids '-') or DECIMAL's
   [+-]?[0-9]+(\.[0-9]+)?. No whitespace/grouping/exponent/locale conversion.
   Canonical text removes leading zeroes/'+', suppresses negative zero and pads
   DECIMAL to its explicit scale. Excess fractional digits reject, even zeroes;
   no rounding. Authored raw bytes always stay exact and separate from the
   derived canonical representation. Display formatting belongs elsewhere.
   Allocation-free validation uses at most42 canonical stack bytes and bounded
   counters, never a heap/parser grant. Failure preserves info/input. */
ManualValueStatus manual_value_measure(ManualValueInput input,ManualValueInfo *info);
/* Complete new ownership is admitted BEFORE allocation/copy. out is NULL or an
   independent closed owner. reserved/owned cover only the complete NEW owner;
   old output and borrowed input remain caller-owned coexistence charges.
   Failure preserves out. Build may borrow raw from the
   old output; allocation/copy precedes its release. NULL owner represents sparse
   absence; explicit NULL is a present owner with no text payload. Text raw and
   canonical share one owned span; numeric representations are separate within
   one allocation. This is no CellStore/command/wire/clipboard/UI activation. */
ManualValueStatus manual_value_build(ManualValueInput input,size_t reserved,ManualValue **out);
/* Exact copy, never a reparse or raw-input rewrite; safe when source==*out. */
ManualValueStatus manual_value_clone(const ManualValue *source,size_t reserved,ManualValue **out);
ManualValueView manual_value_view(const ManualValue *value); /* Borrow ends at free/replacement. */
/* Authored state equality includes presence, kind/type, exact raw and canonical
   bytes. Different raw spellings remain different states even for equal numbers. */
bool manual_value_equal(const ManualValue *a,const ManualValue *b);
void manual_value_free(ManualValue **value); /* Disposal reads no borrowed input. */
#endif
