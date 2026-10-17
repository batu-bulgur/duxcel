# Duxcel interaction study

[Open the local prototype](http://localhost:4173/)

A disposable browser workspace for refining Duxcel's UI before native implementation. It uses React/CSS for rapid design iteration, actual IBM Plex Sans font files, Carbon icons, and illustrative financial transactions. The product remains C99 + raylib + DuckDB.

## Try these interactions

- Select, drag, or Shift-extend a cell range. Use arrows and Shift+arrows. Drag a column boundary, double-click to fit, or focus its separator and use Alt+arrows.
- Open a header's filter menu. Search, change the checklist, Apply or Cancel, or choose a sort direction. Alt+Down opens the selected column's menu.
- Click a field in the left inventory to reveal its source column while retaining the selection. Search filters this inventory only.
- Switch sheets in the vertical Workbook list; drag entries to reorder. Each example sheet retains its selection, source filters, analysis definition and scroll position during the session.
- Choose Create view. Drag fields to the stacked Rows/Columns shelves, Measures, Marks, or view filters. Reorder shelf tokens with insertion feedback. Token/field menus provide assignment alternatives.
- Switch Matrix, Bar chart, and Line chart from the same authoring controls. Return to data explicitly or click a source cell.
- Open Transform. Resize its pane, collapse it to a retained entry, then dismiss it completely. Inspect SQL and try a clearly labelled historical preview.
- Right-click the thin status bar, or use its ellipsis, to toggle eight selection statistics. Hidden metrics remain available with their sample values in this menu.
- Try Full, Compact, Icons only, and Focus from Ribbon display. View contains pane recovery, row density, interface scale, and Reset study. Escape restores focus mode.
- UI scale (80–150%) changes the whole interface; the adjacent zoom control (60–160%) changes the source grid only. F6 cycles visible regions; Control+Tab changes sheets.

The interaction guide inside the prototype lists the principal shortcuts. Reset study restores the canonical design baseline, including pane sizes, sheet order, sample filters, selection, scale, and ribbon. Ribbon density and metric preferences persist locally between reloads; the remaining design state is session-only. Save downloads a design-state JSON for inspection, not a production workbook.

## Motion and responsive rules

Pane width: 160 ms; pane opacity: 120 ms; popup fade: 110 ms; popup translation: 120 ms; button feedback: 90 ms; contextual shelves: 140 ms. Easing is `cubic-bezier(.22,.8,.25,1)`. Pane resizing tracks the pointer immediately. Charts have no decorative motion. Reduced-motion CSS removes animations and transitions.

Ribbon groups progressively collapse and then enter named overflow; every group can open its complete controls. Display/help and Save remain reachable. The left dock becomes a recoverable overlay below 700 logical units. The right pane overlays below 1040 logical workspace units. The matrix and chart stack below 880 canvas units. Source preview height adapts to the canvas height. Short docks retain field browsing with explicit scrolling. Popover coordinates account for interface scale and clamp to the viewport. The status line measures complete labels and fits as many as the available space allows. Frozen cells always cover scrolling selection, including its active border and handle. Very narrow views offer an explicit Unfreeze action when frozen columns consume the usable grid width.

## Scope and limits

There are 310 synthetic source rows and normally 240 filtered example rows, rather than a pretend 48,000-row engine. Basic browser filtering, sorting, grouping and statistics support convincing UI changes. Charts and matrix summaries illustrate the authoring concept. SQL and transformation history are inspection examples; they do not constitute a reproducible query backend. Some Marks assignments change visible tokens without implementing their full rendering semantics. New sheet creates another example analysis rather than a blank spreadsheet.

The study does not validate financial arithmetic guarantees, DuckDB performance, WebAssembly integration, native accessibility, raylib font rendering, arbitrary object placement, full workbook persistence, or an exhaustive keyboard workflow. Disabled paste/cut/wrap controls explain their limits through tooltips. No C99/raylib/DuckDB implementation changes were made by this prototype pass.

[Browser validation evidence](design-qa.md), [product design](../../DESIGN.md), [interaction rules](../INTERACTIONS.md), and [validation scope](../VALIDATION.md) are separate from final user approval. A visual QA pass does not establish that the interaction design is accepted.

## Development

The live server is already running at the link above. For a later session, use `npm run dev -- --host 0.0.0.0 --port 4173 --strictPort` in this folder. `npm run build` produces the local build and preserves the template's optional hosting package. No publication has been performed.
