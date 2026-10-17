# Native agentic review implementation plan

## R5 follow-up — unified automatic queue

The latest request extends the initial reader/outbox into one persistent native/Wasm queue. Native review binaries continue atomically publishing local evidence without a service dependency. The review-only browser host first commits its immutable archive to IndexedDB, then submits it to the same project outbox through one same-origin loopback Wasm service. A local-pending acknowledgment and a durable shared-queue acknowledgment are different user-visible outcomes; neither is design acceptance. Reload retries preserve report IDs and bytes. Browser pending storage is origin-scoped and remains subject to site-data deletion/eviction before the shared service acknowledges it.

An external standard-library queue uses transactional leases, bounded retries and recoverable completion state. A single optional worker invokes the existing Codex CLI read-only, attaches the actual frame and records structured triage, source targets and acceptance checks. Notes and captured text are data, not command authority. Automatic pickup does not imply automatic implementation, design approval or passing verification. No user/global agent configuration is changed. No collector, socket, IndexedDB adapter, model invocation or review asset belongs in production artifacts. Stop the old mock/Agentation services; only the one Wasm review origin is needed.

Acceptance includes concurrent consumers, restart/expired leases, rejected malformed uploads and conflicting IDs, offline→reload→retry in real browser storage, actual hidden native and Wasm captures through the agent, and compiled production absence. Keep the current source/binary/viewport evidence and timings separate from broader UI parity. Browser blur and trackpad panning remain explicit roadmap defects.

Approved 17 October 2026. This document converts the [feasibility analysis](NATIVE_REVIEW_FEASIBILITY.md) into an implementation handoff. [ROADMAP.md](../ROADMAP.md) owns priority/order; [STATUS.md](../STATUS.md) owns capability and evidence claims. Approval is not evidence that a feature is already implemented.

## Accepted decisions

1. **Build native feedback first.** Users report problems within the actual C99/raylib application. Further broad UI refinement should use that evidence once the reporting loop is available.
2. **Compile the same C99/raylib UI to Wasm for the browser product.** Share rendering/layout/interaction code. Keep browser-specific loader, input/file/clipboard and engine glue behind adapters; do not implement another JavaScript spreadsheet UI.
3. **Start browser computation with the official DuckDB-Wasm asynchronous Worker behind a thin typed bridge.** The custom combined-build alternative in the research is not the starting implementation task.
4. **Provide structured identities for canvas widgets.** Raylib controls are drawn objects, not individual HTML buttons. Canvas does not automatically make agent feedback easier by eliminating enumeration: agents still need bounded visible-widget snapshots and point-to-target resolution supplied by C. Do not enumerate the whole spreadsheet.
5. **Production excludes all added review machinery.** Separate compile/link configuration and artifacts; no runtime-disabled-only implementation, native web runtime, embedded model, or required agent daemon.
6. **Keep platform boundaries portable.** macOS is the acceptance host now; retain Linux/Windows seams and avoid putting Cocoa/POSIX-only types in the portable registry/protocol. The browser's asynchronous lifetime, engine/file/Worker boundaries and rendering/input differences are the main new design constraints. No unperformed OS/browser validation is claimed.

## Integration ownership

The development chat **Build Duxcel analytical spreadsheet**, `codex://threads/01a141e7-7a37-7303-9f70-f2c1ec1a3e89`, is the implementation/integration owner. This design chat owns the plan, decisions and UX feedback, and should not race it on App, shared headers, build or desktop tests.

Use small reviewed commits. Independent pure registry/schema tests or external tooling may use owned branches/worktrees, but there remains one owner for `src/app/app.c`, shared UI/platform interfaces and `Makefile`. Follow the existing two-compiler/engine-process limit and shared desktop lock. Never terminate or control the user's running app for automated acceptance, and never replace a tested executable during a run.

## First delivery: R1–R4 as one working native workflow

Deliver a useful report from an open native filter menu end to end. The stages below are integration slices, not reasons to stop after a helper passes tests.

### R1 — Build separation and production absence

- Add an explicit review feature define, tentatively `DUXCEL_DEV_REVIEW`, independent of `NDEBUG`, optimization and sanitizers.
- Extend the existing per-configuration incremental build/signature machinery. Proposed user entry points are `make review` and `make review-debug`, with distinct binaries under ignored `build/review/` and `build/review-sanitize/`. These commands are not available until implemented; record the final paths in the user guide when they are real.
- Compile new review modules only into those targets. Conditional hooks must disappear without evaluating registration arguments; exclude review-only fields, buffers, source strings, commands, platform/IPC bindings and browser exports from shipping targets.
- Keep all generated objects/dependencies and binaries distinct from release, sanitizer application, helper and stubbed unit-test configurations. Preserve atomic replacement and existing build serialization.
- Add a build-contract check proving the added machinery is absent from shipping objects/link outputs and command surfaces. Limit this gate to the new feature; it is not a prerequisite to remove every historical test scenario in the same commit.

### R2 — Portable identities, geometry and capture schema

- Introduce a small pure C99 review core, tentatively under `src/dev/`, with explicit capacity/ownership and no live OS/DuckDB/browser objects in its protocol records.
- Separate stable **action ID**, logical **widget instance ID**, current **presentation path**, and **data target**. Use stable names, not translated labels, geometry hashes, pointer values, source lines or enum ordinals alone.
- Cover the first real flow's ribbon/menu/filter/pane controls. Register actual bounds, visible clip, layer/input eligibility, transform, label/role, enabled/checked/expanded/focused state and development source location. Detect duplicate IDs and report capacity overflow explicitly.
- Resolve grid cells/selection through existing geometry on demand; include source/object/schema/query generation and row/result position. Until stable record identity is available, targets are scoped to that query version and must become stale after reindexing.
- Seal a versioned frame registry and owned snapshot. Record screenshot frame ID/time, source dirty fingerprint/binary/build flags, font/assets, window/framebuffer/DPI/UI scale/grid zoom, selected target and relevant UI state.
- Preserve exact integer/DECIMAL text and typed NULL/empty/literal `NULL` whenever values are intentionally included. Escape and bound UTF-8/JSON safely. Never retain evictable Page/Cell pointers or read mutable worker fields unsynchronized.

### R3 — Native reporting UI and same-frame evidence

- Add a review-only Report UI issue command and a conflict-checked shortcut. Take ownership before normal shortcut/click dispatch; prevent click-through, worksheet typing and outside-click menu dismissal.
- At a defined render boundary, capture the actual image, registry and state before drawing report decoration. Support picking a control, its container or a region, including disabled controls and properly clipped/frozen surfaces.
- Reuse existing native controls/text input for a compact note composer. Preserve focus/selection and an open menu. Handle cancellation, failed capture/write, retry and document/window shutdown explicitly.
- Capture pixels on the graphics thread; transfer owned buffers for encoding/writing. Use no GPU work from a writer/IPC worker. Local capture must not query DuckDB or retain its connection.
- Background query completion can continue, but the issue owns the captured state. Define whether UI publication pauses while composing; verify resume/cancellation, page eviction and document lifecycle. Keep callbacks/allocations bounded and avoid new idle SQL.

### R4 — Agent-readable outbox and feedback lifecycle

- Atomically save a local bundle with unique report/session identity, image, target/state/manifest and bounded recent event/command/async timeline. Use a platform-selected writable root; no hardcoded project/macOS path in the portable core.
- Supply a project-scoped reader that lists/reads reports and records acknowledgment, replies and verification state. The agent must be able to consume a complete report with the correct native build/target. App capture works even if the agent/service is stopped.
- Initially use existing external tooling/runtime facilities and file ingestion. Add an external local IPC/MCP adapter if required for the accepted loop; keep dependency/version locks outside native product targets. An HTTP listener being reachable is not proof that an agent tool is registered or receiving feedback.
- Keep the schema independent of Agentation's DOM model. The current browser queue may be adapted, but native targets/source provenance must remain explicit. Do not label an app widget path as a CSS/React locator.
- Begin with observation/feedback lifecycle tools. Remote app actions are a later explicitly enabled test-session feature using normal input/command routes; do not add arbitrary SQL/shell/memory access to this reporting milestone.

## Native acceptance before the milestone closes

- A user opens a real filter menu, starts reporting, picks one of its controls, writes/saves a note and sees its report ID. The original menu/action/selection is unchanged and focus returns correctly.
- An agent retrieves the same report and identifies the actual widget, native source/binary, frame image, UI scale and captured state; acknowledgment/reply history is durable. Mark implementation ready for review separately from user design acceptance.
- Cover frozen scrolling cells, popup/modal ownership, closed/disabled controls, ribbon/overflow identity and panes. Exercise 100/150% UI scales and narrow/short native layouts; keep OS input/IME/physical gesture claims scoped to actual tests.
- Pure tests verify ID scope, duplicates, clipping/layer picking, bounded overflow, stale targets, ownership, UTF-8 serialization and typed-value distinctions. Maintain ASan/UBSan coverage.
- Focused real-renderer/native tests verify capture/composer/input isolation, query replies/eviction, cancellation and write failure/retry. Acquire the existing desktop lock; preserve source/binary hashes and actual completed stage ranges.
- Shipping configuration has no added review modules/data/commands/listeners/exports. Compare optimized review inactive/active and shipping behavior; report frame-time tails, memory, input/capture costs and SQL/job counts without substituting sanitizer timing for feel.

Do not add per-frame filesystem/network/JSON work, whole-grid trees, periodic GPU screenshots or new idle SQL. Use a bounded event ring and selected-capture registry detail. Continuous video and general deterministic replay are explicitly later work; they are not necessary to deliver the first functioning native feedback loop.

## Wasm-ready development constraints now

- Define capture/registry serialization over portable value records. Share C identity/geometry/input rules; put screenshot, outbox, clocks, input composition, clipboard/files and agent transport behind narrow adapters.
- Add lifecycle seams as required by review capture and the browser tick; do not broadly rewrite App merely to create parallel work. Coordinate any test-harness extraction with the App integrator.
- Keep UI/rendering and backend requests nonblocking. Design browser request/reply messages around existing bounded jobs/pages, request identity, latest-wins/stale rejection, cancellation, errors and ownership. Neither a C pointer nor an allocator is shared with a separate Wasm/Worker module by default.
- Keep transformation/SQL definition semantics in shared C and execution in DuckDB. Browser glue marshals typed requests/results; it does not recreate calculations in JavaScript.
- Preserve source/target versions and all coordinate spaces, including eventual canvas CSS/backing dimensions and DPR. Debug adapters may expose visible C widget metadata; production accessibility is a separate product capability.

## Authorized dependency and browser proof sequence

Native R1–R4 starts with the existing C99/raylib/DuckDB/macOS toolchain. No new native GUI/runtime dependency is required. Use existing Python/Node tooling only externally where useful; a local file-based reader can precede a protocol daemon.

After native reporting works, install/pin the required Emscripten SDK, raylib web build and official DuckDB-Wasm/browser-host packages in an isolated project development setup. The user has authorized these necessary dependencies for this milestone; the prior blanket “no Wasm tooling” deferral is superseded. Do not alter global Git identity/configuration or introduce those runtimes into native distribution. Document reproducible versioned commands and resource use on this 8 GB host.

The first browser proof must:

1. Compile the same C99/raylib UI and run a browser-scheduled tick with the real fonts/icons. Keep host JS/HTML to platform/loader/engine/review adapters.
2. Run a real official asynchronous DuckDB-Wasm Worker query, initially with its supported single-threaded engine bundle unless evidence requires experimental threading. An engine Worker remains asynchronous to the UI.
3. Import a real source and demonstrate bounded typed pages, sort/filter, error/cancellation/stale handling, and responsive UI while work is active. Preserve exact integers/DECIMAL/NULL/text over the entire bridge; do not enable lossy canonical-value casts to JS numbers.
4. Reuse the C review registry/schema through a browser adapter. No assumption that canvas widgets become HTML elements. Gate all review Wasm exports/host assets out of shipping builds.
5. Measure startup/download, memory/copy and rendering/query costs. Record tested browsers, deployment headers/assets and remaining platform differences.

The subsequent browser product milestone covers `.dxls`/source-reference portability, existing direct DuckDB C persistence calls, OPFS/recovery/export, browser permissions/input/composition/clipboard, extension compatibility, context loss and production accessibility. Renderer/query success alone does not close it. Do not start a custom combined DuckDB build as a parallel architecture experiment unless new evidence warrants revisiting the selected Worker path.

## Reporting and completion boundaries

Update the one current status/roadmap as slices land. Distinguish approved architecture, implemented portable helpers, integrated reporting, native visual/interaction acceptance and real analytical semantics. Keep historical evidence and known limits with the tested source/binary. The frozen JS reference remains unchanged; new native UX decisions can be recorded without restarting mock implementation.

First handoff objective: complete R1–R4 end to end on macOS, with explicit production absence and portable seams. Then start W1–W4 in the roadmap. The implementation owner should proceed autonomously within this approved scope and report a concrete build path and tested reporting flow.
