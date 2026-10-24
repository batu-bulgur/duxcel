#ifndef DUXCEL_MARKS_ASSET_H
#define DUXCEL_MARKS_ASSET_H
#include "definition_wire.h"
#include "../transforms/marks.h"
#define DX_MARKS_ASSET_ENCODING 1u
#define DX_MARKS_ASSET_HEADER_BYTES 32u
#define DX_MARKS_ASSET_BODY_BYTES 8u
#define DX_MARKS_ASSET_FIELDS ((DX_MARK_ROLES-2u)*DX_MARK_FIELDS+2u)
#define DX_MARKS_ASSET_OWNED_BYTES (sizeof(MarkDefinition)+DX_MARKS_ASSET_FIELDS*4097u)
#define DX_MARKS_ASSET_MAX_BYTES (DX_MARKS_ASSET_HEADER_BYTES+DX_MARKS_ASSET_BODY_BYTES+DX_MARKS_ASSET_FIELDS*(8u+4096u))
typedef struct { uint64_t sheet,owner; MarkDefinition definition; } MarksAsset;
typedef DefinitionWireMeasure MarksAssetMeasure;
typedef enum { MARKS_ASSET_OK,MARKS_ASSET_INVALID,MARKS_ASSET_LIMIT,MARKS_ASSET_UNSUPPORTED } MarksAssetStatus;
/* Closed ordered Marks binding asset, not a rendered chart or source proof.
   DXMK/u16 encoding/u16 zero/u64 sheet/u64 positive object owner/u32 body length/
   u32 zero; six u8 role counts(Color/Size/Label/Detail/Tooltip/Shape)/u16 zero;
   each field: u8 aggregate/date-grain/measure(0 or1)/zero then u32 name length
   and exact nonempty UTF-8 bytes. No native layouts or unused fixed slots.
   Size/Shape take one field; other roles take eight. All display order, latent
   aggregate values and measure/dimension distinctions survive independently of
   current chart capabilities. Outer Object/source/representation closure remains
   separate; this performs no IO, queries, live adoption or App activation. */
/* Measurement is allocation-free and includes full Marks semantics (no parser
   scratch). owned includes the inline definition and copied name terminators. */
MarksAssetStatus marks_asset_measure(uint64_t sheet,uint64_t owner,const MarkDefinition *definition,uint64_t object_high_water,MarksAssetMeasure *out);
/* Fresh disjoint arena. Every failure preserves arena and written. */
MarksAssetStatus marks_asset_encode(uint64_t sheet,uint64_t owner,const MarkDefinition *definition,uint64_t object_high_water,
    void *arena,size_t capacity,size_t *written);
/* Allocation-free framing/text/enum and owned/scratch inspection for a
   composed owner's aggregate budget BEFORE allocating any nested graph. This
   does NOT prove full definition grammar/duplicate/query rules or owner/source
   closure. Only successful decode provides the complete family validation.
   Input remains immutable throughout the composed preflight/decode; failure
   preserves info. */
MarksAssetStatus marks_asset_preflight(const void *data,size_t bytes,uint64_t object_high_water,DefinitionAssetInfo *info);
/* Two-pass framing/owned preflight before copying any name. out is zero or owns
   an independent valid asset; failure preserves it, success replaces atomically. */
MarksAssetStatus marks_asset_decode(const void *data,size_t bytes,uint64_t object_high_water,size_t reserved,MarksAsset *out);
void marks_asset_free(MarksAsset *asset);
#endif
