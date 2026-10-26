#ifndef DX_BROWSE_PREVIEW_SET_CODEC_H
#define DX_BROWSE_PREVIEW_SET_CODEC_H
#include "browse_preview_set.h"
#include "browse_preview_codec.h"
#define DX_BROWSE_PREVIEW_SET_ENCODING 1u
#define DX_BROWSE_PREVIEW_SET_HEADER_BYTES 64u
#define DX_BROWSE_PREVIEW_SET_ENTRY_BYTES 32u
typedef struct {
    BrowsePreviewSetInfo set;
    size_t bytes,parser_owned,validation_scratch,coexistence;
} BrowsePreviewSetWireInfo;
/* Current DXPS: fixed64-byte LE header, count fixed32-byte directory entries,
   then contiguous current DXBP children. Scope is explicitly auxiliary current
   artifact only. Canonical unique (sheet,owner) keys and exact child lengths;
   reserved fields are zero. No unknown children, defaults or old encodings.
   Immutable inputs remain pinned through calls, including callbacks. */
BrowsePreviewSetStatus browse_preview_set_encode_measure(const BrowsePreviewSet *set,const BrowsePreviewPacket *prior,BrowsePreviewSetWireInfo *out);
BrowsePreviewSetStatus browse_preview_set_encode(const BrowsePreviewSet *set,size_t reserved,BrowsePreviewPacket *out);
/* Allocation-free framing/domain/budget inspection against an already admitted
   immutable actual snapshot. Exact document and persisted command revision;
   runtime epochs inside children remain observations. Full metadata admission
   happens once per child during atomic Set take. Caller charges the actual
   model in other_owned. Retained quota bounds the complete new Set; reserved
   additionally covers input bytes, prior Set, parser storage and max sequential
   validation scratch. Failure/cancellation preserves out and all borrowed data.
   This component has no native target/body/hash/receipt authority. */
BrowsePreviewSetStatus browse_preview_set_read_measure(const void *data,size_t bytes,const Workbook *actual,
    const BrowsePreviewSet *prior,size_t retained_limit,size_t other_owned,BrowsePreviewSetWireInfo *out);
BrowsePreviewSetStatus browse_preview_set_decode(const void *data,size_t bytes,const Workbook *actual,
    size_t retained_limit,size_t other_owned,size_t reserved,BrowsePreviewSetControl control,BrowsePreviewSet **out);
#endif
