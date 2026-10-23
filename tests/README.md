# Native tests and inputs

C/Objective-C `test_*` sources exercise the real native product API, ownership,
engine and renderer. They are compiled only by test targets. Product sources
stay in `src/`; generated programs/databases/reports stay in `build/tests/`.

| Location | Role | Retention |
| --- | --- | --- |
| `test_*.c`, `test_*.inc`, `test_*.m` | Native suites and their implementation fragments | Tracked source |
| `test_fixture.c` | Native load/PIVOT/typed data/save/reload acceptance of canonical/large inputs | Tracked C test |
| `test_workbook_journal.c` | Private DuckDB lineage, atomic batches, BLOB/uint64 round-trip, exact retry, rollback/conflict/cancel, bounds, version/gap rejection and initial timing | Optimized/sanitizer engine runs serially; no App/canonical fixture |
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
