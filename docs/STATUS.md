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

## Current layout change

Build/test outputs are isolated under `build/`, dependency caches under `.cache/`, downloaded reference inputs under `data/downloads/`, and stable checkout locks under `.state/`. `make clean` removes all build output; `distclean` additionally removes dependency caches, and both preserve reference data. Native C helpers live in `tests/support/`; Python tool tests in `tools/tests/`; browser tests/drivers in `web/tests/`. Three versioned UI contracts retain accepted values without archived run manifests. These are tooling/layout changes, not new GUI features. See [repository boundaries](REPOSITORY.md).

Historical receipts and discarded studies remain in Git at `archive/pre-layout-cleanup-20261018`, not in active agent context. No new full application-inventory acceptance is implied by this layout cleanup.

Incremental native App/review/helper builds now default to eight compilation jobs (`BUILD_JOBS`), sharing a recursive Make jobserver. Legacy whole-program module builds and mutable test execution remain serial. On this Mac, an isolated clean optimized App build compiled 56 translation units in 18.852s with one job versus 9.630s with eight (1.96×); warm builds took about 0.46s with no compiler/linker calls. The focused gate passes 73 real Make checks and 37 runner/workspace contracts. The parallel-built optimized and O1 ASan/UBSan applications each complete six hidden native stages (3.676s/3.652s). This changes the native build graph, not product behavior or Wasm compilation; no new full regression is claimed. Concise hashes/timings live in ignored `build/tests/results/parallel-build/summary.json`.

## Tooling boundary verification

The role/layout integration passes 169 Python contracts, 325 Node contracts, 60 real incremental/atomic Make checks and optimized/O1 ASan/UBSan canonical/large C fixture runs. Focused grid/inspector/shadow/codec checks pass both configurations; hidden native selection completes 41 stages (90.7s including builds), then base/NYC completes 14 stages (10.3s). The restored NYC source is in `data/downloads/`.

Real hidden shared-C Wasm panel/SQL workflows pass 42.6s/58.2s; the relocated isolated C/official-Worker host passes 29.3s. The staged production browser import/filter gate passes 14.0s. Native bundle smoke from `/` passes six stages in 3.2s and codesign verification passes. Normal/review Wasm and native release/sanitizer artifacts pass production review-exclusion checks. These verify the layout/interfaces, not full 1351-stage native or physical-input/browser/OS coverage. Concise source/binary results live in ignored `build/tests/results/tooling-summary.json`; transient successful captures are disposable.
