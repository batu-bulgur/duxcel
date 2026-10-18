# Inspector body-shadow checkpoint — 18 October 2026

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

The shared C99/raylib inspector now paints a cached body shadow in the measured narrow expanded states. Exact original reference colors pass without a per-pixel exception. This checkpoint closes that bounded body-shadow slice and adds a stronger, separately scoped Steps ink check; it does not close complete inspector parity.

| Gate | Tested source | Result and elapsed time |
| --- | --- | --- |
| Body shadow, DPR2 untransformed | product `d67c2b9`, runner `bc8a5a7` | 322 checks;23.997s driver /26.437s browser /26.758s total |
| Body shadow, DPR2 transformed | same product, runner `a23b317` | 332 checks;26.152/28.568/28.794s |
| Shadow component, optimized + O1 ASan | private `260c146`, byte-identical helper/test to `174ad1d` and integrated `829ed1f` | 2,004,589 checks/configuration; build2.388s, runs0.597/0.688s, total3.779s |
| Hidden native Steps16 `[882,898)` | private `2b0d50c`, RGB fix included | 8.320s App /9.677s total; module2,004,589 also passes |
| Stronger Steps ink, DPR2 untransformed | product `d67c2b9`, runner `4b827b7` | 1,537 checks;42.763s driver /45.238s browser /45.587s total |
| Stronger Steps ink, DPR2 transformed | product/runner `e3ad8d9` | 1,537 checks;42.826s driver /45.360s browser /45.600s total |
| Earlier native review400, optimized/O1 | **pre-RGB** App `483e6f9`, runner `2519a95` | 17.682/21.215s; three exact reports/configuration |

These are different workloads, not a matched engine-speed comparison. The full review400 and shipping-exclusion results predate the RGB coefficient fix; their exact binary hashes remain separately recorded.

## What changed and what was measured

`src/ui/shadowview.c` generates a bounded premultiplied RGBA8 atlas. Its general RGB coefficient conversion rounds to IEEE binary16, with the opacity byte handled separately, before RGBA8 encoding; the atlas is not an RGB16 texture. `draw_pipeline_shadow` caches it by interface scale and render density. The tested run uploads twice:52×85 /17,680 bytes at100%, then78×127 /39,624 bytes at150%. Resize, one actual outer scroll, Collapse/reopen, Dismiss and idle reuse those uploads. Wide, retained and dismissed states have no active shadow. Source Pages, history, selection and owned clipboard remain unchanged, with **zero jobs after source setup** (the body profile has one real setup job).

The frozen oracle is [the checked-in source-profile header](../../../../tests/shadow_reference.h), SHA256 `118c8d1f68570fc5477a6ad6b657857fe5b5ffb0073337c88c5020b1af5a9f9b`, derived from the original `030846-reference-rail-1269` capture. [Existing reference measurements](../2026-10-18-steps-sort-input/rail-reference/measurements.json) retain the original prototype/source pins; they are linked rather than duplicated. Source pins (archived `source-pins.json`) prove the helper/test byte equality for the retained fresh pure run.

The four actual viewports are640×720/100%,1440×1024/100%,640×640/150% and1920×1120/150%, all DPR2 with Compact ribbon. Narrow strips exclude the inside border and top/bottom corners. Untransformed exact-color checks cover192 pixels at100% and288 at150% per checked phase. Transformed checks accept only compositor pixels wholly covered by the constant reference plateau:45 eligible /203 excluded at100%,62/274 at150%. Every eligible mismatch count is zero. Wide absence uses identical body coordinates in expanded and dismissed frames; transformed witnesses compare full compositor-pixel footprints, retaining gridlines and excluding boundary coverage rather than assuming a white canvas.

Untransformed measurements (archived `browser-untransformed.json`) and transformed measurements (archived `browser-transformed.json`) retain the actual semantic controls, body probes, source/frame hashes, cache states, exact band colors and coverage counts. Per-pixel excluded-coordinate arrays and raw RGBA arrays are omitted; original full-file hashes are in originals.json (archived `originals.json`).

!Actual narrow100 body shadow (archived `shadow-narrow-100.png`)

!Actual narrow150 transformed body shadow (archived `shadow-narrow-150-transformed.png`)

## Natural failures and stronger ink acceptance

The unchanged old `43b43d3` product fails naturally because its sampled expanded strip is all white:44 checks,9.725s total. The first canonical candidate then fails at a single green channel:expected `[239,241,242,255]`, actual `[239,240,242,255]`, four vertically repeated pixels. Bounded old-product evidence (archived `natural-red-missing-shadow.json`) and single-channel evidence (archived `natural-red-single-channel.json`) retain the exact colors and source scopes. The RGB fix applies generally; neither oracle nor acceptance gained a fitted color, exception or tolerance.

The later untransformed (archived `browser-ink-untransformed.json`) and transformed ink gates (archived `browser-ink-transformed.json`) retain the original1,067 geometry/input checks and require nonempty title, Collapse/Dismiss icons, detail title and description ink at default/reopened and short150 scrolled/partial states. Fixed headers and same-position details keep exact RGBA. Intentionally translated details keep an exact neutral-ink bitmap, followed by exact same-position RGBA against the observed outer-end baseline. An earlier translated description had nine pixels differing by one channel while retaining the same2,371-pixel neutral mask; this checkpoint therefore **does not claim full translated RGBA equality**. The transformed result uses the separately built `e3ad8d9` product and the recorded CSS mapping; it is not a general transform/physical-input claim. Geometry alone was not treated as proof of painted text.

## Source and delivery boundaries

- The actual browser body/untransformed-ink artifact is `d67c2b9`, including integrated RGB fix `829ed1f`; transformed ink uses later `e3ad8d9`; the source aggregator and Wasm/module hashes are retained. The private native RGB16 run is `2b0d50c`, and the retained pure run is later `260c146`; those are explicitly separate sources.
- The earlier400-stage native binaries, linked native exclusions and normal Wasm shipping exclusion belong to pre-RGB App `483e6f9` / runner `2519a95`. The normal Wasm artifact remains pre-RGB; review Wasm is newer. No whole-application sanitizer rerun after RGB is implied by the component sanitizer result.
- The signed bundle and stable8767 service remain the earlier accepted native code `43b43d3` / wrapper `392f859` and artifact `aac0c29a0cbd70b80360`. No package refresh, service restart, queue delivery test or user-tab reload is claimed here. [Prior distribution evidence](../2026-10-18-steps-scroll-border/) remains authoritative.
- Source bindings/App680/Worker221 keep their earlier `57d459a` scope. No engine/queue end-to-end workload was repeated for a shadow.

Top/bottom shadow corners, exiting animation, arbitrary scales/densities/transforms, full translated RGBA, arbitrary transformed ink and physical/native GPU pixel parity remain open. SQL reference tooling/pure layout work is prerequisite evidence only; SQL App acceptance is outside this checkpoint. Complete exact UI parity and browser persistence remain open. This checkpoint’s shadow/Steps sources have the898-layout/1,331-App catalog. Later root `e3ad8d9` extends the maintained inventory to911/1,344 for the combined SQL29 profile; it does not extend this acceptance or imply a full catalog pass.

## Retention

This directory contains two unchanged actual PNGs, bounded browser measurements/natural failures, source pins, small retained component/native summaries and logs, and the earlier binary-specific exclusion results. It contains no event trace, auth token, database, clipboard payload or duplicate review-report bundle. retention.json (archived `retention.json`) describes curation; files.sha256.json (archived `files.sha256.json`) seals every other retained file except itself. The original pure `174ad1d` run was agent-reported without retained sanitizer logs; the fresh `260c146` optimized/ASan run is the durable component evidence here.
