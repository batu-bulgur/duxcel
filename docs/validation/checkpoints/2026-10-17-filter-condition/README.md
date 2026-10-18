# Filter Condition select integration

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

17 October 2026. Main native code `ed1249e` has the same tracked source/tests/build as worktree `138cb2c`; separate binary hashes remain in each run. This accepts the Condition control's anatomy and interactions, not complete UI or OS-popup pixel parity.

## Reference and native behavior

The frozen reference `design/prototype/src/ui.jsx:37` defines the Condition select; `styles.css` supplies its white surface, 11px type,7px horizontal padding,1px border,2px radius and25-unit height. The real DOM measurements (archived `reference/metrics.json`) show a196×25 field at wide100%, matching the native logical layout. The150% browser field is196.667×25 after browser pixel rounding. Reference captures use the existing private Vite build, with input/asset hashes (archived `reference/reference-inputs.json`); the prototype was unchanged.

Native `ui_select_field` now paints this style. Its option list reuses the bounded compact helper, actual field width,26-unit rows, current-value check and keyboard focus treatment. It retains the parent popup and typed filter semantics. An opening pointer cannot choose an overlapping clamped item; queued Down/Enter still works. Hidden options scroll within the existing bounded list. The parent filter remains driven by real available values and date hierarchy, whose content differs from the mock.

Shift+Tab leaves the child for the value list; forward Tab reaches the condition region. Escape closes one level and permits keyboard reopen. Native-command and queued Control-Tab use the same region controller and close the child before changing focus. Choosing a new condition retains both draft terms; Cancel preserves the applied predicate. Pointer selection at640×640/UI150% and actual numeric Between/text Contains application are included.

Review targets now use the same displayed rows/scroll offset and selected-state predicate as paint. The native reader retrieved the actual `filter.condition.option.equal` at150%. Current shipping binary absence checks found no review symbols/feature strings. No query, history mutation or page replacement is introduced by opening, traversing or canceling the control; native subphases assert request/cache/history retention. New production code adds no per-frame allocation or database call.

## Verification

| Gate | Result / elapsed | Evidence |
| --- | --- | --- |
| Current optimized shipping App |66 maintained stages;35.612s App /36.607s total;477 hidden-window checks|summary (archived `release/summary.json`), App log (archived `release/native-filter.txt`)|
| Current O1 ASan/UBSan App |Same66 stages;37.611s App /38.783s total;477 background checks|summary (archived `sanitize/summary.json`), App log (archived `sanitize/native-filter.txt`)|
| O1 ASan/UBSan native reporting |26 stages,2.351s App; two actual frame-bound reports retrieved|summary (archived `review/summary.json`), log (archived `review/native-review.txt`)|
| Integrated choiceview/select painters, release and ASan/UBSan |8,780 checks each;2.890s including1.702s build|summary (archived `helper/summary.json`)|
| Old style compatibility |2,048 before/after drawing/state scenarios identical; all6,302 prior geometry assertions retained|helper evidence (archived `helper-compatibility.json`)|
| Current optimized shipping versus current sanitizer review binary |Build contract + actual linked absence passed|absence record (archived `shipping-absence.json`)|

The extra filter subphases keep the existing24-frame pacing and original66 stage inventory. A compile attempt caught an incorrect test modifier type and was corrected before runtime acceptance. Read-only review caught the region/child ownership and missing captured check state; the final gates cover their revised source. No wait or assertion was removed to make the gate faster.

Native wide (archived `release/compact-condition-menu.png`), date (archived `release/condition-date.png`) and narrow (archived `release/condition-between-150.png`) captures were visually inspected. Browser screenshots do not capture OS select popup pixels, so the compact native list remains an intentional native presentation with unmeasured exact OS-popup paint. Full parent-popup sizing across differing domains, physical DPI/IME/gesture/responder behavior and complete N4/N5 acceptance remain open. Native-only multi-sort/source-query dialogs have no corresponding mock wizard screen and require their own functional/shared-control acceptance. [Record-following formatting](../../../design/SOURCE_FORMAT_IDENTITY.md) also remains unfinished; Page provenance does not complete it.

All automated windows were hidden and desktop-locked. The current executable is repository-root `./duxcel`; existing packaged bundles and older browser artifacts were not rebuilt by this checkpoint.
