#ifndef DUXCEL_SOURCE_VERSIONS_CODEC_H
#define DUXCEL_SOURCE_VERSIONS_CODEC_H
#include "source_scope.h"
#define DX_SOURCE_VERSIONS_ENCODING 1u
#define DX_SOURCE_VERSIONS_HEADER_BYTES 32u
#define DX_SOURCE_VERSION_ROW_BYTES 28u
#define DX_SOURCE_VERSIONS_MAX_BYTES (DX_SOURCE_VERSIONS_BYTES+DX_SOURCE_VERSIONS_HEADER_BYTES)
typedef struct { uint64_t sheet; SourceVersions versions; } SourceVersionsAsset;
typedef enum {
    SOURCE_VERSIONS_CODEC_OK,SOURCE_VERSIONS_CODEC_INVALID,SOURCE_VERSIONS_CODEC_LIMIT,SOURCE_VERSIONS_CODEC_UNSUPPORTED
} SourceVersionsCodecStatus;
/* Closed metadata asset, not source-file/dataset bytes or a .dxls version.
   DXSV/u16 encoding/u16 zero flags/u64 sheet/u64 captured high-water/u16 count/
   u16 zero/u32 zero, followed by ordered rows: u64 version/owner, u8 kind
   (CSV=1, Parquet=2), u8 zero/u16 zero/u32 locator length/u32 RAW packet length,
   exact UTF-8 locator and binary provenance/schema packet. RAW length0 means
   pending, not an embedded empty dataset. No native structs/capacity/terminators.
   Paths are preserved, never resolved/queried. Owner bounds and packet identity
   are checked; file/Page verification, document binding and asset digest are
   separate. Failure preserves outputs. */
SourceVersionsCodecStatus source_versions_asset_size(uint64_t sheet,const SourceVersions *versions,uint64_t object_high_water,size_t *bytes);
SourceVersionsCodecStatus source_versions_asset_encode(uint64_t sheet,const SourceVersions *versions,uint64_t object_high_water,
    void *arena,size_t capacity,size_t *written);
/* Framing/owned-memory preflight precedes allocation. reserved covers new rows,
   NUL-terminated locators and copied RAW packets; input/old out are separately
   owned. out is zero-initialized or owns an independent valid asset. Success
   replaces it only after full ledger/packet validation; failure preserves it.
   Captured high-water is an obligation, NEVER an Undo value to assign over a
   newer live counter. The final owner must preserve known immutable completions,
   required rows and maximum allocator bounds when projecting historical state.
   No IO, live-ledger adoption, version issuance, replay or activation proof. */
SourceVersionsCodecStatus source_versions_asset_decode(const void *data,size_t bytes,uint64_t object_high_water,
    size_t reserved,SourceVersionsAsset *out);
void source_versions_asset_free(SourceVersionsAsset *asset);
#endif
