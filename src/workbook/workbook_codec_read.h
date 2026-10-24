#ifndef DUXCEL_WORKBOOK_CODEC_READ_H
#define DUXCEL_WORKBOOK_CODEC_READ_H
#include "workbook_codec.h"

#define DX_WORKBOOK_CODEC_READ_ROWS 256u
#define DX_WORKBOOK_CODEC_READ_TEXT 65536u
/* Component only: this cursor cannot open, save or publish a Workbook. It
   accepts exact current core15 metadata, the leaf payload inside native16.
   Source activation, complete durable identity/model admission and browser Open
   remain separate owners; captured counters alone do not activate a source. */
typedef struct WorkbookCodecRead WorkbookCodecRead;
typedef struct WorkbookCodecHeader WorkbookCodecHeader;
typedef struct { uint64_t document_epoch,operation_ticket,snapshot_generation; } WorkbookCodecKey;
typedef enum { WORKBOOK_CODEC_META,WORKBOOK_CODEC_SHEETS,WORKBOOK_CODEC_HEADER_READY,WORKBOOK_CODEC_HEADER_CLAIMED,
    WORKBOOK_CODEC_SORTS,WORKBOOK_CODEC_PREFERENCES,WORKBOOK_CODEC_OBJECTS,WORKBOOK_CODEC_QUERIES,WORKBOOK_CODEC_SOURCE_OPTIONS,
    WORKBOOK_CODEC_TABLE_SOURCES,WORKBOOK_CODEC_TABLE_FILTERS,WORKBOOK_CODEC_TABLE_SORTS,
    WORKBOOK_CODEC_PIVOT_FIELDS,WORKBOOK_CODEC_MARKS,WORKBOOK_CODEC_FILTERS,WORKBOOK_CODEC_COLUMNS,WORKBOOK_CODEC_CELLS,WORKBOOK_CODEC_OBJECT_COUNTERS,WORKBOOK_CODEC_SOURCE_VERSION_COUNTERS,WORKBOOK_CODEC_SOURCE_VERSIONS,WORKBOOK_CODEC_SOURCE_BINDINGS,WORKBOOK_CODEC_FORMAT_ROWS,WORKBOOK_CODEC_FORMAT_STATE,WORKBOOK_CODEC_FORMAT_SELECTORS } WorkbookCodecFamily;
typedef enum { WORKBOOK_CODEC_SHEETS_25=25 } WorkbookCodecSheetsLayout;
typedef enum { WORKBOOK_CODEC_NULL,WORKBOOK_CODEC_I32,WORKBOOK_CODEC_U64,WORKBOOK_CODEC_DOUBLE,WORKBOOK_CODEC_BOOLEAN,WORKBOOK_CODEC_TEXT,WORKBOOK_CODEC_BLOB } WorkbookCodecAtomKind;
typedef struct { const char *data; size_t bytes; } WorkbookCodecText;
typedef struct { const unsigned char *data; size_t bytes; } WorkbookCodecBlob;
/* Conservative known64-bit native-model category, not an ABI guarantee. */
#define DX_WORKBOOK_CODEC_SOURCE_VERSION_BYTES (sizeof(SourceVersion)>48u ? sizeof(SourceVersion) : 48u)
/* Large RAW name-sorting stack category. Ordinary fixed C frames are separate. */
#define DX_WORKBOOK_CODEC_RAW_SCRATCH_BYTES (128u*1024u)
/* Logical native64-bit row category, separate from actual Header capacity. */
#define DX_WORKBOOK_CODEC_FORMAT_PATCH_BYTES (sizeof(FormatPatch)>96u ? sizeof(FormatPatch) : 96u)
typedef struct {
    WorkbookCodecAtomKind kind;
    union { int32_t i32; uint64_t u64; double real; bool boolean; WorkbookCodecText text; WorkbookCodecBlob blob; } value;
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
   issuance or wrap prevention. version must be exactly15. The declared
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
   adapter's checked inventory: the current core requires the table. May arm once; failure preserves the prior cursor.
   Present data uses one ordered result of at most448 rows plus a449th sentinel,
   fed in batches<=256. This is a component contract, not Worker transport or
   browser Open support. No SQL or source schema is executed/resolved here. */
bool workbook_codec_read_add_sorts(WorkbookCodecRead *read,bool table_present,char **error);
/* Once at complete unclaimed Header, independent of other continuations.
   The current core requires this table. Caller supplies all retained input/old-output bytes because validating
   primary JSON can allocate. Admission reserves max native parser scratch
   before any filter validator call. One named5-column ordered result admits
   960 secondary rows plus a961st sentinel, in atomic<=256 batches; names4096,
   values4MiB, at most15 secondaries per sheet. Raw secondary text and combined
   primary/secondary native text (including terminators) have separate8MiB caps.
   No source schema/SQL or setter-based deduplication is involved. */
bool workbook_codec_read_add_filters(WorkbookCodecRead *read,bool table_present,size_t retained_input,char **error);
/* Independent once-only continuation at complete unclaimed Header. Every writer
   package15 requires columns; empty ordered results are valid. One named
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
/* Independent once-only complete Header continuation. The current core requires cells. One named I/U/I/T result
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
   adapter checks table presence/schema: the current core requires preferences. One C-built named projection
   ignores unrelated valid keys and admits invalid unrelated current NULL/NUL rows
   for rejection. Known visibility keys use canonical decimal0..63 only, not
   historical sscanf coercions. At most65 rows plus a66th overflow sentinel.
   Whole-batch POD publication owns no input strings; failure preserves state.
   Completion of this family still does not validate/publish a Workbook. */
bool workbook_codec_read_add_preferences(WorkbookCodecRead *read,bool table_present,char **error);
/* Current core15 descriptor registry, not complete SheetObjects.
   The current core requires this table. One named ordered result
   admits2048 descriptors plus a2049th sentinel, in atomic batches<=256, with at
   most32 descriptors per sheet. Source references resolve to same-sheet Table
   descriptors only when the family completes. Tables' source definitions,
   Pivot fields/measure arity, Marks, cells and unarmed allocation counters remain
   unvalidated; no source query or runtime revision is constructed. */
bool workbook_codec_read_add_objects(WorkbookCodecRead *read,bool table_present,char **error);
/* Once after complete object descriptors, before claim, independent of other
   completed continuations. The current core requires one exact INTEGER/UBIGINT row per sheet, contiguous from0,
   with high_water>=every live descriptor ID. Zero is valid for an empty sheet;
   retired IDs and UINT64_MAX remain exact. One named ordered result admits64
   rows plus a65th sentinel in atomic batches<=256. Fixed Header-owned POD is
   charged by sizeof; even allocation-free feeds admit all retained caller input.
   This preserves allocation state only, not issuance, history or source epochs. */
bool workbook_codec_read_add_object_counters(WorkbookCodecRead *read,bool table_present,char **error);
/* Once after complete objects AND object counters, before claim. The current core requires one
   exact INTEGER/UBIGINT row per worksheet, ordered contiguously from0. One named
   result admits64 rows plus a65th sentinel in atomic<=256-row batches, including
   explicit empty EOF after all data. Fixed Header-owned POD is fully charged;
   allocation-free feeds still admit all caller-retained input. Any uint64 value
   including0/MAX is valid independently of object IDs. Ledger bounds, issuance,
   bindings, formatting and source activation are not validated here. */
bool workbook_codec_read_add_source_version_counters(WorkbookCodecRead *read,bool table_present,char **error);
/* Once after objects and both counter families, before claim. The current core requires the table. One ordered named7-column result admits65536 rows plus a sentinel,
   <=1024 per sheet, in atomic<=256 feeds. definition alone is nullable: pending
   is encoding0/SQL NULL, complete is encoding1/canonical RAW strictly<256KiB.
   BLOB atoms borrow exact binary bytes; no hex or second wire parser lives here.
   Raw references are nonempty<=64KiB UTF-8 archive bytes, including glob text.
   NULL resolver preserves an independent owned empty locator; supplied false is
   an error, true requires the existing exact owned bytes+1 allocation contract.
   Only nonempty resolved locators receive native UTF-8/64KiB/no-glob checks;
   a nonempty logical locator is not file/content/capability verification.
   Archive and known-restored ledger reservations count max(sizeof(SourceVersion),
   48) per entry and have1MiB/sheet,8MiB/workbook lower bounds; future formats and
   unavailable final native locators still need complete candidate admission.
   Actual Header arrays, BOTH strings and packets have separate caller-owned
   accounting including old/new/staged coexistence and retained input. Any
   complete batch reserves128KiB RAW scratch before packet validation. No model,
   ledger issuance, binding/path equality, format or source activation is made. */
bool workbook_codec_read_add_source_versions(WorkbookCodecRead *read,bool table_present,size_t retained_input,char **error);
/* Once after complete Table definitions (including queries/options) and source
   ledger/counters, before claim. The current core requires complete coverage.
   One named I/U/U result has at most2112 rows plus a sentinel, fed atomically
   <=256 at a time: each sheet primary0, then each live Table in owner order.
   Every owner needs an explicit row, including unbound version0. A nonzero
   binding must match its ledger entry's owner/kind; pending RAW is permitted.
   Both nonempty logical locators must compare equal. Either unavailable locator
   defers that comparison; empty==empty is never proof. No raw-reference spelling,
   content/schema/engine/file identity or Analysis transform equality is inferred.
   Fixed Header-owned PODs are separately exposed; Analysis.source_version stays0
   until a future complete-model adoption. Arming/feeds admit caller-retained
   bytes even though this continuation allocates no heap or parser scratch. */
bool workbook_codec_read_add_source_bindings(WorkbookCodecRead *read,bool table_present,size_t retained_input,char **error);
/* Base format properties only, once at complete unclaimed Header. Formats are
   mandatory for core15, together with complete ledger and
   binding views. One named13-column ordered result admits32768 rows plus one
   sentinel, <=512 per sheet, in atomic<=256 batches. Positions start0 and stay
   contiguous within each sheet; sheets without patches need no rows. Lossless
   scalar capture and shared format_valid validate range/mask/style properties.
   No selector, source-aware mode, scope, guards, membership, history/revision,
   migration, renderer or complete-format readiness is inferred/exposed.
   Actual Header geometric capacity and old/new array coexistence are metered
   with all retained caller input. Separately named logical format_native_bytes
   uses max(sizeof(FormatPatch),96known64-bit) per row. The current core adds that category to
   BOTH existing ledger archive/restored1MiB-sheet/8MiB-workbook reservations;
   These are logical categories,
   not universal ABI, allocator/RSS or complete-model validity guarantees. */
bool workbook_codec_read_add_format_rows(WorkbookCodecRead *read,bool table_present,size_t retained_input,char **error);
typedef struct { bool state_present,selectors_present; } WorkbookCodecFormatTagsPresence;
/* Once after complete unclaimed base format rows. current core forbids both tables and
   completes without SQL with explicit false/SPATIAL defaults. current core forbids state
   and requires complete SPATIAL selectors. current core requires state first: one I/B row
   per sheet, including false and empty-format sheets; then one I/I/I selector per
   existing row in exact(sheet,position) order. Requests admit64/32768 rows plus
   sentinels, respectively, in atomic<=256 feeds. Explicit awareness is never
   inferred from ledger/counters/bindings or selector count. Nonspatial tags need
   true awareness and known enum bounds, but scopes/guards/memberships are NOT
   validated here. True with SPATIAL or zero rows is allowed. Fixed Header-owned
   mode and existing row selectors allocate nothing; arming/every feed still
   admits all retained caller input. Whole tags readiness is separate from full
   formatting/model validity; no incomplete FormatStore escapes. */
bool workbook_codec_read_add_format_tags(WorkbookCodecRead *read,WorkbookCodecFormatTagsPresence present,size_t retained_input,char **error);
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
   The current core requires all three tables, including on sheets without Tables.
   Raw text is bounded8MiB/sheet, independently of owned-buffer admission.
   Completion also checks the known native object-current byte lower bound:
   object/name/Table/query data and actual available locator bytes. Unavailable
   native path lengths and future Pivot/Marks payloads still require completed
   candidate validation. Preserved definitions do not authorize source SQL or
   file execution or constitute complete native ObjectStore validation. */
bool workbook_codec_read_add_table_sources(WorkbookCodecRead *read,WorkbookCodecTableSourcePresence present,char **error);
typedef struct { bool fields,marks; } WorkbookCodecPivotMarksPresence;
/* Once after the complete descriptor/query/options/Table chain, before claim.
   The current core requires both families, even with no objects.
   Named9/8-column results admit65536/98304 rows plus one
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
   filters/sorts/preferences/object descriptors/object and source-version counters/ledger and binding views/base format-row properties and explicit mode/selector tags/
   queries/source options/Table sources/Pivot/Marks/columns/cells).
   It is not a complete workbook: formats, final binding adoption, historical
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
typedef struct {
    uint64_t version,owner;
    SourceKind kind;
    const char *reference,*locator;
    bool locator_resolved;
    WorkbookCodecBlob raw;
} WorkbookCodecSourceVersionView;
/* All view pointers borrow immutable Header ownership. Empty locator means
   unresolved, never path equality or a fabricated valid native SourceVersion.
   Pending raw is NULL/0. False preserves out; count is0 when unready/invalid. */
bool workbook_codec_header_source_versions_ready(const WorkbookCodecHeader *header);
size_t workbook_codec_header_source_version_count(const WorkbookCodecHeader *header,size_t sheet);
bool workbook_codec_header_source_version_at(const WorkbookCodecHeader *header,size_t sheet,size_t index,WorkbookCodecSourceVersionView *out);
typedef struct { uint64_t version; bool locator_matched; } WorkbookCodecSourceBindingView;
/* POD metadata only, never an active source/format token. Version0 is unbound
   and locator_matched=false. Nonzero+false retains a comparison deferred by an
   unavailable locator; true means only equal nonempty logical locators. Neither
   status verifies a file, RAW schema/content or capability. Getter owners are
   primary0 or live Tables only; incomplete/invalid/NULL out preserves out. */
typedef struct { FormatRange range; unsigned mask; CellFormat value; } WorkbookCodecFormatRowView;
/* POD property copies only; specified is0. No incomplete FormatStore escapes.
   Row order and all stored properties (including unmasked values) are preserved.
   Count is0 when unready/invalid; row_at false preserves out. */
bool workbook_codec_header_format_rows_ready(const WorkbookCodecHeader *header);
size_t workbook_codec_header_format_row_count(const WorkbookCodecHeader *header,size_t sheet);
bool workbook_codec_header_format_row_at(const WorkbookCodecHeader *header,size_t sheet,size_t index,WorkbookCodecFormatRowView *out);
/* POD getters require whole tag-chain readiness; false preserves out. Explicit
   awareness can remain true with zero rows. A nonspatial selector is only a
   captured tag, not proof of valid scopes, guards, membership or activation. */
bool workbook_codec_header_format_tags_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_format_source_aware(const WorkbookCodecHeader *header,size_t sheet,bool *out);
bool workbook_codec_header_format_selector(const WorkbookCodecHeader *header,size_t sheet,size_t index,FormatSelector *out);
bool workbook_codec_header_source_bindings_ready(const WorkbookCodecHeader *header);
bool workbook_codec_header_source_binding(const WorkbookCodecHeader *header,size_t sheet,uint64_t owner,WorkbookCodecSourceBindingView *out);
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
