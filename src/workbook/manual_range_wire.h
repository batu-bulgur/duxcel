#ifndef DUXCEL_MANUAL_RANGE_WIRE_H
#define DUXCEL_MANUAL_RANGE_WIRE_H
#include "manual_value_wire.h"
#include "clipboard.h"
#define DX_MANUAL_RANGE_HEADER_BYTES 32u
#define DX_MANUAL_RANGE_VERSION 1u
typedef struct ManualRange ManualRange;
typedef struct {
    size_t rows,columns,count;
    const ManualValue *const *values; /* Row-major; NULL slot is sparse absence. */
} ManualRangeView;
typedef struct { void *data; size_t bytes; } ManualRangePacket;
typedef struct { size_t rows,columns,count,bytes,owned,heap_scratch; } ManualRangeInfo;
typedef enum { MANUAL_RANGE_OK,MANUAL_RANGE_INVALID,MANUAL_RANGE_LIMIT } ManualRangeStatus;
/* Current values-only DXMR1: canonical32-byte header (magic4, version1,
   zero3, LE rows/columns/count uint32, zero4, LE body bytes uint64), followed
   by count (LE uint32 frame bytes, exact current DXMV1 frame) records.
   Positive rows/columns, columns<=16384, rows*columns=count<=20,000; complete
   packet<=8MiB. Every coordinate is explicit, including missing holes. No
   ragged defaults/offsets/document/sheet/source/ref/formula/format identities.
   Values are transferable across documents; TEXT starting '=' stays text.
   Both measurement and full preflight are allocation-free; heap_scratch=0.
   Nested DXMV validation uses its fixed42-byte canonical workspace plus bounded
   scalar stack state, serially, not count multiplied. owned covers the new
   decoded header/slots and every independent present-value owner exactly. */
ManualRangeStatus manual_range_measure(ManualRangeView view,ManualRangeInfo *info);
ManualRangeStatus manual_range_preflight(const void *data,size_t bytes,ManualRangeInfo *info);
/* Complete prior packet.bytes + new info.bytes is reserved before allocation.
   Borrowed input value/slot owners are separate caller charges. */
ManualRangeStatus manual_range_capture(ManualRangeView view,size_t reserved,ManualRangePacket *out);
/* Complete prior manual_range_owned + new info.owned is admitted before any
   slot/value allocation. Borrowed frame remains a caller charge. Failure keeps
   prior out and every input unchanged, including late nested decode failures. */
ManualRangeStatus manual_range_decode(const void *data,size_t bytes,size_t reserved,ManualRange **out);
ManualRangeView manual_range_view(const ManualRange *range);
size_t manual_range_owned(const ManualRange *range);
bool manual_range_at(const ManualRange *range,size_t row,size_t column,const ManualValue **out);
void manual_range_free(ManualRange **range);
void manual_range_packet_free(ManualRangePacket *packet);
/* Explicitly LOSSY public fallback, using the existing clipboard text encoder:
   missing/SQLNULL -> empty unquoted field; empty TEXT -> quoted empty field;
   other TEXT exact, including literal NULL; NUMBER -> exact derived canonical
   dot-decimal/integer text, never display grouping/rounding or authored raw.
   Up to8MiB excluding final NUL. There is no type/presence/ragged/trailing-row
   round-trip guarantee: external cells_tsv_scan still caps fields at64KiB and
   one trailing newline adds no row. No parsing/inference/paste authorization.
   Future caller validates offsets/protected rectangle and adopts atomically.
   TSV measure returns complete new allocation bytes including NUL. Export
   admits strlen(*out)+1 (if any) + new bytes; no temporary pointer matrix/heap
   scratch. Output must be NULL or an independent owned NUL-terminated string. */
ManualRangeStatus manual_range_tsv_measure(ManualRangeView view,size_t *owned);
ManualRangeStatus manual_range_tsv(ManualRangeView view,size_t reserved,char **out);
#endif
