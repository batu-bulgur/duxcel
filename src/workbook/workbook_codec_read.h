#ifndef DUXCEL_WORKBOOK_CODEC_READ_H
#define DUXCEL_WORKBOOK_CODEC_READ_H
#include "workbook_codec.h"

#define DX_WORKBOOK_CODEC_READ_ROWS 256u
#define DX_WORKBOOK_CODEC_READ_TEXT 65536u
/* Component only: this header/cursor cannot open, save or publish a Workbook.
   It accepts well-formed writer12/13/14 metadata, not all historical native
   coercions. Native production loading and later family defaults are unchanged. */
typedef struct WorkbookCodecRead WorkbookCodecRead;
typedef struct WorkbookCodecHeader WorkbookCodecHeader;
typedef struct { uint64_t document_epoch,operation_ticket,snapshot_generation; } WorkbookCodecKey;
typedef enum { WORKBOOK_CODEC_META,WORKBOOK_CODEC_SHEETS,WORKBOOK_CODEC_HEADER_READY,WORKBOOK_CODEC_HEADER_CLAIMED,
    WORKBOOK_CODEC_SORTS } WorkbookCodecFamily;
typedef enum { WORKBOOK_CODEC_SHEETS_24=24,WORKBOOK_CODEC_SHEETS_25=25 } WorkbookCodecSheetsLayout;
typedef enum { WORKBOOK_CODEC_NULL,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_DOUBLE,WORKBOOK_CODEC_BOOLEAN,WORKBOOK_CODEC_TEXT } WorkbookCodecAtomKind;
typedef struct { const char *data; size_t bytes; } WorkbookCodecText;
typedef struct {
    WorkbookCodecAtomKind kind;
    union { int32_t i32; uint64_t u64; double real; bool boolean; WorkbookCodecText text; } value;
} WorkbookCodecAtom;
typedef struct { const char *name; WorkbookCodecAtomKind kind; } WorkbookCodecColumn;
typedef struct {
    const WorkbookCodecColumn *columns; size_t column_count;
    const WorkbookCodecAtom *cells; size_t row_count,first_row;
    bool end;
} WorkbookCodecBatch;
typedef struct {
    WorkbookCodecFamily family;
    const char *sql; /* Borrowed until next feed/free; host must copy before await. */
    /* One SQL execution per family. first_row is the next accepted position in
       that single owned result, not permission to rerun SQL after a partial feed. */
    size_t first_row,column_count,source_column_count,row_limit;
} WorkbookCodecRequest;
typedef struct { char *data; size_t bytes; } WorkbookCodecOwnedText;
/* Trusted platform callback: no source SQL/I/O is needed by this component.
   Return an independent malloc allocation of exactly bytes+1, including its NUL
   terminator, with bytes excluding terminator and bytes+1<=max_bytes. Empty is allowed as an unavailable
   locator. Do not return a borrowed/interior pointer or mutate the reference.
   Any returned allocation, also on false, is retired by the reader. Copy an
   overallocated platform buffer first; no malloc-size inspection is performed.
   This is a logical ownership contract, not an allocator/RSS bound. */
typedef bool (*WorkbookCodecResolve)(void *context,SourceKind kind,const char *reference,size_t max_bytes,WorkbookCodecOwnedText *out);
typedef enum { WORKBOOK_CODEC_READ_ERROR=-1,WORKBOOK_CODEC_READ_STALE=0,WORKBOOK_CODEC_READ_ACCEPTED=1 } WorkbookCodecReadStatus;
/* All three key scalars must be nonzero; only equality is guaranteed, not key
   issuance or wrap prevention. version is prevalidated12/13/14. The declared
   layout is the adapter's checked source schema, while all requests project25
   sheets columns in named order. Begin replaces an independent owned reader
   only on success; failures preserve it. Context stays borrowed until free. */
bool workbook_codec_read_begin(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,size_t owned_limit,WorkbookCodecResolve resolve,void *context,char **error);
const WorkbookCodecRequest *workbook_codec_read_request(const WorkbookCodecRead *read);
/* Explicit continuation after complete sheets, before finish; the default
   begin/feed/finish path still reads only meta/sheets. Validate primary sort
   definitions only when this continuation is armed. table_present is the
   adapter's checked inventory: absent12/13 retains primary-only definitions;
   v14 requires the table. May arm once; failure preserves the prior cursor.
   Present data uses one ordered result of at most448 rows plus a449th sentinel,
   fed in batches<=256. This is a component contract, not Worker transport or
   browser Open support. No SQL or source schema is executed/resolved here. */
bool workbook_codec_read_add_sorts(WorkbookCodecRead *read,bool table_present,char **error);
/* Cells/column descriptors are borrowed for this call, not a DXP1 buffer. The
   existing wire decoder remains the sole wire parser; an adapter supplies exact
   typed atoms. Include all concrete caller-owned input/adapter bytes that
   coexist in retained_input. This is a logical owned-buffer admission bound,
   not allocator/parser/engine/RSS peak. Diagnostic allocation is separate.
   Success adopts a whole batch; malformed/budget/resolver failures leave prior
   reader state unchanged. A stale key is ignored before reading batch/state or
   changing error. At most64 sheets; a65th sentinel is an explicit error. */
WorkbookCodecReadStatus workbook_codec_read_feed(WorkbookCodecRead *read,WorkbookCodecKey key,
    WorkbookCodecFamily family,const WorkbookCodecBatch *batch,size_t retained_input,char **error);
/* Moves only a complete opaque header once (meta/sheets plus explicitly armed
   sorts). It is not a complete workbook: cells, objects, formats, ledger,
   queries and preferences are absent.
   Failure preserves read and independent owned out. */
bool workbook_codec_read_finish(WorkbookCodecRead *read,WorkbookCodecHeader **out,char **error);
void workbook_codec_read_free(WorkbookCodecRead *read);
void workbook_codec_header_free(WorkbookCodecHeader *header);
size_t workbook_codec_read_owned_bytes(const WorkbookCodecRead *read);
size_t workbook_codec_header_owned_bytes(const WorkbookCodecHeader *header);
/* Getter pointers borrow read-only storage from this Header until it is freed.
   They never transfer an Analysis, SheetView, string or key to the caller.
   A completed optional family still does not make this a complete Workbook. */
const WorkbookCodecKey *workbook_codec_header_key(const WorkbookCodecHeader *header);
unsigned workbook_codec_header_version(const WorkbookCodecHeader *header);
size_t workbook_codec_header_sheet_count(const WorkbookCodecHeader *header);
size_t workbook_codec_header_active_sheet(const WorkbookCodecHeader *header);
float workbook_codec_header_user_scale(const WorkbookCodecHeader *header);
/* True only after an explicitly armed sorts family completed/defaulted; false
   for the original meta/sheets-only Header, including an empty sort_count. */
bool workbook_codec_header_sorts_ready(const WorkbookCodecHeader *header);
const char *workbook_codec_header_name(const WorkbookCodecHeader *header,size_t sheet);
const char *workbook_codec_header_reference(const WorkbookCodecHeader *header,size_t sheet);
const Analysis *workbook_codec_header_analysis(const WorkbookCodecHeader *header,size_t sheet);
const SheetView *workbook_codec_header_view(const WorkbookCodecHeader *header,size_t sheet);
#endif
