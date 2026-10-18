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
    WORKBOOK_CODEC_SORTS,WORKBOOK_CODEC_PREFERENCES,WORKBOOK_CODEC_OBJECTS,WORKBOOK_CODEC_QUERIES } WorkbookCodecFamily;
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
/* Independent optional continuation at HEADER_READY before claim, once. The
   adapter checks table presence/schema: absent12/13 defaults motion=false and
   all sheets visible; v14 requires preferences. One C-built named projection
   ignores unrelated valid keys and admits invalid unrelated v14 NULL/NUL rows
   for rejection. Known visibility keys use canonical decimal0..63 only, not
   historical sscanf coercions. At most65 rows plus a66th overflow sentinel.
   Whole-batch POD publication owns no input strings; failure preserves state.
   Completion of this family still does not validate/publish a Workbook. */
bool workbook_codec_read_add_preferences(WorkbookCodecRead *read,bool table_present,char **error);
/* Optional writer12/13/14 descriptor registry, not complete SheetObjects.
   Absent12 defaults empty; v13/v14 require this table. One named ordered result
   admits2048 descriptors plus a2049th sentinel, in atomic batches<=256, with at
   most32 descriptors per sheet. Source references resolve to same-sheet Table
   descriptors only when the family completes. Tables' source definitions,
   Pivot fields/measure arity, Marks, cells and allocation counters remain
   unvalidated; no source query or runtime revision is constructed. */
bool workbook_codec_read_add_objects(WorkbookCodecRead *read,bool table_present,char **error);
/* Explicit once-only continuation after the descriptor registry completes.
   All supported versions require queries. The trusted C projection preserves
   bytes while mapping SQL NULL, >64KiB, trim-empty and NUL text to required
   NULL for rejection; original source types must be checked by its adapter.
   The pure feed additionally rejects empty/oversized/NUL/invalid UTF-8 text,
   not SQL whitespace using a different generic isspace interpretation.
   One ordered result admits2112 rows plus a2113th sentinel. Primary owner0
   requires a non-NONE source; nonzero owners must be exact same-sheet Table
   descriptors. This stores definitions only, never parses or executes SQL. */
bool workbook_codec_read_add_queries(WorkbookCodecRead *read,bool table_present,char **error);
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
   sorts/preferences/object descriptors/queries). It is not a complete workbook:
   cells, full object sources/fields, formats and ledger remain absent.
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
bool workbook_codec_header_preferences_ready(const WorkbookCodecHeader *header);
/* Readiness applies only to the complete descriptor registry. Names borrow
   immutable Header storage; never free them or retain them beyond Header free.
   Invalid/not-ready getters return0/false; object_at preserves out on false.
   IDs are worksheet-local full uint64 values, not array positions. */
typedef struct {
    uint64_t id,row,height,source_id;
    size_t column,width;
    ObjectKind kind;
    ChartMark mark;
    unsigned chart_measure;
    const char *name;
} WorkbookCodecObjectView;
bool workbook_codec_header_objects_ready(const WorkbookCodecHeader *header);
size_t workbook_codec_header_object_count(const WorkbookCodecHeader *header,size_t sheet);
bool workbook_codec_header_object_at(const WorkbookCodecHeader *header,size_t sheet,size_t index,WorkbookCodecObjectView *out);
/* Definitions borrow Header-owned bytes. False preserves out for an invalid
   owner or incomplete family; true+NULL means that existing owner has no stored
   query, including the primary slot of a SOURCE_NONE sheet. Table definitions
   are private strings, not fake Analyses. */
bool workbook_codec_header_queries_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_query(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const char **out);
/* POD defaults are false until preferences is explicitly complete. Invalid
   Header/sheet arguments return false; check sheet_count to validate identity. */
bool workbook_codec_header_reduced_motion(const WorkbookCodecHeader *header);
bool workbook_codec_header_sheet_hidden(const WorkbookCodecHeader *header,size_t sheet);
const char *workbook_codec_header_name(const WorkbookCodecHeader *header,size_t sheet);
const char *workbook_codec_header_reference(const WorkbookCodecHeader *header,size_t sheet);
const Analysis *workbook_codec_header_analysis(const WorkbookCodecHeader *header,size_t sheet);
const SheetView *workbook_codec_header_view(const WorkbookCodecHeader *header,size_t sheet);
#endif
