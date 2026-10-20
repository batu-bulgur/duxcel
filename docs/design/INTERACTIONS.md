# Duxcel interaction rules

The editable [Excel shortcut/feature inventory](../EXCEL_SHORTCUTS.md) owns individual platform bindings, feature status and acceptance cards. This document owns shared behavior rules. Shortcut suggestions below are proposals subject to conflicts; current bindings are in [USER_GUIDE](../USER_GUIDE.md#shortcuts-and-interaction). Find/filter/Focus/F5/F12 differences remain open rather than silently redefining current input.



## Native motion and layout requirements

These are intended behavior contracts for the C UI. Consult current status for acceptance; this document does not establish that every state has been tested.

| Interaction | Current treatment |
| --- | --- |
| Button hover/press | 90 ms color/border feedback; icon moves down 0.5 logical units on press; no elastic bounce |
| Dock open/close | Width changes over 160 ms with `cubic-bezier(.22,.8,.25,1)`; opacity fades over 120 ms; inner content slides 10 units left or 12 units right |
| Manual dock resize | Immediate pointer tracking; width transition is disabled during the gesture; keyboard separator arrows adjust by 8 units |
| Workbook disclosure | 160 ms row collapse plus 120 ms opacity; collapsed content is inert |
| Popover | 110 ms opacity and 120 ms translation from 4 units above; dismissed content stops accepting input immediately |
| Authoring context | Shelves/identity fade and move into place over 140 ms |
| Field reveal | Header highlight fades over 1.5 seconds; selection is preserved |
| Drag start | 5-pixel pointer-distance threshold separates clicks from drags; pointer capture maintains the gesture |
| Drag feedback | Pointer-following labelled preview, valid destination highlighting, insertion marker; frame updates reuse bounded gesture state |
| Drop/cancel | Apply placement on release; remove feedback; Escape cancels the active drag |
| Charts | No decorative data animation; example marks appear directly |
| Reduced motion | disable decorative transitions and smooth scrolling while retaining direct input tracking |

Pane body and rail lifecycle are separate. Collapse retains a 32-unit entry; Dismiss removes the entire right footprint. The last dismissed pane leaves no rail. Workbook and Fields can each be hidden; when no left section is needed, the left footprint disappears. View and shortcuts recover hidden regions.

At less than 1040 logical units of workspace width, the right inspector overlays the canvas and leaves the left dock in place. Treat this as a layout requirement, subject to measured native viewport behavior. At canvas widths of 880 units or less, the matrix and chart stack vertically and retain their own internal scrolling. The source preview height is clamped between 150 and 336 units at 40% of the canvas height. Short left docks retain a usable Fields list and scroll Workbook/Marks deliberately. Long inspectors scroll with a sticky close header.

Interface scale offers 80/100/125/150%. Canvas zoom is independent and changes data-cell text and row/column sizes without resizing the ribbon, panes, status line or header typography. Latest human requirement, 17 October: keep the row-number gutter width and column-header tier heights at interface scale; row-header height follows its data row and column-header width follows its data column. The status step controls use 60–160%; continuous wheel/pinch is clamped to 50–250%. At 100% interface scale the status line remains 22 units. Under width pressure whole metrics move out of the line and remain in its customization menu; values never wrap into a second status row.


## Unsaved document guard

Dirty Close, Quit, New and workbook replacement share one compact guard. At normal width it is 360×152 logical units with 16-unit insets, a 14-unit action-specific title, a 12-unit ellipsized workbook name and a short loss warning. Use the existing UI font, colors, border and 28-unit buttons. The visual action order is Don't Save / Cancel / Save; unnamed workbooks use Save…. Below 320 logical units of panel width, stack the actions without reducing their hit height. Long names must not enlarge the panel.

Escape and Cancel retain the document and abandon the pending action. Tab/Shift+Tab and Left/Right cycle actions; Enter/Space activates the focused enabled action. Save finishes before continuing; failed/cancelled IO preserves the current document. Browser Save remains disabled until workbook persistence exists. Native application-menu Quit and Command-Q enter the same C guard as other quit requests, including while an editor or popup owns focus. Close and Quit remain distinct actions.

## Frozen surfaces and adaptive command states

The source grid owns an isolated stacking context. Its body is below the sticky header. Scrolling cells use body layer 0, active scrolling cells 1, frozen cells 10, active frozen cells 11, row numbers 20, and the header 30 (header corner and frozen header cells have their own local order). These numbers describe relative paint order, not native implementation constants. Selection and its handle must never paint or receive pointer input through a frozen surface. A range spanning the freeze boundary retains correct visible selection in both regions. If configured frozen columns leave fewer than 72 logical units for scrolling data, show a small local explanation with an explicit Unfreeze action. Keep the configured freeze choice until that action is used; do not silently change it.

Ribbon states are Expanded group → Collapsed group → Overflowed group. Width measurement includes actual type, icons, controls, padding, and action space, and uses logical units so UI scale affects the same policy. The user's Full/Compact/Icons choice does not change automatically. Opening any collapsed group or its disclosure shows the full named controls in a bounded flyout. Overflow opens the same group, rather than a differently implemented command. Escape returns to its invoker. If resizing removes an open flyout's anchor, close it and focus the replacement group/overflow control.

The tab names may scroll while display/help remain fixed. No command row may clip horizontally or overlap its action dock. Button hit areas and icon drawing sizes are separate: regular icons 16, full-ribbon analysis icons 20, disclosures 11; the Icons command row is 32 units tall.

Below 700 logical units, a workspace button opens Workbook/Fields/Marks as a temporary left overlay. Close, outside click, or Escape dismisses that overlay without changing section preferences. Source field reveal dismisses it after navigation. Opening the right inspector dismisses the temporary left overlay. Widening the viewport restores the dock's sections. Native modal/focus semantics for these temporary overlays remain a later validation item.

Status metrics now fit by measured complete text widths rather than a fixed item count. At extreme width pressure, no metric may fit; its values remain in customization. Object summaries shorten to complete semantic labels. The bar stays one line.

## Selection and focus

The active sheet, active object, data selection, and keyboard focus are separate state. A pane may receive keyboard focus while the grid selection remains visible. Only one editing context is active at a time.

| State | Visible result | Primary keyboard behavior |
| --- | --- | --- |
| Source/manual cell active | Strong active-cell border; address/value in context strip | Arrows move; Shift extends |
| Cell range selected | Subtle range fill and one outer boundary; active cell retained logically without a second inner active border | Shift extends; Tab/Enter traverse |
| Object frame selected | Boundary and object handles; object identity in context strip | Move/resize commands and Enter to edit |
| Matrix content selected | Matrix cell focus, scoped to that object's output | Cell navigation within matrix |
| Chart marks selected | Mark highlight and mark-scoped summary | Navigate marks and inspect values |
| Pane focused | Visible control focus; grid selection remains but is visually inactive | Controls receive keys; Escape returns appropriately |
| Popup open | Focus stays in the popup | Arrow keys, Space/Enter, Escape |

A matrix frame and a source range must not both appear to be the active selection. Previously selected content can retain a subdued inactive indication. Selection fill, active-cell boundary, object handles, and keyboard focus have different visual treatments.

Opening a pane or changing ribbon mode preserves selection. Resizing chrome preserves the active cell/object in view when possible. Returning focus does not restore an obsolete selection after the user has deliberately selected something else.

## Ordinary grid manipulation

- Click selects a cell. Shift-click extends from the selection anchor. Drag extends a range and auto-scrolls at edges. Selection normalizes overlaps before statistics or copy.
- Move an existing manual cell selection from its border or by Option/Alt-dragging its interior. Capture the grabbed cell, require five interface pixels before moving, preview the destination, cancel with Escape and commit one move on release. No-op makes no history entry. Existing occupied/read-only destination guards apply; analytical-object floating gestures have a separate TRD/roadmap contract.
- Arrows move; Shift+arrows extend. Tab and Enter traverse as established by the current selection. Home/End and page navigation follow existing spreadsheet conventions.
- Row and column headers select their respective data region. Their keyboard equivalents are visible in menus. Selecting an entire source column uses current-result bounds for data statistics rather than querying empty worksheet space.
- Drag a column boundary to resize. A six-unit boundary target is independent of the header filter target. Double-click fits content using an explicitly described sample or full-data operation; it must not silently imply a whole-table scan.
- The column menu offers an exact width input and Auto-fit. Keyboard resizing is available without a pointer. Show the proposed width while adjusting; Escape cancels the adjustment.
- Grid, source result, and embedded objects retain separate identities. Imported data is not edited in place by an accidental keystroke; any future overlay-edit model must be designed explicitly.
- Selection and column-width changes are presentation changes. They do not append data transformation steps.

## Field navigation

Fields is available independently of visual-analysis editing. The list names its source. A table with hundreds of columns remains navigable even when only a few fit in the viewport.

| Context and action | Result |
| --- | --- |
| Browsing: single-click field or Enter | Reveal that source column; retain the existing data selection and active row; highlight its header as a navigation cue |
| Browsing: already-visible field | Keep the viewport stable; highlight the header |
| Browsing: frozen field | Highlight the frozen header; do not scroll the unfrozen area |
| Field menu > Center column | Center it within the scrollable viewport where feasible |
| Field menu > Align column to start | Align after the frozen boundary; clamp at the last column |
| Field menu > Select column | Deliberately select the current-result column and update statistics |
| Field menu > Go to cell | Move the active cell to the revealed column at the active row |
| Search | Filter the field inventory only; preserve worksheet and query state |
| Authoring: single-click field | Focus the available field for assignment; preserve the selected analysis object |
| Authoring: Show in source | Explicitly reveal its source column; if on another sheet, switch only through this explicit action and provide a return path |
| Authoring: drag or Add to… | Assign the field to the indicated shelf/Marks target |

The two click behaviors are indicated by context: authoring has visible shelves and Marks; browsing has the address/value strip. A tooltip and accessible description explain Reveal column or Choose field. Field menus expose both navigation and assignment where applicable.

Field navigation cannot trigger an implicit whole-column statistics query. Missing fields remain identifiable in affected definitions, with a repair action; they are not silently rebound by display-name similarity.

## Workbook navigation

- Sheet entries are vertical on the left, with the complete name available on focus/hover. The active entry stays visible when changing sheets through the keyboard.
- Each sheet remembers its viewport, selection, canvas zoom, and object placement. Switching updates the field-source context and any eligible pinned inspector without mixing sheet state.
- Workbook and Fields have separate visibility controls. Hiding Workbook does not hide Fields, and vice versa. With neither visible and no authoring controls required, the left dock disappears.
- New sheet is a stable separate action. Reorder provides a visible insertion point and equivalent Move up/Move down commands. Escape cancels drag/reorder.
- Next/previous sheet remains Control+Tab and Control+Shift+Tab on all platforms, including macOS, unless later platform testing finds a conflict.

## Pane transitions

| Event | Prior state | Result |
| --- | --- | --- |
| Explicit View > Panes > Transformations or its shortcut | Dismissed | Expand, bind to current source, focus its heading/first relevant control |
| Collapse action | Expanded | Retain a compact dock entry only if the user chose retention; return focus to invoker/grid |
| Close action | Expanded or retained | Dismiss and remove its entry; reclaim width |
| Last retained pane closes | Any right-pane state | Remove the entire right rail |
| Activate retained entry | Collapsed | Expand without changing analytical selection |
| Explicit same-pane toggle | Expanded | Close and return focus |
| Select an eligible object | Contextual pane allowed | Bind the already-open/pinned pane or open according to that pane's preference |
| Select an object after explicit dismissal | Dismissed | Keep dismissed; do not reopen automatically |
| Context disappears | Contextual pane | Hide its contextual body and reclaim footprint unless a meaningful pinned context still exists |

Show/hide is a layout operation and never deletes steps, fields, or an analysis. No pane requires a permanently present rail. Pane widths and registration preferences persist independently from data transformations.

On narrower windows, keep a 640-unit grid target where feasible. If an explicit pane-open action cannot fit the minimum widths, use a labelled temporary overlay rather than silently hiding the opposite dock. Dismiss returns to the previous layout. Refine narrow-window policy against the actual native workflow.

Suggested initial native shortcuts, subject to conflict validation: Primary+Alt+W for Workbook, Primary+Alt+F for Fields, Primary+Alt+T for Transformations, Primary+Alt+I for SQL, and Primary+Shift+F for focus mode. Primary means Command on macOS and Control on Windows/Linux. The browser platform may need visible alternative bindings; they must not redefine the native design silently.

## Transformation inspector and SQL

The inspector is a vertical source-bound chain. Each step has a name, concise parameters, status, and where available an exact result count. A selection in this list is not a data change.

1. Open Transformations: bind it to the selected source/analysis and show the current chain.
2. Select a step: show its parameters; keep the worksheet on the current applied result.
3. Preview step: explicitly enter historical preview; label the worksheet with the step and expose Return to current. A preview cannot be mistaken for the latest result.
4. Edit: show a concrete editable expression/operator/value. Apply is one reversible analytical change; Cancel preserves the definition.
5. Apply a changed definition: keep the last complete result visible with an Updating indication; replace it atomically when the new result is ready.
6. Failure: retain the last successful result and the failed step's error/repair action. Do not display the old result as current without its state label.
7. Inspect SQL: show read-only, selectable, copyable SQL and explicit source/step/object scope. Closing it returns to the inspector or canvas that opened it.

An inspector's filter describes the source's shared result. An analysis filter names its object scope. Changes to a shared source can affect dependent objects; their updating/error states must be visible without forcing the history pane open.

Transient selection, pane state, field searching, and ribbon density never enter the transformation chain. Data sorting/filtering does. Representation or placement changes belong to the relevant object definition and undo history.

## Shared visual authoring

### Enter and exit

- Select an object frame to identify it. Enter or Edit analysis opens its authoring context; creating a view starts a clearly labelled empty object.
- Columns and Rows replace the ordinary address/value context with two horizontal shelf rows. Measures, Marks, and object filters appear on the left.
- Explicitly entering authoring may reveal a previously hidden Fields section for this session. Exiting restores its prior user-chosen visibility. If the user pins it while editing, it stays open.
- Clicking an ordinary source cell exits object authoring, restores the address/value strip, and removes the object-only shelves/Marks. It does not dismiss a Fields section the user independently pinned.
- User-created objects stay anchored on the same sheet. Entering authoring does not replace source data with a separate full-screen pivot page.

### Place, order, and remove fields

- Columns and Rows accept ordered lists, not single fields. A drop insertion marker shows exactly where a new field will go. Measures accepts multiple aggregates, including SUM(Amount) and AVG(Amount) simultaneously.
- Dragging from the available-field inventory adds an assignment and leaves the source field available. Moving an existing shelf token moves that assignment; an explicit Duplicate command makes a second one.
- Each token has a menu for aggregate, date grain where applicable, sort, move, duplicate, and remove. Full field identity is available when a token is shortened.
- Keyboard alternative: focus a field, open Add to…, select a destination, choose position/aggregation, confirm. Tokens expose Move before/after and Move to shelf commands. Remove is also available through the menu.
- Dropping on a populated multi-field shelf inserts. It does not replace the entire shelf. For single-valued targets, show Replace [existing field] before commit; Escape cancels.
- Columns and Rows each allow visible wrapping up to two lines, followed by an explicit More fields control. Overflow is keyboard accessible and reorderable. Rows never rotates into a vertical left shelf.
- Incompatible field/role combinations show a reason and a valid alternative; they do not accept a drag that silently does nothing.

### Measures and Marks

Measures names its aggregate and optional display unit. Matrix cells can show multiple measures. Charts use the same aggregates for axes and encodings. A measure may be referenced from multiple targets without creating ambiguous duplicate definitions.

Label, Detail, and Tooltip accept several fields. Color supports deliberate multi-field assignment with an explicit combined encoding; a plain additional assignment never silently replaces existing fields. Size and Shape are single-valued unless the selected representation defines a meaningful combined mapping.

Marks targets remain distinct from the available-field list. Changing mark type preserves assignments. Unsupported targets are labelled inactive with their reason and recover when a compatible type returns.

### Change representation

Matrix, Bar, Line, and other outputs use the same source, field identity, shelves, aggregates, and filter vocabulary. Changing type previews the resulting layout. A dimension on an axis produces headers/categories; an aggregated numeric field can produce a quantitative axis.

When no unique conversion exists, show the proposed mapping and require the missing axis/measure choice. Keep the previous representation until commit. Cancelling restores it exactly. No silent dropping of row hierarchy, measures, sorting, filters, or encodings is allowed.

Matrix formatting, chart axes, cell selection, and mark selection remain specific to their representations. A shared builder does not imply that these outputs have identical semantics.

## Header filtering and sorting

- The header menu is anchored to its column and names that field. Sorting and filtering share that context, with explicit ascending/descending actions and type-appropriate operators.
- Sorting shows direction and priority in the header. Add to sort is explicit; users can inspect and reorder a multi-field sort. Ties retain or expose a defined ordering policy rather than implying a stable order where none is specified.
- The filter menu includes search, Select all with partial state, typed values, blanks/NULL handling, conditions, Apply, Clear, and Cancel. Dates use expandable calendar/time hierarchy where appropriate.
- Search selects available values, not current viewport values. Paginated results do not impose a hidden distinct-value limit. The menu states the domain and whether other filters constrain it.
- Checklist changes are a draft until Apply. Escape/Cancel discards the draft, keeps the applied filter, and returns focus to the header. Reopening shows the committed state.
- Applying source filters appends/updates reproducible source steps, updates the result count, and updates dependent objects. It does not open history automatically.
- While fetching/filtering, show a local pending state. Cancel and stale-response protection prevent an older request from overwriting a newer one.

## Selection statistics

### Scope and definitions

The menu explicitly names the scope, such as Selected result cells. Statistics cover the entire selected result range, including off-screen rows; they must not be computed only from the currently cached viewport. Filter-excluded rows are outside the current result. Source headers are excluded from source-data statistics. Overlapping selection ranges contribute each cell once.

Whole-source row/column selection intersects the current-result bounds. On manual sheets, the explicitly selected cell rectangle defines scope, including its empty cells. Selections spanning different result objects are labelled Mixed selection rather than silently combining raw data with aggregated output.

| Metric | Definition |
| --- | --- |
| Count of cells | Number of selected data-cell positions in the scoped range, including blank positions |
| Filled cells | Non-NULL cells whose value is not an empty string; whitespace-only text counts as filled |
| Numeric count | Typed finite numeric values; zero counts, numeric-looking text/booleans/dates do not |
| Distinct count | Unique nonblank typed values; case-sensitive text by default; numeric 1 and 1.0 compare as the same numeric value, text "1" remains different |
| Sum | Exact sum of finite numeric values; nonnumeric values ignored; unavailable if there are no numeric cells |
| Average | Sum divided by numeric count; round only for display |
| Minimum / Maximum | Minimum/maximum finite numeric value; unavailable without numeric cells |

NULL and an empty string are both blank for these metrics, although their distinctions remain available in data inspection. Non-finite numeric values are excluded from numeric arithmetic and reported through a concise Unavailable values detail rather than silently included as zero. Mixed currency/unit arithmetic is labelled Mixed units and not shown as one monetary total without an explicit normalization definition.

For matrix content, scope is the selected rendered cells, including selected subtotal/grand-total cells once each; it does not claim to represent underlying transaction totals. A matrix frame selection shows object identity and measure count rather than stale cell statistics. Chart marks show Marks selected and summaries of their displayed values; any underlying-row inspection is a separate explicitly scoped action.

### Customization and calculation states

- Right-click anywhere on the bar opens the customization menu. A keyboard context-menu action is available when the status region is focused; View > Status bar statistics provides an additional path.
- Each metric is a checkbox item. Space toggles without closing the menu; Escape returns to the invoking status item. Preferences persist at application level.
- Count of cells, Filled cells, Numeric count, Distinct count, Sum, Average, Minimum, and Maximum are independently toggleable. No duplicates with vague names such as Count and Count2.
- Default reference visible metrics: Filled cells, Sum, and Average when applicable. An unavailable enabled metric displays an em dash with an accessible reason. Users can disable every selection metric.
- Calculation states are Idle, Calculating, Exact, Unavailable, Cancelled, and Error. Selection changes invalidate earlier calculations immediately. An old number must never appear to describe the new selection.
- Large-range work uses the engine/worker and cancellation; the UI remains responsive. Exact distinct counts can remain Calculating longer. An approximate result must be explicitly requested and labelled approximate; no silent sampling.
- Right-click customization never modifies the worksheet or adds transformation steps. Copy metric value is an explicit menu action with unrounded/full-precision value where meaningful.
- The bar is one line. Overflowed metrics stay in a reachable menu and retain preferences. Status and zoom do not push statistics into a second row or overlap.

## Ribbon and keyboard accessibility

Full, Compact, Icons only, and Focus are explicit display modes. Commands preserve identity, logical groups, disabled reasons, and keyboard access across modes. Formatting state follows the selected cells/object; it does not change just because ribbon density changed.

F6/Shift+F6 cycle visible regions in visual order: ribbon, Workbook, Fields, authoring controls, canvas, right inspector, status. Hidden regions are skipped. Within a control group, use arrow navigation; Tab moves between actionable controls according to platform conventions. Focus is always visible and never represented only by color.

Alt+Down opens a focused column's menu. Context-menu key/Shift+F10 provides the same menu where the platform supports it. Every drag operation has named non-drag commands. Escape first cancels the innermost draft/popup, then leaves the current editing context; it does not silently discard committed analysis changes.

Focus mode records the previous layout and temporarily hides ribbon/docks. Keyboard navigation remains available; the status bar stays thin and visible. An explicit Exit focus command and Escape from the canvas restore the exact previous layout. Showing a pane during focus is a temporary overlay unless the user exits focus or pins it into the restored layout.

Tooltips include command name, current shortcut, and where necessary an unavailable reason. Row/column, aggregation, sort order, checked state, selection count, and asynchronous completion are represented in accessible text as well as visuals. Full native accessibility integration is a separate implementation requirement to validate for raylib; keyboard support alone must not be described as screen-reader support.
