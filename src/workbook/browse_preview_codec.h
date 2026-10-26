#ifndef DX_BROWSE_PREVIEW_CODEC_H
#define DX_BROWSE_PREVIEW_CODEC_H
#include "browse_preview.h"
#define DX_BROWSE_PREVIEW_ENCODING 1u
#define DX_BROWSE_PREVIEW_HEADER_BYTES 256u
#define DX_BROWSE_PREVIEW_WIRE_BYTES (DX_BROWSE_PREVIEW_OWNED_BYTES+DX_BROWSE_PREVIEW_HEADER_BYTES)
typedef struct { void *data; size_t bytes; } BrowsePreviewPacket;
typedef struct {
    BrowsePreviewInfo preview;
    size_t bytes,owned,validation_scratch,coexistence;
} BrowsePreviewWireInfo;
/* One current optional browse component, not a .dxls profile. DXBP encoding1:
   fixed256-byte LE header, existing DXAN and selected DXSV, typed column/cell
   sections, actual record-prefix IDs and nullable reader key. No pointers,
   native structs, padding, decoder defaults or previous encodings. Cell.number
   is exact IEEE754 binary64 display-only bits, including NaN/Inf/signed zero;
   exact nullable UTF-8 text remains authoritative. Observed runtime/file fields
   never certify current data, source execution, record formats or an outer root.
   Model input must be an immutable owned preview. Encode coexistence includes
   input preview, old packet and new packet. Failure preserves output. */
BrowsePreviewStatus browse_preview_encode_measure(const BrowsePreview *preview,const BrowsePreviewPacket *prior,BrowsePreviewWireInfo *out);
BrowsePreviewStatus browse_preview_encode(const BrowsePreview *preview,size_t reserved,BrowsePreviewPacket *out);
void browse_preview_packet_free(BrowsePreviewPacket *packet);
/* Allocation-free complete framing/text/domain/size inspection, not full DXAN/
   DXSV grammar/closure. Full decode delegates that shared child admission once.
   Decode coexistence includes input bytes,new owner,temporary child graphs and
   grammar/RAW stack; callers additionally account other live inputs. Existing
   output ownership is added by decode before any allocation. Failure preserves
   out; success replaces atomically. Input bytes stay immutable through the
   entire call. No model/adoption/allocator/IO activation. */
BrowsePreviewStatus browse_preview_preflight(const void *data,size_t bytes,BrowsePreviewWireInfo *out);
BrowsePreviewStatus browse_preview_decode(const void *data,size_t bytes,size_t reserved,BrowsePreview **out);
#endif
