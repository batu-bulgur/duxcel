# Architecture and ownership

Duxcel has one C99 spreadsheet implementation, raylib rendering and DuckDB computation. Native macOS and browser Wasm use the same App, grid, widgets and analytical definitions. The browser has platform glue, not a second UI.

| Area | Responsibility |
| --- | --- |
| `src/app/` | Owned AppSession lifecycle, commands, jobs, history, source/object selection and orchestration |
| `src/grid/` | Viewport, cell/extent selection, scrolling, resizing and header geometry |
| `src/ui/` | C widgets, ribbon, shelves, panes, typography and renderer/cache ownership |
| `src/transforms/` | SQL/analysis/PIVOT/Marks/number/chart semantics shared by both platforms |
| `src/data/` | Typed Pages, source identity, native DuckDB worker and asynchronous browser bridge |
| `src/workbook/` | Sparse cells, formats, objects, versioned `.dxls` and shared metadata codec |
| `src/platform/` | Portable boundary plus Objective-C AppKit windows, file panels, shortcuts and precise gestures |
| `src/dev/` | Development-only semantic review registry, captures and reporting; excluded from production |
| `web/` | Emscripten startup, DOM/device/file/clipboard/WebGL lifetime, official DuckDB-Wasm Worker transport and browser tests |
| `tests/` | Authored C/Objective-C tests and small immutable inputs/oracles |
| `tools/` | Offline fixture/icon generation, builds, test orchestration, browser automation and external review queue |
| `assets/`, `packaging/` | Runtime fonts/icons/licenses and platform metadata |

## State and resource rules

`AppSession` owns start/frame/finish. Native `app_run` drives it synchronously; the browser host schedules the same frame callback. C owns model/commands/SQL/jobs/Pages. Worker replies are accepted only for matching owned requests; cancellation, stale replies and bounded transport cannot replace accepted state partially.

Keep SQL NULL, empty text and literal `NULL` separate. Preserve canonical integer/DECIMAL strings. Page, schema, cell, format, image, font and request ownership must be explicit. Rendering cached state must not issue queries. UI scale, sheet zoom and framebuffer density remain separate.

Native AppKit is intentionally Objective-C behind the portable platform interface. The browser requires a JavaScript boundary to DOM events, browser file/clipboard APIs and the official asynchronous DuckDB-Wasm package. Moving those calls into C string macros would not eliminate that boundary.

macOS workbook Open Document callbacks enqueue owned filesystem paths in the adapter (16 pending paths, whole-event admission). A will-finish-launching hook installs the delegate before cold-launch documents arrive inside raylib window creation. Ordinary C frame dispatch consumes one eligible request through the existing document controller, preserving unsaved-change guards, ongoing file IO and frozen review boundaries. Closed document surfaces can reopen; shutdown frees pending paths and removes the launch observer. Other hosts keep a no-op boundary. Bundle document/UTI declarations live in `packaging/Info.plist`.

The macOS adapter owns an explicit application menu rather than depending on GLFW's optional defaults. Menu Quit and its Command-Q equivalent call AppKit `terminate:`; the delegate records a quit request and defers termination to the ordinary C document controller. AppKit does not bypass the dirty guard. Shutdown restores the prior application/Services menus and delegate before raylib teardown, supporting repeated App sessions.

The shared workbook metadata reader is staged infrastructure, not completed browser Open/Save. Complete candidate validation, migration, activation, writer/output adapters and recovery remain roadmap work.

## Feature seams and build identity

`src/app/selection.c` owns selection expansion from borrowed sparse cells and
resolved body/header/owner/primary/used bounds. Its typed outcome reports whether
the selection stays inside or selects the whole owner. The App adapter verifies
page/result/source revisions and applies UI focus changes; the controller owns
no worker, workbook, history, page allocation or renderer. Pure module checks
and the real App GUI slice exercise the same controller. Extend this pattern
when a feature changes rather than pre-emptively rewriting all App state.

`src/workbook/cell_command.c` is the first admitted mutation producer. Its v2
in-process envelope freezes durable workbook/sheet IDs, a process/session fence,
expected CellStore and admitted-document revisions, coordinates and bounded
borrowed bytes. The pure admission check rechecks primary/object protection and revision fences;
the mutation producer repeats it before adoption, then publishes
one cell-history action, dirty state and one monotonic command revision only for
a changed value. No-op and failure preserve all three. The App owns drafts,
source readiness, navigation and feedback. The persisted command clock covers
this admitted subset and coordinated history replays; direct legacy
paste/range/object producers still use their own store revisions. It is not a durable receipt or retry protocol.

`src/workbook/identity.c` owns bounded identity validation/allocation/restore.
The first sheet allocates a 128-bit workbook ID from OS/browser cryptographic
entropy, with no time/PRNG fallback. Sheet IDs use a non-recycled uint64 high-water
outside Undo. Rename/reorder preserve them, duplicate allocates another, and Save
snapshots retain the exact durable identity/clock. Transient session fences are
not copied to snapshot/load, so an outstanding draft cannot replay after reopen.
Exhausted counters still allow Open/Save of existing data but reject allocation
atomically. Native entropy and shared-C model behavior are tested; browser crypto
and Windows entropy branches are not platform acceptance in this slice.

Native v15 adds required workbook_identity(high,low,sheet_high_water,command_revision)
and sheet_identity(sheet_id,stable_id) tables to the strict v14 metadata package.
Existing integer sheet_id values remain archive ordinals. The adapter validates
exact types, SQL NULL, row counts, complete ordinal coverage, uniqueness,
nonzero IDs and high-water bounds before publishing a staged Workbook. Native
v1–v14 Open generates identities in RAM without modifying the file or making it
dirty; Save upgrades transactionally. Snapshot/Save As preserve logical identity.
Physical copies can share an identity but their runtime fences remain separate.
The shared partial browser decoder still rejects v15; browser Workbook Open/Save
and final staged-model activation remain incomplete.

`src/workbook/version_index.c` owns pure version metadata admission and
publication planning. It borrows immutable index descriptors and returns an
owned value plan; it performs no allocation, IO, payload decoding or App mutation.
Complete-payload identities are opaque32-byte values, with presence separate
from key bytes. Index generations order metadata publications and do not identify
document content. A transient write-owner token separates physical runtime
owners sharing durable document IDs. User Save can move one Latest Saved pointer;
private checkpoints/exports preserve it, and historical owners can only export.
The native adapter must retain and verify complete assets before publication.
`workbook_save_core_payload` writes a relocatable v15 core leaf through the
existing controlled transaction, storing already-resolved primary/Table/ledger
locators absolutely. It does not read external data, retain results or publish
an index. Ordinary Save keeps relative references. `core_payload.c` owns the closed leaf, private child and exclusive lease. Its
fixed64 KiB SDK hash binds exact bytes/file identity; it rechecks inventory and
identity before consumption/disposal. Unknown or changed artifacts are kept.
The core info is deliberately distinct from a complete version-root descriptor;
exact-byte hashing alone does not prove semantic equality of separate captures.
`core_compare.c` compares every persisted v15 descriptor table in both directions
as typed SQL multisets under64 MB/one-thread engine limits, with private spill,
pending-task cancellation and pre/post sealed-byte verification. It compares
source query text as data and never queries external sources. This full core
proof supports later reuse; it does not certify assets/results outside the codec.
Archive closure and App adoption remain W11a2/W11a3; native files remain v15.

`src/workbook/history.c` coordinates the existing session journals. Each
worksheet_record receives a non-recycled process-order stamp; transfers retain
it and prune/cap operations advance a lost-history floor. Stable IDs identify the
selected sheet independently of its name/position. The coordinator scans bounded
journals for the newest Undo or earliest Redo and refuses misaligned constituents,
ambiguous targets and revision overflow. It stops at the largest discarded order,
so a retained older action on another sheet cannot jump a missing interval.
A separate smallest lost-future order stops Redo after constituent eviction; it
does not block valid older Undo. A new branch clears this future ceiling.
No duplicate snapshot storage or persistence schema is introduced.

Cell/style/composite transfers, dirty state and command revision publication are
owned by C; the App follows the affected sheet, restores column presentation and
checks current source layout. Analysis/object replay delegates to the existing
live-Analysis/cache adapter, using the same global selection and advancing the
clock only after an action transfers. Their wider interaction matrix remains a
milestone gate. Changed cell commands release abandoned Redo constituents across
all sheets only after success; failed/no-op commands preserve them. Legacy new
records invalidate Redo logically immediately; the App frame boundary releases
the obsolete branch, and direct C clients synchronize before history transfer. Display defaults
and identity/high-water metadata remain outside snapshot replay. Order/floor/
branch metadata and history are session state, excluded from snapshot/Save/Open.
The private DUXCEL_FEATURE_WORKBOOK_HISTORY App fixture is compile/link excluded
from production, matching the other feature fixtures.

`src/workbook/lifecycle.c` owns a separate bounded session journal of sheet
commands, merged by process order with the existing worksheet journals. Metadata
replay swaps owned names, positions and visibility. Presence replay moves an
entire Worksheet between the live array and exactly one detached event owner,
including its earlier histories, source ledger, objects and formats. It never
clones a deleted dataset. At most 32 lifecycle events can retain detached sheets
in addition to the 64 live-sheet bound; duplicate still creates independent
current stores. Eviction releases detached ownership and raises the coordinator's
past barrier. Detached format/ledger/composite-history ownership remains charged
to retained workbook usage; duplicate and restoration preflight their prospective
current/retained budgets without evicting history to admit a change. Branch disposal also releases detached Redo payloads and clears
obsolete per-sheet future constituents. Workbook identity/high-water, command
revision and presentation defaults never rewind. Save snapshots clone live
current state only, excluding the lifecycle journal and detached sheets.
The App captures live Analysis/view before structural changes and invalidates
its result/cache adapter when activating the resulting sheet. This does not
implement formula/reference restoration or durable lifecycle replay.

`src/workbook/journal.c` is the staged native DuckDB storage boundary. One
connection owns a versioned private journal tied to a document ID and immutable
checkpoint revision. Cell records carry operation ID, exact uint64 revision and
targets, and bounded before/after BLOB spans, avoiding UTF-8/numeric conversion.
Append validates an ordered 256-entry/4 MiB batch before BEGIN, compares complete
payloads for operation-ID retries and commits all new records plus the durable
head together. Cancel/conflict rolls back; an engine/commit failure poisons the
handle, requiring reopen and same-ID retry to resolve uncertain durability.
Only a successful COMMIT publishes a receipt. Failed initial creation releases
its own new incomplete database/WAL after closing handles; a pre-existing path
is never deleted by failed open. This distinguishes safe retry from erasing
unknown or previously committed state. Reads return at most 31 records
per page (under 4 MiB worst-case text); borrowed spans end at the visitor return.
The caller must match the actual checkpoint and validate before-values when
staging replay. The native App now admits manual-cell commands through the checkpoint/session
coordinator below. This primitive alone does not establish recovery UI, retained
Undo or complete Save fencing.
Its private file format is independent of native workbook v15.

The native journal accepts v1 unbound archives and v2 immutable origins. Binding
an empty journal transactionally stores generation/parent IDs, checkpoint byte
length and SHA-256, and an owned bounded original-workbook locator. It cannot
replace an existing origin or infer one for a nonempty legacy tail. Metadata
reads bound row count and text/blob sizes before native allocation; path bytes
are read as BLOB to reject embedded NUL instead of silently truncating. Recovery
must still hash the actual checkpoint and compare the complete origin before
replay. The session now supplies the actual closed-checkpoint fingerprint and immutable
parent/generation identity before opening the write lane on native macOS.
Missing/conflicting origin or engine failure retains pending records for exact
retry; no failed metadata binding can acknowledge an edit. Startup choice/adoption remains W10c; generation ownership uses the lease below.

`src/workbook/journal_queue.c` is the staged native asynchronous owner. The
producer reserves a bounded immutable record before model mutation and either
cancels it on rejection/no-op or publishes after success. The worker never reads live
Workbook/App pointers; it owns a private DuckDB connection and borrows only the
queue's immutable in-flight prefix. All reserved/pending/in-flight data remains
charged until acknowledged. A failure racing a reservation cannot make its later
publication fail or release its bytes. Failed records remain for explicit
same-ID retry; uncertain commits reopen and resolve through the storage API.
Accepted and durable revisions stay separate. Stop joins only this owned thread,
with explicit drain/abort semantics and reported non-durable work. Checkpoint setup owns an independent Workbook snapshot until the writer has
published its `.dxls` baseline. Failed preparation retains it for retry; setup
release runs only after joining. Nonblocking stop allows the producer to retire
an old generation without waiting inside ordinary frames. No browser journal
adapter is implemented.

`src/workbook/schema.c` owns the allocation-free current-package column proof.
It consumes bounded borrowed table/kind/column/type byte spans, with exact table
inventory and ASCII-folded column identity independent of physical order. Known JSON text aliases retain
the previous C-API VARCHAR compatibility. Invalid
or duplicate fields poison the proof. Native complete-package validation obtains
those fields in one bounded catalog snapshot, preserving embedded NUL and
rejecting oversized names/types. Required row NULL/text checks and owner/ledger
validation stay separate. Historical lenient column/default behavior remains in
the legacy reader; current v15 stays strict. The proof adds no runtime dependency
and compiles with the shared C model.

`src/workbook/journal_session.c` connects native one-cell admission to complete
checkpoint lineage. It creates a private generation only for a valid changed
command, snapshots current owned state, reserves immutable before/after bytes,
applies through the existing producer and publishes only on success. Checkpoint
Save, closed-file fingerprinting and immutable journal-origin binding run on the
queue owner before readiness or any edit COMMIT can be acknowledged. Native
macOS uses the existing fixed-buffer SHA-256 adapter; a leftover baseline WAL
rejects preparation for Retry. Other native hosts retain v1 unbound backups until
their digest boundary is implemented. No App/Page/renderer pointer crosses that boundary. The App captures
its independent live Analysis/view first; source protection remains authoritative.

A bounded scalar/identity/store/action fence tracks the admitted prefix, including
object extents and source-ledger publication. Unadmitted changes/Undo/Redo mark it
uncovered. The next changed cell stages a fresh full current checkpoint; at most
current/retiring generations coexist, and old files/owned bytes retire only when
the replacement is complete and the old writer has stopped. This fence covers
registered mutation paths, not arbitrary writes into model pointers or a content
hash proof. Display defaults/view navigation do not become data edits. Retained
checkpoint format bytes join the App's existing additional-format budget.

Native production generations live under the platform user-data Recovery root
(macOS Application Support/Duxcel/Recovery); automated runners override a private
root under their ignored run directory. Cryptographic directory/operation IDs
separate instances and generations. Normal Open/Save/source entry points reject the private recovery root, active working paths and
aliases, protecting named artifacts from later cleanup. Only fixed files in
current-session owned directories are discarded, never an external source/named workbook or unknown
recovery material. Explicit Don't Save discards the current session; a successful current-state Save
and clean file replacement/close dispose covered working copies. Unexpected dirty shutdown
retains committed files. Catalog/candidate preparation and guarded startup/File
adoption are implemented; broad foundation acceptance remains W10c3, and session
history is not persisted until W12. Linked-source availability remains
the existing file-mode policy.

`src/platform/file_lease.c` owns native OS-backed generation leases. The producer
claims the fixed private zero-byte `lease.lock` before starting its writer and
releases it only after joining. Kept generations retain the inode for a later
exclusive recovery claim; acquisition/release never truncate/unlink it. Explicit
discard removes known owned artifacts while still holding the lease. POSIX uses
nonblocking flock with close-on-exec/no-follow; Windows has an exclusive native
handle boundary. Only macOS execution is validated. Recovery discovery must
claim a generation before inspecting/adopting it and skip active leases. The
module is excluded from the shared-C browser source inventory.

`workbook_journal_open_readonly` opens an existing private archive with DuckDB
READ_ONLY. It never initializes an empty/missing archive, and its handle rejects
bind/append while retaining read/origin access. Actual v2 pre/post-COMMIT process
kills establish read-only WAL recovery without changing the DB/WAL bytes or file
set on the validated engine. A recovery controller must still hold its generation
lease and verify the baseline digest before replay into an isolated candidate.

`src/workbook/recovery.c` prepares an isolated native candidate from a direct
private generation under a resolved root, holding its existing lease. Metadata
inspection validates inventory/schema/origin without scanning the tail; that
handle refuses tail reads. Full replay reopens with the matched document/base
and validates the complete sequence before streaming bounded records. The
controller verifies actual closed-baseline size/SHA-256, rejects baseline WALs
and file/directory aliases, loads through the existing codec and checks every
before-image/worksheet/target before projection. No external source query runs.

Candidate projection uses local CellStore state and immediately retires transient
Undo snapshots. It never allocates live draft identities or process-wide history
order, so another model can accept commands while the worker prepares a candidate.
It does not reconstruct durable Undo (W12). Progress/cancel covers hashing,
loading and per-record replay; cancellation/failure releases only owned memory
and its lease, never the current workbook or recovery files. The returned model
is dirty with no Save path; the original locator is informational. Checkpoint
inspection explicitly marks deferred/unknown or unbound tails and cannot silently
resume them. A take operation transfers only the model; the caller retains the
candidate's lease/info until its lineage choice is resolved. The existing digest
adapter currently makes this preparation available on native macOS. `recovery_catalog.c` now streams direct generation rows in 1–128-entry steps.
It acquires a brief metadata lease, skips active owners, offers bound/unbound/
checkpoint-only/incomplete/invalid/older entries and releases all borrowed row
storage after its visitor returns. Pause supports bounded pages; cancellation
retains the cursor; enumeration failure requires a fresh scan. There is no
recursion, full-root filename inventory, pruning, sorting or mtime-based branch
inference. The POSIX cursor/no-follow metadata boundary is in
`src/platform/directory.c`; Windows iteration currently reports unavailable.
Missing roots are completed empty scans without filesystem creation.

`recovery_worker.c` owns a native reading thread with one active request or
unpolled reply, never latest-wins disposal of a candidate. It copies at most
32 rows into a 4 MiB reply, retaining scan continuation only on that thread.
Cancellation releases partial pages/candidates and restarts a later scan;
publication competes with cancellation under the mutex. Retirement occurs
outside that mutex before reservations are released. Preparation reserves the
loader's maximum format ownership; ready state exposes actual format/row bytes,
not total RSS. Poll transfers those owners to the App; replies/candidates may
outlive the worker. Shutdown cancels/joins and releases only worker-owned data,
without deleting recovery files. No App/live-model pointers cross this seam.
Explicit candidate retirement is separate from free/cancellation. It requires
full bound replay, unchanged known file identities and no additional directory
entries. Deferred/unknown/unbound material is preserved. The lease stays held
through fixed-file cleanup; partial filesystem cleanup failures never invalidate
a completed named Save. A newly edited recovered branch seeds a copied parent
and informational original locator before its first checkpoint, validating the
document identity without changing its Save destination. Noncreating recovery
location lookup allows startup scans without allocating a recovery directory.
`src/app/recovery.inc` now owns request/presentation epochs, bounded page replies,
staged candidates and the adopted parent lease. Startup scan uses noncreating
location lookup and offers choices only over its untouched initial document;
File → Recover session opens the same controller manually. The borrowed
`src/ui/recoveryview.c` panel owns selection/focus/geometry, with no filesystem,
model or lease authority. Current dirty documents pass through the existing
document guard before candidate adoption. Dismissed/late results are freed
without deleting files. Format preflight counts worker/staged ownership until
release. Recovered documents stay dirty/pathless; full clean Save or explicit
discard may retire the parent, while checkpoint forks always preserve deferred
tails. Ordinary Open/Save/source entry refuses the private recovery root.
Crash/restart and broad foundation regression remain W10c3.

Native `workbook_save_controlled` (`src/workbook/save.h`) borrows an immutable
snapshot and a synchronous, bounded saving-thread callback. Progress counts
actual sheet/column/cell writes and bounded metadata stages. Returning false
before COMMIT yields a distinct cancelled result and transactional rollback;
post-COMMIT completion cannot be revoked. The original `workbook_save` uses
empty control. Native Save now reserves the worker lane through reply polling; replacement
read jobs are rejected and generic cancellation cooperatively requests Save
cancellation without changing its request ID. Mutex-owned progress closes the
cancellation gate before COMMIT, preserving its completion even if a later read
or cancellation arrives. The App renders a 360×136 logical-unit progress panel with Escape/button
cancellation and a non-cancellable publication phase. The panel owns its pointer
area; the grid retains scrolling outside it with structural edits locked.
The Save reply retains its immutable snapshot until UI publication, included in
the existing IO staging reservation. `save_fence.c` compares current registered
content, exact sparse values, source/format/object clocks, definitions and view
state with that snapshot, in addition to chronological action order. Failed or
cancelled Save does not adopt a path or clear dirty/recovery state; an older
successful Save cannot close a newer dirty document. Snapshot ownership ends
in `reply_free`; no App pointers cross the worker boundary.

The cell-command GUI fixture has its own App object compiled with
`DUXCEL_FEATURE_CELL_COMMAND`; its private fixture lives in `tests/support/`.
Normal/review/browser App objects omit the hook, state and fixture dependency.
Pure command/model tests have incremental per-file optimized/sanitizer objects,
using the existing shared-C browser-conditioned model without a GUI or DuckDB
engine. This keeps their cache/configuration separate from the production App.

`src/app/build_info.c` owns immutable compiled provenance with a source-archive
fallback. The build generates metadata into one object only; browser cache input
signatures include the generated metadata while other object flags stay stable.
`src/platform/macos_about.m` owns the scrollable native About panel and runtime
TEST icon/Dock badge, preserving raylib's OpenGL context and restoring session
resources. No Git call, test capture bridge or external service runs in product
frames. `tools/test_catalog.py` owns local test plans and deferred interactions;
[TESTING.md](TESTING.md) owns execution policy and acceptance limits.

## Review isolation

Normal binaries omit review modules, command strings and browser bridges at compile/link/package time. Review binaries enumerate bounded stable widget IDs during an armed capture, retain an owned same-frame image/state and publish immutable reports. External Python tools validate/store/queue them and optionally dispatch an agent; product frames do not poll a service or require Python/Node. Browser review uses the same C registry with development-only capture/upload/IndexedDB transport.

## Generated output

`build/bin/` holds native binaries and an executable-relative asset link; `build/obj/` separates release/sanitizer/review objects; `build/tests/` holds test programs, generated databases and reports. `data/downloads/` is optional public-data cache. `.cache/web/` holds installed SDK/dependencies; `build/web/` holds disposable Wasm packages; `build/locks/` holds disposable operation locks; a checkout-specific OS-temp shared/exclusive gate prevents cleanup from replacing a lock while it is in use. No generated executable belongs at root or next to test source.

## Known structural debt

`src/app/app.c` is about 15,500 lines / 1.22 MB; native scenario code shares this translation unit. `src/dev/review_app.inc` is about 4,200 lines / 441 KB. These sizes impede focused compilation and ownership. Extract cohesive controllers and test-only harnesses incrementally, preserving behavior and keeping a single integration owner. Do not undertake a broad rewrite solely to change GitHub language percentages.

Synthetic fixture generation uses Python plus the existing DuckDB CLI and a finite SQL input; native tests separately verify product load/PIVOT/save/reload behavior. Most behavioral assertions already are C. External orchestration remains Python with completion, sanitizer, timeout, binary-fingerprint, fixture-order and desktop-lock contracts. Product-linked helpers remain C under `tests/support/`; tool/browser test sources live in their subsystem test directories.

## Operational walkthrough and assessment

This section explains the current source paths, missing product contracts and architectural proposals for discussion. **Current-behavior sections describe implementation; proposal sections are neither implemented nor accepted decisions.** Preserve C99/raylib/DuckDB and the shared C/Wasm boundaries. Duxcel has a real analytical query engine and sparse text cells, but these do not yet form a shared, typed, recoverable editing model comparable to Excel.

For future workbook behavior, the 21 October user decisions in [the workbook TRD](design/WORKBOOK_MODEL_TRD.md#19-decision-register) take precedence over the earlier proposals below: DuckDB journaling without a new SQLite dependency, explicit Connect/Import/Data Model boundaries, complete saved analytical outputs, move-following A1 references, separate grid occupancy/floating/Master View composition and three-version-state Undo retention. [REQUIREMENTS](REQUIREMENTS.md#workbook-design-direction) summarizes this accepted direction. These are design decisions; the current implementation walkthrough and STATUS do not gain those capabilities from this revision.

### Current layers and main flow

```mermaid
flowchart TD
    Input["Keyboard, mouse, macOS menus"] --> App["AppSession / App<br/>commands, focus, scheduling, publication"]
    App --> Manual["Manual cell entry<br/>CellStore: row, column, text"]
    App --> Definitions["Analysis / SheetObject<br/>source, filter, sort, pivot, chart"]
    Manual --> Book["Workbook / Worksheet<br/>cells, formats, objects, view"]
    Definitions --> Book
    Definitions --> Job["QueryJob<br/>owned definition copy and request ID"]
    Job --> Worker["QueryWorker<br/>one active job, latest read request"]
    Worker --> SQL["C SQL generators + DuckDB<br/>CSV / Parquet / read-only DuckDB"]
    SQL --> Reply["QueryReply / Page<br/>types, NULL, exact text, numeric drawing value"]
    Reply --> Publish["App: check identity and revision<br/>adopt matching result ownership"]
    Publish --> Cache["Page + spare + ObjectView<br/>disposable result caches"]
    Cache --> Draw["Grid / UI / raylib<br/>visible cells, glyph atlases, GPU"]
    Manual --> Draw
    Book --> Draw
    Book --> Snapshot["Explicit Save command<br/>workbook_snapshot"]
    Snapshot --> Save["JOB_SAVE / workbook_save<br/>DuckDB transaction"]
    Save --> File[".dxls<br/>document definitions and manual cells"]
```

`main.c` drives the native `app_run` loop; the browser host schedules the same `app_start/app_frame/app_finish` lifecycle. `AppSession` owns one raylib window/context session. Native `QueryWorker` uses its own DuckDB connection on a separate thread. The UI does not execute queries on that connection; it sends cancellation through `duckdb_interrupt` under the lock.

App currently contains persistent document state, transient views and job orchestration. The active primary source's live `Analysis` resides in `App.analysis`; `Worksheet.analysis` also holds a saved/sheet copy updated by `capture_sheet`. Table sources reside in `SheetObject.source`. Coordinating several ownership and revision types inside App makes it harder for new features to reach all affected layers.

The browser runs the same C App/grid/UI as Wasm. C-generated SQL passes through the thin JS adapter to the official asynchronous DuckDB-Wasm Worker. Arrow output is copied into C Pages through bounded wire transport, preserving type/null information. DuckDB handles and C pointers are not jointly owned with the Worker. Native in-place queries and browser file lifecycles differ: the current browser chooser reads the entire file, limited to 256 MiB, and registers a session buffer. This registration is not a persistent workbook/source reference. Sources: [worker_browser.inc](../src/data/worker_browser.inc), [engine_host.mjs](../web/engine_host.mjs), [official_worker.mjs](../web/official_worker.mjs).

Entry points: [main.c](../src/main.c), [App model](../src/app/app.c), [worker.c](../src/data/worker.c), [workbook.h](../src/workbook/workbook.h).

### What happens when a user enters a cell

1. `TextInput` holds uncommitted text while the user types. Enter, Tab or a relevant focus change reaches `commit_cell`.
2. `manual_range` prevents overlap with the imported primary data area or an object. Existing source cells cannot be edited through this path.
3. `cells_set` stores text in a sparse array sorted by `(row, col)`. Every populated cell owns its text allocation; lookup uses binary search. The virtual canvas does not allocate a dense cell matrix.
4. The previous cell-store state is copied into an undo snapshot; `worksheet_record(EDIT_CELLS)` adds the chronological action and the document becomes dirty.
5. `draw_blank_cells` reads visible text from this store. Typing does not send a DuckDB cell UPDATE for every keystroke.

This store is **text-based**. Entering `123` does not create a numeric value; `=SUM(A1:A3)` has no parser, dependency graph or calculation execution. Manual values entering mixed-selection statistics are also carried as VARCHAR. Applying a number format does not convert text into a numeric type.

Current limits are 20,000 editable cells and 4 MiB of text per sheet, with 64 KiB per value. Calling `cells_set` with empty text removes the cell record; the getter returns empty text for missing cells. This entry path is therefore not a general typed editor for SQL NULL/empty VARCHAR/literal `NULL`. Imported Page values preserve these distinctions separately; the two paths do not have identical semantics.

Sources: [cells.c](../src/workbook/cells.c), [cells.h](../src/workbook/cells.h), `commit_cell`, `draw_blank_cells` and `statistics_snapshot` in [app.c](../src/app/app.c).

Manual range moves preflight the complete model through `worksheet_check_move`, returning ready, invalid/protected or needs explicit replacement. The default `worksheet_move_range` still rejects occupied destinations. After UI confirmation, `worksheet_replace_range` revalidates read-only/object boundaries and admits only manual ranges. It prepares formatting before changing cells, clears destination values under source holes and translates the original sparse cells once, so self-overlap cannot read partially moved values. Existing composite worksheet history records cells/formats as one action. The transient C dialog captures the sheet index, selection and data/format/Analysis revisions; it refuses stale approval and never owns a second copy of the document. This legacy range adapter does not yet use the W05 stable target envelope; general formula rewrites and durable journal commands remain separate.

### How query results live in memory

| Area | Contents | Lifetime and limits |
| --- | --- | --- |
| Workbook / Worksheet | Cell text, source/operation definitions, objects, formats, views and source-identity metadata | Authoritative RAM state; explicit Save writes it to `.dxls` |
| DuckDB query database | Prepared queries, operator memory and native pivot result/totals temporary tables | Native session in-memory database; spills to a private temporary directory as necessary; configured for 512 MB and two threads |
| C Page | Column schema and row-major `Cell[]`; each value has `is_null`, exact `text` and `double number` for drawing | Ordinary pages have at most 256 rows; primary pages have a 16 MiB bound and Table/Pivot pages a 4 MiB bound |
| App result caches | Primary `page`, `spare`, per-object `ObjectView.page` and separate step preview | Disposable and re-queryable; excluded from workbook snapshots |
| Undo / Redo | Cell, Analysis, object and format snapshots plus their matching action sequence | Session memory; not a general durable command journal or saved user version history |
| GPU / UI | Glyph atlases, render batches, transition and popup textures | Not authoritative calculation/data state; paint buffers have a combined 32 MiB budget excluding fonts and driver costs |

`Page.cells[(row - offset) * column_count + column]` addresses only the loaded window. `Page.total` is the result's total row count, or a lower bound until counting completes; it does not allocate `total` cells. `spare` can avoid re-querying a recent previous window. The primary viewport requests a new window after a short debounce when cached coverage is insufficient.

Native `duckdb_result` is released after conversion into a C Page. The Page carries the callback needed to free text using the correct allocator. The `QueryReply` → App transition transfers pointer ownership and zeroes the former owner. Rendering reads the accepted Page. Text is the exact representation of large integer/DECIMAL values; `double` supports chart coordinates or appropriate approximate display. Never reconstruct exact copied values from the drawing double.

**A 256-row output does not mean the query processes only 256 rows.** Sort, DISTINCT, aggregates and COUNT can process much larger data. Native pivots materialize into temporary `_duxcel_pivot_result`; the UI reads pages from it. Primary-page SQL selects all business columns, not just horizontally visible columns. Deep `LIMIT/OFFSET` navigation can be costly. DuckDB's 512 MB setting is not a hard limit on combined C/JS/GPU/process memory. [DuckDB memory management](https://duckdb.org/2024/07/09/memory-management).

Charts also have bounded scope: current ranking operates on loaded pivot groups. Source-derived non-additive “Other” support covers specific combinations, not a universal global top-N guarantee over all data.

Sources: [Page model and cleanup](../src/data/page.h), [page.c](../src/data/page.c), `result_page_visible_status`, `fetch_window`, `pivot_materialize_proved` and statistics queries in [database.c](../src/data/database.c), [chartview.c](../src/ui/chartview.c).

### Rendering and user interaction

Each frame receives platform/UI input, publishes worker replies, processes commands/grid geometry/visible content, sends raylib drawing calls to the GPU, and schedules subsequent page/pivot/count/statistics jobs. There is no direct DuckDB query in the cell drawing loop. However, some `draw_*` functions also process input, focus and command side effects; presentation and behavior are not strictly separated throughout App.

The grid calculates visible rectangles from `top/fraction/horizontal`, selection `active/anchor/extent`, column widths and object coordinates. It draws visible cells and intersecting objects. Frozen columns use a separate clipping plane. Application scale, sheet zoom and physical framebuffer/DPI are separate concepts. UI metrics, glyph caches and bounded framebuffer caches support drawing; the screen buffer does not store the document.

The `Command+Shift+Down` example exposes a specific gap. `grid.c:navigate` sends shortcut-arrow navigation to the edge of `Grid.navigation` when present, otherwise to the primary `Page.total` boundary. `set_grid_navigation` provides ready Table/Pivot bounds, not the occupied/empty sequence in manual `CellStore` data. `cells_region` serves the separate Select All command. There is no shared “find next data boundary” contract. This is a source-level model gap, not evidence of a physical keyboard-event test. Excel Mac documents extending selection to a populated data boundary with Shift+Cmd+Arrow. [Microsoft shortcut reference](https://support.microsoft.com/en-us/accessibility/excel/keyboard-shortcuts-in-excel).

W03's `cells_containing_region` reads the canonical sparse store with a bounded component queue and binary neighbor lookup: O(n log n) work and O(n) temporary memory, independent of worksheet area. Original finite selection bounds seed four-neighbor components once; bounding-box growth cannot recursively swallow disconnected islands in one command. Blank interiors use deterministic enclosing bounds. `select_region` combines this candidate with current explicit owner/used bounds, and `grid_select_next_region` preserves active cell/direction while choosing a strict superset. Cells, formats, history, cached Pages and query ownership remain borrowed. This is Select All region inference; it does not implement C01 Cmd+Arrow occupancy navigation or infer permanent Tables.

Sources: [grid.c](../src/grid/grid.c), [sparse cells](../src/workbook/cells.c), `set_grid_navigation`, `select_region`, `shortcuts` and `draw_app` in [app.c](../src/app/app.c), [UI input/font management](../src/ui/ui.c), [paintlayer backend](../src/ui/paintlayer_backend.c), [macOS adapter](../src/platform/macos.m).

### Save, Close, recovery and versioning are distinct

| Need | Current behavior |
| --- | --- |
| Explicit Save / Save As | A separately owned snapshot commits transactionally with real progress/cooperative cancellation; publication is fenced against newer content and action order. |
| Failed Open / Save | Load builds a temporary candidate Workbook and replaces the current one only after successful validation. Save uses transactions/rollback. This is not autosave. |
| Normal native Close / Quit | `request_document_action` commits an open cell edit and offers Save / Don't Save / Cancel for dirty documents. Pending Quit waits for successful Save. |
| Browser workbook Open / Save | `workbook_browser.c` explicitly returns unavailable. Shared metadata-reader tests do not establish the integrated feature. Browser tab closing also has host constraints. |
| Local recovery / startup restoration | Manual cells have checkpoint-backed transactional journaling and native isolated candidate preparation. Startup/File recovery choice and guarded adoption are implemented; process crash/restart acceptance remains W10c3. Named-file autosave is separate. |
| Undo and document version history | Undo/Redo are bounded RAM snapshots. `workbook_snapshot` excludes them and Pages; reopening does not restore the previous session's undo history. |
| External source data | CSV/Parquet/DuckDB contents are not automatically embedded in the workbook. Source references and supported identity proofs are saved; moving/changing a file requires relinking or reverification. |

`.dxls` schema versions 1–15 represent file-format compatibility. The `SourceVersions` ledger binds source/owner/reader/schema/content proofs; it does not retain old Parquet bytes for time travel. Displayed row position is not durable source-record identity. Supported raw CSV/Parquet paths can carry original record ordinals; universal lineage is absent for custom SQL, DISTINCT, aggregates and the browser. Extending this formatting foundation into an editing model needs separate design and validation.

The user confirmed the compact dirty guard and Command-Q on the current Mac. Scoped native tests cover Save/cancel/failure and pending close with newer edits. Startup recovery now offers verified committed manual-cell tails or a checkpoint copy; persistent Undo and broader command journal coverage remain separate.

Sources: `capture_sheet`, `workspace_job`, `request_document_action`, `draw_unsaved`, `poll_query` and `app_start` in [app.c](../src/app/app.c), [workbook_save/load](../src/workbook/workbook.c), [browser unavailable adapter](../src/workbook/workbook_browser.c), [source_scope.h](../src/workbook/source_scope.h), [browser persistence plan](design/BROWSER_PERSISTENCE.md).

### Proposal: shared document and command contracts

A suitable target is a modular native C application with explicit boundaries. Merely shrinking source files does not fix disconnected behavior; define how every user operation changes the shared document model.

```mermaid
flowchart TD
    UI["Grid, ribbon, menus, accessibility<br/>input and presentation"] --> Intent["Shared command API<br/>SetCell, Paste, MoveRange, ChangeFilter"]
    Intent --> Commit["Document transaction<br/>validate, apply atomically, create revision"]
    Commit --> Model["Workbook domain<br/>typed cells, sources, overlay, objects"]
    Commit --> History["Undo / Redo<br/>before/after values in one transaction"]
    Commit --> Recovery["Recovery writer<br/>ordered durable records and snapshots"]
    Model --> Schedule["Dependencies and invalidation<br/>which results need recalculation"]
    Schedule --> Formula["Cell formula engine<br/>parser, references, dependency graph"]
    Schedule --> Relational["Analytical engine<br/>C definitions to DuckDB SQL"]
    Formula --> Results["Revision-bound results<br/>Page / typed cell result"]
    Relational --> Results
    Results --> UI
    Recovery --> Files["Recovery session + explicit Save<br/>versioned workbook codec"]
```

The Formula branch is a new capability. It shares types, document revisions and dependency boundaries with SQL analytics; each cell keystroke need not become a SQL query. Microsoft's public calculation description also uses a dependency tree, calculation order and recalculation of dirty dependents. This is a formula-engine design reference, not a claim to know all Excel internals. [Microsoft recalculation documentation](https://learn.microsoft.com/en-us/office/client-developer/excel/excel-recalculation).

Proposed shared contracts:

- **One document authority:** Define persistent-model and active-view-copy roles. Avoid having UI code mutate scattered stores and manually complete dirty/undo/query-invalidation chains.
- **Target identity:** Use sheet/cell addresses for manual cells; dataset, immutable base version, original record and field identity for data edits; additionally definition/result revisions for result cells. Define reference movement during row insertion/deletion.
- **Typed value and expression:** Preserve sparse absence, SQL NULL, empty text, literal `NULL`, integer/DECIMAL, dates, errors and formula input. Separate raw input/expression, computed value and formatted display. This does not require a synthetic “blank” storage type.
- **One change result:** Return what changed and which results were invalidated. Multi-cell operations such as Paste are one undo action and one durable transaction. Selection/scroll alone must not dirty data.
- **Scheduling appropriate to job type:** Preserve cancellation/latest-wins for viewport reads. Durable writes complete in order; viewport scrolling must not drop a commit. This is where priorities on the current single worker need separation; adding threads alone does not solve it.
- **Revision-bound publication:** Asynchronous Save reports the revision persisted. Newer edits keep the document dirty. Establish this contract before removing the current broad IO input lock and enabling background autosave.

Existing `worksheet_*` atomic range/history helpers, owned snapshots, source identity and stale-reply protection can support this transition. A rewrite is unnecessary. Extract document lifecycle, command/history, query scheduling/publication and grid-interaction controllers from `app.c` while preserving tests. Move native scenarios behind a test-build boundary; development infrastructure must not become production behavior.

### Proposal: editing large Parquet datasets

Changing text inside a `Page` only changes the display. Correct editing changes the logical relation read by queries. A suitable first slice keeps the base Parquet immutable and stores changes in a local overlay.

```mermaid
flowchart LR
    Base["Immutable source snapshot<br/>Parquet or managed data"] --> Effective["Effective relation<br/>base + typed patch + insert - delete"]
    Patches["Edit overlay<br/>original record and field identity"] --> Effective
    Effective --> Transform["Source query / filter / sort<br/>pivot / statistics"]
    Transform --> Consumers["Viewport, copy, chart, export<br/>same data revision"]
```

If displayed row seven corresponds to original record 8,431 after sorting, an edit cannot target “row seven.” Its identity must be `(dataset, base_version, record_id=8431, field_id=amount)`. Initial support can be limited to direct source records and individually traceable fields. Do not enable aggregate/pivot-total editing without defining which source records change and how.

Keep a bounded current-patch index in RAM. A durable store holds typed patches, inserts/deletes and transaction revisions. Do not materialize the whole source into a C cell array. As overlays grow, measure managed DuckDB tables versus materialization/compaction into new Parquet snapshots. An overlay is a proposal for sparse edits; the same plan is not assumed to remain fast for millions of changes.

Apply the overlay to the effective relation **before filtering, sorting and aggregation**. An excluded row can become visible after an edit; a visible row can leave the filter. Patching only the loaded 256 rows misses these effects. SQL `COALESCE(patch, base)` is also incorrect edit semantics: an explicitly NULL patch would restore the old value. Patch presence and patch-value nullness must be separate information.

Initial editing acceptance: change the correct record under filter/sort; viewport, full-selection statistics, pivot, copy and export observe the same revision; Undo/Redo restores old values; Save/Open and crash recovery retain the edit; external source changes never silently apply patches to other records. A snapshot content hash can verify identity but does not retain the old source. Durable history also needs access to old source bytes.

### Do we need DuckLake or Delta Lake?

| Approach | Capability | Assessment for this project |
| --- | --- | --- |
| Existing DuckDB + typed local overlay | Controlled record editing within C; extend the workbook format and SQL engine | Fewest new components for the first editing slice. Identity, query plans, durable journals and compaction still need design. |
| Managed DuckDB table | Application-owned imported data with real UPDATE/DELETE | Candidate for frequent edits and independent file sharing; measure import time, duplicate disk space and snapshot/history policy. |
| DuckLake | Data snapshots, time travel and small changes stored in metadata; catalog plus data files | Research candidate for large managed datasets and future shared data. Prove extensions, packaging and file compatibility against pinned native/browser engines. |
| Delta Lake / Iceberg | File-based transactions/snapshots and row-level change mechanisms | Consider for external lakehouse interoperability. These do not provide cell/formula/focus/undo contracts automatically. |

DuckLake provides [data inlining](https://ducklake.select/docs/stable/duckdb/advanced_features/data_inlining), [time travel](https://ducklake.select/docs/stable/duckdb/usage/time_travel) and [catalog choices](https://ducklake.select/docs/stable/duckdb/usage/choosing_a_catalog_database). Delta's [deletion vectors](https://docs.delta.io/delta-deletion-vectors/) also offer an alternative to rewriting an entire Parquet file for each small edit. **The resulting proposal** is to keep the storage provider replaceable behind the document/command layer, without making lakehouse integration a prerequisite for basic grid behavior. Multi-user data writing and single-user workbook editing are different scopes.

### Proposal: autosave and crash recovery

Distinguish three promises: the UI accepted the edit; the edit was durably written to recovery storage; the named workbook file was saved. One `dirty` boolean cannot express all three. Track document identity, current revision, recoverable revision and saved revision.

The current manual-cell seam already prepares owned checkpoints and journals ordered commands through DuckDB with pending/durable feedback. There is no artificial debounce; measured bounded natural batching and post-COMMIT receipts define the implemented contract above. General command families, startup choice/adoption, retained history and compaction remain later roadmap work.

Recovery records are not caches and must not live under `build/` or temporary query-spill directories. Store them in the platform's durable user-data location under document/session identities. Startup reads a complete snapshot plus committed continuation records, handles missing/truncated tails safely and resolves source access separately. Acceptance includes disk full, permission failures, writer shutdown and preventing old recovery data from being applied to the wrong workbook. Define how explicit Don't Save retires that session's recovery records.

Named-file autosave updates the chosen workbook automatically; recovery protects work even without explicit Save. VS Code exposes Auto Save separately from Hot Exit for unsaved changes. Office also distinguishes AutoSave and AutoRecover. The proposal drawn from these products is to separate untitled/crash protection from explicit workbook persistence. [VS Code](https://code.visualstudio.com/docs/editing/codebasics), [Microsoft AutoSave](https://support.microsoft.com/en-us/office/collab-files/what-is-autosave?ad=us&rs=en-us&ui=en-us).

### Proposal: Excel-like behavior contracts

Excel-like behavior cannot be accepted through one general parity checkbox. Specify starting state, actual input, expected selection/value and side effects for each behavior. The first matrix should include Cmd+Arrow and Shift+Cmd+Arrow at occupied/empty boundaries, active cell/anchor/extent, Enter/Tab, edit commit/cancel, paste/clear, range move, insert/delete, Undo/Redo and save/close.

A shared navigation service should resolve targets from the model for sparse manual cells, sources and objects. Use the existing occupied-cell index rather than scanning the entire sparse canvas. SQL-backed data can use a bounded boundary query tied to the same source revision when necessary. A 256-row viewport cache cannot represent whole-sheet occupancy. Explicitly decide and verify Excel Mac occupancy rules for SQL NULL, empty strings, formatting-only cells and future formulas returning empty text.

Acceptance covers the affected layers together. Navigation changes need correct target/selection and no unintended dirty/query effects. Data edits also need engine results, undo and persistence. Pure shadow/spacing changes do not require a full persistence regression every time. A unit pass does not replace real event delivery or integrated data correctness.

### Current feedback infrastructure and batch proposal

Native review atomically writes report bundles into a local outbox without requiring a running Python service or agent. Browser review first stores reports in IndexedDB and then attempts shared-outbox delivery; local-pending does not establish shared durable delivery. The external Python queue uses SQLite transactions, leases and retries. The optional worker performs read-only triage, not automatic fixes. Normal product binaries exclude review infrastructure.

```mermaid
flowchart LR
    Capture["User reports<br/>image + note + target + build"] --> Outbox["Durable outbox<br/>collects while agent is stopped"]
    Outbox --> Inbox["Proposal: readable feedback list<br/>group duplicates, assign layer and priority"]
    Inbox --> Batch["Proposal: selected batch<br/>IDs + shared behavior + acceptance"]
    Batch --> Fix["Implementation and data changes<br/>in affected layers"]
    Fix --> Verify["Integrated verification<br/>new build and report IDs"]
    Verify --> Inbox
```

The first missing piece is a **batch workflow** over the existing outbox: collect reports → select a batch → describe shared root cause and acceptance → implement → verify. Keep `triaged`, `implemented`, `verified` and human design acceptance separate. A completed/triaged queue item does not establish that the defect is fixed.

A batch can link report IDs, document/source revisions, expected/observed behavior, layer tags, test scope and fix build. Screenshots alone do not establish data correctness; calculation/persistence reports need small reproducible data or supported workbook fixtures. Existing foundations include reader `list/read/status`, the queue and `worker --once`. Selected multi-report packages, duplicate/root-cause grouping and batch progress views remain missing. Python is external development tooling here; changing its language does not solve the batch workflow.

Sources: [review capture](../src/dev/review_capture.c), [native outbox](../src/dev/review_store.c), [reader](../tools/native_review.py), [queue](../tools/review_queue.py), [worker](../tools/review_worker.py), [service commands](../tools/review_service.py), [review plan](design/NATIVE_REVIEW_PLAN.md).

### Proposed ownership areas and initial order

| Ownership area | Contract | Integrated result to prove |
| --- | --- | --- |
| Grid and interaction | User input to selection/edit commands; hit testing, focus and geometry | Consistent navigation/edit lifecycle for manual and queried data |
| Workbook and commands | Typed values/expressions, target identity, atomic changes, undo/revisions | UI menus and shortcuts perform the same operation |
| Analytics and formula runtime | Effective dataset, lineage, SQL, formula dependencies and result publication | All consumers observe the same revision |
| Persistence and recovery | Versioned codec, durable ordering, snapshots and source references | Accepted work survives Save/Open and crashes |
| Feedback and acceptance | Report/batch/finding/fix/test links with correct build evidence | Each closed report identifies its accepted behavior and tests |

These are logical ownership areas, not a proposal for five simultaneous agents/implementation lanes. Preserve the maximum of two active implementation lanes and one App/shared-interface/build/desktop integrator on this Mac. Compatible initial areas could be grid/manual editing and native persistence/recovery sharing common contracts. Preserve ongoing browser codec/tests without making them a prerequisite for native recovery.

Proposed initial work: (1) isolate closing-related data loss by build and close path, then deliver the first recovery slice including untitled workbooks; (2) fix occupied/empty navigation and basic edit/undo using shared command contracts; (3) connect typed manual cells to the analytical model; (4) complete raw-source record overlays across viewport/pivot/statistics/export/save/recovery. A selected-feedback batch view can be a small parallel external-tool task. General formula dependencies and a lakehouse provider are later capabilities with separate acceptance. Basic grid fixes must not wait for all this infrastructure.

### Review and verification scope

These source paths describe the dirty working tree inspected, not HEAD alone. The product source tree contained 135 files; the review examined module interfaces and App/input → model → worker/SQL → Page → render, plus Save/Open/review paths. Generated brand/icon derivatives were not treated as design authority. This section does not establish native visual or physical-keyboard acceptance; the user's application was not controlled.

Focused checks passed for `grid`, `page`, `cellreadout`, `objects` and `selection_ladder` in release and ASan/UBSan configurations (11.933 s including builds). Native workbook `--formats-only` checks passed in release and ASan/UBSan (17.413 s). Both runs executed zero App scenario stages. Results, source/binary hashes and commands were recorded under ignored `build/tests/results/architecture-review/` and `architecture-persistence/`. Cleanup subsequently pruned these disposable payloads; its current acceptance summary is `build/tests/results/cleanup-summary.json`. Test sources and commands remain. These historical checks do not establish new autosave, general formulas or editable Parquet support.
