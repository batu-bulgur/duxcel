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

W02 follows accepted W01 and reuses its atomic move/history boundary. W03 region discovery is implemented; W05 is the current command/revision foundation. `worksheet_check_move` distinguishes a protected/invalid target from a manual replacement candidate. The explicitly authorized replacement path retains the default reject-on-conflict API for other callers. The generated manual-only `build/tests/generated/move-replace-demo.dxls` opens with B2:D4 selected, occupied H9/I9 and AA20001/AB20001 destinations, and an E5 self-overlap target. Replace must be demonstrated through either Option-drag or the existing Move selection address dialog; user acceptance remains pending.

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
| W08 · P0 · In progress | DuckDB journal writer for the admitted command subset; W05–W06. | Measure single/batched edits and large paste on the pinned engine; ordered pending/durable acknowledgement, idempotent retry and bounded queue. Kill before/after commit, disk-full/write failures yield old/new complete state. No SQLite dependency. RE01/RE03/PL02; commit part of A13. |
| W08a · P0 · Complete | Native DuckDB journal transaction/receipt primitive; W05. | Private checkpoint lineage, exact before/after byte spans and uint64 IDs; bounded atomic append/read; identical operation-ID retry; reject payload/revision conflicts; process kill before/after commit preserves complete old/new state. Engine component only, no App durability claim. |
| W08b · P0 · Complete | Bounded owned asynchronous journal queue and App admission; W08a/W06. | Single mutation owner reserves immutable records before applying supported edits; immediate pending and ordered durable feedback; retries retain operation IDs, full/failing queue never silently loses an edit. Explicit boundary for unadmitted legacy/lifecycle commands. Narrow real-App gate; no startup recovery claim. |
| W08b1 · P0 · Complete | Native bounded asynchronous writer queue; W08a. | Reserve owned bytes before mutation; publish only after success, cancel on rejection; pending/in-flight/reserved share a hard bound. Ordered post-COMMIT acknowledgement, same-ID retry with retained payloads, failed-open recovery and explicit drain/abort shutdown. Engine infrastructure, not App coverage. |
| W08b2 · P0 · Complete | Checkpoint-backed App admission and pending/durable feedback; W08b1. | Capture valid lineage, reserve before a supported edit, publish after its successful command, retain drafts on backpressure. Unsupported mutation coverage must be explicit; document replacement/Save/close cannot silently discard a pending tail. Narrow hidden real-App gate. |
| W08c · P0 · In progress | Fault/performance integration and batching policy; W08b. | Actual write/disk-full/kill/close failures, measured per-edit tail latency, large-paste/checkpoint throughput and bounded backpressure. Legacy paste remains outside the one-cell journal admission until a resolved range command is added. Preserve all-or-nothing state and retry after uncertain commit; finalize W08 acceptance before W09. |
| W08c1 · P0 · Complete | Filesystem failure/retry matrix; W08b. | Real write-limit errors and targeted ENOSPC/EIO/short-write/fsync/close faults preserve complete states and receipts. A failed first creation removes only its newly created incomplete journal; queue records survive repair/retry. |
| W08c2 · P0 · Next | Measured native admission/commit/checkpoint throughput; W08c1. | Pin runtime engine/library provenance; measure bootstrap, warm single edits, burst/backpressure and a finite 20,000-cell legacy paste followed by checkpoint. Set batching policy from evidence; no per-row journal claim for legacy paste. |
| W09 · P0 · Queued | Revision-fenced Save publication and proportional progress/cancel; W08. | Save R while R+1 arrives; report R only after its complete checkpoint is published, retain R+1 unsaved/recoverable. Failed/cancelled Save preserves old file and dirty state. UX05/RE03; A13/A15/A28. |
| W10 · P0 · Queued | Interrupted-session recovery candidate and explicit lineage choice; W08–W09. | Recover committed tail or open checkpoint, including unnamed workspaces; progress/cancel/failure preserves recovery material. New edits cannot silently discard a deferred tail. Persist generation/parent identity and baseline/journal linkage; two states sharing an admitted clock cannot be ordered by mtime alone. RE02; A14. |
| W11 · P1 · Queued | Latest Saved pointer and immutable named versions; W09. | Repeated Save updates one saved entry; unchanged tags share payload, older named states open directly. VE01–VE03; A11. Admit complete assets/results as their slices land. |
| W12 · P1 · Queued | Three-version-state retained Undo and reachability pruning; W10–W11. | With V10 saved, retain V8→V9→V10 plus unsaved suffix across reopen; old tags still restore; assets needed by retained states cannot prune. HI04; A12. |
| W13 · P1 · Queued | Historical preview Save As, initially current-state-only; W11. | Saving an inspected old state creates an independent artifact with its complete dependency closure; live workbook/recovery unchanged. VE05; first path of A27. |
| W14 · P2 · Queued | Selected-history export; W12–W13. | Export an explicit history interval with required assets and valid roots; omitted history is absent, new artifact restores every advertised state. VE05; remaining A27. |

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
ordinary checkpoint/UI edits defer this matrix to its named gate. W08c2 is next.

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
