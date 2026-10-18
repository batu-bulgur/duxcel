# SQL inspector geometry, scrolling and capture ownership

Accepted bounded checkpoint, 18 October 2026. Final untransformed and transformed browser gates use product `f254655`, with runners `d36525d` and `5d5fa28` respectively. Focused native29 uses `f254655`; broader native review400 remains separately scoped to `2bbf0d6`. This closes measured SQL anatomy, nested scrolling, cache lifetime and capture-input quarantine, with the explicit visual and physical limits below.

The shared C inspector now lays out its source, description, padded code viewport and intrinsic footer controls from the measured reference. It owns continuous code scrolling separately from the outer inspector. Steps→SQL resets newly mounted code presentation while retaining owned SQL and wrap storage; Collapse resets the outer lifetime. Real source SQL and its line count remain application-owned. Review captures quarantine native/browser pixel-pan input instead of changing the captured inspector during its frozen render.

| Retained gate | Source and exact scope | Time |
| --- | --- | --- |
| Actual browser SQL1,895, untransformed DPR2 | Product `f254655`, runner `d36525d`; preserves all1,680 earlier checks and adds actual inner/outer thumb End/Home and frozen picks | 59.003s driver /61.328s browser /61.600s total |
| Actual browser SQL1,895, transformed DPR2 | Product `f254655`, runner `5d5fa28`; same five actual states and strict canonical C endpoints, with pinned trusted wheel transport | 55.635s driver /58.068s browser /58.340s total |
| Actual browser SQL1,680, untransformed DPR2 | `2bbf0d6`; four required wide/narrow100/150% states plus exploratory640×400/150% | 53.949s driver /56.470s browser /56.712s total |
| Final hidden native SQL/Steps29, optimized | Product `f254655`, runner `d36525d`, `[882,911)` | 14.233s App /15.642s total |
| Final hidden native SQL/Steps29, O1 ASan/UBSan | Same source/range | 14.504s App /15.735s total |
| Fresh pure inspector geometry2,117 per configuration | `f254655`, optimized/O1; geometry source and tests unchanged from `2bbf0d6` | 0.491/0.571s execution |
| Native review400, optimized/O1 sanitizer | `2bbf0d6`; original400 stages and three reports per configuration, plus two unsaved pan-quarantine captures | 18.227/21.475s App |

The focused native profile contains the earlier16 Steps stages and13 SQL stages. The maintained catalog is911 layout stages, not a new full-catalog pass. `--config both` in the affected runner executes both pure configurations and only the sanitizer App; the optimized29 App was run explicitly. [Sanitizer summary](native-affected-sanitize-summary.json), [optimized summary](native-affected-release-summary.json) and [native review summary](native-review-summary.json) retain the exact commands, source/binary hashes and completion markers.

The preceding `2bbf0d6` native29 gates remain in their [optimized](native-affected-initial-release-summary.json) and [sanitizer](native-affected-initial-sanitize-summary.json) summaries:14.207/14.598s App and15.448/16.536s total. Those original scopes do not become the later caption-adjusted binary merely because the same assertions pass again.

The affected `--no-build` run's old pure executables actually reported2,005 checks; that historical stdout is preserved without relabeling. The [fresh pure gate](pure-fresh-summary.json) rebuilds the tests and reports2,117 inspector checks per configuration. Its4.769s total includes2.743s compilation plus both inspector and shadow test modules, rather than an inspector-only timing.

## Reference and actual behavior

The existing [frozen SQL provider](../2026-10-18-sql-inspector-reference/provider.json), SHA-256 `23480f4c61d507615ac92724da46ebf2509d01ce369d983ab8d435e40e983b2a`, contains the original DOM/box/style and mount-lifetime measurements. Its1,860 checks and35 captures are reference evidence, not native or Wasm acceptance. It is linked rather than duplicated.

The actual browser setup imports the real five-row CSV and applies five retaining predicates. Its real SQL is copied twice only to an injected owned memory writer; no OS clipboard is accessed. Source, manual/object ownership, Page caches, selection, document history and request identity remain stable through the geometry, scrolling, capture cancellation and mount-lifetime checks, with zero UI-only jobs. [Bounded measurements](browser-untransformed-measurements.json) retain source/cache identities, owned SQL byte count/hash, per-state geometry, scroll limits, actual picks and state transitions. The complete SQL/clipboard payload and full event stream are omitted. [Untransformed](browser-untransformed-summary.json) and [transformed](browser-transformed-summary.json) summaries preserve the separate runner and compiled-artifact provenance. [Transformed measurements](browser-transformed-measurements.json) retain the same ownership/geometry/scroll checks without duplicating full events.

The four required viewports have zero outer overflow. The exploratory640×400/150% state has a zero-height code viewport and a fractional5.333333-unit outer range. Cache metadata is emitted after paint even when the code widget has no visible rectangle; no invisible code target is fabricated. The original viewport range remains canonical through idle and scrollbar updates rather than being reconstructed with slightly different floating-point subtraction.

The images below document the final untransformed `f254655` state. Visual inspection confirms complete Copy SQL/Steps captions in the wide100 and narrow150 images, with unobscured header tools. This is retained visual review, not automated full-glyph RGBA equality. The earlier [1,680-check summary](browser-untransformed-initial-summary.json) keeps its original `2bbf0d6` scope.

The transformed wheel receipt has a separate [bounded capsule](browser-wheel-transport.json) and [actual runner source excerpt](browser-wheel-transport-source.txt). On pinned Chrome150 revision `5b586c06e0d27582900f17e2d59c5370d8d6e0bb`, the first100,000-unit overshoot requests CDP delta160,000.0023841858; its binary32 representation is160,000 and the actual trusted DOM receipt is80,000 at DPR2. Recovered UI input is99,999.99850988391, an error of0.00149011609 within the independently derived0.004882812516 transport bound. Each actual receipt must equal `Math.fround(requested)/DPR`; this does not relax canonical C offsets, fractional reversals or End/Home equality. The capsule includes pinned primary Chromium source links. It applies only to this unzoomed top-level DPR2 browser, not arbitrary transports.

The separate [CPU caption measurements](caption-cpu-metrics.txt), [manifest](caption-cpu-manifest.json) and [small proof](caption-cpu-proof.c) explain the former ellipsis: native unkerned float advances exceed the frozen intrinsic slots by0.044–0.078 points, enough to exceed the text painter's fitting allowance. `f254655` extends only the text paint width into existing right padding, preserving control geometry and label origin. These are actual bundled-font cause measurements; the final runtime and visual review have their own scope above.

- [Wide100 SQL](inspector-sql-wide-100-first.png)
- [Narrow150 SQL](inspector-sql-narrow-150-first.png)
- [Scrolled code frozen picker](inspector-sql-narrow-150-scrolled-code-picker.png)
- [Exploratory zero-height code](inspector-sql-narrow-short-150-first.png)

## Natural failures and fixture corrections

[Original browser negatives](browser-natural-failures.json) retain each run's actual product/runner hashes and strict failure:

- The old-product control50010 fails the intrinsic Copy control geometry after421 checks. This is distinct from later candidate regressions.
- Candidate50128 exposes scaled-workspace position `130.999985` versus the reference's `131`. The App now anchors the pane directly to the actual window edge. This is a geometry precision witness, not evidence of visible pixel jitter at that magnitude.
- Candidate52254 reaches1,518 checks before the zero-height code state omits owned SQL-cache metadata. The cache remains owned; the defect is review observability.
- Candidate54319 reaches1,582 checks before canonical outer-range drift. `2bbf0d6` preserves the original range through idle and thumb handling.
- Transformed58825 stops after518 checks at the old0.001-unit receipt assertion. The later actual trusted receipt and pinned float32 derivation explain that test precision failure; it is not evidence of an App pan defect. The old run did not retain its receipt, so none is invented.

The [native old-guard control](native-quarantine-natural-red.json) at `071151c` reaches review stage372 and fails the exact capture ownership assertion: a captured3-unit pan changes Steps outer offset from0 to3 during PICK. The [failure excerpt](native-quarantine-natural-red-excerpt.txt) preserves that actual witness. The fixed native400 run verifies both Steps and SQL captures with actual outer room, unchanged offsets/grid/cache/model/jobs immediately after capture and cancellation. Three new scrollbar drag fences retain the existing gesture-completion requirement. These tests do not exercise a physical trackpad.

Two earlier native-control failures are fixture failures: run053938 lacked a core-generated database, and run054045 stalled because its extra probe waited for IDLE while already in PICK. They are not product-negative evidence. The corrected probe retains400 stages, the original captures/assertions and all existing wait/frame budgets.

## Production boundary and remaining work

[Native production exclusion](native-production-exclusion.json), [native sanitizer exclusion](native-sanitize-production-exclusion.json) and [normal Wasm exclusion](wasm-production-exclusion.json) pass at their recorded source/artifact scopes. The native review400 binary remains the accepted `2bbf0d6` build, rather than a claimed new full native review run for the caption paint adjustment. Review metadata, registry and capture behavior remain compile/link excluded from production. The native review summaries retain the original three report targets per configuration without copying outbox images, sidecars or databases. No new queue/report-delivery acceptance is claimed.

The signed native bundle is quietly refreshed to `f254655` in0.721s, with signature/source verification and no launch. [Package metadata](current-ui-package.json) and [the packaging log](native-package.txt) retain its exact binary/source hashes and separate native review400 scope. The [post-validation package record](current-ui-package-postvalidation.json) adds the later transformed acceptance while preserving those original packaging/signature hashes. The sole8767 review service is quietly refreshed to artifact `5d4dfb7f3121c1f9dc87`/product `f254655` in0.687s, PID59743, with no tab reload or native launch. [Service metadata](review-service.json), [listening-port witness](review-service-ports.txt) and [queue retention](review-service-retention.json) preserve its exact scope: zero reports before/after, every existing ID retained, and no new report-delivery acceptance.

Actual untransformed/transformed thumb delivery, the two named caption visuals and focused native caption-adjusted regressions are accepted above. The independent [strict corner diagnostic](../../../../tests/shadow_corner_diagnostic.json) still fails against exact retained two-dimensional shadow pixels; its opt-in `--reference-corners` mode is deliberately red while the ordinary body component stays green. No corner production change or closure is hidden by SQL acceptance. Matching native physical pixels, physical trackpad/IME input, arbitrary browsers/transforms, browser workbook persistence, broad analytical semantics and complete UI parity remain separate gates. Earlier engine/source-binding evidence keeps its own source scope.

## Retention

[Originals](originals.json) records original paths, byte sizes and SHA-256 hashes. [Retention methods](retention.json) distinguish exact copies from selected fields/log lines. [The seal](files.sha256.json) covers every retained file except itself. No dataset/database, complete SQL or clipboard payload, credential, full event stream or report bundle is copied. The untracked `tools/ui-review/` status is retained honestly in the source manifests. This documentation lane performs no compiler, browser or native runtime work.
