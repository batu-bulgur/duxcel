#ifndef DUXCEL_OBJECTS_ASSET_H
#define DUXCEL_OBJECTS_ASSET_H
#include "objects.h"
#include "analysis_asset.h"
#include "pivot_asset.h"
#include "marks_asset.h"
#define DX_OBJECTS_ASSET_ENCODING 1u
#define DX_OBJECTS_ASSET_HEADER_BYTES 32u
#define DX_OBJECT_ASSET_HEADER_BYTES 56u
#define DX_OBJECTS_ASSET_MAX_BYTES (DX_OBJECT_CURRENT_BYTES+8192u)
typedef struct { uint64_t sheet,high_water; ObjectSnapshot objects; } ObjectsAsset;
typedef struct { void *data; size_t bytes; } ObjectsAssetPacket;
typedef struct { uint64_t sheet,high_water; size_t count; DefinitionWireMeasure measure; } ObjectsAssetInfo;
typedef enum { OBJECTS_ASSET_OK,OBJECTS_ASSET_INVALID,OBJECTS_ASSET_LIMIT,OBJECTS_ASSET_UNSUPPORTED } ObjectsAssetStatus;
/* Closed ordered object snapshot, not cached results or an installed file profile.
   DXOB/u16 encoding/u16 zero/u64 sheet/u64 captured object high-water/u8 count/
   u8 zero/u16 zero/u32 body length. Each object: u64 id/row/height/source-id,
   u16 column/width,u8 kind/mark/chart-measure/zero, three u32 nested asset lengths,
   then u32 UTF-8 name length/name and canonical Analysis/Pivot/Marks assets.
   Nested headers must bind the same sheet/exact object ID. Stable source links,
   geometry, names, representation and array order survive; native layouts, Pages,
   runtime query/title/Marks revisions and derived schema caches are absent.
   Captured high-water is an allocation obligation, NEVER an Undo assignment.
   The adopter preserves live maxima and installs fresh query/cache epochs plus
   verified source/schema bindings before execution. No source IO or activation. */
/* Allocation-free bounded MODEL measure. Full nested grammar/source/duplicate
   semantics follow scratch admission in capture. owned is the exact count-sized
   object array plus all copied texts, excluding the separately owned input/old
   output. App conservative array-slack/job/global budgets remain outer policy. */
ObjectsAssetStatus objects_asset_measure(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high_water,ObjectsAssetInfo *out);
/* Own canonical bytes before model mutation. reserved covers NEW packet bytes
   plus peak grammar scratch, not the borrowed model or existing out packet.
   Failure preserves out; success replaces it only after every constituent is
   encoded. One bounded staging allocation avoids partial caller-arena writes. */
ObjectsAssetStatus objects_asset_capture(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high_water,size_t reserved,ObjectsAssetPacket *out);
void objects_asset_packet_free(ObjectsAssetPacket *packet);
/* Allocation-free wire inspection admits the COMPLETE graph before any copied
   string/array. This is framing/association/geometry/budget inspection; full
   nested and object semantics still require successful decode. */
ObjectsAssetStatus objects_asset_preflight(const void *data,size_t bytes,ObjectsAssetInfo *out);
/* reserved covers complete new array/text ownership plus peak grammar scratch.
   out is zero or owns an independent valid asset. Every failure preserves it;
   success replaces atomically. No live model/adoption/allocator mutation. */
ObjectsAssetStatus objects_asset_decode(const void *data,size_t bytes,size_t reserved,ObjectsAsset *out);
void objects_asset_free(ObjectsAsset *asset);
#endif
