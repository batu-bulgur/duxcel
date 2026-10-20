# Current implementation status

Duxcel uses one C99/raylib UI and DuckDB analytical definitions. Native macOS is the validated host. The former web mock and visual-study/checkpoint trees have been retired; current native behavior and written requirements define the baseline. Persistence and tests remain core work. Native pan/snap-back is user-confirmed resolved on the current Mac; broader gesture coverage is separate.

| Capability | Current behavior and limits |
| --- | --- |
| Sparse workbook/grid | Manual text, single-outline selections, border/Option-interior range moves with grabbed-cell anchoring, explicit Replace/Cancel for occupied manual destinations, atomic value/style Undo/Redo, structural edits, per-sheet viewport, independent sheet zoom/UI scale and fixed header chrome. General formulas/typed editing/import overlays remain open. |
| Native files | Native macOS panels for sources and `.dxls` Save/Open; Finder/LaunchServices workbook Open on startup and in a running instance; compact dirty guard for Close versus Quit; explicit application menu with Command-Q; real versioned DuckDB workbook state and history. External sources remain references. Standalone packaging is incomplete. |
| Analysis | Real DuckDB source/filter/sort/query/available-value/date operations, same-sheet Tables/PIVOT/charts and exact typed statistics. No mock calculation engine. Unsupported Marks grain/lineage remain bounded. |
| NULL and precision | SQL NULL, empty text and literal `NULL` remain distinct; exact integer/DECIMAL text survives supported operations. Sparse missing cells use the distinct NULL mark. |
| Record formats | Verified native primary/Table and supported archived RESULT/GUARDED subsets exist. Broader history/owner migration, new RESULT commands and browser activation remain incomplete. |
| UI | Native ribbon/shelves/panes/dialogs/keytips/keyboard history implemented, with remaining clipping/focus/narrow-window/state-matrix work. Current regressions use accepted native contracts, not a live mock. |
| Typography | Bundled Plex Sans/JetBrains Mono, replacement glyph and failure-safe reload. Broader CJK/emoji and expanded atlas budgets remain open. |
| Shared-C Wasm | Same C UI with official asynchronous DuckDB-Wasm Worker. Real import/PIVOT/statistics/copy/export and captured session registrations are implemented. A focused sheet-zoom row-label glyph comparison currently fails; build/import/Worker/codec/Guide passes do not close that visual check. Browser/IME/clipboard/WebGL/display matrix remains incomplete. |
| Browser workbook persistence | Shared C/native/official-Worker metadata components cover definitions, filters, cells/columns, counters, ledger/bindings and base format rows. Whole-candidate validation/migration/writer/admission/recovery remain open; browser workbook Open/Save is unavailable. |
| Review/feedback | Native and Wasm use the development-only C semantic registry and persistent external queue. Automatic triage is separate from implementing/verifying a report. All review code/commands/bridges are excluded from production. Services start only on explicit developer commands. |
| Web publication | GitHub Pages builds the real shared-C Wasm app. Web tests run independently; publication requires build/package/browser smoke. Hosted native CI is manual/paused; no native Linux/Windows execution claim. |

## Workbook TRD demo slices

[ROADMAP.md](ROADMAP.md#workbook-trd-implementation-slices) decomposes the accepted workbook TRD into prioritized dependency slices. W01 implements manual-range Option/Alt-interior movement through the existing move/Undo command, retaining border movement and protected-area guards. Escape/no-op leaves data unchanged, releasing Option after capture preserves the move, and multi-cell paint retains one outer active outline with the logical active cell unchanged. General formulas, floating object movement, global durable history and refresh/Data Model remain planned capabilities.

The real hidden native range-move profile passes ten stages in optimized (6.07s) and O1 ASan/UBSan (6.25s) App builds, including single-outline pixel assertions at 100%/150%, exact moved values/styles, Undo/Redo and `.dxls` Save/Open. Focused grid tests pass optimized + ASan/UBSan; the runner's 33 orchestration checks pass. The optimized bundle builds and passes strict signature verification. The real shared-C/official-Worker browser selection profile also passes 569 checks (97.25s including owned lifecycle), ending with trusted Alt-mouse movement from the second selected row and one Undo/Redo action without source queries. The 24 browser-runner contract tests pass. Source/binary fingerprints and concise run receipts are in ignored `build/tests/results/runs/`. The user confirmed that the demo opens and movement works; broader platform/input acceptance remains separate.

The W01 demo exposed a separate bundle-entry defect: missing workbook type declarations and unhandled macOS Open Document events. The bundle now declares `.dxls`/legacy `.duxcel`, installs its delegate before cold-launch file delivery and queues owned paths into the existing guarded C document controller. A hidden, uniquely identified test bundle passes real LaunchServices cold and already-running Open with a space/Unicode filename (13.62s); both final framebuffers are byte-identical to the direct-path workbook reference. Native lifecycle and document-safety regressions pass, the production Wasm build links its portable no-op boundary, and the refreshed bundle passes strict signature verification. The user has now confirmed the opening fix. The follow-up demo report was traced to an old still-running executable being reused after the bundle rebuild. The Open regression also loads a test-only native-alert guard, verifies its intentional-failure control, and rejects unexpected modal/sheet alerts in every successful launch. Use `open -n -a` for a fresh rebuilt-demo process, or quit the old instance before normal Open.

W04 replaces the oversized unsaved panel with a 360×152 logical-unit dialog, action-specific title, ellipsized filename, short warning and standard Don't Save / Cancel / Save buttons. The macOS adapter now constructs its application menu explicitly: About, Settings, Services, Hide and Quit. It no longer assumes GLFW supplied Quit; Command-Q and menu Quit enter the existing C document guard. Native document acceptance passes all ten stages in optimized (7.08s) and ASan/UBSan (7.56s) App builds, exercising actual button clicks, long Unicode names, 100%/narrow 150% geometry and failed IO preservation. Repeat-session lifecycle tests pass 38 checks in each build (1.43s/1.59s), including AppKit menu and Command-Q dispatch with text/modal context and clean session termination. The real shared-C/official-Worker browser controls profile passes 1,185 checks (27.32s), retaining unavailable Save and safe Cancel. The rebuilt bundle passes cold/warm LaunchServices Open with the alert guard (12.34s) and strict signature verification. The user has accepted the compact dialog and confirmed physical Command-Q works, completing W04. Durable journal, recovery and W09 Save progress remain separate.

W02 adds explicit Replace/Cancel after a manual drag or destination-address move reaches occupied cells. The complete target is checked, including offscreen cells and source holes; source/object areas remain protected. Cancel/Escape preserves data/history/dirty state, and confirmation refuses changed sheet/selection/data/format/source definitions. A confirmed self-overlap move uses original source values and one existing composite Undo/Redo action. The 15-stage native workflow passes optimized (7.83s) and ASan/UBSan (8.72s), including 100%/640×640 at 150%, actual button input, stale confirmation and Save/Open. Existing optimized range-move/selection-start regressions pass 10/45 stages (5.59s/22.53s). The shared model passes 186 checks per optimized/sanitizer build; runner contracts pass 33 tests. The real shared-C/official-Worker controls profile passes 1,366 checks (34.36s), including exact replacement/Undo/Redo values/styles and default Cancel after button/Enter-driven Move. The rebuilt bundle passes strict signature verification and cold/warm LaunchServices Open with the alert guard. User hands-on acceptance is pending; W02 does not add formula rewrites, global Undo, durable history or analytical-object replacement.

## Current repository and tooling


`main` is the sole local development branch. Ordinary solo work commits directly
there; temporary detached worktrees are for actual parallel lanes. All 231 prior
branch tips and five dirty worktree snapshots are reachable from the historical
`archive/streamline-20261020` tag. The five retired checkouts are removed. Their
component passes and remaining work are recorded in [ROADMAP.md](ROADMAP.md),
not hidden in branch names or declared complete by cleanup. Historical source
remains in Git; duplicate filesystem backups and disposable old run reports are
removed.

Build/test output and operation locks live under `build/`, dependency caches under
`.cache/`, and downloaded references under `data/downloads/`. `make clean` removes
all build output and retires legacy `.state/`; `distclean` also removes dependency
caches. The actual clean verified unchanged reference-data hashes, including NYC,
and retained the SDK. A checkout-specific OS-temp shared/exclusive gate protects
lock lifetime while cleanup removes build-local files. Active owner/conflict and
replacement-lock cases are tested.

Synthetic fixture production is `tools/create_workbook.py`, using the existing
DuckDB CLI and the finite v12 `tests/fixtures/workbook/transactions.sql` input.
No C generator, Python DuckDB package or second generic workbook writer is used.
`tests/test_fixture.c` separately verifies product load/PIVOT/typed values and
save/reload on canonical/large files (44 checks per optimized/sanitizer run).

Incremental native App/review/fixture-test builds default to eight compilation
jobs (`BUILD_JOBS`), sharing the recursive Make jobserver. A clean optimized
56-unit build previously measured 18.852s with one job and 9.630s with eight
(1.96×); a warm build made no compiler/linker calls. Legacy whole-program module
builds and mutable test execution remain serial. This does not change Wasm
compilation parallelism.

Web source/flag discovery reads stdout separately from Make scheduler diagnostics,
preserving command-line overrides when Python is launched inside parallel Make.
The regression covers all four Wasm source inventories and the header flags.
Local release/O1/edit/review native builds and the signed bundle pass, as do all
four Wasm build modes, scoped C sanitizer tests and the production browser
import/filter, Worker, codec and review Guide checks. The separate zoom glyph
comparison remains an explicit visual limitation, not a compilation failure.

## Recovered persistence scope and verification

The recovered shared-C format awareness/selector reader passes 82,792 pure checks
and 451,526 native comparison checks in each optimized/ASan/UBSan configuration.
The native integration includes the authored test fragment hidden by the old
worktree ignore rule. Native comparison took 11.1s/68.9s; pure checks took
0.83s/1.49s. Recovered JS selector transport uses the native `kind` column name;
56 focused tests pass. The complete Node suite passes 337 tests (3.39s), Python
orchestration passes 175 tests (19.62s), final workspace/generator checks pass 11
(0.89s), and real Make graph contracts pass 73 checks (23.4s).

The tag reader and JS transport are components, not an integrated official-Worker
format continuation or browser Open/Save. Format-state transport, real browser/C
admission/cancellation/retry, scopes/guards/memberships, whole-candidate validation,
migration, writer/admission/recovery and App publication remain open.

A clean optimized + O1 ASan/UBSan native build and signed macOS bundle are produced
from the combined source. Hidden active-cell acceptance passes 41 stages in
19.42s (21.90s with its focused/background checks); the sanitizer base passes six
stages in 3.82s. The signed bundle passes six hidden stages from `/` in 3.43s and
strict signature verification. Concise source/binary receipts live under ignored
`build/tests/results/`; no new complete native/browser/physical-input/OS
regression is implied. The earlier shared-C Wasm
UI/Worker checks established their layout boundary before this change; they do
not close the recovered format-tag workflow.
