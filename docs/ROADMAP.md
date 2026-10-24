# Roadmap

Continue product correctness, persistence internals and focused tests together. The native UI is the baseline; the former mock is retired. Native trackpad jitter/snap-back is implemented and user-confirmed on the current Mac. Reopen it only for a new reproduction. Browser workbook Open/Save and complete native/browser input coverage remain incomplete.

## Feature iteration gate

Ordinary slices are complete when their scoped automated gate passes. Detailed
hands-on review belongs at major milestones and does not block the next slice.
Never label automated verification as human testing.

- [x] T01 · P0: Introduce explicit local/deferred/milestone feature plans. The
  default development gate selects owned logic and a narrow real GUI slice;
  full/background/browser/platform regression is explicit. Preserve source and
  binary receipts, sanitizer modules, terminal assertions, waits and desktop locks.
- [x] T02 · P0: Establish a real controller seam in selection expansion using
  borrowed resolved context and explicit outcomes. Standalone C checks require
  no App/worker/engine. Keep the existing analytical/source/statistics/clipboard
  interaction tests registered without running them after every local revision.
- [x] T03 · P0: Distinguish automated real-App instances with a runtime TEST
  icon/Dock badge. Add compiled About revision/author/date/dirty/source identity
  and scrollable recent history, with a Git-free source-archive fallback and
  metadata confined to one translation unit.
- [ ] Register each upcoming W05+ slice before implementation; extend its local
  feature test and add deferred cross-feature coverage alongside it. Extract
  the affected controller incrementally. Existing legacy scenario extraction,
  per-module object caching and browser-driver splitting remain the tooling
  tasks below, not prerequisites for every feature.

T01–T03 are infrastructure, not a change in W02/W03 milestone review or the
numbered workbook dependencies. W05a/W05b implement the admitted single-cell command foundation; W06 completes session chronology; W07 completes sheet lifecycle; W08 journal work is in progress.

## Repository and C tooling

- [x] Remove executable mock, obsolete visual studies, duplicate document entry points and tracked historical screenshots/log payloads. Keep current requirements, real regression inputs, runtime assets/licenses and Git history.
- [x] Generate canonical/large CSV, Parquet and finite workbook inputs in Python using the existing DuckDB CLI; separately verify real native load/PIVOT/save/reload in optimized/sanitizer tests. No C generator.
- [x] Put executable/data/report outputs under `build/`; make clean semantics explicit and test ownership-safe cleanup.
- [x] Enforce role-based language/layout boundaries: C product-linked helpers, Python orchestration, JS browser adapters; tool/browser tests in explicit test subdirectories.
- [x] Separate reference data, dependency caches and stable locks from disposable build output; clean preserves data.
- [ ] Improve orchestration through its existing Python boundary with equivalent timeout, locking, failure/recovery and browser/platform coverage; no blanket language rewrite.
- [ ] Extract native scenarios from App into a test-only build boundary without losing assertions. Split cohesive App controllers only when directly needed.
- [x] Default incremental native App/review/helper builds to eight parallel compilation jobs, with one icon-generation owner and separate configuration/temp outputs. Keep mutable engine/desktop execution serialized. Recursive Python test builds now preserve the inherited Make jobserver, including its explicit BUILD_JOBS limit; Make inspection modes do not launch tests.
- [ ] Extend compatible per-file compilation to remaining legacy module-test builds, preserving renderer-stub/configuration boundaries before admitting their graph to parallel builds. Core/workbook engine suites now share native per-file objects and actual header dependencies; fixture execution remains serial and core-before-workbook. Format and source-scope are now cached as well; other legacy renderer/stub graphs remain queued.
- [ ] Split the large browser integration driver by workflow behind one shared CDP/lifecycle layer; remove duplicated helpers instead of writing a second engine.

## Persistence and source identity

Shared C/native/official-Worker metadata slices already cover definitions, filters, cells/columns, IDs/counters, ledger/bindings and base format properties. These are partial reader components; tests remain essential.

- [ ] Complete end-to-end format-state/selector streams, scopes/memberships, combined ledger budgets and cross-owner validation. The recovered C/native tag reader and JS selector transport are tested components, not browser activation.
- [ ] Finish historical migration, whole-candidate validation, atomic App publication and writer/output adapters. Enable real asynchronous browser Open/Save only after these pass.
- [ ] Finish external source relinking/handles and explicitly bounded source-content identity. Session registrations alone do not establish cross-session persistence or arbitrary SQL dependencies.
- [ ] Verify OPFS recovery, quota/failure/crash handling and portable workbook export; pin compatible Worker/extensions/hosting requirements.
- [ ] Complete source-record format lineage through custom queries/reindexing and unsupported multi-owner/Table/PIVOT histories. Preserve verified native subsets without overstating universal activation.

Use [browser persistence](design/BROWSER_PERSISTENCE.md) and [source-format identity](design/SOURCE_FORMAT_IDENTITY.md) for detailed contracts.

## Excel compatibility and feature inventory

[EXCEL_SHORTCUTS.md](EXCEL_SHORTCUTS.md) owns feature/shortcut records, UI hierarchy and icon/appearance notes. This section owns batch ordering and acceptance gates. Not every Excel entry has been accepted into product scope. Mac comes first; Windows/web have separate acceptance. Preserve existing native/browser persistence and analytical work beyond the shortcut list.

- [x] Create the platform-specific inventory, source ambiguities, code-derived differences and editable behavior/design cards. This is a documentation deliverable, not implemented Excel behavior.
- [ ] **C01 — Grid data boundaries:** Define and implement Cmd+Arrow / Shift+Cmd+Arrow target/anchor/extent behavior for manual occupied/empty islands, source/Table/Pivot and uncached rows. Navigation must not dirty data or enter undo history. Prove pure C and integrated native input separately. Compare NULL/empty/formula-empty occupancy against Excel Mac.
- [ ] **C02 — Commands and bindings:** Separate Find/Replace/Go To, filter, Focus, refresh and diagnostics; decide Cmd+F, Cmd+Shift+F, F5 and F12 conflicts. Implement visible menu/ribbon alternatives and popup/editor focus priority. Until new bindings are accepted, the guide describes current controls.
- [ ] **C03/C04 — Editing and clipboard:** Accept F2, commit/cancel, Enter/Tab reverse directions, within-selection navigation, clear/delete, paste/move and undo across manual/read-only/object contexts. Link typed/source-overlay/formula dependencies to separate slices; TSV paste does not establish Paste Special.
- [ ] **C05 — Document safety:** Reproduce closing-related data loss with the actual binary/platform/close path. Verify native guards, Save/Save As/Close/Quit and the first recovery slice including untitled workbooks. Define saved/current/recoverable revisions and IO failure/cancellation behavior. Browser Open/Save retains its existing persistence gates.
- [ ] **C06 — UI placement and access:** Verify priority feature records' current/proposed ribbon > group > command/context paths and independent Excel ribbon references. Add icon/label/tooltip, disabled/pending/error, narrow/overflow/Focus and keyboard access to cards. Pointer, keytip and shortcut invoke the same data command; record physical-keyboard/screen-reader acceptance separately.
- [ ] **C07 — Fill, structure and formatting:** Schedule Fill down/right, shift-cells insert/delete, hide/unhide and number/border formatting with scope/undo/save acceptance. Formula reference movement depends on the general formula engine; source writes depend on identity/overlay design.
- [ ] **C08 — Advanced scope:** Design formula calculation/dependencies as a separate capability. VBA/Power Pivot/Copilot/macros/drawing/mobile entries are not automatic commitments. Basic grid fixes must not wait for all scope decisions.
- [ ] **Feedback batch links:** Link durable reports to feature/shortcut IDs and group shared root causes into selected batches. Separate triage, implementation, tests and milestone review. Capture must not require a continuously running agent. Extend the existing outbox/queue/reader with selection/grouping/export.
- [ ] **Documentation consistency:** Decide whether empty VARCHAR counts toward Filled/Distinct; resolve the INTERACTIONS target versus USER_GUIDE/source behavior through relevant C/statistics tests. Maintain document ownership through the [docs index](README.md#where-each-kind-of-information-belongs), without new dated audit documents.

This is the initial sequence; prioritize C05 if data loss/recovery is blocking. Close batches with shortcut acceptance, model/analytical effects and applicable Undo/Save/Open/recovery evidence. Selection/view-only changes do not wait for the entire persistence implementation.

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

## Workbook TRD implementation slices

The [workbook model TRD](design/WORKBOOK_MODEL_TRD.md) owns behavior and requirement/acceptance IDs; this section owns delivery order. Its former S0–S9 groups are decomposed below. **P0** protects editing/document integrity and establishes the command/recovery foundation; **P1** delivers data/formula capabilities; **P2** adds advanced composition/automation. Dependencies are hard prerequisites, not a requirement to finish every earlier row. A production data-loss reproduction takes precedence over the first convenience demo. No dates or unmeasured latency promises are implied.

Each row is a separately reviewable implementation/demo boundary. Scope expansion requires splitting the row, rather than hiding a subsystem rewrite inside it. Acceptance includes focused optimized/sanitizer C coverage, the relevant real native workflow, existing-file compatibility and applicable Undo/Save/Open. Distinguish synthetic App input from physical-device/milestone review. Schema/command slices also require the incremental browser gate below; native recovery need not wait for full browser Open/Save.

### First hands-on slice and immediate follow-ups

| ID / priority / state | Scope and dependency | Demo and acceptance gate |
| --- | --- | --- |
| W01 · P0 · User demo accepted | Manual cell-range selection and move gesture; existing range-move command only. No new model/codec. | Option/Alt-drag inside a selected range or drag its border; preserve the grabbed cell, show one outer outline, use a small drag threshold, cancel with Escape, ignore no-op. One existing Undo action, exact Redo and Save/Open; occupied/read-only destinations stay rejected. UX01–UX03; manual-range part of A01–A02. Floating/object gestures and region inference remain separate. |
| W02 · P0 · Complete | Explicit destination conflict feedback and Replace/Cancel for manual moves; W01. Integrates C03/C04. | Full-model/offscreen destination validation; default Cancel/Escape leaves values, formats, history and dirty state unchanged. Explicit Replace moves the entire rectangle, including source holes, with exact values/styles in one Undo/Redo action and identical Save/Open. Revalidate protected areas and reject a stale confirmation. Analytical-object ranges remain protected from replacement. UX04; manual-range A03. |
| W03 · P1 · Complete | Deterministic containing-region discovery; existing selection ladder and C01. | Four-neighbor manual components expand the ORIGINAL finite selection once per command; diagonals remain separate. Blank interiors use deterministic enclosing bounds. Format-only coordinates do not extend used data; ready relation bounds include NULL/empty positions. Strict supersets retain active cell/direction, data/history and viewport; discovery submits no query. WM02; region portion of A01. Cmd+Arrow boundaries remain C01. |
| W04 · P0 · Completed; user accepted | Compact unsaved/close dialogs and repaired macOS application-menu Quit/Command-Q; existing C05 guard, independent of journal. | Narrow/100%/150% windows fit actions and content; Don't Save / Cancel / Save preserve their current safety behavior, no forced startup filename. Both Quit entry points use the same dirty guard; clean Quit ends the session, Close retains the application. UX05; dialog/blank-workspace portion of A28 and F016/F017. |

W01 native acceptance: `python3 tools/test.py affected --feature range-move --config release --modules grid --jobs 2` and the corresponding `--config sanitize` run pass all ten App stages. The generated manual-only `build/tests/generated/layout-option-range.dxls` exercises values/styles, both move gestures, collision/no-op, Undo/Redo and reopen. C input checks also cover Escape, modifier release, blocked controls, bounds, zoom/variable widths and click threshold. The actual shared-C/official-Worker browser selection profile also passes 569 checks, including trusted Alt-drag and one Undo/Redo action. The user confirmed that the demo workbook opens and movement works. This accepts that hands-on slice, not the full platform/input matrix or A01–A02 for analytical objects and region discovery.

W01 demo unblock / C05 entry path: fixed missing macOS bundle workbook declarations and cold/warm Open Document dispatch. The real LaunchServices test opens the generated manual-only workbook with a space/Unicode filename in an isolated hidden bundle; cold and running-instance results match the direct-path framebuffer exactly. Native lifecycle and document-safety regressions pass. The user subsequently confirmed Open and movement; the remaining C05 journal/recovery work stays open.

W04 acceptance: the unsaved guard uses a 360×152 logical-unit panel at normal width, existing font/colors and 28-unit buttons. The ten-stage hidden native document workflow passes optimized and ASan/UBSan, including a long Unicode filename at 100% and a 640×640 window at 150%, actual UI clicks for all three actions, Save/Open values and failure preservation. The 38-check repeat-session lifecycle test passes both configurations, including real AppKit menu action/key-equivalent dispatch while text/modal context is active and clean termination through the C controller. The shared-C/official-Worker browser controls profile passes 1,185 checks, including disabled Save, Cancel retention and narrow 150% layout. The user subsequently accepted the compact dialog and confirmed physical Command-Q works; W04 is complete. W09 Save progress/publication and crash recovery remain separate.

W02 follows accepted W01 and reuses its atomic move/history boundary. W03 region discovery is implemented; W05 is the current command/revision foundation. `worksheet_check_move` distinguishes a protected/invalid target from a manual replacement candidate. The explicitly authorized replacement path retains the default reject-on-conflict API for other callers. The generated manual-only `build/tests/generated/move-replace-demo.dxls` opens with B2:D4 selected, occupied H9/I9 and AA20001/AB20001 destinations, and an E5 self-overlap target. Replace must be demonstrated through either Option-drag or the existing Move selection address dialog; Scoped automated acceptance is complete; detailed hands-on review remains a major-milestone activity.

W02 verification: `python3 tools/test.py affected --feature range-replace --config release --modules grid --jobs 2` and its sanitizer counterpart pass 15 native stages (7.83s/8.72s for the App workflow), including actual drag/Escape/Cancel/Replace input, full-model offscreen targets, clean cancellation, stale data-revision refusal, exact self-overlap Undo/Redo and Save/Open. The existing range-move and selection-start optimized regressions pass 10 and 45 stages. The shared model passes 186 optimized/sanitizer checks, covering holes, literal NULL/exact decimal text, formats, both overlap directions, the last worksheet cell, empty-source replacement, no-op and protected areas. The real shared-C/official-Worker browser controls profile passes 1,366 checks (34.36s), including button and Enter-driven Move, default Cancel, exact replaced value/style Undo/Redo and no source query. Source/binary receipts and screenshots remain under ignored `build/tests/results/`. Detailed physical/visual review is reserved for major milestones.

W03 follows W02 in the numbered roadmap without requiring another data-mutation subsystem. The shared sparse component reader replaces bounding growth with bounded O(n log n) traversal/O(n) scratch (about 180 KB of payload at 20,000 cells on this 64-bit host). It never changes cells, formats or history. The `containing-regions` native profile demonstrates B2:E5 → B2:E7 → B2:Z20001 → whole sheet, with reversed three-row seeds, blank interiors and an offscreen format-only exclusion. Its clean product Save is `build/tests/generated/region-selection-demo.dxls`; Detailed hands-on review is reserved for major milestones. W05a/W05b and W06 are completed below.

W03 verification: 133 sparse component checks and 170 existing ladder checks pass optimized/ASan/UBSan, including maximum admitted cell count, equal-area blank ties independent of insertion order and original model/history retention. The 14-stage hidden native `containing-regions` workflow passes optimized (7.43s) and sanitizer (7.34s); the existing Table/Pivot/source selection-ladder regression passes 27 optimized stages (13.06s). The real shared-C/official-Worker browser selection profile passes 705 checks (77.70s), including eight new actual Ctrl/Cmd+A shape/finite-selection steps plus prior analytical-owner and range-move coverage. Automatic selection statistics are disabled through the ordinary controls before query-retention assertions. Source/binary receipts and screenshots live under ignored `build/tests/results/`; no full physical-device/platform matrix is implied.

### Command, recovery and version foundation

Implement one existing manual-cell edit through each boundary before expanding the operation family. Continue existing persistence/codec work above; it is not replaced by this plan.

| ID / priority / state | Scope and dependency | Demo and acceptance gate |
| --- | --- | --- |
| W05 · P0 · Complete | Stable workbook/sheet IDs and revision-bearing C command boundary for one manual edit; existing C05/C03. | W05a/W05b cover persistent targets and the revision-bearing single-cell envelope; other mutation families remain outside this admission. AU01/HI03/PL01; foundation of A09/A18/A19. |
| W05a · P0 · Complete | Session-fenced, resolved single-cell command; existing cell/history model. | F2/type/click-away/close commit uses the shared producer. Draft captures cell and expected CellStore revision at open; stale data/target cannot be overwritten. Applied/no-op/stale/invalid/protected/limit outcomes are explicit. One existing Undo action, preserved Redo for no-op, exact text and Save/Open. Process-lifetime target fences survive rename/reorder but reset on load/snapshot. |
| W05b · P0 · Complete | Persisted workbook/sheet identities, non-recycled high-water and admitted-command revision; W05a. | Native v15 persists a 128-bit workbook ID, exact uint64 sheet IDs/high-water and command revision. Rename/reorder/reopen/snapshot preserve IDs; duplicate allocates a new ID. Invalid identity archives preserve the live book and prior file; exhaustion rejects allocation without blocking existing-book Save. v1–v14 migrate in RAM and upgrade on Save. Shared-C model/restore checks pass; the incomplete browser header gate still rejects v15 explicitly. Session fences remain independent; no journal/global Undo claim. |
| W06 · P0 · Complete | Workbook chronological Undo/Redo for manual edits on two sheets; W05. | Global session ordering selects the newest Undo / earliest Redo across stable sheet IDs. A→B→A replays in exact reverse/forward order; navigation follows the owner, including hidden sheets. Changed cell commands clear abandoned Redo across all sheets; no-op/failure preserve it. Cell/style composites remain atomic; display defaults stay live. A lost local-history interval stops global replay. Save/Open retains current data/revision but not session history. HI01/HI03; A09–A10 for admitted commands. |
| W07 · P0 · Complete | Undoable sheet lifecycle; W06. | Delete/restore a sheet with cells, objects, binding identities and formats; reorder/hide/duplicate use the same command history. HI02; remaining A09. Do not claim formula restoration before W26b. |
| W08 · P0 · Complete | DuckDB journal writer for the admitted command subset; W05–W06. | Measure single/batched edits and large paste on the pinned engine; ordered pending/durable acknowledgement, idempotent retry and bounded queue. Kill before/after commit, disk-full/write failures yield old/new complete state. No SQLite dependency. RE01/RE03/PL02; commit part of A13. |
| W08a · P0 · Complete | Native DuckDB journal transaction/receipt primitive; W05. | Private checkpoint lineage, exact before/after byte spans and uint64 IDs; bounded atomic append/read; identical operation-ID retry; reject payload/revision conflicts; process kill before/after commit preserves complete old/new state. Engine component only, no App durability claim. |
| W08b · P0 · Complete | Bounded owned asynchronous journal queue and App admission; W08a/W06. | Single mutation owner reserves immutable records before applying supported edits; immediate pending and ordered durable feedback; retries retain operation IDs, full/failing queue never silently loses an edit. Explicit boundary for unadmitted legacy/lifecycle commands. Narrow real-App gate; no startup recovery claim. |
| W08b1 · P0 · Complete | Native bounded asynchronous writer queue; W08a. | Reserve owned bytes before mutation; publish only after success, cancel on rejection; pending/in-flight/reserved share a hard bound. Ordered post-COMMIT acknowledgement, same-ID retry with retained payloads, failed-open recovery and explicit drain/abort shutdown. Engine infrastructure, not App coverage. |
| W08b2 · P0 · Complete | Checkpoint-backed App admission and pending/durable feedback; W08b1. | Capture valid lineage, reserve before a supported edit, publish after its successful command, retain drafts on backpressure. Unsupported mutation coverage must be explicit; document replacement/Save/close cannot silently discard a pending tail. Narrow hidden real-App gate. |
| W08c · P0 · Complete | Fault/performance integration and batching policy; W08b. | Actual write/disk-full/kill/close failures, measured per-edit tail latency, large-paste/checkpoint throughput and bounded backpressure. Legacy paste remains outside the one-cell journal admission until a resolved range command is added. Preserve all-or-nothing state and retry after uncertain commit; finalize W08 acceptance before W09. |
| W08c1 · P0 · Complete | Filesystem failure/retry matrix; W08b. | Real write-limit errors and targeted ENOSPC/EIO/short-write/fsync/close faults preserve complete states and receipts. A failed first creation removes only its newly created incomplete journal; queue records survive repair/retry. |
| W08c2 · P0 · Complete | Measured native admission/commit/checkpoint throughput; W08c1. | Pin runtime engine/library provenance; measure bootstrap, warm single edits, burst/backpressure and a finite 20,000-cell legacy paste followed by checkpoint. Set batching policy from evidence; no per-row journal claim for legacy paste. |
| W09 · P0 · Complete | Revision-fenced Save publication and proportional progress/cancel; W08. | Save R while R+1 arrives; report R only after its complete checkpoint is published, retain R+1 unsaved/recoverable. Failed/cancelled Save preserves old file and dirty state. UX05/RE03; A13/A15/A28. |
| W09a · P0 · Complete | Controlled transactional Save primitive; W08. | Cooperative cancellation before COMMIT, proportional completed work and explicit publication boundary; existing destination remains intact on rejection/cancel/failure. Native optimized/sanitizer module gate; no UI completion claim. |
| W09b · P0 · Complete | Worker/UI Save progress, cancellation and complete revision fence; W09a. | Hidden real-App Save/cancel/progress and R/R+1 tests; pending document actions cannot close over unsaved newer edits. Keep post-publication completion distinct from cancellable preparation. |
| W09b1 · P0 · Complete | Preserve ordered worker Save receipts; W09a. | Latest-wins read replacement/cancel cannot erase an admitted Save or its unpublished reply. Progress and cooperative cancellation are mutex-owned; publication closes cancellation atomically. |
| W09b2 · P0 · Complete | Compact Save progress/cancel UI and complete admission fence; W09b1. | Real keyboard/pointer cancellation and publication races, narrow/scaled panel, newer content remains dirty and pending close stays guarded. |
| W10 · P0 · Complete | Interrupted-session recovery candidate and explicit lineage choice; W08–W09. | Recover committed tail or open checkpoint, including unnamed workspaces; progress/cancel/failure preserves recovery material. New edits cannot silently discard a deferred tail. Persist generation/parent identity and baseline/journal linkage; two states sharing an admitted clock cannot be ordered by mtime alone. RE02; A14. |
| W10a · P0 · Complete | Durable checkpoint/journal origin binding; W09. | Persist generation/parent identity and baseline digest, reject cross-paired journals even with matching document/revision; legacy unbound tails stay conservative. |
| W10a1 · P0 · Complete | Immutable journal origin codec; W09. | v2 generation/parent IDs, baseline byte length/SHA-256 and original locator bind atomically before the first edit. Exact retry is idempotent; conflicts/malformed metadata reject; legacy tails cannot infer a baseline. |
| W10a2 · P0 · Complete | Checkpoint fingerprint and writer origin admission; W10a1. | Real prepared checkpoint bytes bind to the queued generation before acknowledgement; rotation records the parent. Preparation/binding retry retains payloads and exact origin. |
| W10b · P0 · Complete | Bounded recovery discovery and candidate replay; W10a. | Validate known private artifacts, baseline and contiguous tail into an isolated candidate with progress/cancel. Equal clocks do not order branches; preserve all candidates on failure. |
| W10b1 · P0 · Complete | Exclusive generation ownership and read-only journal access; W10a. | Another live process/instance cannot claim an active generation. Recovery inspection never creates or appends a journal; process death releases ownership. |
| W10b1a · P0 · Complete | OS-backed private generation lease; W10a. | Hold ownership from allocation through joined cleanup; another handle/process sees busy, and process death releases it. Acquisition/release never replaces the lock file; rejected paths/content stay intact. |
| W10b1b · P0 · Complete | Read-only journal inspection and interrupted v2 tail proof; W10b1a. | No create/bind/append through inspection; real pre/post-COMMIT kills retain a readable old/new complete state and immutable origin without checkpointing recovery material. |
| W10b2 · P0 · Complete | Verified isolated recovery candidate and bounded discovery; W10b1. | Stream baseline validation and contiguous exact-before-image replay into a candidate; cancel/failure leaves current model/artifacts intact. Preserve ambiguous branches and legacy/unbound checkpoints explicitly. |
| W10b2a · P0 · Complete | Isolated verified recovery candidate; W10b1. | Validate closed baseline and exact-before-image bounded tail projection without touching the live model/global draft or history issuers. Hold ownership; failure/cancel preserves all material, checkpoint inspection marks its deferred suffix. |
| W10b2b · P0 · Complete | Bounded native recovery catalog; W10b2a. | Enumerate known direct generations off the UI thread, skip active owners, expose bound/unbound/incomplete entries and ambiguous branches without pruning or mtime-based lineage inference. |
| W10c · P0 · Complete | Explicit startup recovery/lineage choice; W10b. | Recover committed tail or inspect checkpoint, defer safely, protect a deferred tail from subsequent edits and Save As, and preserve the user's other running instance. |
| W10c1 · P0 · Complete | Background recovery requests and owned page/candidate results; W10b. | Keep filesystem/engine work off App frames, cap row/result ownership, support page continuation and cooperative cancel/join without dropping leases or late results. |
| W10c2 · P0 · Complete | Startup/manual recovery choice and guarded adoption; W10c1. | Restore the complete candidate or explicitly fork from checkpoint; deferred tails survive cancellation, new edits and Save As. Current dirty documents stay guarded; original locators are not automatic write targets. Narrow/scaled hidden App gate. |
| W10c2a · P0 · Complete | Explicit recovery disposition and inherited branch; W10c1. | A checkpoint copy never retires its deferred tail; full recovery cleanup requires explicit Save/discard and unchanged known artifacts. New edits keep the recovered parent/original locator without an implicit Save target. |
| W10c2b · P0 · Complete | Startup/manual choice, guarded App adoption and compact UI; W10c2a. | Connect the independent reader to startup and File actions, preserve dirty/current documents and late results, and validate narrow/scaled hidden input. |
| W10c3 · P0 · Complete | Recovery crash/restart and foundation milestone regression; W10c2. | Kill only private TEST instances, verify committed recovery and active-instance exclusion, then run the registered major-milestone regression. No per-slice human approval flag. |
| W10c3a · P0 · Complete | Archive compatibility and bounded schema proof; W10c2. | Preserve strict v15 and genuine legacy upgrades, byte-for-byte failed-overwrite protection and exact typed metadata; batch catalog proof without relaxing row validation. Focused schema/Save gate plus explicit archive matrix. |
| W10c3b · P0 · Complete | Final foundation integration and native lifecycle/Open; W10c3a. | Exercise the registered native milestone coverage, verify owned crash/restart, lifecycle/Finder entry and hidden TEST boundaries, and record command/runtime limits before W11. |
| W11 · P1 · Complete | Latest Saved pointer and immutable named versions; W09. | Repeated Save updates one saved entry; unchanged tags share payload, older named states open directly. VE01–VE03; A11. Admit complete assets/results as their slices land. |
| W11a · P1 · Complete | Version identities, manifest/index invariants and Latest Saved codec; W09/W10. | Persist one replaceable saved pointer with stable document/state identities; legacy Open stays non-writing and first Save upgrades safely. Malformed references reject without replacing the live book or destination. Focused optimized/sanitizer codec gate. |
| W11a1 · P1 · Complete | Bounded version manifest/index controller and publication intents; W10. | Stable state IDs and one saved pointer; reject duplicate/dangling/exhausted metadata. Prepare changes without publishing them before successful IO. Recovery checkpoints never advance the user pointer. Pure optimized/sanitizer module gate; no archive/tag UI claim. |
| W11a2 · P1 · Complete | Immutable supported-state payload closure and native version codec; W11a1. | Archive every advertised root independently of the editable head, without recursive history copies. Reject missing/invalid payloads and preserve non-writing legacy Open/safe upgrade. Scoped archive gate; no tag UI claim. |
| W11a2.1 · P1 · Complete | Relocatable core snapshot references; W11a1. | Preserve resolved primary/Table/ledger locators across private payload relocation, including offline sources. Reject unresolved inputs before modifying a destination; retain controlled cancellation and ordinary Save behavior. Native payload/Save modules plus narrow Save GUI. |
| W11a2.2 · P1 · Complete | Sealed immutable payload ownership and content proof; W11a2.1. | Closed private core bytes, explicit lease/lifetime, bounded hashing and cancel/failure cleanup; no user publication or recursive history payloads. |
| W11a2.3 · P1 · Complete | Native version root/payload archive codec; W11a2.2. | Complete root closure, chunk integrity, non-writing legacy Open and safe upgrade/overwrite. No advertised unresolved roots; scoped archive gate. |
| W11a2.3a · P1 · Complete | Complete admitted core equality proof; W11a2.2. | Compare all current typed codec rows/columns with read-only IO and pre/post integrity, cancellation and bounded memory. No reuse based only on clocks, SQL or byte count. |
| W11a2.3b · P1 · Complete | Bounded version index and chunk codec; W11a2.3a. | Round-trip all reachable core payloads with exact bytes/closure and bounded per-chunk memory. Reject missing/duplicate/corrupt chunks; rollback cancellation/failure. No App publication yet. |
| W11a2.3b1 · P1 · Complete | Pure bounded chunk framing; W11a2.3a. | Reserved uint64 byte bounds, exact64 KiB framing, gap/duplicate/NULL/trailing/truncation rejection without allocation or IO. Shared optimized/sanitizer module gate. |
| W11a2.3b2 · P1 · Complete | Transactional native chunk encoding; W11a2.3b1. | Stage verified core leaves as bounded SQL BLOB chunks and typed roots/index inside a caller-owned explicit transaction/fresh schema. Reject autocommit/overwrite; caller rollback on cancellation/failure. Keep App publication separate. |
| W11a2.3b3 · P1 · Complete | Staged native chunk decoding and payload closure; W11a2.3b2. | Validate complete index/references/types/bytes/hash/core identity before returning owned leaves; no live-model replacement on failure. |
| W11a2.3b3a · P1 · Complete | Bounded typed version metadata reader; W11a2.3b2. | Validate shared codec inventory, NULLs, bounded labels/roots, positions/references/generations and explicit byte reservation under a borrowed read transaction. No payload/closure admission claim. |
| W11a2.3b3b · P1 · Complete | Owned chunk extraction and core admission; W11a2.3b3a. | Stream exact complete chunks into private leased leaves; validate hashes, inner v15 model/document association and full core root closure before return. |
| W11a2.3b3b1 · P1 · Complete | Strict owned core reconstruction from a bounded stream; W11a2.3b3a. | Exact bytes/hash, inner v15/document/absolute-reference/nonrecursive admission, fixed input blocks, cancel/failure cleanup and out preservation. |
| W11a2.3b3b2 · P1 · Complete | Whole root-set chunk extraction and closure; W11a2.3b3b1. | Reject orphan/missing/duplicate/trailing/oversized chunks and return only a fully admitted leased root set. Preserve metadata/live model on all failures. |
| W11a2.3c · P1 · Complete | Native archive compatibility and safe upgrade; W11a2.3b. | Connect version codec to staged native Open/Save with a new version discriminator, non-writing legacy Open and failed-overwrite preservation. |
| W11a2.3c1 · P1 · Complete | Staged native archive reader and discriminator; W11a2.3b. | Read-only legacy candidates or v16 head plus complete independent core roots; validate document/source identity and preserve older Latest Saved in an R+1 head. Ordinary App Open/Save stays unchanged until integration. |
| W11a2.3c2 · P1 · Complete | Safe native versioned writer/upgrade; W11a2.3c1. | Prepare owned output before commit, preserve existing valid artifacts/unrelated data, reject corrupt targets and source aliases, and cancel/fail without replacing the named file. Scoped native file gate before App adoption. |
| W11a3 · P1 · Complete | Latest Saved publication, Save purpose and recovery snapshot integration; W11a2. | Publish captured R only after successful user Save; retain R+1 dirty. A private R+1 checkpoint keeps the complete older Latest Saved R closure without moving that pointer. Failure/cancel preserves file, index and recovery material. Hidden Save/recovery gates. |
| W11a3.1 · P1 · Complete | Native Save-purpose planning and full-core reuse controller; W11a2. | User Save prepares a root/owner plan; unchanged Saved cores reuse identity. Checkpoints/exports preserve the supplied index. Owned receipts precede commit; no live App mutation. |
| W11a3.2 · P1 · Complete | Shared immutable store lifetime and background IO ownership; W11a3.1. | Retain closed core sets across QueryWorker/journal snapshots, cancellation and owner changes; release safely after all borrowers. Preserve pending/durable and R+1 fences. |
| W11a3.2a · P1 · Complete | Shared immutable core-set references; W11a3.1. | Independent worker/journal owners retain scalar mutex-protected references. Releasing one owner keeps bytes/lease alive; last disposal runs on its owner thread. |
| W11a3.2b · P1 · Complete | Native IO job envelope and background retirement; W11a3.2a. | Transfer typed native contexts through existing ordered file jobs; keep native dependencies out of ordinary Core-only tests, retain source stores and retire owned files off App frames. |
| W11a3.2b1 · P1 · Complete | Typed native file task/retirement boundary in QueryWorker; W11a3.2a. | Route owned Save/Load contexts, keep preparation cancellable, drain intrusive cleanup off mutex/frames and preserve ordinary worker/format ownership. Core-only focused tests plus narrow Save GUI. |
| W11a3.2b2 · P1 · Complete | Versioned native IO adapter and owned replies; W11a3.2b1. | Retain input core sets, call real version Save/archive reader on the existing worker, transfer prepared receipts/candidates and retire disk ownership in background. |
| W11a3.2b2a · P1 · Complete | Non-writing Saved baseline for opened legacy files; W11a3.2b1. | Retain opened Saved R before edits, so the first private R+1 checkpoint carries R even before upgrading the user file. Atomically adopt verified leaf/metadata ownership; preserve original bytes and dirty state. |
| W11a3.3 · P1 · Complete | App Save/Open and Latest Saved publication; W11a3.2. | Adopt prepared v16 candidates/receipts under current owner/index fences, preserve newer dirty edits, legacy Open and compact progress/cancel. Scoped hidden App workflow. |
| W11a3.4 · P1 · Complete | Private recovery checkpoint/version closure adoption; W11a3.3. | A private R+1 checkpoint retains Saved R without advancing it; recovery transfers the complete store, and deferred tails retain ownership. Scoped recovery interaction. |
| W11a3.4a · P1 · Complete | Version-preserving journal checkpoint admission; W11a3.3. | Background private v16 checkpoints retain complete older Saved roots without advancing them; root ownership survives rotation and caller release. Legacy callers remain compatible until App/recovery integration. |
| W11a3.4b · P1 · Complete | Full-store recovery and App checkpoint binding; W11a3.4a. | Recover head/tail plus independent Saved closure, transfer runtime ownership and update R+1 backup after named Save. Narrow journal/recovery UI gates. |
| W11b · P1 · Complete | Transactional named-state retention and tag operation; W11a3. | A tag binds a complete supported authored state; unchanged tags share its payload. Later Save cannot mutate older tagged cells/styles/definitions. Reuse requires complete content evidence, beyond command revision or SQL text. Cancel/failure leaves labels, pointers and the prior artifact intact. |
| W11b.1 · P1 · Complete | Pure owned Create Tag planning; W11a3. | Validate exact UTF-8 labels/duplicates/capacity, retain an owned plan and fence the atomic Saved-plus-tag generation without IO. |
| W11b.2 · P1 · Complete | Atomic native tagged-state capture/publication; W11b.1. | Full-core reuse, complete roots, transactional unique labels and failed/cancelled artifact preservation. No App/Tag UI claim yet. |
| W11c · P1 · Complete | Compact current-version/tag controls; W11b. | Create a named tag from a blank or named workbook, list one Latest Saved plus explicit tags, and show duplicate/invalid-name or IO errors without losing edits. Repeated Save creates no tags. Narrow hidden input gate; no routine hands-on-pending flag. |
| W11c.1 · P1 · Complete | Owned Tag worker/App publication boundary; W11b. | Copy UI label before ordered Save work, distinguish Tag receipts and fence full generation/label/root publication. |
| W11c.2 · P1 · Complete | Compact version list and Create Tag flow; W11c.1. | File entry, unique-label feedback, first destination, one Latest Saved plus explicit tags, narrow physical input/Save/reopen gate. |
| W11d · P1 · Complete | Isolated read-only named-version preview and return to Working Head; W11c. | Open an older tag directly, inspect its sheets/cells/styles/definitions, then return with the current draft, Undo/Redo and recovery continuation unchanged. Late jobs/cancel cannot adopt into the wrong owner. External availability and missing future retained outputs remain explicit; historical Save As is W13. |
| W11d.1 · P1 · Complete | Isolated owned historical core projection; W11c. | Select an admitted opaque root, verify/read/recheck it into an independent model with a historical reader and no Save path. Preserve working Undo/Redo and cancel without adoption. |
| W11d.2 · P1 · Complete | Owned historical read jobs/replies; W11d.1. | Retain protected roots through ordered worker reads, cooperative cancellation and late replies; no working-state adoption yet. |
| W11d.3 · P1 · Complete | Working Head suspension and return controller; W11d.2. | Move the actual owned book/history and runtime write owner rather than copying away Undo; preserve recovery and reject cancelled/wrong-owner preview adoption. |
| W11d.4 · P1 · Complete | Read-only preview UI and return; W11d.3. | Inspect retained authored sheets/cells/styles/definitions, block all mutation/publication routes and return to the exact Working Head. External results not retained by the current core remain explicitly unavailable. |
| W11e · P1 · Complete | Version milestone compatibility/regression gate; W11d. | Migrate App-produced-file assertions to explicit full-archive readers while preserving v15 fixture/legacy tests. Run the broad milestone gate once, plus package/shared-browser checks; record actual coverage and limits. No silent head-only legacy fallback. |
| W11f · P1 · Complete | Same-destination Save conflict protection; W11c, prioritized during W11d. | Refuse implicit Save/Tag against changed file identity or another v16 version index; preserve edits/peer tags. Only explicit accepted Save As can replace a changed destination. |
| W12 · P1 · In progress | Three-version-state retained Undo and reachability pruning; W10–W11. | With V10 saved, retain V8→V9→V10 plus unsaved suffix across reopen; old tags still restore; assets needed by retained states cannot prune. HI04; A12. |
| W12a · P1 · Complete | Explicit version-boundary retention window; W11. | Select three states/two intervals ending at Saved, coalesce only aliases of the same explicit version boundary, preserve an older cursor plus current Redo/unsaved suffix, and reject required history gaps. Pure planning only; no durable Undo claim. |
| W12b · P1 · In progress | Durable path/anchor and reversible-event contract; W12a. | Bind tag/Saved boundaries to proved operation positions independently of reused content roots. Validate branch/cursor/origin, contiguous reversible events and complete referenced payload closure; reject malformed/unsupported profiles atomically. |
| W12b.1 · P1 · Complete | Closed public anchor/index bindings; W12a. | Validate exact document, tag-publication identity, Saved root, anchor high-water and complete public references. Reused content/positions never substitute anchor identity; no event/payload or cross-session Undo claim. |
| W12b.2 · P1 · In progress | Event/path continuity and durable metadata profile; W12b.1. | Prove current lineage/cursor and retained ranges, classify checkpoint-only legacy history explicitly, and bind each reversible payload family before selecting a file profile. Never fabricate old Undo from saved snapshots. |
| W12b.2a · P1 · Complete | Structural event-path and anchor/window join; W12b.1. | Prove ordered lineage ancestry, exact retained transitions, cursor/Redo bounds and monotonic operation IDs. Ancestor prefixes remain on-path; abandoned suffixes stay checkpoints. A forked Saved endpoint cannot silently produce a successful empty window. Payload/file admission remains separate. |
| W12b.2b · P1 · Complete | Independent Saved/current-branch protection; W12b.2a. | Protect the Saved path's three-state window and a newer unsaved branch, including shared-prefix/fork bounds, without dropping older tags or counting abandoned suffixes as current Redo. Fail atomically on either missing interval. |
| W12b.2c · P1 · Queued | Closed durable metadata/profile admission; W12b.2b, W12c. | Bind exact typed reversible payload closure, path/anchor metadata and budgets before selecting a file profile. Admit legacy files as checkpoint-only history; unsupported/corrupt history cannot replace the live document. |
| W12c · P1 · In progress | Persist supported reversible payload families; W12b.2a. | Capture exact before/after cells, formats, analysis, objects, columns and sheet lifecycle payloads, including deleted sheets and stable identities. No raw process structs or session-order identities in the file contract. |
| W12c.1 · P1 · Complete | Sparse-cell reversible events and admission; W12b.2a. | Capture exact before/after bytes, clears, target identity and operation ID before mutation/ring eviction. Queue pressure or invalid ownership rejects without changing cells/history; no-op creates no event. |
| W12c.1a · P1 · Complete | Shared exact sparse-cell payload ownership; W12b.2a. | Validate target/text/clear/no-op and capture exact before/after spans into caller-owned bounded storage before mutation. Existing native recovery reservation uses the same shared controller without another allocation or wire migration. |
| W12c.1b · P1 · Complete | Closed sparse-cell payload encoding; W12c.1a. | Canonical target/value bytes with explicit family encoding, strict bounds, unsupported/truncated/trailing rejection and borrowed decode lifetime. No raw C struct or process/session clock in the payload. |
| W12c.1c · P1 · Complete | Retained event admission and producer handoff; W12c.1b. | Reserve owned event/envelope/payload before changing cells or evicting session rings, recheck owner/revisions and cancel on rejection/no-op. Preserve event IDs and exact inverse beyond session retention; activate cross-session history only after all families/publication are ready. |
| W12c.1c.1 · P1 · Complete | Atomic cell recorder seam and recovery adapter; W12c.1b. | Shared producer reserves copied exact payload before mutation, rechecks fences, publishes after coherent model/history/revision change, cancels on failure and avoids callbacks for no-op/stale input. Native recovery uses this same seam without extra allocation or synchronous IO. |
| W12c.1c.2 · P1 · Complete | Retained cell event owner/envelope handoff; W12c.1c.1. | Bind independent durable operation/lineage positions and canonical owned assets before ring eviction. Bounded backpressure cannot mutate data/history; event ownership survives pending persistence and session rings. Keep App retained history inactive until every producer/publication family is supported. |
| W12c.2 · P1 · In progress | Format/scoped-selector payloads; W12c.1. | Preserve complete before/after manual/result/record selectors, scope/membership bytes and composite bindings. Source-version high-water stays monotonic; no inferred record identity or live-default replay. |
| W12c.2a · P1 · Complete | Cancellable spatial/result format planning; W12c.1. | Prepare independent owned candidates and detect semantic no-op before reservation. Cancel/failure preserves live styles, mode, Undo/Redo and revision; ordinary apply uses the same planner. Narrow actual input/Save/Open workflow. |
| W12c.2b · P1 · Complete | Closed scoped-format payload encoding; W12c.2a. | Capture stored style properties, snapshot mode, exact opaque scopes, guard indices and record/original-field membership in canonical before/after bytes. Strict bounded owned decoding; derived rendering state and native structs are absent. |
| W12c.2c · P1 · In progress | Format event admission and source dependency closure; W12c.2b. | Reserve copied assets before commit, fence workbook/sheet/format/source revisions and preserve all referenced immutable source versions without rewinding high-water or display defaults. Reject partial composite bindings; coordinate range/analysis/lifecycle consumers in their own slices. |
| W12c.2c.1 · P1 · Complete | Closed source-version metadata assets; W12c.2b. | Preserve exact ordered version/owner/kind/locator/RAW provenance rows, pending/completed distinction and captured allocator bound in owned canonical assets. Decode is not live adoption and cannot justify counter/completion rollback or source materialization. |
| W12c.2c.2 · P1 · Complete | Format-to-source dependency closure; W12c.2c.1. | Pin referenced immutable rows across before/after selectors, prove owner/version/RAW/schema bindings, classify inactive opaque legacy scopes explicitly and reject missing/conflicting record dependencies without mutation. |
| W12c.2c.3 · P1 · In progress | Atomic format admission and event ownership; W12c.2c.2. | Capture format plus source dependencies before commit, reject stale owner/revision/budget, preserve monotonic source metadata and bind source/analysis/object composite constituents explicitly. App activation remains ordered publication work after all families. |
| W12c.2c.3a · P1 · Complete | Owned format-event reservation; W12c.2c.2. | Reserve canonical format and exact source-metadata assets together in the bounded cell/format FIFO. Cancel preserves clocks/Redo; incomplete composite effects reject. Source/sheet/definition preflight and complete model adoption remain producer responsibilities. |
| W12c.2c.3b · P1 · In progress | Fenced format producer and composite admission; W12c.2c.3a, W12c.3. | Recheck workbook/sheet/format/source state after reservation, then atomically adopt every constituent and publish once. Preserve prepared input on failure; cover analysis/object source-version changes before activating retained App history. |
| W12c.2c.3b.1 · P1 · Complete | Fenced manual-format producer; W12c.2c.3a. | Source-free/object-free spatial formatting reserves before mutation, rechecks durable/session/cell/format/document fences, then publishes one complete worksheet action. Failure preserves the owned plan; no-op preserves Redo and clocks. Normal manual App formatting uses this controller with session-only history. |
| W12c.2c.3b.2 · P1 · Queued | Source-bound composite format producer; W12c.2c.3b.1, W12c.3. | Admit source ledger plus primary/Table bindings and format assets together, preserving immutable baseline normalization and all source/readiness/revision fences. No partial constituent publication or early retained-history activation. |
| W12c.3 · P1 · In progress | Analysis and object payloads; W12c.1. | Capture source/query/filter/sort and Table/Pivot/Chart/Marks/name/geometry changes with stable IDs. App owns result invalidation/readiness; the codec retains definitions without querying external data. |
| W12c.3a · P1 · Complete | Closed primary/Table Analysis definition asset; W12c.1. | Preserve stable sheet/owner/source-version IDs, exact nullable texts, query/unique/group/aggregate, ordered filters/sorts and finite enum tags. Preflight owned memory and grammar scratch before copying; exclude derived schema caches and require verified rebinding before execution. |
| W12c.3b · P1 · Complete | Closed object/Pivot/Chart/Marks definitions; W12c.3a. | Preserve IDs, geometry, titles, source links and complete typed definitions without embedding runtime Pages or activation claims. |
| W12c.3b.1 · P1 · Complete | Closed Pivot/Chart query-definition asset; W12c.3a. | Preserve ordered role fields, exact names/filter values, aggregation and date-grain tags; enforce shelf/duplicate/grammar rules after bounded scratch admission. Share checked definition-wire primitives with Analysis; derived schema caches require rebinding. |
| W12c.3b.2 · P1 · Complete | Closed Marks bindings; W12c.3b.1. | Preserve all six ordered role bindings, measure/dimension distinction, aggregation and date grain with strict role limits. |
| W12c.3b.3 · P1 · Complete | Closed object snapshot/envelope; W12c.3a–W12c.3b.2. | Preserve stable IDs, geometry, names, source links, chart representation/measure and complete nested definitions under one owned bound. Captured allocator obligations cannot rewind live counters or runtime query epochs. |
| W12c.3b.3a · P1 · Complete | Allocation-free nested definition preflight; W12c.3a–W12c.3b.2. | Inspect exact header owner, framing/text and owned/scratch requirements before allocating any constituent of an object graph. Full decode remains mandatory for semantic/grammar admission. |
| W12c.3b.3b · P1 · Complete | Bounded complete object asset capture/decode; W12c.3b.3a. | Compose geometry/source links and all nested definitions, prove exact sheet/owner associations and source/Table graph closure, and preflight the complete owned graph before allocation/adoption. |
| W12c.3c · P1 · In progress | Analysis/object producer and composite event admission; W12c.3a–W12c.3b. | Reserve every changed definition/dependency before adoption, recheck source/request/workbook state and publish once; coordinate source-bound formats and preserve allocator high-water. |
| W12c.3c.1 · P1 · Complete | Primary/Table source-version closure; W12c.3b. | Pin exact owner/kind/locator rows across both definition states, preserving pending/complete distinction, explicit unbound observations and captured allocator bounds. Share immutable subset ownership with format closure; never infer source identity or execute queries. |
| W12c.3c.2 · P1 · Complete | Canonical definition/composite event ownership; W12c.3c.1. | Capture exact before/after Analysis/object/format constituents and their union of source dependencies before mutation; one event owns all assets under explicit staging/scratch budgets. No partial composite publication. |
| W12c.3c.2a · P1 · Complete | Authored definition/preimage projection; W12c.3c.1. | Compare exact persisted definitions/order without derived NumberType/query epochs. Ignore computed Pivot/Table extents, retain authored Chart dimensions, and keep schema/table identity strict until an explicit resolution owner closes baseline enrichment. |
| W12c.3c.2b · P1 · Complete | Owned canonical compound event; W12c.3c.2a. | Capture complete changed constituent pairs plus one union of source dependencies under explicit staging/scratch admission; immutable ownership precedes any model mutation. |
| W12c.3c.2b.1 · P1 · Complete | Bounded compound preparation without metadata clones; W12c.3c.2a. | Share parser-scratch accounting, select dependency indices atomically against one immutable ledger and encode their canonical union directly. Preserve format-event bytes; keep metadata-reader tests cached by actual dependencies. |
| W12c.3c.2b.2 · P1 · Complete | Closed compound packet and event handoff; W12c.3c.2b.1. | Budget all required constituent graphs/validation before allocation, capture their complete before/after payloads and source union, reject mismatched effects/no-op/owner bindings and hand off one immutable event before mutation. |
| W12c.3c.2b.2a · P1 · Complete | Compound format/source graph preflight; W12c.3c.2b.1. | Inspect format before/after framing/owned/uniqueness scratch and source-row header/order/owned needs before any constituent allocation. Full semantic decode remains mandatory. |
| W12c.3c.2b.2b · P1 · Complete | Atomic compound capture/decode and stream adapter; W12c.3c.2b.2a. | Bind exact changed family pairs and one source union, enforce effect/owner/grammar/budget closure, and hand off one immutable event without partial mutation or intermediate full-asset copies. |
| W12c.3c.3 · P1 · In progress | Fenced definition producers and App adapters; W12c.3c.2. | Recheck complete workbook/sheet/source/request/model ownership after reservation, adopt all changed constituents once, then publish. Preserve cancellation/Redo and runtime query invalidation; activate retained history only after every producer/publication family. |
| W12c.3c.3a · P1 · In progress | Baseline completion/readiness ownership; W12c.3c.2a. | Close worker-resolved default schema/table and other non-user enrichments against retained origins/replay. Do not turn them into user actions, silently waive changed source identity, or let cache extents invalidate authored preimages. Required before retained App activation. |
| W12c.3c.3a.1 · P1 · Complete | Resolved-relation event admission; W12c.3c.2. | Reject retained events containing unresolved DATABASE primary/Table definitions before reservation; preserve legacy/core packet compatibility and distinguish NULL default selection from invalid empty identifiers. |
| W12c.3c.3a.2 · P1 · Queued | Origin completion integration; W12c.3c.3a.1. | Establish legacy/new-source origins only after default relation completion, without rewriting admitted assets or manufacturing user edits; integrate with closed-profile admission before App activation. |
| W12c.3c.3b · P1 · In progress | Fenced definition/composite App adapters; W12c.3c.2b, W12c.3c.3a. | Prepare exact after-plans, reserve before session eviction, recheck all owner/source/model fences, then publish one complete action with correct query/cache invalidation. |
| W12c.3c.3b.1 · P1 · Complete | Same-connection primary transform controller; W12c.3c.2, W12c.3c.3a.1. | Prepare independent after-plans against the authoritative primary, admit parser/ownership/source closure, reserve before eviction, recheck durable/session/source/history fences and publish one action. No-op preserves Redo. |
| W12c.3c.3b.2 · P1 · Complete | Primary Remove duplicates App adoption; W12c.3c.3b.1. | Route Remove duplicates through the controller with bounded candidate/scratch admission and narrow input/Undo/Redo/Save/Open coverage. Preserve runtime invalidation and recovery checkpoint rotation. |
| W12c.3c.3b.3 · P1 · Complete | Remaining primary same-connection adapters; W12c.3c.3b.2. | Prepare sort/filter/group/reset/query after-plans before adoption; preserve schema/readiness and source-format budget ownership. Prove each migrated lane with focused module/input coverage. |
| W12c.3c.3b.3a · P1 · Complete | Primary sort/filter command adapters; W12c.3c.3b.2. | Share typed budget/adoption helpers with unique rows; prepare complete sort/filter plans, preserve runtime query/selection behavior and prove real ribbon input, no-op Redo, Undo/Redo and Save/Open. |
| W12c.3c.3b.3b · P1 · Complete | Primary group/reset/query adapters; W12c.3c.3b.3a. | Prepare complete after-plans for grouping, reset/clear and validated source queries, rejecting stale/unready input and preserving drafts and source identity. |
| W12c.3c.3b.3c · P1 · Complete | Primary pipeline step edits; W12c.3c.3b.3b. | Route filter/sort reordering and step removal through coherent command admission. Preserve cached result/query epochs for filter-display reordering while recording authored order and preserving Undo/Redo. |
| W12c.3c.3b.3d · P1 · Complete | Verified-query recovery readiness; W12c.3c.3b.3b. | A matching successful validation reply may repair a stale/failed prior result; ordinary field transforms stay blocked, with exact request/candidate/source epoch, connection and read-only fences retained. |
| W12c.3c.3b.4 · P1 · In progress | Source/object/composite producers; W12c.3c.3b.1. | Close complete source binding/replacement, object and format constituents before adoption, retaining allocator high-water and one chronological action. |
| W12c.3c.3b.4a · P1 · Complete | Same-connection Table source commands; W12c.3c.3b.1. | Prepare an independent full object after-plan, reserve before history eviction, recheck workbook/sheet/object/source/layout fences and adopt once with matching query invalidation. |
| W12c.3c.3b.4a.1 · P1 · Complete | Independent Table source transform planning; W12c.3c.3b.3. | Measure complete candidate/scratch before cloning, preserve source identity and allocator bounds, own all object definitions independently and leave live Undo/Redo/counters unchanged. |
| W12c.3c.3b.4a.2 · P1 · Complete | Fenced Table command adoption/App integration; W12c.3c.3b.4a.1. | Reserve complete DXDE assets before any live object-history eviction, recheck all owner/layout/readiness fences and adopt the plan as one action. Keep source/format replacement composites separate. |
| W12c.3c.3b.4a.2a · P1 · Complete | Table plan model ownership transfer; W12c.3c.3b.4a.1. | Move the old current graph into bounded Undo and consume the independent after-plan without copying again; preserve store counters and prove count/byte-pressure history transfers. No unchecked App command admission. |
| W12c.3c.3b.4a.2b · P1 · Complete | Table event reservation/fences and App adapter; W12c.3c.3b.4a.2a. | Bind durable/session/object/source/layout ownership, reserve before eviction, recheck and publish the whole action once; verify unrelated Table/Pivot caches and the smallest real GUI lane. |
| W12c.3c.3b.4a.2b.1 · P1 · Complete | Fenced Table event producer; W12c.3c.3b.4a.2a. | Freeze durable/session/request/object/source/layout state, reserve closed DXDE before eviction, recheck and publish one coherent object action. Reject pressure/stale/cancel without model changes. |
| W12c.3c.3b.4a.2b.2 · P1 · Complete | Table producer App integration; W12c.3c.3b.4a.2b.1. | Route same-connection Table transforms through admitted planning/adoption; retain replacement/composite owners and prove target/dependent/unrelated caches plus narrow input/Undo/Redo/Save/Open and recovery rotation. |
| W12c.3c.3b.4b · P1 · Complete | Object presentation command admission; W12c.3c.3b.4a. | Retain names, presentation and chart/Marks definitions as complete object events without unnecessary source queries; preserve source links, identity and layout ownership. |
| W12c.3c.3b.4b.1 · P1 · Complete | Shared object transaction fences; W12c.3c.3b.4a. | Extract the proven identity/request/layout/source/metadata fence for reuse by Table and presentation controllers; keep kind/readiness semantics in each command owner. |
| W12c.3c.3b.4b.2 · P1 · Complete | Object name command producer/App adapter; W12c.3c.3b.4b.1. | Capture rename draft identity independently from background result completion, reserve a complete object event before mutation, preserve caches/Redo and verify actual editor/Undo/Redo/Save/Open. |
| W12c.3c.3b.4b.2a · P1 · Complete | Admitted object name producer; W12c.3c.3b.4b.1. | Preserve exact rename draft identity/prior name, rebase unrelated background completion, reserve/recheck a complete object event and publish one title/action/document revision without source queries. |
| W12c.3c.3b.4b.2b · P1 · Complete | Rename editor App integration; W12c.3c.3b.4b.2a. | Keep stale/invalid drafts, preserve Table/Pivot/Chart Pages and bind actual editor/Undo/Redo/Save/Open to the admitted producer. |
| W12c.3c.3b.4b.3 · P1 · Complete | Chart mark/measure presentation commands; W12c.3c.3b.4b.2. | Prepare complete object plans for mark and selected-measure changes, reserve before mutation and preserve query epochs/Pages. Retain App scatter/schema availability checks, no-op Redo and exact title counters. |
| W12c.3c.3b.4b.4 · P1 · Complete | Marks binding definition commands; W12c.3c.3b.4b.3. | Retain all authored role/aggregate/grain/measure bindings as one event, preserve source/query boundaries and bounded unsupported behavior, and verify cache/invalidation plus Undo/Redo/Save/Open. |
| W12c.3c.3b.4b.5 · P1 · Complete | Matrix/chart representation and sizing commands; W12c.3c.3b.4b.4. | Recheck placement/manual/source overlap and complete dimensions before admission; preserve compatible cached results and explicit invalidation when representation semantics require it. |
| W12c.3c.3b.4c · P1 · Complete | Pivot definition commands; W12c.3c.3b.4b. | Admit complete field/aggregate/date/filter/reorder definitions with selected-measure remapping, bounded query revision and one object event. Keep App schema checks, no-op Redo and unrelated caches; verify real authoring, Undo/Redo and Save/Open. |
| W12c.3c.3b.4d · P1 · Complete | Source connection/binding replacement commands; W12c.3c.3b.4c. | Prepare primary/Table connection and analytical source-binding replacements with complete before/after dependency closure, explicit reset/invalidation and owner/source fences. Default-relation completion remains origin enrichment, not a manufactured edit. |
| W12c.3c.3b.4d.1 · P1 · Complete | Resolved Table connection/relation replacement; W12c.3c.3b.4c. | Admit changed file/relation/version bindings through the existing Table transaction, close old/new source metadata before eviction and invalidate only target/dependent results. Preserve no-op Redo, formats, stable IDs and unrelated Pages; worker default completion remains enrichment. |
| W12c.3c.3b.4d.2 · P1 · Complete | Primary connection/relation replacement; W12c.3c.3b.4d.1. | Bind the live App primary authority, source/readiness/owner fences and complete dependencies before one Analysis action. Support explicit relinking from unavailable prior results, preserve no-op Redo and keep default-relation enrichment separate. |
| W12c.3c.3b.4d.3 · P1 · Complete | Analytical object source binding; W12c.3c.3b.4d.2. | Admit changed source_id against an existing Table or primary owner, reserve full before/after graph/dependencies before eviction, preserve identity/layout/Marks and invalidate only the rebound object. Source/format composites remain W12c.3c.3b.4f. |
| W12c.3c.3b.4e · P1 · In progress | Object topology and placement commands; W12c.3c.3b.4d. | Admit create/delete/move with full graph/geometry/dependency validation and monotonic allocator obligations before eviction; preserve dependent-source deletion guards, chronological Undo/Redo and independent caches. |
| W12c.3c.3b.4e.1 · P1 · Complete | Admitted object deletion; W12c.3c.3b.4d.3. | Preserve dependent-Table guards and allocator high-water; reserve complete before/after graph/source union before eviction, support empty after-graphs and reject stale/pressure/cancel without mutation. Verify actual delete/Undo/Redo, independent caches and Save/Open. |
| W12c.3c.3b.4e.2 · P1 · Complete | Single-object placement admission; W12c.3c.3b.4e.1. | Reserve/recheck complete graph, source/manual/object extents and destination; preserve query caches, IDs and no-op Redo. Keep mixed range moves with structural/range composites. |
| W12c.3c.3b.4e.3 · P1 · Queued | Object creation admission; W12c.3c.3b.4e.2. | Prepare complete graph and monotonic new identity before publication, with resolved source/geometry ownership and one action. Keep worker origin enrichment and source-format composites explicit. |
| W12c.3c.3b.4f · P1 · Queued | Source/definition/format composite adoption; W12c.3c.3b.4d–W12c.3c.3b.4e, W12c.2c.3b.2. | Coordinate complete source ledger, primary/object binding and format effects under one reservation/recheck/publication. Prove no half-adoption on pressure, stale, cancellation or failure; close every remaining legacy producer before retained App activation. |
| W12c.4 · P1 · Queued | Structural and range composites; W12c.1–W12c.3. | Paste/move/replace/row/column actions retain their full effect set and exact inverse as one event. Preflight every constituent before adoption; failure cannot leave half an event or half a mutation. |
| W12c.5 · P1 · Queued | Sheet lifecycle and detached payloads; W12c.1–W12c.4. | Preserve create/delete/duplicate/reorder/hide/rename owners and detached contents across reopen. Cross-sheet ordering and identity high-water survive replay; all producer families are covered before durable history activation. |
| W12d · P1 · Queued | Ordered Save/checkpoint/Open history publication; W12b.2c, W12c. | Captured R owns its full retained interval and root closure; R+1 remains dirty/recoverable. Save preserves valid Redo; failure/cancel preserves the prior artifact/history and reports limits before any protected interval is lost. |
| W12e · P1 · Queued | Bounded cross-session history replay; W12d. | Reopen and Undo/Redo through the retained chronological interval under request/owner/revision fences. Preserve monotonic identity allocation and live display defaults; only an accepted changed edit abandons Redo. |
| W12f · P1 · Queued | Reachability-based history/root pruning; W12d–W12e. | Protect named checkpoints, retained events, current Undo/Redo, recovery frontiers and active readers. Prune only proved unreachable assets through atomic publication; no silent truncation to meet a byte limit. |
| W12g · P1 · Queued | Retained-range presentation and milestone gate; W12e–W12f. | Show actual retained Undo coverage and limits. V8→V9→V10 plus unsaved suffix survives reopen, older tags remain openable, and failure/branch/pruning checks pass at the explicit major milestone. |
| W13 · P1 · Queued | Historical preview Save As, initially current-state-only; W11. | Saving an inspected old state creates an independent artifact with its complete dependency closure; live workbook/recovery unchanged. VE05; first path of A27. |
| W14 · P2 · Queued | Selected-history export; W12–W13. | Export an explicit history interval with required assets and valid roots; omitted history is absent, new artifact restores every advertised state. VE05; remaining A27. |

W11 integration constraints: distinguish user Save/Tag, private recovery
checkpoint and historical export intents. A working checkpoint never advances
Latest Saved; every advertised saved/tagged state must retain a complete supported
payload closure. A successful R capture may update saved/tag metadata while R+1
remains dirty. Keep the write owner separate from historical display ownership,
even when both share a durable document ID. Payload reuse needs full supported
content evidence; current command clocks alone cannot identify every mutation.
In particular, a private checkpoint of R+1 may contain an older Latest Saved R:
its complete supported payload must travel with the checkpoint. State identities
must also remain unambiguous across independent Save As artifacts that share a
durable document ID. A flat current
root plus an old dangling saved ID is insufficient. W11a2 establishes this
closure before W11a3 publishes user-visible saved metadata.

W11a1 complete: the pure borrowed version-index controller validates opaque
32-byte root identities, explicit absent/present saved references, UTF-8 tag
metadata, reachability and bounded counters/capacity without decoding payloads.
Owned value plans separate user Save from checkpoint/export and fence publication
by transient write owner plus immutable index generation, independently of R+1
data edits. Same-root Save allocates no entry; failure preserves inputs/output.
The registered `version-index` gate passes229 checks per optimized/ASan/UBSan
configuration in2.433s (1.192s build);54 runner/catalog/web-inventory contracts
pass. Its test TU depends only on its real header/module. Native codec, asset
closure, Tag UI and Save/recovery adoption remain W11a2/W11a3/W11b; no new file
format or user-visible version history is claimed.

W11a2.1 complete: a separate native core capture stores resolved primary,
Table and source-ledger locators verbatim, so its v15 leaf can move between
private directories without rebasing external references. Offline absolute
locators remain references; unresolved paths reject before destination IO.
Ordinary Save still writes relative references. The `payload-paths` gate passes
364 relocation/offline/preflight/cancellation checks per optimized/ASan/UBSan
build, existing controlled/worker Save modules, and18 hidden native Save stages
in29.180s total (6.033s GUI). No external bytes, retained outputs, history or
user version publication are claimed. Sealing and full archive closure follow.

W11a2.2 complete: `core_payload` owns a new private directory, exclusive lease
and closed relocatable v15 leaf. The SDK SHA256 verifier uses fixed64 KiB reads;
exact identities and the two-file inventory fence consumption and cleanup.
Cancellation/failure cleans ordinary captures; changed/aliased/unknown artifacts
are preserved. Handles never publish user metadata or expose mutable DB handles.
The `core-payload` gate passes145 checks per optimized/ASan/UBSan configuration,
plus real lease tests, in4.263s with no App launch.54 runner/catalog/web inventory
contracts pass; native sealing stays outside Wasm. This is exact-byte core proof,
not semantic deduplication or a complete retained-data/result root.

W11a2.3a complete: native `core_compare` proves equality across all28 typed
v15 metadata tables using bidirectional EXCEPT ALL, read-only handles,64 MB
engine memory/private spill, pending-task cancellation and pre/post byte proof.
External query strings remain metadata. Changed text, NULL/empty/literal NULL,
styles, widths, definitions, names and counters differ even with unchanged
command/query/style clocks. Separate identical captures compare equal; file
mutation/cancel never publishes an equality bit. The `core-compare` gate passes
in16.390s (3.269s optimized/12.023s ASan/UBSan; no App launch), with full descriptor
coverage and targeted cancellation. Callback-dependent assertion counts are not
a stable inventory. Chunk archive IO/native file adoption remain next.

W11a2.3b1 complete: the shared pure chunk controller frames positive leaves
into exact64 KiB blocks under an explicit byte reservation. It rejects NULL,
wrong-length, duplicate/gapped/trailing and incomplete observations, including
uint64 boundary/offset overflow cases, without allocation or IO. The registered
`version-chunks` optimized/ASan/UBSan gate passes in1.674s with no App launch.
Framing is not a hash/asset proof; the native encoder/decoder must still verify
bytes, model identity and complete index closure before publication.

W11a2.3b2 complete: native `version_stage` streams verified leaves and bounded
index/tag metadata into a fresh reserved SQL schema, with an explicit aggregate
storage reservation and64 KiB chunks. Two stable transaction-ID observations
reject autocommit before mutation. The borrowed transaction/connection remains
caller-owned: READY is uncommitted; every failure/cancel requires caller rollback.
The scoped `version-stage` gate passes in4.352s including pure index/framing and
optimized/ASan/UBSan SQL tests. Exact chunk bytes, shared tag leaves, Unicode
labels, private visibility, multi-root cancellation, rollback, empty index and
existing-schema protection pass. This encoder neither changes native fileversion
nor publishes a user pointer; strict decoding/closure follows W11a2.3b3.

W11a2.3b3a complete: the bounded native metadata reader validates the shared
four-table/18-column codec inventory before typed row admission. It preflights
keys/labels/counts, enforces ordered positions, UTF-8 labels, generations,
references and reserved aggregate bytes, and owns returned arrays independently
of SQL results. Pure transaction proof is shared with the encoder. The scoped
`version-metadata` gate passes378 reader checks per optimized/ASan/UBSan build
plus the adjusted staging module in6.220s; cancellation leaves out unchanged.
Zero-byte-pattern keys remain valid identities. Metadata READY explicitly does
not admit chunk contents; extraction/hash/inner-model closure is W11a2.3b3b.

W11a2.3b3b1 complete: bounded sequential input restores a new private leased
leaf with exact size/hash, strict v15 model/document admission and post-load
integrity. The strict leaf reader rejects stored relative locators, unrelated
tables and recursive version schemas without replacing its output. Ordinary
legacy Open is unchanged. The `core-restore` gate passes782 restoration checks
per optimized/ASan/UBSan build plus refactored core ownership/relocation modules
in16.463s, with zero App launches. Copy/hash/admission/recheck cancellation,
wrong proof/document/version and rehashed invalid core bytes preserve out and
clean ordinary staging. Whole root-set extraction is W11a2.3b3b2.

W11a2.3b3b2 complete: native core-set restoration preflights total chunks/bytes,
NULLs/size/orphan roots, then independently reconstructs every referenced leaf
with exact ordered framing, hash and inner-model/document admission. Only a
complete owned set returns READY; missing older tagged bytes also reject. Valid
sets survive closing the outer connection and releasing original leaves. The
`version-closure` gate passes325 closure checks per optimized/ASan/UBSan build,
plus strict restoration/metadata modules, in22.479s. Corruption/duplicate/gap/
trailing/orphan/missing-root cases and cancellation leave no partial output.
Catalog strings are bounded; strict core shape is proved before any metadata
view could execute. This completes codec-v1 core closure, not native fileversion
upgrade, App adoption or future retained Data Model/result assets.

W11a2.3c1 complete: the native staged reader admits read-only legacy models or
a v16 head plus complete independent codec-v1 core roots, with matching document
IDs and unchanged source identity. A checkpoint head R+1 correctly retains the
older Latest Saved R and tag bytes. Legacy headers with hidden version metadata
reject; v16 requires closed bytes without WAL. The `archive-read` gate passes144
checks per optimized/ASan/UBSan build in12.730s, including v15/v14 non-writing Open,
exact file hashes/identity, cancellation/cleanup and missing older-root rejection.
The prepared candidate owns transferable model/core-set outputs. Normal App
Open/Save still uses v1-v15; safe writing/upgrade and App adoption remain next.

W11a2.3c2 complete: native v16 writing extends the existing transaction through
an internal typed adapter, leaving ordinary v1-v15 Save behavior unchanged.
Existing core/version closure is admitted before mutation; unrelated tables stay
intact. New core sets/receipts are owned before cancellable PUBLISH/COMMIT; optional
post-close identity capture cannot report a false failure. Borrowed leaf and
external source aliases reject before writing. The `archive-write` gate passes210
new checks per optimized/ASan/UBSan build, controlled/worker Save and archive-read
modules plus18 hidden Save stages in69.092s (5.726s GUI). Cancel during inspection,
chunk writes, receipt preparation or PUBLISH rolls back/removes new output;
v14 upgrade, corrupt-target preservation and older Saved R in R+1 pass.54 tool
contracts and the shared-C Wasm build pass; browser persistence remains unavailable.
W11a2's current core-profile codec is complete. App Save/Open/publication and
private recovery adoption remain W11a3; future retained data/results stay separate.

W11a3.1 complete: the native Save controller captures/fully compares User Save
cores, reuses unchanged Saved roots and prepares the pure owner/generation plan
before transactional writing. Checkpoints/exports preserve supplied metadata;
state-only export explicitly omits history. Caller-owned head/path/dirty/index
remain untouched. The `version-save` gate passes in21.924s with optimized/
ASan/UBSan controller tests plus the pure index module, without App launch.
Repeated Save adds no root/generation, data changes with identical command clocks
still differ, and private R+2 keeps Saved R+1. Capture/compare/final-PUBLISH
cancellation preserves the named file/base. Internal capture PUBLISH is explicitly
separate from named-file cancellation. Worker/App/journal lifetime adoption follows.

W11a3.2a complete: immutable core sets now use mutex-protected owning references.
Worker/journal snapshots can retain their own lifetime without duplicating bytes
or borrowing a replaceable App pointer. Last release owns disposal; getters stay
borrowed/immutable. The `version-lifetime` gate passes in10.789s with optimized/
ASan/UBSan two-thread ownership/lease checks (2,000 reference churn iterations)
and the existing full closure module. Dropping the receipt/first consumer keeps
files locked; last background release removes ordinary private artifacts.
Make prerequisites were checked to include the actual native modules. Background
retirement and job-envelope integration remain W11a3.2b, before App adoption.

W11a3.2b1 complete: QueryWorker now transfers typed opaque native Save/Load
contexts into owned replies, exposes cancellable preparation, and drains intrusive
retirement outside its mutex before pending work/stop. Replaced/canceled objects
are released after unlocking, so disposal can safely reenter scalar worker APIs.
Ordinary jobs retain their existing path. The `worker-native` gate passes in24.495s
including optimized/ASan/UBSan protocol, ordered Save, record ownership and source
digest tests plus18 hidden Save stages (6.165s GUI). Existing record/digest worker
checks now reuse actual per-file Core objects and join the milestone inventory;
no monolithic/global-header build is needed for these focused targets. Protocol
fixtures use real ordinary Save/Load; the versioned IO adapter remains W11a3.2b2.

W11a3.2b2a complete: admitted legacy files can seed an owned runtime Saved R
baseline without rewriting their original bytes or moving a user pointer.
Source identity is checked around capture. Bounded metadata copy and atomic
sealed-leaf adoption preserve all input ownership on failure/cancel. A first
private R+1 checkpoint now retains opened legacy Saved R even before file upgrade.
The `legacy-version-seed` gate passes in16.483s with optimized/ASan/UBSan seeding,
metadata and closure tests. Non-NUL label spans, mismatched descriptors, cancel,
reservation and changed-source refusal pass. Native IO adapter integration follows.

W11a3.2b2 complete: the native file-task adapter calls real version Save/archive
Open, retains its base independently, seeds legacy Saved R before adoption and
transfers prepared plans/stores through owned replies. Folder creation and disk
retirement stay on the worker; stopped-worker disposal supports shutdown. The
`worker-archive` gate passes in17.456s with optimized/ASan/UBSan real IO plus
protocol/ordered Save tests. Deterministic private hooks prove internal capture
PUBLISH remains cancellable and named-file PUBLISH does not. A held reply keeps
its old base after queued App-owner release; R+1 edits remain independent of the
saved snapshot. Test hooks are compiled only into private adapter objects. Normal
App job construction/publication/UI is the next W11a3.3 slice.

W05a uses `CellEditTarget`/`CellEditCommand` in `src/workbook/cell_command.c`.
The App captures a draft once and supplies its freshly resolved live source
protection at commit; the producer resolves sheet ownership and records one
existing worksheet action only after a changed value is adopted. No-op and
failure preserve dirty/history state. W05b resolves durable workbook/sheet IDs
and checks both expected CellStore revision and the admitted-command clock.
Session fences stay outside Undo/serialization: even the same saved workbook
cannot accept an old draft after reopen. Native v15 adds workbook_identity and
sheet_identity to the strict v14 package. The ordinal sheet_id joins in existing
metadata remain unchanged; sheet_identity maps them to durable IDs. Old files
are not rewritten by Open, and are clean until a user edit; their first Save
persists the generated identities. Reopening an unsaved legacy file can generate
a different identity. Ordinary Save As retains the logical document ID; independent
runtime instances have separate fences. W13 historical forks must explicitly
choose a new document identity. The command clock covers only admitted changed
cell commands and W06 history replays; direct legacy paste/range/object producers
still have their own store clocks. W08 journal durability remains separate.

The local workbook-identity gate reuses per-file objects, runs shared-C identity,
cell-command and descriptor checks, serial private native archive tests and a
nine-stage hidden TEST App fixture. Broader lifecycle/browser/source interaction
checks remain registered and deferred. Its two-sheet demo is
build/tests/generated/workbook-identity-demo.dxls; the untouched finite legacy
input is build/tests/generated/identity-legacy.dxls. See tests/README.md for limits.
The local `cell-command` gate runs isolated C model checks and a test-only real
App fixture without the canonical workbook or background/browser suites. Its
clean demo is `build/tests/generated/cell-command-demo.dxls`; stale draft and
final captures live under `build/tests/results/cell-command/`.

W06 uses `src/workbook/history.c` to select existing per-sheet journals by
non-recycled process order. It owns cell/format snapshot transfers, abandoned
Redo cleanup, retained-history barriers and monotonic replay revisions. Existing
Analysis/object actions keep their App-owned replay adapter and share target
selection without being skipped. Their wider native/source/browser interactions
remain deferred to the named catalog gates. No second snapshot archive is added.
The existing 32-entry limit per sheet remains; the largest discarded order is the workbook's
oldest safe replay boundary. W07 adds sheet lifecycle to this chronology. The local
workbook-history plan covers owned C modules plus 21 hidden TEST App stages;
its generated two-sheet checkpoint is `build/tests/generated/workbook-history-demo.dxls`.
History is deliberately session-only until the journal/version retention slices.

W07 complete: shared-C sheet lifecycle joins workbook chronology; detached
ownership, IDs, format/ledger budgets and retained-history barriers are preserved.
Focused evidence: 788 lifecycle assertions per optimized/sanitizer build;
24 hidden GUI stages/29 assertions, plus the adjusted W06 workflow. History
remains session-only; formula restoration is W26b.

W08a complete: private DuckDB schema-v1 cell journal, bounded atomic append/read,
post-COMMIT receipts and exact operation-ID retry. Optimized/sanitizer each pass
190 storage assertions and 14 real pre/post-COMMIT kill/retry checks. Initial
32-single/256-batch timing samples are component evidence, not a UI latency SLA.

W08b1 complete: immutable reserve/publish/cancel admission, 256-entry/4 MiB queue,
ordered acknowledgements, retained failures and drain/abort. Checkpoint setup and
nonblocking retirement extend the same owner in W08b2; 1,443 queue assertions now
pass per optimized/sanitizer configuration.

W08b2 complete: native manual-cell edits use an owned asynchronous checkpoint
plus journal. Pending/backed-up/paused/Save-needed feedback and keyboard/mouse
Retry are integrated; backpressure keeps the draft. Unsupported edits make
coverage explicit and the next changed cell starts a bounded replacement
checkpoint. Newer admitted/recorded edits remain dirty after an older Save.
Focused models: 126 session, 1,443 queue, 190 storage and 169 cell-command
assertions; optimized/sanitizer coverage and 14 kill/retry checks per binary.
The final narrow GUI passes 18 stages/28 assertions (6.797s; 17.542s release gate). Narrow 150% Retry, checkpoint
rotation, Save R/R+1, protected working-file ownership, reopen and discard pass; the 21-stage W06
interaction also passes (5.611s). Receipts, captures and
`journal-session-demo.dxls` stay under ignored `build/tests/`. W08c is next;
startup recovery UI, persisted Undo and full Save progress/cancel remain
W10/W12/W09. No per-slice human approval is required.

W08c1 complete: first-creation disk-full errors no longer strand a malformed
journal; only files proven absent before the failed attempt are removed. Existing
journals/user tables stay intact. The separate `journal-faults` gate passes 192
storage/1,443 queue assertions plus 16 process/fault cases and 76 checks per
optimized/sanitizer configuration (18.466s total). It covers process kills,
ENOSPC/EIO, short writes, fsync, shutdown checkpoint failure, real file-size limits
and failed first creation with retained-record retry. The related 18-stage App
workflow passes in 6.594s; 52 runner/catalog/web-inventory contracts pass.
Filesystem injection is test-only and limited to private journal/WAL paths;
ordinary checkpoint/UI edits defer this matrix to its named gate.

W08c2/W08 complete: the `journal-performance` gate passes optimized and ASan/UBSan
in 18.208s, with full 20,000-cell checkpoint/tail replay comparison and deterministic
full-queue rejection preserving the model. DuckDB v1.5.6 and the actual loaded
library/input/helper hashes are retained in each ignored metrics receipt.
Optimized warm producer p95 is 0.005ms, durable p95 1.516ms; 256 burst edits take
42.274ms through two commits. Bootstrap durability is 100.728ms; 20,000-cell legacy
paste takes 1.126ms, next checkpoint producer 0.991ms and background durability
2,136.776ms (helper peak 48.172MiB). Keep immediate writer wakeup and natural
coalescing, max 256 records/4MiB, without artificial debounce. These are local C
API measurements, not physical-input/frame SLAs or power-loss certification.
Legacy paste itself remains outside journal admission. The 56 runner/catalog/
benchmark/web-inventory contracts pass in 9.706s; no desktop boundary changed.
W09 is split into controllable persistence and worker/UI publication slices.

W09a complete: the controlled transactional writer reports completed sheet,
column and cell rows plus bounded metadata stages; total is reached only after
COMMIT. Validation/preparation/write/pre-publication cancellation returns a
separate result, rolls back the transaction and removes only a failed new file.
The original `workbook_save` keeps its API through empty control. Six cancellation
points on new/existing destinations, exact text/IDs/revision, unrelated tables,
failed destinations and late cancellation pass 413 assertions per optimized and
ASan/UBSan run (15.885s). The related hidden TEST App checkpoint/Save workflow
passes 18 stages/28 assertions (6.812s; 11.007s release gate); 56 tool contracts
pass. Save progress/cancel UI and worker write ownership remain W09b.

W09b1 complete: native Save owns the worker lane until its reply is polled.
Read replacement and generic cancellation cannot drop its completion; cooperative
cancel returns a typed result, and pre-publication progress closes cancellation
under the same mutex. Five deterministic barrier cases pass optimized/sanitizer
(1.762s/5.477s), including attempted replacement reads, generic cancellation,
newer live data and late cancellation. The existing hidden 18-stage Save workflow
passes in 6.583s (22.283s focused gate); 56 tool contracts pass. Test barriers are
compiled only into private worker objects. App progress/cancel remains W09b2.

W09b2/W09 complete: native Save has a compact 360×136 logical-unit progress
panel, Escape/button cancellation and a non-cancellable publishing phase.
Pointer ownership protects the panel while real grid pan remains live outside
it; structural/data editing stays IO-guarded. The reply retains R until exact
sparse-value/definition/view and registered format/source/object fences plus
action order are checked. R+1 remains dirty/recoverable and blocks a pending
Quit. Failed/cancelled Save preserves the prior file/path and recovery state.
The `workbook-save` gate passes 425 controlled-writer assertions per optimized/
sanitizer variant, ordered-worker barriers and 18 real App stages/27 checks
(5.581s GUI, 25.167s total). It includes native pan, Escape, pointer Cancel at
640×640/150%, late cancellation, real C R+1 admission, guarded Quit, reopen and
failed Save. The related original 18-stage checkpoint workflow passes in 6.784s;
56 runner/catalog/benchmark/web-inventory contracts pass. `save-ui` is the GUI-only
iteration plan. Captures/receipts stay ignored; no human acceptance is claimed.

W10a1 complete: private journal v2 atomically binds immutable generation/parent,
checkpoint size/SHA-256 and an owned original locator before any edits. Exact
origin retries remain valid after appends; conflicting origins and inferred
upgrades of nonempty legacy tails reject. v1 remains readable. Bounded schema/
cardinality/blob/path validation rejects malformed rows, duplicate origins and
embedded NUL locators without silent text truncation. The focused journal gate
passes 255 assertions per optimized/sanitizer variant plus its existing kill/IO
matrix (12.950s). This is a storage primitive; real checkpoint fingerprints and
session admission remain W10a2, startup recovery remains W10b/W10c.

W10a2/W10a complete: native macOS checkpoint preparation hashes the closed
baseline using the existing bounded streaming digest adapter, then binds the
immutable generation/parent/size/SHA-256/original locator before readiness or
edit acknowledgement. A leftover baseline WAL rejects preparation for Retry.
Origin failure retains queued edits; exact-origin retry reopens safely. Other
native hosts retain v1 backup behavior until their digest adapter is implemented;
no new dependency or browser persistence claim is introduced.
The focused gate passes 138 session, 1,470 queue, 255 storage and 169 cell-command
assertions per optimized/sanitizer configuration, then 18 App stages/28 checks
(6.680s GUI; 26.862s total). Tests compare actual baseline bytes/digest, named and
unnamed origins, parent rotation, missing-origin retention and mismatch repair.
The separate fault gate adds four origin-binding ENOSPC/EIO/short-write/fsync
cases: 20 cases/84 checks per configuration now pass (19.330s total). The optimized
performance refresh passes in 4.451s: warm producer/durable p95 0.005/1.532ms,
256 edits/two commits in 42.675ms, full 20,000-cell checkpoint in 2,111.586ms;
56 tool contracts pass. W10b first adds live-instance leases before discovery.

W10b1a complete: private recovery generations hold a native OS lease from
allocation through writer join/release. A second handle/process cannot claim
one; SIGKILL releases ownership. Acquisition rejects symlinks, hardlinks and
nonzero/nonregular files without truncating or replacing them. Kept generations
retain a fixed zero-byte lease file; discard removes only owned artifacts while
still holding ownership. The native-only module stays excluded from Wasm.
The `recovery-owner` gate passes 26 lease and 143 session assertions per optimized/
sanitizer variant, then 18 hidden App stages/28 checks (6.747s GUI, 16.041s total).
The updated maximum-paste/checkpoint workload passes optimized in 4.613s; 56 tool
contracts pass, including explicit browser source exclusion. macOS is validated;
other native implementation paths are not an execution claim. Read-only crashed
journal inspection is W10b1b; no startup recovery UI is active yet.

W10b1b/W10b1 complete: journal inspection opens the existing file in DuckDB
READ_ONLY and refuses create/bind/append without poisoning read access or changing
receipts. Real v2 writers killed before/after COMMIT reopen read-only with exact
old/new state and immutable origin; DB/WAL hashes and the artifact set stay
unchanged. Normal inspection creates no WAL; a missing path creates no file.
The focused journal gate passes 264 assertions per optimized/sanitizer variant
plus 10 process/fault cases/72 checks each (14.989s). The expanded crash driver
remains in the separate journal/fault plans, not ordinary UI iteration. All 56
tool contracts pass (9.254s). Verified isolated candidate replay/discovery is next.

W10b2a complete: `recovery.c` prepares an isolated candidate while holding the
existing generation lease. Actual baseline size/SHA-256, identity/revision and
exact before-images gate full bounded replay; aliases, unfinished baseline WAL,
wrong baseline/digest, gaps and protected/invalid targets reject atomically.
Checkpoint mode preserves deferred/unknown/unbound tails; source queries never
run. Returned state is dirty with no automatic Save destination. Projection does
not consume live draft/history issuers or reconstruct durable Undo (W12).
The `recovery-candidate` gate passes 222 deterministic assertions per optimized/
sanitizer configuration (18.676s), including all cancel stages, lease contention,
named/Unicode/format/view state, unbound/broken tails and a real worker preparing
R while the live model accepts/undoes R+1. Both recovery files stay byte-identical
across preparation/cancel. The updated journal gate passes 280 assertions plus
10 process/fault cases/72 checks per configuration (16.009s); the related hidden
18-stage checkpoint App passes in 6.658s (15.964s gate). All 56 tool contracts pass.
Preparation currently needs the macOS digest adapter; discovery/App adoption is
still W10b2b/W10c, and older folders without an existing lease stay untouched.

W10b2b/W10b complete: the native catalog streams direct generation folders with
one owner/cursor and a 1–128 physical-entry step budget. Visitor pause supports
bounded pages; cooperative cancel preserves the cursor and collected rows.
Missing roots stay absent. Brief metadata leases skip active owners; bound,
unbound, checkpoint-only, incomplete, invalid and pre-lease legacy entries remain
separate. Equal document/revision branches are never collapsed or ordered by
mtime, and scanning never prunes files. WAL/file/directory aliases are refused;
metadata rows borrow storage only until the visitor returns.
The `recovery-catalog` gate passes 20 directory and 101 catalog assertions per
optimized/sanitizer configuration (4.838s), including two same-clock branches,
active ownership, pause/cancel/resume, legacy/broken sidecars and byte preservation.
The related candidate hardening passes 228 assertions per variant (17.609s),
including dangling WAL aliases; 56 tool contracts pass (8.882s). Native-only
sources remain excluded from Wasm. POSIX iteration is validated on macOS;
Windows enumeration currently reports unavailable. Background App requests and
startup adoption now move to W10c; no recovery UI completion is claimed.

W10c1 complete: the independent native recovery reader owns scan continuation,
32-row/4 MiB copied pages, isolated candidates and cooperative cancellation.
One active request/unpolled reply is admitted at a time; late candidates cannot
be dropped by a replacement request. Publication and cancellation share the
mutex, while expensive retirement happens outside it and reservations stay held
until actual release/transfer. Active preparation reserves at most 8 MiB of
format ownership, then reports the actual ready model; this is not total RSS.
The `recovery-worker` gate passes 156 assertions per optimized/sanitizer variant
(5.897s): page pause/restart, 65,536-byte original locators, cancelled rows/replay/
late results, unpolled admission, shutdown join, lease transfer and replies
outliving the worker. Original checkpoint/journal bytes remain unchanged; 56 tool
contracts pass (9.362s). Barriers exist only in private worker objects and the
module is excluded from Wasm. Actual startup/manual UI adoption is W10c2.

W10c2a complete: recovered edits seed copied document/parent/original metadata
without acquiring a Save destination. Wrong-document admission rejects before
IO/model mutation. Explicit retirement preserves every deferred/unknown/unbound
tail and refuses changed artifacts or additional files before deletion. Candidate
free/cancel never retires files; cleanup failures remain separate from named Save.
The `recovery-disposition` gate passes 172 session and 271 candidate assertions
per optimized/ASan/UBSan variant plus 18 hidden App stages (6.379s GUI; 31.542s
total). It covers copied lineage, replay/checkpoint disposition, extra files/WAL,
lease-held idempotent cleanup and a transferred model surviving retirement.
Subsequent startup/manual UI and crash/restart acceptance is recorded under W10c2b/W10c3b below.

W10c2b/W10c2 complete: startup and File → Recover session use the independent
reader, bounded pages and a compact keyboard/pointer panel. Startup only offers
inactive sessions over its untouched initial document. Cancel/dismiss invalidates
the presentation epoch, keeping late candidates from adoption. Recovery enters
the existing dirty-document guard; checkpoint copies keep deferred tails across
new edits and Save As. Full replay cleanup occurs only after complete current
Save or explicit discard. Private recovery paths are refused by ordinary IO.
Format ownership includes pending/ready/staged candidates and checks admission.
The `recovery-ui` gate passes 18 hidden real-App stages/32 checks (3.622s GUI;
11.271s total), including cancelled preparation, dirty-guard Cancel, inherited
edit parent, both Save/reopen outcomes and 640×640 at 150%. Keytip tests pass
optimized/sanitizer; 25 tool contracts pass in 7.944s. Test hooks are private;
crash/restart, active-instance exclusion and foundation coverage are recorded
under W10c3b below. No physical-input or browser-persistence completion is claimed.

W10c3 process workflow implemented: a private hidden real-App writer commits
one editor change, a second hidden observer excludes the live lease, and only
the owned writer PID is SIGKILLed. A fresh App restores the same document and
revision; checkpoint/journal/WAL artifact names and hashes remain unchanged.
Optimized and ASan/UBSan process drivers pass (2.269s/3.697s; 7.716s gate), with
shutdown before private-directory cleanup. The updated UI fixture waits for its
dirty dialog's first paint before injecting a click; 18 stages pass in 4.145s
(11.296s gate). These are automated TEST processes, not user hands-on evidence.
The gate exposed recursive test Make calls losing their jobserver descriptors.
Recipes now expose the inherited pipe only to recursive Make; an actual Python
bridge honors the parent's two-slot limit. All 76 incremental build checks and
49 runner/catalog contracts pass; -n/-t/-q cannot launch tests. The registered
full native foundation milestone is next; W10/W10c remain in progress until it
passes. Browser persistence and full durable Undo remain separate.

W10c3 integration review corrected File metadata ownership: adding recovery had
left independent eight-entry icon/review arrays. One checked File registry now
owns labels, keys, icons and semantic IDs; Quit remains last. The recovery
scenario enters through the actual painted File menu, with explicit first-paint
waits before dirty-guard clicks. `recovery-ui` passes 19 stages/35 checks in
4.195s (13.195s gate); the named `recovery-menu` ASan/UBSan App gate passes the
same 19 stages in 9.210s (42.984s including 32.896s compilation). Its registry
passes 2,753 optimized/sanitizer checks, and 54 tool contracts pass. The earlier
full run was intentionally interrupted before continuing with this correction;
no success is claimed for that interrupted receipt. Current milestone regression
and the shared review interaction still need completion.

W10c3a complete: compatibility tests now distinguish the strict 28-table v15
package from genuine legacy packages, verify non-writing v14 Open followed by
v15 identity persistence, and preserve malformed targets byte-for-byte. Legacy
mark/measure defaults require legacy headers; the rollback fixture reaches its
actual CHECK failure. Core/workbook tests share native per-file objects and
actual dependencies, with unchanged serial core-before-workbook execution.
A profile identified repeated catalog shape queries. The pure bounded schema
controller consumes borrowed exact byte spans, accepts reordered/ASCII-folded
column identifiers, rejects duplicate/unknown/view/type/NUL-prefix metadata and
poisons invalid proofs. Native complete-package validation now uses one bounded
catalog query; required NULL/text and owner-row checks remain separate. No format
version or persistence semantics changed. The same 18,522 archive assertions pass
optimized (27.081s versus 48.573s before batching) and ASan/UBSan (233.811s;
the prior monolith exceeded 300s). The integrated gate passes 2,528 pure proof
checks and 1,751 core checks per configuration plus 18 Save App stages (6.060s;
283.686s including the explicitly broadened archive matrix). All 89 incremental
build checks and 54 runner/catalog/web-inventory contracts pass. These are local
measurements, not frame/SLA promises.
A subsequent physical-type parity probe preserves DuckDB JSON's existing
C-API VARCHAR compatibility and its constrained-write rollback (19 checks in
each build); the proof module now has 2,530 assertions. The final short local
gate passes in 8.923s (5.687s GUI). The optimized journal workload also passes in
3.669s: warm producer/durable p95 0.005/1.262ms, two burst commits and the complete
20,000-cell checkpoint at 1,952.913ms, with exact replay/provenance retained.
The ordinary `workbook-schema` gate now selects only the proof module and narrow
Save App workflow. `workbook-archive` keeps the expensive complete matrix as a
named interaction/full-milestone check; the existing object-archive CLI provides
a smaller edit-time path. Foundation coverage and command limits are recorded
under W10c3b; the shared-C/official-Worker controls boundary has passed in 59.001s.

W10c3b complete under scoped automated coverage: optimized/sanitizer engines, owned crash/restart
and fault checks passed. All440 non-layout native stages pass with the real NYC
fixture. Legacy seeds now record aligned command histories, budget preparation
uses workbook-wide Undo and real drafts, and Save assertions capture settled
results/viewport state without relaxing R/R+1 protection. Table73/Query47/Unique32
pass in37.402s/24.664s/15.634s. The combined layout run exercises0–941 without
an assertion failure, including the repaired pending-count barrier, then exhausts
its old25,000-frame budget. The independent936–950 reset fixture passes all14
stages in8.444s, completing950-stage coverage across receipts. The failure budget
now matches the focused runner's46,080 frames; the enlarged single-run command
has not been rerun, and no green full-command receipt is claimed. All49 runner/
catalog contracts and shared-C/Wasm compilation pass. Both native lifecycle
configurations pass38 checks each; cold/warm Finder Open with the alert guard
passes in12.811s. The current bundle passes strict signature verification with
no test/review hooks in its public symbols. Physical input/browser persistence are separate; current
source/binary receipts are under ignored build/tests/results/.

### Owned data, refresh and complete analytical results

| ID / priority / state | Scope and dependency | Demo and acceptance gate |
| --- | --- | --- |
| W15 · P1 · Queued | Explicit Connect source mode and provenance; W05/W09. | Read-only Parquet/CSV query views save definitions/styles and a bounded preview; offline dependencies are visible, originals unchanged by Save/export. DE06; Connect portion of A16/A24. |
| W16 · P1 · Queued | Import retained source bytes and recipe; W15/W11. | Import then remove external file: workbook/version still queries retained data; failed import leaves no partial dataset. Retention policy and path/content identity explicit. DE07; retained-source A16/A24. |
| W17 · P1 · Queued | Add selected query result to hidden Data Model; W16. | Materialize the complete query once, save/reopen offline and query it without a visible table; optional sheet view references the same dataset. DE07; model portion of A24. |
| W18 · P1 · Queued | Workbook-owned effective relation and one typed record patch; W08/W17. | Distinguish duplicate records, NULL presence, empty/literal text and exact DECIMAL; viewport/filter/pivot/statistics/copy/export read one revision. Connect requires explicit conversion; originals unchanged. DE01/DE03/DE04; A07. |
| W19 · P1 · Queued | Owned row insert/delete and field add/drop/reorder; W18. | Stable surviving IDs/order, exact Undo/reopen; consumers rebind or report schema errors. Resolve bindings rather than rewrite raw SQL text. DE05; A08. |
| W20 · P1 · Queued | Inspectable resolved edit/transform steps; W18–W19. | Show the admitted operations and parameterized effective SQL without inventing editable raw-SQL history; replay reproduces edits under sort/filter. DE02; complements A07–A08. |
| W21 · P1 · Queued | Complete pivot/aggregate/chart result persistence; W09/W15. | Save cells outside the viewport and chart state; offline reopen displays the full result, while a million-row connected browse result remains optional. Distinct definition/data/result generations survive reopen. VE04; A20/A25. |
| W22 · P1 · Queued | Stale object warning and retained-result Save; W21. | Change definition/data offline, save and reopen with old complete output plus newer definition and clear stale provenance; successful recompute replaces output atomically. VE06; object/Save portion of A31. Formula behavior follows W26b. |
| W23 · P1 · Queued | Atomic Refresh replacement/reset for admitted owned datasets; W18/W21/W08. | Success replaces data and clears affected ad hoc cell/row styles/value overrides, even for identical/duplicate rows; compatible field formats remain. Failure/cancel preserves old data/styles/history; Undo/retained version restores prior state. No record matching/merge or automatic tag. DE09; A26/A32–A34. |
| W24 · P1 · Queued | Conditional formatting on a current relation; W18/W23. | A key=482 rule evaluates all matching current rows after refresh, including duplicates/changed other fields; compatible rules persist and incompatible fields fail explicitly. Field formatting and ad hoc styles stay distinct. Rule portion of A26/A32. |
| W25 · P1 · Queued | Pivot drill-through into a new query sheet; W17/W21. | Apply the exact displayed cell's group/filter provenance; model-backed result drills offline; unavailable external provenance reports a clear error without fabricated rows. DE08; A25. |

### Typed ranges, positional formulas and analytical geometry

| ID / priority / state | Scope and dependency | Demo and acceptance gate |
| --- | --- | --- |
| W26a · P1 · Queued | Typed sparse manual cells; W05/W08. Integrates C03/C04/C07. | NULL/empty/literal text, integers and exact DECIMAL survive edit/clipboard/Undo/reopen; no implicit locale-dependent conversion. WM01; typed-cell portion of A06. This does not wait for the Data Model or formula engine. |
| W26b · P1 · Queued | Basic parsed positional formulas and dependency runtime; W26a/W21. Integrates C08. | An explicit small operator/function set recalculates typed manual/analytical references, detects cycles and displays typed ref/type/calc errors. Valid changed numbers recalculate; saved stale output retains its provenance. WM04; formula part of A06/A31. |
| W27 · P1 · Queued | Explicit manual-range analytical input; W26a. | Bind a finite typed range to a relation and pivot; one edit updates consumers at one revision; schema changes invalidate explicitly. WM03; remaining A06. |
| W28 · P1 · Queued | Code/display names and object-local positional references; W26b/W21. | `sheet_code.object_code.A1` resolves offscreen; display rename is cosmetic, code rename updates parsed bindings atomically. No category/GETPIVOTDATA lookup. WM07–WM08; naming part of A29. |
| W29 · P1 · Queued | Reference-following manual/analytical move; W28/W02. | Cross-sheet dependents follow moved target, including absolute bindings; move/Undo/Redo updates addresses and values together. Sorting still addresses output positions. WM04; A21 and formula part of A03. |
| W30 · P1 · Queued | Independent analysis inventory and owner-local styles; W21/W28. | List/reveal/edit offscreen analyses; moving/restoring retains local styles without changing query data or object IDs. WM05–WM06; foundation of A04/A29. |
| W31 · P1 · Queued | Blocked placement/spill state; W29/W30. | No same-sheet analytical/manual-content overwrite; retain last effective footprint/result plus pending target, reachable error handle and typed bound-read spill error. Valid retry publishes placement/ref changes once. LA01/LA06; A22. |
| W32 · P2 · Queued | Floating matrix/chart placement and scale; W31. | Preserve local grabbed point, preview and Undo/Save/Open geometry; snapped→floating converts sheet analytical bindings to qualified local form atomically. LA02/UX02–UX03; transform/mode portions of A04/A29. |
| W33 · P2 · Queued | Floating stacking and always-reachable controls; W32. | Drag/click promotes the correct object, including a fully covered one; hit/paint order agrees and formulas retain owners. LA07; occlusion part of A04. |
| W34 · P2 · Queued | Object-focused coordinate chrome; W28/W32. | Off-origin matrix local A/B/C and 1/2/3 labels align at 100%/150%, clipping/freeze consistent, focus return restores worksheet headers; reference insertion uses shown owner. UX06; A30. |
| W35 · P2 · Queued | Matrix-to-sheet-grid fit; W31/W32. | Explicit snap uses existing sheet metrics (including variable column widths), previews conflicts and survives one Undo and Save/Open. LA03; first mode of A05. |
| W36 · P2 · Queued | Per-row geometry and sheet-to-matrix fit; W35. | Show impact on other anchored objects before changing sheet metrics; publish one reversible geometry transaction and identical reopen. LA04; second mode of A05. |
| W37 · P2 · Queued | Chart-to-range fit; W32. | Fit to chosen sheet rectangle without mutating sheet metrics; exact Undo and persisted bounds. LA05; third mode of A05. |
| W38 · P2 · Queued | Inspection-first Master View; W33/W36. | Compose independent sheets with explicit active owner/transparency and z-order; each retains its own metrics, addresses and formulas. No implicit cross-sheet grid edits. LA01; A23. |

### Automation and cross-cutting delivery gates

| ID / priority / state | Scope and dependency | Demo and acceptance gate |
| --- | --- | --- |
| W39 · P2 · Queued | Narrow authorized production IPC over admitted commands; W05/W08. | Same typed command through UI/IPC yields identical state; stale/duplicate/unauthorized/oversized requests are bounded and predictable. Development review bridges stay excluded. AU01–AU02; A17/A18. |
| W40 · P2 · Queued | Core-only command host; W39. | Open/apply/query/save the supported subset without a window; optional preview renderer remains separate; same typed results as UI. AU03; remaining A18. |
| G01 · Each affected slice | Shared codec/migrations and browser acceptance; schema/command producer of that slice. | Existing supported files load or reject atomically; shared C + actual official Worker exercise new records/commands and cancellation/quota/reload where applicable. Existing browser Open/Save gates remain open until end-to-end publication passes. PL01; A19. A shared-C gesture change needs focused browser input validation, not a persistence rewrite. |
| G02 · Each affected slice | Resource/ownership/failure budgets; W08 establishes measured journal baseline. | Bound retained memory/queue/assets, preserve NULL/precision and explicit ownership; record tested binary, elapsed time and coverage limits. No whole-table browser/native snapshot disguised as a cache. PL02; TRD section 16. |

Suggested dependency path under automated slice gates: **W04/W05/W06 → W07/W08 → W09/W10**, with W07 once workbook history is available. Then W11/W15/W21 unlock useful offline/version demos; W16–W25 and W26a–W31 are separate capability tracks; typed ranges need not wait for general formulas. The rows are solo delivery steps, not authorization for concurrent implementation lanes. FT01–FT03 in the TRD remain unscheduled future features.

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

W11a3.3 complete: native App jobs use the owned v16 worker adapter. The small
`app/versionstate` controller adopts only matching owner/index receipts, rotates
runtime owners on Open and keeps model/path/dirty/history decisions in App.
R+1 edits remain dirty after captured R publishes; cancellation/failure retains
the current store and file. Native Open admits full roots, legacy Open seeds its
Saved baseline without rewriting it, New retires stores off frames, and compact
progress distinguishes snapshot preparation from named-file publication.
The `app-versions` gate passes optimized/ASan/UBSan controller and real worker
checks in35.658s including18 hidden Save stages. The extended `save-ui` gate
passes20 Save/cancel/R+1/reopen/New stages in15.778s (9.067s GUI), and19 catalog/
web-inventory contracts pass in8.221s. Recovery still uses legacy checkpoints
until W11a3.4; no Tag UI or retained analytics claim. Evidence is under ignored
`build/tests/results/` with the `w11a33` prefix.

W11a3.4a complete: journal sessions can explicitly admit an immutable version
binding before edits or after Saved advances. The background writer produces a
v16 checkpoint with the existing Saved pointer and complete roots; each generation
retains its own store and supports a caller-provided background retirement adapter.
A covered matching generation is reused; rotation keeps old roots until publication.
The `version-checkpoint` gate passes new round-trip/rotation/caller-release checks
and existing legacy journal-session tests under optimized/ASan/UBSan in17.111s.
App use and recovery admission remain W11a3.4b. Evidence: ignored
`build/tests/results/w11a34a-version-checkpoint.log`.

W11a3.4b/W11a3 complete: recovery admits v16 heads and complete independent
root stores before replaying a verified tail. Legacy checkpoints remain readable
without inventing a user Saved baseline. Recovered owners are fresh and have no
named-file identity; full and deferred-tail candidates retain the same older
Saved closure. App binds versioned journal checkpoints before changed cell commands
and re-checkpoints newer dirty state after a captured named Save publishes.
A failed replacement stays uncovered; Retry rebuilds the current version binding.
Versioned sessions refuse a legacy fallback that could drop roots, and App queues
store retirement off frames through shutdown. Native disk assertions explicitly
read versioned candidates instead of weakening ordinary legacy loading.
The `recovery-versions` gate passes optimized/ASan/UBSan checkpoint/recovery/App
modules and19 hidden recovery stages in49.721s (6.039s GUI); the named
`recovery-disposition` interaction passes legacy modules plus18 journal/Save stages
in28.838s (7.533s GUI). Recovery-worker cancellation/late-reply tests pass in6.760s;
19 catalog/web-inventory contracts pass in8.994s. Evidence uses the ignored
`build/tests/results/w11a34b` prefix. This completes current core-profile Save/
recovery integration; Tag UI, durable Undo and retained analytical assets remain
separate roadmap work. W11b follows with atomic named-state retention.

W11b.1 complete: the pure `version_tag` controller owns its validated UTF-8 label
and plans one atomic Saved-plus-tag publication, including when unchanged Saved
content reuses its existing root. Canonical owner/index checks reject stale or
noncanonical plans; duplicate/invalid labels, historical owners, capacity and counter
exhaustion preserve input/output. This follows TRD8.2's default that Create Tag
also updates the named workbook. The registered `version-tag-plan` gate passes
48 new checks plus229 existing index checks per optimized/ASan/UBSan configuration
in3.545s. Native IO and visible controls remain W11b.2/W11c.

W11b.2/W11b complete: the native Create Tag API preflights labels before capture,
proves complete supported-core equality for root sharing, and writes the new label,
Latest Saved and full root set in one existing archive transaction. Repeated labels
fail without IO; cancellation/storage failure preserves prior bytes/index. Ordinary
Save creates no tags and can reuse an equivalent older named root. Tag receipts
expose their complete owned tag plan separately from ordinary Save plans, preventing
publication with the Save-only generation. The `version-tag` gate passes48 pure
planning checks,72 native tagged-state round-trip/reuse/offscreen/cancel/budget checks
and existing Save-controller coverage per optimized/ASan/UBSan configuration in50.192s.
Evidence: ignored `build/tests/results/w11b2-tag-save.log`. UI/worker Tag command
binding remains W11c; no visible Tag/preview or retained analytical-output claim.

W11c.1 complete: Tag jobs use the ordered Save lane with an owned validated UTF-8
label and retained input store. Worker replies distinguish complete Tag plans from
ordinary Save plans. The App controller verifies the owner/index, generation,
root/tag counts and appended label/root before adoption; repeated or stale receipts
cannot republish. The `tag-worker` gate passes real versioned worker and App-owner
modules under optimized/ASan/UBSan in31.209s, including copied-label lifetime,
named PUBLISH cancellation boundary, Tag reopen and fresh-owner/repeated-receipt
checks. Visible controls remain W11c.2. Evidence: ignored
`build/tests/results/w11c1-tag-worker.log`.

W11c.2/W11c complete: native File → Versions (including the macOS menu bridge)
shows one Latest Saved row and explicit immutable names. Its shared C widget owns
only bounded layout/focus/scroll state; App owns destination/name validation and
ordered Tag Save publication. The panel shrinks for short lists, fits640×640 at150%,
and supports list scrolling/Home/End, Tab, Escape, duplicate feedback and preserved
name/draft on cancel/failure. First Tag chooses a destination; ordinary Save never
adds a tag. The ten-row File registry fits without hiding Quit at normal sizes.
The final `version-controls` gate passes keytips/choice geometry under optimized/
ASan/UBSan plus26 real hidden App stages in22.145s (9.954s GUI), including physical
C-menu selection and a real native-menu command. Only the first destination dialog
is stubbed; its Save/Cancel routing, writes, reopen, failed target and cancellation
are real. Named normal Save interaction passes20 stages in15.661s (8.911s GUI);
Tag worker/App owner interaction passes in33.034s. Production bundle builds without
compiler warnings, signs strictly and excludes test hooks; shared C web compilation
passes, along with54 runner/catalog/web contracts in10.876s. Browser workbook UI
remains unavailable. Evidence uses ignored `build/tests/results/w11c2` outputs.
Older-state opening is W11d; current tags retain the supported authored core,
not external source bytes, durable Undo or future full analytical outputs.

W11d.1 complete: `version_preview` accepts only an admitted opaque root and a
matching historical reader owner, retains its protected store, verifies bytes
before/after strict decoding and returns a separate model without a Save path.
It executes no external source query and never receives the Working Head.
Model/store lifetimes are independent; cancellation at each phase or inside
hashing preserves output and current ownership. Decoder SQL is cooperatively
cancellable before/after its call, not between every internal query. The registered
`version-preview` gate passes55 checks per optimized/ASan/UBSan build in7.341s,
including offline definitions, offscreen data, exact working Undo/Redo/draft-target
state, deletion of the original named file and independent returned models.
Worker/App/visible adoption remains W11d.2–W11d.4. Evidence: ignored
`build/tests/results/w11d1-version-preview.log`. W11e explicitly owns broad
version-format consumer migration/regression after preview integration.

W11d.2 complete: historical Load contexts retain an admitted store/root and copy
a matching reader owner without receiving a Working Head or resolving a user
path. The worker returns an independent model plus protected reader metadata;
preview predicates/getters are distinct from ordinary Open/Save/Tag, so normal
App adoption rejects them. Cancellation suppresses late publication and background
retirement releases abandoned roots before subsequent work. Taken model/store
ownership survives reply and worker shutdown. The registered `preview-worker`
gate passes private historical worker and existing real Save/Open/Tag worker tests
under optimized/ASan/UBSan in19.651s. It proves cancelled-root retirement, non-file
path handling, ordinary-Open rejection and protected post-stop ownership. No UI
or Working Head suspension claim yet. Evidence: ignored
`build/tests/results/w11d2-preview-worker.log`.

W11f complete: a reproduced stale-session Save could discard a peer's newly
created tag. Implicit same-path Save/Tag now copies the last observed file identity
into its worker context, checks before opening and inside the transaction, and
compares the admitted existing v16 index against its retained input. The paired
manifest fence prevents a newer observed stat from blessing another writer's
index. Unknown current-file identity requires Save As; accepted Save As has an
explicit replacement path. These are conservative cooperating-writer fences,
not protection against arbitrary raw mutation during a save. Preparation failure
also keeps the current request epoch so unrelated replies are not stranded.
The initial `file-conflict` regression failed on the old behavior; the fixed gate
passes optimized/ASan/UBSan in14.057s, including copied-observation lifetime,
peer tag/file preservation, pre-open→transaction stamp changes, explicit replace
and subsequent ordinary Save. The22-stage Save GUI passes in17.347s (10.450s GUI),
including accepted same-path Save As without a usable old identity. The26-stage
Versions GUI passes in21.756s (10.479s GUI), including failed pre-submit epoch/name
preservation. Evidence uses ignored `build/tests/results/w11f` outputs. This
correctness fix was prioritized before continuing W11d.3 ownership transfer.

W11d.3 complete: the borrowed typed `app/versionpreview` controller fences reader
adoption by request, runtime write owner and exact immutable store instance.
It moves the actual owned Working Head and version/file-proof state into protected
suspension, switches independent historical models and returns the original pointers,
Undo/Redo, dirty/path and prepared cell-target identities. Data edits accepted during
a read move with the latest Working Head at adoption. Cancelled/wrong-owner/replaced-
store replies never adopt; a return refuses to overwrite an unrelated live owner.
App remains responsible for readiness, submission/cancellation and presentation
surfaces; no rendering/focus pointer is retained. The `app-preview` gate passes
controller and historical worker checks under optimized/ASan/UBSan in12.663s,
including exact history/pointer restoration, cross-preview switching and R+1 edits.
Visible read-only capabilities and UI/recovery integration remain W11d.4. Evidence:
ignored `build/tests/results/w11d3-app-preview.log`.

W11d.4/W11d complete: File → Versions selects named/Latest Saved states into a
separate read-only Grid/definition viewer. Working model/history/owner and original
Pages/grid/editor stay owned and are restored before document lifecycle guards;
authoring, source jobs and file publication are fenced. Missing source rows and
analytical output values stay explicit placeholders; no external query is issued.
Focused `history-preview` passed in20.383s (optimized/sanitized controller plus
20 GUI stages, including visible text, offline definitions, exact restoration,
Close and deterministic cancellation). The shared version-controls interaction
passed26 stages in20.988s;54 runner/catalog/web contracts passed in12.702s.
Evidence: ignored `build/tests/results/w11d4-{history-preview,version-controls,tool-contracts}.log`.
Historical Save As remains W13; durable Undo/retained data are separate work.
W11e is now the explicit broad compatibility milestone, not a per-slice gate.

W12a complete: shared pure `history_window` consumes adapter-proved version-boundary identities
and operation positions, retaining three boundary states/two intervals plus current Redo and the
unsaved suffix. Labels at one anchor coalesce; A→B→A content reuse cannot erase
its separate event intervals. Fewer than three states retain the declared origin;
an already older cursor extends protection, and a missing required prefix reports
GAP without changing the output. Optimized/sanitized33-case gate passed in2.467s.
Durable anchors/events, replay, file publication and actual pruning remain W12b–W12g;
this preparatory controller does not make existing v16 session history durable.

W12b.1 complete: pure `history_anchors` validates public Saved/tag bindings against
the existing admitted version index, joining immutable tag creation identities
rather than user labels. Every anchor resolves to a known matching root, fits its
high-water and has a public reference; duplicate IDs, cross-document links,
unknown/misbound tags and hidden orphan descriptors fail. Root reuse, independent
operation positions/lineages, zero IDs and exhausted counters stay explicit.
The31-case binding proof plus33-case window passed optimized/sanitized in2.081s,
including maximum1,025 public anchors sharing one content root. Event/path proof,
wire admission and actual durable Undo remain W12b.2–W12g; no file profile changed.

W12 producer boundary: existing worksheet/lifecycle Undo rings retain at most32
session actions and can evict detached content. Save-time ring serialization is
therefore insufficient. W12c.1–W12c.5 admit owned reversible events before mutation
and eviction, including composite actions; a late void record hook cannot promise
durability. Keep the current session behavior until all supported producer families
and the W12d publication path are ready; do not expose partial cross-session Undo.

W11e broad receipt on `f888e33`: optimized/sanitized engine and metadata matrices,
standalone GUI workflows, background/GPU, the full950-stage layout in610.147s,
Base, Workspace and Filter passed. The command then reached Pivot stage31 while
its real versioned Save was still running and exhausted the old1,800-frame
budget; no Pivot assertion or sanitizer diagnostic fired. Its failure budget is
now4,800 frames, with the same45 stages/terminal checks and completion-driven
exit. Continue with the named Pivot and remaining native workflows rather than
rerunning the completed broad matrix. No green full-command receipt is claimed.

Historical viewer follow-up complete: a real-frame regression exposed sheet
navigation clearing live layout/hit regions for one frame. Sheet resource cleanup
now preserves geometry; tests wait for painted controls and validate every drawn
grid frame. Native queued navigation is replayed into the independent reader,
and End uses actual authored/object extent rather than spare stored widths.
The20-stage GUI now verifies visible offscreen AE9001 text and unchanged Working
selection/viewport, as well as exact model/history restoration. `history-viewer`
passed in7.481s; the same workflow with full-App ASan/UBSan passed in42.414s
(including31.990s compilation). The GUI-only feature defers unchanged backend
modules. Evidence: ignored `w11e-history-{layout,preview-sanitize}.log` and the
finite layout/navigation regression receipts under `build/tests/results/`.

W11e continuation: Pivot passes all45 stages in46.581s with the revised ceiling.
Export/Count/Clipboard/Sort/Navigation and the real NYC source pass. Document
reaches its terminal stage10 with Quit's Save still active at its old700-frame
limit, without an assertion/sanitizer failure. Save-heavy Document/Chart/Table/
Unique ceilings now allow versioned IO; all completion predicates, immediate
assertion failures and stage counts remain unchanged. Passing tests do not wait
out a ceiling. Continue at Document rather than repeating the completed checks.

W11e continuation: Document/Chart/Statistics/Table/Icon pass, including Table's
million-row source and Save/Open. Query exposed File's enlarged storage capacity
also enlarging the shared context viewport. Choice storage now supports ten File
commands while scrollable context lists retain nine visible rows; exact geometry
tests cover both. Optimized/sanitized choice/keytip modules and the26-stage native
version-controls workflow pass in12.430s; browser File/access passes in17.334s.
Continue Query/Unique only. The integrated temporary retention checkout was
removed; all authored commits remain reachable on main.

W11e complete by combined receipts, not a green single full-command run. The
frozen `f888e33` broad optimized/sanitized engine/metadata and standalone GUI
matrix, background/GPU and950-stage layout passed. Continuations pass every
remaining native scenario; final Query47/Unique32 pass in37.980s/24.196s
(`20261024T035420-native-23644`). Old Save-heavy deadlines were increased without
removing assertions; the real context-viewport regression was fixed separately.
The final26-stage version-controls gate passes in12.430s; browser File/access
passes in17.334s. Shared production/review Wasm builds and the native bundle
build pass; strict signature and linked shipping test/review exclusion pass.
Historical viewer's20-stage full-App sanitizer and focused model/GUI receipts
above cover the later reader fixes. Existing v15 compatibility stays separate
from explicit full v16 readers. Native source/analytical payload retention,
browser Open/Save and durable Undo are not established by this milestone.
Concise current receipts remain under ignored `build/tests/results/`.

W12b.2a complete: structural path validation proves contiguous retained transition
positions, monotonic durable operation/lineage IDs and shared-prefix ownership,
then joins public anchors to the existing retention planner. Reused content never
substitutes branch identity. A Saved endpoint on an abandoned suffix explicitly
requires separate protection; it is not silently excluded or mapped to the draft.
The49-case path/join plus31-anchor/33-window optimized/sanitized gate passes
in2.561s, including100,000 transitions and exhausted unsigned clocks. No GUI,
archive profile, payload decoder, event producer or cross-session Undo changed.
Payload capture follows the path foundation; final file-profile admission follows
all payload families, avoiding a circular dependency. Evidence:
`build/tests/results/w12b2-history-path.log` (ignored).
The55 runner/catalog/web-build contracts also pass in11.505s.

W12b.2b complete: the same history controller now protects the independent Saved
window and current draft together. Shared-prefix operation IDs must agree;
divergent suffixes cannot reuse IDs or rejoin lineages. Both required floors
extend to an older fork/cursor, preserving the inverse of a new edit even before
the previous Saved window. Either missing interval or regressed durable clock
rejects without changing the output. The77-case path/join/protection module plus
anchor/window optimized/sanitized gate passes in1.700s, including empty Saved,
complete Redo, cross-document rejection and UINT64_MAX endpoints. Evidence:
`build/tests/results/w12b2-protection.log` (ignored). Shared Wasm links the path
foundation; no file format or App mutation behavior changed. Final metadata
admission W12b.2c depends on payload families; the next ready slice is W12c.1.

W12c.1a complete: shared `cell_delta` validates exact sparse manual text/clear
and captures both byte spans into caller-owned storage. Native journal validation
and queue reservation use it before mutation; the existing single allocation,
queue byte accounting, journal schema and operation clocks remain unchanged.
Literal NULL/exact DECIMAL/Unicode bytes survive capture; no-op, invalid target,
embedded NUL and capacity rejection leave outputs/arena unchanged. The43-case
module, journal queue/storage optimized/sanitized checks and18-stage hidden TEST
App journal workflow pass in22.614s (GUI7.811s). The55 tool contracts pass in13.255s;
shared Wasm builds. Evidence: ignored `w12c1-{cell-delta,tool-contracts,web-build}.log`.
This is the common owned payload boundary, not retained cross-session Undo or a
new `.dxls` profile; source typed NULL/zero-length values are outside sparse text.

W12c.1b complete: sparse-cell encoding1 uses a canonical34-byte DXCD header
(fixed little-endian target/length fields, zero reserved flags) plus exact value
bytes, independent of host structs/pointers/session clocks. The closed decoder
rejects unsupported encoding, truncated/trailing/oversized/NUL/no-op records and
borrows only the retained input asset. This is an internal payload family, not a
new `.dxls` profile or activated event producer. The codec and43-case capture
optimized/sanitized gate passes in2.715s;55 tool contracts pass in11.841s.
Evidence: ignored `w12c1-codec*.log`. Event admission/producer handoff remains
W12c.1c; final file binding follows all supported payload families.

W12c.1c.1 complete: `CellEditRecorder` gives the shared command producer one
reserve/recheck/apply/publish-or-cancel boundary. The native journal session now
adapts to it; rejected model mutations no longer consume its operation counter.
There is no extra payload allocation or synchronous IO.715 cell-command checks
include malformed adapters, capacity pressure, preserved Redo, stale-owner fault
injection, model-limit cancellation and40 immutable captured edits beyond the
32-entry session ring. The focused optimized/sanitized command/delta/session and
18-stage App gate passes in21.913s; the changed chronology interaction passes
21 GUI stages in16.347s including modules/build.55 tools pass in14.236s; shared
Wasm links. Receipts: ignored `w12c1-recorder*.log`. This is producer/ownership
admission, not a retained event store, file-profile activation or cross-session
Undo; W12c.1c.2 still owns independent durable event identities/assets.

W12c.1c.2/W12c.1 complete at the payload/producer boundary: shared
`history_stream` owns canonical cell assets before mutation, with independent
action/transition/lineage clocks and runtime document/edit-session fences.
Publish alone advances clocks/path; fork/cancel/backpressure never drops old
assets or consumes rejected IDs. One pending/reserved/in-flight budget remains
charged until explicit completion; failed writes can return FIFO ownership for
retry. Wrong-owner completion and teardown with a live lease reject atomically.
The136-case stream,1,001-command real producer/stream and82-codec optimized/
sanitized gate passes in4.174s, including a40-edit lease surviving32-ring eviction.
55 tools pass in13.003s and shared Wasm builds. Receipts: ignored
`w12c1-history-stream*.log`. Completion in these tests is a mock storage handoff,
not durable IO. The current App still uses its existing recovery recorder;
retained-stream activation, composed recovery/history reservations, cursor
transitions and full file publication wait for all producer families and W12d/e.
The recommended8-MiB staging capacity and256-entry ceiling are backpressure, never history truncation. Explicit capacities up to32 MiB admit supported compound packets under the outer memory governor; capacity is not a preallocated pool.

W12c.2a complete: spatial/scoped format apply now uses public owned planning
helpers, matching the existing record planner. Candidates preserve full scope/
membership ownership and can be canceled without touching mode, Redo, revision
or history. Semantic match detects no-op independently of pointer identity.
The89,276-case format module passes optimized/sanitized; a new10-stage hidden
App slice covers actual Bold pointer input, Italic/Underline keys, Undo/Redo,
canceled scoped/no-op plans,100/150 percent and versioned Save/Open. Gate12.923s,
GUI3.838s;55 tool contracts13.149s. Evidence: ignored
`w12c2-format-plan*.log`. The148-stage format-controls/source-format interactions
stay registered/deferred. No format-event file profile is activated yet.

W12c.2b complete: format encoding1 stores canonical before/after snapshots,
including mode, six stored style properties, exact binary scopes, guard indices
and64-bit record/original-field vectors. Derived specified bits, native structs,
revision clocks, live defaults and source high-water are absent. Two-pass decode
preflights exact owned arrays/bytes plus peak uniqueness scratch before allocation;
full model validation and semantic-change checks precede replacing an owned out.
The405-case codec and89,276-case format optimized/sanitized gate passes in3.709s;
55 tools pass in12.238s. Coverage includes an independent byte oracle, inner/outer
truncation, duplicate scopes/membership, bounded scratch, maximum membership,
UINT64_MAX/large integers, mode-only state and input-independent owned graphs.
Evidence: ignored `w12c2-format-codec*.log`. File/document/source-version closure
and producer activation remain W12c.2c; opaque scope bytes alone never prove them.

W12 format integration boundary: source-version issuance/completion stays outside
Undo. Legacy guarded-baseline preparation must precede the retained format
producer's captured before-state/origin; source/import/refresh migration effects
must be explicit composites, not silent rewrites of immutable historical assets.
Source-version rows/locators/packets referenced by scopes must remain owned through
pending publication and later sheet/object deletion. Closure work is coordinated
with W12c.3 source-definition assets and W12c.5 lifecycle payloads before activation.

Focused codec build-cache slice complete: format tests reuse compatible native
per-file objects; source-scope keeps a dedicated cached strict -Werror graph.
Both use actual compiler-generated header dependencies and configuration
signatures, not global HEADERS inventory, and can compile with eight job slots.
The format/source-scope/format-codec optimized/sanitized gate passes in5.794s;
55 tools pass in11.870s. An actual unrelated HEADERS override leaves all30 watched
objects/binaries unchanged in0.643s. Evidence: ignored `w12-codec-cache*.log/json`.
No product behavior or assertion coverage changed; wider GUI/milestone work is
still deferred. Other legacy module build conversions remain separate.

W12c.2c.1 complete: source-version metadata encoding1 preserves exact ordered
IDs, owner, explicit CSV/Parquet tags, locators, pending/complete RAW provenance
and captured high-water. It stores no dataset bytes, resolves no path and performs
no IO. Decode preflights owned memory and validates ledger/packet identity before
replacing an independent asset; unused allocation slots are not serialized.
The271-case asset and13,894-case source-scope optimized/sanitized gate passes
in2.783s;55 tools pass in12.430s. Cases include a byte oracle, corruption/truncation,
UTF-8/wildcards, packet mismatch, exact budgets, maximum ledger/locator, large IDs,
UINT64_MAX and input-independent copied packets. Receipts: ignored
`w12c2-source-versions*.log`. Historical metadata cannot be assigned over a newer
live allocator/known completion; final projection/closure remains owner work.
Source bytes and retained file-history publication remain separate roadmap work.

W12c.2c.2 complete: format dependency closure copies only the immutable source
rows referenced by before/after guards, deduplicated across both snapshots.
Exact owner/version/kind/RAW bindings and original-field schema bounds are
required; missing/pending/conflicting packets reject without changing inputs or
an existing output. Opaque non-record legacy scopes remain explicitly inactive;
unknown record identity rejects. Unreferenced scopes do not invent dependencies.
The34-case closure,271-case source asset and405-case format codec gate passes
optimized/ASan/UBSan in2.926s;50 runner/catalog contracts pass in2.394s
(no GUI boundary changed). Receipts: ignored
`w12c2-format-dependencies*.log/json`. This owns metadata only; source/Page proof,
atomic producer admission and retained file publication remain separate work.

W12c.2c.3a complete: typed format reservations copy canonical before/after and
closed source-metadata assets before mutation. They share bounded staging,
operation/transition IDs, lineage and FIFO dispatch/retry with cell events.
Cancel, pressure, invalid owner/revision and missing dependencies preserve clocks,
Redo and existing assets. Format-only admission rejects every other effect set,
so source/analysis/object/range composites cannot masquerade as complete events.
The445-case format event,136-case stream and1,001-case real cell producer gate
passes optimized/ASan/UBSan in4.285s;50 runner/catalog contracts pass in2.309s.
The shared-C Wasm build also passes. Coverage includes exact byte/entry pressure,
source lifetime independence, copied maximum IDs/high-water, retry, mixed event
ordering and40 format commits retaining the first event beyond32 session entries.
Receipts: ignored `w12c2-history-format*.log/json`. No GUI boundary changed;
source/format/workbook fence recheck plus full composite adoption is W12c.2c.3b,
and durable publication/App activation remains W12d after all producer families.

W12c.2c.3b.1 complete: ordinary source-free/object-free App formatting now uses a
shared typed command. It resolves durable sheet IDs across reorder, checks the
independent runtime owner plus cell/format/document clocks, validates the owned
plan and projected history budget, reserves before mutation, rechecks, then
publishes one action/dirty/branch/document revision. No-op reserves nothing and
preserves Redo; rejection preserves the prepared graph. Source-aware/analytical
commands remain with their existing owner pending full composite admission.
The237-case command,445-case format event and565-case chronology tests pass
optimized/ASan/UBSan;177 native journal-session checks additionally prove that an
admitted format revision requires a new checkpoint before the next cell append,
and its styles survive recovery. The hidden10-stage real-App workflow passes
20 checks at100/150 percent, actual Bold/Italic/Underline and Undo/Redo, no-op
Redo preservation, monotonic command revisions and versioned Save/Open. Combined
local gate13.073s; GUI3.092s;50 runner/catalog contracts pass in2.403s. Receipts: ignored `w12c2-format-command*.log/json`.
Retained event reservations are exercised with the real controller through40
edits beyond the32-entry session ring, but retained history remains inactive in
the App until all producer families and ordered publication are ready.

W12c.3a complete: closed Analysis encoding1 retains exact nullable UTF-8 texts,
query/unique/group/aggregate state, ordered filters/sorts and stable source-version
bindings. Framing and owned+grammar-scratch admission precede decode allocation;
encode failure preserves its arena. Derived schema caches require later verified
rebinding, not replay as user edits. The focused optimized/ASan/UBSan gate passes
in2.613s;50 runner/catalog contracts pass. Byte oracle, inner/outer truncation,
invalid grammar/tags/UTF-8, exact budgets, full vectors,64 KiB query and8 MiB owned
boundaries pass. Receipts: ignored `w12c3-analysis-asset*.log/json`. No App/file
profile activation or source materialization claim; object/composite work follows.

W12c.3b.1 complete: Pivot/Chart definition encoding1 preserves all ordered shelves,
aggregates, date grains and nullable filter payloads. Shared checked definition
wire/text/scratch primitives preserve the existing Analysis byte contract. The
350-case Pivot and383-case Analysis optimized/ASan/UBSan gate passes in4.081s;
50 runner/catalog contracts pass in2.550s. Malformed Pivot validation now safely
accepts an omitted diagnostic pointer instead of dereferencing NULL. Byte oracles,
inner/outer truncation, grammar/UTF-8/tag errors, full vectors, duplicate shelves
and8 MiB boundaries pass. Receipts: ignored `w12c3-pivot-asset*.log/json`.
No output materialization, source execution, App profile activation or GUI change.

W12c.3b.2 complete: Marks encoding1 preserves all six ordered roles, exact UTF-8
names, latent aggregate values, date grains and measure/dimension flags. The
292-case optimized/ASan/UBSan gate passes in2.638s;50 runner/catalog contracts pass
in2.303s. Byte oracle, inner/outer truncation, invalid tags/UTF-8, role/duplicate
rules, exact owned budgets and all34 maximum-length assignments pass. Receipts:
ignored `w12c3-marks-asset*.log/json`. No GUI/runtime chart behavior changed.

W12c.3b.3a complete: allocation-free nested preflight reports exact sheet/owner,
framing/text and owned/parser-scratch requirements. Decoders reuse it; semantic
admission still requires full decode (a framed invalid filter explicitly proves
that distinction). The432 Analysis/383 Pivot/310 Marks optimized/ASan/UBSan gate
passes in4.815s;50 runner/catalog contracts pass in2.333s. Receipts: ignored
`w12c3-definition-preflight*.log/json`. The complete object graph can now be
budgeted before any constituent allocation; object envelope/capture is next.

W12c.3b.3b complete: one closed object asset owns ordered IDs/geometry/names,
source/Table links, chart representation/measure and all nested definitions.
Complete graph preflight precedes any array/string copy; capture preserves the
old packet until all constituents encode, and decode preserves the old graph
until full nested/local source-graph semantics pass. Captured allocator maxima
cannot rewind live counters; runtime query epochs and schema caches need fresh
owner rebinding before adoption. The1,034-case object plus definition codec gate
passes optimized/ASan/UBSan in3.474s;50 runner/catalog contracts pass in2.313s.
Coverage includes exact nested owner associations, unsupported/corrupt children,
late semantic rejection, independent ownership, byte-identical recapture despite
runtime-epoch reset, all32 objects and8 MiB owned boundaries. Receipts: ignored
`w12c3-objects-asset*.log/json`. No retained App/file publication or cached outputs.

W12c.3c.1 complete: primary/Table source closure pins both definition states'
exact owner/kind/locator rows, deduplicates immutable versions, preserves pending
metadata and classifies version0 as unbound observations. A source change retains
old and new versions; missing/conflicting/unsupported bindings reject atomically.
Shared selected-row ownership now serves both format and definition closure.
The44-case closure,34-case format,271-case source codec and13,894-case source-scope
optimized/ASan/UBSan gate passes in2.751s;50 tool contracts pass in2.325s.
Receipts: ignored `w12c3-definition-dependencies*.log/json`. Source materialization,
full definition semantics and event/App admission remain separate obligations.

The shared-C production Wasm build passes at422c89d, covering all current nested
object/definition/source codecs and closure components. Receipt: ignored
`w12c3-definition-assets-web-build.log`; this is compile/link, not browser Open/Save.

W12c.3c.2a complete: pure typed definition equality retains exact nullable texts,
field/paint order, source IDs and all stored intent. Derived NumberType/query
epochs and computed Pivot/Table extents are excluded; authored Chart dimensions
remain strict. The74-case optimized/ASan/UBSan gate passes in2.734s;50 tools pass
in2.317s. Receipt: ignored `w12c3-definition-state*.log/json`. Inspection found
worker default schema/table completion also occurs without user actions; equality
keeps that identity strict and W12c.3c.3a must close its baseline/replay ownership
before activation. No unsafe source-name waiver or App behavior change is made.

W12c.3c.2b.1 complete: source-row selections no longer require temporary ledger
clones. Format reservations encode selected metadata directly, byte-identical to
owned-subset encoding. Shared Analysis grammar-scratch measurement now serves
canonical definitions and the metadata reader; source-scope grammar scratch is
explicitly documented in compound admission. Dependency selection failures are
atomic. Three registered gates preserve the four-module catalog limit: compound
preparation2.527s, named definition preflight2.742s and source codec1.091s, all
optimized/ASan/UBSan;50 tool contracts pass in2.345s. The metadata reader now uses
cached strict per-file objects/actual .d dependencies with eight compile slots.
An unrelated HEADERS override leaves all24 watched objects/binaries unchanged
in0.870s. Receipts: ignored `w12c3-compound-*.log/json` and
`w12c3-metadata-read-cache*.log/json`. No App/GUI/profile activation changed.

W12c.3c.2b.2a complete: format/source assets expose allocation-free compound
framing and ownership inspection, reused by their ordinary decoders. Tests prove
that no-op format frames and well-framed invalid locators still require semantic
decode. The422-case format and291-case source optimized/ASan/UBSan gate passes
in3.601s;50 tool contracts pass in2.312s. Receipts: ignored
`w12c3-compound-preflight*.log/json`. No IO/model/profile/GUI behavior changed.

Compound object staging complete within W12c.3c.2b.2b: a private validated writer
encodes directly into a disjoint unpublished compound buffer, without allocating
another full object packet. Failure preserves written but the caller discards
unpublished bytes; the public packet API keeps atomic output replacement.
The1,046-case optimized/ASan/UBSan object gate passes in3.133s;50 tool contracts
pass in2.334s. Direct/public bytes match, including maximum graphs and late Table
semantic rejection. Receipts: ignored `w12c3-compound-object-staging*.log/json`.
Compound envelope/effect/dependency closure and stream handoff remain next.

Closed compound packet capture/decode complete within W12c.3c.2b.2b: Analysis,
Objects and their Format composites bind exact changed family pairs, nested
owners and one deduplicated source union. Complete graph/grammar admission
precedes copied constituents; encoding writes one final private buffer, and
failure preserves the prior packet/graph. Extra dependencies, false observation
counts, wrong effects and partial/unsupported children reject. The583-case packet,
74-case intent and50-case dependency optimized/ASan/UBSan gate passes in3.635s;
50 tools pass in2.500s. Coverage includes distinct-owner unions, dormant legacy
scopes, pending bindings and a >16 MiB compound with exact graph budgets. A
sanitizer-detected intermediate pointer-arithmetic bound was corrected before
acceptance. Receipts: ignored `w12c3-definition-event*.log/json`. Stream admission
must accommodate supported large definition packets under the outer governor;
App producers/publication remain inactive until their own required slices.


W12c.3c.2 complete: the typed definition recorder reserves closed DXDE packets
in the same FIFO/action/transition/lineage clocks as cell and format events.
Encoding writes one admitted event allocation; cancellation, queue pressure and
invalid/no-op input preserve clocks and Redo. The276-case recorder gate plus
stream/format/packet optimized and ASan/UBSan tests passes in4.366s. The named
cell-command/stream interaction passes in5.911s;50 tool contracts pass in2.314s.
A >16 MiB before/after packet rejects an8 MiB capacity without mutation and
succeeds with explicitly admitted32 MiB, retaining bytes after inputs are freed.
Receipts: ignored `w12c3-history-definition*.log` and corresponding run summaries.
No App producer, file profile or durable-history activation is implied. Next:
baseline/readiness ownership and fenced definition producers (W12c.3c.3).


W12c.3c.3a.1 complete: pure baseline readiness checks now guard typed definition
stream admission. Unresolved DATABASE defaults remain representable in legacy/core
packets, but cannot enter immutable retained events. Invalid empty identifiers,
unresolved primary/Table pairs and mixed object snapshots reject without clocks,
queue ownership or Redo changes. The24-case readiness and290-case recorder gate
passes optimized/ASan/UBSan in3.988s;50 tool contracts pass in2.606s. Receipts:
ignored `w12c3-definition-baseline*.log` and corresponding run summaries. Full
origin completion integration remains W12c.3c.3a.2; no App activation changed.


W12c.3c.3b.1 complete: the borrowed-context primary transform controller accepts
independent same-connection candidates and binds actual App/worksheet authority,
durable/session identity, runtime source epoch, cell/format/document clocks,
history order and source high-water. It admits parser scratch before retained
state validation, proves source closure, reserves before bounded-history eviction
and publishes one coherent Analysis action. Authored no-op ignores derived types
and preserves Redo. The201-case command,290-case recorder and565-case chronology
optimized/ASan/UBSan gate passes in4.328s;50 tool contracts pass in2.684s. Coverage
includes independent App Save caches, sheet reorder, callback cancellation/stale
fences, unresolved defaults, ownership aliases, pressure and immutable first-event
bytes beyond32 session actions. Receipts: ignored `w12c3-analysis-command*.log`.
App query/cache invalidation remains the adapter's responsibility; retained App
history and source replacement/composite producers remain inactive/queued.


W12c.3c.3b.2 complete: primary Remove duplicates now uses the fenced command
controller rather than recording history before mutation. Allocation-free scratch
inspection admits candidate/grammar coexistence under the native App governor;
source refresh/read-only/stale states cannot enter the transform. Existing query
invalidation and the Table source owner remain in their App adapters. The202-case
command,290-case recorder and565-case chronology modules pass optimized/ASan/UBSan;
216 native journal checks prove a new definition forces a checkpoint before the
next cell tail and recovers the exact transformed source. The hidden TEST App
passes10 stages/23 checks including Cmd-Z/Redo, no-op Redo preservation, stale
preparation, Enter-driven pipeline removal,150% layout and v16 Save/Open. Combined
gate24.020s;50 tool contracts2.917s. The GUI waits for resized layout publication
before Save R (the initial simultaneous resize correctly preserved newer dirty
state). Receipts: ignored `w12c3-analysis-command-app*.log` and run summaries.
Retained App history/durable Undo remains inactive; broader interactions are
registered but deferred. Next: remaining primary same-connection adapters.


W12c.3c.3b.3a complete: primary sort and per-column filter edits now prepare
independent definitions and share explicit budget/adoption helpers with duplicate
transforms. Failed admission keeps the candidate/live history unchanged; accepted
plans record one command, while sort/filter no-op preserves query clocks and
Redo. The222-case optimized/ASan/UBSan command suite additionally proves complete
query/filter/multi-sort event bytes, empty-text preservation, derived-type no-op
and authored filter/sort reordering. Its narrow hidden TEST GUI passes11 stages/
25 checks including actual Sort ribbon input, DuckDB order/filter values,
Cmd-Z/Redo, clearing transforms and v16 Save/Open (15.866s gate). The named shared
unique/recovery interaction passes19.204s (10 GUI stages;216 native journal
checks/config);50 tool contracts pass2.514s. Receipts: ignored
`w12c3-primary-filter-sort*.log` and run summaries. Broader sort/filter scenarios
remain registered for milestone coverage. Group/reset/validated-query migration
is the next primary lane; retained App history remains inactive.


W12c.3c.3b.3b complete: grouping, reset, Clear filters and validated primary
source-query application now adopt independent complete after-plans through the
shared command boundary. Reset/Clear with no authored change preserve Redo and
query clocks. Query admission moves the worker reply's already-owned definition
instead of cloning another full graph; failed validation/admission keeps editor
text and the current source. Native accounting includes the held reply wrapper.
The222-case command and290-case recorder optimized/ASan/UBSan gate plus hidden
TEST App passes16.551s:14 GUI stages/32 checks cover exact grouped totals,
Cmd-Z/Redo, no-op reset, actual Command-Enter invalid/valid SQL, draft retention,
query ownership after reply disposal, clear/reset semantics and v16 Save/Open.
The named sort/filter interaction passes13.785s (release/11 GUI stages);50 tool
contracts pass2.694s. Receipts: ignored `w12c3-primary-group-query*.log` and run
summaries. Primary pipeline step editing is the remaining same-connection lane;
connection replacement/object composites remain separate and retained App history
is still inactive.


W12c.3c.3b.3 complete: the final same-connection primary lane, pipeline step
removal/reordering, now reserves/adopts through the same command boundary. Filter
display order remains authored history while preserving the exact current Page,
request, Analysis epoch and worker query/job counts; result-changing removal still
invalidates/refetches. The222-case command/290-case recorder optimized/ASan/UBSan
gate plus hidden TEST App passes14.763s. Its11 stages exercise actual Alt-Up and
Delete input, Cmd-Z/Redo across both cache-preserving and result-changing edits,
command clocks and v16 Save/Open. Fifty tool contracts pass2.528s. Receipts:
ignored `w12c3-primary-steps*.log` and run summaries. All regular primary
same-connection paths now prepare after-plans; source replacement/rebinding,
object/format composites and baseline origin integration remain separate W12
work. This does not activate durable history or change the native v16 core profile.


W12c.3c.3b.4a.1 complete: Table source transforms now have allocation-free complete
projection measurement and an independent owned after-plan. Connection/binding
changes and unresolved defaults reject; derived-type no-op owns no new graph;
filter display reorder preserves query revision, while result changes advance it
without wrap. Planning admits complete graph/grammar budgets before cloning,
never clones/prunes Undo/Redo, and preserves prior output on failure. The focused
planner,1,046-case object asset and74-case intent optimized/ASan/UBSan gate passes
3.564s;50 tool contracts pass2.729s. Receipts: ignored
`w12c3-object-source-plan*.log` and run summaries. No App adoption or retained
history activation is claimed; fenced Table adoption is next.

Primary-producer packaging boundary: sourceb88c3b5 has a signed native bundle
(8.099s), compiled commit provenance and verified absence of private feature/review
hooks. The normal shared-C Wasm application compiles/links in24.294s;28 affected
sources rebuilt. Receipts: ignored `w12c3-primary-producers-bundle.log`,
`w12c3-primary-producers-package.json` and `w12c3-primary-producers-web.log`.
No user app was launched/closed and no browser runtime/Open/Save acceptance is
inferred from compilation. Table planning/adoption primitives compile in both
targets; their App command integration remains queued.


W12c.3c.3b.3d complete: a narrow GUI regression reproduced rejection of a valid
replacement query solely because the prior Page was stale/failed. Primary query
adoption now accepts a successful JOB_VALIDATE_SQL reply only when its candidate
address, request ID, primary object role and Analysis epoch match. Ordinary field
transforms still require the current result; connection/session/read-only fences
are unchanged. The15-stage/36-check hidden TEST GUI proves recovery, rejects an
ordinary duplicate transform and stale synthetic validation evidence, and retains
invalid SQL drafts, no-op Redo, typed aggregate values and Save/Open coverage.
The focused optimized/ASan/UBSan modules plus GUI pass16.558s;50 tool contracts
pass2.470s. Receipts: ignored `w12c3-stale-query-repro.log` (expected failure),
`w12c3-stale-query-fix.log`, `w12c3-stale-query-tools.log` and run summaries.
This is same-connection validated recovery, not offline validation or unresolved
DATABASE baseline activation. Table plan ownership also passes159 checks/config;
fenced Table adoption remains the next scheduled implementation.


W12c.3c.3b.4a.2a complete: the nonfallible model transfer consumes an admitted
independent Table after-plan, moves the prior current graph into bounded Undo
and clears Redo without cloning it again. Store allocation/title/Marks counters
remain intact; planned per-object query epochs are installed. The290-case planner/
transfer,1,046-case object asset,74-case intent and existing object primitive
optimized/ASan/UBSan gate passes3.278s;50 tool contracts pass2.527s. Tests cover
exact moved pointers, Undo/Redo ownership,40 further actions,6 MiB graphs and
multi-snapshot eviction under the independent8 MiB history limit. Receipts:
ignored `w12c3-object-source-adopt*.log` and run summaries. This primitive requires
prior complete command/event admission; it is not installed in the App yet.
Fenced Table reservation/adoption/App integration remains W12c.3c.3b.4a.2b.


W12c.3c.3b.4a.2b.1 complete: the typed Table controller owns prepare/check/plan/
reserve/recheck/adopt/publish for same-connection definitions. Runtime fences
capture durable/session IDs, request/document/cell/format/history state, complete
native object/store headers and immutable source ledger row descriptors. Non-user
extent/cache/default completion cannot silently overwrite a prepared graph.
The controller closes complete before/after source dependencies and reserves DXDE
before any bounded object-history eviction. Input proposals remain caller-owned;
no-op/pressure/cancellation preserve history/Redo/clocks. The focused four-module
optimized/ASan/UBSan gate passes8.347s;50 tool contracts pass4.005s. Coverage
includes sheet reorder, callback stale/denial paths, pending-source completion,
query-cache fences, display-only filter order, immutable first events beyond32
commands and queue pressure. Receipts: ignored `w12c3-table-command*.log` and run
summaries. App readiness/invalidation and complete global staging accounting
remain with the next Table adapter; retained history is not activated.


W12c.3c.3b.4a complete: regular same-connection Table filter/sort/unique/query/
pipeline edits now use the admitted controller with native candidate, caller
proposal, parser and reply-wrapper coexistence accounting. Connection/binding/
default-relation replacement remains with its separate owner. App call sites
were checked for old SheetObject pointers across graph adoption. The280-case
command,290-case plan,290-case recorder and565-case chronology modules pass
optimized/ASan/UBSan;256 native journal checks prove Table definitions enter the
next checkpoint before a cell tail. The hidden TEST App passes14 stages/33 checks:
actual Command-Enter/Alt-Up/Cmd-Z/Redo, target and dependent Pivot values, untouched
unrelated Table Pages, display-only filter reorder without new jobs, and v16
Save/Open. Gate26.471s. The named source-result-format interaction passes22.962s
(24 native stages plus scoped modules);50 tool contracts pass2.492s. Its existing
layout profile is now a registered feature, fixing four deferred commands, and
tool contracts reject nonexistent feature references. Receipts: ignored
`w12c3-table-command-app*.log`, `w12c3-table-source-format.log` and run summaries.
No full regression, browser runtime or durable-history activation is claimed.
Source/object replacement and format composites remain W12c.3c.3b.4 work.


W12c.3c.3b.4b.1 complete: shared object transaction fences now live in
`object_fence.c`; Table commands retain their explicit kind/default-relation
readiness and event/budget owner. Full native descriptor capture/check logic is
shared, without relaxing any source/layout/request/metadata fence. The unchanged
Table scoped module/sanitizer/journal gate and14-stage native workflow passes
26.468s. Receipt: ignored `w12c3-object-fence-table.log` and run summary. This
prepares the next presentation producer; it does not broaden App history activation.


W12c.3c.3b.4b.2a complete: rename drafts now freeze document/sheet sessions,
object ID and prior name independently from background result completion. A fresh
complete object fence captures latest layout/source/cache state at commit; closed
DXDE reservation precedes eviction, and name/title/action/document clocks publish
once. Shared allocation-free normalization preserves trim/control/uniqueness/
size rules for the legacy model API. Generic definition ownership transfer avoids
another graph copy; title counters cannot wrap and query/Marks/allocator counters
stay unchanged. The232-case rename,280-case Table,290-case recorder and existing
object optimized/ASan/UBSan gate passes3.699s;50 tool contracts pass2.522s. Tests
cover UTF-8, duplicate names, background rebase, stale owners/renames, no-op Redo,
callback cancellation/pressure, exact source closure and immutable events beyond32
actions. Receipt: ignored `w12c3-object-name-command*.log`. App integration remains
next; no retained App history activation is claimed.


W12c.3c.3b.4b.2 complete: the real rename editor owns a small draft target from
opening through commit. Duplicate/invalid/stale edits keep their text; current
selection/document identity is checked, and background request changes do not
invalidate an otherwise valid name. Native planning admits graph/parser coexistence.
The232-case rename,280-case Table,290-case recorder and object primitive modules
pass optimized/ASan/UBSan. Native journal coverage additionally proves a renamed
Table enters the checkpoint before the next cell tail. The hidden TEST App passes
16 stages: actual Enter/Escape/Cmd-Z/Redo, trim/no-op Redo, duplicate and competing
rename rejection, request-epoch rebase, Table/Pivot/Chart Page/job preservation,
150% view publication and v16 Save/Open. Combined gate21.629s;50 tool contracts
pass2.715s. Receipts: ignored `w12c3-object-name-app*.log` and run summaries.
No full regression or retained App history activation is claimed. Remaining
presentation/Marks/representation producers stay under W12c.3c.3b.4b.


Table/rename packaging boundary: product sourceba5f8c9 produces the signed native
bundle in6.964s; compiled commit provenance and exclusion of private feature/review
symbols pass. The normal shared-C Wasm application compiles/links in24.873s with31
affected sources rebuilt. Receipts: ignored `w12c3-table-name-bundle.log`,
`w12c3-table-name-package.json` and `w12c3-table-name-web.log`. No user app was
launched/closed, no broad regression was run, and browser runtime/Open/Save or
retained-history activation is not inferred. Next: W12c.3c.3b.4b.3 chart mark/
measure admission, followed by Marks bindings and representation/sizing.


W12c.3c.3b.4b.3 complete: Chart mark and selected-measure edits now use a
Page-independent presentation controller with the full object transaction fence,
complete graph/grammar/source closure and optional DXDE reservation before
history eviction. Changed commands publish one object action/document revision;
only measure changes advance the title counter. Source/query/Marks/allocator/
layout state remains intact; no-op preserves Redo even without plan capacity.
App retains scatter/schema availability and native memory admission. The244-case
controller,290-case recorder and318-case object modules pass optimized and
ASan/UBSan. The hidden TEST App passes15 stages/31 checks: actual mark shortcut,
measure menu, Cmd-Z/Redo, no-op Redo, unavailable scatter rejection, three Page/
job/request caches,150% layout and v16 Save/Open. Reopen explicitly selects
lazily loaded offscreen results. Combined gate16.979s (GUI8.207s);14 feature catalog
contracts pass. Receipts: ignored `w12c3-object-presentation-app.log`,
`w12c3-object-presentation-tools.log` and run summary. No broad regression or
retained-history App activation is claimed. Next: W12c.3c.3b.4b.4 Marks bindings.


W12c.3c.3b.4b.4 complete: the presentation controller now admits complete
borrowed Marks binding proposals, deep-clones them in the owned after-graph,
reserves the same complete event before eviction and advances only the Marks
counter. Canonical authored equality retains latent dimension aggregate/grain
state; malformed/UTF-8/noncanonical proposals and exhausted counters fail before
mutation, while no-op preserves Redo. App native admission also charges caller
binding staging. Unsupported encoding/grain/lineage behavior and query/source/
layout/allocator state remain bounded and unchanged. The212-case binding,
244-case presentation,290-case recorder and310-case Marks asset modules pass
optimized/ASan/UBSan;50 tool contracts pass2.425s. The hidden TEST App passes15
stages/50 checks: real Color/Size menus, keyboard clear, Cmd-Z/Redo, aliased no-op
Redo, complete six-role binding publication, three Page/job/request caches,
150% layout and v16 Save/Open. Combined gate18.731s (GUI8.461s). Receipts: ignored
`w12c3-object-marks-app.log`, `w12c3-object-marks-tools.log` and run summary.
No broad regression or retained App event writing is claimed. Next:
W12c.3c.3b.4b.5 representation/sizing command admission.


Chart/Marks packaging boundary: product source4f656f3 produces the current native
bundle with strict signature, compiled commit provenance and private feature/
review symbol exclusions verified. The normal shared-C Wasm application compiles/
links in23.511s wall time. Receipts: ignored `w12c3-chart-marks-bundle.log`,
`w12c3-chart-marks-package.json` and `w12c3-chart-marks-web.log`. No user App was
launched/closed. This is build/packaging evidence, not browser runtime/Open/Save,
retained-history activation or full regression acceptance.


W12c.3c.3b.4b.5 and presentation admission complete: a layout controller borrows
live source extent/count scalars and freezes them with the full object fence.
Complete rectangle/graph/grammar/source validation and event reservation precede
one ownership move/action/document revision; query/title/Marks/allocator/source
state stays intact. Pending counts protect the entire source lane. Changed
extent/manual/object state after reservation cancels atomically. App retains
schema/scatter checks, native memory admission, exact chart-selection resizing
and compatible Pages/jobs; matrix conversion requires an accepted result extent.
Render adapters re-resolve object pointers after graph adoption. The265-case
layout,244-case presentation,290-case recorder and318-case object optimized/
ASan/UBSan gate passes18.047s;50 tool contracts pass2.312s. The hidden TEST App
passes18 stages/42 checks (GUI9.013s): real representation menu, size dialog,
corner gesture, selection extent, representation and sizing Undo/Redo, stale
Page-owner rejection, no-op Redo, Cmd-Z/Redo, manual collision
rejection, unchanged three Page/job/request caches,150% layout and v16 Save/Open
including the untouched blocking note. Receipts: ignored
`w12c3-object-layout-portable-app.log`, `w12c3-object-layout-tools.log` and run summary.
No broad regression or retained App writer activation is claimed. Remaining
source/object producers are now explicitly ordered as W12c.3c.3b.4c–4f; next is
complete Pivot definition admission rather than early durable-history activation.


Layout portability correction: the shared-C Wasm build caught the previously
native-test-only location of the App result-readiness helper. The helper now
lives in shared production App code and also requires matching result/object ID;
no Page from another object can supply a matrix extent. The scoped gate above
includes explicit representation Undo/Redo and stale-result owner rejection.


Layout packaging boundary: corrected product source1661b37 produces the signed
native bundle in6.683s and the normal shared-C Wasm application in23.692s wall
time. Strict signature, compiled commit provenance and private feature/review
symbol exclusions pass. Receipts: ignored `w12c3-object-layout-bundle.log`,
`w12c3-object-layout-package.json` and `w12c3-object-layout-web.log`. No user App
was launched/closed. Compile/link acceptance does not imply browser runtime or
browser workbook Open/Save, durable history or full regression acceptance.


W12c.3c.3b.4c complete: a Pivot definition controller admits full field/aggregate/
date/filter/reorder proposals with canonical graph/grammar/source closure and
optional DXDE reservation before history eviction. Changed definitions advance
one object query revision/document action without wrap; no-op (including derived
NumberType-only changes) preserves Redo and cache. Shared allocation-free measure
selection remapping keeps the legacy model and admitted producer consistent.
App field authoring and the separate filter editor now use this owner; only the
changed object's result is invalidated. Native memory admission includes caller
proposal staging; render/input owners re-resolve pointers after adoption.
The 234-case command, 290-case recorder, 383-case Pivot asset and 318-case object
modules pass optimized/ASan/UBSan. The hidden TEST App passes18 stages/39 checks
(GUI 9.550s; combined 19.056s): actual Alt-Left reorder, aggregate/delete keys,
filter Apply, month/year authoring, no-op Redo, Cmd-Z/Redo, unrelated Table/Pivot
Page preservation and 150% v16 Save/Open. The finite eight-row Date/City/Amount
fixture proves the filtered yearly average is 65 before and after reopen. Resize
settles before Save so subsequent viewport publication is not mistaken for a
failed save of the captured state. Fifty tool contracts pass in 2.348s. Packaging
evidence follows below. Receipts: ignored `w12c3-object-pivot-app.log`,
`w12c3-object-pivot-tools.log` and run summary. No broad regression or retained
App writer activation is claimed. Next: W12c.3c.3b.4d source connection/binding
replacement, retaining the separate complete format-composite boundary.


Pivot packaging boundary: product source 9b69891 produces the signed native
bundle in 6.897s and the normal shared-C Wasm application in 29.389s wall time.
Strict signature, compiled commit provenance and private feature/review symbol
exclusions pass. Receipts: ignored `w12c3-object-pivot-bundle.log`,
`w12c3-object-pivot-package.json` and `w12c3-object-pivot-web.log`. No user App was
launched/closed. Compile/link evidence does not establish browser runtime,
browser workbook Open/Save, durable history or broad regression acceptance.


W12c.3c.3b.4d.1 complete: explicit Table replacement reuses the bounded projection/
transaction, retaining both source dependencies and guarding resolved DATABASE
endpoints. The App legacy fallback is removed. Source-version changes invalidate
results even with unchanged query text; unrelated graph/format/ledger state stays
intact. Shape-measured result comparison now avoids parsing filter JSON before
scratch admission; the public validated comparison retains its previous semantics.
The 204-case replacement, 280-case Table, 290-case planner and 290-case recorder
optimized/ASan/UBSan gate passes in 20.624s, including 16 native stages/45 checks
(GUI 10.516s): relation/view selection, file rebind, Undo/Redo, no-op Redo,
dependent Pivot totals 300→3000, stable unrelated Page/column format and v16
Save/Open at 150%. The named source-result/guarded formatting interaction passes
24 stages in 23.919s; 51 tool contracts pass in 2.561s. Finite SQL/CLI fixtures
avoid the unrelated core/million-row suite and preserve exact DECIMAL text.
Receipts: ignored `w12c3-table-replacement*.log` and run summaries. No broad
regression, retained App writer or dataset retention is claimed. Primary relink
and analytical source_id admission remain W12c.3c.3b.4d.2–4d.3.


Table replacement packaging: source 821acc1 produces the signed native bundle
in 1.975s and normal shared-C Wasm build in 33.430s wall time. Strict signature,
compiled commit provenance and private feature/review exclusions pass. Receipts:
ignored `w12c3-table-replacement-bundle.log`, `w12c3-table-replacement-package.json`
and `w12c3-table-replacement-web.log`. No user App was launched/closed; browser
runtime/Open/Save, durable history and broad regression remain separate gates.


W12c.3c.3b.4d.2 complete: explicit primary replacement reuses Analysis transaction
admission with full live/cached definition, allocator and immutable ledger fences.
Reservation also freezes the owned candidate descriptor. Changed commands guard
source/document clock exhaustion, close both source dependencies and publish one
action; no-op preserves Redo. App native/text relink and queued relation selection
share one owner, admit clone/scratch coexistence and allow actual unavailable-result
recovery while ordinary transforms keep their result-readiness checks. Relation
selection clears query/filter/sort/group/unique state; worker default enrichment
remains separate. The scoped gate passes in 19.568s with 17 native stages/42 checks
(GUI 9.050s), including actual missing-file query failure, relink recovery,
Undo/Redo/no-op Redo, transform reset, dependent Pivot and unrelated Table Page,
150% v16 Save/Open. The expanded primary module passes 189 optimized/ASan/UBSan
checks (additional binding run 2.866s); existing Analysis/recorder/dependency modules
also pass. Named primary-group/query (15 stages, 17.108s) and source-result/guarded
formatting (24 stages, 22.554s) interactions pass; 50 tool contracts pass in 2.488s.
Receipts: ignored `w12c3-primary-replacement*.log` and run summaries. No full
regression, native file-panel gate, retained App writer or dataset retention is
claimed. Next: W12c.3c.3b.4d.3 analytical source_id admission.


Primary replacement packaging: source e8d3d00 produces the signed native bundle
in 1.957s and normal shared-C Wasm build in 24.049s wall time. Strict signature,
compiled provenance and private feature/review exclusions pass. Receipts: ignored
`w12c3-primary-replacement-bundle.log`, `w12c3-primary-replacement-package.json`
and `w12c3-primary-replacement-web.log`. No user App was launched/closed; browser
runtime/Open/Save, durable history and broad regression remain separate gates.


W12c.3c.3b.4d.3 complete: Pivot/Chart Change data source admits primary or an
existing resolved Table through a complete graph/primary fence, bounded grammar
and dependency validation, optional event reservation and one history action.
The compact chooser preserves identity, placement and Marks; no-op keeps Redo,
stale drafts reject, and only the rebound result invalidates. Unchanged primary
metadata/data remain parent/root obligations rather than a changed event family.
The focused optimized/ASan/UBSan gate passes in 21.735s (262 binding checks per
configuration; 21 hidden native stages, GUI 10.319s), including actual source
switching, Undo/Redo, no-op cache retention, stale selection, independent Pages
and 150% v16 Save/Open. Fifty tool contracts pass in 2.342s. Receipts: ignored
`w12c3-object-binding*.log` and run summaries. Source/format composites, retained
App writing, durable Undo and dataset retention remain separate. Next:
W12c.3c.3b.4e object topology/placement admission.


Analytical binding packaging: source 3d4f467 produces the signed native bundle
in 6.965s and normal shared-C Wasm build in 24.434s. Strict signature, compiled
provenance and private feature/review exclusions pass. Ignored receipts:
`w12c3-object-binding-bundle.log`, `w12c3-object-binding-package.json` and
`w12c3-object-binding-web.log`. No user App was launched/closed; runtime browser
persistence and broad regression are separate.


W12c.3c.3b.4e.1 complete: deletion admits a complete independent after-graph,
including empty graphs, before optional DXDE reservation and one ownership/action
publication. Dependent Tables stay protected; deleted source metadata and object
ID high-water remain retained. Pressure, stale callbacks, invalid ownership and
read-only state leave the command unapplied. The focused optimized/ASan/UBSan
gate passes in 36.034s (355 deletion checks per configuration; 16 native stages/
32 checks, GUI 9.383s), covering real menu delete, cache preservation, Undo/Redo,
last-object removal and 150% v16 Save/Open. Fifty tool contracts pass in 2.332s.
Receipts: ignored `w12c3-object-delete*.log` and run summaries. Retained App event
IO, durable Undo and source/format composites remain separate; next is
W12c.3c.3b.4e.2 single-object placement admission.


Deletion packaging: source cb659d4 produces the signed native bundle in 7.083s
and normal shared-C Wasm build in 30.150s. Strict signature, compiled provenance
and private feature/review exclusions pass. Ignored receipts:
`w12c3-object-delete-bundle.log`, `w12c3-object-delete-package.json` and
`w12c3-object-delete-web.log`. No user App was launched/closed. Next-slice source
inspection confirms that even a single-object range move currently relocates
positional cell formats; placement admission must preserve that compound behavior.
Mixed/multi-object range admission remains W12c.4.


W12c.3c.3b.4e.2 complete: exact single-object moves admit geometry and existing
positional-format behavior together (DXDE effect4 or12, one action), preserving
identity/query/Marks/title/allocator and cached results. Full format descriptors
and source extent recheck after actual native retained-memory admission and
optional event reservation. No-op preserves Redo; occupied/manual/source/object
areas reject without partial adoption. Mixed/manual/multi-object ranges retain
their existing complete producer until W12c.4. The optimized/ASan/UBSan gate
passes in 18.985s (339 move checks per configuration; 16 hidden native stages/
37 checks, GUI 8.918s), including dialog Enter, collision/Cancel, positional
formats, Table/Chart moves, cached Pages, Undo/Redo/no-op and 150% v16 Save/Open.
The named range-move interaction passes ten stages in 14.277s (GUI 6.456s);
fifty tool contracts pass in 2.473s. Ignored receipts: `w12c3-object-move*.log`
and run summaries. Retained App event IO and durable Undo remain separate.
Next: W12c.3c.3b.4e.3 object creation admission, not started. The user requested
pausing after this slice and delivery of the cumulative build.


Placement delivery/pause: product source d64b3a0 produces the cumulative signed
native bundle at `build/Duxcel.app` in 2.016s and normal shared-C Wasm build in
25.195s. Strict signature, compiled provenance and private feature/review
exclusions pass; ignored `w12c3-object-move-package.json` records the binary hash.
No user App was launched/closed. User-requested pause begins after this verified
slice; W12c.3c.3b.4e.3 remains queued. Resume there, with origin/default-relation
completion and source-format composite boundaries still explicit.


## Active parallel roadmap execution

The user authorizes four GPT-6.1 medium-effort subagent lanes, superseding the
usual two-lane default for this run. Base: `55ad3f6`. The coordinator owns main,
active docs, shared App/build/catalog reconciliation and linear integration.
The current agent service permits root plus three running subagents; isolation
bootstrap hands off its slot to historical Save As after its first milestone.
Four worktrees are registered; do not confuse a ready checkout with a running
agent or a module result with integrated product acceptance.

| Lane / managed worktree | Dependency-ready scope | Reconciliation boundary |
| --- | --- | --- |
| `roadmap-isolation` | Shared operation lock, checkout identity, compiled provenance and TEST lane markers | Bootstrap first; distribute its verified infrastructure commit before lane builds |
| `roadmap-object-create` | W12c.3c.3b.4e.3 object creation admission | Complete graph/allocator/geometry, source-origin and manual-format constituent closure; root integrates App adapter |
| `roadmap-source-format` | Independent W12c.3c.3b.4f transaction prerequisites | Ledger/binding/format all-or-nothing ownership; final App adoption waits on creation/origin prerequisites |
| `roadmap-history-save-as` | W13 current-state-only historical export; W11 complete | Independent export identity, closure and Working Head preservation; W14 waits for W12 |

Every checkout keeps private `build/` fixtures/binaries/receipts. The user's
follow-up supersedes whole-command serialization: take the shared compilation
lease only while building, with eight compilation jobs, and release it before
independent private test execution. Keep same-checkout runner ownership to avoid
overwriting a tested executable. App tests use dedicated lane-tagged TEST
executables; subagents do not launch the normal `Duxcel.app`. Clipboard, native
file-panel and other actual shared desktop interactions retain their separate
native-service lock. Hidden software-injected GUI tests that use no shared OS
services may run independently. No user App is controlled or terminated.
The coordinator integrates verified commits at dependency milestones, runs only
the affected combined feature/interaction gates and records product completion
here. Unresolved dependencies stay explicit; no lane activates retained App
history before complete producer/publication coverage. Disposable scheduling
details live in ignored `build/locks/roadmap-lanes.json`. The isolation lane is
verifying the phase-aware runner before any new lane launches.



Parallel bootstrap integrated: original infrastructure commit `a7e530e` (main
`2bc8442`) plus `22f6b87` provides `tools/lane.py`, a private checkout marker,
shell-free operation execution, repo-wide blocking advisory ownership and
checkout/lane receipts. Registered Make asset links into the same checkout are
allowed; sibling mutable outputs/cache symlinks reject. Child cancellation keeps
the operation lease until command exit. Compiled About provenance records lane
and checkout without runtime Git; automated native Dock badges show TEST+lane.
Nineteen focused tool/workspace tests pass in 2.126s, including two actual Git
worktrees contending on the same lock; the isolation lane's registered About
module/sanitizer/hidden-native gate passes in 7.469s. Evidence is under private
`build/tests/results/`. All four named agents/worktrees are established; isolation
completed bootstrap and handed its active slot to W13, while object-create,
source-format and historical Save As author/run their scoped gates. No product
roadmap slice is marked complete by this infrastructure milestone.
