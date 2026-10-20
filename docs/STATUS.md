# Current implementation status

Duxcel uses one C99/raylib UI and DuckDB analytical definitions. Native macOS is the validated host. The former web mock and visual-study/checkpoint trees have been retired; current native behavior and written requirements define the baseline. Persistence and tests remain core work. Native pan/snap-back is user-confirmed resolved on the current Mac; broader gesture coverage is separate.

| Capability | Current behavior and limits |
| --- | --- |
| Sparse workbook/grid | Manual text, selections/range moves, structural edits, per-sheet viewport, independent sheet zoom/UI scale and fixed header chrome. General formulas/typed editing/import overlays remain open. |
| Native files | Native macOS panels for sources and `.dxls` Save/Open; guarded Close versus Quit; real versioned DuckDB workbook state and history. External sources remain references. Standalone packaging is incomplete. |
| Analysis | Real DuckDB source/filter/sort/query/available-value/date operations, same-sheet Tables/PIVOT/charts and exact typed statistics. No mock calculation engine. Unsupported Marks grain/lineage remain bounded. |
| NULL and precision | SQL NULL, empty text and literal `NULL` remain distinct; exact integer/DECIMAL text survives supported operations. Sparse missing cells use the distinct NULL mark. |
| Record formats | Verified native primary/Table and supported archived RESULT/GUARDED subsets exist. Broader history/owner migration, new RESULT commands and browser activation remain incomplete. |
| UI | Native ribbon/shelves/panes/dialogs/keytips/keyboard history implemented, with remaining clipping/focus/narrow-window/state-matrix work. Current regressions use accepted native contracts, not a live mock. |
| Typography | Bundled Plex Sans/JetBrains Mono, replacement glyph and failure-safe reload. Broader CJK/emoji and expanded atlas budgets remain open. |
| Shared-C Wasm | Same C UI with official asynchronous DuckDB-Wasm Worker. Real import/PIVOT/statistics/copy/export and captured session registrations are implemented. Browser/IME/clipboard/WebGL/display matrix remains incomplete. |
| Browser workbook persistence | Shared C/native/official-Worker metadata components cover definitions, filters, cells/columns, counters, ledger/bindings and base format rows. Whole-candidate validation/migration/writer/admission/recovery remain open; browser workbook Open/Save is unavailable. |
| Review/feedback | Native and Wasm use the development-only C semantic registry and persistent external queue. Automatic triage is separate from implementing/verifying a report. All review code/commands/bridges are excluded from production. Services start only on explicit developer commands. |
| Web publication | GitHub Pages builds the real shared-C Wasm app. Web tests run independently; publication requires build/package/browser smoke. Hosted native CI is manual/paused; no native Linux/Windows execution claim. |

## Current repository and tooling

`main` is the sole local development branch. Ordinary solo work commits directly
there; temporary detached worktrees are for actual parallel lanes. All 231 prior
branch tips and five dirty worktree snapshots are reachable from the historical
`archive/streamline-20261020` tag. The five retired checkouts are removed. Their
component passes and remaining work are recorded in [ROADMAP.md](ROADMAP.md),
not hidden in branch names or declared complete by cleanup. Historical mock and
reports remain archived, outside active source.

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
