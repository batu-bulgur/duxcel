# Workbook editing analytics and versioning TRD Draft

Status: Technical draft revised 21 October 2026 with accepted Refresh replacement, snapped-to-floating reference conversion and stale Save/calculation behavior, in addition to local pivot coordinates, positional references, spill, Master View and durability. Source baseline remains `b5e6be0e92b9a784027781b78343de51a783e5d3`; the first draft was committed as `4099673`. Sections 19–21 distinguish accepted decisions, implementation parameters and deferred features. [REQUIREMENTS](../REQUIREMENTS.md) summarizes the accepted direction. The accepted scope is now sequenced in [ROADMAP](../ROADMAP.md#workbook-trd-implementation-slices); [STATUS](../STATUS.md) records the implemented W01 manual-range subset and its verification limits. The baseline in section 2 remains the original source assessment, not a claim that all proposed capabilities are implemented.

Duxcel should combine freeform spreadsheet editing with relational analysis and spatial dashboard authoring in one workbook. Users should be able to edit manual cells and eligible imported records, arrange independent analytical objects over a sheet, reference their values, undo workbook changes, retain selected versions and recover committed work. The recommended foundation is a shared C document model and command boundary, with separate data identity, placement, durable history and disposable calculation caches.

The major change is the document contract. Preserve DuckDB as the persistence and analytical engine without adding SQLite now. Separate Connect, Import, materialized Data Model datasets and their views. Preserve worksheet ownership in formulas while allowing a Master View to composite sheets in z-order. A second UI, a separate analytics server and a new rendering engine are unnecessary.

## 1 Product outcomes and boundaries

The intended outcomes are:

1. A sparse editable worksheet can contain irregular manual content and several independently owned analyses, including tables, pivots and charts.
2. Manual ranges and imported datasets can feed analyses through explicit bindings. A future formula can reference worksheet cells and analytical values without relying on what is currently painted.
3. Users edit the workbook's imported/model data, definitions and presentation. External Parquet/CSV files remain unchanged; Connect is read-only and data exports are explicit.
4. Grid-resident objects have unambiguous, non-overlapping ownership on their sheet. Floating objects and a composite Master View can overlap visually without merging sheet values. Movement retains the actual grabbed point and can optionally align to sheet geometry.
5. One coherent workbook history includes cross-sheet and sheet lifecycle changes. Selected versions remain directly restorable independently of retained fine-grained Undo.
6. Committed work survives an interrupted session, while Save produces a coherent named artifact with fast reopening. Recovery can be deferred without losing the recoverable continuation.
7. UI actions and authorized automation use the same document operations. Core execution can eventually run without a window.
8. Save retains complete pivot/aggregate outputs and chart state. Drill-through depends separately on the availability of the backing external or embedded dataset.

Preserve C99, raylib, DuckDB, exact typed values, explicit ownership and the shared C/Wasm UI. Objective-C remains inside the macOS adapter. Browser storage/automation adapters must not become another spreadsheet implementation. Product automation is distinct from development-only review infrastructure.

General Excel formula compatibility, arbitrary SQL write-back, collaborative editing, cloud sync, VBA/COM compatibility, a complete chart catalog and a complete dashboard layout system are separate capabilities. This design must leave room for them without making them acceptance gates for basic selection, recovery or raw-record editing.

## 2 Verified baseline and architectural fit

This baseline comes from the current source and the four documentation entry points. It establishes source behavior, not new native visual or physical-input acceptance.

| Area | Current implementation | Fit and required change |
| --- | --- | --- |
| Manual cells | `SheetCell` stores `(row, col, text)` in a sorted sparse array. `cells_set` removes an empty input. Limits are 20,000 cells and 4 MiB of text per sheet. | Keep sparse storage; introduce typed values and explicit presence before promising empty-text/NULL editing or formulas. No dense sheet allocation. |
| Selection | Active cell, anchor and extent are separate. `select_region` supplies inferred manual, ready object/source and used-area candidates to the existing selection ladder. | Extend candidate/occupancy semantics. Repeated Select All already exists; it needs refinement, not replacement. |
| Selection drawing | `grid.c` draws an active-cell border and the enclosing range border. | The brief's duplicate-border complaint has a concrete source path. Change visible treatment while retaining the active cell internally. |
| Sheet objects | `SheetObject` has a sheet-local ID, integer row/column placement, row/column extent, source binding and representation definitions. | Reuse object identity/definitions. Add placement modes, z-order, local metrics and explicit hit-test ownership. |
| Occupancy | `objects_fits`, manual-edit guards, source-overlap checks and Save validation reject overlapping content. `objects_hit` returns the first matching array item. | Layering changes a model invariant, not just draw order. Every dependent guard, selector, reader, writer and test needs a migration policy. |
| Geometry | Sheet columns have widths; `Grid` has one row-height scalar. Existing chart resizing changes grid spans within bounds. | Per-row sheet heights, object-local matrix metrics, free placement and true independent scale are new model/codec work. |
| Analytics | Real DuckDB definitions, filters/sorts, Tables/PIVOT/charts and bounded typed Pages. Manual cells are not a general analytical relation or formula runtime. | Add explicit range-to-relation adapters and a common effective dataset for edited sources. Retain current SQL generation. |
| Source formats | Native supported primary/Table paths bind styles to verified source versions and record/field identities. Unsupported scopes suspend. | Build on [source-format identity](SOURCE_FORMAT_IDENTITY.md); styles do not establish universal editable lineage. |
| Undo | `Worksheet` coordinates separate bounded Analysis/Cell/Object/Format snapshot stacks using `EditKind`. App Undo operates on the active sheet. | Preserve existing composite atomicity while migrating to workbook transactions. Sheet identity cannot remain an array index. |
| Save | `.dxls` is a versioned DuckDB database. `workbook_snapshot` copies definitions/presentation/ledgers and excludes Undo and Pages. External sources remain references. | Durable journal, user checkpoints, embedded assets and result caches are new persistence families. Format version 14 is not user version 14. |
| Jobs | Native `QueryWorker` owns its DuckDB connection; jobs are latest-wins with stale-reply guards. Browser uses the official asynchronous Worker. | Keep latest-wins reads. Ordered durable mutations cannot be discarded when a newer viewport request arrives. |
| Commands | `DxCommand` maps native/menu intents into App functions; many changes still call stores and history directly. | Introduce a typed document API behind those entry points. An input enum is not yet a durable command protocol. |
| Recovery and automation | No complete production crash journal, restored Undo, user-tagged checkpoints or live-instance semantic automation API. Review has its own excluded queue/registry. | New product boundaries; do not expose review internals or claim their tests as product acceptance. |
| Browser | Shared C UI and substantial metadata components exist; workbook Open/Save remains unavailable. | Extend the shared schema incrementally and preserve existing codec work. Native completion does not establish browser persistence. |

Source map:

- [Workbook model and history](../../src/workbook/workbook.h), [native writer and loader](../../src/workbook/workbook.c), [shared codec](../../src/workbook/workbook_codec.c).
- [Sparse cells](../../src/workbook/cells.c), [object model](../../src/workbook/objects.h), [object validation and movement](../../src/workbook/objects.c), [formats](../../src/workbook/format.h), [source versions](../../src/workbook/source_scope.h).
- [Grid model and transforms](../../src/grid/grid.h), [input and selection drawing](../../src/grid/grid.c), [selection ladder](../../src/grid/selection_ladder.c).
- [App controllers](../../src/app/app.c): `manual_range`, `commit_cell`, `history`, `select_region`, `move_selection`, `capture_sheet`, `workspace_job`, `request_document_action`.
- [Worker ownership and jobs](../../src/data/worker.h), [database execution](../../src/data/database.c), [SQL definitions](../../src/transforms/analysis.h), [platform commands](../../src/platform/platform.h).

## 3 Domain vocabulary and identity

Use **worksheet cells** for manually authored content anywhere on the sparse sheet. Use **inferred region** for a transient rectangle discovered from occupancy. Use **range binding** for an explicitly retained rectangle or named range. Use **dataset** for a typed relation, **table view** for its tabular presentation and **analysis object** for an independently identified analytical definition/presentation.

An inferred region is a navigation convenience. It is not automatically a persisted object, a dataset schema or the owner of the cells it happens to contain. Tetris-like content cannot always be partitioned unambiguously; allow an explicit range binding to resolve ambiguity.

| Entity | Authority and identity | Contract |
| --- | --- | --- |
| Workbook | Durable document UUID, monotonic revision and ID allocation high-water marks | Owns sheets, datasets, definitions, transactions and checkpoint references. File path is a locator. |
| Worksheet | Stable sheet ID; separate code name, display name and order | Owns sparse manual cells, sheet metrics, range bindings, placements and view state. Rename/reorder preserves references. |
| Worksheet cell | Sheet ID plus logical row/column address | Stores optional typed value or expression. Structural operations rewrite references under an explicit rule. |
| Range binding | Stable binding ID plus sheet/address definition | Declares analytical input, headers, schema and expansion policy. Plain selection does not create one. |
| Dataset | Stable dataset ID, immutable base version and schema/field IDs | Represents a range relation, read-only connection, imported source or materialized Data Model query result. Dataset identity is distinct from its views. |
| Record | Dataset/base identity plus stable record ID | Never a displayed row index. Inserted records use durable, non-reused IDs. |
| Analysis definition | Stable analysis ID, code/display names and definition/data revisions | Binds sources, transforms, shelves, measures and representation. A matrix also has its own positional A1 address space. |
| Placement | Placement ID, sheet ID, referenced analysis/view ID and geometry | Owns location, size, anchor policy, layer/order and optional local scale. Initially one placement per existing object. |
| Master View | Workbook-owned composition ID plus ordered sheet IDs and display transforms | Composites existing sheets visually. It owns no copied cells or independent calculation plane. |
| Result value | Analysis ID plus result generation and local positional address | Resolves offscreen values independently of placement. Group/measure semantic references are deferred FT01, not required now. |
| Transaction | Transaction ID, parent state, versioned resolved delta and effect set | One atomic user operation can touch several sheets/stores. |
| Checkpoint | Immutable state manifest plus retained asset references | Opens without replay from workbook creation. Physical blobs may be shared. |
| Version tag | User label pointing to a checkpoint | Immutable target until an explicit retag/delete operation. Separate from Latest Saved. |

These are logical contracts, not a requirement for C classes or one heap object per cell. Existing structures should evolve incrementally. A separate definition/placement entity becomes necessary when one analysis has multiple presentations; the first migration can keep a one-to-one representation.

Source-version counters and object ID high-water marks already protect identity allocation outside Undo. Extend that principle to workbook/sheet/dataset/field IDs: Undo may restore an old binding but cannot recycle an issued identity. A durable UUID identifies a copied document lineage only under the Save As policy in section 14.

## 4 Logical layers and references

```mermaid
flowchart TD
    UI["Grid, ribbon, object inventory, automation"] --> API["Shared C command and read API"]
    API --> TX["Validate and prepare workbook transaction"]
    TX --> Durable["Ordered durable journal"]
    Durable --> Model["Publish one workbook revision"]
    Model --> Cells["Typed worksheet cells and range bindings"]
    Model --> Datasets["Dataset bases, typed overlays and transforms"]
    Cells --> Relations["Revision-bound input relations"]
    Datasets --> Relations
    Relations --> Analysis["DuckDB analytics and future formula dependencies"]
    Analysis --> Results["Typed results, complete analytical snapshots and bounded runtime Pages"]
    Model --> Layout["Sheet metrics, placements and object-local grids"]
    Results --> Renderer["Shared C renderer and explicit hit testing"]
    Layout --> Renderer
```

Logical separation should allow the same analysis to appear on a sheet and be revealed through an inventory/inspector. That inventory selects/reveals the existing object; it must not create another query authority or force the current bottom-pane design. A later dashboard can reuse definitions with its own placements.

### 4.1 Sheet ownership and references that follow movement

**Accepted direction D02:** ordinary worksheet addresses can refer directly to grid-resident pivot/table output. Binding prioritizes the analytical object's cell ownership within its successful footprint. A formula referring to that output follows the object when it moves, including references from other sheets. It must not fall back to the manual cell newly exposed at the old location.

For example, grid output at `analysis.B7` is referenced as `=analysis.B7*2`. Moving its pivot five columns right updates the sheet-address expression to `=analysis.G7*2` and preserves the same object-local target. The move, dependent reference rewrites and placement changes are one workbook transaction and one Undo action. Absolute references such as `$B$7` prevent copy-relative adjustment; they do not freeze a reference against movement of its target. Whole-object moves do not require recomputing the pivot solely because its screen position changed. Dot-qualified names are the preferred Duxcel syntax; this example is not a claim of Excel formula-text compatibility.

Implement references as parsed bindings to stable sheet/object identities and local targets, retaining relative/absolute flags and a rendered A1 representation. Do not search/replace formula strings. A sheet rename/reorder preserves its identity. Moving a manual range uses the analogous reference-update contract. Copying a range remains a separate operation with its own relative-reference rules.

Grid-resident analytical footprints cannot overlap other grid-resident analytical footprints on the same sheet. The working interpretation also blocks covering populated manual cells. Floating geometry and Master View composition do not claim sheet A1 addresses merely by covering pixels. A floating pivot has its own local A1 space, independently of the sheet coordinate under its upper-left corner.

**Accepted Q02:** references are positional now. Local `A1` means that position in the object's output; sorting can change its value and removing that position produces an explicit reference error. A reference does not follow a category/measure after regrouping. GETPIVOTDATA-style semantic references are an unscheduled nice-to-have in FT01, not an open gate for movement or formulas. Internal object IDs preserve the referenced object through a move; they do not introduce semantic group lookup.

#### 4.1.1 Object local coordinates and names

**Accepted Q01:** a pivot/matrix has its own A/B/C columns and 1/2/3 rows in both floating and grid-aligned presentation. If its upper-left lies over sheet D10, its local A1 remains A1. A qualified reference can be `sales.regional_pivot.A1`, where `sales` is the sheet's code name and `regional_pivot` the object's code name. A sheet reference such as `sales.D10` is a different reference form. Reference insertion follows the active sheet versus object authoring context and must show that context in the address/formula strip.

Keep three naming concepts separate: immutable internal IDs, readable code names for formulas/automation, and free-form display names for dashboard titles. A display name such as Regional Sales Pivot may contain spaces and localized text without introducing quotes into ordinary expressions. Propose ASCII identifiers of the form `[A-Za-z_][A-Za-z0-9_]*`, with sheet code names unique per workbook and object code names unique in their sheet. Validate reserved/ambiguous identifiers; generate safe unique defaults. Display-name changes do not rename code names. An explicit code-name rename rewrites serialized references through stable bindings atomically rather than breaking them.

Object-local references remain stable under move, zoom, local resizing and z-order. Grid sheet-address references continue to follow successful object relocation while it participates in the sheet value plane. **Accepted R01:** when a snapped pivot becomes floating, convert existing analytical sheet bindings to the qualified object-local form in the same undoable mode-change transaction. Keep already-qualified local references unchanged; re-snapping does not silently convert them back. Undo restores both the previous mode and expression/binding form. Never let an existing analytical reference start reading the sheet cells exposed underneath.

#### 4.1.2 Object focused coordinate chrome

When an addressable matrix is focused, use its local geometry to project column letters into the existing top header band and row numbers into the existing left gutter. Draw these labels only across the visible projected span of that object, starting from its local coordinates, regardless of the underlying sheet's D10 position or unrelated widths/heights. De-emphasize or suppress the ordinary sheet labels outside that span so two coordinate systems cannot both look active. Avoid creating a full-sheet local grid outside the object.

The address strip identifies the sheet/object code context; the visible title uses the display name. Returning focus to the worksheet restores its coordinate chrome without changing its cell geometry. Use the same object-local transforms for headers, hit tests, selection and formula-reference insertion. Frozen planes, offscreen portions and clipping must not duplicate or mislabel local indices. Chart frames are not assigned fictitious A1 cells; this contract applies to addressable table/matrix output.

The exact placement of the first addressable local cell relative to pivot headers/totals needs one canonical implementation rule. Proposed default: include addressable matrix headers/totals, exclude the object title/frame, and never renumber the local grid based on viewport clipping. This is a layout/address-origin detail, not a request to build semantic references.

### 4.2 Master View composition

**Accepted direction:** sheets can be displayed together through a Master View in a user-defined sheet z-order. A manual formula on one sheet can reference a pivot on another, while their rendered content overlaps in the composite. The highest sheet/object in the display stack contributes its own formatting and visible content. Its z-order never changes the ownership of either referenced value.

Master View is a composition of existing sheet IDs, metrics and object transforms, not merged cell storage and not a new formula sheet. Resolve a click to an explicit sheet/object owner before inserting a reference. There is no unqualified master `B7` that changes meaning when the sheet stack is reordered. **Accepted Q04:** start with an inspection-first composite and explicit active owner, retaining each sheet's geometry. Direct cell editing through the composite is a later capability.

Preserve each sheet's column widths, row heights and independent alignment unless an explicit layout command changes them. A composite can share a viewport origin/zoom or apply display transforms, but rendering itself must not rewrite source sheet geometry. Use transparent absent/unfilled areas initially; explicit cell fills can cover underlying display. Grid alignment is a viewing option or explicit layout command. Drag/click promotes floating focus; explicit ordering commands persist authored order without making every focus click an Undo action.

Copy/statistics/read APIs name the selected sheet/object owner or an explicit mixed scope. A rendered composite is suitable for image export but is not automatically a typed rectangular dataset. Editing an aggregate does not imply write-back to its underlying rows.

### 4.3 Typed values and manual analytical input

Keep raw input/expression, typed logical value and formatted display separate. A sparse missing coordinate has no entry and reads with the established NULL semantics. Explicit SQL NULL, present zero-length text and text `NULL` retain distinguishable value/presence metadata; no synthetic blank storage category is introduced. Integers and DECIMAL retain exact canonical representations rather than round-tripping through `double`.

A range-to-relation adapter needs explicit rules for headers, duplicate/empty names, type declarations/inference, missing positions and formula errors. Keep generated field IDs stable across header renames. A refresh that changes schema must invalidate or repair consumers; it must not rebind by similar display names. Define whether a binding grows on append and how insert/delete transforms its boundaries.

SQL and future spreadsheet formulas are different evaluators with shared types and dependency revisions. Manual data may be exposed as a managed relation through batched updates; each keystroke need not issue SQL. A pivot over a bound range updates after the cell transaction commits. A manual formula reading that pivot can form a cycle: detect and report it before publishing a mixed-generation result. Do not promise a general formula engine as a side effect of adding bindings.

Row alignment has three meanings: visual alignment, positional reference and relational key alignment. Equal screen rows do not establish a join. UI alignment must not silently create a data relationship.

## 5 Functional requirement inventory

The IDs below are discussion and future acceptance identifiers. **Brief** denotes an expressed product outcome; **Derived** denotes an engineering requirement needed to deliver it safely; **Future** denotes a direction whose complete scope is still open. Proposed semantics are detailed in subsequent sections.

| ID | Origin | Required behavior | Principal dependency |
| --- | --- | --- | --- |
| WM01 | Brief | Support sparse manual content and independent analytical objects on the same sheet. | Preserve existing stores; explicit ownership |
| WM02 | Brief | Expand Select All through deterministic containing regions to the sheet, without turning inferred regions into permanent tables. | Occupancy contract and existing selection ladder |
| WM03 | Brief | Bind an explicit manual range as analytical input and update its consumers. | Typed cells and range-to-relation adapter |
| WM04 | Brief | Reference worksheet and grid-resident analytical cells through A1, including offscreen values, and rewrite dependent references atomically when their target moves. | Parsed stable bindings and formula dependency runtime |
| WM05 | Brief | List/reveal/edit analyses independently of their location on the sheet. | Stable identities and object inventory |
| WM06 | Brief | Retain object-local formatting and representation state across movement and version restore. | Owner-scoped styles and codec |
| WM07 | Brief | Address a floating or aligned matrix through sheet/object-local A1 coordinates independently of its screen position. | Parsed reference kind and stable owner/local position |
| WM08 | Brief | Separate formula/automation code names from user-facing sheet/object titles. | Unique validated identifiers, immutable IDs and atomic rename |
| DE01 | Brief | Edit workbook-owned imported/model records and fields; keep connected datasets read-only and external files unchanged. | Connect/Import/Data Model modes and effective relation |
| DE02 | Brief | Inspect reproducible transformations and resolved record edits as meaningful steps and SQL where applicable. | Typed operations, bound parameters and read-only SQL presentation |
| DE03 | Derived | Apply edits consistently to viewport, filters, sort, pivot, chart, statistics, copy and export. | One effective relation and revision fence |
| DE04 | Derived | Keep duplicate physical records, explicit NULL patches and inserted records distinct. | Stable record IDs and patch-presence metadata |
| DE05 | Brief | Reorder displayed fields/rows deliberately and preserve formats/references. | Distinguish sort, display order and schema operations |
| DE06 | Brief | Connect to external Parquet/CSV without embedding the whole query result; retain definition/history/styles and limited browse preview. | External binding, provenance and offline status |
| DE07 | Brief | Import a source or materialize a selected query into a workbook Data Model without requiring a visible sheet table. | Durable internal dataset and refresh recipe |
| DE08 | Brief | Drill through a pivot cell into a new query-backed sheet when its backing dataset is available. | Complete result identity, group/filter predicate and dataset provenance |
| DE09 | Brief | Refresh atomically replaces affected data and resets its ad hoc row/cell formats and local value overrides, retaining compatible field formats and conditional rules. | Explicit refresh scope, candidate publication and ordinary Undo/version policy; no row matching |
| HI01 | Brief | Undo/redo document changes in workbook chronology, including cross-sheet operations. | Workbook transaction history |
| HI02 | Brief | Undo sheet deletion and restore its cells, objects, bindings and references. | Stable sheet IDs and retained deletion payload |
| HI03 | Brief | Replay/redo independently of later application preference changes. | Resolved commands and versioned semantics |
| HI04 | Brief | Retain cross-session Undo over the latest three version states including Latest Saved, plus the current unsaved suffix. | Durable history, pruning and checkpoint reachability |
| VE01 | Brief | Create manually named versions with direct state restoration. | Immutable checkpoints and retained assets |
| VE02 | Brief | Save repeatedly into one Latest Saved entry; share identical state with manual tags. | Mutable saved pointer and checkpoint deduplication |
| VE03 | Brief | Restore a complete version without replaying all earlier operations. | Complete state manifest and required assets |
| VE04 | Brief | Save complete pivot/aggregate output and chart state, including offscreen output; keep large browse-query results optional unless materialized into the Data Model. | Complete analytical snapshots distinct from runtime Page caches |
| VE05 | Brief | Save As the inspected historical state by default, with an explicit option to include a selected history subset. | Independent new artifact and retained dependency closure |
| VE06 | Brief | Warn on an analytical object when its data/query/definition is newer than its displayed complete result. | Independent desired/computed/displayed generations and saved provenance |
| RE01 | Brief | Journal committed document operations and detect an unfinished session. | Ordered durable writer and session generation |
| RE02 | Brief | Offer fast checkpoint open or latest committed-state recovery after an unsaved/interrupted session. | Candidate replay, progress/cancel and preserved journal |
| RE03 | Derived | Handle failed commits, partial Save, disk exhaustion and ambiguous acknowledgements without silent loss or duplicate edits. | Failure protocol and idempotency |
| AU01 | Brief | Route UI and automation through the same semantic operations. | Shared C command boundary |
| AU02 | Brief | Query/manipulate an authorized running instance through a platform transport. | Protocol, capability scope and instance routing |
| AU03 | Future | Run document/analysis operations without a window and optionally render separate previews. | Headless core with optional rendering host |
| LA01 | Brief | Prevent same-sheet grid analytical collisions, allow floating visual overlap and composite sheets through Master View z-order. | Separate occupancy, floating geometry and sheet composition |
| LA02 | Brief | Move and scale analytical objects freely using their local coordinate frame. | Free placement and transform/hit-test model |
| LA03 | Brief | Fit a matrix's local rows/columns to the existing sheet grid. | Explicit grid-alignment mode |
| LA04 | Brief | Fit sheet row/column metrics to a matrix's displayed local metrics in one undoable operation. | Per-row metrics, impact preview and composite transaction |
| LA05 | Brief | Fit a chart to a chosen sheet rectangle without altering sheet metrics. | Geometric bounds and explicit target range |
| LA06 | Brief | Show a cell/anchor spill error for blocked analytical placement and recover visualization after moving to a valid region. | Placement state separate from computed result |
| LA07 | Brief | Bring a dragged or explicitly clicked floating object forward and keep covered objects reachable through unobscured controls. | Floating z-order, overlay handles and object inventory |
| UX01 | Brief | Show one outer selected-range boundary without a second equally active cell border. | Selection paint contract |
| UX02 | Brief | Move a selected range/object from its border or by Option/Alt-dragging its interior. | Focus/gesture arbitration and platform bindings |
| UX03 | Brief | Preserve the grabbed point/cell while dragging, with a truthful destination preview. | Captured local grab offset and coordinate transforms |
| UX04 | Brief | Detect offscreen destination conflicts before destructive cell moves and expose an actionable warning. | Full-model destination validation |
| UX05 | Brief | Keep Save progress and close/unsaved dialogs proportional to the application's existing UI. | Compact content-driven modal sizing and narrow-window checks |
| UX06 | Brief | On matrix focus, show its local row/column labels in the existing chrome over its visible span and de-emphasize the worksheet context. | Shared local transforms, clipping and focus restoration |
| PL01 | Derived | Preserve existing native files and shared-C browser codec progress during schema changes. | Explicit migrations and platform acceptance |
| PL02 | Derived | Keep work bounded and responsive on the current 8 GB Mac. | Measured memory, latency, checkpoint and retention budgets |

## 6 Command and transaction contract

### 6.1 Distinguish operations from UI events

All controllable actions should have semantic entry points. Only document mutations belong in document Undo and the recovery journal. Pointer samples, hover, selection, pan, focus and pane searches remain transient actions. View restoration can persist a coalesced bookmark independently. Copy is a read/clipboard effect; Paste journals the resolved pasted values, not a future read of the system clipboard.

Global settings remain outside workbook history. **Accepted D08:** workbook display defaults such as locale/date-time display are persisted preferences outside cell/data Undo. A format stores either an explicit override or an Automatic binding to the current workbook default. Undo/Redo restores that binding/override, not a previously rendered string; Automatic display can therefore reflect a newer workbook locale without changing stored values or calculations. Explicit formatting remains independent of that default.

Durably record preference changes separately from the user Undo stack, with a presentation generation and saved presentation generation so changing defaults still makes Save necessary. Version checkpoints capture their defaults for historical display; ordinary cell Undo/Redo does not rewind them. Typed input is resolved at entry, so replay never reparses an old date using a new locale. Calculation timezone, rounding and other future semantic policies cannot be smuggled into a visualization-only setting: any value-changing policy needs a separate explicit command/design.

Per-cell formats and row/column metrics remain undoable document mutations. Selection, window size and personal ribbon density remain session/application presentation. A Save snapshot captures the content revision and presentation generation coherently.

Separate the command requested by an actor from the delta accepted by the document. For example, Paste from current selection becomes a concrete set of cells; Delete filtered rows becomes a frozen record-ID set; Snap becomes exact target geometry and sheet-metric changes. Redo applies the resolved delta without repeating external reads or UI gestures.

Suggested command families are `SetCells`, `ClearCells`, `PasteValues`, `MoveRange`, `InsertSheetRows`, `DeleteSheetColumns`, `CreateSheet`, `DeleteSheet`, `RenameSheet`, `ReorderSheets`, `BindRange`, `ConnectSource`, `ImportSource`, `MaterializeQuery`, `RefreshDataset`, `EditDatasetCells`, `DeleteRecords`, `AddField`, `ChangeTransform`, `SetFormats`, `MoveObject`, `ResizeObject`, `SetObjectLayer`, `SetMasterView`, `SnapObject`, `Undo`, `Redo`, `SaveCheckpoint` and `CreateVersionTag`. A separate non-Undo presentation-preference path uses the same durability discipline. Names are illustrative API vocabulary, not an agreed ABI.

Each mutation envelope carries a protocol/schema version, document ID, request/idempotency ID, expected revision and explicit target identities. Its resolved record includes parent/new revision, before/after values or retained blob references, affected stores/dependencies, and command semantic version. Bulk payloads are bounded blobs, not millions of independent transactions. Counters and IDs must fail on overflow rather than wrap.

### 6.2 Commit and publication order

```mermaid
sequenceDiagram
    participant A as UI or automation
    participant C as Document coordinator
    participant J as Durable writer
    participant Q as Calculation worker
    A->>C: Command with expected revision and request ID
    C->>C: Resolve, validate, reserve and prepare owned delta
    C->>J: Append complete transaction and referenced durable payload
    J-->>C: Durable commit acknowledgement
    C->>C: Adopt prepared state and publish new revision
    C-->>A: Committed revision and calculation state
    C->>Q: Recompute invalidated results for that revision
    Q-->>C: Owned revision-bound result or error
    C->>C: Publish only if document and dependencies still match
```

**Accepted Q05:** give prompt UI feedback, retain pending versus durable acknowledgement, and make successful Save flush/publish every included action. Choose immediate versus short bounded group commit from DuckDB measurements; the exact interval is an implementation acceptance parameter, not another unresolved engine choice. An optimistic UI preview or buffered action must not claim durability. Allocate/validate before durable commit so normal adoption cannot fail halfway. If a committed transaction cannot be adopted because of an exceptional runtime failure, stop further mutations and reload/recover it; do not continue from a divergent RAM model.

Serialized document mutations establish a total order even when an agent and a user act together. Preconditions reject stale writes rather than editing a target derived from old selection state. Check completed request IDs before revalidating a retry's now-stale expected revision; return the original outcome for the same payload and reject reuse with different payloads. Persist enough deduplication information to cover the promised retry window. Cancellation before commit discards preparation; after commit it cannot erase the accepted operation.

Definition commit and calculation completion are separate statuses. A source edit can be durable while its pivot says Updating. Queries used for validation may run before commit; errors must preserve the old definition when the command promises validated application. Later resource/source errors retain the accepted document and a labelled stale/error result, rather than rewriting history. Automation receives a ticket/revision and can wait for the required analytical result.

### 6.3 Replay and Undo semantics

Replay operates on document data, never by re-enacting keystrokes or button presses. It must not reopen dialogs, import a changed file, send a clipboard operation or execute an untrusted saved SQL string as an arbitrary command.

Resolved deltas make redo deterministic with respect to edited document state. Idempotency is a separate guarantee provided by request deduplication; a command such as Add Sheet is not inherently idempotent. Recomputing analytical results additionally requires pinned inputs and compatible semantics. Capture or materialize outcomes of nondeterministic/external operations when exact replay is promised. Do not assume unchanged SQL text produces identical values across time or engine upgrades.

## 7 Connections imports Data Model and transformations

### 7.1 Source modes and view ownership

**Accepted D01/D03:** external source files are read-only inputs. Editing takes place in the workbook, never by overwriting the original Parquet/CSV through Save. The user can begin in a blank workspace without a Save dialog or mandatory technical name such as Untitled DXLS. First Save requests a `.dxls` destination; recovery can protect the workspace before it has a pathname.

| Operation | Retained data | Behavior and external dependency |
| --- | --- | --- |
| Connect to Parquet/CSV | Connection/reader/query definitions, transformation history, style scopes, source proofs and a limited browse preview; complete dependent analytical outputs as specified in section 9. | Query-backed read-only dataset. Cell edits require conversion/materialization. Refresh and drill-through require the external source. |
| Import Parquet/CSV | Workbook-owned source content plus its reader/query definition, metadata and record identity. Physical storage can be an embedded source asset or an equivalent typed internal base. | Creates an internally queryable/editable base; the original path is optional refresh provenance. Save includes the owned data. |
| Add query result to Data Model | Complete typed result of a selected query, schema, assigned internal record IDs and the query/connection used to refresh it. Does not require retaining the entire upstream file. | An independent internal dataset that can feed views/analyses without a visible sheet table. External access is needed only for a requested refresh. |
| Create a table view | Definition, formatting, placement and binding to a connected or internal dataset. | Presentation does not itself decide whether the backing data is embedded. A large connected table view need not be fully saved. |
| Create pivot/chart | Definition, formatting/layout and complete analytical output tied to the backing dataset version. | Display can survive unavailable input. Drill-through availability follows the backing dataset, not the existence of the visible pivot. |

Import and Add to Data Model both establish workbook-owned data, but capture different boundaries: a source base versus a selected query result. Neither requires displaying it as millions of worksheet cells. Keeping the external connection as refresh provenance does not make ordinary internal queries depend on that connection.

An accepted flow is external Parquet → query Q1 → materialize Q1 into Data Model M1 → internal filtered view Q2 over M1 → pivot P1. Save retains M1, Q1/Q2, P1's complete output and all relevant metadata. Reopen can display P1 and query M1 without the external file. Refresh of M1 reevaluates Q1 externally into a candidate new version; it must not silently redirect Q2 to the external file.

Persist the logical dataset/query binding separately from the external refresh recipe and the session's executable file/table name. Resolve imported/model reads to their owned internal version; resolve Connect reads to its verified external binding. Native paths and browser registration aliases are runtime locators, not replacement dataset identities. This requires evolving the current `Analysis.path`/Table-source ownership rather than merely changing a filename in generated SQL.

Connected query text, filters, sorts and formatting changes remain workbook operations with history. Read-only means no direct cell/record mutation on that external relation; it does not prevent authoring SQL/transform definitions. Export Parquet/CSV writes the selected current relation to an explicit destination, omits workbook formatting/history and leaves the input file unchanged. Replacing an existing export target is a separate explicit file operation.

### 7.2 Workbook-owned effective relation

Use an immutable source base plus typed overrides, inserted records and delete tombstones as the first sparse-edit model:

`effective dataset = base with present field patches, excluding deleted record IDs, union inserted records`

Apply this relation before downstream filters, sort and aggregation. A changed value may enter or leave a filter. Every consumer must read the same dataset revision, including full-range statistics and export. Modifying only a loaded `Page` would produce conflicting answers.

For a sorted display row representing source record 8,431, target record 8,431 and its field ID. Row 7 in a Page is not the edit address. Duplicate rows remain independent records. Distinguish no patch, a present NULL patch and a present empty-string patch; `COALESCE(patch, base)` cannot express this correctly.

Begin editable data with imported bases or materialized Data Model results carrying internal record/field IDs. Native raw-source provenance helps import verification and connected formatting but is not general SQL lineage. DISTINCT, aggregates, joins and arbitrary custom SQL views remain read-only as views until a specific inverse/write-back rule exists. Materializing any supported query as a new internal dataset establishes a new editable boundary; it does not infer how edits should modify its external source.

### 7.3 Schema and order

Add Field is a schema operation with field ID, type, default/expression and backfill policy. Editing a cell enforces or explicitly changes that type. A computed field's formula, a constant column and a patched physical field have different semantics. Field rename/reorder must preserve bindings; drop/type change must invalidate dependents predictably and retain what Undo needs.

Distinguish source transforms from sheet structure. Delete Records removes selected dataset rows, whereas Delete Sheet Rows moves worksheet content and placements. Display-column reorder is different from dropping/adding relation columns. A manual row order needs explicit durable order keys; relational rows do not acquire a permanent user order from their current viewport. Sort ties need a deterministic tiebreaker. DuckDB documents order changes through joins/grouping and potentially unstable sorting; do not promote query position into identity. [DuckDB order preservation](https://duckdb.org/docs/current/sql/dialect/order_preservation).

The current `Analysis` is a structured definition with session history snapshots, not a general DDL/DML event log. Keep a user-facing transformation history separate from the durable document journal. Format/move commands belong in document history but not the SQL transformation chain. SQL is useful inspectable output from typed operations; raw SQL strings alone omit before-images, affected IDs, parameters and nonrelational state.

### 7.4 Compaction and source retention

A sparse overlay is not assumed to remain efficient for millions of changes. Benchmark overlay joins against managed DuckDB tables and compacted Parquet bases. Compaction creates a new immutable base/manifest, preserving record/field IDs or recording an explicit migration. Retained tags/Undo pin old bases. Never compact by overwriting a base still referenced by a version.

Connect minimizes source duplication but cannot promise arbitrary new queries after external bytes disappear. Import/Data Model snapshots retain their selected data boundary, so historical reads do not depend on the original file. A content hash proves identity; it does not retain data. Missing/changed connected inputs suspend dependent new queries or require explicit relinking; complete saved analytical outputs remain readable at their saved generation.

### 7.5 Refresh replacement and ad hoc state

**Accepted R02:** Refresh replaces the affected dataset/view's returned data and discards its ad hoc row/cell formatting and local data overrides. Do not search for old rows in the new result, prove deletion, compare full-row values, match duplicates or retain unresolved styles for later automatic reattachment. The reset is part of an explicit successful Refresh, even if a costly comparison might have concluded that some rows were unchanged.

| State in the refreshed scope | Successful Refresh behavior |
| --- | --- |
| Query/reader/transform definitions | Retain and execute the current definition; these are reproducible workbook logic, not ad hoc value overrides. |
| Compatible field/column formatting | Retain through its valid field binding; a removed/incompatible field requires the existing missing-field/repair state. |
| Conditional formatting rule | Retain the rule and reevaluate it against new values. If record_key is still 482, the condition may match regardless of changes in other fields. |
| Direct manual row/cell highlight | Discard from the refreshed current state. It belongs to the previous view state and does not follow an inferred business record. |
| Ad hoc edited cell values, inserted records or delete overrides | For editable imported/model data, replace/reset them in the refreshed scope rather than merging/reapplying them to the new source result. Connect remains a read-only relation. |
| Unrelated manual worksheet cells, objects and other datasets | Leave unchanged. Scope refresh explicitly; it is not a clear-sheet operation. |

A condition matching record_key equals 482 applies to every current row that satisfies it, even when another field changes. If there is no match, no row is formatted; duplicates can all match. This is rule evaluation, not unique-record reconciliation. There is no requirement to invent a key for direct highlights or ad hoc edits.

Build the refreshed data as a separate candidate and publish the new values plus reset ad hoc state atomically only on success. Failure/cancellation preserves the old accepted state. Candidate staging is for safe publication, not a request to diff old/new rows. The Refresh action itself communicates replacement; do not add a per-record reconciliation dialog or require a report proving what changed.

Save before Refresh if the previous view/edit/format state needs a durable checkpoint; use a named tag when that checkpoint must survive later Saves. Ordinary bounded Undo/version retention still applies to states and data already retained under the chosen storage mode. Refresh does not create an automatic user tag, promise all-time copies, or recover old linked source bytes it never retained.

Same-version sort/filter/page fetch is not Refresh. Preserve the already verified record-format behavior and source-identity protections within its accepted native scope. The new policy defines the future user Refresh boundary; it does not remove existing tests or authorize styles to attach to a different source version accidentally. No row-by-row identity or deletion proof is needed merely to reset ad hoc state at that boundary.

### 7.6 Drill-through and full analytical snapshots

A saved pivot's displayed values are not its source rows. Drill-through creates a new query-backed sheet against the exact backing dataset/version, constrained by the selected group's dimension keys, object/source filters, measure/grain and subtotal/grand-total role. Quote/bind typed values including NULL; do not build the predicate from formatted labels. Show provenance and avoid silently returning current external rows as the historical rows behind a saved number.

For a pivot backed only by Connect, unavailable or changed external data can disable exact drill-through while the full saved pivot remains readable. For a pivot backed by saved Data Model M1, drill-through queries M1 and works without the external file. The new sheet initially holds a query/view, not an automatic second full copy of the matching rows; the user can explicitly materialize it.

Non-additive measures, calculated fields and top-N/Other are not always invertible to a simple equality predicate. Enable only supported lineage/grain combinations and provide a concrete unavailable reason otherwise. Drill-through predicates still require source/group provenance; this does not require implementing the deferred semantic formula-reference feature FT01.

### 7.7 Excel refresh formatting findings

Microsoft documents formatting preservation and refresh layout controls separately from query transformations. The following describes those documented contracts, not a measured claim about every Power Query connector or a particular installed Excel build.

| Mechanism | Documented behavior | Relevance to record annotations |
| --- | --- | --- |
| External data range properties | Preserve cell formatting and separate controls for sort/filter/layout and row insertion, clearing or overwrite. [Microsoft Support](https://support.microsoft.com/en-au/excel/manage-external-data-ranges-and-their-properties) | Controls how returned data occupies/formats the worksheet; does not document a universal primary-key match for a manually highlighted source record. |
| QueryTable PreserveFormatting | Can extend formatting shared by the first five data rows to newly returned rows. [Microsoft Learn](https://learn.microsoft.com/en-us/office/vba/api/excel.querytable.preserveformatting) | A formatting-preservation/extension rule, not evidence of durable identity for one highlighted business record. |
| Conditional formatting | A formula determines whether the current cell data meets a formatting condition. [Excel for Mac](https://support.microsoft.com/en-gb/excel/use-a-formula-to-apply-conditional-formatting-in-excel-for-mac) | A rule such as matching a record key is reevaluated against current data; this is different from manually painting one row. |
| Power Query Merge | Joins tables using chosen matching columns. [Microsoft Learn](https://learn.microsoft.com/en-us/power-query/merge-queries-overview) | Can associate a separate annotation table with data through an explicit key; it is not automatic preservation of cell styles. |

**Design inference:** Excel's documented Preserve Formatting is insufficient as the guarantee for keeping a unique row highlight attached across arbitrary insert/delete/reorder/duplicate refreshes. No claim is made that every Excel refresh path always loses or always preserves such a highlight. Keyed annotations and value-based rules solve explicitly defined cases; they cannot identify a deleted or indistinguishable duplicate record without additional policy.

**Accepted product decision:** use the simpler Refresh replacement policy in section 7.5. Field formatting and conditional rules survive compatible bindings; ad hoc row/cell formatting and local value overrides do not carry over. No missing-row proof, dormant reattachment or key-based edit reconciliation is required. This is the user's chosen Duxcel behavior; the Microsoft sources above are not presented as a claim that every Excel connector always resets formatting identically.

## 8 Workbook history and named versions

### 8.1 One workbook chronology

Use one workbook mutation order by default. A transaction changing two sheets is undone as a unit. Deleting a sheet retains its stable ID, contents, analyses, placements and reference state for restoration; hiding/reordering is not deletion. Undo may reveal the affected sheet to explain the operation, without changing which operation is undone.

Do not initially expose a toggle for independent per-sheet Undo. It becomes selective Undo once references cross sheets and requires dependency/conflict analysis. A filtered history view is safe; skipping intervening dependent mutations is a different feature. This follows the brief's later preference for coherent workbook-wide history.

Keep the durable revision sequence monotonic. Undo/Redo append resolved state transitions with links to the original user action and update the user-history cursor/stacks. Recovery replays those transitions, including cursor changes, rather than assuming that every logged mutation is another undoable user edit. Undoing an Undo must not recursively add endless user-history entries.

New edits after Undo clear the current Redo branch. Previously tagged checkpoints remain reachable. Initial history UI is linear; a general branch graph is deferred. Previewing an old version is read-only. Restoring it creates a new current revision/checkpoint target after preserving the previous recoverable state, rather than silently deleting later work.

### 8.2 Save and tag state machine

Use three distinct concepts:

- **Working Head:** latest accepted document revision, with a separately known durable revision.
- **Latest Saved:** a single replaceable pointer to a complete checkpoint in the named workbook artifact.
- **Named Tag:** an explicitly retained label pointing to an immutable checkpoint.

Save writes a coherent checkpoint for captured revision R and presentation generation P, including data required by the selected source modes and complete analytical outputs, then updates Latest Saved only after successful publication. It does not append a new named tag. Create Tag writes/reuses a checkpoint and durably retains its label; the proposed default also updates the named workbook. A blank workspace needs a destination on its first user Save/Tag; opening the application does not force a filename or save dialog.

| Sequence | Expected visible history |
| --- | --- |
| Edit, Save, close, reopen, Edit, Save | One Latest Saved entry; no automatically multiplying named versions. |
| Edit to R10, Create Tag V1 | V1 and Latest Saved can reference the same checkpoint. Show one state with both labels if useful. |
| Continue to R20, Save repeatedly | V1 still points at R10. Latest Saved advances; intermediate automatic saves do not become named tags. |
| Create Tag V2 at unchanged R20 | Reuse the existing state payload; add V2's label. |
| Open V1 in preview | Current head and recovery continuation remain intact. |
| Restore V1, then edit | Start a new current lineage; retain V1 and other named tags, clear incompatible current Redo. |

Deduplicate checkpoint content/manifests, not just revision numbers: two revisions may describe equal content, while equal SQL text over changed source bytes is not equal state. View bookmarks can have a separate generation so a pan alone need not duplicate a large dataset snapshot. Automatic recovery checkpoints are implementation artifacts and do not create user-visible versions.

### 8.3 Retention and pruning

**Accepted D07:** retain Undo across the latest **three version states including Latest Saved**, plus the current unsaved suffix. This means **two inter-version intervals**, not three. In the user's example V10 is Latest Saved: retain V8→V9 and V9→V10, plus changes after V10. V7→V8 is outside the fine-grained window. V1 through V7 remain directly openable checkpoints. Ten versions is an example, not an accepted total-version cap.

Build the window from ordered named-version boundaries plus Latest Saved, merging the same boundary when a tag and Latest Saved point to it. Repeated Save replaces the Latest Saved endpoint without creating another named boundary or discarding history from the retained start. With fewer than three version states, retain the available history. Internal recovery checkpoints never consume a visible version slot. Cross-session Undo is part of this default; expose the actual retained range.

An immutable content root is not an Undo boundary: A→B→A can reuse A's payload
while retaining two separate operation intervals. Bind version boundaries to
explicit durable version-boundary anchors plus reversible-operation positions on
a proved path; coalesce labels only when they share that anchor. A presentation
state can change without an Undo event. Boundary issuance must observe history
and presentation changes even if a content root is reused; index publication
generations and root hashes alone cannot serve as the history anchor. Saved at an undone cursor must keep valid current
Redo, and an older active cursor must remain reachable. The pure W12a planner
checks this window but does not persist anchors or make existing session Undo
durable; adapters must prove actual event/payload continuity before publication.

Implementation constraint: current worksheet and lifecycle rings retain at most
32 session actions. Saving those rings later cannot recover already evicted
operations or detached sheet contents. Durable event admission must precede
coordinated mutation and ring eviction, with explicit owned before/after payloads
for cells, formats/selectors, analysis, objects, structural composites and sheet
lifecycle. Queue/admission failure must not leave half a mutation or half an event;
no-op changes do not consume history. Do not serialize process structs or process
session-order IDs as the durable format. Live display defaults remain outside data
Undo. Keep durable history inactive until all supported producer families and
Save/checkpoint/Open publication preserve the complete accepted window.

Use an explicit resource policy for unusually large intervals. Byte/age/operation budgets must not silently reduce the accepted window: report a storage limit and offer deliberate pruning/export or reject the operation before protected history is lost. Do not promise all-time fine-grained history.

Prune only after replacement checkpoints and their manifests are durable. Reachability includes named tags, Latest Saved, active head, retained Undo/Redo, recoverable continuations, historical preview and in-flight Save/read snapshots. Enforce storage limits without deleting the only recovery path or silently evicting a named tag. Tag deletion can free data only after all other references retire. Large intervals may exhaust budget even when their count is small.

## 9 Checkpoints caches and file packaging

### 9.1 Complete document state

A checkpoint must contain all authored cells, expressions, schemas/field identities, overlays, transformations, sheet structure/metrics, bindings, object definitions/placements/styles, Master View composition, workbook display defaults and required source/asset references for that revision. It also retains complete analytical outputs as specified below. It is independent of viewport coverage. It may share immutable chunks with other checkpoints, but its manifest must resolve without replay from an earlier tag.

Separate completeness of authored state from completeness of evaluated output. For linked mode, an independent checkpoint still has declared external dependencies; only retained inputs can guarantee a self-contained portable version. A snapshot of RAM Page caches cannot satisfy either document completeness or full analytical-output completeness.

### 9.2 Complete analytical outputs and bounded runtime caches

**Accepted D06:** Save/Tag retains the full output of each pivot/aggregate analytical object, including all values, typed schema, dimension/measure identities, totals and formatting/layout, even when parts are offscreen. Charts retain their complete plotted analytical data, encodings, axes/labels and presentation state. A 5×5 pivot must reopen with all 25 values without rerunning the external source query. Its underlying million-row table is not implied by those 25 values.

| Stored state | Save requirement |
| --- | --- |
| Manual cells, definitions, edits and presentation | Complete authored state, independently of viewport. |
| Imported source or Data Model dataset | Complete workbook-owned dataset at the captured version; no external file needed for queries over it. |
| Pivot/aggregate/chart object | Complete typed analytical result and representation state; required historical payload, not an evictable preview cache. |
| Large connected browse/table query | Definition/history/styles and bounded preview; full result is optional until explicitly imported/materialized. |
| Runtime Pages and GPU textures | Bounded/disposable loading and rendering caches. Their bounds do not truncate the saved analytical snapshot. |

Stream required results to storage with bounded RAM and backpressure. Do not impose a hidden output-row cap or silently fall back to saving only the visible Page. Save shows progress for required persistence and fails/cancels safely if a required payload cannot be written completely. A complete older generation is different from an incomplete result: accepted Q07/R03 preserves it with its actual provenance and warning rather than requiring new computation merely to save the workbook.

Key stored results by source/base/overlay versions, definition/schema and relevant calculation semantics, with explicit completeness. Workbook display defaults may reformat typed output without invalidating its calculation. Preserve saved results for historical display even when source refresh is unavailable. Exact drill-through is governed by section 7.6.

Formatting, movement and Master View z-order normally invalidate layout/paint, not SQL aggregates. Row/column edits can invalidate reference mapping and layout. Formula/input edits invalidate affected calculation dependencies. Reuse immutable unchanged analytical payloads between checkpoints rather than copying/recalculating on every Save.

#### 9.2.1 Outdated analytical output warnings

**Accepted Q07:** keep an explicit warning on the analytical object when known input data, query or definition revisions have advanced beyond the complete result currently displayed. Explain which dependency changed and identify the last completed result generation/time where available. Pending calculation, failed refresh, unavailable source and stale displayed data are distinguishable states; an object can be both stale and in error.

Track desired definition/data revisions separately from computed/displayed result revisions. Movement, focus, zoom or display-locale changes alone do not mean the analytical data is stale. An unverified external source is not proof that its bytes changed; display verification/unavailability honestly. Refresh completion clears the warning only if the accepted result matches all current dependencies.

**Accepted R03:** Save may persist the newer definition together with the previous complete result and its explicit stale status. Keep their distinct provenance on reopen; never relabel the previous output as a newly computed result. If no complete result exists, save the authored definition with an explicit uncomputed/pending/error state and no fabricated result payload. IO failure still fails Save safely; unavailable recalculation alone does not prevent preserving the authored workbook.

Recalculate dependent formulas when a new analytical result is accepted. A referenced position that disappeared yields a reference error; an incompatible value/type or failed calculation yields the appropriate typed value/calculation/unavailable error in the formula cell. A changed but valid numeric result simply recalculates normally. Staleness alone is not proof that every reference is broken.

While required analytical work is pending, preserve any last completed display with its pending/stale provenance; do not publish a newly calculated current value from mismatched generations. Missing output is pending/unavailable, not zero, empty text or a successful result. Bound reads, statistics, copy/export and drill-through identify the actual result generation; inspecting/copying a last-known snapshot cannot silently present it as current. The evaluator's exact error labels are implementation details, and typed errors remain distinct from literal text containing those labels.

### 9.3 DuckDB persistence and journal topology

**Accepted D04:** use DuckDB for workbook persistence and the local recovery journal. Do not add a SQLite dependency or make a DuckDB/SQLite bake-off a prerequisite. Reconsider only if a measured workload demonstrates an unmet requirement and a concrete benefit.

DuckDB supports ACID transactions, including BEGIN, COMMIT and ROLLBACK; its persistent database is distinct from a queried Parquet file. The native workbook writer already uses a DuckDB transaction. [DuckDB transaction management](https://duckdb.org/docs/current/sql/statements/transactions), [persistent database connections](https://duckdb.org/docs/current/connect/overview). The application journal still needs explicit transaction/history/checkpoint records: database rollback after failure is not user Undo across already committed actions.

The recommended topology is a private durable DuckDB recovery/working store per document/session, plus explicit publication of the named DuckDB `.dxls` artifact. They use the same engine but have different lifetimes and Save boundaries. Read/write journal records through a coordinator-owned connection; the app does not have to export the named file just to append an action. Preserve one writer process and define connection ownership; the current analytical in-memory connection alone cannot provide restart recovery.

Two files do not become one transaction merely because both use DuckDB. The authoritative sequence remains journal commit → prepared model adoption → derived analytical/cache updates. Save publishes the exact revision/presentation generation and required payloads into the named artifact, then advances Latest Saved and eventually trims its covered journal prefix. Required payloads become durable before a manifest references them. Test crashes at every boundary; do not depend on a transaction spanning independently published files.

Keep the journal connection open and use prepared/batched writes where measured useful. Pointer motion never generates durable actions; one paste/drag is one logical transaction. Under accepted Q05, measure and state the bounded group-commit window while preserving pending versus durable acknowledgement. Save always drains included pending actions and guarantees the complete captured state before reporting success.

Keep product recovery/retained assets in durable platform user-data storage or an explicitly owned workbook working store, outside ignored `build/` and temporary query-spill paths. Generated tests and fault-injection artifacts remain under `build/tests/results/`. A portable `.dxls` must not require the user's private working-store file or a live database WAL to reconstruct its promised saved state. A different package container is not required for the first slice.

### 9.4 Proposed persistence records

These are logical record families for the selected backend, not committed SQL table names or a second generic workbook writer.

| Family | Minimum contents and invariant |
| --- | --- |
| Document and session | Document ID, format/command semantic versions, allocation high-water marks, accepted/durable/saved revisions, presentation generations, session generation and closure state. |
| Transactions | Transaction/request ID, actor, parent/new revision, command kind/version, resolved payload hash/reference and affected owners. Commit only complete records. |
| User history | Original action IDs, grouping, Undo/Redo links and current cursor/branch metadata. Reopening reconstructs the promised retained user history. |
| Payloads | Typed before/after chunks, deleted-sheet state, frozen record selections and any nondeterministic accepted outputs. References carry size/hash and lifetime ownership. |
| Checkpoint manifests | Complete logical state root, internal dataset roots, presentation defaults, required analytical snapshot roots and optional browse-cache roots for one revision/generation. Publish only after required chunks are durable. |
| Tags and saved pointer | Named tag to immutable manifest binding and one replaceable Latest Saved manifest binding. Tag labels do not duplicate datasets. |
| Assets and source bases | Dataset/base identity, retained content hashes, size/type and either retained bytes or an explicitly external dependency. |
| Analytical snapshots | Complete pivot/aggregate/chart output, definition/data identity, typed schema, provenance, completeness and historical/frozen status. Retained checkpoints pin these required payloads. |
| Browse caches | Optional bounded connected-source preview, compatible definition/source key and coverage. Eviction does not delete required analytical snapshots or internal datasets. |

Database transactions/WAL protect record writes; these application records define Undo and user versions. A manifest root or payload reference cannot be pruned while a retained record still requires it. A portable file must carry the promised retained-history subset, not an unresolvable pointer to private session storage.

## 10 Recovery Save and close lifecycle

Replace a single dirty flag as the conceptual model with document ID, accepted head revision, durable journal revision, saved checkpoint revision, session generation and saved-view/cache completeness. The existing dirty flag can remain a derived UI signal during migration. Head equality and content equality are different when Undo returns to saved content.

| Event/state | Required outcome |
| --- | --- |
| Prepared command, no durable acknowledgement | Show pending/unsaved state; never claim crash protection. Commit failure leaves the accepted model unchanged. |
| Crash after commit but before UI acknowledgement | Recover the committed transaction once; retry returns its recorded outcome. |
| Crash with committed suffix beyond Latest Saved | Offer Open saved checkpoint, Recover committed changes and Cancel, with revision/time and recovery estimate where known. |
| User chooses fast checkpoint open | Preserve the recoverable continuation. First mutation requires recovery or an explicit new lineage/copy decision so it cannot overwrite the deferred tail. |
| Recovery underway | Build a separate candidate with progress/cancel. Activate it atomically after validation; keep old checkpoint/tail on failure. |
| Save of R finishes while head is R+N | Mark R saved, retain R+N as unsaved/recoverable. Late completion cannot clear newer dirty state. |
| Save canceled or IO fails | Preserve old artifact, working state and journal. Report the actual remaining durability. |
| Normal close after successful Save | Record session closure only after the intended saved/durable boundary and writer shutdown protocol. |
| Close with committed but unsaved edits | Apply the explicitly selected Save, keep for recovery, discard or cancel policy. Fast close can retain a recoverable suffix without full materialization. |
| Explicit discard | Deliberately retire the relevant continuation after identifying its scope; never treat a crash as discard. |
| Disk full, permission loss or writer failure | Retain the last durable state, stop claiming protection, preserve pending work and expose retry/Save As/copy options. Do not silently continue a guaranteed-durable mode. |

A clean-exit marker is diagnostic, not sufficient proof that no unsaved work exists. A deliberate fast quit may be clean but leave head newer than Latest Saved. Compare durable/saved revisions and session ownership. A stale lock cannot be broken solely because a process ID is absent; verify the document/session identity and platform ownership policy.

**Accepted D05/Q05:** local DuckDB journal protection plus explicit named-file Save. Optional named-file autosave is separate. Save includes complete data according to Import/Connect/Data Model rules and declared analytical-output generations; progress describes actual work and success only follows full publication. Measure the bounded pending interval and power-loss guarantees as implementation acceptance, preserving the accepted pending/durable distinction.

**Accepted UI feedback:** current close/unsaved dialogs appear oversized relative to the application. Redesign them with compact, content-driven bounds, restrained padding/buttons and clear Save/Discard/Cancel semantics; keep progress visible without a disproportionate modal. Validate short/narrow windows and scaling with the actual native UI. This is a requested visual correction, not a newly reproduced runtime measurement.

**Accepted D14:** Save As from a historical preview exports that inspected state, not the later live head. Default to a fresh current-state-only `.dxls`; optionally include a selected retained history range. Preserve all assets/analytical snapshots required by the chosen state/history even when truncating old operation records. Propose a new document identity with origin provenance so its recovery session is independent. Missing external dependencies remain declared; Save As does not secretly import them. CSV/Parquet export remains a different data-only action.

## 11 Layered placement and grid alignment

### 11.1 Geometry and ownership

Separate **grid occupancy**, **floating presentation** and **Master View sheet composition**. A grid-resident analytical object owns its successful local worksheet footprint and cannot overlap another such object on that sheet. A floating object can overlap other floating objects visually. A Master View can composite different sheets even where their grids/objects cover the same pixels; each retains its own address space.

For floating objects, dragging brings the object to the front; explicitly clicking an exposed object also brings it forward. This affects visual interaction, never a formula binding or a dataset. Under accepted Q04, keep click-induced promotion in transient focus and persist deliberately authored ordering through explicit commands; a focus click does not append document Undo.

Provide always-reachable object controls outside occludable content: an overlay handle/edge rail plus the object inventory or equivalent guaranteed route. Protruding handles alone are insufficient when many objects exactly coincide or the handle is offscreen. Resolve handle collisions in the overlay UI and provide keyboard Select/Reveal/Bring to front. Covered objects must remain recoverable without moving unrelated content.

Moving an analysis changes placement and dependent reference mapping as one transaction; it does not paste values into `CellStore`, overwrite manual cells or transfer their formatting. Use explicit selected-owner input routing. Accepted Q04 Master View geometry/transparency remain display rules, not a mutation of participating sheet data.

Retain three separate geometries:

- Sheet layout: cumulative column widths and row heights in unscaled document units.
- Object layout: local matrix row/column metrics or chart bounds, local scale and local pan where supported.
- View transform: sheet zoom, UI scale, scroll/frozen planes and device pixels.

Persist document geometry, not framebuffer coordinates. Apply and invert the same transforms for drawing, hit testing, selection and dragging. Test variable metrics, frozen columns and zoom rather than assuming a uniform row grid or multiplying screen pixels into stored positions. Sparse row-height overrides and indexed cumulative geometry avoid dense per-row allocations.

### 11.2 Explicit alignment modes

| Mode/action | Exact effect | Impact on other content |
| --- | --- | --- |
| Free placement | Retain unscaled position, bounds and local scale, optionally with a cell anchor plus offsets. A matrix keeps its own grid and qualified local A1 addresses. | Visual overlaps are allowed; snapped-to-floating reference migration is R01. |
| Snap matrix to sheet | Choose a concrete anchor and map its local rows/columns one-to-one to the target sheet rows/columns. Local display metrics adopt sheet metrics; define title/header/total participation. | Does not change sheet widths/heights; must pass grid occupancy. |
| Fit sheet to matrix | Resolve the matrix's displayed local metrics into target sheet row/column metric changes and align placement. | Changes every use of those sheet rows/columns, including offscreen cells and other anchored objects. |
| Fit chart to range | Set chart bounds to the edges of an explicit/previewed sheet rectangle; the chart has no invented cell grid. | Does not change sheet metrics; decide floating-aligned versus grid-resident reservation explicitly. |
| Detach from grid | Capture current displayed bounds/local metrics as free placement. | Values and references stay bound to the same analysis. |

Fit sheet to matrix must preview the affected row/column spans and use one transaction for metrics plus placement. Competing snapped objects may request incompatible widths/heights; choose a policy such as reject with affected-object details or explicitly detach/update affected placements. Do not silently let the last pointer event redefine other objects' layout contracts.

Nearest range is ambiguous. Resolve a deterministic anchor, span, distance/tie rule and sheet-bound clamp before commit, then persist the resolved result. Charts can snap to the nearest previewed bounds or a user-chosen range; whether a matrix preserves its row/column count through scaling is a separate rule. The default is one local result row/column per target row/column when snapped; free scale does not change analytical cardinality.

**Accepted D10:** retain explicit persistent alignment with a bounded frame by default, and do not automatically resize sheet metrics whenever a pivot refreshes. Distinguish the complete result from its bounded visible frame. Explicit result-footprint growth must pass occupancy checks; blocked growth uses the spill state below. Growth never erases cells. One-time Fit sheet does not authorize future automatic sheet resizing.

### 11.3 Existing overlap invariants

Replace the single `objects_fits` concept with distinct validations: worksheet bounds, placement overlap policy, destructive cell-move conflicts and source-write eligibility. Audit `manual_range`, `source_overlap`, history restoration, object hit testing, range plans, copy/statistics routing, source-format publication, Save/Load and shared browser validators. Removing one collision check would leave invalid saved workbooks or ambiguous input behavior.

Manual range selection must not automatically include every visually covered object when moving cells. Preserve an explicit operation scope: cells only, selected placement, or a deliberate compound selection. The current range planner's contained-object behavior needs a compatibility decision under overlap.

### 11.4 Spill state and placement conflicts

**Accepted:** moving/snapping a grid analytical object into another grid object's occupied area must produce an Excel-like cell/anchor spill error. Retain the definition and complete computed output behind the failed visualization. Moving to a valid region enables display without requiring recalculation merely to repair placement. This is placement blockage, not failed analytical materialization.

Represent `Ready`, `PlacementBlocked` and analytical `Pending/Error` separately. Save/reopen must retain blocked requested geometry, result provenance and a reachable move handle. Do not paint a partially overlapping output or destroy another object's cells to make space.

**Accepted Q03 behavior:** keep the last successful effective footprint and reference mapping until a new placement succeeds; retain the requested target and show its spill error through a reachable anchor/overlay. Bound formula reads report a typed spill error while blocked; previous complete values remain separately inspectable. A successful retry atomically publishes placement and dependent A1 rewrites. If there was no prior successful footprint, retain an error anchor without expanding the body. An already-occupied anchor cannot be replaced by an error cell; preserve existing content and use the selected object's handle/status. The typed error must not be confused with literal text containing an error label.

## 12 Selection and dragging contracts

**Selection appearance:** for a multi-cell selection, render one outer active boundary. Retain the active cell/anchor for keyboard navigation, formula entry and traversal, using a subdued interior cue if needed. Object frame selection, matrix cell selection and chart-mark selection must remain visually distinct. A single cell may have one active border. This is a focused change that can ship before durable history.

**Region discovery:** start from the current finite selection and offer strict containing supersets. Use explicit binding/object/result bounds before broader inferred/used-area bounds, with deterministic tie rules. No inference rule claims that it knows the user's intended table or creates a permanent table. Candidate discovery must not scan a dense sheet or infer occupancy from cached Pages alone.

W03's initial manual policy uses four-neighbor (up/down/left/right) components of stored nonempty text. Diagonal contact and missing cells do not connect components. Literal `NULL`, whitespace and formula-looking text are populated text; manual empty input removes the sparse entry. Each command takes its ORIGINAL finite selection as the seed, unions the complete components with populated cells inside that selection, and encloses those bounds plus the original selection. Cells brought into the bounding rectangle do not recursively seed other components during the same command. A later Select All may deliberately expand again from its new finite selection. This replaces the old bounding-growth heuristic that could absorb irregular neighboring islands immediately.

If the original selection has no populated manual seed, choose the smallest component bounding rectangle that contains it (a blank interior), resolving equal area by `(r0,c0,r1,c1)`. Otherwise skip this inferred stage when no strict expansion exists. Formatting-only coordinates neither connect manual regions nor extend the used-data rectangle. Ready source/Table/Pivot bounds include SQL NULL and zero-length VARCHAR positions through their complete relation extent, independently of currently cached values. Used-data bounds include known offscreen manual/source/object extents; row/column selections and the whole sheet retain the existing ladder semantics. Future typed formula cells should use stored formula presence even when their result displays empty; typed/formula execution remains W26a/W26b, not an implemented capability of W03.

**Gesture arbitration:** border drag moves an already selected range/placement. Option on macOS/Alt elsewhere plus interior drag requests the same move intent, subject to focused controls/editor/popup ownership. During text editing, Option remains available to text/IME behavior. Set a drag threshold to distinguish selection/click from movement; retain visible keyboard/menu Move alternatives. Escape cancels the preview and creates no document transaction.

Capture the source geometry, target identity, document/metric revision and the actual local grab point at pointer-down. For a grid range, retain the grabbed cell's row/column offset within the selection. For free placement, retain its local continuous coordinate. Resolve the preview from that point, not by snapping the object's origin under the cursor. At sheet bounds, clamp the whole selection and show the actual result without implying that an impossible pointer anchor was preserved.

With free-object transform `T`, local grabbed point `q` and pointer in document coordinates `p`, translation is chosen so `T(q) = p`. With a snapped range, target origin is the target pointer cell minus the captured cell offset. Variable-width cells require cell-aware mapping; a fixed pixel delta is insufficient. If subcell fractional anchoring is desired for snapped objects, define it explicitly rather than promising both exact pixel anchoring and exact grid alignment.

For a three-row selection grabbed in its second row, dropping the pointer on row 10 places the selection starting on row 9, within bounds. Dragging a free chart from its lower-right interior keeps that interior point under the pointer. The current border-drag code already captures a grabbed row/column delta; extend and verify it instead of assuming all movement starts at the upper-left corner.

Validate the complete destination from the model, including offscreen cells and intervening metric/source changes. Floating overlap needs an occlusion cue, not a destructive-overwrite warning; grid analytical collisions use the spill state. Manual cell moves retain Excel-style replacement as the target behavior, with explicit replace/cancel before data loss; the accepted D11 initial fallback is safe rejection until that UI exists. A bottom status message alone cannot authorize replacement. Moving a range partly over itself uses one atomic plan, not sequential clearing and pasting. Store exact before/after targets and affected values/styles/references. A drag is one Undo action, including its dependent formula rewrites.

## 13 Automation and headless execution

Create an in-process C document API first. UI intents, tests, a future CLI and platform IPC adapters call it. Core reads include document/revision metadata, sheets/objects, typed cell/range/result queries, schemas, source status, history and operation status. Read requests name a revision or receive one explicitly; paginated results cannot silently mix revisions. Prefer semantic IDs and values to screenshot coordinates for ordinary agent work.

The running app remains the owner of its workbook. External clients send bounded messages; they do not write `.dxls`, recovery journals or DuckDB tables behind it. DuckDB's documented in-process read/write model supports this ownership choice. [DuckDB concurrency](https://duckdb.org/docs/current/connect/concurrency).

Proposed transports are a user-scoped Unix-domain socket on macOS/Linux and a named pipe on Windows, behind one versioned protocol. Exact transport selection is deferred; COM or D-Bus wrappers may be added for ecosystem integration without defining document semantics. Browser control needs a separately authorized message/host bridge into the shared C API, not an assumption that native IPC exists in Wasm.

The protocol needs capability negotiation, explicit instance/document routing, stable error codes, revision preconditions, bounded payloads, request deduplication, pagination, asynchronous completion/cancel and local authorization. No listener is required by default. Local filesystem/user permissions and a scoped session capability should protect an enabled endpoint. Opening/importing/exporting files and executing SQL remain declared capabilities, not implicit privileges of a read-only client. This is product automation, with separate build/runtime controls from the development-only review bridge.

Headless means document transactions, typed reads and analytical execution can run without raylib window/GPU/platform dialogs. Hidden-window mode still has rendering/context dependencies and is a different execution profile. Screenshot/render export is an optional renderer host with deterministic size/scale and an explicit completion barrier. Extract core lifecycle incrementally from App; do not describe a hidden test window as a headless engine.

## 14 Migration and platform contracts

### 14.1 Code ownership changes

| Existing area | Proposed change and boundary |
| --- | --- |
| `src/workbook/workbook.*` | Own stable workbook/sheet identity, content/presentation generations, internal datasets, Master View composition and transaction adoption. Make live authority explicit instead of relying on active App copies captured before Save. |
| `src/workbook/cells.*`, `objects.*`, `format.*` | Add prepare/adopt APIs that produce owned deltas without independently appending a second history entry. Retain existing atomic range/format plans during migration. |
| New narrow command/history modules in `src/workbook/` | Validate typed targets, resolve mutations, coordinate one workbook history and return an effect/invalidation plan. No raylib, platform dialogs or IPC parsing. |
| New journal/checkpoint seam beside workbook persistence | Own serialized record codecs, durable completion, replay, checkpoint manifests and retention. Keep backend-specific handles behind the seam. |
| `src/transforms/` and `src/data/` | Resolve Connect/Import/Data Model boundaries, effective/range relations, refresh candidates and supported drill-through predicates; carry data/definition revisions and lineage. Keep SQL semantics out of UI gestures. |
| `src/data/worker.*` and browser bridge | Separate latest-wins reads from ordered writes and long-running preparation. Define cancellation/publication before selecting extra threads or connections. |
| `src/grid/` | Own selection/drag state, grid occupancy/spill state and reversible geometry transforms; coordinate parsed reference rewrites through commands rather than mutating stores from painting. |
| `src/ui/` | Render the accepted value/layout state and distinguish selection, owner and result status. Drawing must not query or commit. |
| `src/app/app.c` | Integrate document lifecycle, command dispatch, scheduling and publication through extracted cohesive controllers. Keep one App/shared-interface owner and move scenarios to test-only boundaries incrementally. |
| `src/platform/` and `web/` | Adapt durable locations, IO/picker permissions and authorized IPC/host messages. Preserve shared C domain/command semantics. |
| `tests/`, `tools/tests/`, `web/tests/` | Extend existing model/engine/native scenarios and external/browser fault orchestration with named consumers; keep generated recovery files private. |

The first command slice should wrap an existing manual-edit path end to end before converting every handler. Use one history authority per converted operation; avoid a transitional state where both store snapshots and the new workbook history independently undo it. Later extractions follow concrete ownership needs rather than a broad App rewrite.

### 14.2 File and identity migration

1. Introduce stable document/sheet identities and a workbook revision independently from `analysis_revision`, request IDs and sheet indices. Legacy files receive identities in a validated candidate; persist the mapping only through a successful new-format Save. Preserve existing object/source high-water marks.
2. Migrate existing objects to an explicit legacy aligned, non-overlapping placement mode. Preserve current source-header/address behavior and verified format owners. New overlapping/free states require a schema version that older writers cannot silently flatten or discard.
3. Add metadata families through the shared C codec inventory, admission budgets, validation, writer and native/browser adapters. A native-only new format must fail clearly in unsupported browser readers while existing supported files retain their current component behavior.
4. Keep candidate loading and atomic App publication. Validate duplicate IDs, dangling owners/references, malformed journals/manifests, unsupported command versions, oversized payloads and asset integrity before activation. Archive extraction must reject path traversal and excessive expansion if a package is selected.
5. Save As serializes the inspected state and optional selected history with its full required dependency closure. Use a new independent recovery identity by default, retaining origin provenance. Do not bind recovery to pathname alone or merge histories merely because two file copies share a name.
6. Preserve native core-before-workbook fixture ordering, sanitizer coverage, and the current format identity/Worker acceptance. New retained-history policy must coexist with bounded current snapshot stacks during migration without applying both Undo systems to one edit.

Browser durability remains its own integration gate. Storage is origin-scoped and quota-managed; an estimate is not a reservation or a portable workbook Save. [Storage Standard](https://storage.spec.whatwg.org/). Prove DuckDB-Wasm persistence, journal lifetime, embedded datasets and complete-result streaming through the official Worker and shared codec. No SQLite-Wasm dependency is introduced. Keep one shared C model/UI.

## 15 Main pitfalls and mitigations

| Risk | Failure example | Required mitigation |
| --- | --- | --- |
| Mixing identity and position | Format/edit follows visible row 7 after a sort onto a different record. | Dataset/base/record/field identity; verify source epochs before mutation. |
| Visual overlap changes formulas | Reordering Master View sheets changes the value of `B7`. | Stable sheet/object binding, unambiguous grid ownership and atomic A1 rewrites on movement. |
| Partial collision migration | UI permits overlap but Save rejects it, or copy reads the hidden owner. | Separate validators and migrate all consumers together. |
| Latest-wins mutation loss | Scrolling replaces a pending journal write. | Ordered write lane and revision-bound read scheduling. |
| Journal/artifact inconsistency | Recovery journal acknowledges R20 but the named `.dxls` contains R19 after a crash. | One durable sequence, deterministic projection and Save publication protocol, even with DuckDB on both sides. |
| Preference-dependent redo | Paste uses the new locale or today's clipboard. | Capture resolved typed values and semantic policy. |
| Undo breaks dependencies | Undo only the source sheet while retaining dependent sheet changes. | Workbook chronology first; defer selective Undo. |
| Expensive inverse | Delete a million rows but retain only DELETE SQL. | Retain immutable base/before payload references or reject beyond budget. |
| False self-contained version | A tag stores a Parquet hash but the source was removed. | Retained assets or a declared externally dependent version. |
| Unbounded Save | Every minor format change recalculates/copies every full result. | Reuse immutable complete analytical snapshots; stream required new results with progress and bounded RAM. |
| Invalid historical cache | Old totals appear current beside new formulas. | Cache/result revision provenance and explicit stale/frozen states. |
| Local headers change target meaning | A floating pivot over D10 inserts a sheet D10 reference while its chrome shows local A1. | Explicit sheet/object reference kind, local header transforms and visible authoring context. |
| Names break formulas | Renaming a dashboard title changes every expression or ambiguous display names resolve incorrectly. | Separate display/code names and immutable IDs; atomically rewrite code-name references. |
| Misleading durability | Commit acknowledged before chosen sync/flush guarantee. | Define and measure acknowledgement/power-loss contract. |
| Retention removes recovery | History cleanup deletes a base still needed by deferred recovery. | Reachability-based pruning after durable replacement publication. |
| Grid metric conflicts | Fit one pivot to sheet shifts another dashboard object. | Impact preview, anchor rules and explicit conflict policy. |
| Headless still opens windows | Core API triggers native picker or initializes raylib. | Inject platform IO and separate optional rendering host. |
| Runtime leaks from tooling | Agent API reuses excluded review endpoint or Python queue. | Product C protocol and separately authorized adapters. |
| Memory amplification | Live model, Undo, prepared command, Save snapshot and result caches coexist. | Account owned buffers and in-flight/canceled work before admission. |
| Historical sensitive data | A deleted value remains in retained versions/export. | Explicit history export/retention policy and fresh current-state export; no unsupported secure-erasure promise. |
| Refresh retains obsolete ad hoc state | A new row acquires the previous third row's highlight or local patch. | Reset affected ad hoc styles/overrides in the successful Refresh transaction; retain field formats/rules without row matching. |
| False drill-through | Saved historical aggregate opens today's changed external rows as its original contributors. | Pin internal dataset/version or reject unavailable exact provenance. |
| Required result treated as cache | Eviction removes the only saved full pivot because its source is offline. | Distinguish retained analytical snapshots from optional browse Pages. |

## 16 Performance and resource acceptance

Validate the accepted DuckDB backend using the pinned project engines and current Mac. Measure a single cell, 20,000-cell paste, format range, internal record patch/delete batch, Undo/Redo, journal reopen, import/model refresh and checkpoint publication while analytics is busy. Compare immediate versus bounded group commit and complete-result streaming. Reconsider another dependency only after a concrete measured shortfall.

Measure p50/p95/p99 command preparation, durable acknowledgement, UI frame stalls, recovery time, first useful frame, full-result readiness, file growth and peak owned/RSS memory separately. Use the existing large-source reference for read/overlay behavior without changing its bytes. Include disk-full and interrupted-checkpoint cases, not just a warm happy path.

Proposed product targets for discussion: local selection/drag preview stays within a 16.7 ms frame budget; ordinary edit feedback appears within 50 ms even when durability is pending; small local commits target p95 below 100 ms; eligible saved view reopening targets a useful first frame within 1 s on the stated fixture; long materialization/recovery exposes progress/cancel and never freezes the window. These are candidate targets, not measured results or universal file-size promises. Define fixture sizes and cold/warm conditions before acceptance.

W08c2 native policy: wake the writer immediately, naturally coalesce at most
256 records/4MiB, and acknowledge only after COMMIT. Keep pending distinct from
durable; no artificial debounce or guaranteed wall-clock durability deadline.
On this Mac/DuckDB v1.5.6, optimized warm producer/durable p95 is 0.005/1.516ms;
256 burst edits use two commits. A 20,000-cell legacy paste needs a new baseline:
about 0.99ms producer copy and 2.14s background checkpoint, with full replay
verified. Runtime library/input/helper hashes live in ignored run receipts.
These are C API timings, not UI latency SLAs, and do not admit legacy paste into
the one-cell journal. Later range/import/result commands need their own bounded
measurements; keep analytical-worker contention separate from this workload.

Retain existing Page/wire/format limits until deliberately changed with evidence. Add explicit bounds for journal queue, resolved deltas, retained history, tags/assets, full-result cache, and Save/recovery staging. Low storage or memory must reject/adapt before publication rather than pruning protected history opportunistically. Group commits can improve throughput only under an explicit acknowledgement/data-loss-window contract.

## 17 Acceptance scenarios

Every scenario needs the relevant model, analytical, persistence and real-input evidence. Pure geometry tests do not prove event delivery; a reopened file does not prove correct formula/reference behavior.

| ID | Scenario | Required assertion |
| --- | --- | --- |
| A01 | Select a three-row manual range, then expand Select All repeatedly. | One outer active border; active cell retained; deterministic strict supersets; no dirty/history/query side effect. |
| A02 | Border-drag and Option/Alt-interior-drag from the second cell at several zooms/metric layouts. | Grabbed cell remains the pointer anchor; truthful preview; one committed move; Escape/no-op makes no history entry. |
| A03 | Move partly over the same range and onto offscreen occupied cells. | Atomic self-overlap behavior; full destination checked; explicit conflict action; exact Undo. |
| A04 | Float/scale overlapping objects, promote by drag/click and reveal a fully covered object. | No values/styles deleted; unobscured controls remain reachable; paint/hit order agrees; formulas do not rebind by z-order. |
| A05 | Apply all three snap/fit operations with variable metrics and another anchored object. | Mode-specific geometry, visible impact, deterministic conflict policy, one Undo and identical Save/Open geometry. |
| A06 | Edit a typed bound manual range feeding a pivot and a dependent expression. | NULL/empty/literal text/precision preserved; coherent revision, schema changes explicit, cycles reported. |
| A07 | Patch duplicate imported/model records under filter/sort, including a present NULL and empty-text value. | Correct internal IDs changed; all consumers including full export agree; untouched duplicate preserved; external bytes unchanged. |
| A08 | Add/drop/reorder a field and insert/delete records. | Stable surviving field/record IDs; dependent bindings repaired/invalidated; Undo restores exact values/schema/order. |
| A09 | Edit sheet A, then B; delete A; Undo/Redo across them. | Workbook chronological order; deleted sheet's identity/content/references return; IDs never recycled. |
| A10 | Change workbook display locale after applying Automatic and explicit formats, then Undo/Redo/replay a data edit. | Typed values/calculation identical; Automatic uses the current workbook display default, explicit format stays fixed, data Undo does not rewind locale. |
| A11 | Save repeatedly, create unchanged/changed manual tags and reopen each. | One Latest Saved; immutable named tags; shared state payload; restore independent of old fine-grained history. |
| A12 | With V10 Latest Saved, retain three version states and compact source/history storage. | V8→V9 and V9→V10 plus current suffix survive across reopen; V7→V8 may prune; older tags still open; repeated Save creates no extra boundary. |
| A13 | Kill the owned test process before commit, after commit/before acknowledgement, and through each Save publication stage. | Old or new complete state only; no partial multi-store change; committed retries apply once. |
| A14 | Open checkpoint instead of recovering, cancel recovery, then attempt a new edit. | Tail remains available; candidate failure is atomic; explicit lineage policy prevents loss. |
| A15 | Save R while another command produces R+1; scroll and cancel analytics. | Save reports R; R+1 stays unsaved/recoverable; no durable write dropped. |
| A16 | Remove/replace linked bytes, then restore a version; repeat with retained source assets. | Linked dependencies fail explicitly; retained version remains exact; no positional reattachment. |
| A17 | Send stale, duplicate, unauthorized and oversized automation requests, including UI/agent contention. | Predictable errors and bounded admission; one ordered authority; no hidden filesystem/SQL privileges. |
| A18 | Use the same operation through UI, IPC and headless core. | Equivalent typed document state and analytical results; no window for core-only execution. |
| A19 | Load legacy supported files and new features through native/shared-codec/browser paths. | Correct migration or explicit unsupported result; old candidate preserved; no weakened current format-identity coverage. |
| A20 | Reopen a checkpoint with pivot cells outside the last viewport and a million-row connected browse query. | Full pivot/aggregate/chart output retained; connected browse result need not be embedded; no Page-only snapshot presented as full analytical output. |
| A21 | Reference a pivot through A1 from another sheet, move it, then Undo/Redo. | Same local value remains bound; displayed A1 references update in the move transaction, including absolute references; no stale-position fallback. |
| A22 | Place two grid analytical footprints in conflict, including an occupied attempted anchor. | Visible spill/anchor error and retained calculated result; no overwritten cells; reachable recovery handle; valid retry restores visualization. |
| A23 | Render sheets together in Master View with different widths/heights and change sheet order. | Independent sheet addresses/data/formulas unchanged; explicit click owner; documented transparency; no implicit geometry mutation. |
| A24 | Connect, Import and Add query to Data Model, including a hidden internal dataset. | Correct source/data boundary; Connect edits convert explicitly; Save/export never changes input bytes; model remains queryable without a sheet table. |
| A25 | Save external-backed and Data-Model-backed pivots, remove external access and drill through. | Both full pivots display; model-backed drill-through opens a correctly filtered query sheet offline; external-backed drill-through reports unavailable provenance. |
| A26 | Refresh after row additions/deletions/duplicates with ad hoc formats and local imported/model edits. | Successful publication replaces affected data and resets ad hoc state; compatible field formats/rules remain; no row matching/reconciliation. |
| A27 | Save As from an old preview with current-state-only and selected-history options. | Artifact contains inspected state, complete required assets/results and only selected history; live workbook/recovery remain independent. |
| A28 | Open blank workspace and exercise Save progress/close dialogs in narrow and scaled windows. | No forced initial Save/name; compact proportional dialogs with clear actions; no false completion while required data/output is pending. |
| A29 | Rename display/code names, move a matrix and switch from snapped to floating with both reference forms. | Display rename leaves expressions intact; code rename rewrites bindings; mode change converts analytical sheet references to qualified local form in one Undo action; existing local references remain unchanged. |
| A30 | Focus an off-origin matrix with independent row/column metrics at several zooms and then return to the sheet. | Local A/B/C and 1/2/3 chrome aligns to the visible object span; no duplicated labels or geometry mutation; reference insertion names the correct owner. |
| A31 | Change input/query/definition while retaining an old complete result, then Save/reopen and accept a refreshed result. | Save succeeds with distinct generations and warning; removed references/type/calculation failures appear as typed formula-cell errors, while valid changed values recalculate normally. |
| A32 | Refresh a highlighted/edited view with unchanged, removed, reordered and duplicate rows. | All affected ad hoc highlights/value overrides reset regardless of content equality; field formats stay compatible and conditions reevaluate current values, including key 482 with other changed fields. |
| A33 | Fail/cancel Refresh after preparing new data but before publication. | Current data, ad hoc edits/formats, document history and saved state remain intact; successful retry publishes values and reset together. |
| A34 | Save/tag before Refresh, refresh successfully, then Undo/restore within retained history. | Restore the retained prior values/styles without a row-diff engine; no automatic user tag or promise of unretained external bytes. |

Fault injection belongs in owned test processes/private databases, never the user's running application. Use C99 product-linked tests, Python orchestration, and JS browser adapter tests in their existing repository locations. Hidden native workflows retain the desktop lock. Record source/binary/fixture and elapsed limits in ignored results rather than tracked evidence capsules.

## 18 Delivery plan ownership

The dependency-ordered implementation backlog now lives in [ROADMAP.md — Workbook TRD implementation slices](../ROADMAP.md#workbook-trd-implementation-slices). W01–W40 (with W26a/W26b split) replace the former coarse S0–S9 proposals, with priorities, prerequisites, separately demonstrable scope and acceptance gates. G01/G02 apply codec/browser and resource/failure checks to each affected slice instead of leaving integration until the end.

This TRD continues to own semantics, requirement IDs, acceptance scenarios and deferred capabilities. ROADMAP owns sequencing and implementation state; STATUS owns verified current behavior and limits. Independent selection/gesture improvements can ship before durable history. Native recovery does not wait for complete browser persistence. Semantic pivot references FT01 remain unscheduled.

## 19 Decision register

These product directions were accepted or refined on 21 October. Remaining details are listed separately in section 20; this register no longer presents them as fourteen unanswered product questions.

| Decision | Accepted direction | Remaining boundary |
| --- | --- | --- |
| D01 | Separate Connect, Import and query-result materialization into an optional nonvisual Data Model. Successful Refresh replaces affected data and resets ad hoc edits/styles; retain field formats/conditional rules. | Accepted R02; internal storage representation is an engineering choice. No record survival/diff/duplicate reconciliation. |
| D02 | Positional sheet and object-local references, separate code/display names and local focused headers. Same-sheet grid ownership is unambiguous; floating/Master View allow visual composition. | Accepted R01 converts analytical sheet references to qualified local form on snapped-to-floating change. Semantic pivot lookup is deferred FT01. |
| D03 | Edit the workbook and its internal data; never implicitly edit external Parquet/CSV. Connected relations are read-only; data-only export is explicit. | Materialize to establish a new editable boundary when a query is otherwise read-only. |
| D04 | Continue with DuckDB, including the local journal. Add no SQLite dependency now. | Q05 accepts pending/durable separation; measure exact flush/latency bounds during implementation. |
| D05 | Protect edits through the local journal and publish named `.dxls` state on Save, with progress. Save complete older output with its newer definition/stale warning; preserve explicit status for uncomputed objects. | Accepted R03: recalculate on new results and show actual formula failures in their cells. Named-file autosave is separate. |
| D06 | Save full pivot/aggregate outputs and chart data/state; do not require saving every large connected browse query. Data Model data is retained in full. | Required output cannot silently truncate to runtime Page limits. |
| D07 | Retain cross-session Undo across three version states including Latest Saved: V8→V9 and V9→V10 in the ten-state example, plus the current suffix. | Resource limits must be explicit; this is not a ten-tag retention cap. |
| D08 | Persist workbook visualization defaults outside data Undo. Automatic formats look up the current default; explicit formats ignore it. | Keep input parsing/calculation semantics separate and typed. |
| D09 | Preview old versions read-only; restore into a preserved/new current lineage and Save explicitly. | Retain previous recoverable work before restoration. |
| D10 | Persistent alignment, bounded frame by default and no automatic sheet metric resizing on refresh. | Explicit footprint growth validates occupancy and can produce spill blockage. |
| D11 | Safely reject occupied manual destinations until explicit replacement UI exists; target behavior supports deliberate Excel-style replacement. | Analytical occupancy is not a manual replacement target. |
| D12 | One placement per analysis initially, with identities separated enough for later reuse. | Master View re-renders existing sheets; it does not duplicate analysis ownership. |
| D13 | Start with document safety/command boundary and the focused selection/drag fixes, then source/model editing and layered placement. | Proposed dependencies only; no roadmap scheduling in this revision. |
| D14 | Save As captures the inspected historical/current state; default current-state-only export, optional selected history inclusion/truncation. | Preserve dependency closure and propose an independent new document/recovery identity. |

## 20 Resolved behavior and implementation parameters

The product policies previously identified as Q01–Q07 and R01–R03 are resolved. No blocking product choice remains from that list. The implementation still needs bounded measurements, concrete APIs and verification; those do not reopen the accepted behavior.

| Question | Accepted resolution | Implementation responsibility |
| --- | --- | --- |
| Q01 | Local positional A1 space for floating/aligned matrices, preferred dot-qualified sheet/object names, separate code/display names and focused local headers. | Canonical header/title origin, identifier validation, owner-aware parser and geometry. |
| Q02 | Positional references only now. No category/measure-following lookup. | FT01 is an unscheduled nice-to-have; drill-through keeps its separate supported lineage rules. |
| Q03 | Preserve last successful placement/reference mapping plus pending target; show spill without overwriting content. Bound reads report spill errors; previous values remain inspectable. | Typed error, reachable handle and exact Undo/Save restoration. |
| Q04 | Inspection-first Master View, explicit active owner, independent sheet geometry and explicit alignment. | Clipping/transparency/focus; no composite cell editing initially. |
| Q05 | Prompt feedback, pending/durable distinction, measured bounded group commit, full flush/publication on successful Save. | Record actual latency/power-loss/loss-window evidence with the pinned DuckDB path. |
| Q06 | Explicit Refresh resets affected ad hoc edit/format state and replaces returned data, retaining compatible field formats and conditional rules. | Atomic candidate publication and scope isolation; no record-survival or reconciliation engine. |
| Q07 | Warn on analytical objects when known data/query/definition revisions postdate the displayed complete result; retain actual provenance. | Distinct desired/displayed generations and dependent recalculation/error states under R03. |

| Policy | Accepted behavior | Acceptance implication |
| --- | --- | --- |
| R01 | Snapped-to-floating mode change converts existing analytical sheet bindings to qualified object-local form atomically. Existing local references remain unchanged; re-snapping does not silently rewrite them back. | One Undo restores both mode and reference form; no accidental reads of newly exposed worksheet cells. |
| R02 | Refresh replaces affected values and discards ad hoc row/cell formats and local overrides. Keep compatible field formats and conditional rules. No row diffs, deletion proof, duplicate matching or dormant-style reattachment. | Reset on successful Refresh even for unchanged returned rows; failure/cancel leaves old state intact. User Save/tag plus ordinary retained history protects prior states. |
| R03 | Save preserves authored definitions and available complete result snapshots with explicit stale/uncomputed status. New analytical results trigger formula recalculation; actual reference/type/calculation failures appear in formula cells. | Stale warning alone is not a broken-reference error. Preserve last completed displays while pending, avoid mixed-generation current results, and never fabricate missing values. |

Implementation parameters to settle in the relevant slice:

- Choose and measure the journal's maximum pending interval, flush contract, queue bounds and shutdown behavior on the pinned native/browser engines.
- Fix the formula grammar's identifier case/reserved-name rules, local header/title origin, range syntax and concrete typed error labels while preserving the accepted reference kinds.
- Choose bounded Page/result streaming, checkpoint/history staging, resource-denial behavior and legacy/shared-codec migration details with existing ownership guarantees.
- Define the Refresh scope descriptor and reset plan so affected datasets/views change atomically while unrelated cells/objects remain intact. This is address/ownership validation, not record matching.
- Validate compact dialogs, local header clipping, snap geometry and Master View interaction through the real native workflow.

These are engineering parameters and acceptance work. The deferred capabilities below remain separate from the first implementation slices.

## 21 Deferred capabilities

| Feature | Scope | Scheduling and nondependency |
| --- | --- | --- |
| FT01 | GETPIVOTDATA-style semantic references keyed by dimensions/measures/categories rather than local cell position. | User-requested nice-to-have. Unscheduled; not required for positional formulas, object movement, local grids, Save or drill-through. |
| FT02 | General conditional-formatting authoring/runtime beyond the initial formatting controls. | Rule semantics are accepted: reevaluate conditions over refreshed values. This is not a request for key-based edit/style reconciliation or record-survival tracking. |
| FT03 | Editing underlying sheets directly through Master View and reusing one analysis in several authored placements. | Later extension; initial Master View is inspection-first and analysis placement remains one-to-one. |
