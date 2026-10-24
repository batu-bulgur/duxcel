#include "version_chunks.h"
static uint64_t chunk_count(uint64_t bytes) { return bytes/DX_VERSION_CHUNK_BYTES+(bytes%DX_VERSION_CHUNK_BYTES!=0); }
WorkbookChunksStatus workbook_chunks_begin(uint64_t bytes,uint64_t reserved,WorkbookChunks *out) {
    if (!bytes || bytes>reserved || !out) return VERSION_CHUNKS_BOUNDS;
    *out=(WorkbookChunks){.bytes=bytes,.count=chunk_count(bytes)}; return VERSION_CHUNKS_OK;
}
bool workbook_chunk_span(uint64_t bytes,uint64_t position,uint64_t *offset,size_t *length) {
    if (!bytes || position>=chunk_count(bytes) || !offset || !length) return false;
    uint64_t start=position*DX_VERSION_CHUNK_BYTES,remaining=bytes-start;
    *offset=start; *length=remaining>DX_VERSION_CHUNK_BYTES ? DX_VERSION_CHUNK_BYTES : (size_t)remaining; return true;
}
static bool valid(const WorkbookChunks *c) {
    return c && c->bytes && c->count==chunk_count(c->bytes) && c->next<=c->count && c->received<=c->bytes &&
        c->received==(c->next==c->count ? c->bytes : c->next*DX_VERSION_CHUNK_BYTES);
}
WorkbookChunksStatus workbook_chunks_offer(WorkbookChunks *c,uint64_t position,const void *data,size_t bytes) {
    if (!c) return VERSION_CHUNKS_BOUNDS;
    if (c->error) return c->error;
    if (!valid(c)) return c->error=VERSION_CHUNKS_BOUNDS;
    if (position!=c->next || c->next==c->count) return c->error=VERSION_CHUNKS_SEQUENCE;
    uint64_t offset=0; size_t expected=0;
    if (!workbook_chunk_span(c->bytes,position,&offset,&expected) || !data || bytes!=expected) return c->error=VERSION_CHUNKS_LENGTH;
    c->next++; c->received+=bytes; return VERSION_CHUNKS_OK;
}
WorkbookChunksStatus workbook_chunks_finish(const WorkbookChunks *c) {
    if (!c) return VERSION_CHUNKS_BOUNDS;
    if (c->error) return c->error;
    if (!valid(c)) return VERSION_CHUNKS_BOUNDS;
    return c->next==c->count && c->received==c->bytes ? VERSION_CHUNKS_OK : VERSION_CHUNKS_INCOMPLETE;
}
