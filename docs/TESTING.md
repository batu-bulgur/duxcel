# Testing

Product/engine/grid/codec assertions are primarily C; macOS integration uses Objective-C. Python owns process/fixture/desktop orchestration and browser lifecycle, while Node exercises the real Wasm/browser adapters. No test requires the retired mock.

## Feature development (default)

```sh
make test-feature FEATURE=containing-regions
make test-feature FEATURE=about
make test-feature FEATURE=cell-command
python3 tools/test.py feature --feature containing-regions --plan
make test-feature FEATURE=test-infrastructure
```

A feature plan in `tools/test_catalog.py` lists its owned boundary, selected C
modules, one focused GUI workflow, deferred cross-feature commands and the
milestone gate. `--plan` never builds or launches anything. The default CLI tier
is `feature` and requires a registered feature; it never silently selects every
module. Add/update the plan when implementing a slice. There is no automatic
claim that changed filenames reveal every dependency.

Local selection checks run only cells/ladder/controller logic in optimized and
ASan/UBSan configurations, then one real release-App GUI slice. They retain
terminal-stage checks, source/binary fingerprints, animation waits and the
shared native-service lock where the scenario accesses OS services. They omit
the broad background-window check and
unrelated modules/browser scenarios. Local About checks compile/link only the
metadata and AppKit adapter plus their test, exercise real scrolling and TEST
identity restoration, and compile the direct-C source-archive fallback. They do
not load the App, DuckDB or the canonical workbook.

`--config` controls the selected headless modules. Feature App GUI normally stays release; the named `recovery-menu` boundary gate
uses an ASan/UBSan App to verify actual File paint/activation.
standalone About GUI stays ASan/UBSan. Use explicit integration commands below
when full-App sanitizer or platform boundaries are affected. The receipt records
these configurations and the deferred commands. A focused pass is acceptance of
that boundary, not whole-product/native-browser parity.

Independent control fixtures are explicitly listed in `WIDGET_GUI`. They retain
hidden TEST identity, pinned binaries, the desktop lock and exact completion
contracts, but own a widget/window lifecycle instead of instantiating App.
Their fixture checks window closure before its terminal assertion; results count
widget stages separately. All App fixtures still require the genuine App
shutdown summary. A widget pass never establishes App/editor integration.

The `cell-command` gate runs only `test_cell_command` in optimized/ASan/UBSan
and a 13-stage release App fixture compiled separately from production. The
fixture drives the real editor/Enter/Escape/Undo/Redo path, simulates a competing
command against an open draft, verifies rejection without draft/history loss,
then saves/reopens a finite manual workbook and rejects the old session target.
It needs no canonical fixture or background-window bootstrap. Test-only hooks
are absent from production/review/browser App objects. Both command model and
GUI fixture are included in the explicit full native milestone inventory.

Its incremental browser interaction gate is
`python3 tools/test_web_app.py --review --review-cell-only`: real F2, UTF-8 draft,
review/cancel, commit and Undo at 100/150 percent. Wider document/workspace and
other browser scenarios remain deferred. The command API's session fences do
not establish persisted identity, global Undo, journaling or recovery.

## Interaction checks and milestones (explicit)

A feature's `deferred_interactions` are implemented tests, kept available without
running them after every revision. Run a named interaction when its shared
boundary changes or a reproduction needs it. Run the broad gate at a major
milestone/release; it retains mutable engine ordering and existing coverage.
Major milestones require local native full regression and maintained Wasm/browser
gates, not just successful compilation. Include browser adapter/orchestration
contracts and real shared-C/official-Worker App smoke/package checks; retain the
source-scoped component evidence and state any separate platform limitations.
After the coherent milestone passes, push its commits to origin, follow the
matching CI/CD run and verify the resulting deployment when applicable. A green
local gate or successful push alone does not prove remote CI/CD success.

```sh
make test-affected FEATURE=selection-ladder TEST_ARGS='--modules selection_ladder selection --config sanitize'
make test-native SCENARIO='statistics clipboard'
make test-milestone
# Separate explicit browser/platform milestone gates, as applicable:
python3 tools/test_web_app.py --review --selection-only --source tests/fixtures/null-text.csv
python3 tools/test_app_lifecycle.py
python3 tools/test_open_documents.py
```

The registered `engines` tier (`make test-engines TEST_ARGS='--config both'`)
runs the entire ordered engine inventory, current fixtures and registered process
fault/recovery gates through the same existing engine controller. It requires
serial execution and does not replace the standalone App, GPU or named native
scenario inventories. At a milestone, a failure confined to a test fixture can
be followed by this complete engine chunk and the remaining native scenarios,
retaining earlier passed App evidence only when product sources and those test
inputs/binaries remain unchanged. Keep the original failed report; record the
coverage union and source limits instead of claiming a failed raw run passed.

The native `full` gate also includes standalone About checks. It does not imply
that the separate browser/platform matrix passed. Do not schedule those gates
implicitly from a feature run. Existing low-level helpers remain available:

```sh
make test-fast TEST_ARGS='--modules grid motion cellreadout --jobs 2'
make test-fixture
make test-records
make test-formats
```

`test-fast` builds optimized and O1 ASan/UBSan by default. Use `--config release` or `sanitize` for a justified narrower run. `affected` uses an independently bootstrapped native profile and validates exact terminal stages. `--edit` selects isolated O0 App sanitizer objects; it does not replace optimized/O1 acceptance. `--no-build` requires callers to establish freshness.

`python3 tools/test.py --list` lists modules, native profiles and scenarios. Useful profiles include active-cell, selection-ladder, zoom-headers, shelves, source-controls, inspector-scroll and inspector-sql. Standalone module binaries are `build/tests/test_<module>` and `-debug`; native binaries are `build/bin/duxcel` and `-debug`.

Native automated windows are hidden by default and use a real graphics context/framebuffer. Only explicit `--visible` opts into foreground QA. Synthetic events do not establish physical gesture, IME, file-panel or OS-clipboard acceptance. Never operate or terminate a user's running app.

The macOS document-entry regression is `python3 tools/test_open_documents.py` after `make bundle` and a passing native `range-move` profile. Its finite manual-only input is the generated `build/tests/generated/layout-option-range.dxls` Save/Open output; the runner copies it to a space/Unicode filename in its private result directory. It compares real LaunchServices cold launch and Open in the same already-running process against direct-path framebuffer bytes. A test-only injected Objective-C library rejects NSAlert modal/sheet entry points before any alert appears. An intentional alert must exit with code 73, and each successful app launch must report that the guard loaded; framebuffer equality alone is insufficient. The library is built under `build/tests/` and never linked/packaged into the product. Rebuild handoff must distinguish a newly launched binary from a still-running old executable; `open -a` may reuse the latter, whereas `open -n -a` starts a separate instance. The hidden private bundle has a unique identifier and no default-handler rank, holds the desktop lock, unregisters itself and removes its app copy on exit. It never addresses the user's Duxcel instance. `python3 tools/test_app_lifecycle.py` covers callback queue ownership/order/capacity and repeat session lifetime in optimized/sanitizer binaries. `python3 tools/test.py native --scenario document --config both` checks the existing guarded document lifecycle (sanitizer App).

The W04 document workflow additionally captures `build/tests/results/current-workspace/document-prompt-100.png` and `document-prompt-150.png` from the real hidden App. It checks compact panel/button bounds with a long Unicode filename at normal size and 640×640/150%, then injects clicks through ordinary next-frame UI input for Cancel, Save and Don't Save. `python3 tools/test.py native --scenario document --config release` complements the sanitizer run. Lifecycle tests pass 38 checks per optimized/sanitizer binary, including actual AppKit Quit menu and Command-Q key-equivalent dispatch with text/modal context, plus clean termination through C. They do not send global keys or focus the user's app. The shared-C browser control regression is `python3 tools/test_web_app.py --review --review-controls-only --source tests/fixtures/transactions.csv`; disabled Save, Cancel retention and narrow 150% layout remain required. Native and browser desktop-owning runs execute serially.

W02 has a focused `range-replace` layout profile: `python3 tools/test.py affected --feature range-replace --config release --modules grid --jobs 2` and the corresponding `--config sanitize` command. Its 15 stages bootstrap an independent manual sheet and use real UI gestures/clicks for cancellation and replacement, compare exact values/styles/history, refuse stale approval, and Save/Open the result. `build/tests/generated/move-replace-demo.dxls` is the clean product Save used to prove cancellation leaves dirty state unchanged; `layout-replace-range.dxls` is the final round-trip output. They are generated test/demo outputs, not authored fixture inputs. Screenshots are `build/tests/results/workspace-layout/replace-range-100.png` and `replace-range-150.png`. `tests/test_workbook_model.c` owns the shared pure model checks for replacement holes, overlap, boundary, format/history and protected-area behavior. The browser controls profile above additionally exercises button/Enter-driven Move, default Cancel and one-action replacement/Undo/Redo through trusted input; exact small values are read back through the real cell editor. A broader physical-input matrix remains separate.

W03's default local gate is `make test-feature FEATURE=containing-regions`. Its explicit full-App sanitizer interaction gate is `python3 tools/test.py affected --feature containing-regions --config sanitize --modules cell_regions selection_ladder selection --jobs 2`. `test_cell_regions.c` owns finite shape/blank/tie/bridge/edge cases and the maximum 20,000-cell read-only stress input; it links only shared C cells/common code. The 14-stage real App workflow uses queued Command-A, checks data/format/history/request/Worker retention, captures `region-selection-100.png` / `region-selection-150.png` under `build/tests/results/workspace-layout/`, and reopens its clean product Save `build/tests/generated/region-selection-demo.dxls`. It is a test/demo output, not an authored generator input. The existing `selection-ladder` profile retains analytical/source transient-state coverage. The browser gate is `python3 tools/test_web_app.py --review --selection-only --source tests/fixtures/null-text.csv`; it seeds a separate manual sheet through real entry and checks eight new shape/finite-selection steps through trusted Ctrl/Cmd+A. Selection statistics are explicitly disabled through actual controls before zero-query assertions. Native and browser desktop runs remain serial; physical input acceptance remains separate.

## Engine and integration ordering

Ordinary `make`, `make debug`, `make bundle` and incremental native binary/object targets compile with eight parallel jobs by default. Use `make BUILD_JOBS=16` to change that budget or `make BUILD_JOBS=1` for a serial comparison (Apple Make 3.81 applies Makefile job flags after command-line `-j`). Recursive makes inherit the existing jobserver. Legacy whole-program module-test build recipes remain serial until they have compatible per-file objects; do not share their renderer stubs with the App. Mixed goals outside the admitted native build graph also remain serial.

Compilation parallelism is separate from executing tests. The core suite writes fixture databases consumed by workbook tests. Run core before workbook and finish one configuration before another overwrites generated files. Mutable engine suites remain serial. Read-only already-built pure tests may use `--jobs 2`; native desktop tests still have one owner.

```sh
make build/tests/test_core build/tests/test_workbook
./build/tests/test_core
./build/tests/test_workbook
make test-full TEST_ARGS='--require-data'
```

Each worktree owns its writable fixtures. Public data is cached at `data/downloads/`; `tools/download_test_data.py` reproduces optional sources only on explicit request. Missing required data fails; optional skips remain explicit. The canonical 310-row and large transaction fixtures are generated by Python using the existing DuckDB CLI and a finite v12 SQL input. `make test-fixture` separately checks native optimized/sanitizer load, exact sums/counts/PIVOT shape, SQL NULL/empty/literal text and product save/reload. No generator is compiled.

The complete maintained native scenario inventory is larger than an ordinary focused check. Full regression is appropriate at coherent integration/release boundaries or for changes to shared ownership/rendering, not after each cosmetic edit. No new full pass is implied by an isolated profile passing.

## Build, runner and review contracts

```sh
python3 -m tools.tests.test_runner
python3 -m tools.tests.test_build
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
python3 -m tools.tests.test_web_build
node --test --test-concurrency=2 web/tests/*.test.mjs
python3 tools/test_web_app.py --review --inspector-sql-only --source tests/fixtures/transactions.csv --dpr 2
python3 tools/package_web_pages.py
python3 tools/test_web_pages.py --directory build/pages
```

Use the existing pinned SDK and dependencies. Bootstrap/download only when explicitly needed; ordinary native builds do not install browser tooling. Browser runs own a temporary profile, ephemeral server and hidden Chromium process; they must close all three. Do not reload the user's browser or leave project ports running. The CI-specific `test-browser-core` uses the pinned header preparation and headless browser-core configuration.

Wasm source/flag queries keep Make diagnostics on stderr and read metadata only
from stdout. The build contract checks every source mode from both direct
Python invocation and an eight-job parent Make recipe, including inherited
command-line header overrides. Native recursive compilation keeps its shared
jobserver; a metadata warning cannot become a source path or compiler flag.

`tests/fixtures/ui/` contains finite accepted geometry/scroll/paint contracts. They are regression inputs, not a demand to match a retired external demo. Preserve assertions when changing intentional behavior, update the corresponding contract deliberately, and keep visual behavior separate from engine correctness.

## Outputs and retention

Disposable build/test output belongs under ignored `build/`:

- `bin/`: optimized/O1 native executables and executable-relative assets link.
- `obj/`: separate release/sanitizer/edit/review objects and dependency sidecars.
- `tests/`: module executables, `generated/` databases and `results/` logs/screenshots/summaries.
- Reference inputs live separately in `data/downloads/`; clean targets preserve them.
- `web/`: disposable Wasm packages/objects; `pages/` is production staging. SDK/raylib caches live separately in `.cache/web/`.
- `Duxcel.app/`: local signed macOS bundle.

The runner records source/binary hashes, commands, expected completion, exit status and elapsed time. Native/browser scenarios may capture selected states for inspection; not every build takes new screenshots and old captures are not automatically a before/after baseline. Meaningful visual comparison requires the same fixture, viewport, UI scale, sheet zoom and state. Keep failures or a few useful frames while investigating; remove obsolete outputs after integration. Do not commit full screenshot/event/log capsules.

`make clean` removes all of `build/`. `make distclean` also removes `.cache/` and `web/node_modules`. Both preserve `data/downloads/`, authored fixtures, assets/licenses, user workbooks and the external review outbox. Operation locks live in `build/locks/` and are removed too. A checkout-specific OS-temp shared/exclusive gate remains stable while cleanup deletes build output, preventing a new runner from locking a replacement inode. Legacy `.state/` is retired after refusing any active legacy owner. Legacy data/cache is migrated before cleanup; conflicting copies cause refusal rather than overwrite. A moved SDK may require offline bootstrap to refresh its absolute configuration.

Use `python3 -m unittest discover -s tools/tests -t .` for automation contracts, `make test-build` for the real miniature Make graph, and `npm test --prefix web` for browser adapter/contracts. Native suite sources remain under `tests/`; product-linked helpers under `tests/support/`; browser test-only hosts/probes and drivers under `web/tests/`. No test source is a disposable output. Only targeted tests are required during edits; full native/browser/OS acceptance remains separately scoped.

## Test identity and compiled provenance

Real App automated options (`--frames`/smoke) or `DUXCEL_TEST_INSTANCE=1` apply a
purple TEST strip to the runtime macOS icon and a TEST Dock badge; authored icon
assets and the normal bundle icon remain unchanged. Hidden/unfocused behavior
and shared-service locks remain required for actual OS interaction. The
Open/lifecycle runners set the explicit
role too. The standalone adapter check verifies the badge, different image,
no key/visible window, repeated panel lifetime and restored original identity.

About Duxcel shows the compiled revision, subject, author/date, build time,
working-tree status, a source-code fingerprint and a selectable, scrollable recent-40-commit history. It
never calls Git at runtime. `tools/build_info.py` generates a stable header;
only `build_info.c` includes it, so a new commit recompiles one small object.
Source archives/direct C compilation use an honest unavailable-history fallback.
`SOURCE_DATE_EPOCH` supplies a reproducible UTC metadata timestamp. Rebuild a
bundle after a commit to embed that commit; an already-running process retains
its compiled metadata. Use a fresh `open -n -a` process for hands-on validation.

The `journal-session` local gate owns native checkpoint-backed one-cell admission.
Its headless/engine tests retain optimized/ASan/UBSan coverage; its private App
fixture uses 18 completion-checked stages, hidden TEST identity and a per-run
backup root. It checks actual editor/keyboard/pointer behavior, retained drafts,
post-COMMIT acknowledgement, 150% Retry, checkpoint rotation, Save R/R+1, reopen
and discard. The storage kill/filesystem drivers run serially for each already-built
binary in their owning `workbook-journal`/`journal-queue`/`journal-faults` plans
and the full tier; ordinary checkpoint/UI changes defer the filesystem matrix. Full regression is registered, not
implicitly run after each journal change.

The Python test runner borrows an inherited GNU Make jobserver only when
launching recursive Make, retaining BUILD_JOBS rather than falling back to one
compiler. Test recipes expose those descriptors; Make -n/-t/-q still performs
no build/test launch. `tools/tests/test_build.py` verifies an actual Python
bridge with a two-slot parent and an eight-slot child request, preventing both
serial fallback and oversubscription. Test execution concurrency is unchanged.

`make test-feature FEATURE=recovery-crash` runs the real hidden TEST App crash/
restart driver in optimized and ASan/UBSan configurations. It holds the shared
desktop lock, owns every launched PID, verifies active-instance exclusion, kills
only its writer after durable acknowledgement, and restores the same document/
revision without changing recovery artifact hashes. Owned processes stop before
their temporary directory is removed. This process gate is also in the full
native milestone inventory; ordinary feature gates do not run it implicitly.

`workbook-schema` is the short schema edit loop: a pure C bounded proof module
in optimized/ASan/UBSan plus the narrow controlled-Save App workflow. The named
`workbook-archive` interaction runs core before workbook in each configuration,
preserving all large malformed-package/migration cases. It belongs at a changed
archive boundary or milestone. `test_workbook --object-archives-only` supports
the smaller strict/current versus legacy mark/measure edit check. The full
workbook suite remains available and is not trimmed for iteration speed.

Core/workbook engine binaries reuse the same native per-file objects as the
product and depend on actual compiler-generated headers. The isolated build
contract proves that unrelated UI headers trigger no rebuild, a local workbook
assertion edit rebuilds only its test TU, and real shared headers invalidate both
consumers. Mutable fixture execution remains serial; compiler parallelism does
not change the core-before-workbook contract.

The `history-preview` feature gate runs optimized/sanitized `app_preview` ownership
checks and a private hidden TEST App with 20 stages. It uses actual retained-root
jobs, keyboard/pointer version opening, sheet/definition navigation, offline-source
placeholders, mutation fences, exact Working Head/grid/editor/Undo/Redo restoration,
Close's dirty guard and a deterministic cancel-at-ready worker barrier. GUI hooks
are compile/link excluded from production; selected interactions remain separately
registered in `tools/test_catalog.py`.

The pure `history-window` gate owns optimized/sanitized retention planning cases:
V8→V9→V10, repeated Save, same-anchor aliases, A→B→A content reuse, older cursor,
Redo/unsaved suffix, fewer than three states, required-history gaps, arbitrary
boundary order, maximum named-state count and exhausted unsigned clocks. It
links only its actual C module/header, without a database or GUI. Durable event,
archive and cross-session replay acceptance remains separate roadmap work.

`history-preview-sanitize` is the explicit milestone interaction for the same
20-stage historical viewer workflow with the full-App ASan/UBSan object graph.
It belongs to the full native inventory and is deferred by the ordinary
`history-preview` feature plan. Standalone GUI receipts report the binary's actual
sanitizer configuration rather than assuming every feature GUI is release.

`history-anchors` runs its31-case public-directory proof and the existing33-case
window in optimized/sanitized C builds. Cases include cross-document/root/tag
misbinding, duplicate/unknown/orphan IDs, alias/content reuse, zero and exhausted
IDs, arbitrary physical order and the maximum1,025-anchor public inventory. The
module links only its real version-index dependency; no database, file decoder or
GUI is involved. Event continuity and actual cross-session history remain queued.

`history-viewer` is the small GUI-only edit gate for read-only paint/navigation;
it defers the unchanged reader/controller engines and full-App sanitizer variant.
Its private frame checkpoint rejects zero layout/grid geometry immediately after
sheet switches. Input waits for painted target rectangles. Queued native End,
visible offscreen text and exact Working viewport/selection retention are checked
through actual UI rendering, with no broad suite or human acceptance flag.

`history-path` runs the49-case structural path/join proof plus the anchor/window
modules in optimized/sanitized builds. Coverage includes shared ancestor prefixes,
abandoned suffixes, forked Saved rejection, complete Redo, prefix gaps, malformed
operation/lineage/position order, aliases, presentation-only boundaries, empty and
exhausted clocks,100,000 transitions and unchanged output on rejection. It has no
GUI/database and does not decode/admit durable event payloads; App history and
preview interactions remain deferred in the registered feature plan.

The same `history-path` gate now has77 path/join/protection cases. Independent
Saved/current branches cover overlapping/gapped floors, a fork before the prior
window, inconsistent shared operations, reused divergent IDs, attempted lineage
rejoin, stale high-water marks, impossible anchor positions on either path, empty
Saved and exhausted clocks. Every rejected protection plan preserves its output.
These are structural range obligations; payload closure and durable IO remain
separate gates. No unrelated GUI suite is run for this pure controller extension.

`cell-delta` selects its43 pure C checks, native journal queue/storage modules in
both configurations and the existing18-stage hidden App journal workflow. Exact
nonterminated/Unicode/DECIMAL/literal-NULL bytes, clear/inverse, immutable copied
inputs,64-KiB sides, arena sentinels and no-op/invalid/budget rejection are checked.
The native consumer keeps its existing single reservation allocation and schema;
filesystem faults/recovery interactions remain explicitly deferred by this plan.

`cell-delta-codec` selects only its canonical byte-oracle/closed-decode checks and
`cell_delta`, optimized and sanitized. It covers every truncation of the finite
oracle, unsupported encoding, reserved bits, target/length bounds, trailing bytes,
embedded NUL, no-op, raw Unicode/clear, maximum two-sided64-KiB values and unchanged
outputs/arena on rejection. Native journal/App and history interactions stay
implemented/deferred; this new family has no activated App/archive consumer.

`cell-recorder` selects715 command and43 delta checks in both configurations,
native journal-session modules and the18-stage hidden App journal workflow.
The command fixture audits reserve-before-mutation, coherent publish, exact copied
values, no-op/stale exclusion, backpressure preserving Redo/dirty state, malformed
adapter cleanup, stale-fence fault injection, model-limit cancellation and40
captured edits surviving32-entry session eviction. The named workbook-history
interaction is run when this producer/coordinator boundary changes. Full App,
filesystem faults and recovery versions remain separately registered coverage.

`history-stream` selects136 owner/protocol checks,1,001 command checks and82
codec checks in optimized/sanitized builds. Coverage includes copied raw values,
independent clocks, exact byte/entry backpressure while in-flight, FIFO retry,
wrong-owner completion, guarded teardown, canceled fork issuance, empty/exhausted
counters and a real40-edit producer keeping its first dispatched payload after
session history evicts eight actions. Storage acknowledgements are protocol mocks;
no durable IO/cross-session-history claim is made. Native recorder/path interactions
remain deferred because this new owner is not activated in the App yet.

`format-plan` selects the format module (optimized/sanitized) and a private
10-stage real App fixture. It verifies pointer Bold, native-key Italic/Underline,
Undo/Redo, canceled no-op/scoped candidates preserving Redo/mode/revision,
100/150 percent and versioned Save/Open with exact raw values. The source module
also checks independent nested record/scope ownership and late budget failure
preserving an existing plan. Wider148-stage format-controls and source-format
interactions remain deferred. Ribbon fixture input uses the production logical-
to-screen transform; hooks/stage state are absent from shipping builds.

`format-delta-codec` selects405 codec cases plus the format module in both builds.
A canonical byte oracle, all finite-oracle truncations, adjusted inner-frame cuts,
unsupported/reserved fields, bad bounds/values/guards, duplicate scopes/records/
fields, no-op records and failed replacement ownership are checked. Valid cases
cover binary scope bytes, mixed selector precedence, mode-only changes, complete
20,000-record/16,384-field vectors, exact large IDs and input-independent graphs.
A34-scope case requires exactly the owned graph plus peak uniqueness scratch;
under-reservation rejects before allocation. No App/source-version/file profile
consumer is activated by this codec; its named interactions stay deferred.

`codec-test-cache` verifies format/source-scope/format-codec runtime coverage in
both builds. Format reuses compatible native per-file objects. Source-scope uses
its own cached strict -Werror flags/signatures to retain that configuration
boundary. Compiler .d dependencies replace the global header inventory for these
targets; unrelated added headers cannot rebuild them. Mutable engine/desktop
execution remains serialized. A local unrelated-HEADERS probe records watched
object/binary mtimes under ignored build/tests/results; it is build-cache evidence,
not extra product acceptance or a reason to run all fast modules.

`source-versions-codec` selects271 asset cases and the13,894-case source-scope
module in optimized/sanitized builds. It checks canonical explicit kind tags,
pending/complete identity, unused-capacity normalization, ordered IDs, exact
memory limits, all finite-oracle truncations, incomplete RAW packets, owner/kind/
version mismatch, malformed locators, maximum ledger/locator, UINT64_MAX and
large file/version values without IO. Paths/packets remain owned after input
release. This does not establish file materialization, live Page verification or
historical-ledger adoption; final owner/format closure is separately scheduled.


## Parallel lane test execution

Initialize a private checkout identity once with
`python3 tools/lane.py --checkout "$PWD" --lane NAME --init`. For mixed focused
build/test work, use
`python3 tools/lane.py --checkout "$PWD" --lane NAME -- python3 tools/test.py feature --feature ID`.
The runner takes repository-wide ownership only during each compilation phase;
it releases that ownership before private tests execute. Direct compilation uses
`python3 tools/lane.py --checkout "$PWD" --lane NAME --build -- make -j8 TARGET`.
Do not wrap a complete test workflow in `--build`.

A same-checkout runner lease protects binaries for the complete run. App-bearing
feature executables are independent copies under
`build/tests/lanes/<lane>/<run>/`, named with `TEST-<lane>`. Receipts record the
original and pinned paths, SHA256, checkout and lane; runtime TEST identity and
hidden windows remain mandatory. These tests do not launch the normal bundle.
Reviewed process-local GUI input can run independently across worktrees. Actual
clipboard, native-panel and other shared desktop interactions retain a separate
OS-service lock; new unreviewed resource profiles default to that lock. Fixture
files and writable databases stay private to their checkout.
