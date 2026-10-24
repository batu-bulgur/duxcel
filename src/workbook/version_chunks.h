#ifndef DUXCEL_VERSION_CHUNKS_H
#define DUXCEL_VERSION_CHUNKS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DX_VERSION_CHUNK_BYTES 65536u
typedef enum { VERSION_CHUNKS_OK,VERSION_CHUNKS_BOUNDS,VERSION_CHUNKS_SEQUENCE,VERSION_CHUNKS_LENGTH,VERSION_CHUNKS_INCOMPLETE } WorkbookChunksStatus;
typedef struct {
    uint64_t bytes,count,next,received;
    WorkbookChunksStatus error;
} WorkbookChunks;
/* Pure framing controller. A caller has already reserved a positive byte
   budget; no allocation, IO, hashing or document/format/asset verification.
   Every chunk is exactly64 KiB except a positive final remainder. Positions
   are zero-based uint64 ordinals, not byte offsets. Empty leaves are invalid.
   begin/span preserve out on rejection; callers own/read all supplied storage. */
WorkbookChunksStatus workbook_chunks_begin(uint64_t bytes,uint64_t reserved,WorkbookChunks *out);
bool workbook_chunk_span(uint64_t bytes,uint64_t position,uint64_t *offset,size_t *length);
/* Consume one ordered non-NULL BLOB observation. Bytes are borrowed for the
   call, never dereferenced/stored; the adapter separately copies/hashes them.
   Wrong/duplicate/gap/trailing positions or wrong lengths poison this sequence.
   Successful finish is repeatable. Extra offers after finish poison it too.
   The public state must remain unmodified between begin/offer/finish calls. */
WorkbookChunksStatus workbook_chunks_offer(WorkbookChunks *chunks,uint64_t position,const void *data,size_t bytes);
WorkbookChunksStatus workbook_chunks_finish(const WorkbookChunks *chunks);
#endif
