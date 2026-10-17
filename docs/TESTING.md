# Testing without a long edit loop

Keep the existing coverage; choose a gate that matches the change. The warm module tests are fast. Compilation and native stage pacing dominate elapsed time. The previous full native checkpoint took about 14m20s, so it is an integration gate rather than the default after every edit.

Measured on the integrated tooling: grid +choiceview, release and ASan/UBSan, **46,228 assertions in 0.452s** including warm build checks. All 17 read-only modules at the earlier tooling checkpoint ×both configurations passed **74,820 assertions in 39.734s**, including a 29.468s rebuild. These are local observations, not cold-checkout guarantees.

```sh
# Fast headless release + ASan/UBSan (no desktop/clipboard).
make test-fast

# Focused native record SQL/Page checks, private temporary data, no GUI.
make test-records
make test-fast TEST_ARGS='--modules grid choiceview --jobs 2'

# Headless checks plus one independently initialized native feature slice.
make test-affected FEATURE=shelves TEST_ARGS='--modules grid marksview --jobs 2'

# Existing complete workflows without the full suite (hidden by default).
make test-native SCENARIO=table TEST_ARGS='--config sanitize'

# Explicit coherent integration: headless, core/workbook, GPU, 18 native scenarios.
make test-full TEST_ARGS='--require-data'

# Inventory/help; runner's own failure/provenance/locking checks.
python3 tools/test.py --list
make test-runner
make test-build
```

`--config release`, `sanitize` or `both` selects headless configurations. With `both`, the native phase uses the sanitizer binary. `--jobs 2` overlaps only independent **already-built read-only module tests**. Builds stay serialized under the existing Make rules; the runner does not enable unsafe parallel asset generation. `--no-build` is for deliberate reuse of known binaries/fixtures, with freshness the caller's responsibility.

## Tiers and ownership

| Tier | What runs | Appropriate use |
| --- | --- | --- |
| `fast` | 19 headless modules, or named affected modules; release/sanitizers | Widget, geometry, formatting, motion and presentation edits |
| `records` | Raw-record SQL and integrated native Page tests, sequential per configuration | Record provenance, positional consumers and engine ownership; no desktop or shared fixtures |
| `affected` | Selected headless modules, canonical design fixture, named native layout slice | A visible control or interaction change |
| `native` | Named existing complete application workflows, with only their required fixtures | A table/filter/document regression without unrelated scenarios |
| `full` | All headless modules; core then workbook for each configuration; GPU probe; 18 native workflows | Coherent integration/release boundary; shared lifetime/renderer changes |

Focused native profiles currently expose existing valid bootstraps: `statistics`, `dock`, `pane-motion`, `popup-motion`, `shelves`, `view-menus`, `source-menu`, `source-controls`, `shared-actions`, `author-menus`, `selection-start`, `active-cell`, `criteria-selects`, `ribbon-collapse`, `ribbon-overflow`, `record-pages`, `ribbon`. `--list` records their exact stage ranges. Arbitrary numeric jumps are unsafe because the older layout harness is stateful. The new active-cell profile has its own independent fixture bootstrap; selection-start validates the early assertion changes. Profile acceptance never implies the whole state matrix passed. `ribbon-collapse` covers stages **[620, 638)**: 18 checks for reserved narrow-ribbon layout, collapsed-group flyouts/Escape focus, live chrome above the drawer and Full/Compact/Icons/Focus resizing without changing source/result caches or idle SQL. Run it with `make test-affected FEATURE=ribbon-collapse TEST_ARGS='--modules grid --config sanitize'`. `ribbon-overflow` covers **[638,665)**: 27 checks for the More menu, disabled/history traversal, real Undo/Redo including leaving historical preview, anchors and keyboard return. `ribbon` preserves the original **[0,64)** workflow for adjacent selection/ribbon behavior.

Core creates generated databases consumed by workbook/native tests. The runner preserves **core → workbook**, then completes that configuration before the next overwrites fixtures. It never parallelizes these mutable suites. Worktrees have separate `tests/generated/`; do not share it via symlink. The preview module's DuckDB connection uses its own private temp directory and reads authored input; it does not mutate those fixture databases.

The full native inventory is **1,108 maintained application stages**, plus separate **110 GPU** and **477 background-window** checks. NYC's eight stages require `tests/downloads/yellow_tripdata_2025-01.parquet`. `--require-data` fails if it is missing. Without that flag the runner records an explicit skip and reports `passed-with-skips`, never a full 1,098-stage pass. `tests/download_data.sh` can reproduce the existing public inputs; nothing downloads automatically during ordinary tests.

The full tier does not mean every optional stress/packaging/OS path was exercised. `make spill-test`, `make unique-large-test`, icon regeneration/verification, standalone distribution, physical trackpad/DPI/IME and manual file panels remain separately scoped checks. Run those when their area changes; Windows/Linux validation is deferred.

## Reproducibility and desktop safety

Every run writes individual logs and `summary.json` under ignored `tests/results/runs/<UTC>-<tier>-<pid>/`. The record includes Git revision/dirty state, aggregate source hash, binary hashes, commands, expected stages, exit/completion checks, elapsed times and explicit skips. Binary replacement or source changes during the run invalidate coherent acceptance. Keep implementation edits outside that worktree while its acceptance run executes.

A per-worktree lock prevents competing fixture/build runners. Native phases also acquire `/tmp/duxcel-native-tests-<uid>.lock`, independent of `TMPDIR`, so different worktrees cannot fight over windows, clipboard or preferences. The lock is nonblocking and explains the current owner. Manual application testing, direct legacy smoke targets, signing and bundles must be coordinated with this same desktop owner; they cannot be made safe merely by using another branch.

Success requires actual completion, exit zero and no sanitizer diagnostics, not merely a screenshot or an early PASS line. Layout profiles additionally require the application’s exact `Layout smoke completed [start,end)` line; the full/named layout workflow requires `[0,675)`. This rejects a familiar PASS from a binary with an older stage inventory; it does not establish arbitrary `--no-build` source freshness. Native frame budgets remain failure timeouts. Existing fixed animation/stage waits remain intact. Long native waits are not evidence that pure tests are slow, and reducing assertion counts would not solve the main delay.

Completed native scenarios now stop after all enabled scenarios reach their terminal assertions, a subsequent final frame is drawn, and application/worker/publication state is idle. Document Quit retains its save/exit lifecycle. A matched export check took 19.153s before and 10.542s after with the same 1,000-frame ceiling; the new run needed 482 frames. Filter/query/document, combined export/icons and a two-frame failure budget were checked separately. The integrated release author-menu profile passed in 9.659s. This does not estimate a new whole-suite duration.

Selection/readout/control integration at `413b25b` has dedicated acceptance for 183 native stages and the GPU/background probes. The later 18-stage narrow-ribbon gate has a separate source/binary manifest and took 10.877s in the app, 13.171s total. These are separate focused gates, not a fresh full-suite pass. Use [STATUS.md](STATUS.md) for the exact accepted source, optimization and scope.

## Hidden native windows

Automated App runs (smoke flags or `--frames`) and GPU checks create **hidden, unfocused, always-running windows before native creation**. They render the real GL framebuffer; captures do not require foreground activation or switching the user’s Space. The runner discards inherited `DUXCEL_TEST_VISIBLE` and records its chosen visibility. Every native gate first checks Cocoa invisibility/non-key state, app inactivity, external frontmost PID and actual pixels through swap/resize. This does not programmatically switch or pin macOS Spaces.

Use `--visible` with affected/native/full only for deliberately scheduled desktop QA. For a direct bounded manual launch, use `DUXCEL_TEST_VISIBLE=1 ./duxcel --frames 600`. Normal interactive launches remain visible. Hidden runs refuse real OS file panels; panel focus, menu/responder/IME routing, physical gestures/monitor changes and OS window lifecycle need explicit visible QA. Current clipboard scenarios still touch the system clipboard, so the shared desktop lock remains necessary even while windows are hidden.

## Incremental builds and remaining work

The application now compiles per translation unit into separate `build/obj/release/app/` and `build/obj/sanitize/app/` trees. The design-fixture helper uses its own `build/obj/release/design/` tree and include flags. Compiler-generated `.d` files track actual headers; a lost sidecar forces its object to recreate it. Changed command signatures invalidate their configuration. Successful compiler/linker work atomically replaces objects, dependency sidecars and binaries. Do not replace a binary during its acceptance run.

One shared icon stamp validates checked-in derivatives before regeneration. Multiple missing outputs trigger one repair; ordinary builds preserve valid bytes. Module tests retain their independent commands so renderer stubs and different include/define configurations cannot share application objects. `.NOTPARALLEL` remains; lifting it and making module compilation incremental are separate follow-ups. Mutable fixtures and desktop ownership still require serialization. Do not use `make -B`.

`make test-build` runs an isolated tiny real-C99 build contract without app windows, DuckDB fixtures or canonical asset changes. It checks cold/warm builds, actual source/header dependencies, missing sidecars, compile/link flags, configuration isolation, failure preservation and coordinated icon repair. **44 checks passed in 12.705s** on integration; the implementation worktree recorded 12.540s. See [commands, configurations and provenance](validation/checkpoints/2026-10-17-incremental-ribbon/).

With explicit development `CFLAGS=-O0 -g`, the widget rebuild changed **2.734s →0.259s**, warm build **0.123s →0.199s**, and cold build **3.159s →5.317s**. Default O1 ASan/UBSan observations were cold **31.424s**, warm **0.206s**, widget **0.263s**; the helper's O0 cold/warm builds took **1.279s /0.177s**. Per-file compiler startup costs make the cold build slower; these are local build timings, not optimized application-performance claims.

Apple's installed GNU Make 3.81 compares whole-second timestamps. A source/header/flag change in the same clock second as the corresponding artifact can be missed; the mutation contract deliberately crosses timestamp boundaries. When freshness is in doubt, explicitly force the affected source with `make -W src/ui/selectview.c duxcel` (using the intended flags) or clean/rebuild that configuration. Configuration signatures do not remove this underlying timestamp limitation. There is no claim of arbitrary concurrent Make invocation safety in one worktree.

## Native review development gates

`make test-review-core` runs the pure registry/outbox tests in optimized and ASan/UBSan configurations, then the reader and build-checker fixture tests. `make test-review` runs the focused 37-stage hidden reporting flow in both review configurations, preceded by the same 477-check desktop-safety probe. Use `TEST_ARGS='--config release'` or `--config sanitize` for a single affected configuration; `--no-build` deliberately reuses known artifacts. The runner serializes the worktree and desktop, uses a private outbox per run/configuration, verifies actual completion and stable source/binary hashes, and checks real image/frame identity plus durable acknowledgment/reply history. It rejects sanitizer diagnostics and partial stage counts.

Run linked production exclusion after building all four artifacts:

```sh
python3 tools/test_review_build.py --shipping ./duxcel --review build/review/duxcel-review
python3 tools/test_review_build.py --shipping ./duxcel-debug --review build/review-sanitize/duxcel-review
```

These inspect immutable artifact copies for linked review symbols, source paths, protocol/command strings and isolated build configuration; they do not launch applications. Hidden native acceptance still needs an unlocked macOS graphics session. A locked screen can prevent even a hidden context from being created; that is a recorded failed gate, not permission to activate a visible app. The native scenario exercises queued controller input and real GPU pixels, not physical Cocoa shortcut/IME delivery. Capture/write timings are recorded separately from ordinary app frame summaries; normal, capture and frozen composition frames are all sampled.

## Isolated web preparation

`python3 tools/web_toolchain_test.py` checks bootstrap fixtures without network or graphics. `python3 tools/web_toolchain.py status` verifies the installed local compiler; `python3 tools/build_web_raylib.py` builds the pinned dependency with one compiler process, validates every archive object as Wasm, and publishes its library atomically under `build/web/raylib/`. It uses a private copied source/build tree and does not use Homebrew's native archive. The dependency uses its upstream GNU99 web setting for Emscripten `EM_ASM`; application C remains C99. See [setup and constraints](../web/README.md) and [actual compiler/dependency evidence](validation/checkpoints/2026-10-17-web-toolchain/). Toolchain checks alone do not close browser acceptance. `make web` builds the shared App; `python3 tools/test_web_app.py --no-build` acquires the desktop lock and checks it in an owned headless Chromium profile. The W1 interaction proof is separate from W2 query/ownership acceptance.

## Real browser engine gates

```sh
make test-browser-core
python3 tools/build_web.py --worker-driver
python3 tools/test_web_app.py --no-build --worker-driver
make web
python3 tools/test_web_app.py --no-build --source tests/fixtures/transactions.csv --filters
python3 tools/test_web_app.py --no-build --source tests/downloads/yellow_tripdata_2025-01.parquet --busy
python3 tools/test_web_app.py --no-build --worker-probe
make test-browser-persistence
```

Core gates are independent release/ASan C ownership/SQL/model checks plus pure JS/packaging tests. The separate no-window C Worker driver runs 33 real semantic stages; the optional probe separately checks the official adapter/engine. Application source gates use its real Open command and browser chooser, wait for actual C page/count publication, and preserve wide/narrow captures. The CSV filter gate uses the actual descending-sort button, value checklist/Apply and year→month→day expansion. The Parquet busy gate requires a real View ribbon frame before its pending source job replies; this is not an input-latency benchmark. All browser runs acquire the desktop lock and use only an owned headless Chromium profile; no user browser or OS input is touched. Immutable App/driver outputs have separate manifests and latest pointers. The probe is served only by the test server, and the C driver/host is never packaged with the normal App. The loopback server avoids reverse DNS (previously35 s of setup); setup/browser/total timings are now explicit. Full browser input/DPR/GPU and reference parity are separate acceptance.

## Shared C browser reporting

The unified queue's focused pure gate is `make test-review-queue`. It covers strict atomic browser ingestion, native discovery, duplicate/conflicting IDs, competing leases, crash/restart recovery, bounded consumer failures and same-origin/token isolation. The Codex adapter's unit test uses a clearly identified fixture executable; it is not evidence of real model pickup. Real CLI acceptance is recorded separately with actual captures. No desktop is needed for this pure gate.

Browser input/transport checks can run without a browser:

```sh
node --test web/canvas_input.test.mjs web/canvas_surface.test.mjs web/canvas_wheel.test.mjs tools/test_web_cdp.test.mjs
```

The real manual gate (`python3 tools/test_web_app.py --no-build`, optionally `--worker-probe`) retains every keyDown/frame/keyUp phase and checks navigation default ownership on the actual focused canvas. Failures retain pending requests, bounded visibility/debugger/screenshot diagnostics and explicit diagnostic failures; `browser-process.json` and `cleanup.json` record the owned child and held keys. Disconnect is an immediate failure, not a retry. The normal/review host guard preserves printable input and modifier/composition boundaries; broader keyboard ownership remains separate. See [failure/control/guarded evidence](validation/checkpoints/2026-10-17-browser-navigation/).

```sh
make web-review
python3 tools/test_web_app.py --review --no-build --source tests/fixtures/transactions.csv
python3 tools/test_web_app.py --review --no-build --source tests/fixtures/transactions.csv --dpr 2 --canvas-transform
python3 tools/test_web_app.py --review --no-build --source tests/fixtures/transactions.csv --dpr 2 --canvas-transform --platform --filters
python3 tools/test_web_app.py --review --review-offline --no-build --source tests/fixtures/transactions.csv
node --test web/review_host.test.mjs
python3 tools/test_web_review_build.py --no-build
```

Review artifacts live separately under `build/web/review/`. The real C picker/composer stages an immutable archive in actual IndexedDB and submits it to the same-origin private service/queue. The gate reads the actual persisted manifest/PNG/sidecar, checks frame hashes and retrieves the C widget through the existing reader; no download is expected. `--review-offline` blocks only the owned page's review networking, commits a real report, reloads the same owned profile/origin, verifies unchanged pending bytes, then reconnects and requires one shared report and a retired pending archive. `--review-outbox PATH` joins a private native acceptance outbox for an integration gate. The inset-canvas case tests real pointer mapping rather than assuming devicePixelRatio is the framebuffer scale. `--platform` reads frame-bound C state for fractional pan/reversal, trusted wheel delivery, region isolation, independent pinch zoom, DPR-only changes and CSS-sized embedding, with captured GPU frames. Physical gestures, other browsers and broader reporting states remain separate. The build-contract gate checks positive review modules/exports and actual absence from normal Wasm, hosts/assets and native shipping binaries, including the uploader module/routes. Rebuild both normal/review targets before using its no-build mode. Earlier download-adapter evidence remains [historical](validation/checkpoints/2026-10-17-browser-review/).

`make test-browser-persistence` builds the native C codec into isolated release/sanitizer helpers and runs `tools/test_web_app.py --persistence`. Its private v12 fixture is never a user file. One owned official Worker rewrites a registered database copy, exports/reopens it, preserves the original hash, and returns bytes for native `workbook_load`/exact type checks in both configurations. The helper records binary hashes and runs under the desktop lock. `--no-build --persistence` requires both current helpers plus the accepted normal artifact. This BP0 proof does not test browser Save/Open controllers, blank-file creation, source relink or OPFS. `resources.json` records first completed C frame, parent resource timing and scoped memory/callback observations; it does not measure Worker RSS/GPU memory or input latency. [Current platform/file evidence](validation/checkpoints/2026-10-17-browser-platform/).

The `records` tier accepts `--config release|sanitize|both` and `--no-build`; it rejects `--jobs 2` because engine sequences remain serialized. Its warm release/sanitizer gate took1.666s locally. The `record-pages` native profile covers **[665,675)** after the canonical bootstrap: actual Parquet IDs/sort/filter/cache ownership, hidden business schema, detected file replacement, refresh and query fallback. Run `make test-affected FEATURE=record-pages TEST_ARGS='--modules page grid --config sanitize'`. Added stages bring the current layout range to[0,675) and inventory to1,108; older checkpoints retain their original counts. [Evidence and limits](validation/checkpoints/2026-10-17-record-pages/).

The Condition-select port has its own current optimized/sanitizer **66-stage filter** acceptance, with extra phases for date labels, backward Tab, Escape/reopen, region cycling and 150% pointer selection. Existing stage waits/assertions remain; the gate took35.612s optimized /37.611s sanitizer. The warm choiceview release+sanitizer gate took2.890s including1.702s rebuild; its8,780 checks/configuration record actual select painter calls without a GPU window. [Evidence](validation/checkpoints/2026-10-17-filter-condition/).

The focused chrome/motion follow-up adds a direct hidden platform helper in `tests/test_titlebar_native.m` (sanitizer build command documented at its top). Run it under the same `FileLock(native_lock_path(), ...)` desktop owner; it must never activate/order a window or inject OS input. The helper tests AppKit content/control geometry and actual GL pixels at80/100/150%, rather than claiming AppKit controls appear in a GL screenshot. `./tests/test_grid --overscroll` and its sanitizer counterpart isolate1,186 new physics/ownership checks; normal grid execution retains all prior assertions. The existing `--platform` browser gate now samples actual C elastic return/edge colors and keytip GPU fades. That earlier checkpoint used explicit DOM Alt events while the trusted driver crash was unresolved. The current input/lifecycle gate below fixes the protocol fields and uses trusted CDP throughout; physical OS menu/gesture delivery remains separate. [Commands, results and failed-driver evidence](validation/checkpoints/2026-10-17-chrome-motion/).


The current input/lifecycle gate uses trusted CDP keys throughout: `python3 tools/test_web_app.py --review --no-build --source tests/fixtures/transactions.csv --dpr 2 --canvas-transform --platform --filters --alt-stress`. It retains every protocol phase, records actual same-frame H1/HCC delivery, tests File-menu Delete isolation and cancel/discard Close, and samples real C GPU pixels during sustained pulls, decaying/gappy and alternating tiny tails. The old Windows-to-macOS native virtual-key driver bug is fixed; DOM bypass results in the earlier checkpoint remain historical. `node --test tools/cdp_keyboard.test.mjs tools/test_web_cdp.test.mjs` covers protocol fields and held-key cleanup.

For access-key/File-only edits, use `python3 tools/test_web_app.py --review --no-build --access-only --dpr 2` after building review Wasm. This separate blank-workbook profile passed 35 checks in 9.689 s: trusted same-frame H1/HCC, menu edit isolation, outside-click dismissal and guarded Close/Cancel/Discard, with zero database jobs. It rejects source/engine/platform/report-upload options and does not replace those acceptance gates. The native document profile additionally covers a completed outside click captured between frames.

`tests/test_pan_native.m` documents the hidden native adapter test/build: exact logical points and gesture phases, non-consuming frame snapshots, modifier transitions, Close/New/Quit menu/delegate semantics and shutdown, plus actual UIContext→grid input/rendering with bundled fonts, GL backing/edge pixels and current elapsed-time recovery. The 1,037-check sanitizer run took 1.473 s without posting OS input. Run under the desktop lock. The grid's focused `--overscroll` gate now has13,695 checks/configuration; the complete grid gate has 34,783, including ordered access-key input. These remain synthetic acceptance; actual hardware gesture feel, Space behavior, held-Alt/browser menus and IME are separate. [Current evidence](validation/checkpoints/2026-10-17-input-lifecycle/).
