# Roadmap

Continue product correctness, persistence internals and focused tests together. The native UI is the baseline; the former mock is retired. Native trackpad jitter/snap-back is implemented and user-confirmed on the current Mac. Reopen it only for a new reproduction. Browser workbook Open/Save and complete native/browser input coverage remain incomplete.

## Repository and C tooling

- [x] Remove executable mock, obsolete visual studies, duplicate document entry points and tracked historical screenshots/log payloads. Keep current requirements, real regression inputs, runtime assets/licenses and Git history.
- [x] Generate canonical/large CSV, Parquet and finite workbook inputs in Python using the existing DuckDB CLI; separately verify real native load/PIVOT/save/reload in optimized/sanitizer tests. No C generator.
- [x] Put executable/data/report outputs under `build/`; make clean semantics explicit and test ownership-safe cleanup.
- [x] Enforce role-based language/layout boundaries: C product-linked helpers, Python orchestration, JS browser adapters; tool/browser tests in explicit test subdirectories.
- [x] Separate reference data, dependency caches and stable locks from disposable build output; clean preserves data.
- [ ] Improve orchestration through its existing Python boundary with equivalent timeout, locking, failure/recovery and browser/platform coverage; no blanket language rewrite.
- [ ] Extract native scenarios from App into a test-only build boundary without losing assertions. Split cohesive App controllers only when directly needed.
- [x] Default incremental native App/review/helper builds to eight parallel compilation jobs, with one icon-generation owner and separate configuration/temp outputs. Keep mutable engine/desktop execution serialized.
- [ ] Extend compatible per-file compilation to legacy module-test builds, preserving renderer-stub/configuration boundaries before admitting their graph to parallel builds.
- [ ] Split the large browser integration driver by workflow behind one shared CDP/lifecycle layer; remove duplicated helpers instead of writing a second engine.

## Persistence and source identity

Shared C/native/official-Worker metadata slices already cover definitions, filters, cells/columns, IDs/counters, ledger/bindings and base format properties. These are partial reader components; tests remain essential.

- [ ] Complete end-to-end format-state/selector streams, scopes/memberships, combined ledger budgets and cross-owner validation. The recovered C/native tag reader and JS selector transport are tested components, not browser activation.
- [ ] Finish historical migration, whole-candidate validation, atomic App publication and writer/output adapters. Enable real asynchronous browser Open/Save only after these pass.
- [ ] Finish external source relinking/handles and explicitly bounded source-content identity. Session registrations alone do not establish cross-session persistence or arbitrary SQL dependencies.
- [ ] Verify OPFS recovery, quota/failure/crash handling and portable workbook export; pin compatible Worker/extensions/hosting requirements.
- [ ] Complete source-record format lineage through custom queries/reindexing and unsupported multi-owner/Table/PIVOT histories. Preserve verified native subsets without overstating universal activation.

Use [browser persistence](design/BROWSER_PERSISTENCE.md) and [source-format identity](design/SOURCE_FORMAT_IDENTITY.md) for detailed contracts.

## UI, interaction and platform work

- [ ] Close remaining dropdown/menu/inspector focus, clipping, resize and narrow-window issues against current native requirements, with real or honestly disabled actions. Do not consult or recreate the retired mock.
- [ ] Verify browsing, same-sheet empty/existing pivots, contextual Fields/Rows/Columns/Values/Filters/Marks, charts, value/date filters, Steps/SQL/history, ribbon modes, short windows and 150% scale as a coherent native/browser state matrix.
- [ ] Finish intended shadow corners/exiting behavior; the opt-in corner diagnostic remains red. Decide intentional rendering changes explicitly rather than treating historical pixels as immutable design.
- [ ] Complete broader Unicode glyph coverage within measured CPU/GPU budgets. The current replacement glyph preserves text; CJK/emoji and larger atlas activation remain unresolved.
- [ ] Validate physical pinch/Space gestures, browser trackpad delivery, Safari, browser zoom/display changes, IME/focus, clipboard permissions, WebGL/context loss and accessible production semantics. These coverage gaps do not mean the accepted native pan fix is broken.
- [ ] Keep Linux/Windows boundaries portable; do not claim native OS validation that has not run. Standalone native distribution remains unfinished.

## Analytical work

- [ ] Extend Marks to fields beyond current result grain, bounded continuous color, persistent labels and multiple bindings.
- [ ] Implement typed manual cells/formulas and an explicit imported-data overlay model.
- [ ] Add further transforms/joins/table nesting and recovery features through narrow designs with actual DuckDB tests.

Retain native/Wasm review and the persistent agentic queue as development infrastructure, with strict production exclusion. Review helpers passing does not close a product feature. Use small isolated implementation lanes, concise source-scoped results and hidden desktop-owned tests; do not store repeated full evidence capsules in source control.

## Recovered worktree work

Retiring a checkout does not complete its feature. Saved source is recoverable
through `archive/streamline-20261020:worktrees.json` and the snapshot IDs there.
The prior branch names/commits and unfinished authored changes are retained in
Git. There is no separate filesystem backup. Do not restore old checkpoint/report
trees into active source.

| Retired lane | Current integration and verified scope | Remaining work |
| --- | --- | --- |
| `duxcel-bp1-format-tags-native` | Recovered explicit v12/v13/v14 format awareness/selector reader and previously ignored native test include. Pure C and real native comparison pass release + ASan/UBSan. | Actual shared-C/official-Worker format-state/selector continuation, then scope/guard/member validation and candidate activation. |
| `duxcel-format-selectors-stream` | Recovered bounded 3-column selector transport and helper tests; column name corrected to native `kind`. 56 focused Node tests pass. | A real Worker/C decoder workflow, including 2-column awareness state, cancellation/retry and retained-budget admission. Node transport tests alone do not close this. |
| `duxcel-native-precise-pan` | Dirty frame-time/phase/close/keytip work already exists in main’s newer native adapter/grid. User-confirmed native pan stays accepted. | Broader browser/physical pinch/Space/IME matrix remains in UI/platform tasks; no old pan code is reapplied. |
| `duxcel-page-record-wire` | Dirty Page record ownership and wire-replacement assertions already exist in main’s stronger Page/source-identity/wire tests. | Portable source lineage and persistence activation remain separate engine/persistence tasks above. |
| `duxcel-workbook-format-rows-checkpoint` | Base format-row code is already in main. Remaining untracked files were disposable run reports; no external archive is retained. | Whole-candidate scope/member validation, migration and browser Open/Save remain open above. |

- [x] Recover the C/native format-state/selector reader without losing the original assertions.
- [x] Recover bounded selector transport helpers with native column names.
- [ ] Connect the recovered tag reader and both stream families through the real official Worker; verify failure, cancellation, retained Header and retry before marking this integrated.
