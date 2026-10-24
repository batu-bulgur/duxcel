# Native tests and inputs

C/Objective-C `test_*` sources exercise the real native product API, ownership,
engine and renderer. They are compiled only by test targets. Product sources
stay in `src/`; generated programs/databases/reports stay in `build/tests/`.

| Location | Role | Retention |
| --- | --- | --- |
| `test_*.c`, `test_*.inc`, `test_*.m` | Native suites and their implementation fragments | Tracked source |
| `test_fixture.c` | Native load/PIVOT/typed data/save/reload acceptance of canonical/large inputs | Tracked C test |
| `test_journal_session.c` | Native checkpoint plus exact cell-tail replay, lazy no-op, protected/rejected admission, rotation after other edits, IDs/styles/history branching and root failure/discard | Private optimized/sanitizer engine runs; no App/desktop |
| `test_journal_session_gui.c`, `support/journal_session_app.inc` | 18-stage real editor/backpressure/retry/checkpoint/Save R+1/reopen/discard workflow | Hidden TEST App; private compile-time fixture excluded from production |
| `test_journal_queue.c` | Real native worker reservation/publication races, owned bytes, count/byte limits, batching/order, retry, lineage and drain/abort | Serial optimized/sanitizer engine targets; private journals; no App/desktop |
| `test_workbook_journal.c` | Private DuckDB lineage, atomic batches, BLOB/uint64 round-trip, exact retry, rollback/conflict/cancel, bounds, version/gap rejection and initial timing | Optimized/sanitizer engine runs serially; no App/canonical fixture |
| `support/journal_fault.c`, `../tools/test_journal_faults.py` | Targeted write/short-write/fsync faults, failed creation and exact queue repair/retry; real file-size-limit failure | Test-only interposer, never an App object/asset; private journal/WAL paths only |
| `../tools/test_journal_process.py` | Kills the API writer at pre/post-COMMIT barriers and verifies exact old/new payloads and repeated idempotent retries | Each already-built variant; private temp journals; no desktop |
| `test_workbook_lifecycle.c` | Sheet command chronology, detached ownership, binding/style restoration, IDs, branches, hidden sheets, stale drafts, limits and eviction | Shared-C optimized/sanitizer; no engine/GUI |
| `test_workbook_lifecycle_gui.c`, `support/workbook_lifecycle_app.inc` | 24-stage menu Delete, Cmd-Z/Shift-Cmd-Z, rename/reorder/duplicate/hide, retained values/styles/objects and Save/Open | Hidden TEST release App; production excludes fixture |
| `test_workbook_history.c` | Shared-C global ordering, branch cleanup, composite styles, stable targets, stale drafts, retention barriers, overflow and atomic rejection | Reuses per-file optimized/sanitizer model objects; no engine/GUI |
| `test_workbook_history_gui.c`, `support/workbook_history_app.inc` | 21-stage real App Cmd-Z/Shift-Cmd-Z across two sheets, hidden owner reveal, no-op/branch behavior, rename/reorder and Save/Open | Hidden TEST release App; production excludes fixture |
| `test_workbook_identity.c` | Shared-C identity validation, allocation/exhaustion, snapshots and admitted command clock | Tracked C tests; reuses per-file model objects |
| `test_workbook_identity_native.c` | Private native v15 round-trip, v1–v14 metadata migration, exact uint64 and malformed-archive atomic rejection | Tracked C test; optimized/sanitizer engine runs are serial |
| `test_workbook_identity_gui.c`, `support/workbook_identity_app.inc` | Nine-stage legacy Open, rename/duplicate/reorder, edit, Save/Open and stale-session workflow | Hidden TEST App; production excludes fixture |
| `support/legacy_workbook.h` | Explicit test-only downgrade of current checkpoints for historical v12–v14 component assertions | Does not generate source data; never linked to product |
| `test_cell_command.c` | Shared-C bounded command validation, session fences, stale/no-op/history ownership, protection, exact text and limits | Tracked C test; per-file optimized/sanitizer objects |
| `test_cell_command_gui.c`, `support/cell_command_app.inc` | Isolated real App editor/Undo/Redo/stale-draft/Save/Open workflow; private compile-time fixture | Tracked C tests; hidden TEST-badged release App |
| `test_selection.c` | Controller rules over borrowed cells/resolved analytical bounds, with model/viewport retention | Tracked C test; optimized/sanitizer targets |
| `test_build_info.c` | Direct C/source-archive metadata fallback without Git or generated headers | Tracked C test; optimized/sanitizer targets |
| `test_about_native.m` | Standalone real AppKit history scrolling, TEST icon/Dock identity and repeated cleanup; consumes canonical `assets/icons/duxcel.icns` | Tracked macOS adapter test; hidden ASan/UBSan GUI |
| `test_cell_regions.c` | Pure shared-C sparse region shapes, blank ties, read ownership, edges and 20,000-cell traversal limit | Tracked C test; optimized/sanitizer targets |
| `fixtures/workbook/transactions.sql` | Finite v12 four-sheet workbook input for the Python generator | Tracked SQL test input |
| `fixtures/*.csv` | Seven tiny input datasets: NULL/text/quoting/sort/chart cases | Tracked authored inputs |
| `fixtures/ui/*.json` | Versioned expected UI geometry/scroll/paint contracts | Tracked finite regression inputs |
| `fixtures/rendering/*.h` | C pixel-array oracles used by native paint tests | Tracked test input; preserve values |
| `../tools/tests/` | Python tests for automation, queues, packaging and cleanup | Tracked tool tests |
| `../web/tests/` | Browser adapter tests, drivers and probe/host support | Tracked browser tests |

A fixture is an input with a named consumer, not a synonym for a previous run.
Session manifests, commands, elapsed-time reports, captures and event dumps are
outputs and never belong in fixtures. They are ignored under `build/tests/`.

`tools/create_workbook.py` generates synthetic CSV, converts it to Parquet with
the existing DuckDB CLI and creates the fixed workbook from the finite SQL input.
There is no C generator or Python package installation. The native test loads
those files, checks real product PIVOT/NULL/empty/literal text/exact DECIMAL, then
saves and reloads the workbook. `make test-fixture` checks optimized and sanitizer
paths; generation and product acceptance are distinct. Dataset basenames stay
stable for the existing native/browser scenarios.

W05b local gate: `make test-feature FEATURE=workbook-identity`. Its pure modules
are identity, cell-command and metadata descriptors, followed by serial private
native archive checks and the nine-stage real App fixture. The CLI's plan lists
wider document/workspace/browser interactions without launching them. Native
source files and both optimized/sanitizer objects are cached per translation unit.
The finite legacy GUI input comes from `tools/create_workbook.py --identity-only`
and `tests/fixtures/workbook/identity-legacy.sql`, without canonical/large datasets.
The completed two-sheet demo is `build/tests/generated/workbook-identity-demo.dxls`.
The legacy component reader intentionally remains limited to v12–v14 and rejects
v15; the identity model is shared C, not proof of browser persistence activation.

W06 local gate: make test-feature FEATURE=workbook-history. It runs only the
history, cell-command and identity modules in optimized/ASan/UBSan builds, then
the private 21-stage real GUI fixture. It does not bootstrap the canonical dataset,
full engine archive matrix, background worker, browser or lifecycle suites.
Their named interactions remain in tools/test_catalog.py for milestone runs.
The saved demo workbook is build/tests/generated/workbook-history-demo.dxls;
like ordinary current saves it retains values/IDs/revision, not session history.
No routine human-acceptance-pending status is required after a local pass.

W07 local gate: `make test-feature FEATURE=workbook-lifecycle`. It reuses the
lifecycle/chronology/cell-command model objects in optimized/ASan/UBSan builds
and a private 24-stage App fixture. The checkpoint is
`build/tests/generated/workbook-lifecycle-demo.dxls`; session-only deleted sheets
are intentionally absent after reopen. The full-App/source/browser interaction
commands stay registered in the feature catalog for milestone runs.

W08b1 local gate: `make test-feature FEATURE=journal-queue`. Only the native queue,
underlying storage API and each already-built binary's kill/retry process gate
run. Commit barriers make in-flight capacity and failure races deterministic;
short bounded waits only observe completion. The queue's private checkpoints
are finite API test outputs, not authored user/source fixtures or recovery UI.

W08b2 local gate: `make test-feature FEATURE=journal-session`. It runs only the
cell-command model, session/queue/storage engine variants, registered process
barriers and the private 18-stage App. The runner supplies a per-run absolute
`DUXCEL_TEST_RECOVERY_ROOT`; native automated launches without a runner fall back
to `build/tests/generated/recovery`, never the production user-data root. The GUI
uses a two-entry queue to exercise ordinary backpressure without 256 UI commits,
then both keyboard and pointer Retry, narrow 150% layout, unsupported Undo/new-
sheet rotation, real Save/Open and Don't Save cleanup. R+1 is injected through
the admitted C producer after the Save snapshot; it is not a claim that physical
editing during IO is already enabled. Source/browser/full-App sanitizer gates
remain registered for milestones. `journal-session-demo.dxls` is generated output.

W08c1 uses `make test-feature FEATURE=journal-faults`. The private filesystem
interposer is built/fingerprinted alongside the exact optimized/sanitizer helper;
its environment applies only to that launched process and journal/WAL. ENOSPC,
EIO, partial write, fsync and close faults plus a real process file-size limit
exercise the actual storage API. New-file failures must retry after repair;
existing file failures must preserve old/new complete state. The queue retains
both immutable records and acknowledges them only after repaired commit. No real
disk is filled. Mac is validated; this is not Windows/Linux platform acceptance.
The full tier includes all registered drivers. The ordinary `journal-session`
plan defers this matrix, retaining its own model/GUI gate.

W08c2 uses `make test-feature FEATURE=journal-performance`. The explicit finite
`fixtures/workbook/journal-paste.sql` is generated by
`tools/create_workbook.py --journal-paste-only` into ignored `build/tests/generated/`;
its only consumer is `test_journal_performance.c`. It contains 20,000 nonempty
single-column clipboard values, including exact numeric text, literal NULL and
quoted Unicode/tab/newline text. The C helper exercises real admission, commit,
bounded backpressure, legacy paste, baseline rotation and complete replay.
`tools/benchmark_journal.py` runs each variant once, validates runtime DuckDB
version/library identity against its linkage and retains before/after hashes of
the helper, library and input. Only optimized timings inform batching; sanitizer
runs establish ownership/correctness. Producer API time is not physical input or
frame latency. No App/desktop launch or unrelated filesystem matrix is needed.

W09a uses `make test-feature FEATURE=workbook-save`: `test_workbook_save.c`
links the real native writer and verifies six cancellation points on both new
and existing destinations, rollback/retry boundaries, exact content/identity,
unrelated user tables and refusal to cancel a completed publication. It also
runs the existing narrow checkpoint/Save GUI. The primitive's callback performs
no synthetic fixture generation or physical desktop interaction.

`make test-feature FEATURE=save-worker` runs `test_worker_save.c` optimized and
sanitized, then the narrow checkpoint GUI. Private worker objects alone expose
saving-thread barriers. The test holds validation/write/publication/completion
while attempting explicit/generic cancellation and read replacement; committed
receipts must remain pollable, and failed/cancelled saves preserve old contents.

W09b2's complete local gate is `make test-feature FEATURE=workbook-save`:
controlled writer and ordered worker variants, then `test_workbook_save_gui.c`.
The `save-ui` plan runs only that GUI for UI iterations. Its private App/worker
objects hold actual writes at test barriers; production excludes those symbols.
The 18-stage workflow uses real Enter/Escape and pointer input, native pan input
through the real grid, 100%/narrow 150% geometry, complete versus cancelled
publication, failure preservation and Save R/R+1 with a pending Quit. The R+1
producer is the actual C cell command while physical editing stays IO-guarded.
`save_fence.c` also catches view/column width, definition, format and exact sparse
value changes that are not represented by the admitted command clock alone.

The journal primitive's default test now covers v2 origin ownership, full uint64
IDs/sizes, exact retries after appends, generation/parent/digest/locator conflicts,
unnamed-workbook origins, malformed rows/schema/digests/embedded NUL and refusal
to infer an origin for a legacy nonempty tail. No startup recovery UI is implied.

W10a2 extends `journal-session` with actual closed-checkpoint size/SHA-256,
original named/unnamed locator and parent-generation assertions. Queue tests
hold preparation, reject a missing/conflicting origin before acknowledgement,
retain pending records and repair with the exact origin. The separate
`journal-faults` matrix arms its private interposer only when binding begins,
verifying four v2 origin write/fsync failure modes with exact two-record retry
and fresh-process metadata/content checks. That named matrix remains deferred
from ordinary App/UI edits. Streaming fingerprint integration is macOS-only;
portable journal v2 accepts explicit caller-provided origin bytes on native C.

`make test-feature FEATURE=recovery-owner` runs the standalone actual-header/module
`test_file_lease.c`, session variants and the existing 18-stage checkpoint App.
A private forked child owns a lease; the test kills only that child and verifies
OS release. Separate handles/processes cannot claim an active generation, and
symlink/hardlink/nonzero content remains intact. Session tests verify active
ownership and claimability after keep-close; fixed lease files are known private
artifacts, not a fixture dataset or build-operation lock. The finite performance
helper also cleans that artifact. Test output stays under ignored `build/tests/`.

W10b1b extends the existing journal primitive/process plan with explicit READ_ONLY
inspection, missing-file/no-WAL creation checks, refused bind/append with preserved
receipts, and real v2 writer kills before/after COMMIT. The driver hashes the DB
and any WAL before inspection and verifies both content and artifact-set identity
after it. Exact same-ID retry remains valid afterward. Only private child writers
are killed; no desktop or user's process is addressed.

`make test-feature FEATURE=recovery-candidate` runs `test_workbook_recovery.c`
against real session-generated checkpoints/journals in optimized and sanitizer
variants. It checks held leases, full/checkpoint-only modes, exact values/IDs/
Unicode/format/view state, all cancellation phases, wrong baselines/digests,
wrong before-images/gaps, legacy or unreadable tails, aliases/WALs and ownership
transfer. A private worker barrier lets the live model accept and Undo an edit
while candidate projection runs; the candidate cannot issue live history/draft
IDs or retain fabricated durable Undo. DB/checkpoint bytes are preserved. This
controller uses the macOS streaming digest adapter; no App/desktop launch is
part of this backend gate. Its related `recovery-owner` GUI remains registered.

`make test-feature FEATURE=recovery-catalog` runs `test_directory_cursor.c` and
`test_recovery_catalog.c` against native streaming iteration and real session
artifacts. It covers no-create empty roots, borrowed row/cursor ownership,
pause/cancel/resume and step limits, no-follow symlinks/hardlinks, active leases,
two distinct same-document/revision branches, older/unbound and malformed/
incomplete artifacts, and unchanged checkpoint/journal bytes. It never traverses
unknown folders or prunes anything. POSIX iteration is validated on macOS; the
Windows adapter currently reports unavailable. The named `recovery-candidate`
interaction checks shared no-follow/dangling-WAL handling. Both native modules
stay excluded from the shared-C browser source inventory.

`make test-feature FEATURE=recovery-worker` runs `test_recovery_worker.c` with
private optimized/sanitizer reader objects. Test-only barriers hold before IO,
during replay and before reply publication. It exercises 32-row continuation,
65,536-byte original locators, copied rows/results outliving the worker, format/
row reservation transfer, cancelled and late results, rejection while an owned
reply is unpolled, and shutdown/join with a held lease. Artifact hashes remain
unchanged. Production App/reader objects contain no barrier symbols; no GUI or
external account/service is used by this backend gate.

`make test-feature FEATURE=recovery-ui` selects keytips (optimized/ASan/UBSan)
and the private hidden real-App recovery workflow. Its runtime setup uses the
actual session writer, then exercises startup, actual File menu paint/activation, dismissed preparation, checkpoint
fork/edit/Save As, deferred-tail preservation, dirty-guard Cancel/discard, full
replay/cleanup/reopen and a 640×640 150% panel. Borrowed UI geometry/focus lives
in `src/ui/recoveryview.c`; App/lease ownership lives in `src/app/recovery.inc`.
Hooks in `tests/support/recovery_app.inc` compile only into its private App object.
`make test-feature FEATURE=recovery-crash` uses the same private App build with
explicit writer/observer/restore roles, driven by `tools/test_recovery_process.py`.
It proves actual SIGKILL/restart and live-instance exclusion in optimized and
ASan/UBSan builds. The driver owns all PIDs, leases, private paths and logs; it
never interacts with the user's instance. The full milestone includes this gate.

The File command registry in `src/ui/keytips.c` supplies label/key/icon/semantic
metadata together. `test_keytips.c` validates every entry, unique IDs and invalid
actions. `make test-feature FEATURE=recovery-menu` additionally runs the real
19-stage recovery menu/document workflow in ASan/UBSan, preventing a direct
controller call from hiding a menu-paint or metadata indexing regression.

`test_workbook_schema.c` consumes the immutable current-package descriptors via
`src/workbook/schema.c`. It checks complete/reordered/ASCII-folded proofs and
missing, duplicate, unknown, view, type, NULL and embedded-NUL/oversized spans,
without an engine/window. The registered `workbook-schema` gate adds the existing
real Save workflow; `workbook-archive` retains the full native malformed-file
matrix. Runtime legacy-copy assertions use the existing native legacy helper,
verify non-writing v14 Open and v15 upgrade/identity, then remove their private
copy. They are codec assertions, not a standalone synthetic fixture generator.
Core/workbook test objects have actual `.d` dependencies and share native engine
objects; generated inputs and core-before-workbook execution ordering are intact.

`test_workbook --metadata-types-only` verifies actual C-API JSON/VARCHAR metadata
compatibility, exact stored text and byte-preserved constrained-write failure.
The pure proof's type mapping and real-engine alias check are separate; alias
support does not relax numeric signedness or row-value validation.

`test_version_index.c` exercises the pure borrowed version-index controller:
full-width opaque payload identities, explicit absent/present saved references,
UTF-8 tag metadata, dangling/duplicate/overflow/capacity rejection, unchanged Save,
private checkpoint/export policy and stale write-owner publication fences.
The `version-index` gate runs only its actual module/header in optimized and
ASan/UBSan configurations. Payload decoding/integrity/closure, native IO and GUI
publication belong to W11a2/W11a3 and are explicitly deferred interactions.

`test_workbook_payload.c` consumes the authored `transactions.csv` and
`chart-series.csv` paths as external locators without querying their contents.
It verifies actual core capture/ordinary Save SQL references, relocation,
offline paths, identities/exact manual text, preflight preservation and
controlled cancellation in optimized/ASan/UBSan builds. The `payload-paths`
gate also runs controlled/worker Save modules and the18-stage hidden Save GUI.
Full archive/recovery interactions remain named deferred checks.

`test_core_payload.c` exercises native core ownership without launching App:
private capture, closed read-only access, offline locators, exact SDK digest,
exclusive lease, write/hash cancellation, independent captures and working edits,
repeat verification, fixed-file cleanup and changed/aliased/unknown-file retention.
The `core-payload` gate runs this module and real file-lease tests in optimized
and ASan/UBSan configurations. App Save/archive/recovery adoption stays deferred.

`test_core_compare.c` observes all28 v15 table descriptors and both file checks,
then compares independently captured equal/changed cores. Cases include exact
text, NULL/empty/literal NULL metadata, formats, widths, primary/Table SQL,
pivot names/fields, ledger/command counters, display scale, document IDs and
changed/retired files. Cancellation exercises hash, pending SQL and final recheck
without publishing the output bit. The `core-compare` gate is native module-only;
Save/archive adoption remains a named deferred interaction.
