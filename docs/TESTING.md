# Testing

Product/engine/grid/codec assertions are primarily C; macOS integration uses Objective-C. Python owns process/fixture/desktop orchestration and browser lifecycle, while Node exercises the real Wasm/browser adapters. No test requires the retired mock.

## Focused development

```sh
make test-fast TEST_ARGS='--modules grid motion cellreadout --jobs 2'
make test-affected FEATURE=active-cell TEST_ARGS='--modules grid motion cellreadout'
make test-native SCENARIO=base
make test-fixture
make test-records
make test-formats
```

`test-fast` builds optimized and O1 ASan/UBSan by default. Use `--config release` or `sanitize` for a justified narrower run. `affected` uses an independently bootstrapped native profile and validates exact terminal stages. `--edit` selects isolated O0 App sanitizer objects; it does not replace optimized/O1 acceptance. `--no-build` requires callers to establish freshness.

`python3 tools/test.py --list` lists modules, native profiles and scenarios. Useful profiles include active-cell, selection-ladder, zoom-headers, shelves, source-controls, inspector-scroll and inspector-sql. Standalone module binaries are `build/tests/test_<module>` and `-debug`; native binaries are `build/bin/duxcel` and `-debug`.

Native automated windows are hidden by default and use a real graphics context/framebuffer. Only explicit `--visible` opts into foreground QA. Synthetic events do not establish physical gesture, IME, file-panel or OS-clipboard acceptance. Never operate or terminate a user's running app.

## Engine and integration ordering

The core suite writes fixture databases consumed by workbook tests. Run core before workbook and finish one configuration before another overwrites generated files. Mutable engine suites remain serial. Read-only already-built pure tests may use `--jobs 2`; keep at most two compiler/engine processes on this Mac.

```sh
make build/tests/test_core build/tests/test_workbook
./build/tests/test_core
./build/tests/test_workbook
make test-full TEST_ARGS='--require-data'
```

Each worktree owns its writable fixtures. Public data is cached at `build/data/`; `tests/download_data.sh` reproduces optional sources only on explicit request. Missing required data fails; optional skips remain explicit. The canonical 310-row and large transaction fixtures are generated entirely by C/DuckDB; `make test-fixture` checks normal and sanitizer paths, exact sums/counts/PIVOT shape, SQL NULL/empty text and saved workbooks.

The complete maintained native scenario inventory is larger than an ordinary focused check. Full regression is appropriate at coherent integration/release boundaries or for changes to shared ownership/rendering, not after each cosmetic edit. No new full pass is implied by an isolated profile passing.

## Build, runner and review contracts

```sh
python3 tools/test_runner_test.py
python3 tools/test_build.py
python3 tools/test_review_build.py --self-test
make review
python3 tools/test_review_build.py
make test-review-core
make test-review-queue
```

The runner detects sanitizer diagnostics even with exit0, incomplete terminal summaries, timeout, binary replacement, missing data and lock conflicts. Cleanup refuses active runner/browser-build/desktop ownership before deleting files. Review exclusion checks inspect actual shipping and review symbols/strings; a Make graph check alone is not linked acceptance. Review services and agent dispatch are explicit opt-in development commands, never a native app dependency.

## Browser integration

```sh
make web
make web-review
node --test --test-concurrency=2 web/*.test.mjs tools/cdp_keyboard.test.mjs tools/test_web_cdp.test.mjs
python3 tools/test_web_app.py --review --inspector-sql-only --source tests/fixtures/transactions.csv --dpr 2
python3 tools/package_web_pages.py
python3 tools/test_web_pages.py --directory build/pages
```

Use the existing pinned SDK and dependencies. Bootstrap/download only when explicitly needed; ordinary native builds do not install browser tooling. Browser runs own a temporary profile, ephemeral server and hidden Chromium process; they must close all three. Do not reload the user's browser or leave project ports running. The CI-specific `test-browser-core` uses the pinned header preparation and headless browser-core configuration.

`tests/fixtures/ui/` contains finite accepted geometry/scroll/paint contracts. They are regression inputs, not a demand to match a retired external demo. Preserve assertions when changing intentional behavior, update the corresponding contract deliberately, and keep visual behavior separate from engine correctness.

## Outputs and retention

Everything generated belongs under ignored `build/`:

- `bin/`: optimized/O1 native executables and executable-relative assets link.
- `obj/`: separate release/sanitizer/edit/review objects and dependency sidecars.
- `tests/`: module executables, `generated/` databases and `results/` logs/screenshots/summaries.
- `data/`: optional cached public CSV/Parquet inputs.
- `web/`: pinned SDK/raylib plus disposable Wasm packages/objects; `pages/` is production staging.
- `Duxcel.app/`: local signed macOS bundle.

The runner records source/binary hashes, commands, expected completion, exit status and elapsed time. Native/browser scenarios may capture selected states for inspection; not every build takes new screenshots and old captures are not automatically a before/after baseline. Meaningful visual comparison requires the same fixture, viewport, UI scale, sheet zoom and state. Keep failures or a few useful frames while investigating; remove obsolete outputs after integration. Do not commit full screenshot/event/log capsules.

`make clean` removes generated outputs while retaining the installed SDK/raylib and public-data cache. `make distclean` also removes those caches and `web/node_modules`; do not use it for routine edit loops. Authored tests, small fixtures, assets/licenses, user data and the external review outbox remain. Git stores historical code/evidence; no separate archive tree is required in HEAD.
