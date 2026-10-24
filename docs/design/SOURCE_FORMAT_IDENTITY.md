# Source-record formatting identity — implementation slices and remaining plan

This document combines implemented slices and remaining proposed policy.
STATUS and ROADMAP own current capability and task state. Prerelease persistence
admits only current container16/core15; older file readers, upgrades and automatic
saved-coordinate baseline migration have been removed. Current RAW/RESULT/GUARDED
selectors, immutable origins and manual-first-Table behavior remain supported.

S1 native raw Parquet/CSV/demo Page IDs and deterministic ties remain separately accepted. The native primary CSV/Parquet slice now captures an owned bounded intent, verifies content off the graphics thread, collects the entire finite selection once, rechecks identity/fences, and commits one action. Actual sort/filter/both-page eviction, duplicate identity, selection capture, stale/cancel/no-op and first-binding Undo/Redo pass. Current core15 preserves typed scopes/ledgers/mode, and automatic lowest-priority verification reactivates compatible primary RAW styles after reopen without changing history/dirty state. Same-schema file replacement remains suspended without frame-driven retries. Current actual-App/model evidence.

The generic native controller now also supports one embedded raw CSV/Parquet Table: local record/field coordinates, full owner and pending-placement fences, one owned ObjectSnapshot/format action, complete graph reservations and verified current-format reopen/refresh. Independent same-file owners stay isolated. Actual36-stage optimized/O1 sanitizer/O0 edit and adjacent primary30/containment43/journal4 pass; a nonzero-offset raw preview retains12 styled records beside a real unstyled DuckDB PIVOT. Current Table acceptance.

Current admission is intentionally narrow: one ready raw primary or embedded Table CSV/Parquet owner, finite cell selection<=20,000 records, complete count and no incompatible finite legacy/current/history patches. Whole-column defaults and manual behavior remain. Supported archived primary/Table RESULT definitions now activate only after exact RAW/file/schema/owner and structured-definition verification; unmatched RESULT, matrix and totals tokens stay suspended. Primary24 covers RESULT/GUARDED restoration and strengthened Table36 covers Table RESULT/independent-owner/PIVOT isolation. Canonical result publication is cached outside painting, and no-op model/history ownership is preserved. Typed GUARDED current archival supports1–33 complete exact RESULT owners but creates no automatic migration guards. There is no new user-facing RESULT command. Accepted archive activation. Mixed commands, browser source epochs and complete S0–S4 product acceptance
remain open. Legacy-coordinate baseline migration is retired, including its
planner, App scheduler and compatibility-only tests.

Before the first Table import, eligible prior manual histories, including empty
and broad-only stores, remain manual. First-Table creation records the object and
format mode as one composite action; Undo restores the exact pre-import mode and
Redo restores the imported state. Blank Pivot templates alone introduce no
imported targets. Source-side manual edits can make an eligible explicit manual
handoff without issuing an imported source version. Current/Undo/Redo and current
Save/Open preserve these meanings; active imported scopes are not relabeled.

The App now uses the accepted opt-in1/8/32/64MiB category accounting and conservative Worker/SQL/I/O reservations, including canceled-but-active operations and polled replies. This is not allocator/RSS measurement or an absolute peak guarantee: complete active engine diagnostics are not capped by the reservation allowance. Do not infer verified source lineage from saved packets, session stamps or helper success. Collector/journal prerequisites retain their own source scopes.

The intended result is simple: bolding a particular imported transaction follows that transaction when it is sorted, temporarily disappears when that transaction is filtered out, and returns when it becomes visible. It must never silently style another transaction. Missing worksheet cells, SQL NULL, zero-length text and literal `NULL` remain distinct; formatting changes display only, never stored values or calculation types.

## Original coordinate behavior and remaining containment

`src/workbook/format.h` defines a sheet-local `FormatStore` with at most 512 coordinate patches and 32 snapshots in each history stack. `format_at()` in `format.c` merges matching masks newest first. Range moves and structural edits already plan and commit formatting with sparse cells/objects through the worksheet journal.

Unbound spatial stores resolve absolute coordinates. The new eligible primary command instead uses `src/app/source_format.inc` and record/field tokens shared by grid, raw preview and active format/readout. Source-aware unmatched imported targets cannot fall back to finite manual coordinates. Mixed-command and remaining result identities are necessary before universal record-following can be claimed; an unbound coordinate patch never proves the originally styled record.

`Page` in `src/data/page.h` now owns optional native raw-source record IDs plus reader-key/stamp provenance for the separately accepted S1 paths. These session locators do not establish a durable source version or activate record styles. `analysis_revision`, object revisions and worker request IDs identify session/query/cache lifetimes. They are not durable records: sheet activation increments the analysis revision, while workspace load recreates object revisions at 1. Object IDs are stable within their owning worksheet; worksheet array positions are not stable across reordering.

Coordinate patches alone contain no source/query binding from which an original
record can be recovered. Historical preview copies whole-column formats and
deliberately omits unproven row/range patches. No migration infers that provenance.

Dropping or suspending styles whenever a query changes prevents a wrong association. It does not make styles follow records, preserve a filtered-out record's style, distinguish duplicate physical records, or provide verified persistence. That containment is a useful first deliverable, not completion of the record-identity roadmap item.

## Proposed formatting semantics

These are proposed product decisions. In particular, the finite-selection limit, broad-column meaning, refresh policy and fallback for arbitrary SQL need to remain visible in the implementation handoff and acceptance record.

The 21 October [workbook TRD Refresh contract](WORKBOOK_MODEL_TRD.md#75-refresh-replacement-and-ad-hoc-state) supersedes the earlier proposal for preserving/suspending ad hoc styles across a user Refresh: replace affected data and reset ad hoc formats/value overrides while retaining compatible field formats and conditional rules. No row survival/deletion proof or reconciliation is required. This is future product behavior, not a claim that the current native refresh path or its verified within-version format coverage has changed.

| Target or operation | Proposed behavior |
| --- | --- |
| Blank/manual cells and ordinary spatial ranges | Keep worksheet-coordinate formatting and existing move/insert/delete behavior. A finite spatial style applies to manual/blank targets, not as an automatic imported-record fallback. Missing cells remain sparse; do not create values to carry a style. |
| Explicit worksheet-column or whole-sheet selection | Keep a spatial default, including future cells in those worksheet columns. Do not reinterpret this command as a permanent semantic field format. A future field-format command would be a separate product decision. |
| Finite cells in an eligible imported source | Bind the selected source records and source fields; sort/filter does not change membership. A mixed selection splits into spatial and source-owned intersections, committed as one action. |
| Entire unfiltered eligible source, all its records selected | Permit an explicit source-all selector for the selected source fields. It is constant-size and includes all records of that immutable source version. This optimization is legal only when the full raw extent is known and no row-reducing query/filter/unique/group operation is active. |
| Sort, including ties, or filter | Finite record styles follow their IDs and fields. Hidden records retain styles. Never transfer a style to a result row because it acquired the same ordinal. |
| Column reorder | Record styles follow a validated original source-field binding, not the new output column index. Field removal/type or identity mismatch suspends that binding. |
| Table/pivot placement move | Source/analytical styles follow the object's persistent ID and local fields. Spatial cells move through the existing range plan. Placement must not change record IDs or double-translate a style. |
| Custom source query, including an apparently simple projection | Initially use explicitly result-scoped formatting, not inferred source lineage. Bind its exact result definition and source version; suspend on a changed definition. Do not parse SQL with string heuristics or assume one-to-one lineage from row counts. |
| DISTINCT or Group/Aggregate | These change grain. Suspend raw-record styles; never add an internal ID to `DISTINCT *` or grouping keys. New formatting is result-scoped in the first release. |
| PIVOT, totals and chart marks | Do not propagate a transaction's cell style to a group/measure/mark. Initial matrix styles are result-scoped; totals keep the existing separate numeric-display policy. Marks remain presentation bindings, not cell-format records. |
| Return to a previous definition through Undo | Reactivate compatible retained styles after definition/version/schema checks. Suspending a style does not delete it or create another undo action. |
| Explicit user Refresh | Publish replacement data and reset affected ad hoc styles/value overrides atomically on success. Retain compatible field formats and conditional rules; no row matching, deletion proof or dormant reattachment. Ordinary retained Undo/version state remains separate. |
| Relink or unrequested external modification | Preserve the existing fail-closed identity checks; never silently reuse old record/result styles on different bytes. This safety check is distinct from the new explicit Refresh reset contract. |
| Duplicate physical records | Distinct original ordinals have distinct styles within an eligible immutable source. Equal values, including all-NULL rows, do not merge records. |
| Historical source preview | Apply record styles only when the preview carries the same source-version IDs and field bindings. Otherwise retain the current conservative whole-column-only behavior. Preview is read-only and must not mutate live styles. |

Result-scoped formatting means output positions within one exact source/query/grain/filter/sort definition. It is an explicit fallback, not a claim of record identity for volatile SQL. A repeated query containing `random()`, time-dependent expressions, sampling or external reads can produce different values even without a textual change; preserving logical records in those queries remains unsupported. The UI should identify this scope using existing status/help surfaces, without adding a wizard to every format action.

## Smallest record-identity slice

Start with native `SOURCE_CSV`, `SOURCE_PARQUET` and `SOURCE_DEMO`, with no custom query, DISTINCT or grouping. Require one concrete regular file for file sources. Globs, multiple files, remote/mutable datasets, database views and arbitrary custom SQL remain result-scoped initially. A database table needs a separately validated key/snapshot policy; its storage `rowid` is not automatically a permanent logical key.

Use `(worksheet-owned source instance, source version, original row ordinal)` as the record locator. Source instance 0 means the primary source in that particular worksheet; a table uses its existing object ID. Do not use the current worksheet array index, process pointer, query request ID, or a table placement coordinate. Version counters are persisted, monotonic and nonzero; overflow must fail without wrapping. Keep their allocation high-water mark outside undoable selector snapshots. Undo can restore an older binding, but cannot reuse an already-issued version for different data.

Generate the internal ordinal before user filters and sorting. A shared C SQL helper should expose one business projection and, only on eligible paths, an internal identity projection whose column position is known to the converter. Generate a collision-free internal alias after inspecting schema; user columns with internal-looking names must still work. `Page` owns an optional `uint64_t` vector with one ID per row. Job/reply publication binds it to the captured source instance/version and result definition; the vector and its small validated scope record travel with Page moves into spare/preview ownership. Never combine an older Page's IDs with the current App scope. Conversion validates a non-NULL integral ID, removes the internal column from visible metadata and accounts for the vector in `Page.bytes`. Existing pages without IDs remain valid. Borrowed previews never acquire ownership of the vector.

For the first uniform implementation, `row_number() OVER ()` over the raw reader is a candidate, not an assumed physical-file byte offset. DuckDB's current 1.5 [order-preservation documentation](https://duckdb.org/docs/current/sql/dialect/order_preservation) describes the reader/order guarantees and warns that sort ties are not stable. The installed native 1.5.6 [setting definition](https://raw.githubusercontent.com/duckdb/duckdb/v1.5.6/src/common/settings.json) defaults `preserve_insertion_order` to true, and its [streaming-window tests](https://raw.githubusercontent.com/duckdb/duckdb/v1.5.6/test/sql/window/test_streaming_window.test) exercise the empty-window row numbering. Set/check that prerequisite explicitly in the identity path. No new engine execution was performed to prepare this plan.

Append the original ordinal as a deterministic tie-breaker after requested sort keys on eligible row-position operations. Page, copy/range-copy and selection statistics must use the same ordering; otherwise a correctly styled visible record and an offset-based copied record can differ. Count can keep its existing order-free relation. Internal metadata must not leak into exported columns, clipboard values, statistics inputs, pivot domains, measures or SQL business schema. Preserve the existing primary filter parameter binding and resolved exact numeric comparisons.

Benchmark first-page, filtered page and deep-offset behavior before accepting the wrapper. In particular, an ordinal window may limit Parquet filter pushdown. A later file-specific implementation can use documented reader metadata, but must reproduce the same identity and ordering contract; do not silently change numbering on an existing source version. This slice does not require materializing every imported row or creating another query engine.

The subsequent native Parquet measurement now demonstrates that cost: the pre-filter window is2.2–5.4× slower on these filtered queries and its plan blocks scan pushdown. `file_row_number+1` preserves the tested IDs/business values and stays near baseline warm timings, with a retained slower fresh sorted sample. Prefer investigating this reader-specific candidate for single-file native Parquet. This does not yet accept production Page integration, CSV, collisions, multiple files or browser compatibility; no uniform window policy is selected by the original proposal.

## Definition and source-version validation

Keep record identity separate from result identity. A record binding includes raw source kind/location/relation, reader options, engine/type-schema compatibility and original field descriptors. Filters and sorts are excluded from the raw record version; they determine which record IDs appear in a result. A result-scoped binding additionally owns the complete relevant Analysis definition and, where applicable, PivotDefinition, original typed schema and owner ID. Comparison must respect existing semantic equality for filter order and ignore presentation-only marks, placement, width, scale and ribbon changes. Different owners remain isolated even when they read the same file.

For source fields, retain original ordinal, exact name and logical SQL type, including DECIMAL precision/scale. Validate them before resolving a style; ambiguous or incompatible fields suspend it. Do not use a floating display value or a rendered date/number string as identity. SQL NULL, empty VARCHAR and literal `NULL` require no special record-key encoding because the physical ordinal identifies the record.

`dx_file_stamp()` is a useful cache-change hint, not proof of content identity. During the session-only slice, compare full file identity/size/modification metadata before and after identity collection and on worker fetch boundaries. An observed change invalidates the version and prevents publication. Do not perform a stat/read/hash on every repaint. Detectable changes fail closed; this is not a transactional snapshot of a file another process is editing.

Verified cross-session restoration needs more than that hint. The proposed persistence gate requires a complete file-content digest plus reader/type-schema compatibility before record styles become active in a new session. Compute/verify it asynchronously using bounded reads; compare file metadata before and after, and reject a concurrently changed file. On macOS a narrow existing-SDK cryptographic adapter can provide the digest without a new package. Do not link review-only code into shipping to obtain a hash. Until verification succeeds, keep restored source styles suspended while showing real data normally. Session-only styles whose saved file version lacks a complete digest remain retained but inactive after reopen. Do not advertise their persistence as complete.

Explicit Refresh always creates a new binding under this initial policy, even when the bytes happen to match. A future remap by validated business keys or identical-content versions is a separately proposed product change. Hashing a file is not permission to assume that its reordered records represent the same logical records in a changed file.

## Store, history and ownership

Extend the existing formatting model rather than adding a second undo system. A proposed patch selector distinguishes spatial range, source-record set, source-all, exact-result range and the guarded legacy rectangle described below. Scope definitions are owned, deduplicated within the store by exact comparison, and patches refer to stable store-local scope IDs. A hash can accelerate comparison but cannot replace equality with a potentially colliding cache hash.

Record sets own sorted IDs or coalesced inclusive ordinal runs. Field selectors own validated source descriptors; patches never borrow `Page`, `Column`, `Cell`, query-editor buffers or worker job definitions. Format planning builds an owned next state and checks all limits before modifying the live store, history or workbook dirty bit. All selectors share the existing newest-mask precedence: an older record patch must not override a newer broad default merely because they have different scope kinds.

Keep `EDIT_FORMAT` and composite worksheet edits as the single journal sequence. Snapshot copy/free, duplication, worker save snapshots, move/axis plans and load failure cleanup must cover the new owned payload. Use deep-owned immutable snapshots first; reference-count sharing would require a separately justified C99 thread-safe ownership design. Spatial plans translate spatial selectors; source record IDs and semantic field selectors do not shift when cells are inserted beside a source. Removing an object can retain its dormant styles for Undo; a new object ID never inherits them.

Worker jobs and replies transfer owned identity data through existing latest-wins/poll ownership. Cancellation/error/stale replies release every partial buffer; a canceled collection creates no style or history entry. Stop the worker before freeing its active collector inputs. Borrowed pages/previews continue to obey the existing Page move/free contract.

## Proposed explicit budgets

The values below are implementation proposals and acceptance limits, not existing public behavior. Reject an oversized format action atomically with an explanation and a whole-column/source-all alternative when applicable. Do not silently fall back to positional formatting, trim a selection, evict current styles or truncate exact definitions.

| Resource | Proposed hard bound and accounting |
| --- | --- |
| Current patches | Preserve 512 per worksheet across all selector kinds. One command may create several scoped patches; preflight their combined count. |
| Undo/Redo | Preserve the existing 32-entry stack bounds and worksheet journal alignment. Byte limits can require pruning complete oldest actions in the edited sheet, planned atomically; never prune the format stack alone. If this cannot make room, fail the new action. |
| Live source owners | At most 33 per worksheet: primary plus the existing 32 objects. Dormant selectors for deleted objects and history-owned bindings remain subject to patch/text/byte bounds; do not assume their distinct historical IDs are also limited to 33. |
| Collected finite selection | At most 20,000 distinct physical record IDs across all source intersections in one action, plus selected field descriptors. The value reuses the current copy ceiling as an initial number, but this is a proposed **record** limit, not a 20,000-cell limit. |
| Collector result | One bounded identity query per intersected eligible owner, at most 33 queries per command. Fetch at most the remaining ID allowance plus one overflow sentinel; accumulate at most 20,000 IDs/160,000 bytes. Convert in batches of at most 256 rows, but do not rescan the relation once per batch. The final collector reply owns an ID vector, not an oversized ordinary Page. Run compression does not raise the collection ceiling. |
| Broad selections | Spatial whole-column/whole-sheet and a proven unfiltered source-all selector are constant-size. A filtered finite selection above 20,000 IDs fails initially; support for arbitrarily large frozen membership sets is not claimed. |
| Scope/field text | At most 256 KiB owned exact text per deduplicated scope; each field name at most 4,096 UTF-8 bytes. Enforce existing stricter query/type limits as well. A valid query/filter definition too large for this format budget remains usable for data, but formatting that needs its scope fails explicitly. |
| Current selector storage | At most 1 MiB per worksheet and 8 MiB across the workbook, including arrays, IDs/runs, descriptors and terminators. Include retained inactive selectors. |
| Current plus history | At most 32 MiB of formatting-owned allocations across the workbook, including scope/patch metadata. Plan any journal pruning before commit. |
| Request definitions | At most 8 MiB of collector-owned Analysis/Pivot text per job/reply. Budget for one active job, one latest pending job and one published reply: at most 24 MiB, not just the latest request. Reject oversized jobs before allocation/submit. |
| Page metadata | At most 2,048 extra ID bytes per 256-row Page. Borrowed views count once. Include active/spare/object/preview/reply ownership in App memory evidence. No full-sheet ID map. |
| Content verification | One 64 KiB read buffer and fixed digest state; one active verification operation. It is cancellable between chunks and never runs on the graphics thread. No arbitrary total file-size cap is implied. |
| Aggregate App reservation | At most 64 MiB additional formatting-owned live/history/collector/staging/I/O-snapshot data per App, excluding pre-existing values/render/engine memory. Check concrete owned sizes at handoff; this is explicit model accounting, not a replacement allocator. Defer or fail a new request when reservation is unavailable. |

Save snapshots and staged loads count toward the aggregate bound; current formats are copied without undo history as today. Do not assume an older active job is gone immediately after cancel. Per-sheet/current/history bounds remain independently enforced even when the aggregate has room. Historical stacks are session state and are not serialized. All arithmetic and inclusive-range size calculations must check overflow against the existing 2^40-row/16,384-column canvas.

These initial limits deliberately leave large filtered record selections incomplete. A future scalable membership representation must preserve frozen selection semantics and prove resource bounds; merely raising a number or retaining the live filter expression would change which records get styled later.

## Asynchronous finite-selection transaction

1. Capture an owned intent: worksheet/source/object identity, source version, exact result definition/schema, requested masks/value, selection extent, local field bindings and current format-store revision. Split spatial and source-owned intersections with checked arithmetic. Validate budgets before submitting work.
2. Reuse cached Page IDs only when their complete scope/offset/schema checks match the intent and cover the whole selected owner intersection. With partial coverage, query that entire bounded intersection once instead of separately filling every cache gap. Collection uses one logical worker operation under the captured ordering/definition, with a single bounded identity query per affected owner. Convert a result in bounded batches; do not issue a fresh OFFSET query for every 256 IDs. Selection outside both current and spare pages is included. Never run one job per cell. A source-all optimization requires a known complete extent; an unresolved count must not be mistaken for all records.
3. Keep painting/navigating while collection or version verification is pending. A selection move does not retarget an already captured intent. A source/definition/version change, format edit, Undo/Redo, sheet/document switch, save/load, or later format intent cancels it. The canceled command leaves all selectors/history unchanged. Under the existing latest-wins worker, a page request can also supersede collection; expose that cancellation through status, rather than silently dropping or automatically retargeting the action. A later explicit command may retry. Changing this scheduling policy is not a prerequisite or hidden part of the slice.
4. On a complete matching reply, validate exact IDs/fields/version and all byte/count limits again. Build the owned next FormatStore plus any spatial intersections. Commit once as one worksheet action and set dirty once. A stale reply is freed without partial application.
5. Rendering, active-format state and source preview read the same resolver. The resolver has no database, filesystem or worker calls and performs no allocation. A no-format fast path remains cheap. Index owned record sets for bounded lookup; add a retained visible/page-style cache only if measured and with an explicit separate byte cap/invalidation key.

There are **zero additional SQL jobs caused by repaint**. A finite format command can schedule one logical collector job with at most one bounded identity execution per affected eligible source, and version verification can schedule one bounded-buffer I/O task. Schema preparation, if required, uses the existing cached schema boundary and is reported separately. Counters/evidence must report these command costs separately from idle frames; a selection spanning several owners is not promised a single SQL execution. A pending command must have an existing status/cancel path rather than blocking the UI or claiming the style is already applied.

## Current persistence contract

Native core15 requires all 28 metadata tables, durable document/sheet identities,
object/source high-water counters and explicit format mode/selectors. The user
file is container16 with independently validated complete core roots. Exact
current schema, bounds, owner references, UTF-8 and typed selector validation are
mandatory before candidate adoption or destination overwrite. Unsupported files
reject without changing the live workbook or original bytes.

Retain RAW/RESULT/GUARDED archival and supported verified activation; these are
current product semantics, independent of retired compatibility migration. No
coordinate patch proves original record authorship. Production format migrations
can be designed when an end-user compatibility obligation exists. Browser
metadata component proof does not establish browser Open/Save.

## Progressive deliverables and acceptance

| Slice | Concrete implementation and gate | What it does not close |
| --- | --- | --- |
| S0: scoped containment | Extend pure selectors/resolution; suspend finite source/result styles after incompatible query/schema/version changes; preserve raw/manual values and broad spatial styles. Pure release/ASan tests cover precedence, suspension, ownership, planning failure and Undo. A focused native filter/sort case proves no wrong-row carryover. | Record-following, arbitrary selection collection and verified save/reopen remain open. |
| S1: native raw-source provenance | Shared C SQL helper and owned Page IDs for eligible raw sources; stable sort ties shared by positional consumers; exact hidden-column removal and cleanup. Native real-engine tests exercise pages 0/256/512, filters and duplicate records. Measure cold/first/filtered/deep-page cost and memory against the same source without identity. | No record style feature is claimed from a Page helper alone. Custom query/grain and browser bridge remain outside scope. |
| S2: integrated record formatting | Asynchronous captured-intent collection, bounded record/source-all selectors, single existing journal and common renderer/readout resolver. Focused native tests prove actual style follows records across sort/filter/page eviction and source/table moves, including stale/canceled work. Accept the proposed selection/budget messages. | This can be accepted as session-only behavior; verified cross-session restoration is still open and must be labeled so. |
| S3: current persistence | Native digest/version verification, transactional current workspace metadata, duplication and failed-load preservation. Save/reopen reactivates only verified compatible styles; refresh/relink/content/schema changes suspend them. Core then workbook fixture ordering and focused native document flow pass in release/ASan. | Business-key remapping across changed files, custom SQL lineage and aggregate/mark identities remain explicit limits. |
| S4: scope/product acceptance | Verify fallback labels and agreed behavior for DISTINCT, grouping, custom SQL, PIVOT/totals and large filtered selections. Record native reference states and exact supported/unsupported cases in the current status/roadmap. | Do not check off universal source-record identity if those limits conflict with the accepted product requirement. |

S0–S2 are development acceptance boundaries. Supported typed selectors persist
losslessly in exact core15, with primary/Table RAW and supported archived
RESULT/GUARDED reactivation verified separately. Unsupported payloads fail before
touching a destination. Remaining S3 verification and agreed S4 scope stay
explicit; compatibility migration is outside prerelease scope.

Implementation file ownership should follow [ARCHITECTURE.md](../ARCHITECTURE.md): the integrator owns App/shared interfaces/build; a narrow pure model lane can own `format.*` and focused tests; an engine lane can own the shared SQL helper/native converter and real-engine tests. Persistence must be serialized with workbook model changes. Small reviewed commits do not imply simultaneous edits of shared headers or generated fixtures. No new dependency or global architectural rewrite is needed for S0–S2.

The minimum integrated scenario uses more than 600 raw records, tied sort keys, two physically identical rows, SQL NULL/empty/literal `NULL`, BIGINT beyond 2^53 and exact DECIMAL. Format one duplicate independently; sort both directions, filter it out and restore it, evict both cached pages and return. Assert membership and field binding rather than a particular screen row. Repeat for an embedded table beside manual values, then move the whole table and undo/redo the move and style. An independent source instance reading the same file must not inherit the patch.

Collection tests cross both cached pages, hit exactly 20,000 IDs, reject 20,001 atomically, cover mixed spatial/source selections, and cancel after one or more chunks. Exercise source/query changes, a competing format edit, sheet switch and worker stop before late completion. Model tests cover every count/byte boundary, malformed persisted selectors, deep copy/free, empty and overlapping runs, source-all precedence and overflow. No assertion may merely check that the old coordinate stayed bold.

Native persistence tests use exact current fixtures, a maximum 512-patch file, overlapping current styles, an unavailable source, a changed file, same-name replacement, schema/type change, guarded-part movement, byte-budget failure and immutable original-file preservation. New metadata and the content digest must be associated with the tested source version, not a later worker reply. Database/custom SQL/aggregate cases verify honest result scope and suspension without contaminating calculations.

Use [TESTING.md](../TESTING.md): smallest affected pure release/sanitizer tests during edits, private core-before-workbook fixtures, then one focused hidden native profile through the desktop lock. Keep real OS/file-panel acceptance separately scoped. Record revision/binary, eligibility, completed stages, SQL/job counts, additional owned bytes, first-page and command-collection/digest duration, and optimized frame tails with zero/maximum styles. No repaint SQL or steady-frame allocations is an invariant; sanitizer timing does not establish responsiveness. Browser Page/protocol/persistence acceptance is a later coordinated adapter slice, not implied by native success.
