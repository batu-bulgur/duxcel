# Duxcel

A native C99 + raylib + DuckDB analytical spreadsheet. Start with a blank worksheet, import CSV/Parquet or a read-only DuckDB relation, browse a virtualized grid, transform data visually, and save the workspace as `.dxls`.

The interface direction combines Excel's expansive worksheet with Tableau's spatial analysis authoring. Native file panels, a blue/gray native ribbon, editable worksheets and virtualized distinct/date filters are implemented. A primary source, independently sourced named tables, multiple anchored two-dimensional pivots, bounded charts and manual ranges coexist on the same sheet. Further table-placement and overlay work is tracked in [current roadmap](ROADMAP.md).


## Build and run

On macOS with Apple Command Line Tools and Homebrew:

```sh
brew install raylib duckdb pkg-config
make
./build/bin/duxcel
./build/bin/duxcel /path/to/data.parquet
./build/bin/duxcel /path/to/workspace.dxls
make run ARGS='--demo'
make debug
make bundle
open build/Duxcel.app
```

The existing machine already has the native dependencies. The optional browser toolchain is isolated under `build/web/`; see [its setup guide](../web/README.md). The bundle links Homebrew libraries. Fonts and their OFL license are bundled. Assets resolve relative to the executable or app Resources; `DUXCEL_FONT` can override the font path. DuckDB 1.5.6 and raylib 6.0 were tested on macOS Apple Silicon. A small Objective-C bridge uses built-in AppKit for native panels, menu commands and pinch events; the application/engine remain C99.

**Open** uses the macOS file picker for CSV/TSV/CSV.gz, Parquet, DuckDB and `.dxls`; file drop and command-line paths also work. Opening data adds a worksheet. Opening a workspace replaces the workbook. The native File menu contains document, import and export actions; Edit contains history and clipboard actions. Home, Insert, Data, Analyze, View and contextual Visualization follow the design definition. Insert creates analyses; Analyze/Visualization edit the selected view; View recovers panes and controls interface size, density, gridlines and focus. Font size, bold/italic/underline, text color, alignment, Accounting/Number and decimal controls apply bounded range formats, with Undo/Redo and saved state. Cmd+B/I/U share those handlers. View → Freeze first 2 columns fixes the first two source columns in browsing and the authoring preview; its setting persists separately from the workbook. Crowded views retain the setting and expose Unfreeze. Font family is read-only and Wrap is disabled, as in the definition; the six Marks targets accept owned field bindings, with cached Row colors, numeric sizes, line shapes and typed hover fields; complete encodings remain pending. Developer diagnostics are available with F12. **Ribbon display** or Control+F1 opens named Full/Compact/Icons only/Focus choices. Focus hides the ribbon and docks temporarily; Escape or Exit focus restores the previous mode and pane preferences. Commands are measured with the actual IBM Plex Sans font: groups progressively collapse to named controls, then enter named overflow instead of clipping or scrolling. Each group's disclosure opens its complete controls. Save and display remain available above the command area. Sequential Excel-style keytips invoke the same real actions; keyboard traversal reveals hidden commands in their group flyout, with Escape restoring ribbon focus. Ribbon mode, pane choices/widths, motion and interface scale persist as macOS application preferences; opening a workbook keeps these application settings.

Workbook navigation is vertical on the left, with a separate **+** target and wheel/thumb scrolling. Workbook uses 28-unit rows, fitting the four-sheet compact reference into 20.25-unit rows; larger workbooks scroll. Fields uses 24-unit rows. Workbook can collapse; either section can be hidden. The Fields heading remains a label, as in the definition. Both hidden leaves no dock. Fields also works outside pivot editing: search, click or Enter to reveal a source column while preserving cell selection and row position. F6 reaches the field list; arrows/Page/Home/End navigate filtered matches. Transformations is a resizable right pane with separate collapse and dismissal controls: collapse retains a 32-unit entry with Expand and Dismiss; dismissal reclaims its entire footprint. Focus the inspector with F6 and use Left/Right to collapse/expand, or Enter on the retained entry. Reopening uses View. At narrow widths the inspector overlays the worksheet. Below 700 logical units, Workbook/Fields/Marks are recovered through the Workspace button as a temporary overlay; outside click or Escape dismisses it without changing visibility preferences. Transformations/SQL overlays below 1040 units.

The status bar is 22 logical units tall. It fits complete 12-unit selection metrics, retaining hidden values in customization rather than truncating a number. Right-click anywhere on it, or use its ellipsis, to customize statistics. **UI 100%** opens the compact interface-scale menu; macOS Preferences opens Settings. Application scale does not mark the workbook edited. The adjacent +/− controls change canvas zoom; clicking its percentage resets to 100%. F6 reaches status controls in visual order: customization, interface scale, zoom out, reset and zoom in. Action messages appear briefly above the bar.

Column-header arrows and Alt+Down open available-value checklists with search, multi-select, typed NULL/empty text, sort, Apply/Clear and a separate visible Conditions list. DuckDB returns 128-row distinct pages; four cached pages are bounded to 4 MiB each. Search has a 160 ms debounce and cancels stale requests. Selection uses up to 4,096 exceptions / 4 MiB rather than a 1,000-value display ceiling. Typed date/time columns expand Year → Month → Day → Hour → Minute, with partial parent selection. Up to 16 simultaneous column filters compose with AND. Distinct lists respect the other active filters. Analysis steps exposes edit, move and remove controls; all definitions persist in dxls.

## Native issue reporting (development build)

```sh
make review
./build/review/duxcel-review
# ASan/UBSan variant: make review-debug
# ./build/review-sanitize/duxcel-review
```

Use **Review → Report UI issue…** or **Command+Shift+.** while a filter or menu is open. The app freezes its real frame before showing reporting controls. Click a control (including a disabled button), switch to Containers, or drag a region. Tab/Shift+Tab traverses visible targets; Enter chooses the highlighted target. Space moves the picking toolbar so it does not hide the bottom of the worksheet. Write a note and use **Save report** or Command+Enter. The saved report ID confirms durable local publication. Escape cancels and returns to the existing menu, selection and text focus. Active drags finish before capture begins.

Query execution may finish in the background; result publication pauses while reporting and resumes after closing it. Capture and writing failures offer retry and retain the note. The registry enumerates the title bar/labels/Save, ribbon tabs/display/Help and blank strip areas, column filters/conditions, ribbon commands/groups/More, pane containers and on-demand worksheet cells. Other dialogs and every pane child control are not yet fully enumerated; a region report remains available. Cell positions are scoped to the captured document/source/object/query version, not stable database record IDs. Notes are limited to 4,095 UTF-8 bytes.

Reports need no running agent. The default project outbox is under `~/Library/Application Support/Duxcel/Review/`; its project ID is shared across this repository's worktrees. Set `DUXCEL_REVIEW_OUTBOX` to an absolute path to use a different private outbox. Read and maintain feedback externally:

```sh
python3 tools/native_review.py list
python3 tools/native_review.py read REPORT_ID
python3 tools/native_review.py ack REPORT_ID
python3 tools/native_review.py reply REPORT_ID --message 'What changed and what to verify'
python3 tools/native_review.py status REPORT_ID --implementation ready-for-review --verification passed
```

Design acceptance, implementation status and verification status are independent. Reports retain the immutable native image, frame/widget/state metadata and exact build fingerprints. Review code, menu commands and storage/reader bridges are compile-time excluded from `duxcel` and `duxcel-debug`; the review builds use separate preferences and artifacts. The existing `.app` bundles require their own rebuild and do not automatically include this tool.

For automatic pickup, use the single development review service below. It discovers native reports in the same outbox even when they were saved while the service was stopped. Agents acquire exclusive expiring leases; an interrupted agent can be replaced. Delivery after a crash is at least once, so report IDs are the idempotency key. The optional Codex worker performs read-only triage and records durable results in the queue; it does not implement changes or mark design/verification accepted. It uses the existing authenticated CLI with no global configuration edits. One worker runs at a time, with three attempts before a retained terminal failure.


## Browser issue reporting (development build)

`make web-review` builds the shared C interface with its review tools under `build/web/review/<signature>/`; `build/web/review/latest.json` identifies the current directory. Start one loopback service:

```sh
make review-serve
# Equivalent: python3 tools/review_service.py serve --agent codex
# Collection without model calls: python3 tools/review_service.py serve
```

Open <http://127.0.0.1:8767/>. Command/Ctrl+Shift+. freezes the actual C frame, with the same picker and note composer. **Save report** commits its immutable archive into browser IndexedDB, then submits it to the same persistent queue as native reports. The confirmation distinguishes **Saved to the shared review queue** from **Saved in this browser; awaiting the review service**. Pending browser reports retry automatically on startup, reconnect and bounded backoff. There is no automatic download/manual import step.

Keep that hostname and port stable: browser pending storage is scoped to its origin. Reload preserves pending bytes, but deleting site data or browser eviction can remove reports before server acknowledgment. A shared-queue acknowledgment follows validated, atomic, durable local publication. Stop the service with Ctrl+C; already queued native/browser reports persist. The obsolete mock/Agentation ports are not needed.

```sh
python3 tools/review_service.py list
python3 tools/review_service.py read REPORT_ID
python3 tools/review_service.py retry FAILED_REPORT_ID
# Another agent can wait for and lease a report without a model worker:
python3 tools/review_service.py next --worker my-agent --wait 60
```

The local queue is also readable at <http://127.0.0.1:8767/__review/queue>. Completed rows contain the agent's structured triage in `message`, separate from the human lifecycle. Failures retain evidence and diagnostics; there is no claim that triage fixes a reported defect.

The existing `native_review.py` reader also reads these unified report folders. Browser metadata records actual CSS, backing dimensions and DPR separately. These tools and all automatic transport are excluded from normal browser/native builds. The service binds only to loopback and requires a same-origin upload token; no Agentation, external feedback account, public endpoint or application-embedded model runtime is required.

## Workbook persistence

The workbook format is **dxls**, default extension **`.dxls`** (case-insensitive input). Legacy `.duxcel` files are accepted. Save As uses the native macOS save panel; subsequent Save writes to the selected location. Cancelling either native panel leaves the workbook intact.

Save writes transactional DuckDB metadata, using version12 for ordinary lossless files and owner-only version13 when retired object IDs must be retained or an existing v13 file is updated, provided no v14 metadata is required. It preserves primary and table source references, source queries, source-bound pivot/chart definitions, date grains, selected chart measure and positions, range formats, manual text cells, sheet definitions, per-sheet canvas zoom, interface scale, widths, selection and viewports. Versions1–14 remain readable. Native v14 additionally preserves typed source-format scopes, source-version metadata and explicit source-aware mode. Explicit manual awareness can require v14 even with no imported source bindings. Native development builds re-verify compatible primary/Table record styles and supported archived exact-result/guarded styles before displaying them. Unverified imported values keep broad column defaults but cannot inherit finite manual styles in that mode. Eligible loaded native primary CSV/Parquet legacy formats can acquire a guarded baseline after file/schema/loaded-definition verification. Changing that definition suspends their imported part; old coordinates never prove the originally styled record. Before the first Table, eligible prior manual histories, including empty/broad stores, retain their manual meaning; safe source-side manual editing also stays saveable. A blank Pivot template alone remains legacy/manual and saveable; its Save test does not establish an empty Create View UI command. Broader multi-owner/Table/PIVOT legacy migration remains unfinished. External source data and cached query rows are not embedded. Relative references allow moving a workbook and its data together. Missing sources retain their definitions; **Relink source** opens the native picker for a replacement of the same file type. Unrelated existing databases and unsupported versions are rejected on save.

On a blank sheet or in empty canvas beside imported data, type to replace a cell or press F2 to edit it. Arrow/Home/End keys position the text caret; Shift extends a partial selection, Option+arrows traverse words and Command+arrows reach the ends on macOS. Copy uses the selected substring; typing and paste replace only the selection. Command+Z / Shift+Command+Z undo and redo inside the focused editor, including its caret and selection. Consecutive typing/deletion is grouped; paste is a separate step. Committing a cell remains one worksheet Undo action. Mouse clicks and drags also place/select text, with double-click word and triple-click all selection. Paste quoted TSV into a range with Cmd+V; UTF-8, tabs and embedded newlines round-trip through copy/paste. Paste is atomic and one undo operation. Drag the selected range's border to move its contents; occupied destinations are rejected. Right-click a column to insert left/right or delete. Row-header menus offer insert above/below and delete for the complete selected row span below imported rows; whole tables/pivots/charts and notes shift together, and operations through an object's interior are rejected. These edits are sparse text cells, limited to 20,000 populated cells / 4 MiB per sheet, with 64 KiB per value. Formula evaluation and imported-data overlays are pending; external Parquet/CSV is never modified.

Chronological cell/object/analysis undo and redo are per sheet, limited to 32 actions and session-only. Manual history also has an 8 MiB text budget per stack. External source changes can alter results. Closing or replacing a modified workbook offers Save / Don't Save / Cancel; automatic crash recovery remains pending.

Cmd+W closes the Mac workbook surface and leaves Duxcel running. Cmd+Q quits. Use File→New Workbook (Cmd+N), Open or the Dock to reopen a surface. Shift+F11 adds a worksheet. In the browser, Ctrl/Cmd+W keeps its browser meaning; **Alt→F→C** closes the workbook inside the live tab. Modified workbooks offer Cancel/Discard; browser Save is still unavailable and disabled.

**File→Import table…** places a separate read-only source table at the current destination in the same worksheet; Open adds a worksheet instead. **Alt→F→I** is a Duxcel extension. Native import uses the macOS file picker. Browser import accepts CSV/TSV/CSV.gz/Parquet up to256MiB; keep the sheet and destination unchanged while it loads. Moving the destination rejects the pending import. This session can issue1,024 source names; reloading discards its unsaved data. Browser workbook persistence remains unavailable.

File supports Up/Down, Home/End, Tab/Shift+Tab and Enter/Space, skipping unavailable commands. In a short window its command list scrolls with the wheel or thumb, and keyboard navigation reveals the focused command. An outside click dismisses it before other controls receive input, including a Rows token beneath the menu. A second independent click can activate that control.

## Shortcuts and interaction

| Input | Action |
|---|---|
| Arrows / Shift + arrows | Move / extend selection, including empty canvas cells |
| Tab / Shift+Tab; Enter / Shift+Enter | Move horizontally / vertically; cycle within a selected range |
| Cmd/Ctrl + arrows | Jump within the current table/pivot, or the primary source |
| Home / End; Cmd/Ctrl + Home / End | First/last table/pivot column; worksheet start/used end |
| Page Up / Down | Move one viewport |
| Cmd/Ctrl + A | Current table/pivot/manual range → used bounds → worksheet |
| Cmd/Ctrl + C / V | Copy TSV / paste into empty canvas, with source/object collision checks |
| Type / F2 / Delete | Enter / edit / clear blank-sheet text cells |
| Shift+Space / Cmd/Ctrl+Space | Select rows / columns |
| Native Cmd/Ctrl + O / S / Shift+S / N | Open / save / save as / new workbook |
| Shift+F11; Mac Cmd+W / Cmd+Q | Add worksheet; close workbook window / quit app |
| Control + Tab / Shift+Tab | Next / previous worksheet (Control on macOS too) |
| Cmd/Ctrl + F; Alt+Down | Filter selected column |
| Cmd/Ctrl + Alt + T / F / W / I | Toggle Transformations / Fields / Workbook / Inspect SQL |
| Cmd/Ctrl + Shift + F | Toggle Focus view |
| Cmd/Ctrl + Z / Shift+Z / Y | Undo / redo / redo |
| Alt then F/H/N/A/W; F6 / Shift+F6 | File/Home/Insert/Data/View access keys / cycle ribbon, Workbook, Fields, visible shelves, grid, right pane and status bar |
| Arrows / Page / Home / End; Enter in a pane | Navigate sheet entries, filtered fields or source steps; activate |
| Alt+Left / Right in a pane | Resize the focused left/right pane; Escape returns to the grid |
| Down / Tab; Left/Right / Tab in ribbon | Enter commands / move between commands; Up returns to tabs |
| Enter / Space; Alt→H→1/2/3; Alt→H→C→C | Run focused command / Bold, Italic, Underline / Copy |
| Control + wheel / trackpad pinch | Zoom only the canvas under the pointer |
| Cmd/Ctrl + plus / minus / 0 | Increase / decrease / reset canvas zoom |
| Cmd + comma | Settings: separate application interface scale |
| Wheel / Shift + wheel; horizontal trackpad | Fractional vertical / horizontal scrolling |
| Header-edge drag / double click | Resize / fit using cached values |
| Alt + Left/Right | Resize selected column |
| Row/column header; Shift+click/drag | Select rows/columns; extend range |
| Right click; Shift+F10 on the grid | Column width, fit loaded values, sort/filter, and insert/delete outside imported columns |
| Selected-range border drag | Move manual cell contents or a whole table/pivot/chart, with undo |
| Shift+F10, End, Enter on a cell range or object | Move selection/object to a typed destination such as H12; Enter applies, Escape cancels |
| Cmd + Option + P | Insert an empty pivot beside the source or at the selected empty destination |
| F6 in a pivot/chart | Cycle Fields, Columns, Rows, Values, Filters, chart Marks and grid |
| Command+Shift+M; right-click status | Selection statistics and remembered metric choices |
| Status focus: Tab/arrows, Enter | Move between statistics and zoom buttons; open or activate |
| 1 / 2 / 3 or Left/Right in Marks | Select Bar / Line / Dots without a query |
| Alt→Y→R/C/V; Alt→J→R/C/V in a selected view | Custom Analyze/Visualization Rows, Columns or Values actions; arrows and Enter assign a field |
| Left/Right; Alt + Left/Right in a shelf | Select / reorder a pill; Delete removes it; Enter opens a filter |
| Escape / F5 / F12 | Close dialog or cancel query / reload analysis / diagnostics |

Access keys are sequential: press/release Alt, then the displayed letters or digits. Home uses 1/2/3 for Bold/Italic/Underline, FF/FS/FC for font/size/color, AL/AC/AR for alignment, AN for number format and CC for Copy. Data uses SA/SD for ascending/descending sort. Escape backs out through a prefix, tab hints and then the grid. AnalyzeY, VisualizationJ and Z-prefixed Duxcel commands are custom extensions; unavailable Excel actions stay disabled. This is the supported English mapping, not a claim that every Excel command or localized keymap is implemented.

**File → Import Table…** opens the native file picker and places an independent CSV, Parquet or DuckDB source at the selected empty range in the current sheet. **File → Open** keeps its separate new-sheet behavior. Tables retain their own filters and sort; header dropdowns enumerate values and dates. Select a table, then **Pivot table**, or use its context menu, to create a bound pivot beside it. The contextual fields and four shelves follow that table's schema. Charts created from a pivot retain its source binding. Filtering one table updates its dependent objects while preserving unrelated result pages. The Transformations pane, sort dialog, cell inspector, copy and selection statistics understand these sources. Drag a table's selected border to move it; F2 at its title renames it. Its context menu offers native source relinking and, for DuckDB files, **Choose table or view…** with a visible scrollable list and keyboard selection. A changed relation clears that table’s filters and sort in one undoable action; bound pivots retain their definitions. A table supplying a pivot/chart cannot be deleted until those objects are removed.

Table pages are capped at 256 columns / 4 MiB, with one 256-row page per object; each sheet supports at most 32 objects / 8 MiB of definitions. The first table page can appear before the exact count; status shows ≥ and counting. Results below 256 rows need no separate count query. If notes or objects share the table’s possible future footprint, display waits for complete size validation. Growth beyond the sheet or into occupied cells is rejected. A resolved count is reused when an obstructing note is removed or undone. Copy, statistics and table moves wait for a complete count. Undo restores the previous source definition and preserves known cached result dimensions. Table source files remain external. The first DuckDB relation is pinned to the table definition; the context menu can select a different table/view. The choice survives save/reopen. Unloaded source values use an ellipsis; actual SQL NULL retains its distinct monospace mark.

Select **Insert → Analysis view** to insert an empty pivot on the current sheet. Clicking a pivot reveals its contextual Fields list, compact Columns/Rows, and left Measures/Marks/View filters. A field click highlights it; drag, Add field or its action menu assigns it. The source preview and paired matrix/chart views share existing cached engine results. Preview cells return to the real source; header filters remain source-scoped. Below 880 logical units the views stack in a scrollable canvas. Multiple row/column fields and measures generate actual DuckDB `PIVOT`, with at most 128 result columns. A pivot's context menu can create an independent chart; charts render up to 64 groups and four series and display those bounds. The same shelves support Bar, Line, Dots and numeric Scatter Marks; changing Marks retains the current result and supports undo. Lines break at missing/non-finite values; Dots uses the categorical group axis. Scatter uses one numeric Rows field as X and one numeric measure as Y, with Columns splitting series; NULL/non-finite pairs are omitted. Numeric positions use actual distance, and axes describe the loaded groups. Definitions, Marks and placement save to `.dxls`. In the authoring workspace, Bar/Line use Columns as categories and rank the three largest loaded Row groups, with Other for composable aggregates. Plot chooses an assigned measure without a new query; its state supports Undo/Redo and save/reopen. Partial results and unsupported Other aggregates are explicitly labelled. Matrix Total columns and pinned Grand total readouts use real source-derived GROUPING SETS. Aggregate-aware chart Other for AVG/distinct remains pending.

Closing a modified workbook or opening another workbook asks Save / Don't Save / Cancel. Save completes before the deferred action proceeds; failed save/load preserves the open workbook. An active manual cell edit is committed before this check. Save As cancellation returns keyboard focus to the canvas.

The canvas starts with 52 columns and grows on rightward navigation to 16,384. It has at least 1,048,576 rows, extending beyond larger sources. Manual text cells outside source/object ranges are editable; imported sources are read-only. Elastic edge overscroll keeps logical cells and query offsets fixed, then springs back. Settings → Reduce motion disables it. Named table nesting remains planned.

Rectangular copy includes source, pivot and adjacent manual cells. Cached rectangles need no query; uncached intersections use bounded projected worker queries. Copy enforces 20,000 cells and an 8 MiB output limit. TSV preserves full values and quotes embedded tabs/newlines/quotes; NULL becomes an unquoted empty field, while a genuine empty VARCHAR is quoted. The value strip displays the selected cached/manual cell; it is not a formula editor. A selection containing entire pivots/charts and notes can move together, with one undo/redo action. Partial objects, source-data moves and occupied destinations are rejected atomically.

## Validation and datasets

Use the [tiered testing guide](TESTING.md) for routine work:

```sh
make test-fast TEST_ARGS='--modules grid choiceview --jobs 2'
make test-affected FEATURE=author-menus TEST_ARGS='--modules choiceview'
make test-full TEST_ARGS='--require-data'
```

The existing individual smoke/stress targets remain available for focused investigations; the full runner records completion, hashes and timings. Public downloads are optional outside their named data scenarios and stay ignored. `python3 tools/download_test_data.py` restores the NYC source; optional `--roundtrip` creates CSV/Parquet derivatives under build.

Release and ASan/UBSan suites cover typed queries, exact numeric filters, virtual distinct/date filters, clipboard atomicity, worker recovery, font metrics, grid geometry, vector icon geometry and workbook ownership/history/round trips/failure handling. Current counts, coherent regression checkpoints and measured timings are in validation records. Native scenarios cover analytical objects, independent sources, filters, clipboard/export, range movement, sheets, typography and ribbon layouts. These application-state smokes complement physical mouse/key/DPI inspection. LeakSanitizer is unsupported by this Apple Silicon runtime.

The approved modular duck icon is maintained as a true SVG with generated C geometry and prebuilt native sizes. Normal builds use the prebuilt assets; changed artwork regenerates them offline using existing Python 3. The macOS bundle registers its ICNS, and the native bridge also supplies the icon to direct binaries. Settings uses a single mipmapped display cache. See [application identity and reproducible asset checks](../assets/icons/README.md).

Public fixtures use [NYC TLC trip records](https://www.nyc.gov/site/tlc/about/tlc-trip-record-data.page) and [seaborn's taxi CSV](https://github.com/mwaskom/seaborn-data/blob/master/taxis.csv). Downloads include a 3,475,226-row Parquet, a full CSV export and a verified CSV-to-Parquet round trip, approximately 460 MiB combined. They are ignored. See validation records for measurements and limits.

## Architecture

`src/app` coordinates commands; `src/ui` owns fonts/widgets; `src/grid` owns geometry/input/visible-cell drawing; `src/workbook` owns raylib-independent definitions and persistence; `src/transforms` generates SQL; `src/data` owns DuckDB results and the worker; `src/platform` isolates threads, clocks and paths.

Layout/input use logical units. Application interface scale and per-sheet canvas zoom are separate. Cell sizes/text receive their product; row-number gutter width, column-header height and header text stay at interface scale. Row labels follow row height and column labels follow column width. Fonts rasterize at framebuffer density and interface scale; canvas zoom reuses the atlas. Glyph origins snap to physical pixels. Visible glyphs use constant-time lookup and clipped texture quads, avoiding per-cell scissor changes. Column prefix offsets support binary-search hit testing. Blank cells have geometry without allocation; only populated manual cells are stored. Cursor choice is applied once after frame hit testing.

The worker owns its connection. Pending DuckDB execution checks cancellation between tasks/stages; IDs reject stale replies. The UI only invokes `duckdb_interrupt` under the worker mutex. Shutdown interrupts and joins before freeing database state. Workspace IO runs on the same worker; source/schema changes are blocked during save/load so metadata writes cannot be replaced by ordinary paging.

Two 256-row pages are retained for back scrolling. Counts and catalog metadata are reused; uncovered viewports are debounced for 25 ms. Blank canvas browsing performs no page query. All source columns are still fetched for a page, so very wide/long data can use substantial memory. Payload bytes exclude allocator/driver overhead. DuckDB uses a 512 MB engine budget and two threads; this is not a process RSS limit.

Query and workbook connections use separate, private directories in writable system temporary storage, rather than a `.tmp` directory beside the executable or in the launch directory. DuckDB closes before these directories are removed. Finder launches and read-only working directories therefore support queries that spill beyond the memory budget. `make spill-test` verifies real spill files with a 32 MB limit, an unwritable `TMPDIR` fallback, correct results and cleanup.

For a source table, Data → **Query…** or its context menu's **Edit source query…** opens a multiline editor. Read the original CSV/Parquet/DuckDB relation as `input`, for example `SELECT City, Amount::DECIMAL(12,2) AS Amount FROM input WHERE Approved`. Command+Enter validates and applies; Escape cancels. Only one SELECT/CTE with no unbound parameters is accepted. Validation parses and binds on the worker without issuing a data-page or count query; CSV binding may inspect source samples. Syntax/binding failures preserve the displayed source and draft. Data-dependent conversion errors can still occur when the result runs. A changed query clears that source's filters/sort/summary in one Undo action, and refreshes its bound pivots/charts. Queries persist in dxls files (introduced in version 7; ordinary files write12; owner-only files can write13; dormant typed source formats require14). Versions1–14 remain readable. Supported native primary/Table record styles and archived exact-result styles now reactivate only after verification; arbitrary query lineage and browser record identity remain unfinished.

The native **Data → Remove / Restore Duplicate Rows** menu action keeps one row for each exact combination of all output columns from the selected source query. The button becomes **Keep duplicates** while active. Filters and sort priorities remain in place; bound pivots/charts update from the same relation. The **Unique rows** card in Transformations removes this step independently, with Undo/Redo. SQL NULLs compare together for deduplication and remain distinct from empty VARCHAR and literal `NULL` text. The source CSV/Parquet/database stays unchanged; version 8 saves this option for primary and independent table sources.


## Limits and portability

Native development builds support record-following formatting for a finite selection of at most20,000 records in one ready primary or embedded Table CSV/Parquet source. The first command verifies content and collects the captured selection in the background; moving the selection does not retarget it. Sort/filter/page eviction and Table moves preserve physical-record styles, including identical rows; independent Tables reading the same file stay isolated. Save/Open preserves typed styles in v14 and re-verifies before displaying them; a changed source stays suspended. Refresh starts a new binding and suspends the previous styles. Whole-column defaults remain spatial. Incompatible finite legacy/history states still block imported activation. A proven loaded primary CSV/Parquet definition can receive a guarded baseline without inventing original record identity or rewriting Undo/Redo. Unavailable, changed or incompatible owners remain suspended or reject Save with an explanation. An existing v14 source-aware archive can also restore exact-result styles when its complete file/schema/owner proof and structured filters/sorts match; changing that definition suspends those styles until it matches again. This applies to supported primary/Table sources and their raw preview, without styling a bound PIVOT result or an independent Table. There is no new RESULT-format command. Mixed/query/PIVOT formatting, broader multi-owner legacy/history migration and browser record identity remain unfinished. Pre-import manual histories and explicit source-side manual formatting stay manual and saveable through the supported import/Save/Open handoffs. Verified RAW/RESULT token lookup at the final format revision is tested separately from source-ledger exhaustion. The local signed `build/Duxcel.app` now contains10f changes, verified without launching it; standalone distribution remains separately scoped. Current behavior and limits; earlier result activation.

Explicit ordering and tie keys still matter for reproducible relational output. Deep OFFSET can repeat scans/sorts, especially for CSV; keep source files unchanged while browsing. Up to sixteen composable source filters precede an optional summary and eight sort priorities. Independent same-sheet pivots use DuckDB PIVOT with ON for Columns shelves and support their own filters; bounded bar charts share those definitions. Richer chart encodings/layout, primary-table placement, joins, source overlays, Excel import and formulas remain on the roadmap. Column insertion/deletion moves sparse notes and whole objects together, with shared history and restored widths; operations that split an object or cross the final column reject. Imported data remains read-only.

Attached DuckDB databases are read-only. Views with original-catalog references or unavailable extensions/files can fail. Nested types use DuckDB string output; unsupported glyphs fall back to `?`, and complex-script shaping is absent. General FLOAT/DOUBLE display adapts to the column width; inspection, copy and export retain the original values, including binary rounding. Exact integer and DECIMAL text is preserved.

This iteration is macOS-only, as requested; Windows/Linux validation is deferred. Mixed-monitor DPI, physical gestures, IME composition and standalone packaging need broader coverage. Synthetic direct typing lost an initial non-ASCII character during one native test; UTF-8 clipboard and persistence checks pass, so full Unicode/IME input is not yet claimed. The Makefile uses pkg-config with a Homebrew fallback for DuckDB. Library locations can be supplied with `DUCKDB_PREFIX`, `DUCKDB_CFLAGS`/`DUCKDB_LIBS` and `RAYLIB_CFLAGS`/`RAYLIB_LIBS`.

SQL NULL uses a small monospace `NULL` mark in data cells, pivot categories and filter lists. Unassigned worksheet cells also display this missing-value mark, including the value strip and inspector; they are never allocated as a dense matrix. Literal `NULL` text uses normal worksheet typography; genuine zero-length VARCHAR has a monospace quoted-text mark. CSV imports preserve quoted empty fields as text and unquoted missing fields as NULL, using DuckDB's [`allow_quoted_nulls`](https://duckdb.org/docs/current/data/csv/overview) setting. No source data is rewritten to match display labels.

Use **Data → Inspect value** or **Command+Shift+I** to read a complete cached cell with its address, column and type. Long text wraps and scrolls without changing the worksheet. Copy value copies the entire text; SQL NULL is identified as missing data and is never copied as the literal word NULL.

The compact column popup keeps a searched value checklist and condition together. Checked values and the condition combine with AND; Between includes both endpoints and preserves exact integer/DECIMAL comparisons. The value search does not become a data filter by itself. Select all toggles only the matching/scoped choices and preserves unrelated selections. Bulk choices are atomic and bounded to 4,096 exceptions /4 MiB; narrow a larger search or clear it to select the whole column. Tab stays inside the popup; Up/Down/Page/Home/End navigate its value list, Space toggles a row, Enter applies, and Escape cancels. Right opens a date branch and Left returns to its parent. A date hierarchy enumerates Year/Month/Day/Hour/Minute lazily. Source headers and view-filter pills keep their respective scopes.

**File → Export Parquet** writes the selected table, pivot or chart's complete data result. With no object selected, it exports the sheet's primary table. Each table retains its own filters and sort; pivot/chart exports use the actual DuckDB PIVOT result, including rows beyond the displayed page. Exported pivot column names include category fields and aggregation, distinguishing SQL NULL from quoted literal `NULL` and empty text. Choose the destination in the native Save panel; Escape cancels while the worker exports. Existing destinations are replaced only after success, and workbook/source files are protected. Open the Parquet to add it as a separate sheet. Save/Save As retains workbook notes, object definitions and layout in `.dxls`. The 128-bit precision guard also applies to pivot results; DECIMAL stays exact.


The native **Data → Sort by Multiple Columns…** menu action edits up to eight sort fields in priority order. Search and choose columns, select Ascending/Descending, move a level up/down or remove it, then Apply. Cancel leaves the table unchanged. Tab moves between search, columns and priorities; Enter adds a field or applies from the priority list; Alt+Up/Down reorders a selected level and Left/Right sets its direction. NULL values sort last. Use a unique final key when tied values require reproducible row ordering. Header arrows show priority; Clear sort removes the order. The sort survives undo/redo and `.dxls` save/reopen. The twelve-stage native sort scenario passed, including screenshots at 80%, 100% and 150% interface scale.

Rectangular copy combines the data table, adjacent manual cells and pivot results. Cached selections avoid a database query; uncached source/pivot intersections are projected on the worker. Missing values produce empty TSV fields, while genuine empty strings produce quoted `""` fields. Native clipboard consumers may still interpret these according to their own import rules. Typed native clipboard formats remain future work.

Browser copying uses the same bounded TSV path, up to20,000 cells/8MiB. It shows **Copying to clipboard…** until the browser acknowledges the write; denied/unavailable writes report a failure. Quotes, apostrophes and literal `NULL` are preserved. Actual cached and uncached copying is tested with an isolated memory clipboard; system permission and other browsers still need separate validation. Browser workbook Save/Open remains unavailable. Bounded data export is described below.


In the browser, **File → Export Parquet**, **Alt → F → E** or **Command/Control+Shift+E** exports the selected result through the real Worker and requests a download. The default name uses the displayed sheet/object name. The workbook stays modified; **Download requested** acknowledges dispatch, not durable saving. Escape cancels a pending export before delivery. Export stays unavailable during the two-second Blob URL release interval. Parquet transfer is bounded to 64 MiB, with a separate 24 MiB definition/metadata bound; these are not engine/process peak-memory limits. Exact integer/DECIMAL and NULL/empty/literal text survive actual download and reopen. Raw HUGEINT/UHUGEINT and unsupported nested export types fail explicitly. Browser workbook Save and OPFS recovery remain separate work.

Native typography follows the confirmed **IBM Plex Sans** choice: Regular for data, Medium for commands/column headers, and Semibold for pane titles. All three cuts and the OFL license are bundled; raylib font-data checks verify equal digit advances at eight raster sizes. Typed NULL/empty marks use bundled **JetBrains Mono Regular** (five glyphs at current display density), with its separate OFL license. Font provenance is in `assets/fonts/`; no system font path is required. Retina native layout screenshots have been inspected. Broader fallback coverage, atlas/startup profiling and mixed-monitor validation remain pending.

The canvas grows as you navigate, independently of the data length. Sparse notes and pivot/chart placements can extend past Excel's million-row boundary, up to 2^40 virtual rows while the same memory limits remain. Row-number gutters use actual font digit widths. Thumb drags keep their range fixed until release, and Command+End includes sparse content. Missing canvas values display the NULL mark without allocating cells; literal text remains distinct. Selecting a free range below the source retains that exact pivot destination. A full-width source can place a new pivot below its known row extent instead of overflowing the last worksheet column.

Sheet navigation: right-click a worksheet in **Workbook** for Rename, Duplicate, Move up/down, Hide and Unhide. F2 and Shift+F10 expose the same operations from workbook focus. Duplication copies notes, anchored objects and layout independently, while retaining the same external source reference; its undo history starts empty. At least one sheet stays visible. Names, order and visibility survive `.dxls` save/reopen. Visibility is optional version 5 presentation metadata, so older readers can still open all sheets. Outside pivot authoring, a field’s context menu offers Reveal, Center, Select result column and Go to active row. Reveal/Center preserve selection and row offset without querying the source.

Selection statistics run on the DuckDB worker after selection settles. The status menu offers Cells, Filled, Numeric, Distinct, Sum, Average, Minimum and Maximum, plus full-value copy. Enter, keypad Enter or Space opens the focused menu without changing a metric; choose its checkbox separately. Command or Control navigation works in the browser main canvas and historical preview. NULL is excluded from Filled/Distinct; empty VARCHAR and literal NULL count as values. Integer/DECIMAL sums retain exact precision; floating values and averages show `~`. Selections may combine source data, independent pivot results, pivot titles/headers, manual text and missing canvas cells. Each data intersection supports up to 256 columns; sparse snapshots are bounded to 32,768 labels/8 MiB. Chart marks are visual objects; their title is a worksheet cell. Whole row/column/sheet counts use the virtual canvas bounds without allocating empty cells.

The column menu offers an exact width at 100% canvas zoom and **Fit loaded values**, measured from loaded source/pivot pages (up to 256 rows per result) and manual cells. Changing width preserves data and selection without querying DuckDB. During border resizing, the header shows the current width; Escape restores its starting width. Right-click inside a selected range keeps that selection.

Fast mouse gestures that begin and end inside one event poll retain their starting point and modifiers. Native cell-border moves have been exercised with Undo/Redo and saved-placement inspection. The event buffer is bounded, and ordinary frame-visible pointer events continue through the existing handlers.

Selected charts have an outlined lower-right resize handle. Drag it to change the occupied worksheet cells; the preview marks rejected overlaps. Escape cancels an active resize. The object context menu also offers **Chart size…** with separate Columns and Rows fields and keyboard navigation. Resizing preserves field definitions and query results, supports Undo/Redo and saves in dxls. Sizes are bounded to 3–256 columns and 8–4096 rows.

Pivot/chart context menus include **Rename…**; F2 on the title cell opens it directly. Names support UTF-8, reject empty/control-character or duplicate names on the same sheet, and support Undo/Redo and dxls persistence. Names and chart size are presentation edits and reuse the analytical result.

In a filter popover, Control+Tab (or Control+Shift+Tab) switches Available values/Conditions. Conditions has visible focus for the mode tab, operator list, value and Apply. Up/Down or Home/End selects an operator; Enter moves to its value, and the next Enter applies it. Tab skips the value for Is NULL/Is not NULL. Escape cancels. In the normal worksheet, Control+Tab changes sheets, including when chrome has focus.


Fields supports a right-click / Shift+F10 menu with Reveal, Center, Select column and Go to field at active row. Reveal/Center retain the active range and vertical offset; Go to also brings an offscreen active row into view. Arrow keys, Tab/Shift+Tab, Home/End and Enter/Space operate the menu. Short completed button clicks are preserved even when press and release occur between frames; releasing outside a button cancels that click.

New view filters stay in the editor until Apply. Opening, cancelling or clearing a new draft preserves the current matrix, cached result and Undo history; Clear on an applied view filter removes its field.

Select All uses **Command+A** natively on Mac and **Control/Command+A** in the browser. In a Table or Pivot it expands through data, headers, the complete object, the used sheet, then the canvas, skipping unavailable stages. The active cell and selection direction stay fixed. Blank space outside used bounds selects the sheet; row/column selections also expand to the sheet. No count query is submitted just to select a range.

Move destinations match the visible grid numbering. A source's header occupies displayed row1, so its first data/model row displays2; blank worksheets start at1. Embedded Table headers do not add another global offset. Source header row1 is not a movable data cell.

The current local bundle is10f606e; the owned8767 review service serves accepted0b8cdbc Wasm. Restarting that service preserved the project/outbox configuration and zero-report count; it did not reload an already open browser tab or launch the native app. Official Worker acceptance remains separately pinned to4604369.
