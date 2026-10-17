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
```

`--config release`, `sanitize` or `both` selects headless configurations. With `both`, the native phase uses the sanitizer binary. `--jobs 2` overlaps only independent **already-built read-only module tests**. Builds stay serialized under the existing Make rules; the runner does not enable unsafe parallel asset generation. `--no-build` is for deliberate reuse of known binaries/fixtures, with freshness the caller's responsibility.

## Tiers and ownership

| Tier | What runs | Appropriate use |
| --- | --- | --- |
| `fast` | 18 headless modules, or named affected modules; release/sanitizers | Widget, geometry, formatting, motion and presentation edits |
| `affected` | Selected headless modules, canonical design fixture, named native layout slice | A visible control or interaction change |
| `native` | Named existing complete application workflows, with only their required fixtures | A table/filter/document regression without unrelated scenarios |
| `full` | All headless modules; core then workbook for each configuration; GPU probe; 18 native workflows | Coherent integration/release boundary; shared lifetime/renderer changes |

Focused native profiles currently expose existing valid bootstraps: `statistics`, `dock`, `pane-motion`, `popup-motion`, `shelves`, `view-menus`, `source-menu`, `source-controls`, `shared-actions`, `author-menus`, `selection-start`, `active-cell`, `criteria-selects`. `--list` records their exact stage ranges. Arbitrary numeric jumps are unsafe because the older layout harness is stateful. The new active-cell profile has its own independent fixture bootstrap; selection-start validates the early assertion changes. Profile acceptance never implies the whole state matrix passed.

Core creates generated databases consumed by workbook/native tests. The runner preserves **core → workbook**, then completes that configuration before the next overwrites fixtures. It never parallelizes these mutable suites. Worktrees have separate `tests/generated/`; do not share it via symlink. The preview module's DuckDB connection uses its own private temp directory and reads authored input; it does not mutate those fixture databases.

The full native inventory is **1,053 maintained application stages**, plus separate **110 GPU** and **477 background-window** checks. NYC's eight stages require `tests/downloads/yellow_tripdata_2025-01.parquet`. `--require-data` fails if it is missing. Without that flag the runner records an explicit skip and reports `passed-with-skips`, never a full 1,053-stage pass. `tests/download_data.sh` can reproduce the existing public inputs; nothing downloads automatically during ordinary tests.

The full tier does not mean every optional stress/packaging/OS path was exercised. `make spill-test`, `make unique-large-test`, icon regeneration/verification, standalone distribution, physical trackpad/DPI/IME and manual file panels remain separately scoped checks. Run those when their area changes; Windows/Linux validation is deferred.

## Reproducibility and desktop safety

Every run writes individual logs and `summary.json` under ignored `tests/results/runs/<UTC>-<tier>-<pid>/`. The record includes Git revision/dirty state, aggregate source hash, binary hashes, commands, expected stages, exit/completion checks, elapsed times and explicit skips. Binary replacement or source changes during the run invalidate coherent acceptance. Keep implementation edits outside that worktree while its acceptance run executes.

A per-worktree lock prevents competing fixture/build runners. Native phases also acquire `/tmp/duxcel-native-tests-<uid>.lock`, independent of `TMPDIR`, so different worktrees cannot fight over windows, clipboard or preferences. The lock is nonblocking and explains the current owner. Manual application testing, direct legacy smoke targets, signing and bundles must be coordinated with this same desktop owner; they cannot be made safe merely by using another branch.

Success requires actual completion, exit zero and no sanitizer diagnostics, not merely a screenshot or an early PASS line. Native frame budgets remain failure timeouts. Existing fixed animation/stage waits remain intact. Long native waits are not evidence that pure tests are slow, and reducing assertion counts would not solve the main delay.

Completed native scenarios now stop after all enabled scenarios reach their terminal assertions, a subsequent final frame is drawn, and application/worker/publication state is idle. Document Quit retains its save/exit lifecycle. A matched export check took 19.153s before and 10.542s after with the same 1,000-frame ceiling; the new run needed 482 frames. Filter/query/document, combined export/icons and a two-frame failure budget were checked separately. The integrated release author-menu profile passed in 9.659s. This does not estimate a new whole-suite duration.

Current selection/readout/control integration has dedicated focused acceptance: 183 native stages and the GPU/background probes. This is not a fresh full-suite pass. Use [STATUS.md](STATUS.md) for the exact accepted source, optimization and scope.

## Hidden native windows

Automated App runs (smoke flags or `--frames`) and GPU checks create **hidden, unfocused, always-running windows before native creation**. They render the real GL framebuffer; captures do not require foreground activation or switching the user’s Space. The runner discards inherited `DUXCEL_TEST_VISIBLE` and records its chosen visibility. Every native gate first checks Cocoa invisibility/non-key state, app inactivity, external frontmost PID and actual pixels through swap/resize. This does not programmatically switch or pin macOS Spaces.

Use `--visible` with affected/native/full only for deliberately scheduled desktop QA. For a direct bounded manual launch, use `DUXCEL_TEST_VISIBLE=1 ./duxcel --frames 600`. Normal interactive launches remain visible. Hidden runs refuse real OS file panels; panel focus, menu/responder/IME routing, physical gestures/monitor changes and OS window lifecycle need explicit visible QA. Current clipboard scenarios still touch the system clipboard, so the shared desktop lock remains necessary even while windows are hidden.

## Next build improvement

Current Make targets compile whole source sets and broadly depend on every header. `.NOTPARALLEL` also protects a real multi-output icon-generation race. The next tooling milestone is per-file objects with compiler-generated dependencies, distinct release/sanitizer object trees, coordinated icon regeneration and isolated fixture scheduling. Measure cold/warm builds before changing defaults. Do not simply remove `.NOTPARALLEL` or run `make -B`.
