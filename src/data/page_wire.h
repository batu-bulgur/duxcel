#ifndef DUXCEL_PAGE_WIRE_H
#define DUXCEL_PAGE_WIRE_H
#include "page.h"

#define DX_PAGE_WIRE_MAGIC UINT32_C(0x31505844)
#define DX_PAGE_WIRE_VERSION 1
#define DX_PAGE_WIRE_COLUMNS 256
#define DX_PAGE_WIRE_TEXT_BYTES (4u*1024u*1024u)
#define DX_PAGE_WIRE_BYTES (6u*1024u*1024u)
#define DX_PAGE_WIRE_COPY_CELLS 20000u

/* Parse a bounded SQL type from DESCRIBE, retaining no text. Unknown types are
   VARCHAR/NUM_NONE; canonical DECIMAL(p,s) requires 1<=p<=38 and 0<=s<=p.
   False preserves both outputs. This parses schema metadata, not cell values. */
bool page_type_from_sql(const char *name,duckdb_type *type,NumberType *number);

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
/* Only an active copy operation may select this decoder. The wire cannot opt
   into its larger row bound: <=20,000 rows AND cells, same text/wire limits. */
bool page_from_wire_copy(Page *out,const void *bytes,size_t size,uint64_t offset,
                         uint64_t total,bool count_pending,char **error);
/* Same fixed row/cell policies. A trusted caller reserves the output's exact
   C-owned bytes against its other retained data. All wire validation and size
   accounting finish before any output allocation; failure preserves out. */
bool page_from_wire_owned(Page *out,const void *bytes,size_t size,uint64_t offset,
                          uint64_t total,bool count_pending,size_t owned_budget,char **error);
bool page_from_wire_copy_owned(Page *out,const void *bytes,size_t size,uint64_t offset,
                               uint64_t total,bool count_pending,size_t owned_budget,char **error);
/* Only the trusted active secondary-sort metadata cursor selects this policy:
   exactly four columns, at most449 rows (448 legal rows + one sentinel), offset
   zero, same text/wire/owned bounds. Ordinary pages retain their256-row bound.
   Decode once, then feed the shared row cursor in batches of at most256. */
bool page_from_wire_workbook_sorts_owned(Page *out,const void *bytes,size_t size,uint64_t offset,
                                        uint64_t total,bool count_pending,size_t owned_budget,char **error);
/* Only the trusted active object-registry metadata cursor selects this policy:
   exactly eleven columns, at most2049 rows (2048 legal rows + one sentinel),
   offset zero, unchanged text/wire/owned bounds. Decode one result, then feed
   the shared cursor in <=256-row batches. The wire cannot select this policy;
   object/schema/reference validity is checked by that cursor, not this parser. */
bool page_from_wire_workbook_objects_owned(Page *out,const void *bytes,size_t size,uint64_t offset,
                                          uint64_t total,bool count_pending,size_t owned_budget,char **error);
/* Trusted active owner-metadata cursors (queries/source options) only: exactly
   three columns, <=2113 rows (64*(32+1) plus one sentinel), offset zero, same
   text/wire/owned admission. C separately validates the original family schema,
   owner references and values. Decode one result and feed in <=256-row batches;
   neither the wire nor ordinary source-page requests may select this policy. */
bool page_from_wire_workbook_owners_owned(Page *out,const void *bytes,size_t size,uint64_t offset,
                                         uint64_t total,bool count_pending,size_t owned_budget,char **error);
#endif
