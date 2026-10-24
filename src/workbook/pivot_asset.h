#ifndef DUXCEL_PIVOT_ASSET_H
#define DUXCEL_PIVOT_ASSET_H
#include "definition_wire.h"
#include "../transforms/pivot.h"
#define DX_PIVOT_ASSET_ENCODING 1u
#define DX_PIVOT_ASSET_HEADER_BYTES 32u
#define DX_PIVOT_ASSET_BODY_BYTES 8u
#define DX_PIVOT_ASSET_MAX_BYTES (DX_DEFINITION_OWNED_BYTES+1024u)
typedef struct { uint64_t sheet,owner; PivotDefinition definition; } PivotAsset;
typedef DefinitionWireMeasure PivotAssetMeasure;
typedef enum { PIVOT_ASSET_OK,PIVOT_ASSET_INVALID,PIVOT_ASSET_LIMIT,PIVOT_ASSET_UNSUPPORTED } PivotAssetStatus;
/* Closed Pivot/Chart query-definition asset, not output cells/source relation.
   DXPV/u16 encoding/u16 zero/u64 sheet/u64 positive object owner/u32 body length/
   u32 zero. Body: four u8 role counts in Rows/Columns/Values/Filters order, u32
   zero. Each ordered field: u8 aggregate/filter/date-grain/zero, nullable UTF-8
   name/value spans(u32 length or UINT32_MAX, exact bytes). Names must be nonempty.
   Date-grain and duplicate-shelf rules remain pivot_valid's authority. Unused
   slots, native structs and schema-derived NumberType are absent. Typed filters
   require verified schema rebinding before execution. Outer Object closure owns
   kind/geometry/source links; decoding does not prove that owner or execute SQL. */
/* Allocation-free bounded framing/text/tag measure. Full duplicate/grammar
   validation follows admitted scratch in encode/decode. Failure preserves out. */
PivotAssetStatus pivot_asset_measure(uint64_t sheet,uint64_t owner,const PivotDefinition *definition,uint64_t object_high_water,PivotAssetMeasure *out);
/* Fresh disjoint arena. Failure preserves arena/written; validation_reserved
   covers parser scratch separately from input/output ownership. */
PivotAssetStatus pivot_asset_encode(uint64_t sheet,uint64_t owner,const PivotDefinition *definition,uint64_t object_high_water,
    size_t validation_reserved,void *arena,size_t capacity,size_t *written);
/* Allocation-free framing/text/enum and owned/scratch inspection for a
   composed owner's aggregate budget BEFORE allocating any nested graph. This
   does NOT prove full definition grammar/duplicate/query rules or owner/source
   closure. Only successful decode provides the complete family validation.
   Input remains immutable throughout the composed preflight/decode; failure
   preserves info. */
PivotAssetStatus pivot_asset_preflight(const void *data,size_t bytes,uint64_t object_high_water,DefinitionAssetInfo *info);
/* Two-pass owned+scratch preflight; atomic independent graph replacement.
   reserved includes sizeof(PivotDefinition), text terminators and grammar scratch.
   No IO, output materialization, live adoption or App/file-profile activation. */
PivotAssetStatus pivot_asset_decode(const void *data,size_t bytes,uint64_t object_high_water,size_t reserved,PivotAsset *out);
void pivot_asset_free(PivotAsset *asset);
#endif
