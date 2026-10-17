# Native review builds and a shared WebAssembly UI

Research date: 17 October 2026. Repository inspection checkpoint: `413b25b9215e361d804632d7ed07aed61c306ce8`; other implementation work continued during this analysis. **Proposal, not an implemented capability or a native acceptance result.** No native application was launched, controlled, built or modified for this research. No browser toolchain was installed.

Subsequent decision, 17 October 2026: the user approved native agentic review as the first implementation priority, the shared C99/raylib Wasm UI as the browser product route, and the official DuckDB-Wasm Worker bridge as the starting engine architecture. [NATIVE_REVIEW_PLAN.md](NATIVE_REVIEW_PLAN.md) and the current roadmap now govern execution. Alternatives retained below record the analysis; they are not an undecided engine selection. No feature completion is implied by approval.

## Recommendation

Build a small C99 review layer into an explicitly separate native development target. It should let the user pick an actual rendered widget, write a note inside Duxcel, and save a screenshot plus bounded, owned UI state and recent input/command history. Compile that layer entirely out of shipping builds. Keep agent orchestration and issue storage outside the app.

Use the same widget identities and capture schema in a future raylib/Emscripten build. This makes the real native and browser UIs observable without implementing a second spreadsheet interface. A browser companion can display native screenshots and feedback bundles before the full Wasm port exists. Native capture should not wait for that port.

This recommendation is an engineering assessment based on source inspection and primary documentation. Costs and performance budgets below are planning proposals, not measured results.

## What already exists in Duxcel

| Existing code | Useful foundation | Work still needed |
| --- | --- | --- |
| `src/ui/ui.h`, `ui.c` | Shared controls, pointer handling, clip stack, interface scale, fonts, modality and finite animation feedback | Explicit semantic widget identity and a bounded frame registry. Not all controls route through one function. |
| `src/ui/ribbon_spec.h`, `ribbon.h` | Action enum, command labels and measured command/group/overflow rectangles | Stable external action names and distinct widget instance paths. Enum ordinals alone are not a versioned protocol. |
| `src/grid/grid.h` | Active cell/extent, viewport, freeze, geometry and a separate `GridInput` structure | On-demand cell identification, visible clip/layer metadata and query/source identity. Do not register a worksheet's full cell population. |
| `src/platform/platform.h`, `macos.m` | Captured native gestures/modifiers, platform commands and presentation preferences | Review-mode input ownership; optional private local IPC and a browser platform adapter. |
| `src/app/app.c` | Actual modal/context/selection state, query routing, render timing and screenshot helpers | Small integration hooks and owned snapshot assembly under the single App integration owner. |
| `src/data/worker.h`, `worker.c` | Owned jobs/replies, latest-wins identities and query counters | Capture metadata without new SQL or borrowed page pointers; a browser backend boundary if using official DuckDB-Wasm. |
| `src/ui/paintlayer_backend.c` | Render-target, scissor and framebuffer restoration | Validate capture and overlays against real GPU state; WebGL port checks. |
| `src/main.c`, `Makefile` | Normal/debug binaries, native scenarios and screenshot flags | Review feature/build separation. Both existing binaries currently use the shared source list; the debug target chiefly changes optimization/sanitizers. |

`ui_feedback_colors()` currently hashes rectangle geometry and kind for short-lived hover/press animation state. That is appropriate to that cache, but unsuitable as a durable issue identity: responsive layout changes it, and multiple controls can reuse the same rectangle. Existing test flags are also visible in the common entry point. A shipping build does not currently demonstrate a strict “all development machinery excluded” boundary merely because it is called `duxcel`.

Installed raylib headers report 6.0.0 and expose screenshot and automation-event APIs. Header declarations do not prove the installed library's optional recording feature is enabled. The project uses the DuckDB C API through native linking; DuckDB's package metadata was not available through pkg-config, and no version assumption is made from that failed lookup. `emcc` was not found in the inspected environment.

## Native feedback interaction

1. In a review build, invoke a development-only **Report UI issue** command while the problem is visible. A shortcut should be assigned after auditing existing bindings.
2. The review layer takes ownership before application shortcuts/click handlers run. It seals a screenshot, widget registry and UI snapshot from the same rendered frame, before drawing review decoration. A tiny picker is drawn over that frozen scene. Opening it must not close a filter, apply an edit, reorder a shelf or alter selection.
3. Hover/click the actual element; allow choosing its containing group or an arbitrary region. Disabled controls can be reported too. Distinguish visible painted objects from objects currently eligible to receive input.
4. Type the observed/expected behavior into a small native composer using Duxcel's existing text-input/control machinery. Text entered here cannot fall through into the worksheet or its shortcuts.
5. Save locally, receive a stable issue ID, and resume with the prior application focus/selection intact. A failed write must retain the note for retry. A running query can continue; the captured artifact owns its data and does not freeze or retain the worker connection.

Capturing must have a defined frame boundary, not an arbitrary screenshot call paired with a later JSON dump. GPU readback happens on the render thread at the designated safe point. Encode/write outside the hot draw path using owned buffers. Never transfer an OpenGL context to a file-writing worker. If the displayed scene is held while composing, query completion/publication and document-close behavior need explicit rules and regression coverage; “pause the app” is not a complete concurrency design.

For animation or drag problems, retain the recent input/transition history and attach a short recording when needed. Pausing after the problem provides a target, but cannot recover a video of frames that were never recorded. Start without continuous video capture.

## Widget registry and evidence schema

Use separate **action**, **widget instance** and **data target** identities. A shared action such as `transformations.open` can appear in the ribbon, a menu and a shortcut; those are distinct widget instances. An illustrative widget path is `ribbon/home/analysis/transformations`. It must survive resizing, localization and button relocation into overflow. Store the current presentation path as well, so overflow and ribbon-specific defects remain distinguishable.

A registry entry needs: stable ID, parent/group ID, role, concise accessible label, action identity, enabled/checked/expanded/focus state, logical bounds, actual visible clip, paint/input layer, coordinate transform, and development source file/function/line metadata. Source lines are supporting evidence, not the identity. Detect duplicate IDs in review builds.

For cells, store worksheet/object/source IDs, query/schema revision, column identity, active cell and selection bounds, visible row/result position, freeze and scroll. A row ordinal is not a permanent record identity after sorting or filtering. Preserve that limitation until the source-record identity work is complete; mark a target stale instead of silently pointing to a different row. Compute a picked cell through existing geometry, or register visible regions, rather than constructing a retained tree for millions of cells.

Picker hit testing must use the real transform and clipped visible rectangle, modal/popup ownership and draw order. The earlier frozen-pane bug is a direct acceptance case: a selected scrolling cell underneath a frozen column must not win the visual picker or normal input hit test. Cached closing-popup pictures may be visible but must not be described as active inputs.

Each versioned feedback bundle should contain:

- Issue/session ID, schema version, native/browser backend and human note.
- Screenshot, frame ID, monotonic timestamp and selected target/region.
- Git revision **and dirty-source fingerprint**, executable hash/build options, dependency/font/asset identity and OS/renderer information.
- Window size, framebuffer size, DPI, interface scale, grid zoom, and the transforms connecting these spaces. A future browser build also records canvas CSS bounds, browser zoom/DPR and backing-store dimensions.
- Current sheet/object, active cell/selection extent, pane/ribbon/menu/focus/drag state and relevant animation phase.
- Bounded recent pointer/keyboard/command events with ordering/modifiers; query request/reply IDs and completion observations where relevant.
- Fixture identity and the limits of the capture. Raw data, typed text, paths and SQL should be included only when needed and intentionally selected; a screenshot itself may contain data.

Exact integers and DECIMAL values must remain exact strings or typed binary values if included. Keep SQL NULL distinct from empty text and literal `NULL`. Do not export `Cell.number` as the canonical value. Snapshot copied state within explicit bounds; never retain a borrowed Page/Cell/column pointer after page eviction.

Read UI-owned state or correctly published worker status at the capture boundary. Do not inspect mutable `QueryWorker` fields unsynchronized from a review/IPC thread. Query counters are useful acceptance evidence, not permission to bypass ownership and synchronization.

## Keeping production free of review overhead

Create an explicit feature such as `DUXCEL_DEV_REVIEW`, independent of `NDEBUG`, optimization and sanitizer settings. Proposed targets are an optimized review binary for judging feel, a review/sanitizer variant for ownership testing, and shipping binaries without the feature.

Exclude review source files from shipping source/object lists. Compile registration/capture hooks to no-ops whose arguments are not evaluated. Exclude review-only fields, static buffers, source strings, bindings, commands and platform bridges too. A runtime `if (review_enabled)` switch is insufficient for this requirement. A release browser build must similarly omit review exports, JavaScript bindings and companion assets.

Use separate object/output directories so changing feature flags cannot reuse incompatible objects or overwrite a binary under test. Keep review preferences/capture directories separate from the user's ordinary preferences and workbooks. Preserve the existing desktop-test lock and immutable binary/source provenance during acceptance.

Acceptance must include preprocessed/object/link inspection for absent review code/data, no review symbols/commands/listeners/exports in shipping artifacts, and comparisons against a clean shipping baseline. Metadata needed for genuine production accessibility remains a separate product feature and cannot be discarded merely because the debug registry has a related shape.

In a review build, use fixed-capacity storage and static IDs. Illustrative bounds—not final capacities—are 4,096 entries ×128 bytes =512 KiB per frame registry and 2,048 events ×64 bytes =128 KiB. A second registry can preserve a completed frame. Overflow must be explicit rather than corrupting the capture. There should be no per-frame JSON construction, file/network write, GPU readback, SQL request or heap allocation for recording ordinary controls. Inactive recording can skip building the detailed registry entirely.

One 3840×2160 RGBA frame is about 31.6 MiB uncompressed. Continuous screenshots create a substantial bandwidth/readback/encoding cost. Capture on request; make video an explicit later feature. Instrumentation can perturb the bug being investigated, so compare optimized review mode with capture disabled/enabled and a shipping build. Measure frame-time tails, input-to-presentation latency, memory and GPU capture stalls; average FPS and current CPU render timings alone are insufficient. On 60/120 Hz displays the total frame budgets are approximately 16.7/8.3 ms, respectively. Choose a review-overhead budget from measurements rather than promising a percentage now.

## Agent connection and browser companion

Start with an atomic local feedback bundle/outbox. The agent can read it directly; neither an HTTP server nor a model belongs inside the application for that first milestone. A separate local review service can ingest bundles, preserve notes/status/history and later expose MCP tools such as list feedback, read issue, inspect captured target, acknowledge, reply and attach verification evidence. These are proposed tool capabilities, not currently available native tools.

The current browser queue is reusable as tooling, but its DOM selectors, origin filter and reference fingerprint are not a native protocol. Introduce an explicit native adapter and preserve the full versioned bundle. Do not disguise a native widget ID as a trustworthy CSS selector or React source location. Queue/storage choice must remain replaceable; the native app need not link Agentation or its runtime.

Prefer file ingestion first; add a private local IPC channel only if live inspection proves useful. Keep protocol I/O outside the render thread and operate on immutable snapshots. Initial agent access should be observational. Agent-driven UI actions or semantic command execution can come later in an explicitly enabled test instance; they must use the normal command/input paths and preserve document guards. Replaying `filter.apply` is not evidence that a pointer/drag path works.

If a local HTTP/MCP service is added, isolate sessions/worktrees and use loopback binding, validated origins and session authentication; do not expose arbitrary SQL, memory access or shell execution. These controls follow the [MCP transport requirements](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports). A stopped agent/service should not prevent local capture. No always-running agent loop is implied.

There are three distinct “web interface” approaches:

| Approach | What the user sees | Fidelity and cost | Recommendation |
| --- | --- | --- | --- |
| Native review UI + optional browser feedback desk | Real app interaction; browser shows captured native pixels, IDs, notes and history | Exact captured native pixels; low duplication; browser is not the spreadsheet | First choice now. A useful bridge before any Wasm port. |
| Stream native window into browser | Video/images of the native app with possible remote input | Pixels originate natively, but compression/input latency changes feel; adds capture/transport/permissions work | Later remote-review tool, not the first feedback milestone or a latency benchmark. |
| Compile shared C99/raylib UI to Wasm | Real shared renderer and interaction code running locally in the browser | Eliminates separately authored product layouts; still has browser-platform differences | Strategic browser product route. Share registry/schema, but run native acceptance too. |

A separately reimplemented React spreadsheet remains useful for historical design intent, but does not establish native behavior. Native review does not require waiting for another mock revision. Preserve the current frozen reference and make deliberate, versioned design decisions when native feedback changes intended behavior.

An additional transitional option is a Wasm C UI connected to a local native DuckDB/backend service. It can postpone browser engine/storage porting while testing the shared renderer. It still requires a bounded, typed asynchronous protocol, one authoritative workbook/session owner, and intentional local access controls. It depends on an installed native companion and therefore is not the standalone browser product. A new HTML spreadsheet connected to the native engine would validate some data behavior while retaining the UI implementation discrepancy that prompted this analysis; it is not recommended as the new feedback baseline.

## Wasm feasibility and the real porting work

[Raylib's official web guide](https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5)) and [web main-loop example](https://www.raylib.com/examples/core/loader.html?name=core_window_web) establish the renderer path. Use a browser-scheduled update/draw callback; do not carry the current native lifetime loop and blocking shutdown straight across. The guide discusses Asyncify as an alternative with cost; it should not be the default solution to every synchronous subsystem. Emscripten maps supported OpenGL ES functionality to WebGL, so validate Duxcel's framebuffer/scissor/render-target paths against the selected target ([OpenGL support](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)).

Application UI/interaction code remains C99. A browser host still needs Emscripten's JavaScript loader and platform glue for files, input, clipboard and workers. That is different from maintaining a second DOM/CSS spreadsheet. Native builds acquire no web runtime dependency. DuckDB itself is a C++ dependency already used through its C API; compiling its implementation for Wasm does not require rewriting Duxcel's application UI in C++.

| Area | Duxcel-specific work / acceptance |
| --- | --- |
| Lifetime | Separate initialization, one update/draw tick and cleanup; asynchronous browser exit/save rules. Coordinate this with App/test-harness extraction rather than a broad rewrite. |
| Graphics and typography | Build raylib for the selected WebGL target; bundle the same Plex/mono/fonts/icons; validate opacity textures, clipping/frozen cells, shader/texture limits, context loss, CSS/backing size and DPR. Shared source does not guarantee identical pixels across GPU stacks. |
| Input | Adapt mouse/pointer/wheel/modifiers and drag state; browser-owned shortcuts/fullscreen behavior; focus loss; composition/IME. Current native-captured gesture/modifier queues cannot be assumed identical to browser events. |
| Files and persistence | Replace Cocoa panels and absolute-path assumptions with file handles/uploads/downloads and browser storage. Implement source relinking and workbook import/export explicitly. An Emscripten virtual filesystem is not unrestricted access to Finder. |
| Query execution | Keep expensive DuckDB work off the browser UI thread; preserve page bounds, stale replies, cancellation, errors and totals/distinct semantics. A single-threaded engine in a Worker can keep UI responsive; engine multithreading is a different capability. |
| Workbook storage | `src/workbook/workbook.c` directly uses DuckDB C handles, prepared statements and paths. This must be covered by a browser backend or a custom combined build; changing only `worker.c` is insufficient. Preserve `.dxls` semantics/migrations, not design-state JSON. |
| Accessibility | A canvas has no automatic semantic control tree. A development picker is useful infrastructure but is not a production accessibility implementation. Browser semantic adapters, keyboard behavior and native accessibility need their own scope. |
| Performance/deployment | Measure download/initialization, memory/copies, large queries and UI latency on target browsers. Serve matching pinned module/worker/extension assets and required headers; test real deployment as well as localhost. |

### DuckDB integration choices

The [official DuckDB-Wasm client](https://duckdb.org/docs/current/clients/wasm/overview) already executes DuckDB in the browser. It exposes an asynchronous Worker API and Arrow results. It is not a native `.a`/`.dylib` that can simply be linked into the existing macOS build. Compare two plausible paths before choosing:

| Path | Advantages | Costs / uncertainties |
| --- | --- | --- |
| Shared C UI Wasm module + official DuckDB-Wasm Worker + thin host bridge | Reuses official browser file/network/extension integration and async execution; no second product UI; independently bounded UI/engine memory | Adapt the current database/workbook operations to async requests. Results cross Worker/JS/Wasm boundaries; preserve exact types/ownership and measure copies. Modules do not share ordinary C pointers or one allocator by default. |
| Custom combined Duxcel/raylib/DuckDB Wasm build, using the C API internally | Potentially reuses more existing database/workbook code; direct C API and fewer cross-module marshaling boundaries | Own a coherent DuckDB browser build, VFS/file/network/extension support, exception/thread/toolchain compatibility and worker/main-thread arrangement. Native libraries cannot be reused as Wasm. Compile/runtime memory and maintenance cost are less certain. |

Recommendation: test the official Worker bridge first and compare it against a small combined-build feasibility experiment only where the source-reuse benefit justifies it. Do not commit to a monolithic build merely because the app is written in C. Conversely, do not rewrite SQL/transformation semantics in JavaScript: the host marshals requests/results, while existing C definitions and DuckDB remain responsible for behavior.

The current [DuckDB-Wasm documentation](https://duckdb.org/docs/current/clients/wasm/overview) describes a 4 GB Wasm memory ceiling with potentially lower browser limits and experimental engine multithreading. This is a constraint of the current client/target, not a prediction that all future Wasm implementations are forever limited that way. Keep result pages bounded and avoid whole-table materialization. Native large-data/spill behavior is not automatically a browser guarantee. HTTP partial reads also depend on the actual filesystem/network path in use; [DuckDB troubleshooting](https://duckdb.org/docs/current/clients/wasm/troubleshoot) documents relevant limitations.

The [threaded bundle guidance](https://duckdb.org/docs/current/clients/wasm/instantiation) requires explicit bundle selection, cross-origin isolation and compatible extensions. [Emscripten pthread guidance](https://emscripten.org/docs/porting/pthreads.html) explains shared-memory/header requirements and main-thread blocking/deadlock risks. Start a bridge spike with an asynchronous single-threaded engine Worker unless measured requirements demand the threaded path. That recommendation does not make the UI wait for queries.

The [DuckDB instantiation options](https://duckdb.org/docs/current/clients/wasm/instantiation) also allow integer/DECIMAL conversion to doubles. Those lossy conversions must be disabled or bypassed for Duxcel's canonical data. Explicitly test wide integers, high-precision/negative decimals, typed NULL, empty strings and the literal `NULL` through the full bridge. Preserve Arrow validity/type metadata or a bounded exact-string representation; generic `JSON.stringify()` of a JS result object is not a sufficient data protocol.

[DuckDB now documents OPFS persistence](https://duckdb.org/2026/09/18/opfs-wasm); it must be validated with the pinned client version and selected threading bundle. OPFS is origin-private storage, not direct user-visible file access or a backup. Quotas/eviction and private browsing differ ([browser storage behavior](https://developer.mozilla.org/en-US/docs/Web/API/Storage_API/Storage_quotas_and_eviction_criteria)). Preserve explicit workbook export/recovery and define portable external-source references. A browser refresh is not a durable-save test.

### Feedback in a Wasm build

Compile the C review registry/picker into review Wasm only. Export a versioned snapshot/pick API through Emscripten's [C/JavaScript interface](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html). A browser feedback desk can consume that snapshot, annotate the real canvas pixels and use the same issue bundle. Batch by capture/frame, not one cross-boundary call per cell per frame.

Ordinary DOM annotation tools cannot infer widgets inside a WebGL canvas. [Canvas documentation](https://developer.mozilla.org/en-US/docs/Web/HTML/Reference/Elements/canvas#accessibility) explicitly describes the lack of exposed drawn-object semantics. Optional transparent DOM targets can be generated from the C registry in review mode, but must not intercept normal interaction or become a second independently authored layout. They are an adapter, not the source of geometry.

Browser canvas video is possible using [`captureStream()`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/captureStream) plus a supported recorder; format/support, origin-clean content and capture overhead still need validation. Do not enable costly recording or preserve-drawing-buffer settings for every shipping frame simply to simplify review screenshots. Schedule capture at a valid frame point and keep review-only configuration separate.

## Existing projects to learn from

- [Dear ImGui debug tools](https://github.com/ocornut/imgui/wiki/Debug-Tools) demonstrate item picking, ID inspection, clip/metrics inspection and focused popup/input logs. This is strong precedent for observable immediate-mode UI. Dear ImGui is C++ and these tools depend on its own widget internals; adopting it is not required or proposed.
- [Dear ImGui Test Engine](https://github.com/ocornut/imgui_test_engine) demonstrates semantic UI automation and verification tied to widget identity. Borrow the testing model rather than claiming it supports Duxcel's custom controls out of the box.
- [Raylib automation events](https://github.com/raysan5/raylib/blob/master/examples/core/core_automation_events.c) provide input record/playback building blocks. They do not by themselves record DuckDB reply ordering, workbook state, native panels or Duxcel's extra native input queues. Validate library build support and coverage before selecting them for replay.
- The current Agentation queue is useful feedback infrastructure. Its DOM-specific picker cannot provide native widget introspection; a small application-specific instrumentation layer remains necessary.

## Proposed delivery sequence and acceptance

| Stage | Deliverable | Exit criteria / complexity |
| --- | --- | --- |
| 1. Native capture vertical slice | Separate optimized review target; in-app report command/composer; one-frame screenshot and owned state bundle; a few explicit ribbon/pane identities | Can report an open filter/Transform pane from the real app without executing or dismissing it. Local capture works with agent absent. Shipping binary excludes the new feature. Moderate; first useful slice, not full widget coverage. |
| 2. Reliable picking | Frame registry across shared controls, menus/shelves, grid regions, disabled states, nested clips and source metadata | Stable IDs survive full/compact/icons/overflow modes. Frozen panes and popup layers pick correctly at 100/150% and high DPI. Duplicate/overflow/stale cases are explicit. Moderate-to-high; coverage is the main work. |
| 3. Agent feedback loop | External bundle ingestion, issue status/history and read-only tools; optional native screenshot review desk in browser | Agent identifies the actual widget/build, replies with evidence and retains unresolved work. No live database pointer access or new idle SQL. Moderate; reuse storage where practical. |
| 4. Reproduction and performance | Bounded input/command/async timeline; opt-in record/replay on fixtures; matched overhead and tail-latency measurements | Demonstrate which events are replayable and which are merely evidence. Keep pure, sanitizer and focused GPU/native acceptance; observe the desktop lock. Higher/ongoing; do not promise general deterministic replay. |
| 5. Wasm decision spike | Shared C UI runs in browser; real bounded DuckDB query; exact-value transfer; source file import; report/capture using shared IDs | Compare official Worker bridge and combined-build tradeoffs with measured startup/memory/query/UI results. Include save/reopen/export proof before choosing a full browser architecture. High uncertainty until built. |
| 6. Browser product port | Complete platform/engine/workbook adapters, deployment and accessibility scope | Native and browser scenario matrix with documented differences. Separate product milestone, not the prerequisite for native feedback. High; not a single compiler-flag task. |

Keep one integration owner for App, shared platform/UI interfaces and build changes. New review modules and pure registry/serialization tests can be isolated. Do not create a broad UI rewrite or compete with existing native desktop tests. Run affected pure checks during edits, then a focused native profile for visible behavior, widening at renderer/shared integration boundaries.

Use explicit review acceptance for: capture before menu dismissal; no click-through; focus restoration; disabled controls; partial/closing animation surfaces; frozen panes; narrow/short windows; UI versus grid zoom; typed exact values; snapshot ownership after page eviction; disk-full/capture errors; query completion during report composition; and no review code/data/bridge in shipping artifacts. Native visual parity, native interaction correctness and analytical semantics remain independent conclusions.

The immediate next engineering task is Stage 1 with a narrow set of controls and a strict shipping-exclusion check. Browser target work should reuse that foundation after a separate Wasm decision spike; no new React spreadsheet implementation is recommended.
