# Testing without a long edit loop

Keep the existing coverage; choose a gate that matches the change. The warm module tests are fast. Compilation and native stage pacing dominate elapsed time. The previous full native checkpoint took about 14m20s, so it is an integration gate rather than the default after every edit.

Measured on the integrated tooling: grid +choiceview, release and ASan/UBSan, **46,228 assertions in 0.452s** including warm build checks. All 17 read-only modules at the earlier tooling checkpoint ×both configurations passed **74,820 assertions in 39.734s**, including a 29.468s rebuild. These are local observations, not cold-checkout guarantees.

```sh
# Fast headless release + ASan/UBSan (no desktop/clipboard).
make test-fast
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
| `fast` | 18 headless modules, or named affected modules; release/sanitizers | Widget, geometry, formatting, motion and presentation edits |
| `affected` | Selected headless modules, canonical design fixture, named native layout slice | A visible control or interaction change |
| `native` | Named existing complete application workflows, with only their required fixtures | A table/filter/document regression without unrelated scenarios |
| `full` | All headless modules; core then workbook for each configuration; GPU probe; 18 native workflows | Coherent integration/release boundary; shared lifetime/renderer changes |

Focused native profiles currently expose existing valid bootstraps: `statistics`, `dock`, `pane-motion`, `popup-motion`, `shelves`, `view-menus`, `source-menu`, `source-controls`, `shared-actions`, `author-menus`, `selection-start`, `active-cell`, `criteria-selects`, `ribbon-collapse`, `ribbon-overflow`, `ribbon`. `--list` records their exact stage ranges. Arbitrary numeric jumps are unsafe because the older layout harness is stateful. The new active-cell profile has its own independent fixture bootstrap; selection-start validates the early assertion changes. Profile acceptance never implies the whole state matrix passed. `ribbon-collapse` covers stages **[620, 638)**: 18 checks for reserved narrow-ribbon layout, collapsed-group flyouts/Escape focus, live chrome above the drawer and Full/Compact/Icons/Focus resizing without changing source/result caches or idle SQL. Run it with `make test-affected FEATURE=ribbon-collapse TEST_ARGS='--modules grid --config sanitize'`. `ribbon-overflow` covers **[638,665)**: 27 checks for the More menu, disabled/history traversal, real Undo/Redo including leaving historical preview, anchors and keyboard return. `ribbon` preserves the original **[0,64)** workflow for adjacent selection/ribbon behavior.

Core creates generated databases consumed by workbook/native tests. The runner preserves **core → workbook**, then completes that configuration before the next overwrites fixtures. It never parallelizes these mutable suites. Worktrees have separate `tests/generated/`; do not share it via symlink. The preview module's DuckDB connection uses its own private temp directory and reads authored input; it does not mutate those fixture databases.

The full native inventory is **1,098 maintained application stages**, plus separate **110 GPU** and **477 background-window** checks. NYC's eight stages require `tests/downloads/yellow_tripdata_2025-01.parquet`. `--require-data` fails if it is missing. Without that flag the runner records an explicit skip and reports `passed-with-skips`, never a full 1,098-stage pass. `tests/download_data.sh` can reproduce the existing public inputs; nothing downloads automatically during ordinary tests.

The full tier does not mean every optional stress/packaging/OS path was exercised. `make spill-test`, `make unique-large-test`, icon regeneration/verification, standalone distribution, physical trackpad/DPI/IME and manual file panels remain separately scoped checks. Run those when their area changes; Windows/Linux validation is deferred.

## Reproducibility and desktop safety

Every run writes individual logs and `summary.json` under ignored `tests/results/runs/<UTC>-<tier>-<pid>/`. The record includes Git revision/dirty state, aggregate source hash, binary hashes, commands, expected stages, exit/completion checks, elapsed times and explicit skips. Binary replacement or source changes during the run invalidate coherent acceptance. Keep implementation edits outside that worktree while its acceptance run executes.

A per-worktree lock prevents competing fixture/build runners. Native phases also acquire `/tmp/duxcel-native-tests-<uid>.lock`, independent of `TMPDIR`, so different worktrees cannot fight over windows, clipboard or preferences. The lock is nonblocking and explains the current owner. Manual application testing, direct legacy smoke targets, signing and bundles must be coordinated with this same desktop owner; they cannot be made safe merely by using another branch.

Success requires actual completion, exit zero and no sanitizer diagnostics, not merely a screenshot or an early PASS line. Layout profiles additionally require the application’s exact `Layout smoke completed [start,end)` line; the full/named layout workflow requires `[0,665)`. This rejects a familiar PASS from a binary with an older stage inventory; it does not establish arbitrary `--no-build` source freshness. Native frame budgets remain failure timeouts. Existing fixed animation/stage waits remain intact. Long native waits are not evidence that pure tests are slow, and reducing assertion counts would not solve the main delay.

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
