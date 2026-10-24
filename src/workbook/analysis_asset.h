#ifndef DUXCEL_ANALYSIS_ASSET_H
#define DUXCEL_ANALYSIS_ASSET_H
#include "definition_wire.h"
#define DX_ANALYSIS_ASSET_ENCODING 1u
#define DX_ANALYSIS_ASSET_HEADER_BYTES 32u
#define DX_ANALYSIS_ASSET_BODY_BYTES 16u
#define DX_ANALYSIS_ASSET_OWNED_BYTES DX_DEFINITION_OWNED_BYTES
#define DX_ANALYSIS_ASSET_MAX_BYTES (DX_ANALYSIS_ASSET_OWNED_BYTES+1024u)
typedef struct { uint64_t sheet,owner; Analysis definition; } AnalysisAsset;
typedef DefinitionWireMeasure AnalysisAssetMeasure;
typedef enum { ANALYSIS_ASSET_OK,ANALYSIS_ASSET_INVALID,ANALYSIS_ASSET_LIMIT,ANALYSIS_ASSET_UNSUPPORTED } AnalysisAssetStatus;
/* Closed definition asset, not data/results or an executable relation. Owner0
   is the worksheet primary source; positive owner IDs require outer Table proof.
   DXAN/u16 encoding/u16 zero/u64 sheet/u64 owner/u32 body length/u32 zero.
   Body: u8 kind/flags(unique=1,descending=2)/filter-op/aggregate, u64 source-version,
   u8 extra-filter/extra-sort counts/u16 zero. Nine nullable texts follow in model
   order, then extra filters(u8 op/3 zero/column/value), then extra sorts(u8 flags/
   3 zero/column). Texts use u32 length or UINT32_MAX for NULL, exact UTF-8 bytes,
   no terminators. NULL, empty and literal NULL are distinct. No native structs,
   unused array slots, NumberType schema caches, runtime clocks or display defaults.
   Filter/sort display order is preserved even when SQL results would be equal.
   Deriving executable typed filters requires verified source/schema rebinding;
   decoding never activates a source or resets its allocation high-water. */
/* Allocation-free measurement checks bounded shape/text, query and sort rules;
   full filter grammar validation occurs in encode/decode AFTER scratch admission.
   All failures preserve out. owned includes sizeof(Analysis) and text terminators;
   scratch follows the existing bounded BETWEEN/COMPOSITE parser allocation model. */
AnalysisAssetStatus analysis_asset_measure(uint64_t sheet,uint64_t owner,const Analysis *definition,
    uint64_t object_high_water,AnalysisAssetMeasure *out);
/* Fresh disjoint arena/written; failure preserves both. validation_reserved is
   separate from borrowed model/output bytes and must cover measured scratch. */
AnalysisAssetStatus analysis_asset_encode(uint64_t sheet,uint64_t owner,const Analysis *definition,uint64_t object_high_water,
    size_t validation_reserved,void *arena,size_t capacity,size_t *written);
/* Two-pass framing/owned+scratch preflight before any string allocation. out
   owns an independent asset or is zero. Failure preserves it; success owns all
   texts independently and clears derived NumberType caches for later rebinding.
   No FS/SQL, locator resolution, dataset retention, live adoption or owner proof. */
AnalysisAssetStatus analysis_asset_decode(const void *data,size_t bytes,uint64_t object_high_water,size_t reserved,AnalysisAsset *out);
void analysis_asset_free(AnalysisAsset *asset);
#endif
