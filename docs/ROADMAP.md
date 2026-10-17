# Delivery roadmap

Current plan, 17 October 2026. Deliver the exact reference UI in C99/raylib using the real engine. Preserve all earlier requests in the [requirements/history record](archive/ROADMAP-2026-10-17.md); the order below replaces the append-only execution log, not those requirements. [STATUS.md](STATUS.md) is the single capability/evidence matrix.

Latest priority decision: implement native agentic feedback first, then prove the shared C99/raylib UI in Wasm with the official DuckDB-Wasm Worker bridge. This supersedes the former blanket deferral of browser work. Existing correctness/UI/analytical backlog remains below. Section 0 records the development baseline; its remaining infrastructure items are not prerequisites unless they block these slices. Detailed execution/ownership is in [NATIVE_REVIEW_PLAN.md](design/NATIVE_REVIEW_PLAN.md).

## 0. Make development bounded and reproducible

- [x] Initialize Git, preserve the pre-audit project and classify source versus generated/local files.
- [x] Consolidate specifications, current plan/status and archived evidence under `docs/`; keep the executable prototype unchanged.
- [x] Audit independently for implementation quality, test runtime and safe ownership lanes.
- [x] Land and measure fast/affected/full test tiers, reproducible result manifests and a shared desktop-test lock. The warm affected headless gate took 0.452s; full module release/sanitizer checks passed. Native profile/full orchestration has its own scoped validation record.
- [x] Reduce completed native scenario idle frames without weakening assertions, animation waits or failure detection. Export fell from 19.153s to 10.542s in a matched local comparison; filter/query/document, combined scenarios and insufficient-budget failure passed.
- [x] Create automated native/GPU windows hidden and unfocused before creation; verify real framebuffer captures after resize and keep visible OS QA explicit. Hidden native gates and the 477-check Cocoa probe passed.
- [x] Introduce per-file application release/sanitizer and isolated fixture-helper objects/dependencies, atomic artifacts and coordinated icon generation. Build contract and cold/warm/widget measurements are recorded; module tests keep their existing independent commands.
- [ ] Lift blanket build serialization only after configuration/fixture ownership is demonstrably safe; preserve mutable-suite and desktop ordering. Extend incremental compilation to module tests without sharing renderer stubs or incompatible flags.
- [ ] Gradually move embedded native scenarios out of `src/app/app.c` behind a test-build boundary. First preserve exactly the same scenarios; avoid changing feature behavior during extraction.

Acceptance: a new checkout has documented build/test commands; a routine widget edit has a small credible gate; full regression is explicit, reproducible and associated with the tested binary. Two independent branches can integrate cleanly.

## 1. Native agentic review build — active priority

Latest follow-up: close the shared native/Wasm feedback loop before further UI refinement. The unused mock/Agentation services are stopped. Keep a single stable loopback Wasm review origin only when needed; automatic agents must remain external development tooling.

- [x] R1: Add isolated optimized and sanitizer review configurations/binaries with an explicit compile-time review feature. Exclude all added review code/data/commands/bridges from shipping outputs and verify that exclusion. Extend the existing incremental signatures; do not overwrite a binary under acceptance.
- [x] R2: Establish a portable C99 semantic widget registry, stable action/widget/data identities and versioned bounded capture schema. Track actual visible clips, layers, transforms, focus/enabled state and source metadata; resolve grid cells on demand.
- [x] R3: Integrate the in-app report command, picker, native note composer and same-frame screenshot/state bundle. Consume report input before ordinary dispatch, preserve open menus/selection/focus, retain notes after write failure and preserve worker/page ownership.
- [x] R4: Supply a durable atomic local outbox plus agent-facing reader/feedback lifecycle. Add an external IPC/MCP adapter where needed; native capture works while the service/agent is absent. Do not embed a web or model runtime in the app.
- [x] Accept the first complete reporting path on real native frames: open filter/popup, ribbon/overflow, panes and frozen grid; 100/150% scale; keyboard and pointer; worker reply/page eviction; capture failure/retry. Record bounded tracing and optimized overhead, and prove no new idle SQL, queries or per-frame serialization/readback.
- [x] R5: Automatically collect native bundles and upload Wasm reports into one durable project queue; survive offline browser reload, service restart, duplicate delivery and agent crashes. Add exclusive expiring agent leases, bounded read-only automatic triage and durable results without changing design/implementation/verification decisions. Actual hidden native/browser captures were automatically consumed by the installed Codex CLI; offline reload/retry and current shipping exclusion passed. No mock/Agentation process is required. [Acceptance and limits](validation/checkpoints/2026-10-17-unified-review/).
- [x] Fix review picking over the title/ribbon strip: include labels, Save, tabs, display/Help and blank areas, preserve disabled targets and actual tab clipping. Current37-stage native optimized/sanitizer and12real browser pointer selections pass. [Evidence](validation/checkpoints/2026-10-17-review-chrome/).

Current integration: the expanded37-stage optimized/sanitizer native gate passes, including title/ribbon targets, clipped tabs at150%, panes, narrow More/disabled history, queued query replies and nested drag deferral. Four actual bundles were validated and consumed by the reader with durable independent lifecycle state; the 477-check hidden-window probe passed. Registry/outbox release+sanitizer and current linked shipping absence passed. The optimized inactive/active/shipping frame-tail and RSS observations are recorded, including a fix for previously unmeasured frozen frames. Additional dialogs and authoring child controls remain partially enumerated; physical IME/gesture acceptance is separate. See [source/binary-scoped evidence](validation/checkpoints/2026-10-17-native-review/).

Acceptance: the user can report an actual native control without activating/dismissing it, receive an issue ID, and an agent can retrieve the correct target, frame/state and tested build. A library/helper or screenshot alone is incomplete. Shipping exclusion is a gate for this milestone; continuous video and general deterministic replay are not.

## 2. Shared C UI + official DuckDB-Wasm Worker proof

- [x] W1: Pin an isolated Emscripten/raylib web toolchain and official DuckDB-Wasm/browser-host dependencies. Build the same C99 UI into Wasm using a browser-scheduled update/draw lifecycle; package the existing fonts/icons. No second JavaScript spreadsheet UI.
- [x] W2: Connect real bounded source queries to the official asynchronous DuckDB-Wasm Worker through a thin typed bridge. Preserve C transformation/SQL definitions, request/reply identity, errors/cancellation, ownership and exact values. Start without experimental engine multithreading unless evidence requires it.
- [x] W3: Prove real source-file import, exact integer/DECIMAL/NULL/empty/literal text round-trip, paging/sort/filter, and UI responsiveness while a query is active. Validate platform seams rather than emulating Cocoa/absolute paths.
- [x] W4: Reuse the C registry/capture schema for browser review. Validate canvas CSS/backing/DPR transforms and review-only exports; shipping Wasm contains no review bridge/assets.
- [x] Record startup/download size, memory/copy costs, query/UI timing and supported browser/platform gaps; establish the [browser persistence adaptation plan](design/BROWSER_PERSISTENCE.md). Actual DPR/input and native/Worker file compatibility now have separate [evidence](validation/checkpoints/2026-10-17-browser-platform/); browser Save/Open integration remains incomplete.

W1–W4 now run the shared C App, official EH Worker and C reporting registry/capture/composer. The 33-stage real C bridge accepts typed source pages, available-value/date filtering, exact DECIMAL thresholds, errors/cancellation and stale callbacks. Actual CSV filter selection and the public 3,475,226-row Parquet work through the browser chooser; a real View frame renders before the pending source reply. Native filter and release/sanitizer session gates pass. Browser reports persist one frame-bound archive through the shared review queue and the existing reader retrieves its C target. Actual DPR1/1.25/2 backing and an inset CSS transform are accepted separately from engine semantics; broader reporting/input/GPU states remain open; PIVOT/totals/statistics/copy/export and workbook persistence are not claimed. [Source/filter evidence](validation/checkpoints/2026-10-17-browser-filters/) and [browser reporting evidence](validation/checkpoints/2026-10-17-browser-review/).

Acceptance: actual shared C UI and a real official Worker query run in the browser; agents can identify a captured C widget; exact types remain intact. This is an approved implementation proof with documented capability limits, not a mock calculation engine or a custom combined DuckDB build experiment.

## 3. Close current interaction correctness work

- [x] Finish bounded active-value snapshot integration, including source/sheet/query invalidation and off-page ownership. Do not query merely to repaint the value bar.
- [x] Accept active-cell/extent retention through pointer drag, Shift navigation, range reversal and Tab/Enter; update legacy native assumptions to the intended behavior.
- [x] Accept pointer versus keyboard focus against the reference at wide/narrow and 100/150% UI scales.
- [x] Rebase or invalidate source bookmarks when a table moves.
- [x] S1 Parquet foundation: carry bounded single-file raw Parquet record IDs through native Pages, retain business-only schema/grain, share deterministic positional ordering and reject observed changed-file pages. Real native10-stage and existing81-stage table/NYC gates, release/sanitizer engine/ownership checks and integrated paging measurements passed. [Evidence](validation/checkpoints/2026-10-17-record-pages/). CSV/demo IDs and source-version-bound async consumers remain open.
- [ ] Fix source-record formatting identity through sorting/filtering/query reindexing; define behavior when a query changes row identity. The [proposed scoped plan](design/SOURCE_FORMAT_IDENTITY.md) separates wrong-row containment, Page provenance, real record-following and verified persistence; the Parquet Page foundation is accepted separately, while the formatting feature remains incomplete.

Acceptance: focused real-app regressions with no added idle SQL/allocations, source/PIVOT cache identity preserved and typed NULL/exact values intact. These are discrete fixes, not an invitation to keep polishing without a completion boundary.

## 4. Finish native UI parity and refine through native feedback

Current user-directed refinements: unify the macOS traffic lights, logo, app name and document into one title strip; place ribbon display at its lower-right edge in Full/Compact/Icons modes; use light fading Alt keytips with reliable Escape dismissal. Preserve native window behavior and separate canvas/UI scale. Shared canvas overscroll must return quickly without momentum-tail sticking or packet-dependent resistance, expose a darker backing, and retain the displaced first-row/first-column boundaries. Hidden real-frame acceptance is required; physical trackpad/Space gestures remain separate QA.

Keep the implemented N1 ribbon mechanism, N2 workspace composition and N3 authoring core. Their completion never implied final visual sign-off. N4 shared controls and N5 integrated parity remain open.

- [x] User-directed chrome refinement: share one macOS native/C title strip, move ribbon display to the lower-right in Full/Compact/Icons, and replace black keytips with bounded light badges/140ms transitions and reliable Escape dismissal. Hidden native geometry/renderer/controller and real shared-C browser pixels pass; physical native window/shortcut delivery remains separate. [Evidence](validation/checkpoints/2026-10-17-chrome-motion/).
- [x] Replace generic filter-pill labels with bounded cached actual criteria/values.
- [x] Match reserved collapsed-ribbon geometry, reference group icons/type and Alignment retention; keep ribbon/status live above narrow drawers. The 18-stage `ribbon-collapse` gate through `09ce3de` accepts flyouts, Escape focus and resizing without idle SQL/cache changes.
- [x] Port the 240-unit More ribbon menu, hidden-group rows/separator, history-only narrow Icons state and real Undo/Redo; preserve More anchors, Escape/reopen and leaving historical preview before history. Require the actual completed native stage range in acceptance.
- [x] Integrate compact Representation/Plot/font/number lists at their actual field widths; unify pointer/keyboard anchors and retain the parent ribbon flyout. The expanded24-stage native select gate passed in optimized/sanitizer builds, with18 adjacent sanitizer ribbon stages and no select-only query/cache changes. [Evidence and unmeasured OS-popup scope](validation/checkpoints/2026-10-17-select-popups/).
- [x] Port the filter Condition field and compact options, including selected checks, date labels, backward Tab, Control-Tab region ownership, Escape/reopen and 150% pointer selection with retained drafts. Current optimized/sanitizer66-stage and review26 gates pass. [Evidence](validation/checkpoints/2026-10-17-filter-condition/).
- [ ] Finish remaining dropdown and inspector/menu/focus parity using measured reference anatomy and real/explicitly disabled actions. Native-only multi-sort/query wizards have no exact reference screen; accept their shared control/focus behavior separately. Exact OS select-popup pixels and complete parent-flyout state acceptance are not covered by the compact-list integration; the intentional53-unit Font flyout field is verified separately from the48-unit main-ribbon field.
- [ ] Close remaining inspector/menu/focus differences using fixed reference states rather than another design direction.
- [ ] Record matching native/browser captures and working pointer/keyboard checks for: browsing; selected matrix with compact shelves/Marks; chart; filters/date hierarchy; Transformations/SQL/history preview; Full/Compact/Icons/Focus ribbon; narrow/short windows and 150% scale.

Acceptance: every state has a reference, viewport, fixture, native capture, interaction checklist and explicit remaining exception. No unresolved UI mismatch is hidden by claiming an engine test count. The prototype's token-only Marks behavior does **not** require finishing all future analytical semantics before this milestone can close; native controls must honestly describe their supported behavior.

## 5. Complete browser platform, workbook and delivery adapters

User-reported issues, 17 October: blurry Wasm rendering and trackpad panning now have implemented platform fixes and focused real-app acceptance. Physical gesture feel remains a separate gate.

- [x] Use bounded real DPR-sized canvas/render targets and density-sized fonts, with synchronized logical projection/scissors and preserved CSS embedding. Actual DPR1/1.25/2 transitions, pointer transforms, independent UI/grid scale and frame-bound Retina reports pass. [Captures and scope](validation/checkpoints/2026-10-17-browser-platform/).
- [x] Preserve accumulated fractional two-axis browser pan before GLFW quantization, normalize pixel/line/page modes, separate Ctrl-wheel canvas zoom, and retain pointer-region ownership. Real C movement, reversals, Shift, trusted browser wheel delivery, ribbon/sidebar isolation and zero pan/DPR queries pass; native selection remains accepted.
- [x] Fix sticky/jittery elastic return under tiny trackpad tails; use packet-invariant resistance, darker exposed backing and contrasting displaced row1/A boundaries. Synthetic real-C GPU return/pixels, canonical-position retention and pointer-owned column growth pass. [Evidence and physical-input limits](validation/checkpoints/2026-10-17-chrome-motion/).
- [ ] Accept physical trackpad momentum/pinch feel, Safari nonstandard gestures and physical browser zoom/display changes. Synthetic DPR transitions and hidden input do not close this gate or broad shelf/menu gesture coverage.
- [x] BP0 file foundation: real native v12 metadata with object IDs above2^53 is rewritten by the pinned official Worker into a registered valid database copy, exported/reopened and accepted by native `workbook_load` in release/ASan. Exact typed sentinels pass. Blank-file creation/export, C codec/controller integration, source relink and OPFS remain open; this is not the browser Save feature.

First focused input fix accepted: the thin host now claims unmodified/Shift canvas navigation without blocking GLFW/C input. The old default-leak control fails as expected; two manual flows and transformed actual reporting pass on the same normal Wasm. One owned Chromium crash reached Cocoa menu recursion; broader modified-key/IME/stability coverage remains open. [Evidence](validation/checkpoints/2026-10-17-browser-navigation/).

- [ ] Finish asynchronous file import/export/relinking and `.dxls` persistence/migration behavior using the selected Worker backend; account for existing direct DuckDB C API use in workbook persistence.
- [ ] Resolve the installed headless Chromium150/macOS trusted Alt menu-routing crash; the focused canvas bare-Alt default guard and actual DOM→GLFW→C hint/fade checks pass, but the guard did not eliminate the native browser crash. Preserve that explicit acceptance boundary.
- [ ] Accept browser input/composition/focus/clipboard, GPU/render-target/clip behavior, context loss and DPI/zoom across the target browser matrix. Keep Linux/Windows portability at platform boundaries without claiming unperformed OS validation.
- [ ] Validate OPFS recovery, quota/failure handling and explicit portable workbook export. Pin compatible engine/worker/extensions and deployment headers/assets; add engine multithreading only with measured need and compatible hosting.
- [ ] Define and accept production accessibility separately from the review registry; keep product shipping builds independent of development feedback infrastructure.

Acceptance: real browser file/workbook and analytical workflows, native/browser parity evidence with explicit differences, bounded resource behavior and clean production artifacts. This work follows the smaller proof and does not block native reporting.

## 6. Complete analytical behavior in separate features

- [ ] Aggregate-aware chart Other for AVG/distinct using real source aggregation.
- [ ] Marks query-grain support for fields outside the current result; bounded continuous color, persistent labels, multiple bindings and matrix/legacy behavior.
- [ ] Typed manual cells/formulas and explicit imported-data overlay model, with clear read-only source boundaries.
- [ ] Further transforms/joins/table nesting, recovery and distribution work as prioritized from the preserved requirements. Each needs a narrow design/acceptance definition before implementation.

Acceptance: correctness/ownership/persistence tests plus the smallest applicable native workflow. Reuse the accepted modern UI rather than opening another UI redesign.

## Working method

Implement the [approved native review workflow](design/NATIVE_REVIEW_PLAN.md) before further broad GUI refinement. The existing [browser-reference review sidecar](design/FEEDBACK.md) remains available while that work lands. Preserve issue/target provenance and distinguish mock defects, intended design changes and native gaps. Preserve the read-only reference until an accepted design revision is deliberately promoted; native acceptance stays separate from resolving a browser note.

Use two independent implementation lanes initially, with one owner for App/shared integration and serial desktop testing. Review small branch commits, run affected checks, integrate, then broaden at milestone boundaries. Reopen earlier checks only for changes or evidence that justify them. Keep a per-feature distinction between UI parity, working interaction and analytical completeness. macOS remains the native acceptance host; Linux/Windows ports retain separate validation. Browser toolchain/dependency work is authorized for the Wasm milestone and stays isolated from native production dependencies.
