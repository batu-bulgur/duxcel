#include "cell_delta.h"
#include <string.h>
static bool span(const char *text,size_t bytes) {
    return bytes<=DX_CELL_DELTA_TEXT_BYTES && (text || !bytes) && (!bytes || !memchr(text,0,bytes));
}
CellDeltaStatus cell_delta_check(CellDelta in,size_t *bytes) {
    if (!in.sheet || in.row>=DX_SHEET_ROWS || in.column>=16384 || !span(in.before,in.before_bytes) || !span(in.after,in.after_bytes)) return CELL_DELTA_INVALID;
    if (in.before_bytes==in.after_bytes && (!in.before_bytes || !memcmp(in.before,in.after,in.before_bytes))) return CELL_DELTA_NOOP;
    if (bytes) *bytes=in.before_bytes+in.after_bytes;
    return CELL_DELTA_READY;
}
CellDeltaStatus cell_delta_capture(CellDelta in,void *arena,size_t capacity,CellDelta *out) {
    if (!out) return CELL_DELTA_INVALID;
    size_t bytes=0; CellDeltaStatus status=cell_delta_check(in,&bytes);
    if (status!=CELL_DELTA_READY) return status;
    if (!arena) return CELL_DELTA_INVALID;
    if (bytes>capacity) return CELL_DELTA_LIMIT;
    char *data=arena;
    if (in.before_bytes) memcpy(data,in.before,in.before_bytes);
    if (in.after_bytes) memcpy(data+in.before_bytes,in.after,in.after_bytes);
    in.before=data; in.after=data+in.before_bytes; *out=in;
    return CELL_DELTA_READY;
}
CellDelta cell_delta_inverse(CellDelta in) {
    const char *text=in.before; in.before=in.after; in.after=text;
    size_t bytes=in.before_bytes; in.before_bytes=in.after_bytes; in.after_bytes=bytes; return in;
}
