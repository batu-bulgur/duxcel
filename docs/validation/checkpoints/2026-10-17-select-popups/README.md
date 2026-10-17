# Compact native select menus

17 October 2026. Production remains C99/raylib/DuckDB; the prototype is unchanged.

## Accepted implementation

`cbf16b3` adds the allocation-free select-list painter. `76c1a4f` integrates font/number fields with their actual ribbon/flyout widths, selected checks, opening-click protection and keyboard focus/history. `8ae75b2` integrates Representation/Plot, a common pointer/keyboard opener, post-layout anchoring after drawer reveal/resize, and the visible passive ribbon flyout behind a child formatting menu. Parent and child share one bounded capture rectangle; only the child receives input. Existing source calculations and field-assignment/action menus retain their behavior.

The final sanitizer run records dirty `76c1a4f`; its exact three changed source files were committed as `8ae75b2`. The release run records `fd7d394`, which adds only the proposed source-format identity document to that same C code. Each summary records actual source/binary hashes and unchanged before/after provenance. No whole-suite or newer-source acceptance is implied.

## Reference and visual scope

The reference's `Ribbon.jsx` defines six font sizes (11–16) in a 48×26 field and the Accounting/Number select. `Chrome.jsx` defines Matrix/Bar chart/Line chart, and Plot in measure order. Representation width follows the sidebar; Plot uses the remaining row width and 10.5px type.

An owned headless Chromium capture of the existing prototype measured the font field at `(241.71875,61,48,26)`, value13, padding2px3px, border1px/radius2px. See [reference metrics](reference/metrics.json), [closed reference](reference/closed.png) and [open-attempt capture](reference/open.png). Vite wrote to a private ignored build directory, without editing the reference. The browser's OS option popup was absent from page screenshots, including while pressed; **open-OS popup pixel parity has not been measured**. Native 26-unit rows, selected check and bounded panel are the compact native presentation, not a claim about those unobserved OS pixels.

The real native font/number lists no longer contain the extra title, divider or action icons. Real native Representation/Plot lists follow their own fields rather than fixed280/244-unit menus. Captures at wide100% and narrow150% were visually inspected; the final font capture shows the actual parent flyout remains visible. The older collapsed Font flyout's field remains53 units rather than the reference48; its wider family field, parent sizing and further group geometry are still N1/N5 work. Full UI parity remains open.

## Working interactions and semantics

The expanded existing **24-stage [596,620) profile** exercises real pointer and keyboard opening, Escape/reopen, ordered Down/End+Enter, actual font13→14 and pointer16 selection, Undo/Redo, Number format, and narrow150% Representation/Plot/flyout behavior. It checks parent focus in actual GPU pixels, unchanged source/PIVOT cell pointers, unchanged request IDs, history action counts, selected measure and renderer ownership/clip state. Extra phases preserve the original24-frame pacing and animation waits; the stage inventory remains1,098, not a count of individual assertions.

Formatting is still coordinate-based. These controls do not fix record formatting through sort/filter, add chart grain, or complete the dialogs. Source/PIVOT computation remains real; no query is added by these presentation actions. The final sanitizer flow reports10 worker jobs/43 SQL executions/4 count queries for its whole fixture/filter bootstrap; its select-only stages assert the request/cache remains unchanged.

## Focused verification

| Scope | App time | Total | Evidence |
| --- | ---: | ---: | --- |
| Font/Number first integration, O1 ASan/UBSan |17.591s|43.299s including23.781s build|[summary](font-number/summary.json) |
| Final four selects + visible parent, O1 ASan/UBSan |20.533s|47.511s including24.853s build|[summary](authoring/summary.json), [app log](authoring/layout-criteria-selects.txt) |
| Adjacent collapsed-ribbon18 stages, same sanitizer binary |10.129s|11.329s, deliberate no-build|[summary](ribbon-adjacent/summary.json) |
| Final four selects, optimized shipping `duxcel` |19.751s|24.366s with warm App build|[summary](release/summary.json), [app log](release/layout-criteria-selects.txt) |

Final gates retain **6,302 choice-menu +575 formatting checks per configuration** and the **477-check hidden-window probe** before each native run. Forty-two unique native stages passed on the sanitizer binary; the24 select stages additionally passed on the optimized binary. Final sanitizer draw-submission p95 was3.807ms, excluding swap/pacing; this is not input latency or a performance comparison. The first expanded attempts exposed a test-input delivery mistake and an incorrect assumption that the existing narrow field was48 units; those failed runs remain under ignored test results and are not acceptance.

Commands:

```sh
make test-affected FEATURE=criteria-selects TEST_ARGS='--modules choiceview format --config sanitize'
make test-affected FEATURE=ribbon-collapse TEST_ARGS='--modules choiceview format --config sanitize --no-build'
make duxcel
make test-affected FEATURE=criteria-selects TEST_ARGS='--modules choiceview format --config release'
```

All native windows were created hidden/unfocused through the desktop-locked runner. No user's running app was touched; physical OS shortcuts, IME, file panels, trackpad/Spaces and broad DPI remain separate QA. Both current `./duxcel` and `./duxcel-debug` contain these changes. Other application bundles and earlier browser outputs are not covered by this checkpoint.
