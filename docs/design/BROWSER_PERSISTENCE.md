# Browser workbook persistence adaptation


**Browser workbook Open/Save remains unavailable.** The shared C99/raylib UI
and official asynchronous DuckDB-Wasm Worker remain the implementation boundary.
Current native user files require container16 with exact core15 roots; prerelease
readers/defaults/upgrades for earlier formats are removed. BP0 cross-engine
current-core rewrite acceptance and BP1 shared metadata decoding are components,
not product Open/Save. STATUS and ROADMAP own current completion evidence.

## Existing behavior and the missing seam

| Area | Existing implementation | Required adaptation |
| --- | --- | --- |
| Workbook model | [`workbook.h`](../../src/workbook/workbook.h) and the shared part of [`workbook.c`](../../src/workbook/workbook.c) own sheets, source/analysis definitions, manual cells, formatting, objects, views and deep snapshots. `DX_WORKSPACE_VERSION` is exact core15, including durable document/sheet identities; user files use container16 with independently validated complete roots. Snapshots omit session Undo/Redo and Pages. | Preserve the model and its validators. Add asynchronous operation lifetime rather than blocking a browser frame until a synchronous API returns. |
| Native `.dxls` | `workbook_save/load` use direct DuckDB C queries, typed prepared bindings/results, a transaction, `_duxcel_workspace` tables and native relative-path helpers. Load constructs a temporary Workbook and replaces the caller only after validation. Only exact core15 is admitted internally. Native user Open/Save uses the current container16 archive boundary, complete root closure and current typed source/format metadata, without source queries. | Execute equivalent metadata SQL in the official Worker. The browser cannot call the native engine or reuse its live C handles. Native persistence remains functional during extraction. |
| Browser persistence | [`workbook_browser.c`](../../src/workbook/workbook_browser.c) explicitly rejects load/save. The shared model is present; persistence is absent. | Replace the unavailable adapter only as complete asynchronous paths pass acceptance. A successful renderer or review upload is not a workbook save. |
| Browser engine/files | [`engine_host.mjs`](../../web/engine_host.mjs) serializes source-file administration with queries; [`official_worker.mjs`](../../web/official_worker.mjs) opens an in-memory official engine. The current chooser accepts CSV/TSV/Parquet up to 256 MiB, reads the whole file and registers an ephemeral `import-N-name` buffer. | Add metadata administration, document operations, portable logical source references and recovery storage. Accepted epoch/registration tokens guard immutable registered buffers only within one engine session; neither those tokens nor ephemeral virtual filenames are persisted source locations. |
| Existing typed transport | [`worker_browser.inc`](../../src/data/worker_browser.inc), [`browser_query.c`](../../src/data/browser_query.c) and [`page_wire.c`](../../src/data/page_wire.c) copy bounded query results into C-owned Pages with session/request identity. | Reuse the identity/ownership rules and bounded wire primitives; introduce distinct metadata operations. Do not coerce arbitrary workbook metadata into an ordinary latest-wins source-page job. |

The pinned package is `@duckdb/duckdb-wasm` 1.32.0 / engine 1.4.3; native acceptance uses engine 1.5.6. The installed `web/node_modules/@duckdb/duckdb-wasm/dist/types/src/parallel/{async_bindings,async_connection}.d.ts` and `bindings/config.d.ts` were checked for this update. They expose `registerFileHandle`, `registerOPFSFileName`, `flushFiles`, `copyFileToBuffer`, `globFiles`, `open`, asynchronous connections/prepared statements and explicit OPFS file handling. Declarations alone do not prove runtime behavior: the accepted `.dxls` export path still needs a registered valid-file baseline. No package upgrade is required for this plan.

## Current storage contract

The planned exported `.dxls` remains a real DuckDB database with the native
container/root contract. Browser JSON transports bounded metadata and local
capability indexes; it is not a substitute workbook format.

Exact core15 has28 required tables: the shared inventory in
[`workbook_codec.c`](../../src/workbook/workbook_codec.c) is authoritative, including
`workbook_identity` and `sheet_identity`. No older layout/default branch remains.
Native external files require container16 with codec-v1 version metadata and
independently admitted core15 payloads. The BP0 core probe does not yet implement
that container or the browser document lifecycle.

Validate exact known tables, named columns/types, SQL NULL and embedded NUL,
contiguous positions, complete primary/Table bindings including version0, durable
identities/high-water bounds and typed scope/member references. Physical column
reordering is accepted through named reads/writes. Unsupported formats and unknown
reserved metadata reject without replacing the live candidate or original bytes.

Current core15 accepts SPATIAL, RECORDS, RESULT and GUARDED selectors. RECORDS requires one canonical RAW packet; RESULT requires one canonical RESULT packet; GUARDED requires 1–33 distinct owners' canonical RESULT guards. Packets must match complete ledger RAW proofs exactly; unknown encoding, opaque blobs, dangling references, pending proofs used as guards and zero-guard GUARDED rectangles fail atomically. Pending ledger entries themselves are valid and retained. A packet is strictly below 256 KiB; ledger entries are bounded to 1,024 per sheet. The codec permits at most 512 scopes/format patches per sheet, 20,000 record members and 16,384 original-field ordinals per selector, with contiguous membership positions. Combined current format/ledger ownership is at most 1 MiB per sheet and 8 MiB per core archive. History and App staging reservations have separate current/retained/in-flight accounting.

The native current codec archives proofs without verifying external files. Native App activation separately re-verifies content/schema/reader/engine and owner/definition identity; supported primary/Table RAW and archived RESULT/GUARDED activation do not imply browser verification. A browser decoder must preserve these payloads while leaving their source styles suspended. Native Save As resolves relative locators and preflights the restored representation's bounds; the browser needs an explicit logical-locator adapter, not OS `realpath` or resolution against the review server URL. Preserve externally owned data outside the metadata schema when an accepted workbook contains it; an unrecognized database must not be overwritten.

Keep Duxcel container/core schema, DuckDB storage version and engine/package
versions separate in evidence. Native and browser engines can differ even when
the Duxcel schema matches. Probe the pinned engine using actual current files;
strict typed comparisons, original-file hashes and native readback are required.
The current BP0 rewrite targets `STORAGE_VERSION 'v1.0.0'`; its actual Worker
acceptance must close separately from shared C metadata tests. A platform adapter
alone does not require new persisted semantics. Production file migrations are
deferred until an end-user compatibility obligation exists.

Importing a workbook must not automatically run persisted source SQL, install/load extensions, contact remote URLs or grant local file access. First validate metadata in a staging catalog, disable automatic extension installation/loading for metadata reads, and resolve source capabilities separately. Valid definitions outside accepted browser execution remain owned and preserved with an unavailable execution state; silently discarding a PIVOT/Marks definition to permit a browser save is unacceptable. Bounded Rows/Columns PIVOT, totals, statistics, Copy and Parquet export already have their own real browser acceptance; none is a workbook codec. Unknown future schema/enum values fail load atomically.

## Shared C codec and asynchronous lifetime

Extract only metadata validation, table descriptions, row encoding/decoding and exact current-version admission into an engine-independent C99 seam. Native execution still binds typed parameters through DuckDB C; the browser executes C-built SQL through the existing Worker. Keep source-query SQL in the existing C transforms. JavaScript routes commands, file capabilities and typed results; it does not construct the workbook model, infer cell types or implement migrations.

Use ordered bounded commands/results with an operation kind, document/session cookie, monotonically increasing ticket and snapshot generation. Candidate commands are begin-open, read-metadata-batch, begin-save, write-metadata-batch, commit/checkpoint, abort and export-file. A save takes a deep `workbook_snapshot` (which can fail on invalid owned source metadata); add an explicit document mutation generation so completion clears dirty state only for that exact saved generation after durable delivery. Newer edits stay dirty. Save must not borrow mutable App strings or undo stacks while an asynchronous request is pending.

The metadata transfer contract carries explicit column descriptors and a separate null flag. UInt64/int64/DECIMAL values cross as canonical text or exact binary fields, never an unsafe JavaScript `Number`; booleans and finite presentation doubles retain their proper types. SQL NULL, an empty VARCHAR and literal `NULL` remain distinct. Sparse absent manual cells are absent records; do not manufacture empty-string cells. The current manual-cell model is text-only: persistence must preserve its current semantics, not invent typed manual editing.

Implement bounded reads and writes, initially at most 256 rows, 4 MiB of payload and 6 MiB of wire per transfer, and at most 1 MiB of C-built SQL, matching normal page/SQL ceilings. One oversized row fails explicitly; do not trim text to fit. Preserve the native per-sheet/model limits, ordered IDs/positions, UTF-8 and numeric validation; add an aggregate snapshot/transfer byte budget before offering Save. Avoid concatenating an entire 64-sheet workbook or all editable cells into one SQL string. If using C-built INSERT literals, use the existing safe SQL quoting and explicit casts for exact numeric values; do not interpolate arbitrary strings in JavaScript.

C owns snapshots, decoded rows, the candidate Workbook and final model. The host immediately copies any asynchronous command payload borrowed from the C heap; Worker buffers and browser file handles stay host-owned. Each reply is copied into C allocation before temporary host/heap allocations are released. No live pointer or DuckDB connection crosses modules. A stale callback may free its own temporary resources but cannot publish into a new document.

Extend the existing administration chain to drain/cancel source reads before a metadata operation owns the connection/catalog. Saving must not be superseded midway by viewport scrolling. Continue C input/rendering while pending; pause publication where a document replacement requires it. On failure, roll back, close temporary readers/catalogs/registrations and retain the current workbook and dirty flag. A committed save cannot be undone by ignoring its callback; define the commit boundary and account for any completed staged file during shutdown/recovery.

### Verified current metadata components

The exact core15 inventory/schema and opaque bounded `workbook_codec_read` Header
are shared C. Native release/sanitizer comparisons cover normal/reordered current
fixtures and malformed current metadata; the frozen current matrix passes all40
results in332.446s. Historical compatibility-success variants are removed.

Fourteen actual official-Worker family profiles pass over current15/layout25
fixtures: sorts, preferences, objects, queries, options, Table/Pivot/Marks, filters,
grid cells/columns, object/source counters, RAW ledger, bindings and base format
rows. Current malformed preference/query fixtures remain. Their133.346s summed
runtime covers3,641,520 C checks,94 atomic rejections and17 stale observations.
The independent base-header gate passes9.147s. Per-profile source and artifact
evidence is recorded in ROADMAP, not duplicated as historical checkpoint files.

These components preserve exact integer/DECIMAL and NULL/empty/literal text,
bounded owned staging, stale/cancel/failure cleanup and independent prior Header
ownership. They do not construct a complete Workbook or activate source proofs.
Format-state/scopes/guards/memberships, whole-candidate validation, container/root
closure, writer batches and browser App publication remain distinct work.

### Next BP1 implementation

BP1 is not a request to port the entire native SQL loader in one change. Continue the existing shared `workbook_codec` policy and opaque `workbook_codec_read` staging modules with their focused tests; the integration owner keeps App, Worker/public interfaces and build changes. Use this sequence:

1. Use the existing exact current inventory/schema and native validator; keep common semantic validation in C. Do not restore historical layouts/defaults or copy model rules into JavaScript.
2. Add an owned decoder/cursor for bounded `meta`/`sheets` batches first, then extend one metadata family at a time until every required current table is covered. The cursor requests C-built, explicitly ordered named-column SQL; native DuckDB rows and browser transport feed the same row decoder. A partial family pass must not expose product Open/Save. Use schema descriptors checked before canonical projection, explicit NULL flags, exact scalar parsers and bounded hex-to-BLOB decoding for packets; ordinary source Page fallback rules are not permission to accept unknown metadata types. The native adapter still uses typed prepared bindings for writes.
3. Make asynchronous lifetime independently testable with an operation key of `(document_epoch, operation_ticket, snapshot_generation)`, all exact nonwrapping uint64 values. These are proposed operation inputs, not fields already present in Workbook. The codec owns its candidate/snapshot and cursor; the host copies each borrowed request before awaiting and carries the same key back. Late/mismatched replies clean up without publishing. Cancel before commit discards the candidate; completion after commit must retain an owned staged result until explicit delivery/cleanup. Do not reuse `analysis_revision` as the workbook generation: manual cells, object/title/format/source-ledger changes, sheet order, preferences and persisted view state also matter.

A successful decoder finish moves a fully validated candidate once; failure preserves the destination, source bytes, prior cache and dirty state. Pass an explicit locator resolver: native uses current relative-path behavior; a browser component preserves saved logical references without granting execution or rewriting them to `import-N-*`. It must load no source SQL or extensions. Snapshots omit Undo/Redo and Pages, but preserve durable owner/source high-water marks, all current definitions and inactive scopes. Project snapshot, transfer and current/staging coexistence budgets before allocating or publishing; the current format/ledger cap is not a whole-workbook or process-memory cap.

Keep each initial family integration behind the test boundary: C-generated metadata requests against private read-only current15 normal/reordered fixtures, decoding into the opaque partial Header with native comparisons. Only after all required families are validated may the adapter construct a complete staged Workbook. The BP0 JavaScript fixture copier is not that decoder. Add shared C-generated write batches and an actual Worker rewrite only after the read path and native regressions pass; use a fresh registered valid copy until blank creation is separately proved. Writes retain named columns, explicit exact casts/bindings and the native exact-version decision. Accept original-file hashes and unrelated-table preservation, not only exported row counts.

Focused gates: release/ASan deep ownership and unchanged-output failures; strict current fixtures and malformed packages; split/batch-boundary and oversized-row errors; NULL/empty/literal strings; UINT64_MAX and values above2^53; exact BLOB bytes; stale operation keys; edits/Undo/new document between capture and completion; cancel/read/write/commit/export failure and recovery. The first App handoff must audit every persisted mutation before enabling generation-based dirty clearing. Until a usable output channel completes, report only a staged/export-requested result and keep dirty/recovery state. No new schema version is needed for this adapter.

## Source references and file permissions

Current session identity is already implemented: successful registration returns canonical nonzero uint64 `engineEpoch`/`registrationId` strings; App captures POD tokens for up to 33 unique declared primary/Copy/Statistics file paths. Every SQL stage, including schema/domain/totals and export, carries the frozen vector; schema/type caches require the same exact names/order/tokens. The official adapter validates before admission/send and after completion. The actual App680 and Worker221 gates include cached contributors, revocation, malformed vectors and atomic budget denial/recovery. Unbound low-level callers retain their explicit legacy contract; missing required App bindings fail closed.

This protects registered immutable buffers sharing one database object during one session only. It does not infer undeclared custom-SQL/remote dependencies, detect raw database/file writes outside that API, unify separate wrappers around an engine, hash content or survive engine replacement/reload. Names/IDs are permanently burned within the bounded 1,024-name registry. Friendly original filenames are session UI labels, separate from executable keys and identity. None of these tokens or labels may be promoted into native `Analysis.source_version`/SourceVersions proofs or serialized as cross-session authority.

Keep a persisted logical source reference separate from its current engine registration. The current `Analysis.path` is used to build SQL; introduce a narrow resolver/binding seam so browser runtime aliases do not replace the saved reference. The future persistent binding includes the snapshot's sheet/object owner, source kind/reference, reader options and expected schema/version hints; it is separate from the accepted runtime token vector. Sheet reorder/duplication/save must rebuild the owner mapping for the new generation; table/object owners cannot share a binding merely because basenames match.

For Chromium-capable hosts, acquire `showOpenFilePicker`/`showSaveFilePicker` handles directly during the originating trusted gesture, before asynchronous snapshot/engine work. Store permitted handles in a separate origin-local IndexedDB directory keyed to the committed document generation; check permissions on resume and request permission only on an explicit user action. The existing raylib frame dispatch can occur after the DOM event's transient activation, so the platform adapter must prove gesture handoff rather than assume a later frame can open a picker. Retain `<input type=file>` and download fallbacks. [Chrome file/gesture/handle contract](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access).

Reload should restore definitions immediately and show unresolved sources without prompting. Resolve a remembered handle only if permission and expected source checks pass; otherwise expose the shared C Relink action. Canceling a picker changes nothing. Relinking affects only the intended owner and invalidates its schema/pages/query generations before execution. Match source kind and schema; size/mtime/name are hints, not a verified content identity. Source-record styles requiring verified provenance stay suspended until their own identity contract passes.

Prefer registering a selected `File` through the pinned official `BROWSER_FILEREADER` handle path for bounded random access, once measured and accepted, rather than unconditionally cloning a 256 MiB source. This is a separate improvement to the existing buffer importer. No implicit copy of external data into OPFS occurs: offer an explicit retained-source copy only with quota, progress, cancellation and identity checks. Large-file import caps stay honest until streaming/handle paths pass. [Official file registration API](https://duckdb.org/docs/clients/wasm/data_ingestion).

## Save, recovery and portable export

First deliver explicit import/export through a private staging database. Validate imported bytes and metadata before swapping the current document; write metadata transactionally, then checkpoint/flush and copy through an accepted official API sequence for bounded `.dxls` download or a user-granted writable stream. Never export a file while it still depends on an unexported WAL. A download request is not proof of a completed durable save, and must not clear the only dirty/recovery copy. In a file-handle path, report success only after the write stream closes successfully.

The current BP0 component registers a separately owned valid native core15
database buffer, copies all28 fixture metadata tables using engine-described
DDL, commits/checkpoints/flushes and exports while the private writer is quiescent.
Native readback must verify exact typed values, document/sheet identities,
allocation counters, bindings and independent ownership. The new actual Worker
profiles remain pending until ROADMAP records their terminal acceptance.

A valid-file clone does not prove blank-document creation or the container16
adapter. Earlier blank-file probes failed exported-new-file and empty-buffer
attachment; do not infer picker/file method behavior from declarations. Prove a
working blank-file/handle adapter before offering New→Save. Catalog types and
nullability use canonical typed projections; C owns metadata validation. Export
compaction cost and unrelated user-table preservation remain separate gates.
Typed Parquet download has independent acceptance and does not resolve blank
DuckDB workbook creation.


An exported workbook retains source references, as native does; external CSV/Parquet files are not automatically embedded. Preserve usable native relative references when importing and re-exporting a native workbook. For browser-selected files without a directory capability, save a logical reference and require relink after moving to native. A `.dxls` that opens with missing sources is different from a self-contained data package. Do not promise the latter in this milestone.

Then add origin-local recovery under one stable, product-specific namespace, separate from review IndexedDB/outbox/service state. Use the official Worker's OPFS path and explicit manual file registration where verified; no synchronous filesystem work belongs on the UI thread. The official guidance requires checkpointing and releasing exclusive handles before another engine opens a file. Exercise the exact pinned implementation rather than treating current documentation as a pass. [Official OPFS lifecycle](https://duckdb.org/2026/09/18/opfs-wasm).

Persist generations: write a new staging generation, commit/checkpoint/close it, verify reopening, then atomically advance a small IndexedDB committed-generation pointer. Keep the previous good generation until the pointer commit is confirmed. OPFS and IndexedDB do not share a transaction; after a crash, an unreferenced staging file is an orphan, not the newest accepted workbook. On recovery, validate the pointer and file, fall back to the prior committed generation on corruption, and offer explicit recovery without discarding the current document. Prune only owned inactive generations after successful commits.

Allow a single writer per document/origin using a feature-detected cross-tab lock plus the OPFS engine's real exclusivity. A second tab must offer read-only/open-a-copy behavior or an explicit busy result; it must not write concurrently or steal a live engine's handle. Close/detach/drop in order on document change, Worker termination and page lifecycle. Do not depend on `beforeunload` to finish an asynchronous save.

Estimate storage before staging large writes and handle quota failures at every actual write/flush/commit. Retain the old pointer/generation and unsaved C snapshot on rejection. Persistent-storage requests may be declined, estimates are not reservations, and origin data can still be removed by the user. Surface recovery-only versus externally exported state through the existing C document/status controls, through the existing native UI. Changing scheme/host/port creates a different storage scope; the review service is never required for product workbook persistence. [Storage persistence/quota model](https://storage.spec.whatwg.org/).

## Ordered implementation and acceptance slices

| Slice | Smallest delivery and gate | Still incomplete after the slice |
| --- | --- | --- |
| BP0: pinned API/format probe | Current core15 /28-table actual Worker rewrite/export with exact typed metadata and native release/sanitizer readback. Keep source hashes and independent owners; no downgrade helper. Remaining acceptance follows ROADMAP. | No container16 browser adapter, shared C writer, App Open/Save or recovery. A valid core clone is not New→Save. |
| BP1: shared C metadata seam | Current inventory/header and bounded families verified above; finish format metadata and whole-candidate closure, then C write batches. | No user Open/Save until complete container/schema, output and App generation boundaries pass. |
| BP2: explicit browser workbook Open/Save | Actual asynchronous shared-C lifecycle saves/reopens blank/manual multi-sheet and complete supported current metadata, including dormant styles and unavailable definitions. Prove current native archive→browser→native; refuse unsupported files atomically. | Durable origin recovery, remembered handles and full analytical execution remain separate. |
| BP3: source binding/relink | Real CSV/Parquet sources, independent table owners, duplicated/reordered sheets, denied/revoked permissions, same-name different files and missing source recovery. Validate remembered handles or require Relink; preserve portable references through export. Keep exact integer/DECIMAL/NULL semantics in actual queries after relink. | OPFS recovery and browser content/record-identity verification remain separate; accepted bounded PIVOT/totals/Copy/Parquet export keep their own scopes. |
| BP4: recoverable origin saves | Real OPFS writes and reload, forced Worker loss at each commit boundary, quota/write/flush failure, corrupt latest generation, orphan cleanup and two competing tabs. Old committed data remains usable; user-exported `.dxls` survives clearing the origin. Read-only/copy behavior is explicit when locks or capabilities are absent. | Broader platform/browser matrix and distribution quality. |
| BP5: resource/platform acceptance | Measure cold/warm startup, download bytes, UI/engine memory and transfer copies, save/reopen timings and rendered frames during pending save/query. Test supported target browsers and state exact missing capabilities; verify normal/review artifact separation. | Renderer/input/analytical/accessibility features not exercised by these gates. |

All graphical acceptance uses the existing desktop lock and an owned headless browser profile; native compatibility checks use private generated databases. No user app/window/profile, user workbook, visible picker or system preference is controlled by these tests. Keep source/binary/fixture hashes, actual engine version, elapsed times and injected-failure points with evidence. Physical picker/trackpad/IME behavior and non-Chromium platform coverage remain separately scoped.

This update introduces no product Save/Open implementation or schema bump; the exact current native contract must be preserved. Probe execution and product acceptance have separate scopes; retain the ordered slice gates and source/binary evidence. Browser Save remains unavailable; do not treat this document or the BP0 probe as completed browser product parity.
