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

All native/module executables and generated fixture/report files now live under `build/`; the seven authored CSVs and immutable UI regression inputs remain in `tests/fixtures/`. The native fixture generator is C/DuckDB and no longer needs the mock, Node or the DuckDB CLI. `make clean` removes generated output while retaining SDK/public-data caches; `distclean` removes those caches too. Cleanup verification and remaining structural debt are recorded in [REPOSITORY.md](REPOSITORY.md).

Historical receipts and discarded studies remain in Git at `archive/pre-layout-cleanup-20261018`, not in active agent context. No new full application-inventory acceptance is implied by this layout cleanup.
