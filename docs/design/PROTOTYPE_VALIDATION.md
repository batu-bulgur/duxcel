# Duxcel design validation

Updated 16 October 2026. This record separates a working UI experiment, observed interaction evidence, future product requirements, and user approval.

## Current scope

The user explicitly asked to validate the UI rather than a calculation engine. Motion, panes, drag/drop, cell selection, control proportions, density, scaling, and window adaptation are the acceptance focus for this stage. Synthetic examples make the transitions legible. Financial/query correctness, DuckDB scale, and WebAssembly integration are outside this experiment's gate.

[Open the prototype](http://localhost:4173/) · [Scope and controls](../../design/prototype/README.md) · [Full visual QA record](../../design/prototype/design-qa.md).

| Item | Status and evidence |
| --- | --- |
| IBM Plex Sans decision | Confirmed by user; Inter superseded |
| Actual font in browser | Local font files; computed grid family is IBM Plex Sans; `document.fonts.check` true; default grid 13 units |
| Revised compositions | Analysis and inspection concepts implemented as a coherent UI study; user review remains pending |
| Core source interactions | Pointer selection/Shift extension, keyboard extension, resizing, sorting, filtering exercised in browser |
| Pane lifecycle | Open, resize, collapse to retained entry, complete dismissal and recovery exercised |
| Visual authoring | Multi-field horizontal shelves, pointer drop, same-shelf reorder, shared Matrix/Bar controls exercised |
| Workspace navigation | Sheet switching and pointer reordering exercised; independent sheet example state is implemented |
| Ribbon and scale | Full/compact/icons/focus modes; interface scale 80/100/125/150%; independent grid zoom sampled |
| Responsive layout | 1280×720, 1280×800, 1440×1024 and 1920×1080 sampled in the in-app browser; no page horizontal overflow in checked states |
| Browser health | No warnings or errors in final checked browser console; production build and five existing fixture checks passed |
| Motion | Pane/popover/button/authoring transitions implemented and observed through interaction; timings recorded; no frame-time benchmark |
| Reduced motion | CSS override inspected; OS preference emulation not performed |
| Native architecture | No production C99/raylib/DuckDB code changed in this prototype pass |
| Final design approval / native handoff | Pending user review; visual QA is not approval |

## Responsive refinement — current iteration

The user's latest review identified selected cells painting above frozen columns and requested finer toolbar/icon placement and viewport adaptation.

- Frozen scrolling-cell regression: at horizontal scroll 504, the selected amount cell occupied x472–618 and overlapped the frozen boundary at x526. A hit at x518 returned Transaction ID (column 1), not the active amount cell. Relative layers are active scrolling 1, frozen 10, header 30.
- Home ribbon: 24 width/mode combinations sampled across Full/Compact/Icons and 360/416/640/768/1024/1280/1440/1920 widths. No toolbar or page overflow; group strips remained before the action dock. [Measurements](../../design/prototype/qa/iteration-responsive-checks.json).
- All five ribbon tabs: another 15 checks at 416/768/1440, all fitting. [Measurements](../../design/prototype/qa/iteration-tab-checks.json).
- Group flyouts: Font/Number full controls, named overflow, formatting, and Data → Filter exercised. Resizing away an open anchor closed the flyout and focused its replacement.
- At 360×900/150% UI, Font flyout and status customization remained within the viewport. Whole metrics moved out of the line when none fit. The grid offered Unfreeze when frozen columns consumed the useful width; activating it removed both the hint and frozen cells.
- Narrow navigation: explicit overlay open, source field reveal, close, and Escape from focused field search passed after a clean load. Right inspector dismissal removed the whole rail. Section visibility restored at wider widths.
- Full-ribbon controls were measured inside their 61–117 content band; dedicated vertical group gaps prevent controls from colliding with captions.
- The final production build passed. A clean browser load produced no new warnings/errors. An earlier stopped-server reconnect message was historical and did not recur after restarting preview.

[Wide comparison](../../design/prototype/qa/iteration-wide-comparison.jpg), [narrow comparison](../../design/prototype/qa/iteration-narrow-comparison.jpg), [toolbar detail](../../design/prototype/qa/iteration-toolbar-comparison.jpg), and [full-control flyout](../../design/prototype/qa/iteration-flyout-comparison.jpg) were viewed as combined inputs. The current QA report supersedes the first pass; prior evidence remains retained.

## Observed interaction results

| Scenario | Input and observed result |
| --- | --- |
| Select and extend | Clicked financial cell then Shift+Down; range contained the two expected visible cells with one anchor. Pointer/Shift selection and pane dismissal retained the original F7:F10 range. |
| Column resizing | Counterparty resized from 184 to 242 logical units through its six-unit boundary target. |
| Sort | Amount descending changed the first sample amounts to 128,450; 74,200; 47,900; 32,800. Header context persisted. |
| Filter | Account checklist changed to Operating and Apply; visible sample result changed to 174 rows, with visible Account cells Operating. |
| Field reveal | Memo click moved horizontal scroll to 504 while retaining the selected amount cells. Frozen columns remained on the left. |
| Field drop | Pointer drag Region → Rows added a third assignment and retained the field in the inventory. |
| Token reorder | Moving Account to the end of Rows produced Category, Account. An earlier insertion-index bug was fixed and retested. |
| Sheet reorder | Moving Transactions before Counterparties produced Monthly cash flow, Transactions, Counterparties, Reconciliation. Reset study now restores original sheet order. |
| Pane width | Dragging left separator changed width 228 → 268 immediately. Right pane and column sizing also expose keyboard separators. |
| Dismissal | Collapse retained a 32-unit entry; dismissing removed the right rail. Hiding Workbook and Fields reclaimed the entire left width; View restored them. |
| Shared authoring | Matrix → Bar chart preserved Columns Year/Quarter/Month and Rows Account/Category; the active frame changed coherently. |
| Focus mode | Temporarily removed ribbon and docks; Escape restored authoring and the right inspector; status remained 22 units. |
| Status | Distinct-count preference updated the line; all eight independent options appear with values in customization; line remains single-height. |
| Scale/zoom | Status was 22 at 100% UI, 27.5 at 125%, 33 at 150%, and about 17.6 at 80%. Grid zoom to 110% changed rows to about 26.4 while status stayed 22. |
| Scaled menu | 150% UI at 1280×720 kept Date filter inside the viewport; Apply/Cancel remained visible. 125% ribbon menu also remained bounded. |
| Short workspace | Fields retained a 105-unit scrollable inventory at 1280×720/100%. At 1280×800 the 772-unit canvas stacked the two analysis objects at 739 units wide. |
| Keyboard | Shift+arrow selection, Alt+Down column menu, Escape dismissal/restoration, and F6 region navigation exercised. Guide Shift+Tab/Tab wrapped between its last and first controls. |

These are sampled browser observations, not an exhaustive usability study or cross-platform certification. A person still needs to judge effort, predictability, and comfort in sustained use. Drag speed, perceived motion, tiny metadata, crowded shelves, and viewport tradeoffs are the next review topics.

## Synthetic content and engine limits

The prototype starts with 310 synthetic source rows, normally 240 after the example Posted/USD filters. Authoring shows a labelled 12-row source preview. Counts describe this actual sample, not the large numbers in generated concepts. Lightweight browser routines support visible sorting, filtering, sample summaries and selection statistics. Charts show three large groups plus Other to keep their legend readable.

The transformation list and SQL are inspection examples. Some encodings expose assignment states without fully implementing their analytical rendering. This is intentional under the user's UI-only scope. Engine-backed replay, exact financial types, cancellation, query errors and very large selections remain future work. The existing five fixture checks passed to keep examples plausible; they do not establish engine accuracy.

### Retained financial fixture

F7:F10 in the canonical source view contains −12,500.00; −18,340.00; +32,800.00; −2,184.62 USD. The expected example sum is −224.62 and displayed average −56.16. An early generated concept's −40,724.62 total was incorrect and is superseded. Generated images are not arithmetic or typography evidence.

## Visual evidence

- [Analysis browser capture](../../design/prototype/qa/analysis-final.jpg) and [inspection browser capture](../../design/prototype/qa/inspection-final.jpg), both 1440×1024 at 100% UI and canvas zoom.
- [Full analysis comparison](../../design/prototype/qa/analysis-comparison-final.jpg) and [full inspection comparison](../../design/prototype/qa/inspection-comparison-final.jpg) contain source and implementation in the same input.
- [Grid type and selection](../../design/prototype/qa/grid-type-comparison.jpg), [shelves and headers](../../design/prototype/qa/shelves-type-comparison.jpg), and [inspector/status](../../design/prototype/qa/inspector-status-comparison.jpg) allow small-detail comparison.
- Actual font [specimen](typography/ibm-plex-sans-specimen.png) and [provenance](typography/README.md) remain useful for glyph inspection. Browser evidence adds actual UI placement; native raylib text rendering remains untested.

The concepts are 1487×1058 and were normalized to 1440×1024 before comparison. The implementation captures are actual 1440×1024 browser pixels at device pixel ratio 1. Generated imagery's chrome heights, font outlines, inconsistent rows, data values and provisional branding do not override the human's density rules or the actual-font requirement. Sharp browser captures were used for the final comparisons; a lower-quality screenshot export was replaced before judging type.

## Future product validation

After design approval, validate C99/raylib focus and accessibility integration, text rasterization at physical display scales, full keyboard workflows, edge auto-scroll, long/many-field overflow, drag cancellation and invalid destinations, free placement/resizing of embedded objects, native file persistence, multi-sort, large distinct-value/date domains, and state restoration across every sheet and context.

Engine-specific acceptance is separate: exact decimal semantics; selection over the full result rather than a viewport cache; cancellation and stale-response protection; reproducible transformations and dependency refresh; source versus object filter scope; explicit conversion mapping where matrix/chart semantics differ; current/preview/error states; and mixed currency/unit handling. These requirements must not delay the present UI experiment or be claimed as validated by it.

Dark appearance, high-DPI native screens, touch input, other browser engines, and WebAssembly packaging remain untested. Do not infer their quality from the light desktop browser prototype.

## Approval and handoff

The next gate is user review of the running UI. Record accepted measurements, motion, keyboard conventions, pane behavior, and unresolved questions before producing a final native implementation handoff. Web libraries, DOM structure, sample calculations and illustrative SQL are disposable; port the accepted interaction rules into C99 + raylib + DuckDB boundaries, with document/data state separate from view, focus and transient gestures.
