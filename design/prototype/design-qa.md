# Duxcel responsive UI refinement QA

final result: passed

16 October 2026. Pass applies to this disposable UI iteration and its sampled browser interactions. User design approval, native implementation, and engine validation remain separate.

## Findings

No actionable P0/P1/P2 issue remains in the checked scope. The frozen selection regression, toolbar clipping, small-window dock pressure, vertical control collisions, and extreme-scale status clipping were fixed and checked again. Prior concept-fidelity evidence is retained in [the first-pass report](qa/design-qa-first-pass.md).

## Source truth, states and comparison inputs

The source is the existing Duxcel design plus the user's explicit instruction to fix frozen selection layering and refine icon/toolbar placement and responsive compaction. It is a scoped refinement, not a new visual direction.

| Comparison | Source | Revised capture | Dimensions/state |
| --- | --- | --- | --- |
| Full analysis | `qa/analysis-final.jpg` | `qa/iteration-wide-final.jpg` | Both 1440×1024, Home/full ribbon, analysis active, Rows Account/Category, Columns Year/Quarter/Month, same example data, right inspector dismissed |
| Narrow inspection | `qa/iteration-before.jpg` | `qa/iteration-narrow-analyze-final.jpg` | Both 416×924, Analyze/compact ribbon, source inspection, same canonical selection; left dock deliberately becomes a recoverable overlay |
| Toolbar detail | Same full-analysis sources | Same revised full-analysis capture | Matching crop x0–1440, y28–137 |

Browser CSS viewport equals captured pixel dimensions, device pixel ratio 1, UI scale 100%, canvas zoom 100%. No density normalization was required for these existing-browser targets. Both source and implementation were combined and viewed together:

- `qa/iteration-wide-comparison.jpg` — 2904×1064.
- `qa/iteration-narrow-comparison.jpg` — 856×964.
- `qa/iteration-toolbar-comparison.jpg` — 2904×149.
- `qa/iteration-flyout-comparison.jpg` — 856×964, current compact Home state beside its full Number controls; supplemental interaction-state comparison, not a prior-version fidelity claim.

Other evidence: `qa/iteration-mid-final.jpg` (1024×800), `qa/iteration-narrow-final.jpg` (416×924 Home), `qa/iteration-flyout-final.jpg` (416×924), `qa/iteration-frozen-fixed.jpg` (1440×1024), `qa/iteration-scale-150.jpg` and `qa/iteration-frozen-space.jpg` (360×900 at150% UI). The last two demonstrate distinct customization/recovery states.

## Corrections and post-fix evidence

| Finding | Severity/impact | Fix | Post-fix evidence |
| --- | --- | --- | --- |
| Active scrolling cell appeared above frozen cells | P1, selection obscured fixed data and intercepted its input | Isolated grid layers: body0, active1, frozen10, active frozen11, row gutter20, header30 | Horizontal scroll504; active amount bounds x472–618 overlap frozen boundary x526. Hit at x518 returns Transaction ID, column1. Final screenshot shows selection clipped behind that frozen surface. Rechecked after the grid-frame refinement. |
| Commands were horizontally clipped at narrow widths | P2, commands disappeared instead of adapting | Measure natural group widths; progressively collapse lower-priority groups, then move whole named groups into overflow. Keep action space reserved. | 24 Home width/mode samples all fit; group strip stayed before action dock. Full/narrow/focused comparisons viewed after correction. |
| Compact/icon modes lacked a direct path to some formatting controls | P2, shrinkage removed discoverability | Expanded-group disclosure and collapsed buttons open the same full controls. Named overflow retains group identity. | Font bold action worked; overflow Number exposed formatting/decimal controls; Data flyout opened the selected Amount filter. |
| Display/help and Save could disappear with the rest of chrome | P2, important layout recovery became inaccessible | Keep display/help beside the independently scrollable tab strip; keep Save outside collapsible document metadata | Visible in416 and360 captures and150% scale sample. |
| Pinned left dock consumed almost the whole narrow canvas | P2, little usable data surface remained | Below700 logical units, preserve section preferences and offer an explicit left overlay from the context-strip workspace button |416 inspection main now uses the whole width; overlay open/close, field reveal, Escape from focused search, and return to wider dock checked. |
| Generic relaxed spacing caused vertical ribbon controls to collide with captions | P2, stacked command rhythm broke | Separate stacked-group row gaps: Clipboard0, Font/Number2; horizontal groups may relax independently | Full groups measured inside content band y61–117. Copy/Paste/Cut occupy62–80/80–98/98–116; Font/Number controls89–117; captions remain separate. |
| At very large scale, status displayed part of a forced metric | P2, truncated labels implied incomplete values | Fit whole text widths to actual remaining space; permit zero visible metrics and retain values in customization | At360×900/150%, zero metrics fit, zoom right352.5 within360; customization right349.5, bottom819.5. At416/100%, Count/Sum/Avg all fit. |
| Frozen columns consumed the usable grid at extreme scale | P2, scrolling data became impossible to see without discovering a remote command | Preserve configured freeze; show a local Unfreeze action when fewer than72 logical units remain for scrolling data | At360×900/150%, action visible; click removed the hint and all frozen-cell classes. No page overflow. |
| Resizing could remove the invoker of an open group flyout | P2, focus could be lost or the popup left detached | Close a flyout whose anchor is removed; focus its replacement group/overflow control | Font flyout open at1024 then viewport360: no flyout remained, focus was Font commands, toolbar still fit. |

A field-search Escape check in the existing hot-reloaded session did not dismiss the overlay. After a clean reload of the final code, Escape closed the overlay and focused Open Workbook and Fields. Final focus-mode checks also passed: opening focus with navigation open removed the ribbon; Escape restored both ribbon and navigation; closing navigation reclaimed the canvas. A fresh final load is left open.

## Required fidelity surfaces

| Surface | Review |
| --- | --- |
| Fonts/typography | Actual IBM Plex Sans files remain unchanged. Grid13/400, compact controls12, section titles12.5–13, metadata10–11, status12. Focused same-scale toolbar comparison confirms consistent family, weights, baselines and truncation. Document title truncates before Save. Labels remain proportional; financial figures use tabular digits. No synthetic image lettering introduced. |
| Spacing/layout rhythm | Full/Compact/Icons rows are80/36/32. Common command targets align; ordinary icons16, full analysis20, disclosures11. Group gutters relax above1400 logical units, then tighten/collapse without changing the chosen ribbon mode. Long names scroll in the tab strip while display/help stay fixed. Core canvas, shelves, Fields, matrix and chart retain the prior desktop composition. Narrow left overlay is an intentional requested adaptation. |
| Colors/tokens | Existing cool chrome, white data surface, blue selection, teal measures, muted text and purple keyboard focus retained. Group hover/open states use the same border/fill vocabulary. Recovery hint is a small contextual strip, without a new warning palette or decorative card. Parentheses/labels/borders carry meaning in addition to color. |
| Image quality/assets | Existing Carbon vector icons and actual Plex font files retained. No rasterized controls, hand-drawn icon substitutes, or new branding. Icons remain sharp at tested scales. Same-pixel focused comparison avoids screenshot downsampling as typography evidence. |
| Copy/content | Group labels describe commands, overflow preserves identities, full controls remain discoverable. Frozen-width recovery says what consumes the space and offers Unfreeze. No backend claim was added. Existing sample rows, SQL and history remain illustrative; formatting/data examples are unchanged. |

## Browser checks

- [24 Home combinations](qa/iteration-responsive-checks.json): three ribbon modes ×360/416/640/768/1024/1280/1440/1920 widths. Every sample had no toolbar/page overflow and no overlap between group strip and action dock.
- [15 tab combinations](qa/iteration-tab-checks.json): Home/Insert/Data/Analyze/View at416/768/1440 widths. All fit. Later spacing changes reduced natural widths; final1440 and1024 captures were checked afterward.
- 150% UI at360×900: toolbar fits; Font flyout x7.5–349.5; statistics menu stays inside the viewport; whole-metric behavior and Unfreeze recovery checked.
- Group controls exercised through actual UI: formatting, Number overflow, Data → Filter, Escape and resize focus recovery. Hidden measuring copies are inert and excluded from the accessibility tree.
- Narrow navigation: explicit open, field reveal, close, focused-search Escape, and wider-dock recovery checked. Right pane at416 occupied x136–416, its header dismiss remained reachable, and dismissal removed the rail.
- Frozen selection verified by visible capture and DOM hit testing at the overlapping coordinates, rather than layer numbers alone.
- Final clean browser loads after server restart: no new warnings/errors. A historical development-server reconnect error from the stopped initial preview did not recur.
- Final production build passed: initial script320.09kB (97.53kB gzip), analysis chunk deferred409.22kB (118.51kB gzip), stylesheet47.87kB (10.15kB gzip). These are bundle measurements, not a frame-time claim.
- Browser viewport override reset; live preview retained. No native code or calculation-engine work performed.

## Residual polish and limits

- P3: group-collapse priorities and exact width thresholds should receive user comfort review with sustained work and longer localized labels.
- P3: small metadata at80% UI remains a typography preference to judge in use.
- Real keyboard/OS accessibility integration, reduced-motion preference emulation, touch, dark appearance, high-DPI native rendering, and quantitative latency are not certified by this browser pass. Earlier engine limitations remain unchanged.
- The frozen-column preference is not silently altered when the viewport changes; explicit Unfreeze recovery is a deliberate interaction choice for review.

## Completion checklist

- [x] Frozen layering fixed and hit-tested.
- [x] Adaptive toolbar, full controls and named overflow exercised.
- [x] Multiple modes, tabs, viewports and large scale checked.
- [x] Prior and revised screens viewed together at full and focused scales.
- [x] Five fidelity surfaces reviewed; no actionable P0/P1/P2 remains.
- [x] Build, final browser load, focus recovery and runtime console checked.
- [x] Interaction rules and prototype preferences updated; native handoff remains pending design approval.

final result: passed
