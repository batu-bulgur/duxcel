> Historical checkpoint record, archived 17 October 2026. Current status: [../STATUS.md](../STATUS.md). Earlier “running,” “next,” and completion wording describes its original checkpoint.

# Native UI implementation plan

17 October 2026. The user explicitly clarified: **the web app interface is the design definition; implement that exact UI in raylib and C99.** This supersedes both the prototype's earlier wait-for-handoff requirement and latitude to reinterpret its native visual design. `design/prototype/AGENTS.md` still governs edits inside the prototype; this implementation leaves it untouched.

## Reference and ownership

Read the **Explore Duxcel UI directions** chat, ID `01a14246-7f48-7151-b6a4-1b090574a996`, including the user's IBM Plex Sans, vertical navigation, spatial shelves, thin status, pane lifecycle and responsive ribbon decisions. Use [DESIGN.md](../design/DESIGN.md), [INTERACTIONS.md](../design/INTERACTIONS.md), [prototype controls](../../design/prototype/README.md), and its final screenshots. These sources describe one evolving design, not a second implementation to maintain. Keep the approved native brand assets.

The existing C99/raylib renderer, worker and document model remain authoritative. Implement small reusable geometry/widgets under `src/ui`; keep source ownership, query dispatch, history and workbook actions in their existing controllers. Use bundled fonts and current build tools. No JavaScript, DOM, CSS, WebView, browser engine or new UI dependency enters the shipped application.

Acceptance must compare the same state and viewport against the actual web app. Match its proportions, font metrics, controls, palette, icon assets and interactions; record any remaining mismatch as unfinished work, not an acceptable stylistic substitution. Use real native source and analytical state where the demo uses synthetic calculations. A functional engine result is not evidence of visual fidelity.

## Ordered delivery

### N1 — Responsive commands

Replace hard-coded horizontally scrolling command positions with measured native groups. Full mode explains groups; compact mode uses icon-plus-name controls; icons mode has compact consistent targets. Collapse lower-priority groups first, then put whole named groups in overflow. Every group opens its complete controls. Keep Save and ribbon display reachable. Preserve stable command keytips and shortcuts; keyboard focus must reveal collapsed/overflowed commands. Opening, cancelling and resizing a flyout must not dispatch queries or lose worksheet context. The canonical Home/Insert/Data/Analyze/View tabs and contextual Visualization tab replace the earlier File/Data/View surface. Document actions remain in the native File menu and title Save control. Unmigrated formatting actions stay explicitly disabled until their real behavior is implemented; this is unfinished parity, not a completed formatting ribbon.

### N2 — Workspace regions

Match the vertical dock's section hierarchy and row density. Workbook and Fields retain independent visibility, collapse and scrolling. Below 700 logical units, recover the dock as an explicit overlay rather than consuming half the canvas. Transformations overlays below 1040; collapse retains a small entry, dismissal removes the entry and all reserved width. Opening a pane may reveal the selected object; closing preserves the viewport. The 22-unit status bar fits complete metric labels, retains hidden metrics in customization, and separates UI scale from canvas zoom.

### N3 — Authoring anatomy

Two 31-unit horizontal shelves, Columns then Rows, follow the 26-unit context title. Measures, Marks and view filters belong in the contextual left area. Retain multi-field assignment, token reorder/removal, keyboard alternatives, real source binding and real DuckDB PIVOT. Preserve independent source tables, matrices and charts on one worksheet. Unimplemented Marks encodings must not pretend to affect charts.

### N4 — Shared widgets and flows

Unify popovers, dropdown/checklists, tabbed inspectors, splitters and wizard/dialog anatomy around bounded geometry, outlined actionable controls, consistent icons, clear active/disabled/focus states and source scope. Reuse real native file panels. Preserve UTF-8 editor ownership, typed NULL distinction and existing undo. Motion uses the study's 90/110/120/140/160 ms timings where useful; resizing follows the pointer immediately and reduced motion removes transitions.

### N5 — Acceptance and return to feature work

For each delivered slice record native screenshots and limitations separately from browser evidence. Cover wide and narrow windows, 80/100/150% UI scale, independent canvas zoom, pointer and keyboard access, cancellation, focus restoration, document dirty state and cache/query stability. Use release and ASan/UBSan tests appropriate to changed shared code. Measure native draw submission separately from paced frames, database time and native dialog waits. After the integrated shell and authoring flows work, resume typed manual cells, formats, source overlays and remaining data features in ROADMAP order. No Windows/Linux native validation is requested.

## Component mapping

| Study component | Native implementation |
| --- | --- |
| Ribbon group/launcher/overflow | `src/ui/ribbon.*` geometry + existing application command handlers |
| Workbook / Fields | existing `draw_workbook_section`, `draw_sidebar`, field matching and navigation |
| Context / shelves / Marks | existing selected object and pivot definition controllers |
| Transformations inspector | existing source-scoped pipeline and SQL/query editors |
| Thin status / customization | existing typed selection statistics worker and preference mask |
| Filter menu | existing bounded DuckDB distinct/date pages and condition editor |
| Grid / matrix / chart | existing raylib grid and anchored objects, not a web recreation |

## Progress

- The browser app is the exact specification. Its original IBM Carbon artwork is bundled in one PNG atlas with the original Apache 2.0 license. No web runtime or additional runtime dependency is used.
- N1's measured/cached group degradation, named overflow, keyboard traversal and shared command dispatch remain in place. N4 now supplies Home/Insert/Data/Analyze/View and contextual Visualization, the 28-unit title and 28-unit tab rows, the Home stacked groups, real Save and history controls. Native range formatting and first-two-source-column Freeze are implemented and tested. Font family and Wrap retain the definition’s read-only/disabled behavior.
- N2 implementation includes separate Workbook/Fields visibility, a recoverable left overlay below 700 logical units, source field reveal/dismissal, right inspector precedence, collapse/dismiss states, a 22-unit status bar and separate UI scale. Native minimum remains 640 × 640; 150% UI exercises approximately 427 logical units. Short Fields sections omit their footer to retain at least one field row. The expanded 172-stage layout and 47-stage query scenarios passed; later matrix border/column/selection refinements also passed their 172+47 native follow-up.
- N3 core implementation includes compact Columns/Rows, left Measures/Marks/View filters, searchable Add menus, actual aggregation/token menus, pointer and keyboard assignment/reorder/removal, and matrix/chart conversion preserving the object/query identity and result cache. Complete queued drags resolve only after destination geometry exists. A native pointer session exercised a City filter excluding Bursa, unchanged source data, narrow overlays, representation change and Undo.
- N4 now has a source preview above paired matrix/chart views. The preview borrows up to 12 cached source rows, retains explicit NULL/empty-text distinctions, shares persisted column widths, opens real source distinct filters, and returns to the source on a data-cell click. Matrix and chart share one real result; no companion query or duplicate object is created. The views stack below 880 logical units and retain their scrollable canvas. Matrix keyboard selection maps back to actual worksheet cells.
- Native SQL inspection now uses the right pane and owns its text separately from modal SQL snapshots. Source identity/revision changes rebuild the snapshot; idle rendering issues no query.
- Release and sanitizer pure checks passed 9,382 assertions per configuration at the formatting/Plot/Freeze checkpoint. Native layout acceptance passed 172 stages plus 47 source-query stages. Later matrix fidelity and independent canvas/chrome sizing refinements passed separate native follow-ups. See [validation](NATIVE-UI-VALIDATION-2026-10-17.md) for checkpoint-specific evidence.
- **Next:** complete six Marks encodings and aggregate-aware chart Other, inspector/shared popup/dialog fidelity, motion/focus and integrated visual acceptance. Matrix readout totals are now implemented and their native layout acceptance passed. The full exact UI port is not complete.

## Latest shared-control progress

The column filter now uses the definition's 288-unit anatomy, actual checklist/search, condition dropdown and compact footer. Values and a condition combine in the real engine; inclusive Between retains exact numeric normalization and excludes NULL. Search-scoped Select all collects up to 4,096 keys through bounded cached/worker pages and applies atomically. The expanded 66-stage native filter run passed, including a clamped text-menu opening press followed by queued Down/Enter. A live canonical sample selected three IDs plus Contains 1043, produced one source transaction and one real PIVOT group, then Undo restored 240/30. No prototype files were changed.

The six native Marks targets accept owned ordered field bindings through a compact field menu, pointer drops and keyboard controls; Size/Shape replace one field while the other roles retain up to eight. Assignments share object history and version-12 persistence, survive representation changes and have a separate presentation revision so they do not invalidate result or selection-statistics identity. The 192-stage native acceptance and related 497-stage run passed. The subsequent 208+33 native follow-up passed cached Row colors, numeric sizes, line shapes and typed assigned tooltip readouts. Fields outside the existing Rows/Columns/Measures result, continuous color scales, persistent mark text and matrix/legacy encodings remain unfinished. Inactive bindings are labelled.

Matrix Total columns and a pinned Grand total footer use source-derived DuckDB [GROUPING SETS](https://duckdb.org/docs/current/sql/query_syntax/grouping_sets) rather than summing rendered child aggregates. Owned bounded readouts retain NULL/empty/literal distinctions and exact decimal text, while the ordinary PIVOT schema, worksheet placement, export and selection-statistics count remain unchanged. Clicking a total readout selects the view frame, matching the definition's object selection. The expanded 216-stage layout acceptance and seven related scenarios passed (511 stages). Aggregate readouts are not ordinary editable worksheet cells.

The native Transformations inspector now matches the numbered rows, selected-step properties and fixed footer; real step menus, editing/reordering and independently owned wrapped SQL passed a 232-stage layout run plus query/unique/filter regressions (377 stages). Historical previews use an independent read-only grid and real owned prefix results. The statistics/guide focused stages 250–275 passed with real filtered and historical aggregates, 150% narrow bounds and keyboard recovery. All 13,430 pure assertions passed in release and ASan/UBSan at the typography checkpoint. Full 276-stage integration after exact pane/ribbon timing is running.

Font sizing now uses the bundled faces’ actual em metrics and cached fractional advances, so native wrapping matches the definition. The Command glyph falls back to the existing Mono atlas. The new shared finite-duration easing helper has 1,791 release/sanitizer assertions; right-pane width and ribbon indicator now use the definition’s 160 ms curve. Other transition/opacity/content-translation work remains pending.


The full 357-stage layout run passed after the nine-action field menu, real source/pivot/filter/encoding actions and Undo. The complete pure checkpoint passed 26,210 checks per release/sanitizer configuration. The subsequent 118-stage focused pane run (250–367) passed stable inner widths, exact finite container timing, 10/12-unit translations, immediate resizing, reduced motion and narrow recovery. Workbook disclosure-height motion is implemented and under test. True composited opacity and remaining popup motion/focus details remain open; keep the full parity milestone open.

The existing rlgl renderer now rotates eight preallocated buffers. Two matched release repeat pairs reduced draw-submission CPU p95 from 11.738/14.127 ms to 1.931/1.852 ms, with byte-identical screenshots in all four runs. This measures CPU submission under this local workload, not GPU execution, input latency or a guaranteed speedup. No dependency or query architecture changed.

Workbook disclosure passed its 129-stage focused native checkpoint, including inert hidden controls, interrupted reopening and reduced-motion snapping. Matching-state native/browser inspection then identified compact sheet flex sizing, token padding and modal ribbon appearance differences; those are implemented and under test. The animation clock now distinguishes time before a new target from time inside that transition, and ribbon indicator geometry uses logical units independently of UI scale.

The subsequent **full 388-stage native layout and 27,143-check pure release/sanitizer checkpoint passed**. The Fields heading now has the reference's inert label behavior, and the left splitter/preferences accept its actual 188–360-unit range. The next slice adds true native group-opacity compositing, with a separate GPU pixel/state/ownership probe before application integration.

For remaining opacity work, preserve actual group compositing. Multiplying each primitive's alpha changes overlapping backgrounds/text and does not reproduce the definition's group opacity. Any cached native paint layer must have bounded owned resources and verified DPI/scissor restoration before this item is accepted. The isolated renderer probe passed 97 native checks, and the focused 43-stage application run (357–399) passed nested pane/Workbook opacity, framebuffer and clipping restoration, 150% overlay recovery, a 32 MiB retained color-only cache budget, and unchanged source queries. Full integration is being checked separately. No additional shader or runtime dependency is used.

The complete **400-stage** native layout checkpoint passed after group-opacity integration, with **28,832 pure checks per release/sanitizer configuration**. Dedicated popup cache slots now have an isolated **104-check** native GPU acceptance; menu controller integration is under test. Closing menus must retain a complete image independently of freed draft state, and input coordinates must follow their translation.

Popup timing/ownership passed focused stages 400–411, and contextual shelves passed 412–415. The updated shared-control checkpoint passed **29,938 pure checks per release/sanitizer configuration** (`popup-full-pure.txt`); the native GPU probe passed **107 checks**, including cursor hit positions using actual Plex advances at 80/100/150%. A subsequent real-app comparison exposed eager view-filter assignment and a fractional-width footer ellipsis. Draft-until-Apply behavior and intrinsic footer width are implemented and under real-engine acceptance.

## Exact authoring controls — current pass

The browser's complete Marks section scrolls, including its heading. Native geometry now matches that behavior, scrollbar reservation, intrinsic badges/targets and filter wrapping. The measured 427-logical-unit Columns shelf is 58 units high with two lines; Rows is 30, giving a 115-unit context including title and bottom border. Add field wraps with the tokens. The preview derives its size from the complete workspace rather than assuming an 88-unit context.

Date and measure tokens now use 240-unit headed action menus. Measure choices drive actual SUM/COUNT/AVG definitions, suppress duplicate COUNT and retain the last measure. Keyboard Enter opens without immediately activating the first item; arrows skip disabled choices. Existing advanced engine aggregates remain available through their existing keyboard commands. The compact field menu exposes additional real columns through scrolling and optional slash search instead of permanently allocating a large search panel.

Focused native stages 426–457 passed, including actual AVG PIVOT/Undo, the transparent drawer pixel regression, 150% narrow controls, and unchanged caches/history on cancellation. The broader integration after spatial wrapped-token keyboard navigation is running. Next shared-control difference: analysis-object ellipsis must open the definition's Matrix/Bar/Line/Return-to-data menu. Full Marks semantics and final visual acceptance remain open.

The next measured controls are implemented: Matrix/Chart ellipsis opens the definition's four actions, Text color uses its four-choice menu, and ordinary source cells expose Copy /Sort /Filter /Create analysis /Inspect SQL. These use existing real worker/history/format commands. Wide/narrow context title spacing, vertical field-action icons, last-measure safeguards and separate outer-dock/Marks lanes are also implemented. Focused source/canvas stages 474–509 and nested-scroll stages 311–318 passed. The analysis canvas now reserves the definition's nine-unit scroll lane, leaving the preview its own scrollbar; pointer updates arrange content during the same frame. The browser's 1280×820 analysis content width is 763 units, with a 739-unit matrix ending at x979.

Remaining fidelity review includes the source value bar (88-unit wide /62-unit compact address control, 22-unit value field, complete type label), source selection restoration when returning from authoring, and consistency of the additional native structural/object management menus. Continue matching rendered states before calling N4/N5 complete; broader engine features stay behind the modern-UI migration.

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
