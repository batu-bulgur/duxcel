#include "cell_delta_codec.h"
#include <string.h>
static void put(unsigned char *to,uint64_t value,unsigned bytes) {
    for (unsigned i=0;i<bytes;i++) { to[i]=(unsigned char)(value&255u); value>>=8; }
}
static uint64_t get(const unsigned char *from,unsigned bytes) {
    uint64_t value=0; for (unsigned i=0;i<bytes;i++) value|=(uint64_t)from[i]<<(8*i); return value;
}
CellDeltaCodecStatus cell_delta_encoded_size(CellDelta in,size_t *bytes) {
    size_t values=0;
    switch (cell_delta_check(in,&values)) {
    case CELL_DELTA_NOOP: return CELL_DELTA_CODEC_NOOP;
    case CELL_DELTA_READY: if (bytes) *bytes=DX_CELL_DELTA_HEADER_BYTES+values; return CELL_DELTA_CODEC_OK;
    default: return CELL_DELTA_CODEC_INVALID;
    }
}
CellDeltaCodecStatus cell_delta_encode(CellDelta in,void *arena,size_t capacity,size_t *written) {
    if (!written) return CELL_DELTA_CODEC_INVALID;
    size_t bytes=0; CellDeltaCodecStatus status=cell_delta_encoded_size(in,&bytes);
    if (status!=CELL_DELTA_CODEC_OK) return status;
    if (!arena) return CELL_DELTA_CODEC_INVALID;
    if (bytes>capacity) return CELL_DELTA_CODEC_LIMIT;
    unsigned char *data=arena; memcpy(data,"DXCD",4);
    put(data+4,DX_CELL_DELTA_ENCODING,2); put(data+6,0,2);
    put(data+8,in.sheet,8); put(data+16,in.row,8); put(data+24,in.column,2);
    put(data+26,in.before_bytes,4); put(data+30,in.after_bytes,4);
    if (in.before_bytes) memcpy(data+DX_CELL_DELTA_HEADER_BYTES,in.before,in.before_bytes);
    if (in.after_bytes) memcpy(data+DX_CELL_DELTA_HEADER_BYTES+in.before_bytes,in.after,in.after_bytes);
    *written=bytes; return CELL_DELTA_CODEC_OK;
}
CellDeltaCodecStatus cell_delta_decode(const void *bytes,size_t length,CellDelta *out) {
    if (!bytes || !out || length<6) return CELL_DELTA_CODEC_INVALID;
    const unsigned char *data=bytes;
    if (memcmp(data,"DXCD",4)) return CELL_DELTA_CODEC_INVALID;
    if (get(data+4,2)!=DX_CELL_DELTA_ENCODING) return CELL_DELTA_CODEC_UNSUPPORTED;
    if (length<DX_CELL_DELTA_HEADER_BYTES || get(data+6,2)) return CELL_DELTA_CODEC_INVALID;
    uint64_t before=get(data+26,4),after=get(data+30,4);
    if (before>DX_CELL_DELTA_TEXT_BYTES || after>DX_CELL_DELTA_TEXT_BYTES || length!=DX_CELL_DELTA_HEADER_BYTES+before+after) return CELL_DELTA_CODEC_INVALID;
    CellDelta in={.sheet=get(data+8,8),.row=get(data+16,8),.column=(size_t)get(data+24,2),
        .before=(const char *)data+DX_CELL_DELTA_HEADER_BYTES,.after=(const char *)data+DX_CELL_DELTA_HEADER_BYTES+(size_t)before,
        .before_bytes=(size_t)before,.after_bytes=(size_t)after};
    if (cell_delta_check(in,NULL)!=CELL_DELTA_READY) return CELL_DELTA_CODEC_INVALID;
    *out=in; return CELL_DELTA_CODEC_OK;
}
