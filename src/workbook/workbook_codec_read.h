#ifndef DUXCEL_WORKBOOK_CODEC_READ_H
#define DUXCEL_WORKBOOK_CODEC_READ_H
#include "workbook_codec.h"

#define DX_WORKBOOK_CODEC_READ_ROWS 256u
#define DX_WORKBOOK_CODEC_READ_TEXT 65536u
/* Component only: this header/cursor cannot open, save or publish a Workbook.
   It accepts well-formed writer12/13/14 metadata, not all historical native
   coercions. Native production loading and later family defaults are unchanged.
   Source ledgers/bindings, formatting and full historical migration remain
   separate; captured counters alone cannot validate or activate a source. */
typedef struct WorkbookCodecRead WorkbookCodecRead;
typedef struct WorkbookCodecHeader WorkbookCodecHeader;
typedef struct { uint64_t document_epoch,operation_ticket,snapshot_generation; } WorkbookCodecKey;
typedef enum { WORKBOOK_CODEC_META,WORKBOOK_CODEC_SHEETS,WORKBOOK_CODEC_HEADER_READY,WORKBOOK_CODEC_HEADER_CLAIMED,
    WORKBOOK_CODEC_SORTS,WORKBOOK_CODEC_PREFERENCES,WORKBOOK_CODEC_OBJECTS,WORKBOOK_CODEC_QUERIES,WORKBOOK_CODEC_SOURCE_OPTIONS,
    WORKBOOK_CODEC_TABLE_SOURCES,WORKBOOK_CODEC_TABLE_FILTERS,WORKBOOK_CODEC_TABLE_SORTS,
    WORKBOOK_CODEC_PIVOT_FIELDS,WORKBOOK_CODEC_MARKS,WORKBOOK_CODEC_FILTERS,WORKBOOK_CODEC_COLUMNS,WORKBOOK_CODEC_CELLS,WORKBOOK_CODEC_OBJECT_COUNTERS,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS } WorkbookCodecFamily;
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
typedef struct { bool full_primary_filters; } WorkbookCodecReadOptions;
/* All three key scalars must be nonzero; only equality is guaranteed, not key
   issuance or wrap prevention. version is prevalidated12/13/14. The declared
   layout is the adapter's checked source schema, while all requests project25
   sheets columns in named order. Begin replaces an independent owned reader
   only on success; failures preserve it. Context stays borrowed until free. */
bool workbook_codec_read_begin(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,size_t owned_limit,WorkbookCodecResolve resolve,void *context,char **error);
/* Explicit before-Sheets capture opt-in: primary filter name4096/value4MiB.
   All other fields retain default limits. The original constructor supplies
   zero options and preserves its64KiB primary capture behavior. This does not
   validate primary filters until their retained-input-aware continuation. */
bool workbook_codec_read_begin_with_options(WorkbookCodecRead **out,WorkbookCodecKey key,unsigned version,
    WorkbookCodecSheetsLayout layout,WorkbookCodecReadOptions options,size_t owned_limit,WorkbookCodecResolve resolve,void *context,char **error);
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
/* Once at complete unclaimed Header, independent of other continuations.
   Absent12/13 defaults ready only after primary validation; v14 requires this
   table. Caller supplies all retained input/old-output bytes because validating
   primary JSON can allocate. Admission reserves max native parser scratch
   before any filter validator call. One named5-column ordered result admits
   960 secondary rows plus a961st sentinel, in atomic<=256 batches; names4096,
   values4MiB, at most15 secondaries per sheet. Raw secondary text and combined
   primary/secondary native text (including terminators) have separate8MiB caps.
   No source schema/SQL or setter-based deduplication is involved. */
bool workbook_codec_read_add_filters(WorkbookCodecRead *read,bool table_present,size_t retained_input,char **error);
/* Independent once-only continuation at complete unclaimed Header. Every writer
   package12/13/14 requires columns; empty ordered results are valid. One named
   INTEGER/INTEGER/VARCHAR/DOUBLE result admits1048576 rows plus one sentinel,
   fed in atomic batches<=256 until explicit end. Each sheet has contiguous
   indices0..16383; names may be empty or duplicate and have no native length
   cap beyond checked arithmetic, UTF-8/NUL and the caller's owned budget.
   Widths must be finite DOUBLE52..1200 before conversion to float. Geometric
   arrays count their full capacity; batch admission includes old+new arrays,
   staged/copied names and all caller-retained input before any allocation.
   Names and view arrays borrow Header ownership; existing name identities
   survive successful array growth. This does not validate full model placement. */
bool workbook_codec_read_add_columns(WorkbookCodecRead *read,bool table_present,char **error);
/* Independent once-only complete Header continuation. Absent12/13 defaults
   ready-empty without a query; v14 requires cells. One named I/U/I/T result
   admits1280000 rows plus a sentinel, in atomic<=256-row feeds until explicit
   end. At most20000 records and4MiB text INCLUDING terminators per sheet;
   row<2^40, column<16384 and text<=65536bytes. Strict(sheet,row,column) ordering
   rejects duplicates for this known-writer decoder; permissive historical
   native CTAS duplicates are outside this contract. Actual empty TEXT records
   stay present, missing coordinates stay absent and no setter normalizes them.
   Preflight reserves old+new geometric arrays, copied strings/name staging and
   retained caller bytes BEFORE UTF-8 scans/allocation. No CellStore/history or
   object placement/full Workbook validation is constructed here. */
bool workbook_codec_read_add_cells(WorkbookCodecRead *read,bool table_present,char **error);
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
   Pivot fields/measure arity, Marks, cells and unarmed allocation counters remain
   unvalidated; no source query or runtime revision is constructed. */
bool workbook_codec_read_add_objects(WorkbookCodecRead *read,bool table_present,char **error);
/* Once after complete object descriptors, before claim, independent of other
   completed continuations. v12 forbids a present counter table; absent v12
   completes without SQL using each sheet's maximum live descriptor ID (or0).
   v13/v14 require one exact INTEGER/UBIGINT row per sheet, contiguous from0,
   with high_water>=every live descriptor ID. Zero is valid for an empty sheet;
   retired IDs and UINT64_MAX remain exact. One named ordered result admits64
   rows plus a65th sentinel in atomic batches<=256. Fixed Header-owned POD is
   charged by sizeof; even allocation-free feeds admit all retained caller input.
   This preserves allocation state only, not issuance, history or source epochs. */
bool workbook_codec_read_add_object_counters(WorkbookCodecRead *read,bool table_present,char **error);
/* Once after complete objects AND object counters, before claim. Versions12/13
   forbid this table and absent defaults to zero without SQL; v14 requires one
   exact INTEGER/UBIGINT row per worksheet, ordered contiguously from0. One named
   result admits64 rows plus a65th sentinel in atomic<=256-row batches, including
   explicit empty EOF after all data. Fixed Header-owned POD is fully charged;
   allocation-free feeds still admit all caller-retained input. Any uint64 value
   including0/MAX is valid independently of object IDs. Ledger bounds, issuance,
   bindings, formatting and source activation are not validated here. */
bool workbook_codec_read_add_source_version_counters(WorkbookCodecRead *read,bool table_present,char **error);
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
/* Independent once-only continuation after complete object descriptors, before
   claim; may precede or follow queries. All supported versions require this
   table. One named ordered INTEGER/UBIGINT/BOOLEAN result admits2112 rows plus
   a2113th sentinel, with whole-batch ownership/budget admission. Primary owner0
   is valid even on a blank sheet, but only false may be stored for SOURCE_NONE.
   Nonzero owners must be same-sheet Table descriptors; explicit false rows
   still count as seen. This does not construct full Table source Analyses. */
bool workbook_codec_read_add_source_options(WorkbookCodecRead *read,bool table_present,char **error);
typedef struct { bool sources,filters,sorts; } WorkbookCodecTableSourcePresence;
/* One chain after complete descriptors/queries/options:14-column sources,
   6-column secondary filters,5-column secondary sorts, each one ordered result
   in atomic<=256-row feeds. Limits2048/30720/14336 plus one sentinel each.
   v14 requires all three tables; absent sources in12/13 defaults empty only
   with no Table descriptors. Present sources always requires both children.
   Raw text is bounded8MiB/sheet, independently of owned-buffer admission.
   Completion also checks the known native object-current byte lower bound:
   object/name/Table/query data and actual available locator bytes. Unavailable
   native path lengths and future Pivot/Marks payloads still require completed
   candidate validation. Preserved definitions do not authorize source SQL or
   file execution or constitute complete native ObjectStore validation. */
bool workbook_codec_read_add_table_sources(WorkbookCodecRead *read,WorkbookCodecTableSourcePresence present,char **error);
typedef struct { bool fields,marks; } WorkbookCodecPivotMarksPresence;
/* Once after the complete descriptor/query/options/Table chain, before claim.
   With an objects table (even empty), writer12/13/14 require both families;
   absent v12 objects defaults empty and ignores these child tables, as native
   load_objects does. Named9/8-column results admit65536/98304 rows plus one
   sentinel, in atomic<=256 feeds ordered by sheet/owner/role/position. Actual
   Table owners cannot contain either family. This validates definitions and
   presentation arity, not source schema, runtime values or workbook placement. */
bool workbook_codec_read_add_pivot_marks(WorkbookCodecRead *read,WorkbookCodecPivotMarksPresence present,char **error);
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
   filters/sorts/preferences/object descriptors/object and source-version counters/
   queries/source options/Table sources/Pivot/Marks/columns/cells).
   It is not a complete workbook: formats, source ledger/bindings, historical
   migration and final model validation remain absent.
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
bool workbook_codec_header_filters_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_columns_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_cells_ready(const WorkbookCodecHeader *header);
typedef struct { const SheetCell *cells; size_t count,text_bytes; } WorkbookCodecCellsView;
/* Borrowed read-only Header ownership; no histories or fake NULL records. False
   preserves out for an invalid sheet, incomplete/unarmed family or NULL out.
   True with count0/cellsNULL means a complete empty sheet, including defaults. */
bool workbook_codec_header_cells(const WorkbookCodecHeader *header,size_t sheet,WorkbookCodecCellsView *out);
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
bool workbook_codec_header_object_counters_ready(const WorkbookCodecHeader *header);
/* False preserves out for an unarmed/incomplete family, invalid sheet or NULL
   argument. True copies one Header-owned scalar; no ObjectStore is exposed. */
bool workbook_codec_header_object_high_water(const WorkbookCodecHeader *header,size_t sheet,uint64_t *out);
bool workbook_codec_header_source_version_counters_ready(const WorkbookCodecHeader *header);
/* False preserves out for an unarmed/incomplete family, invalid sheet or NULL
   argument. True copies one Header-owned scalar, not a ledger or issuer. */
bool workbook_codec_header_source_version_high_water(const WorkbookCodecHeader *header,size_t sheet,uint64_t *out);
size_t workbook_codec_header_object_count(const WorkbookCodecHeader *header,size_t sheet);
bool workbook_codec_header_object_at(const WorkbookCodecHeader *header,size_t sheet,size_t index,WorkbookCodecObjectView *out);
/* Definitions borrow Header-owned bytes. False preserves out for an invalid
   owner or incomplete family; true+NULL means that existing owner has no stored
   query, including the primary slot of a SOURCE_NONE sheet. Table definitions
   are private strings, not fake Analyses. */
bool workbook_codec_header_queries_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_query(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const char **out);
/* False preserves out on incomplete/invalid owner. Valid owner slots default
   false, including a blank sheet's primary slot; no stored row is required. */
bool workbook_codec_header_source_options_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_unique_rows(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,bool *out);
typedef struct {
    const Analysis *analysis;
    const char *reference;
    bool locator_available;
} WorkbookCodecTableSourceView;
/* Complete persisted Table definitions only. All pointers borrow Header storage;
   raw reference and independently resolved locator remain separate. A browser
   resolver may leave the locator unavailable without discarding the definition.
   Invalid/incomplete/non-Table owners return false and preserve out. */
bool workbook_codec_header_table_sources_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_table_source(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,WorkbookCodecTableSourceView *out);
/* Both getters require the completed chain and a genuine non-Table owner.
   Empty definitions borrow Header-owned zero structures; invalid arguments
   preserve out. Every name/value and definition lifetime belongs to Header. */
bool workbook_codec_header_pivot_marks_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_pivot_definition(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const PivotDefinition **out);
bool workbook_codec_header_marks_definition(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,const MarkDefinition **out);
/* POD defaults are false until preferences is explicitly complete. Invalid
   Header/sheet arguments return false; check sheet_count to validate identity. */
bool workbook_codec_header_reduced_motion(const WorkbookCodecHeader *header);
bool workbook_codec_header_sheet_hidden(const WorkbookCodecHeader *header,size_t sheet);
const char *workbook_codec_header_name(const WorkbookCodecHeader *header,size_t sheet);
const char *workbook_codec_header_reference(const WorkbookCodecHeader *header,size_t sheet);
const Analysis *workbook_codec_header_analysis(const WorkbookCodecHeader *header,size_t sheet);
const SheetView *workbook_codec_header_view(const WorkbookCodecHeader *header,size_t sheet);
#endif
