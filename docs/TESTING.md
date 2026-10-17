# Testing without a long edit loop

Keep the existing coverage; choose a gate that matches the change. The warm module tests are fast. Compilation and native stage pacing dominate elapsed time. The previous full native checkpoint took about 14m20s, so it is an integration gate rather than the default after every edit.

```sh
# Fast headless release + ASan/UBSan (no desktop/clipboard).
make test-fast
make test-fast TEST_ARGS='--modules grid choiceview --jobs 2'

# Headless checks plus one independently initialized native feature slice.
make test-affected FEATURE=shelves TEST_ARGS='--modules grid marksview --jobs 2'

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
| `fast` | 17 headless modules, or named affected modules; release/sanitizers | Widget, geometry, formatting, motion and presentation edits |
| `affected` | Selected headless modules, canonical design fixture, named native layout slice | A visible control or interaction change |
| `full` | All headless modules; core then workbook for each configuration; GPU probe; 18 native workflows | Coherent integration/release boundary; shared lifetime/renderer changes |

Focused native profiles currently expose existing valid bootstraps: `statistics`, `dock`, `pane-motion`, `popup-motion`, `shelves`, `view-menus`, `source-menu`, `source-controls`, `shared-actions`, `author-menus`. `--list` records their exact stage ranges. Arbitrary numeric jumps are unsafe because the older layout harness is stateful. New active-readout/focus cases still need their own acceptance; an old menu slice does not imply they pass.

Core creates generated databases consumed by workbook/native tests. The runner preserves **core → workbook**, then completes that configuration before the next overwrites fixtures. It never parallelizes these mutable suites. Worktrees have separate `tests/generated/`; do not share it via symlink. The preview module's DuckDB connection uses its own private temp directory and reads authored input; it does not mutate those fixture databases.

The full native inventory is **988 maintained application stages**, plus **107 GPU checks**. NYC's eight stages require `tests/downloads/yellow_tripdata_2025-01.parquet`. `--require-data` fails if it is missing. Without that flag the runner records an explicit skip and reports `passed-with-skips`, never a full 988-stage pass. `tests/download_data.sh` can reproduce the existing public inputs; nothing downloads automatically during ordinary tests.

The full tier does not mean every optional stress/packaging/OS path was exercised. `make spill-test`, `make unique-large-test`, icon regeneration/verification, standalone distribution, physical trackpad/DPI/IME and manual file panels remain separately scoped checks. Run those when their area changes; Windows/Linux validation is deferred.

## Reproducibility and desktop safety

Every run writes individual logs and `summary.json` under ignored `tests/results/runs/<UTC>-<tier>-<pid>/`. The record includes Git revision/dirty state, aggregate source hash, binary hashes, commands, expected stages, exit/completion checks, elapsed times and explicit skips. Binary replacement or source changes during the run invalidate coherent acceptance. Keep implementation edits outside that worktree while its acceptance run executes.

A per-worktree lock prevents competing fixture/build runners. Native phases also acquire `/tmp/duxcel-native-tests-<uid>.lock`, independent of `TMPDIR`, so different worktrees cannot fight over windows, clipboard or preferences. The lock is nonblocking and explains the current owner. Manual application testing, direct legacy smoke targets, signing and bundles must be coordinated with this same desktop owner; they cannot be made safe merely by using another branch.

Success requires actual completion, exit zero and no sanitizer diagnostics, not merely a screenshot or an early PASS line. Native frame budgets remain failure timeouts. Existing fixed animation/stage waits remain intact. Long native waits are not evidence that pure tests are slow, and reducing assertion counts would not solve the main delay.

## Next build improvement

Current Make targets compile whole source sets and broadly depend on every header. `.NOTPARALLEL` also protects a real multi-output icon-generation race. The next tooling milestone is per-file objects with compiler-generated dependencies, distinct release/sanitizer object trees, coordinated icon regeneration and isolated fixture scheduling. Measure cold/warm builds before changing defaults. Do not simply remove `.NOTPARALLEL` or run `make -B`.
