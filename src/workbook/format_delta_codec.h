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
typedef struct { uint64_t sheet; size_t before_bytes,after_bytes,owned,validation_scratch; } FormatDeltaInfo;
typedef enum { FORMAT_DELTA_OK,FORMAT_DELTA_NOOP,FORMAT_DELTA_INVALID,FORMAT_DELTA_LIMIT,FORMAT_DELTA_UNSUPPORTED } FormatDeltaStatus;
/* The existing DXFD encoding1 snapshot body, exposed for complete owned
   presence components. It is NOT a second archive/event or a changed-pair
   encoding: empty snapshots are valid, and no user action is manufactured.
   The enclosing current component must bind sheet/type/encoding/body length.
   Source ledger closure remains with that owner, exactly as for DXFD. */
typedef struct { size_t bytes,owned,validation_scratch; } FormatSnapshotBodyInfo;
/* Allocation-free structural/ownership measure; semantic format_valid follows
   admitted scratch. Failure preserves info. */
FormatDeltaStatus format_snapshot_body_measure(FormatSnapshotView view,FormatSnapshotBodyInfo *info);
/* Fresh arena/written storage must be disjoint from every borrowed graph.
   Encode validates complete semantics before writing either output. */
FormatDeltaStatus format_snapshot_body_encode(FormatSnapshotView view,size_t validation_reserved,
    void *arena,size_t capacity,size_t *written);
/* Framing inspection before ANY graph allocation. Input must remain immutable;
   complete semantic validation occurs on owned decode. Failure preserves out. */
FormatDeltaStatus format_snapshot_body_preflight(const void *data,size_t bytes,FormatSnapshotBodyInfo *info);
FormatDeltaStatus format_snapshot_body_decode(const void *data,size_t bytes,size_t reserved,FormatSnapshot *out);
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
/* Allocation-free closed framing/count/owned/scratch inspection for compound
   graph admission. Full selector semantics, uniqueness/change and source closure
   are NOT proved by inspection; successful decode remains mandatory. Failure
   preserves info. Input is immutable through the compound inspection/decode. */
FormatDeltaStatus format_delta_preflight(const void *data,size_t bytes,FormatDeltaInfo *info);
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
