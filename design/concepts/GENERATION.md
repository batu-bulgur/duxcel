# Duxcel revised concept generation

Generated 16 October 2026 with the built-in Image Gen tool. The final saved concepts are `analysis-workspace-v1.png` and `data-inspection-v1.png`. They are visual studies, not functional prototypes or measurements of the actual typeface.

## Reference inputs

The analysis generation received the two displayed Duxcel concepts called Workbench and Canvas and a captured official Tableau annotated workspace. These were visual grounding for a combined design. The active user brief overrode their typography, bottom navigation/history, and pivot configuration.

The corrected analysis generation received the preceding analysis image as its edit target. The data-inspection generation received that corrected analysis image as its visual reference, to show another state of the same design.

Official spatial reference: [Tableau annotated workspace](https://help.tableau.com/current/pro/desktop/en-us/Img/environ_workspace1.png).

## Analysis generation brief

Create one high-fidelity 1440×1024 Duxcel desktop screen, current date 16 October 2026, synthetic Q3 Treasury Review.dxls workbook, IBM Plex Sans throughout. Native C99 + raylib + DuckDB product; image-only design experiment. Cool-light surfaces, dark graphite, restrained deep-blue selection and teal measure accents, compact 12–13px type, square geometry, fine gridlines. Avoid SaaS dashboards, oversized cards, decorative whitespace, marketing labels, browser/device framing, and excessive rounding.

Combine vertical Workbook navigation with an independently usable Fields section on the left. Show four sheets: Transactions, Monthly cash flow, Counterparties, Reconciliation. Below Fields, show Marks, Matrix representation, two measures SUM(Amount) and COUNT(Transaction ID), and separate encoding targets and filters.

Show a full Office-style ribbon: Home, Insert, Data, Analyze, View, contextual Visualization. Group Clipboard, Font, Alignment, Number, Data, Analysis commands. IBM Plex Sans and size 13 should be visible in formatting controls. Expose Ribbon display for smaller modes.

Show two full-width horizontal shelves stacked above the canvas: Columns with YEAR(Date), QUARTER(Date), MONTH(Date); Rows with Account, Category. Both have room to add more fields. Do not use a one-line Rows/Columns/Values/Filters form or a right pivot checklist.

Keep source transactions above an embedded matrix and neighboring chart on the same numbered worksheet. Preserve data-grid density, right-aligned financial amounts, typed headers, sort/filter affordances, and thin scrollbars. The matrix is the active selected object. The right inspector and its rail are completely dismissed. The bottom is one thin status bar, with no sheet tabs or history strip.

## Exact final analysis correction prompt

Edit the attached Duxcel Analysis workspace screenshot. Preserve its entire design, high fidelity, IBM Plex Sans typography, exact cool-light palette, Office-style full ribbon, vertical workbook and fields sections, left Marks and Measures, stacked HORIZONTAL Columns and Rows shelves, embedded matrix/chart geometry, all source data, no right pane or rail, and no bottom sheet tabs or transformation timeline. Target 1440 x 1024. Current date 16 October 2026. This is a focused correction of selection and status, not a redesign.

Make ONLY these corrections:

1. The source table cell range F7:F10 must NOT show any blue selection fill, blue outline, blue corner handles, or active cell. Those cells should appear ordinary white like surrounding cells with the same existing values. The embedded matrix below is the ONE active selection, preserve its thin blue object border/handles. This avoids simultaneous conflicting active selections.
2. Reduce the entire bottom status bar to a precisely THIN 22 logical pixels tall, a single aligned text baseline at 12px IBM Plex Sans. It must feel like Excel's thin status line, not a toolbar or large footer. Extend worksheet background to fill the reclaimed height above it.
3. Bottom status text must read only "Ready | Synthetic data | 36,984 of 48,216 rows" at left, "Matrix selected · 2 measures" toward right, and compact minus, "100%", plus at far-right. REMOVE the existing "8 rows × 6 columns", "Count: 4", "Sum: (40,724.62)", and "Average: (10,181.16)" entirely. No cell selection means no cell selection statistics in this state.
4. Refine field tokens in Measures: still two rows SUM(Amount) and COUNT(Transaction ID), 2–3px corners rather than fully rounded capsules.

Do not alter any other geometry, labels, workbook sheets, font family, source table, chart placement or visual theme. No additional panels or narrative.

## Data inspection generation brief

Create a companion 1440×1024 screen of the same design with IBM Plex Sans, palette, workbook, and vertical Workbook/Fields dock preserved. Show ordinary source browsing rather than matrix editing. Use a compact icons-and-labels ribbon, an address/value strip, no authoring shelves or Marks, and roughly 32 visible transaction rows.

Open a 280-unit right Transformations inspector for Transactions, with a vertical six-step list: Import transactions.parquet; Set column types; Filter Status = Posted; Filter Currency = USD; Derive Month from Date; Sort Date ascending. Select the third step only for its properties; show separate Edit and Preview controls and an Inspect SQL action. Do not put history in the status area.

Select F7:F10, with amounts -12500.00, -18340.00, 32800.00, -2184.62. The active cell is F7. Require Count/Filled/Numeric/Distinct = 4, Sum = -224.62, Average displayed = -56.16. The context strip should show the signed active value. Source data uses September 2026 dates, realistic synthetic counterparties, categories, accounts, and statuses.

Show one 22-unit status bar with Ready, Synthetic data, result count, enabled statistics, and compact zoom. Open its right-click Selection statistics menu above the bar: checked Count of cells, Filled cells, Numeric count, Distinct count, Sum, Average; unchecked Minimum, Maximum; scope Selected result cells. No horizontal sheet tabs, timeline, breadcrumbs, metrics cards, or large footer.

## Interpretation

The generation briefs express intent. The resulting images still contain discrepancies listed in [VALIDATION.md](../VALIDATION.md). Exact typography, status height, cell values, numbering, and aggregates must be checked in a real prototype rather than treated as demonstrated by the generated pixels.
