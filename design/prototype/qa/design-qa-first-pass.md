# Duxcel prototype design QA

final result: passed

Checked 16 October 2026. This is a pass for the disposable UI study's visual and sampled interaction scope. It is not design approval, engine validation, or native accessibility certification.

## Findings

No actionable P0/P1/P2 findings remain in the checked scope. Earlier P2 findings, corrections, and post-fix evidence are recorded below. Residual test gaps and optional polish remain explicit.

## Comparison target and normalization

- Source visual truth: `../concepts/analysis-workspace-v1.png` and `../concepts/data-inspection-v1.png`, interpreted alongside the human's IBM Plex Sans, density, pane, and UI-only scope decisions in `../../DESIGN.md` and `../INTERACTIONS.md`.
- Implementation: `http://localhost:4173/`.
- Browser screenshots: `qa/analysis-final.jpg` and `qa/inspection-final.jpg`.
- Comparison viewport: 1440 × 1024 CSS pixels, device pixel ratio 1, interface scale 100%, canvas zoom 100%, light appearance.
- Source pixels: 1487 × 1058, normalized to 1440 × 1024. The slight aspect difference is less than 0.1%; no browser chrome or device frame is included.
- Implementation pixels: 1440 × 1024. Computed body/root zoom is 1, visual viewport scale 1. Actual browser family is IBM Plex Sans and font loading check succeeds.
- Analysis state: Home/full ribbon; Transactions active; Fields/Workbook/Marks visible; Columns Year/Quarter/Month; Rows Account/Category; two measures; matrix selected, chart beside it; right inspector dismissed.
- Inspection state: Home/compact ribbon; Transactions; F7:F10 selected; Fields available without authoring; Transformations open; status customization open with Count/Sum/Average enabled.
- Content difference: the generated mock has inconsistent rows, matrix geometry and financial values. The prototype uses plausible sample content with honest sample labels. Identical data content cannot be normalized from the generated image and is not asserted.

## Full and focused visual evidence

Both source and implementation were placed in the same comparison input and viewed before disposition.

- Full analysis: `qa/analysis-comparison-final.jpg`, 2910 × 1064.
- Full inspection: `qa/inspection-comparison-final.jpg`, 2910 × 1064.
- Grid typography/selection: `qa/grid-type-comparison.jpg`, 1838 × 380.
- Shelves/headers: `qa/shelves-type-comparison.jpg`, 1650 × 250.
- Inspector/status: `qa/inspector-status-comparison.jpg`, 670 × 994.
- Alternate viewport evidence: `qa/analysis-1280-icons.jpg` at 1280 × 800; `qa/scale-150-short.jpg` at 1280 × 720. These retain the earlier screenshot export quality; geometry was also inspected directly in browser DOM measurements.

An initial lower-quality screenshot export softened text. Final comparison inputs were rebuilt using the browser wrapper's sharp capture. `qa/analysis-full-resolution.jpg` also retains the sharp analysis capture. This was an evidence-export issue; computed font, interface zoom and actual rendering were checked independently.

## Required fidelity surfaces

| Surface | Evaluation and disposition |
| --- | --- |
| Fonts/typography | Actual local IBM Plex Sans Regular/Medium/Semibold and matching italics replace generated lettering. Grid 13/400; headers 12/500; sheet labels 12.5; inspector title 13/600; status 12. Numeric digit advances were measured in the original font files and browser uses tabular numeric styling. Proportional names, accounting parentheses, identifiers, consistent baselines and selection edges inspected in focused images. No synthetic italic. Small metadata remains 10–11 units; at 80% it is deliberately dense and needs user comfort review. Native antialiasing remains untested. |
| Spacing/layout rhythm | Vertical Workbook, independent Fields, left Measures/Marks, two stacked horizontal shelves, grid-led canvas and dismissible right inspector preserve the intended architecture. Actual title/tabs 28/28, full commands 80, compact 36, icons 30, status 22. Control radii mostly 2 and popups 3. Source preview is a labelled band rather than pretending embedded objects occupy inconsistent generated row coordinates. Narrow canvas objects stack. These are explicit experiment choices under the human's space/density instructions. |
| Colors/tokens | Cool neutral chrome, white data surface, blue dimensions/selection, teal measures, and restrained red negative values retain the concept's hierarchy. Main ink #20272E; secondary foreground consolidated to #5E6E7C after a contrast finding. Selection #285F86/#E4EEF6 and focus #7952B3 remain distinct. Parentheses, labels, borders and focus outlines carry meaning alongside color. Gradients and decorative elevation are absent; only menus/overlay use restrained shadow. |
| Image quality/assets | No imagery is required for this desktop productivity surface. Real Plex font files and library Carbon vector icons are sharp and consistently proportioned, rather than rasterized UI or handmade icon approximations. The generated provisional D badge was not adopted as a brand asset; the study uses a plain Duxcel wordmark. This is a provisional branding omission, not a claimed logo reproduction. Charts and tables are live elements. No UI is a flattened screenshot. |
| Copy/content | Worksheet, field, transformation and measure names remain concrete. Sample counts replace fictitious large-engine counts. Scope labels distinguish source preview, view filters, selected cells, and historical preview. SQL begins with a read-only inspection-example label. Product commands do not expose web implementation details; design-study identity appears only in the study chrome/help/footer. Disabled source-edit controls explain their limits. |

The implementation retains the concept's structural character while following actual-font and measured-density rules. It does not claim pixel-identical reproduction of generated typography, data, branding or row geometry.

## Comparison and correction history

| Earlier finding | Severity, impact and fix | Post-fix evidence |
| --- | --- | --- |
| Fields collapsed to zero usable rows in a short authoring workspace | P2: field assignment became inaccessible. Added a 160-unit minimum section, intentional list scrolling, and bounded Workbook/Marks sections. | 1280×720/100% DOM: Fields inventory 105 units, scroll height 288; alternate screenshot and final full comparison retain usable fields. |
| Native HTML drag failed to place fields reliably | P2: primary authoring gesture was unreliable. Replaced with pointer threshold/capture, labelled preview, valid targets, insertion feedback, frame-based ghost movement, Escape cancellation and explicit menus. | Actual pointer Region → Rows succeeded. Shelf definition gained Region; source field remained available. |
| Same-shelf move-to-end used the wrong insertion index | P2: drop result contradicted insertion intent. Only adjust the removed index when a concrete target index exists. | Pointer Account → Rows end produced Category, Account. Workbook drag also verified its resulting order. |
| UI scaling displaced menus beyond their anchors/viewport | P2: scaled controls could become inaccessible. Normalize screen coordinates by interface scale and clamp popup height/location. | 125% menu within 1440×1024. At 150%/1280×720 Date menu x531, y7.5, right963, bottom564; Apply/Cancel visible around y510–551. |
| Status options could clip partial values under width pressure | P2: visible labels could imply incomplete statistics. Limit by whole metric and retain all values/options in customization. | Checked short/scaled status remains one line; heights 22 at 100%, 27.5 at 125%, 33 at 150%. Focused inspector/status image shows complete metrics. |
| Source preview dominated short canvases | P2: analysis objects were pushed below the useful area. Clamp preview to 150–336 units at 40% of canvas height. | 150% short viewport preview 225 physical pixels; at 1280×800 icons mode preview adapts and matrix begins near y487. |
| Side-by-side objects clipped the chart in a narrow two-pane workspace | P2: major content region became partially inaccessible horizontally. Stack objects below 880 canvas units. | At 1280×800, main width772; matrix and chart both739 wide at x240, vertically placed y487/y817. No page horizontal overflow. |
| Small secondary labels were too faint | P2: legibility issue exposed by focused comparison. Consolidated low-contrast secondary foregrounds to #5E6E7C. | Final sharp grid/shelf/inspector comparisons viewed after correction; computed footnote foreground rgb(94,110,124). |
| Inspector footer could fall outside a large-scale short window | P2: lower controls were unreachable. Inspector now scrolls and its close header stays sticky. | 150%/1280×720 inspector client height321, scroll height416; header and dismiss button remained visible. |

Initial full comparison remains at `qa/analysis-comparison-before.jpg`. Each correction was followed by browser interaction or geometry checks; final visual comparisons were recaptured after the visual fixes.

## Interactions exercised

- Grid click/drag/Shift selection; keyboard Shift+Down; Alt+Down menu and Escape; source selection retained through pane changes.
- Column boundary pointer resize 184 → 242; source sorting changes row order; searchable checklist filtering changes visible sample rows.
- Field reveal to Memo preserves selection and scrolls horizontally; search and field action affordances remain separate.
- Field → Rows drag, shelf reordering, vertical sheet reorder; multi-field shelves, multiple measures, Matrix → Bar representation retaining shelf definition.
- Left pane resize228 →268; Workbook/Fields independent hide/recovery; both hidden reclaim x0 for main.
- Transformations open/collapse/dismiss/reopen; historical Preview has explicit banner; Return clears preview; SQL opens as read-only inspection example.
- Full/compact/icons modes; command areas80/36/30; sticky command overflow remains reachable. Focus mode restores previous panes/authoring and keeps status thin.
- Statistics customization updates Distinct preference. Interface scales80/100/125/150 sampled; grid zoom110% changes row height without changing status/chrome.
- F6 visible-region movement; help modal Tab/Shift+Tab containment.
- Viewports1280×720,1280×800,1440×1024,1920×1080 sampled; 80% at1920 and150% at1280 additionally checked. Overrides reset before handoff.

This list is sampled, not a claim that every permutation or complete keyboard-only workflow has been validated.

## Runtime checks and boundaries

- Final browser console: no warnings or errors.
- Production build: passed. Initial script311.20 kB (94.80 kB gzip); analysis/chart code deferred409.23 kB (118.50 kB gzip). Sizes describe transfer, not frame-time performance.
- Existing five synthetic fixture checks: passed. No new calculation engine or exhaustive engine tests added.
- Data-grid rows are virtualized; drag ghost uses animation-frame writes; chart animations disabled. No quantitative latency, FPS, large-data or native benchmark is claimed.
- Reduced-motion override inspected in CSS; actual OS preference change not tested.
- No native code edits, DuckDB linkage, WASM integration or external publication performed.

## Open questions and follow-up polish

- [P3] User review should decide whether 10–11-unit metadata is comfortable, especially at80% UI scale, or should be promoted to11–12 with a modest density cost.
- [P3] Matrix header hierarchy can use more compact repeated measure labels after the shared-builder interaction is accepted.
- Drag cancellation edge cases, invalid-role reasons, long-shelf overflow, native accessibility, dark appearance, high-DPI rendering, full engine semantics and free object placement are future validation scope. They are not implied by this pass.
- Provisional wordmark and icon semantics can receive a dedicated brand review after workspace approval.

## Implementation checklist

- [x] Open and compare both source and rendered implementation.
- [x] Full-view and focused comparisons in the same inputs.
- [x] Five required fidelity surfaces reviewed.
- [x] P2 fixes followed by post-fix evidence.
- [x] Core UI gestures, pane lifecycle, scale and viewport samples exercised.
- [x] Build and final browser console checked.
- [x] Record experiment boundaries and defer native handoff until user approval.

final result: passed
