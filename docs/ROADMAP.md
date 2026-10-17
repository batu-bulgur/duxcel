# Delivery roadmap

Current plan, 17 October 2026. Deliver the exact reference UI in C99/raylib using the real engine. Preserve all earlier requests in the [requirements/history record](archive/ROADMAP-2026-10-17.md); the order below replaces the append-only execution log, not those requirements. [STATUS.md](STATUS.md) is the single capability/evidence matrix.

## 0. Make development bounded and reproducible

- [x] Initialize Git, preserve the pre-audit project and classify source versus generated/local files.
- [x] Consolidate specifications, current plan/status and archived evidence under `docs/`; keep the executable prototype unchanged.
- [x] Audit independently for implementation quality, test runtime and safe ownership lanes.
- [x] Land and measure fast/affected/full test tiers, reproducible result manifests and a shared desktop-test lock. The warm affected headless gate took 0.452s; full module release/sanitizer checks passed. Native profile/full orchestration has its own scoped validation record.
- [x] Reduce completed native scenario idle frames without weakening assertions, animation waits or failure detection. Export fell from 19.153s to 10.542s in a matched local comparison; filter/query/document, combined scenarios and insufficient-budget failure passed.
- [x] Create automated native/GPU windows hidden and unfocused before creation; verify real framebuffer captures after resize and keep visible OS QA explicit. Hidden native gates and the 477-check Cocoa probe passed.
- [ ] Introduce incremental object/dependency builds and coordinated icon generation, then lift blanket build serialization safely. Keep this a distinct engineering change with cold/warm measurements.
- [ ] Gradually move embedded native scenarios out of `src/app/app.c` behind a test-build boundary. First preserve exactly the same scenarios; avoid changing feature behavior during extraction.

Acceptance: a new checkout has documented build/test commands; a routine widget edit has a small credible gate; full regression is explicit, reproducible and associated with the tested binary. Two independent branches can integrate cleanly.

## 1. Close current interaction correctness work

- [x] Finish bounded active-value snapshot integration, including source/sheet/query invalidation and off-page ownership. Do not query merely to repaint the value bar.
- [x] Accept active-cell/extent retention through pointer drag, Shift navigation, range reversal and Tab/Enter; update legacy native assumptions to the intended behavior.
- [x] Accept pointer versus keyboard focus against the reference at wide/narrow and 100/150% UI scales.
- [x] Rebase or invalidate source bookmarks when a table moves.
- [ ] Fix source-record formatting identity through sorting/filtering/query reindexing; define behavior when a query changes row identity.

Acceptance: focused real-app regressions with no added idle SQL/allocations, source/PIVOT cache identity preserved and typed NULL/exact values intact. These are discrete fixes, not an invitation to keep polishing without a completion boundary.

## 2. Finish exact visible UI parity

Keep the implemented N1 ribbon mechanism, N2 workspace composition and N3 authoring core. Their completion never implied final visual sign-off. N4 shared controls and N5 integrated parity remain open.

- [x] Replace generic filter-pill labels with bounded cached actual criteria/values.
- [ ] Finish representation, Plot, size/number dropdowns, wizards and advanced dialogs using measured reference anatomy and real/explicitly disabled actions.
- [ ] Close remaining inspector/menu/focus differences using fixed reference states rather than another design direction.
- [ ] Record matching native/browser captures and working pointer/keyboard checks for: browsing; selected matrix with compact shelves/Marks; chart; filters/date hierarchy; Transformations/SQL/history preview; Full/Compact/Icons/Focus ribbon; narrow/short windows and 150% scale.

Acceptance: every state has a reference, viewport, fixture, native capture, interaction checklist and explicit remaining exception. No unresolved UI mismatch is hidden by claiming an engine test count. The prototype's token-only Marks behavior does **not** require finishing all future analytical semantics before this milestone can close; native controls must honestly describe their supported behavior.

## 3. Complete analytical behavior in separate features

- [ ] Aggregate-aware chart Other for AVG/distinct using real source aggregation.
- [ ] Marks query-grain support for fields outside the current result; bounded continuous color, persistent labels, multiple bindings and matrix/legacy behavior.
- [ ] Typed manual cells/formulas and explicit imported-data overlay model, with clear read-only source boundaries.
- [ ] Further transforms/joins/table nesting, recovery and distribution work as prioritized from the preserved requirements. Each needs a narrow design/acceptance definition before implementation.

Acceptance: correctness/ownership/persistence tests plus the smallest applicable native workflow. Reuse the accepted modern UI rather than opening another UI redesign.

## Working method

Use two independent implementation lanes initially, with one owner for App/shared integration and serial desktop testing. Review small branch commits, run affected checks, integrate, then broaden at milestone boundaries. Reopen earlier checks only for changes or evidence that justify them. Keep a per-feature distinction between UI parity, working interaction and analytical completeness. Windows/Linux execution and WebAssembly are deferred; do not install extra tools for this macOS iteration.
