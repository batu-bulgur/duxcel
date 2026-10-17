#ifndef DUXCEL_PAGE_WIRE_H
#define DUXCEL_PAGE_WIRE_H
#include "page.h"

#define DX_PAGE_WIRE_MAGIC UINT32_C(0x31505844)
#define DX_PAGE_WIRE_VERSION 1
#define DX_PAGE_WIRE_COLUMNS 256
#define DX_PAGE_WIRE_TEXT_BYTES (4u*1024u*1024u)
#define DX_PAGE_WIRE_BYTES (6u*1024u*1024u)

/* DXP1 little-endian u32 header: magic, version, columns, rows, text_bytes.
   Each column has name/type/kind byte lengths, precision, scale, followed by
   those three UTF-8 strings. Then come row-major u32 offsets[cells+1], one
   null byte per cell (1 = SQL NULL), and the UTF-8 text blob. NULL consumes no
   text; non-NULL zero-length text owns an empty C string. No embedded NUL is
   representable. Exact text is authoritative; Cell.number is display-only.

   out must be zero-initialized or an owned Page. Decode copies all data using
   the C allocator, replaces/frees out only on success, and retains no wire
   memory. Failure preserves out; optional error is an owned free()-able string
   replaced on failure. Empty pages may lie beyond total; nonempty pages must
   fit within its exact count or observed lower bound. */
bool page_from_wire(Page *out,const void *bytes,size_t size,uint64_t offset,
                    uint64_t total,bool count_pending,char **error);
#endif
