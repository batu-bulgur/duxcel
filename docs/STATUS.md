# Current implementation status

Duxcel uses one C99/raylib UI and DuckDB analytical definitions. Native macOS is the validated host. The former web mock and visual-study/checkpoint trees have been retired; current native behavior and written requirements define the baseline. Persistence and tests remain core work. Native pan/snap-back is user-confirmed resolved on the current Mac; broader gesture coverage is separate.

| Capability | Current behavior and limits |
| --- | --- |
| Sparse workbook/grid | Manual text, single-outline selections, deterministic four-neighbor Select All regions, border/Option-interior range moves, explicit Replace/Cancel for occupied manual destinations, atomic value/style and workbook-chronological session Undo/Redo, structural edits, per-sheet viewport, independent sheet zoom/UI scale and fixed header chrome. General formulas/typed editing/import overlays remain open. |
| Native files | Native macOS panels for sources and `.dxls` Save/Open; Finder/LaunchServices workbook Open on startup and in a running instance; compact dirty guard for Close versus Quit; explicit application menu with Command-Q; real versioned DuckDB workbook state. External sources remain references. Standalone packaging is incomplete. |
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

W02 adds explicit Replace/Cancel after a manual drag or destination-address move reaches occupied cells. The complete target is checked, including offscreen cells and source holes; source/object areas remain protected. Cancel/Escape preserves data/history/dirty state, and confirmation refuses changed sheet/selection/data/format/source definitions. A confirmed self-overlap move uses original source values and one existing composite Undo/Redo action. The 15-stage native workflow passes optimized (7.83s) and ASan/UBSan (8.72s), including 100%/640×640 at 150%, actual button input, stale confirmation and Save/Open. Existing optimized range-move/selection-start regressions pass 10/45 stages (5.59s/22.53s). The shared model passes 186 checks per optimized/sanitizer build; runner contracts pass 33 tests. The real shared-C/official-Worker controls profile passes 1,366 checks (34.36s), including exact replacement/Undo/Redo values/styles and default Cancel after button/Enter-driven Move. The rebuilt bundle passes strict signature verification and cold/warm LaunchServices Open with the alert guard. W02 does not add formula rewrites, global Undo, durable history or analytical-object replacement.

W03 implements deterministic manual containing regions for Select All. Four-neighbor components use the original finite selection as the seed; diagonal islands and format-only coordinates do not connect. Blank interiors use area/coordinate tie rules. Strict supersets retain active cell/direction and viewport, while current Table/Pivot/source bounds keep their existing priority. Pure C component/ladder tests pass 133/170 checks per optimized/sanitizer build. Hidden native W03 passes 14 stages (7.43s optimized/7.34s sanitizer), including reversed three-row selection, 100%/narrow 150%, offscreen data/format-only exclusion, no data/history/query changes and reopen. The existing analytical selection ladder passes 27 optimized stages; the shared-C/official-Worker browser selection profile passes 705 checks (77.70s) with actual Ctrl/Cmd+A and prior range-move/analytical-owner coverage. The rebuilt bundle passes strict signature verification and cold/warm LaunchServices Open with the native-alert guard. Cmd+Arrow data boundaries remain separate C01 work; no inferred Table, typed cell or formula engine is introduced.

W05a/W05b complete the single-cell command foundation under scoped automated
verification. The v2 command captures durable workbook/sheet IDs, session fences and
expected CellStore/admitted-command revisions. Changed edits create one worksheet
action and advance the persisted command clock; no-op/failure preserve it.
Native v15 persists a 128-bit document ID and exact uint64 sheet IDs/high-water.
Rename/reorder/snapshot/reopen retain identity; duplicate allocates a fresh ID.
Exhaustion rejects allocation atomically while existing sheets remain saveable.
Legacy v1–v14 Open assigns IDs in RAM without writing the file; Save upgrades.
Malformed current identity metadata rejects Open/overwrite without replacing the
live document or corrupt archive. Session fences still reject old drafts after
reopen, including a file with matching durable IDs. Direct legacy mutation
producers retain their own store clocks; W06 history
replays now advance the command clock. Durable journal work remains W08.
The partial browser header decoder still rejects v15; no browser Open/Save claim.

The W05a baseline verification recorded 158 shared-model assertions per optimized and
ASan/UBSan build and 13 real native App stages (29 assertions). Native GUI time
is 5.06s; the last local gate is 12.11s including a 6.12s feature App recompile.
The fixture covers actual Enter/Escape/Undo/Redo, preserved no-op Redo, a second
command racing an open draft, visible recovery text/pixel ink at100/150 percent,
and Save/Open followed by old-session rejection. Its test-only hook is omitted
from production/review/browser objects. The dedicated browser F2/UTF-8
review/cancel/commit/Undo interaction passes142 checks (40.65s including build
and owned browser lifecycle); the final native-only warning line-spacing fix
was then verified by the focused pixel check. This is not browser stale-warning
visual acceptance. The 38 runner/catalog contracts pass. Wider document,
workspace, recovery and browser suites remain explicitly deferred. Source/binary
receipts and captures live under ignored `build/tests/results/`; physical input
remains outside this automated gate. The clean generated demo is
`build/tests/generated/cell-command-demo.dxls`.

W05b's focused gate passes75 identity,158 cell-command and2,353 descriptor checks
per optimized/ASan/UBSan configuration, followed by800 private native identity/
migration checks per configuration. The nine-stage hidden TEST App passes18
checks in3.668s; the final local gate takes66.831s including39.595s native archive
sanitizer work and6.119s feature App compilation. It exercises legacy Open,
rename/duplicate/reorder, independent edits, asynchronous Save/Open and stale
session rejection. A separate optimized legacy-header comparison passes1,611
checks across v12/v13/v14/physical-reorder/legacy24 components;41 focused runner,
catalog and identity-fixture tool contracts pass. Source/binary fingerprints,
completion markers, logs and captures are under ignored build/tests/results/.
Full native/source/browser regression remains deferred. Generated example inputs are
build/tests/generated/identity-legacy.dxls (untouched v12) and
build/tests/generated/workbook-identity-demo.dxls (saved v15, two independent sheets).

W06 completes workbook-chronological session Undo/Redo under the local automated
gate. Existing worksheet actions carry process-order stamps, independent of
stable IDs and persistent command revisions. The shared C history coordinator
selects the newest Undo / earliest Redo, verifies aligned constituent counts,
transfers cell/style snapshots atomically and advances the command clock. The
App follows the selected sheet, revealing it when hidden; analytical/object
replay keeps its existing App-owned adapter. New changed cell commands clear all
abandoned Redo payloads; legacy UI records normalize the branch at frame end.
No-op/rejection retain the tail. Display defaults are
not replayed, rename/reorder preserve targeting, and pruning establishes a safe
past/future barriers rather than skipping missing edits on another sheet. Save/Open preserves
current values/IDs/revision and intentionally clears session history. Full history
persistence remains separate. Ordinary slices no longer carry a routine human-
acceptance-pending flag; detailed hands-on review belongs at major milestones.
The local W06 gate passes 565 chronology, 158 cell-command and 75 identity checks
per optimized/ASan/UBSan configuration, then 21 hidden TEST App stages with 37
assertions in 6.394s. Total gate time is 17.702s including 6.048s feature App rebuild;
there is no canonical dataset, archive matrix or full-App sanitizer bootstrap.
The GUI covers actual Cmd-Z/Shift-Cmd-Z, hidden owner reveal, a no-op preserving
both sheets' Redo, legacy style branching/cleanup/replay, a new cell branch,
rename/reorder and Save/Open clearing session history while retaining values,
IDs and command revision. Broader native/source/browser interactions remain
registered for milestones. The generated checkpoint is
build/tests/generated/workbook-history-demo.dxls; tests and source/binary
receipts remain in ignored build/tests/results/.

W07 adds undoable worksheet creation, duplication, deletion, rename, reorder,
hide and show to the same workbook chronology. The worksheet menu now offers
Delete; Undo restores its cells, formats, objects, source bindings, layout and
stable IDs. Deleted ownership is retained without a data clone and released
when the bounded session history or abandoned Redo branch no longer needs it.
The last visible sheet is protected. Save/Open retains current sheets and ID
high-water but intentionally clears session history; durable replay and formula
references remain later slices. The focused gate passes 696 lifecycle, 565
chronology and 158 cell-command checks per optimized/sanitizer configuration,
then 24 hidden TEST App stages/29 assertions in 6.434s (17.590s total gate).
The adjusted W06 GUI also passes; broader source/browser regression is deferred.

Native manual-cell edits now have an asynchronous checkpoint-backed local
journal. The status bar distinguishes Backup pending, Cell edits backed up,
Backup paused with keyboard/mouse Retry, and Save needed when other changes are
outside the current admitted prefix. Queue backpressure preserves the draft.
The next changed manual cell captures a new baseline after unsupported changes;
only two generations may coexist, and obsolete ownership is released after the
replacement is complete. Explicit discard removes current-session owned files;
clean Save/Open retains the named workbook. An older Save acknowledgement does
not clear newer admitted/recorded edits.

The local `journal-session` gate passes 126 session, 1,443 queue, 190 storage and
169 cell-command assertions with optimized/sanitizer coverage, plus 14 process-
kill/retry checks per binary. The final hidden TEST App passes 18 stages/28
assertions in 6.797s (17.542s focused release gate).
It covers the real editor, retained draft, keyboard/mouse Retry at narrow 150%,
checkpoint rotation, Save R/R+1, reopen and Don't Save. The existing W06 GUI also
passes. The separate W08c1 fault matrix now passes actual write-limit and injected
ENOSPC/EIO/short-write/fsync/close failures in optimized/sanitizer builds. A failed
first creation cleans up only its own new incomplete journal, allowing Retry;
existing files remain intact. W08c2 now measures optimized warm producer/durable p95 at 0.005/1.516ms on
DuckDB v1.5.6; 256 burst edits coalesce into two commits. The full 20,000-cell
checkpoint and tail replay passes optimized/sanitizer; its background checkpoint
is about 2.14s. Keep immediate wakeup and bounded natural batching, without an
artificial delay. These are helper API measurements, not UI frame guarantees.
Startup
recovery selection is W10; persistent Undo is W12; W09 still owns complete Save
publication/progress/cancel. Browser persistence is unchanged and has no native
journal backend. Receipts and captures remain in ignored `build/tests/results/`.

## Feature iteration and build identity

The default CLI development tier now requires a registered feature and never
silently runs every fast module. `make test-feature FEATURE=<id>` selects local
C logic and a narrow GUI workflow; plans retain explicit deferred interaction
commands and the native full milestone gate. Current registrations cover manual
containing regions, selection ladder, range move/replacement, About and test
infrastructure. Legacy tests remain available; this is not a completed extraction
of all native scenarios or all App controllers.

Selection expansion now has a borrowed-context C controller, with App retaining
result readiness/jobs/focus ownership. About metadata has its own C translation
unit and source-archive fallback; the macOS adapter owns history scrolling and
runtime TEST icon/Dock identity. About includes compiled commit subject/hash,
author/date, build time, dirty status and a source fingerprint that distinguishes
uncommitted code revisions. Git is not needed at runtime. The native standalone
check exercises hidden real AppKit widgets; lifecycle menu/Open/Quit interaction
coverage is extended and deliberately deferred to its explicit gate.

Focused verification: 42 runner/catalog/browser-build contract tests and six
metadata-generator tests pass. Selection cells/ladder/controller checks pass
353 assertions per optimized/sanitizer build, followed by 14 real native GUI
stages. The warm feature command completes in 9.86s on this Mac, including build
checks; App GUI time is 7.58s. The About gate passes its direct-C archive builds
and 36 hidden real AppKit checks in 5.11s including incremental compilation.
Its offscreen panel/icon captures were inspected. The bundle builds and the
extended lifecycle helper compiles; full lifecycle/Open/browser suites were
intentionally not run for this change. Receipts remain under ignored
`build/tests/results/runs/`. These timings describe the narrower gates, not an
equivalent full-suite benchmark or milestone review.

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

W09 completes native controlled Save publication. A compact progress panel offers
Escape/Cancel until publication starts; actual completed work drives progress.
The grid scrolls outside the panel while structure/data edits remain IO-guarded.
Latest-wins reads/cancellation cannot drop the Save receipt. Exact current values,
definitions/view state and registered mutation clocks/action order fence R against
R+1, preserving newer dirty state and its pending close guard. Failed/cancelled
Save preserves the previous file/path and local recovery material. Focused
optimized/sanitizer writer and worker checks plus 18 hidden real-App stages pass,
including pointer Cancel at 150%, native pan, R+1 and failed Save. Startup recovery
remains W10; retained durable Undo and full analytical outputs remain later slices.

W10a now binds native macOS recovery generations to the actual closed checkpoint
size/SHA-256, parent generation and original locator before acknowledging journal
edits. Origin failure/mismatch keeps pending bytes for explicit Retry. Scoped
optimized/sanitizer model, actual fingerprint/rotation, origin IO-fault and hidden
App checks pass. Startup recovery/branch choice is still unavailable (W10b/W10c).
Other native hosts retain unbound v1 backups pending their digest adapter.
