#ifndef DUXCEL_FORMAT_DELTA_CODEC_H
#define DUXCEL_FORMAT_DELTA_CODEC_H
#include "format.h"
#define DX_FORMAT_DELTA_ENCODING 1u
#define DX_FORMAT_DELTA_HEADER_BYTES 24u
#define DX_FORMAT_SNAPSHOT_HEADER_BYTES 8u
#define DX_FORMAT_PATCH_HEADER_BYTES 34u
#define DX_FORMAT_DELTA_MAX_BYTES (DX_FORMAT_DELTA_HEADER_BYTES+2u*(DX_FORMAT_CURRENT_BYTES+DX_FORMAT_SNAPSHOT_HEADER_BYTES))
typedef struct {
    const FormatPatch *items;
    size_t count;
    const FormatScope *scopes;
    size_t scope_count;
    bool source_aware;
} FormatSnapshotView;
typedef struct { uint64_t sheet; FormatSnapshot before,after; } FormatDelta;
typedef enum { FORMAT_DELTA_OK,FORMAT_DELTA_NOOP,FORMAT_DELTA_INVALID,FORMAT_DELTA_LIMIT,FORMAT_DELTA_UNSUPPORTED } FormatDeltaStatus;
FormatSnapshotView format_delta_store_view(const FormatStore *store);
FormatSnapshotView format_delta_snapshot_view(const FormatSnapshot *snapshot);
/* Closed format payload family, not a .dxls version. DXFD/u16 encoding/u16 zero
   flags/u64 sheet/u32 before length/u32 after length, all little-endian. Each
   snapshot has mode/u8 zero/u16 patch count/u16 scope count/u16 zero; scope rows
   are u64 owner/u32 length/exact bytes. Patch headers contain u64 row bounds,
   u16 column bounds, u8 mask, six stored style bytes, u8 selector/guard count/zero,
   u16 record/field counts; vectors use u16 guards/fields and u64 record IDs.
   No padding, native structs, derived specified bits, live display defaults or
   SourceVersions high-water. Source/version closure and document/event/digest
   binding are outer admission obligations; opaque scopes are not activation proof.
   Valid borrowed graphs only; format_valid uses bounded temporary scratch for
   large scope tables. Failure/NOOP preserves *bytes. */
FormatDeltaStatus format_delta_encoded_size(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,size_t *bytes);
/* Fresh writable arena is disjoint from input graphs/written. Failure/NOOP
   preserves arena and *written; success writes only the measured prefix. */
FormatDeltaStatus format_delta_encode(uint64_t sheet,FormatSnapshotView before,FormatSnapshotView after,
    void *arena,size_t capacity,size_t *written);
/* Two-pass framing/memory preflight precedes owned graph allocation. reserved
   covers new arrays/definitions and peak format-validation scratch; input bytes
   and an existing out graph are separately caller-owned. Decode then validates
   full selectors/values/membership, uniqueness and semantic change. No-op records
   reject. out is zero-initialized or owns an independent valid delta; all failures
   preserve it. Success owns every nested byte/vector independently of input.
   Allocation exhaustion follows dx_alloc. No IO, model mutation or replay. */
FormatDeltaStatus format_delta_decode(const void *data,size_t bytes,size_t reserved,FormatDelta *out);
void format_delta_free(FormatDelta *delta);
#endif
