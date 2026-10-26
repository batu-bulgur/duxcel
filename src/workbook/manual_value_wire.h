#ifndef DUXCEL_MANUAL_VALUE_WIRE_H
#define DUXCEL_MANUAL_VALUE_WIRE_H
#include "manual_value.h"
#define DX_MANUAL_VALUE_WIRE_VERSION 1u
#define DX_MANUAL_VALUE_WIRE_HEADER_BYTES 24u
#define DX_MANUAL_VALUE_WIRE_MAX_BYTES (DX_MANUAL_VALUE_WIRE_HEADER_BYTES+DX_MANUAL_VALUE_TEXT_BYTES)
typedef struct { void *data; size_t bytes; } ManualValuePacket;
typedef struct {
    size_t bytes,value_owned,raw_bytes,canonical_bytes;
    bool present;
    ManualValueKind kind;
    NumberType number;
} ManualValueWireInfo;
typedef enum { MANUAL_VALUE_WIRE_OK,MANUAL_VALUE_WIRE_INVALID,MANUAL_VALUE_WIRE_LIMIT } ManualValueWireStatus;
/* Only current DXMV1. Canonical24-byte header: magic[4], version1, presence1,
   kind1, number1, precision1, scale1, reserved-zero6, LE uint64 raw length.
   Closed kind tags0=NULL/1=TEXT/2=NUMBER; number tags0=none,1..5=signed8..128,
   6..10=unsigned8..128,11=DECIMAL. Raw follows without a terminating NUL.
   Store exact authored raw/type, NEVER a second canonical-value authority.
   Preflight fully validates framing/shape/raw through the manual-value parser;
   derived canonical bytes are only a new-owner allocation measure. Fixed parser
   scratch is unchanged (42 canonical bytes including NUL), with no heap charge.
   Unknown tags/version, reserved/trailing bytes and semantic faults reject.
   All failure outputs remain unchanged; all input is borrowed immutable. */
ManualValueWireStatus manual_value_wire_measure(ManualValueInput input,ManualValueWireInfo *info);
ManualValueWireStatus manual_value_wire_preflight(const void *data,size_t bytes,ManualValueWireInfo *info);
/* Encode to a disjoint already-admitted arena. Complete measure/capacity checks
   precede every write; failure preserves arena and written. No heap allocation.
   The source raw span and written storage must not overlap the destination. */
ManualValueWireStatus manual_value_wire_encode(ManualValueInput input,void *arena,size_t capacity,size_t *written);
/* reserved is prior packet.bytes + complete new frame.bytes. Admission precedes
   allocation/copy; input.raw may borrow the prior packet (including its raw
   subspan), so replacement frees it only after the complete new copy exists.
   Other input owners remain caller-owned coexistence charges. */
ManualValueWireStatus manual_value_wire_capture(ManualValueInput input,size_t reserved,ManualValuePacket *out);
/* reserved is prior value.owned + new value_owned. Decode recomputes canonical
   text with the SAME parser; no locale inference, conversion, rounding or IO.
   Borrowed frame residency remains a separate caller charge. Full preflight and
   admission precede new allocation; prior output survives every failure. */
ManualValueWireStatus manual_value_wire_decode(const void *data,size_t bytes,size_t reserved,ManualValue **out);
void manual_value_packet_free(ManualValuePacket *packet);
#endif
