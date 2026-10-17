# Current implementation status

Audited 17 October 2026 against baseline Git commit `6a3542d` and the earlier frozen acceptance evidence. This is an implementation review, not a fresh full visual sign-off.

The **complete exact UI port is unfinished**. The core is real: asynchronous DuckDB queries, bounded source/result pages, native file panels, saved workbooks and same-sheet analytical objects. Geometry and many interactions already have substantial native coverage. Some visible controls and several deeper semantics remain partial.

| Capability | Implementation location | Current quality / remaining work |
| --- | --- | --- |
| Native Open/Save, `.dxls` v12 | `src/platform/macos.m`, `src/workbook/workbook.c` | Implemented; native document lifecycle and migrations tested. External sources remain references; standalone distribution pending. |
| Six-tab adaptive ribbon | `src/ui/ribbon_spec.c`, `ribbon.c`, `flyout.c`; App actions | Implemented measured adaptation/key access; final matching-state visual acceptance open. |
| Narrow panels and inspector lifecycle | `src/ui/workspace.c`, `dockview.c`; App | Implemented recoverable left overlay, right collapse/dismiss and independent UI scale. Broad physical DPI/gesture coverage remains limited. |
| Compact Rows/Columns and left Measures/Marks/Filters | `src/ui/workspace.c`, `marksview.c`; App authoring controls | Implemented wrapping, scrolling, owned tokens, assignment/reordering and keyboard paths. Filter pills still show generic `· Filtered`; remaining select/dialog anatomy differs from reference. |
| Same-sheet sources and pivots | `src/workbook/objects.c`, `src/transforms/pivot.c`, `src/data/worker.c` | Real DuckDB `PIVOT`; separate source/result identities, placement/history/persistence. No mock computation. |
| Matrix totals | `src/transforms/pivot.c`, `src/data/database.c` | Source-derived `GROUPING SETS` totals implemented; exact DECIMAL/NULL retained. These readouts are not editable worksheet cells. |
| Paired matrix/chart and source preview | `src/ui/preview.c`, `chartview.c`, App | Implemented shared cached result, adaptive stacking, scrolling and return to source. Final state-by-state parity open. |
| Column value/date filters | `src/data/database.c`, transforms and App filter controller | Real paged distinct checklist, search, conditions/Between, five-level date hierarchy, typed NULL and bounded selection. Full filter scenario passed on frozen binary. |
| Marks assignment and rendering | `src/transforms/marks.c`, `src/ui/chartencoding.c` | Bindings persist and selected cached fields drive color/size/shape/hover. Fields must already exist in Rows/Columns/Values; Color row-only, Shape line-only. Arbitrary grain, continuous scales, persistent labels and matrix encodings incomplete. |
| Chart Other | `src/ui/chartview.c` | Composable aggregates supported; AVG/distinct omitted honestly. Aggregate-aware semantics pending. |
| Sparse manual cells and structural edits | `src/workbook/cells.c`, `objects.c`; grid/App | Text entry, clipboard, range moves, contextual insert/delete, bounded undo and persistence work. General formulas, typed editing and imported-source overlays pending. |
| Formatting | `src/workbook/format.c`; App/grid | Exact numeric display and saved range formatting implemented. Coordinate-based formatting can attach to different source records after sorting/filtering; stable source-record identity unresolved. |
| Active cell versus selection extent | `src/grid/grid.c`; App matrix navigation | Newer implementation retains origin under drag/Shift; updated grid tests pass. Newer than frozen native pass; legacy native assumptions and targeted acceptance need reconciliation. |
| Pointer/keyboard focus visibility | `src/ui/ui.c`, `choiceview.c`; App | New modality implementation and unit coverage; fresh native pointer/keyboard/pixel acceptance pending. |
| Active-value retention across page eviction | `src/ui/cellreadout.c`; unused `ActiveReadout` in App | Helper owns bounded UTF-8/type/value data and passes 133 assertions/configuration. **Not integrated**: current value strip still depends on current/spare pages. |
| Return-to-source bookmarks | App source locations; `GridLocation` | Range/viewport restoration implemented. Absolute bookmark coordinates do not detect table placement moves; validate/rebase before calling robust. |
| SQL, transformations, historical preview, statistics | `src/app/step_preview.c`, `src/ui/{stepview,statsview,guideview}.c`; App | Real query/prefix/aggregate integration with explicit owned snapshots; final shared-control/dialog fidelity remains open. |
| Renderer/font/motion | `src/ui/{fontmetrics,motion,paintlayer*,popup,ui}.c` | Real Plex metrics, finite motion, bounded opacity caches and retained batches; dedicated GPU ownership/pixel coverage. CPU timings do not establish input latency. |

## Evidence and version boundary

- Frozen pre-Git binary SHA-256 `98724ccaf6a57102667c0cb305e8a94db3048ce7e4ef394ff7d782ddad77eac4`: **988 maintained application stages in 18 scenarios passed**, plus **107 separate GPU checks**. Recorded total runtime approximately **14m20s**. [Preserved summary](validation/checkpoints/2026-10-17-author-menu/author-menu-regression-summary.txt).
- That checkpoint's pure release and ASan/UBSan suites passed **40,575 assertions each**. These counts and the frozen source manifest describe that checkpoint, not every subsequent edit.
- New active-cell/focus grid tests pass **16,812 assertions per configuration**; new readout helper passes **133 per configuration**. Neither proves complete native integration of the newer code. See [validation](validation/README.md).
- The new Git snapshot captures both accepted older functionality and newer work in progress. It is a recovery point, **not a tested release tag**. There is no remote configured.
- Development-tooling acceptance on integrated commit `3dd441f`: 10 runner checks and all 17 read-only modules in release/ASan+UBSan passed (**74,820 assertions total**, 39.734s including 29.468s rebuild). The warm grid/choiceview affected gate passed **46,228 assertions in 0.452s**, including build checks. These headless results do not close native UI parity. [Run manifests](validation/checkpoints/2026-10-17-development/).
- The optional chart-error fix passed **1,683 workbook assertions per release/sanitizer configuration** in its isolated branch (7.352s /37.867s). Its component source is unchanged by integration; no new full-suite claim is made.
- Native completion now waits for all enabled scenarios, one final rendered frame and idle application/worker publication. Sanitizer export/filter/query/document and combined export/icons passed; an insufficient frame budget still failed. Matched export elapsed time fell **19.153s →10.542s**. The integrated release source at `aac49f6` built without warnings and passed the **15-stage author-menu affected profile in 9.659s**. [Development evidence](validation/checkpoints/2026-10-17-development/).

The current source has **not** passed a new complete native layout run. Its older early layout assertions still contain extent-as-active expectations (for example case 4), which must be reconciled with the newer active-origin behavior and covered by direct regressions. Do not treat the new full-run command or the old frozen pass as proof that this work-in-progress source is fully green.

Track three separate questions for each new feature: does it match the reference, do the real interactions work, and are the intended analytical semantics complete? Closing one does not close the others. New acceptance records belong under `validation/`; this table should describe the latest state without copying run histories.
