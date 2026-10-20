# Repository boundaries and approach

Place files by responsibility, rather than by extension or the session that produced them. Implementation language follows API and process boundaries. Apply this rule to new work; do not rewrite working code merely to change language percentages. All documentation and developer-authored text is in English.

## Source and tooling boundaries

| Location | Responsibility | Language / runtime boundary |
| --- | --- | --- |
| `src/` | Native/shared product model, UI, calculation definitions, workbook and platform adapters | C99; Objective-C at the macOS API boundary |
| `tests/` | Behavior/memory assertions against real native product functions | C99; Objective-C for macOS integration |
| `tests/support/` | Test helpers linked to product APIs | C99, separate test executables |
| `tests/fixtures/` | Small immutable inputs/expected values with named consumers | CSV, versioned JSON or C pixel arrays |
| `tools/` | Process, file, network, build/packaging, test orchestration and external feedback queue | Python standard library; Objective-C for AppKit tools |
| `tools/tests/` | Tool failure, locking, timeout, packaging and queue contracts | Python unittest |
| `web/` | Browser APIs and official DuckDB-Wasm Worker integration for the same C application | Thin JS platform boundary; no second JS spreadsheet UI |
| `web/tests/` | Browser adapter tests, CDP drivers and test-only probes/hosts | Node/JS; excluded from application packages |
| `assets/`, `packaging/` | Runtime fonts/licenses/icons, intentional derivatives and platform metadata | Build inputs |
| `docs/` | One current plan/status, requirements, architecture and guides | No historical session reports |

Synthetic fixture generation is Python in `tools/create_workbook.py`. The existing DuckDB CLI converts CSV to Parquet and creates the fixed v12 input defined in `tests/fixtures/workbook/transactions.sql`. This is not a second general workbook serializer. `tests/test_fixture.c` does not generate fixtures; it opens them through real product APIs and checks PIVOT, exact DECIMAL, NULL/empty/literal text and save/reload behavior. Fixture generation requires neither a C build nor a new Python package.

Test orchestration is Python; native assertions against product functions are C. Browser API and real Worker checks are JS. These are distinct responsibilities, not arbitrary implementations of the same task in different languages. Exceptions need a concrete API/platform/performance reason, an explicit interface and relevant tests. Extracting large App/test-driver sections is separate maintenance work.

## Output, caches and downloaded data

| Location | Contents | Cleanup |
| --- | --- | --- |
| `build/` | Executables/objects, generated test DB/CSV/Parquet, screenshots/logs, Wasm packages and macOS bundle | `make clean` removes everything |
| `.cache/web/` | Downloaded SDK and raylib dependency cache | Removed by `make distclean` |
| `web/node_modules/` | Existing pinned browser dependencies | Removed by `make distclean` |
| `data/downloads/` | Reusable public reference data such as Yellow Cab | Preserved by both cleanup targets |
| `build/locks/` | Test/build operation ownership locks | Removed by both cleanup targets |
| `__pycache__/` | Ignored, reproducible Python bytecode | Not source, fixtures or durable records |

`clean` and `distclean` acquire the checkout-specific OS-temp gate exclusively, plus the desktop lock. Operations holding a build lock retain a shared gate, preventing cleanup from deleting active work and preventing new lock inodes during cleanup. Do not keep a root `.state/` directory. Migrate legacy `build/data` safely to the current data location; refuse conflicting populated copies instead of overwriting them. Ordinary clean also migrates legacy SDK/raylib caches. A moved SDK with absolute configuration may need an offline bootstrap before its next web use. User workbooks and the external review outbox are outside cleanup scope.

Download reference data explicitly with `python3 tools/download_test_data.py`. No test/build starts an automatic download. Optional `--roundtrip` uses the existing DuckDB CLI to write derived CSV/Parquet under `build/tests/generated/public-data/`; originals remain in the data directory. Do not delete durable inputs as generated output.

## Fixtures versus historical run output

A fixture needs a name, schema and real consumer. Seven small CSV files cover NULL/text/quoting/sort/chart cases. Three UI JSON files retain accepted panel/geometry/scroll states and expected values; the [fixture map](../tests/fixtures/ui/README.md) documents each. Two rendering headers are inputs to native pixel tests.

Historical source/prototype manifests, commands, timing/pass reports and unused capture states are not fixtures. Cleanup removed these from the JSON files, along with three old session/provenance files and a verifier tied to an obsolete screenshot directory. Accepted geometry/scroll/color projections remained unchanged and have independent fingerprint checks. Tool tests and browser drivers moved into their own test directories. Fixture generation is Python; product acceptance assertions live in `tests/test_fixture.c`.

Preserve runtime font/icon licenses and intentional checked-in derivatives. Do not restore the executable mock or visual studies. History lives in Git, without separate filesystem archives, bundles, tarballs or patch backups. Do not accumulate audit/archive/screenshot payload trees in HEAD. Old branch tips and worktree changes are retained under the Git tag `archive/streamline-20261020`; ROADMAP tracks unfinished work.

## Verification and remaining work

`tools/tests/test_layout.py` checks these role boundaries. Run tool/loader contracts, native fixture/sanitizer tests and focused native checks for relevant changes. Full native/browser/OS acceptance remains separately scoped. [STATUS.md](STATUS.md) records current capability limits; [ROADMAP.md](ROADMAP.md) records remaining product/persistence work.

Extracting native scenarios from the roughly 15,500-line App into a test-only boundary, incremental module-test compilation and splitting the large browser driver by workflow remain necessary. Layout cleanup does not establish these capabilities, autosave/formulas or browser workbook Open/Save.
