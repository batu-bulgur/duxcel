# Font upload failure ownership — 18 October 2026

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

This checkpoint accepts one font-loader correction: valid CPU glyph data with a zero GPU texture ID cannot replace the current font family. Product `a032c2b` rejects that staged atlas, releases its owned CPU data, and retains the complete previous primary/mono family on reload failure. It adds no glyph ranges, font assets or dependencies.

## Cause and simulated failure acceptance

Raylib 6's [`IsFontValid`](https://raw.githubusercontent.com/raysan5/raylib/6.0/src/rtext.c) validates CPU font fields and expressly excludes GPU validity. The previous loader checked the default texture ID but accepted zero. The updated actual-`ui.c` test uses those real validity semantics; its texture upload remains a bounded stub.

The natural negative (archived `cause/natural-red-manifest.json`) runs product `f139c74` with the new tests. It reaches the first injected primary texture-ID-zero case after the previous assertions and fails `!init(&ui)` as expected. Compile/runtime are 0.733/0.548s. This is the real prior C loader accepting a simulated upload failure, not an observed GPU allocation failure. An intentionally failing process does not establish leak-free cleanup.

The focused optimized/O1 ASan+UBSan run (archived `acceptance/cpu-summary.json`) passes **4,964 checks per configuration**, preserving all earlier replacement-glyph and missing-font validations. Its 21 injected upload failures cover all six primary atlases and the mono NULL-marker atlas at initial load, scale reload and DPI reload. Allocation slots are keyed by owned glyph buffers rather than a successful texture ID. The tests verify complete staged glyph/rectangle cleanup, exact old Font/map/advance/NULL-marker retention, idle retry suppression and subsequent successful recovery.

Build/optimized/sanitizer times are **2.574/1.351/2.251s**, **6.286s total**. The run recorded `e955cb7` plus the then-uncommitted loader fix. Its exact 278-file source fingerprint matches the committed `a032c2b` GPU run: `5d63162da16b77f09a7edb61980a6c902e76193381f99af2d603cf67041060f3`. Both original summaries and exact build commands are retained; provenance (archived `provenance.json`) also hashes the focused sources and unchanged font assets.

## Healthy actual GPU regression

The separate hidden O1 ASan+UBSan GPU run (archived `acceptance/gpu-summary.json`) tests committed `a032c2b` with successful real uploads. It passes the **303 missing-glyph checks plus all original 110 paint checks**. Across six primary atlases and 80/100/150% UI scales at DPR2, unsupported BMP/CJK/emoji pixels equal explicit U+FFFD, differ from literal `?`, preserve UTF-8 and match an identically clipped replacement reference with zero outside ink. Atlas identities remain stable during those draws. The GPU log (archived `acceptance/gpu-sanitize.txt`) retains all 18 exact metric records, and the three PNGs are under [captures](captures/).

Build/GPU/background times are **2.970/2.004/1.338s**, **6.318s total**. The reused, up-to-date background binary was actually rerun and passed its **477 hidden/non-key Cocoa window and framebuffer checks**. This healthy real-GPU run does not inject GPU failure and does not independently validate how a particular driver reports exhaustion.

## Bounded native review integration

The native review summary (archived `acceptance/native-review-summary.json`) accepts **400 hidden renderer/controller stages per optimized/O1 ASan+UBSan configuration** at exact `a032c2b`, with the same `5d63162…` source fingerprint as the focused CPU/GPU runs. Actual App times are **17.860/21.098s** and the separate hidden-window probe takes **1.312s**. The recorded **61.618s rebuild is separate from App runtime**; build commands (archived `acceptance/native-review-build.txt`) are retained.

The bounded log extracts (archived `acceptance/native-review-extracts.json`) retain every PASS line and normal shutdown, original complete-log hashes, and verified hashes for the **three saved reports per configuration** recorded in the summary: filter search, equality option and File scrollbar thumb. Report images/archives and generated owner workbooks are not copied. This is the maintained 400-stage review profile, not a whole application-inventory regression, physical input/IME/trackpad validation, OS file-panel delivery or OS clipboard activation.

## Unactivated coverage investigation

The retained coverage summary (archived `profiles/coverage-summary.json`) and packing summary (archived `profiles/packing-summary.json`) are CPU-only measurements against the exact current raster formulas and bundled sources. Profile provenance (archived `profiles/provenance.json`) records commands, source/font hashes and the private complete profiles; small derived aggregates are retained here.

All five Plex sources contain 418 currently requested real glyphs, plus 73 additional glyphs in U+0370–03FF and 194 in U+0400–052F. Per-source filtering could admit 685 entries inside the existing 1,024-entry advance table and BMP lookup. Tested Greek Extended and additional Cyrillic extension blocks contribute zero glyphs. Raylib already filters unavailable requests, so removing unsupported entries from the request list alone does not shrink today's atlas.

| Actual current configuration, six primary atlases | Current 418 glyphs/face | Candidate 685 glyphs/face |
| --- | ---: | ---: |
| 100% UI, DPR2, canvas font bucket 1 | 11 MiB | 14 MiB |
| 150% UI, DPR2, canvas font bucket 1 | 22 MiB | 22 MiB |
| 150% UI, DPR2, canvas font bucket 2.5 | 88 MiB | 176 MiB |

These are generated GrayAlpha **pixel payloads**, not measured GPU residency, driver overhead or upload costs. The last case's retained per-glyph crop payload also grows from 22.29 to 38.53 MiB; atomic reload temporarily keeps old and new resources together. The small font and mono font use the current canvas bucket too. Mono's seven requested marks were measured unchanged and are excluded from the table.

A second CPU profile compares raylib's default and built-in skyline packers over all six atlases and the three configurations: **72 records**, no out-of-bounds rectangles, overlaps or source-to-atlas alpha mismatches. Both packers produce identical dimensions at every case, including the 88→176 MiB step. Compilation/runtime take 0.662/0.909s. These single CPU observations are not GPU or interactive latency measurements.

**Greek/Cyrillic activation remains open.** Any future bounded loader extension must preserve existing valid custom-font glyphs, mandatory replacement/mono marks, O(1) draw lookup, explicit budget denial and atomic cleanup. No lazy/dynamic atlas architecture is introduced here. Full CJK/emoji coverage, international shaping, physical input, browser rendering, a whole App regression, production exclusion and refreshed distribution are outside this checkpoint unless separately recorded below.

## Shipping exclusion and quiet distribution

The curated build record (archived `delivery/build-summary.json`) retains the exact `a032c2b` source fingerprint, commands, compiler/link flags, immutable core artifact hashes and original complete-manifest hash. Native optimized/O1 builds take **2.519s**; their linked review exclusions take **0.157/0.214s**. Normal/review Wasm builds take **4.467/4.107s**, with exclusion **0.259s**. Native (archived `delivery/native-exclusion.json`), sanitizer (archived `delivery/sanitize-exclusion.json`) and Wasm absence (archived `delivery/wasm-absence.json`) records pass. These are compile/link/asset checks, not a new browser runtime or browser font-failure acceptance. Prior browser UI1,895 runs remain scoped to `f254655`.

The signed native bundle (archived `delivery/package.json`) quietly refreshes to `a032c2b` in **0.770s**, with signature verification and source/binary matching, without launch. The owned8767 service (archived `delivery/service.json`) refreshes in **0.491s** to review artifact `844a82e824cb63ad45c8` (normal App artifact `61452913ef5fbed382b8`). No user browser reload or native App launch occurred. Queue retention (archived `delivery/service-retention.json`) is **0→0**, same project/IDs; it establishes retention rather than new report delivery. An already open browser tab is not claimed to have loaded the refreshed artifact.

## Retention

Only focused logs/manifests, compact profiling summaries and three bounded glyph captures are retained. No executable, generated database, report archive or input event stream is included. `hashes.json` seals the curated files; original paths and SHA-256 values remain in provenance. The older `build/font-fallback-gpu/manifest.json` still described `cca8081` and was deliberately excluded from this newer acceptance.
