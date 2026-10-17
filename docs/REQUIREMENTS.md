# Requirements and later decisions

This traceability index reconciles the [original supplied brief](archive/INITIAL_BRIEF.md), later user instructions and the exact UI definition. It does not certify completion; [STATUS.md](STATUS.md) gives implementation quality/evidence and [ROADMAP.md](ROADMAP.md) orders the remaining work.

| Requirement | Current interpretation / implementation owner | Status reference |
| --- | --- | --- |
| C99 + raylib + native DuckDB C API; GNU Make; no new GUI toolkit/runtime | Existing native source and thin system macOS bridge. Python/JS developer utilities and the read-only mock are not shipped runtimes. | Architecture; build; all native features |
| The actual web interface is the design definition | Match its rendered states, widgets, typography, animation and interactions exactly; real engine replaces mock calculations. | UI parity milestone remains open |
| Excel's blank expansive worksheet and Tableau's spatial authoring | Sparse canvas independent of source bounds, elastic scrolling; same-sheet data and multiple anchored objects. | Grid, objects, authoring core |
| Discoverable outlined/icon buttons, ribbon modes, darker blue direction | The later executable reference resolves exact appearance; do not start another interpretation from Excel/Tableau inspiration. | Adaptive ribbon/shared-control parity |
| Native Open/Save As and consistent file branding | `.dxls`, case-insensitive input; native file panels, proper document lifecycle; no required absolute-path typing. | Native files/persistence |
| Vertical sheet navigation with clear Add and scrolling | Independent Workbook/Fields, wheel/thumb, reorder/hide/rename/duplicate without accidental new sheet. | Workspace/navigation |
| Familiar selection/navigation/resizing | Range anchor separate from extent; contextual Cmd+A; drag border, move, auto-scroll, keyboard equivalents and stable cursor. | Grid; active-origin acceptance still open |
| Canvas zoom separate from application scale | Control-wheel/pinch over canvas; separate interface Settings and DPI/font policy. | Grid/workspace; physical gesture/DPI limits remain |
| Blank/manual cell entry and structural menus | Sparse text editing/paste/move with undo; safe insert/delete around source/object ranges. Sources remain read-only; richer typed editing/overlays future. | Manual cells/structural edits |
| Column-anchored filters with visible choices | Real paged distinct values, search/multiselect, explicit conditions, scalable scrollbar/cache; no click-to-cycle controls or arbitrary 1,000-value cap. | Value filters; final labels/anatomy open |
| Date filter hierarchy | Recognized date/time → Year/Month/Day/Hour/Minute with typed boundaries and NULL handling. | Date filters |
| Same-sheet empty/existing pivot context | Click template/result → left Fields/Measures/Marks/Filters and compact top Rows/Columns; actual DuckDB PIVOT; no detached query-view substitute. | Authoring/pivot; UI parity still open |
| SQL-native reproducible transformations | Real filters/sorts/unique/aggregation, inspectable SQL, owned definitions/history/persistence. Further select/rename/cast/calculated-column/join operations remain future slices, not pretend menus. | Transformations; future analysis |
| Bounded real charts associated with worksheets | Bar/Line/Dots/Scatter and saved placement; Marks interaction and full analytical semantics tracked separately. | Chart/Marks and Other limitations |
| Missing values are genuine NULL and visibly distinct | Sparse missing cells render distinct monospace NULL; preserve imported empty VARCHAR and literal `NULL`/quotes without conflating storage values. No synthetic blank storage category. | Typed data/rendering/clipboard |
| Responsiveness and explicit ownership | Bounded page/distinct/GPU caches; no idle queries or whole-source materialization for browsing; cancel/stale safety; real performance evidence and sanitizers. | Data/worker/renderer; precise measurement limits |
| Representative internet data and existing Homebrew toolchain | Existing NYC3.475M-row Parquet, CSV and round-trip fixtures; no additional installs needed. | Validation records/download script |
| Focus on macOS now | Windows/Linux packaging retained but execution deferred by explicit user instruction; no new platform-install work. WebAssembly later. | Scope/portability limits |
| Git, cleanup and parallel work with shorter tests | Local recovery snapshot, owned branches/worktrees, one App/desktop integrator, tiered checks and coherent acceptance manifests. | Current development milestone |

The original brief's Inter font and minimal-toolbar suggestions are superseded by Plex Sans and the exact mock. It also explicitly permits complete vertical slices with remaining work documented; future Excel import, DuckLake/external connections, relationship models, locale conversions, recovery and large-scale performance work should not silently become gates for this UI migration. They remain product backlog and need explicit acceptance definitions when prioritized.
