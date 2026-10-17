# Duxcel design references — 16 October 2026

Direction: Excel's continuous worksheet canvas and compact command hierarchy, combined with Tableau's independently visible fields and spatial analysis shelves. Duxcel uses its own dark blue accent and code-drawn line icons. Reference images are studied in the browser, not copied into the product.

The current Duxcel chat and original attached brief were reviewed against the hands-on feedback. The resulting execution checklist is at the top of ROADMAP.md. The requested contextual pivot behavior is a product requirement: selecting a pivot/template should reveal its field list and spatial shelves; selecting an ordinary range should restore the cell strip. The current summary dialog is an intermediate working slice, not completion of that requirement.

## Reviewed Tableau screens

Official documentation contains several generations of Tableau styling. These references establish interaction and spatial hierarchy; they are not all evidence of the newest product skin.

| Screen | What it informs |
|---|---|
| [Annotated workspace](https://help.tableau.com/current/pro/desktop/en-us/Img/environ_workspace1.png) | Persistent data pane, independent shelves, dominant working view, bottom sheets |
| [Blank view/drop zones](https://help.tableau.com/current/pro/desktop/en-us/Img/dropFieldHere.png) | Empty state as a useful authoring surface |
| [Shelf construction, first state](https://help.tableau.com/current/pro/desktop/en-us/Img/wwd_shelf_cr1.png) | Rows and Columns as separate spatial destinations |
| [Shelf construction, second state](https://help.tableau.com/current/pro/desktop/en-us/Img/wwd_shelf_cr2.png) | A field remains legible as a pill after placement |
| [Shelves and Marks](https://help.tableau.com/current/pro/desktop/en-us/Img/build_manual_shelves_marks2.png) | Avoid squeezing fields, roles and properties into one selector |
| [Filter shelf state 2](https://help.tableau.com/current/pro/desktop/en-us/Img/filter_shelf2.png) | Filters have an explicit place in the analytical definition |
| [Filter shelf state 3](https://help.tableau.com/current/pro/desktop/en-us/Img/filter_shelf3.png) | Visible filter choices rather than cycling through hidden options |
| [Filter shelf state 5](https://help.tableau.com/current/pro/desktop/en-us/Img/filter_shelf5.png) | Selection controls alongside an intact view |
| [Pages interaction](https://help.tableau.com/current/pro/desktop/en-us/Img/pages7.png) | Secondary controls remain subordinate to the working area |
| [Getting-started blank workspace](https://help.tableau.com/current/guides/get-started-tutorial/pt-br/Img/gs_Drag28.png) | Full available-field list independent of current shelf assignments |

Supporting interaction descriptions: [workspace](https://help.tableau.com/current/pro/desktop/en-us/environment_workspace.htm), [shelves/cards](https://help.tableau.com/current/pro/desktop/en-us/buildmanual_shelves.htm) and [dragging fields](https://help.tableau.com/current/pro/desktop/en-us/buildmanual_dragging.htm).

## Reviewed Excel screens

The recent full-window references inform ribbon density and worksheet proportions. Older Microsoft filter close-ups inform behavior only; their styling is not treated as current Microsoft 365 design.

| Screen | What it informs |
|---|---|
| [Modern Excel for Mac worksheet](https://alexbeaver.com/content/images/2024/05/DraggedImage.png) | Quiet blank workbook, separate formula strip, compact ribbon and sheet strip |
| [Excel web ribbon/add-in entry](https://ibm.github.io/ibm-envizi-emissions-api-excel-addin/_images/appstore-ribbon-addin.png) | Command groups, clear icons, sparse use of accent color |
| [Mac PivotTable/value filter, Microsoft Community](https://techcommunity.microsoft.com/t5/s/gxcuf89792/images/bS00MDM1MjA0LTU0NDA0OGk5QTBFNEM3MjA5RjcyQjQx?revision=1) | Readable visible operators plus a value checklist; abundant worksheet beyond the pivot |
| [Pivot and chart in one worksheet, Microsoft Q&A](https://learn-attachment.microsoft.com/api/attachments/d6c08d13-669f-41a3-a415-788dc2ee6744?platform=QnA) | Several objects can coexist; source/pivot output must not replace the worksheet |
| [Pivot field areas](https://support.microsoft.com/en-us/excel/media/pivottable-fields-in-excel-for-the-web.png) | Fields and role assignments are different concerns; Duxcel should spread the roles spatially |
| [Number-filter dropdown](https://support.microsoft.com/en-us/excel/media/number-filters.jpg) | Enumerated operators, not “click to change” |
| [Custom filter dialog](https://support.microsoft.com/en-us/excel/media/custom-autofilter-box.jpg) | Inputs need explicit operators and clear application/cancellation |
| [Table filter checklist](https://support.microsoft.com/en-us/excel/media/o15-xl-table-filters.jpg) | Column-anchored search, all/individual values, Apply/Clear |
| [Filter gallery](https://support.microsoft.com/en-us/excel/media/o15-xl-filtergallery.jpg) | Sorting and filtering belong to the selected column |
| [Date hierarchy screenshot](https://global.discourse-cdn.com/uipath/original/4X/7/4/0/740086f4bfbd654633cb9efe2e3e76fd2e73fdfc.png) | Expandable year/month branches and partial selection |

Supporting descriptions: [filtering](https://support.microsoft.com/en-us/excel/get-started/filter-data-in-a-range-or-table-in-excel), [PivotTable destination](https://support.microsoft.com/en-us/excel/get-started/create-a-pivottable-to-analyze-worksheet-data), [ribbon collapse](https://support.microsoft.com/en-us/office/foundations-experiences/show-or-hide-the-ribbon-in-office), [keyboard shortcuts](https://support.microsoft.com/en-us/accessibility/excel/keyboard-shortcuts-in-excel).

## Decisions and acceptance

- Default screen is a blank worksheet. Extra source/step panels are opt-in; no development Demo/Diagnostics command in the ordinary ribbon. Remove redundant content title rows.
- File/Data/View tabs have a moving active indicator; commands have outlined surfaces, recognizable line icons and separate hover/pressed/disabled states. Menu shadows provide modest elevation. No flat green command background.
- Workbook sheet tabs scroll independently. The new-sheet target stays fixed and cannot reuse the same click that switched sheets.
- Application scale is in Settings. Canvas zoom is per sheet and responds to Control+wheel, pinch and status controls without changing ribbon dimensions.
- Current source filters enumerate real values through DuckDB. A bounded virtual list, search cancellation and typed lazy date branches replace hidden cycling selectors. One active source filter is the present model limit.
- Pivot authoring must eventually happen in context on the same sheet, with a left field list and distinct Rows/Columns/Values/Filters areas. The working Rows/Values summary uses genuine DuckDB PIVOT, but anchored multi-object placement and two-dimensional ON pivots remain the next vertical slice.
- Design completion requires native inspection across narrow/wide windows and 80/100/150% interface scales, independent canvas zoom, keyboard focus and actual drag gestures. A palette change or static screenshot alone is insufficient.

Implementation evidence and remaining limits are in BENCHMARKS.md and ROADMAP.md. The image review provides about twenty varied screens across both products; it is not a claim that forty unique current-version screenshots were inspected.
