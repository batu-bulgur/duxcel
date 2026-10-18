# Steps scrolling and expanded inspector border

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

Accepted bounded checkpoint, 18 October 2026. Product C/native/Wasm source is `43b43d3`; later runner changes are identified separately below. This closes independent Steps scrolling and the four expanded-header/inside-border states, not complete inspector or application parity.

The shared C Steps inspector now retains the reference's independent outer and inner scrolling instead of compressing short-window content. The header stays sticky, clipped rows and controls own only their visible region, and ordinary Collapse/reopen resets both offsets. The expanded inspector also paints its one-presented-point inside border. Native source calculations remain real DuckDB work; review captures add no SQL or saved reports.

| Accepted gate | Exact scope | Elapsed time |
| --- | --- | --- |
| Actual browser Steps1,067 | Product/runner `43b43d3`; DPR2, four wide/narrow100/150% states, untransformed | 40.572s driver /43.130s wrapper-browser /43.436s total |
| Actual browser Steps1,067, transformed | Same product `43b43d3`; runner `6bd85fb` includes the `c93b380` quantized-cell coverage correction | 39.640s driver /42.532s wrapper-browser /42.847s total |
| Native Steps16, O0 ASan/UBSan edit | `65c0ae3`, exact range `[882,898)`; hidden renderer/controllers | 8.664s App /9.972s total |
| Native Steps16, O1 ASan/UBSan | Runner `c93b380`, unchanged product C `43b43d3`; exact range `[882,898)` | 8.584s App /15.210s total including affected rebuilds |
| Native Steps16, optimized | Runner `c93b380`, unchanged product C `43b43d3`; exact range `[882,898)` | 8.289s App /9.354s total |
| Pure inspector geometry2,005 per configuration | Optimized and O1 sanitizer in the preceding affected run | 0.530/0.670s execution; included above |
| Native review400, optimized/O1 sanitizer | Product C `43b43d3`, runner `fc34836`; three exact reports/configuration | 18.308/20.381s App |
| Actual browser rail/border564, untransformed | Product `43b43d3`, runner `392f859`; all four expanded-header/border and retained-rail states | 38.194s driver /40.589s wrapper-browser /40.821s total |
| Actual browser rail/border564, transformed | Same product/runner and four cases; pinned compositor mapping with explicit coverage exclusions | 39.135s driver /41.516s wrapper-browser /41.669s total |

The earlier private `93ba8d8`14-stage O1 run took7.724s App/9.357s total. Its bounded summary is retained for lineage; it does not cover the later16-stage profile. No new full inventory pass is claimed. The maintained catalog is898 layout/1,331 application stages, with separate GPU/background probes.

## Reference and interaction scope

The existing [frozen Steps provider](../2026-10-18-session-source-bindings/steps-scroll-reference.json), SHA-256 `f265dd239db596d177e96c824db3699d3c1e6a62016f663ab09abaa2fd876f3b`, retains20 original capture identities and seven bounded detailed states. It includes original source/prebuilt hashes, raw DOM/CDP quads, CSSOM metrics and corrected allocated scrollbar bands. This checkpoint links that provider rather than duplicating it. A measured allocated band is not a painted-thumb measurement.

The actual browser setup imports `transactions.csv` through the real chooser and applies five real predicates that retain all five rows. Source plus five filters produces six Steps. Compact ribbon and a normally closed left drawer match1440×1024/100%,1920×1120/150%,640×720/100% and640×640/150%. Source/statistics setup finishes before the zero-job baseline.

The short150% state matches the measured sticky header and full content: outer limit103.333336 logical units, inner limit182, and separate13-presented-point tracks. Trusted wheel events independently reach/reverse these offsets; the partial55-unit row retains exact clipped identity and frozen review-outline bounds. End/context cancellation, covered-away Edit inertness and Collapse/reopen preserve source/Page/request, selection/history and memory clipboard ownership. Edit/Preview/Inspect reachability is proved by frozen semantic picks and cancellation; those actions are not activated in this focused browser flow. Native16 adds actual queued controller traversal and the55-unit row boundary cases. These tests do not claim physical event delivery or input latency.

The accepted short-state images are normal reopened inspector (archived `inspector-scroll-narrow-150-reopened.png`) and frozen clipped row (archived `inspector-short-partial-row-frozen-bounds.png`). Bounded measurements (archived `scroll-measurements.json`) retain relevant geometry, ownership fields and actual picked IDs without the duplicated provider or full state/event stream. The transformed summary (archived `scroll-browser-transformed-summary.json`) retains all four actual geometry states and the partial row's quantized-source-cell full/any coverage, exact ink AABBs and boundary exclusions. This corrects the test's earlier continuous semantic-envelope assumption; it does not waive a painted edge. The32 runner checks pass in2.551s.

## Natural failures and fixture boundaries

The natural expanded-border failure (archived `expanded-border-natural-red.json`) runs the updated `364b381` harness against the actual pre-fix `65c0ae3` product. All68 fully covered wide100 inside-border pixels contain panel fill `#fbfcfd` instead of `#cad2da`; adjacent fill is correct. It fails in15.720s total. The original expanded frame (archived `expanded-border-natural-red.png`) is retained. Product `43b43d3` adds that inside border. Accepted untransformed (archived `rail-browser-untransformed-summary.json`) and transformed (archived `rail-browser-transformed-summary.json`) rail/border summaries each retain all four measured header states,20 actual semantic proofs,16 frozen outline measurements, inside-border/fill samples and unchanged wide/narrow canvas ownership. The narrow150 header uses the actual outer-scroll lane; all live expanded clicks use those proved bounds. Transformed expanded-border bands have27/41 eligible pixels at100/150% per state, all exact, while retained-band/outline phase exclusions remain explicit. These are bounded pinned-Chromium raster measurements, not arbitrary-transform or physical-pixel acceptance.

The prior `111` partial-row run (`20261018T044101-browser-33857`) exposed a real review-marker raster issue: the clipped target expected bottom pixel475 but painted476. Review-only pixel-center quantization in `65c0ae3` addresses that marker. It is distinct from normal Steps content paint.

Other earlier failures are not product geometry negatives: old `e22840d` lacked the new armed metadata; the reverse half-delta case had a fixture wheel-normalization assumption; one rail fixture read the wrong reference style; a later narrow150 rail fixture guessed header coordinates without the new outer lane. Run `20261018T045331-browser-38014` then exposed a cropped test sample: its old64-unit strip began at544 while the actual measured Collapse began at543. Tool correction `fc34836` includes every measured control with a bounded margin. Run `20261018T045554-browser-38364` exposed the corresponding live-click path still using the old guessed position; `392f859` routes every expanded proof and live click through the measured reference. These are bounded fixture corrections, with no product change after `43b43d3` or waived outline assertion. Existing waits and process ceilings remain intact.

## Review and production boundary

The native400 summary (archived `native-review-summary.json`) retains binary/build-header hashes, completion counts and the same three report targets per configuration. Only bounded startup/completion log excerpts are copied; report images, sidecars, databases and outboxes remain omitted. This is renderer/controller regression, not physical AppKit/IME or new queue-delivery acceptance.

Linked optimized/sanitizer native exclusion (archived `steps-border-native-exclusion.json`), sanitizer exclusion (archived `steps-border-native-sanitize-exclusion.json`) and normal Wasm exclusion (archived `web-review-absence-details.json`) pass. The additional marker audit (archived `steps-border-extra-exclusion.json`) ties the new pipeline-scroll metadata and scrollbar IDs to actual native/normal/review Wasm hashes: production artifacts contain none, review contains the expected markers. Review code remains compile/link excluded. This check neither launches the user app nor repeats broad engine/queue end-to-end tests.

## Quiet distribution refresh

The package record (archived `current-ui-package.json`) identifies wrapper source `392f859` and native code `43b43d3`. The signed `build/Duxcel.app` is refreshed in0.765s, with signature/source verification and no launch; the desktop owner checks existing processes and holds the native lock before replacing the bundle. The bounded packaging log (archived `steps-border-package.txt`) records the signature result.

The service record (archived `current-review-service.json`) identifies the accepted review artifact `aac0c29a0cbd70b80360` at `http://127.0.0.1:8767/`, source `43b43d3`, PID39086 at refresh. The port snapshot (archived `steps-border-service-ports.txt`) records that sole project listener. The queue-retention record (archived `steps-border-review-retention.json`) keeps every prior ID, with zero reports before/after and no user-browser reload or native launch. This is queue retention, not a new report-delivery/agent-pickup test. PIDs and package/service pointers are historical observations, not permanent identities.

## Retention and limits

Original identities (archived `originals.json`) record exact size/hash/path for each source run file used in this bounded extraction. Native summaries/logs retain source and unchanged binary hashes; browser summaries record runner/product source, immutable artifact signature and selected artifact hashes. The untracked `tools/ui-review/` status is recorded honestly in original manifests; it is not silently represented as a clean tree. The retained files omit credentials, auth/events, databases, clipboard payloads and report bundles. Retention methods (archived `retention.json`) distinguish exact copies from bounded extractions. The hash seal (archived `files.sha256.json`) covers every other retained file; it excludes itself to avoid a circular digest. The prior444KB frozen provider remains linked, not copied.

Expanded-shadow fidelity, remaining inspector anatomy, SQL inspector outer scrolling, arbitrary transforms/browsers, matching-state native physical pixels, physical trackpad/IME delivery, browser workbook persistence and complete UI parity remain open. Earlier App680/Worker221 source-binding acceptance stays at product `57d459a`/runner `a972913`; session registration identity is not persistent/content identity. Engine semantics and queue delivery retain their own source scopes. No new shadow-module or broad engine acceptance is implied.
