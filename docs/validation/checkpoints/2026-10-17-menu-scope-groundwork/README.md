# Action menus and owned formatting groundwork

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

17 October 2026. Runtime integration `22e019d` combines reviewed worktree commits for action-menu paint and the format model. `8cb2718` and `5d7fceb` add only focused test entry points. The review-only drag follow-up has its own final acceptance below. Provenance (archived `provenance.json`) records exact artifact hashes and individual source scopes. This is focused integration, not a full regression or complete UI-port sign-off.

## Visible behavior

The shared C action-menu row now follows the frozen reference `design/prototype/src/ui.jsx` and `.action-menu` rules in `styles.css`: borderless hover/press fills, independent blue icon ink,11-unit shortcuts and clipped content/focus. Disabled rows stay inert without acquiring a focus outline. Existing geometry and actions remain intact.

File uses the same menu anatomy. It preserves the full-height separator when space permits, then uses a bounded scrolling command list in short windows. Up/Down/Tab wrap across available commands; Home/End reveal the first/last available command; Enter/Space activate it. Keyboard navigation owns its frame over wheel/drag input. Background controls are blocked; only an uncovered browser brand anchor can toggle File. An unrenderable tiny menu dismisses instead of remaining an invisible input owner. Reopening clears obsolete scroll/drag state.

Actual shared-C browser acceptance uses trusted keys and wheel input at DPR2 with an inset/resized CSS canvas. It covers disabled Save/SaveAs/Quit skipping, reverse Tab, short-window reveal/wheel scrolling, Space Close/Cancel, a heading overlapping the brand, outside Help/grid clicks, tiny geometry and no extra database jobs. Short File capture (archived `file-menu-short.png`) and owned state/input evidence (archived `browser-access-keys.json`) retain the actual menu. The66-check run took 14.001 s; shipping Wasm12 took 7.075 s. These do not claim physical hardware/OS gesture acceptance or report-upload acceptance.

The hidden native sanitizer authoring profile passed all 15 stages at 100/150%, covering actual Marks/step/worksheet menus, disabled navigation, clipped scrolling and retained source/PIVOT caches with no extra SQL. See native step actions (archived `native-step-actions-disabled.png`) and worksheet actions at150% (archived `native-sheet-actions-150.png`). File is an extension using reference menu anatomy; the prototype does not define a separate File screen. Exact OS select-popup paint and the full inspector/menu state matrix remain unfinished.

## Formatting model and save boundary

The next correctness issue remains live: coordinate styles can attach to a different imported record after sorting/filtering. This change does **not** switch normal App formatting to new selectors or claim record-following.

`format.c` now owns deduplicated exact definition bytes and up to 33 guards per patch. Manual/blank, exact-result and guarded legacy eligibility share newest-mask precedence. Imported resolver calls reject unguarded finite coordinate patches; broad spatial column defaults remain eligible. Resolving/looking up a prepared token allocates nothing and performs no SQL/filesystem work. Deep snapshots, copy/free, Undo/Redo, moves and axis plans preserve owned guards. Current state is bounded to 1 MiB and current-plus-history to 32 MiB **per store**; aggregate workbook reservations remain open. Pure tests include an independent overlapping-command oracle, exact owner/definition isolation, full512-patch migration and atomic byte/history failures.

Native workbook Save independently rejects scoped payloads before opening/touching a destination, and the App performs an earlier preflight before changing preview/worker state. Real persistence tests prove existing-file bytes remain unchanged, no new destination appears, snapshots/duplicates own separate payloads, ordinary v12 formatting still round-trips, and dormant session history does not block an ordinary save. Historical preview copies only spatial whole-column defaults, avoiding shallow guard-pointer ownership.

Canonical App scope production, schema/source-version verification, live rendering/suspension, record selection collection, object-local analytical semantics and transactional migration/persistence remain open. Enabling session-only guards would currently break successful Save/Open workflows; serializing only coordinates would lose the guard. The [current plan](../../../design/SOURCE_FORMAT_IDENTITY.md) therefore keeps S0 runtime integration and S2 record-following incomplete.

## Gates and iteration cost

| Gate | Accepted scope | Elapsed |
| --- | --- | ---: |
| Pure release + ASan/UBSan |27,128 format,8,875 choice,288 preview and34,783 grid checks/configuration |28.365s including23.803s build |
| Core then workbook, both configurations |1,661 core +1,707 workbook checks/configuration; existing mutable fixture ordering retained |78.779s including17.753s build |
| Native authoring, sanitizer |15 actual application stages +477 hidden-window checks |9.824s app /11.522s runner |
| Native document, sanitizer |10 stages +hidden-window probe |5.006s app /5.958s runner |
| Native optimized base |6 real engine/application stages +hidden-window probe |3.587s app /4.508s runner |
| Native reporting, sanitizer |37 stages +hidden-window probe; individual manifest retains scope |Passed |
| Browser review / shipping |66 transformed Retina menu/access checks /12 shipping checks |14.001s /7.075s |
| Focused workbook formatting |383 existing format/save checks per configuration |1.413s release /4.283s ASan;14.515s including rebuild |
| `formats` runner, warm |Same383 checks/configuration under worktree lock with source/binary manifests |5.013s total |
| Runner Python tests |27 tests, including stale whole-suite summary rejection and failure/binary ownership |1.939s |

The full ASan workbook suite took 41.326 s. The new `python3 tools/test.py formats --config both` deliberately runs only its existing formatting suites, without regenerating unrelated core fixtures. This is a smaller iteration workload, not a measured engine speedup. It retains failure detection, creates its owned fixture directory, runs sequentially, rejects unrelated/parallel options, and requires its focused positive completion summary. Full ordering is unchanged. No assertion or animation wait was removed.

Final review-only follow-up `07a30e7` adds File's active scrollbar to capture deferral; an inactive menu's stale drag flag cannot deadlock reporting. The existing native controller gate now asserts both cases and passes all 37 stages in 2.967 s plus a 0.893 s hidden-window probe. The66-check transformed Retina browser gate passes again in 13.725 s on the final review artifact. Production code/binaries remain scoped to `22e019d`. These checks do not claim complete File-menu semantic target enumeration or physical scrollbar/report gestures.

The first attempted native runner was correctly rejected while the same worktree's workbook gate held its lock; acceptance was rerun after that gate completed. Two implementation agents used separate managed worktrees and explicit file ownership, with at most two compiler/engine processes. Root alone owned all hidden native/headless browser execution. Recoverable archiving was requested through the app for both completed worktrees after commits/evidence were integrated; no manual removal or force reset was used.

The signed `build/Duxcel.app` contains the accepted optimized native binary. One stable review listener remains at `http://127.0.0.1:8767/`, preserving the persistent queue/outbox. Served Wasm/HTML hashes match the accepted artifacts. No user app or browser tab was activated, reloaded or terminated. Production isolation checks pass. Additional review target enumeration, physical trackpad/Space/IME behavior, exact full UI parity and verified record formatting remain separately scoped work.
