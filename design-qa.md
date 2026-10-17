# Native design QA — 17 October 2026

final result: blocked

This is an implementation-progress report. No user decision or external approval is blocking work; the exact native migration is unfinished. The browser interface is the specification, not a loose inspiration.

## Evidence

The reference is `design/prototype/qa/iteration-wide-final.jpg`, the running browser study at Home/full/authoring, and its actual source in `design/prototype/src`. The running reference was captured in this chat at 1440 × 988 after the lazy-loaded analysis views appeared. The native framebuffer at the corresponding viewport/state is `tests/results/workspace-layout/native-exact-authoring-1440.png` (2× Retina). Native fixtures use actual DuckDB input/PIVOT results; synthetic finance values and date grains are not substituted for engine behavior.

The matching owned sample has 310 rows / 240 filtered transactions / 30 PIVOT groups. The complete 400-stage native layout checkpoint and 28,832 pure release/sanitizer checks per configuration passed (`paint-integrated-native.txt`, `paint-full-pure.txt`). It includes correct compact Workbook rows, exact font metrics, field-menu anatomy, source-derived totals, retained renderer buffers and actual nested pane opacity. The separate GPU renderer probe now passes 104 checks, including safe last-complete/back captures for popup exit. Popup application integration and final matching-state visual acceptance are still underway. Screenshots under `tests/results/workspace-layout` are checkpoint evidence, not a claim that all remaining controls match.

## Blocking differences

| Priority | Difference | Required result |
| --- | --- | --- |
| P1 | Six Marks targets have real owned bindings, but full encoding semantics remain incomplete | Complete query-grain support, continuous scales, persistent labels and matrix/legacy behavior |
| P1 | Source-cell formatting identity through sorting/query reindexing is not matched | Preserve the intended formatted source records through source transformations |
| P1 | Aggregate-aware chart Other remains incomplete | Matrix totals now use real source-derived GROUPING SETS; chart Other must also preserve AVG/distinct semantics |
| P2 | Final integrated matrix fidelity acceptance remains pending | Continuous scrollbars and range selection are implemented; verify them with matching source data and views |
| P2 | Transformations, inspectors, filters and dialogs retain portions of the previous visual grammar | Match the canonical shared widget and pane anatomy |
| P2 | Final motion/focus acceptance is incomplete | Pane/group opacity, popup exit ownership and contextual entry pass focused native checks; 90 ms shared feedback and explicit input/visual availability are under complete integration |
| P2 | Final actual-app regression and comparison are pending | Release/sanitizer checks plus native pointer/keyboard acceptance at matching states/viewports |

Implemented geometry now follows the canonical 28-unit title, 28-unit tabs, 80/36/32 ribbon, 26-unit context title, 31-unit shelves, 40-unit gutter, 48-unit source header, 24-unit default rows, 21-unit preview label, 40%-height bounded preview, 318-unit result panels, 880-unit stacking and 22-unit status line. Real data differences are documented; they do not excuse control/layout mismatches.

## Accepted implementation constraints

The shipped app remains C99 + raylib + DuckDB. Original Carbon assets and bundled Plex fonts are reused; no browser, JavaScript UI, WebView, DOM, CSS runtime or new UI dependency is introduced. macOS native file panels remain real native panels. Source and result pages retain their existing ownership and limits.

The actual native field menu was compared with the browser at 1280 × 820, Home/full, selected matrix, Year/Quarter/Month and two measures. Native inspection exposed an eager filter field assignment that blanked the matrix while its editor was open, plus a fractional font-width footer ellipsis. Draft-until-Apply behavior and intrinsic footer fitting passed focused real-engine stages 400–425. The canonical date token menu was subsequently measured at 240 × 245, seven actions and a separator after the third action; its native migration remains next. The latest manual exit screenshot records an unsaved-changes dialog and is diagnostic evidence, not a matching-state authoring capture.

The latest authoring-control checkpoint passed focused native stages 426–457 (`author-controls-focused.txt`) and 39,419 pure checks per release/sanitizer configuration (`shelves-menu-pure.txt`). Marks now scrolls as a complete section and matches measured control geometry; axes wrap and move the preview correctly; the principal field and measure menus match the shared 240-unit anatomy. The opaque narrow-overlay regression is repaired and guarded by a native pixel check. Spatial keyboard navigation through wrapped tokens is included in the integration run underway. The analysis-object ellipsis still opens the older management menu and is the next measured UI difference to port. The implementation remains unfinished.

Focused source/canvas stages 474–509 and nested-scroll stages 311–318 pass after the newer 240-unit view/Text color/source action menus, exact title spacing, last-measure safeguards and independent scrollbar lanes. The complete 510-stage layout and related scenarios are running. The source value bar, source-selection return and remaining advanced menu grammar still need matching-state review. These are implementation items, not an external approval dependency.

## Accepted exact-control checkpoint — 17 October 2026

The coherent 510-stage native layout scenario passed (`author-controls-integrated.txt`), together with query 47, filter 66 and selection-statistics 31 stages: **654 native stages at this checkpoint**. The pure release and ASan/UBSan suites passed **39,471 assertions each** (`canvas-controls-final-pure.txt`); the separate real-GPU renderer probe passed **107 checks** (`canvas-controls-gpu.txt`). Source/binary hashes are retained in `canvas-controls-{source,binary}-sha256.txt`. This is a checkpoint, not completion of N4/N5 or a coherent run of all maintained scenarios.

The isolated signed native application was exercised with actual pointer input: independent preview scrolling, wheel chaining to the authoring canvas at its boundary, parent-thumb restore without resetting preview rows, preview-thumb restore, complete Marks scrolling, and the Matrix view menu. It closed cleanly without document edits. `live-canvas-controls-final.png` and `live-canvas-controls.txt` retain the framebuffer and run log: 3 worker jobs /11 SQL executions /1 count, draw-submission CPU p95 2.944 ms, paced mean 19.79 ms /p95 35.58 ms. These interactive local observations do not measure GPU execution or input latency. The user's separately running app was untouched.

The next source-control slice implements measured source-value-strip geometry and scalar-only source range/viewport bookmarks. Its new checks are underway; this newer source is not covered by the 510-stage checkpoint. Direct browser inspection confirmed the existing native chart palette is already correct: Inventory is red, Payroll gold; no palette change is warranted.

## Source navigation and value-strip checkpoint

`source-controls-focused.txt` passed the 22-stage range 500–521, including the new source controls and existing independent canvas lanes. The native source strip matches the browser's 88-unit address, 16-unit Function icon, 22-unit value box, 52-unit complete type label and 9-unit gaps. Below 700 viewport units it uses the 62-unit address and 5-unit gaps; below 620 it hides the type. Numeric data remains exact and true NULL/empty/literal text markers retain their existing renderer.

Browse restores the previous range, active cell and fractional source viewport through scalar `GridLocation` bookmarks scoped to the source ID/revision. Preview cell clicks still select their clicked row/column. Bookmarks never copy owned column arrays or borrowed format pointers; source changes reject stale row positions. Release and ASan/UBSan each pass **16,779 grid assertions**, including the 20 new geometry/bookmark checks (`source-controls-grid.txt`). Native CPU draw p95 was 4.621 ms in 673 instrumented frames, with 9 jobs/31 SQL executions/3 counts for fixture loading; the source-control interaction stages add no database work. The newer shared row/column/object menu migration is under separate validation.

## Shared native action-menu checkpoint

`shared-actions-focused.txt` passed **30 focused stages (510–539)** after the source value-strip and extra row/column/range/object menu migration. Pointer input inserted real columns and rows, one Undo restored each edit, source-row operations remained protected, keyboard traversal skipped disabled choices, and range/object Move cancellation retained the cached source/PIVOT and history. The 150% narrow menu was bounded. CPU draw-submission p95 was 2.112 ms across 841 instrumented frames; 9 jobs/31 SQL executions/3 counts came from fixture loading. This is not a controlled performance comparison.

The added bounded menu viewport passes **6,302 choice assertions per release/ASan/UBSan configuration** (`shared-actions-pure.txt`), including 240-unit headings/28-unit rows and scroll capacity for 1–16,384 items. `shared-actions-{source,binary}-sha256.txt` records that checkpoint. The subsequent Marks/step/worksheet menu pass is under native validation, including actual extra schema fields and independent enabled appearance versus opening-input suppression.

## Exact menu and source-control integration — 17 October 2026

The frozen author-menu binary (`author-menu-{source,binary}-sha256.txt`) passed the **complete 555-stage native layout scenario**, including the new source value strip, source bookmarks, structural/object menus, and Marks/step/worksheet menus. It ran 13,897 instrumented frames: CPU draw-submission p95 3.204 ms, paced mean 23.63 ms /p95 17.99 ms /max 1,377.84 ms, 92 jobs/336 SQL executions/37 counts across the full scenario. The full release and ASan/UBSan pure suites passed **40,575 assertions each** (`author-menu-full-pure.txt`); the independent GPU probe passed **107 checks**. The current all-scenario run uses that same frozen binary; its results are recorded incrementally in `author-menu-regression-summary.txt`. Do not claim all 988 stages passed before that runner completes.

The signed isolated QA bundle also passed real pointer/keyboard inspection of the exact eleven-row Marks menu and scrolling to the real extra Memo field, source field reveal, four-cell range drag and source/analysis return, worksheet context menu, and clean native Save/quit of only the disposable test workbook. It used 8 jobs/15 SQL executions/1 count including real selection statistics, a temporary analysis creation/Undo, and a save. Its observed CPU draw p95 was 2.349 ms and paced p95 17.01 ms; these are interactive observations, not controlled latency/GPU measurements. `live-author-menu-final.png` is its native framebuffer. The user's separate process remained running.

This manual comparison exposed the next two fidelity issues: dragging/Shift selection moves the native active cell to the endpoint, whereas the reference retains the starting active cell; and native button/menu focus rings appear after pointer activation even though the reference's actual `:focus-visible` state is false. Active-cell/extent separation and explicit keyboard focus visibility are now under implementation. These newer source changes are not covered by the frozen 555-stage checkpoint. Full exact UI acceptance remains open.
