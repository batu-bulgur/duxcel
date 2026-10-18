# Captured browser source registrations and inspector geometry

This checkpoint accepts one bounded browser lifetime boundary: C jobs capture the original registration tokens for their declared file contributors, and cached schema reuse requires the same captured names/tokens. Product UI and SQL remain C99/raylib and DuckDB. It does not establish content hashes, persistent browser source versions or arbitrary SQL lineage.

| Gate | Tested source / runner | Result | Seconds |
| --- | --- | --- | ---: |
| Actual App source bindings | Product `57d459a`, runner `a972913` |680 checks, ten phases, transformed DPR2 |31.994 driver /34.366 browser /34.642 total |
| Official Worker | Product/runner `57d459a` |221 stages /124,982 checks, four sequential Workers |13.405 browser /13.580 total |
| Native review regression | Run source `9a91cbd`, before the later chrome adjustment |400 stages per optimized/O1 ASan/UBSan configuration |18.023 /20.001 App |
| Pure host/adapter/probe | Integrated source-binding implementation |85 Node groups |0.226 Node |
| Inspector geometry | `974119a`, integrating `21bf835` |1,487 checks per optimized/O1 configuration |2.752 total |
| Expanded header and retained rail | Product/runner `e22840d`, untransformed DPR2 |515 checks |38.493 driver /40.959 browser /41.193 total |
| Same header/rail, CSS transformed | Product/runner `e22840d`, DPR2 |515 checks |38.632 driver /41.152 browser /41.464 total |
| Current native integration | `e22840d` |400 stages per optimized/O1 configuration |17.353 /19.773 App |
| Quiet signed bundle refresh | `e22840d` |Source/signature verified, no launch |0.715 |
| Natural missing-token negative | Old product `fe6f179`, runner `b296db1` |Fails the first actual registration-token requirement |10.809 total |
| Natural expanded-header negative | Old product `fe6f179`, runner `aa002ed` |Fails measured wide100 Dismiss geometry |10.298 total |
| Natural chrome-origin negative | Product `57d459a`, runner `9a91cbd` |Wide100 header passes; wide150 panel/grid origin fails |17.658 total |

The App product includes C capture `34375a8`, cached-copy contributor handling `dd7f863`, Worker integration `a0d822c`, atomic admission correction `d68939f`, adapter `94d58aa` and host callback `75f6195`. The App runner's later `a972913` correction uses normal two-column keyboard navigation to reveal Amount beyond frozen columns. It changes no product code. Exact source/build signatures are retained in the run summaries; no evidence is relabeled as a later binary or a full inventory pass.

## Captured identity and cache ownership

The host returns nonzero exact unsigned 64-bit epoch/registration strings with each successful file import. C owns the accepted key, friendly name and immutable token. It admits that entry before the first source Page can be submitted, retains accepted entries across history/document changes, and releases rejected temporary imports without retiring already accepted keys.

At normal job submission, C captures a bounded vector for the primary source and declared Copy/Statistics snapshots. Duplicate filenames retain different executable keys/tokens; friendly renames do not alter executable Analysis paths or SQL. Worker validation rejects missing/invalid required contributors, malformed UTF-8/basenames, duplicate guards and out-of-range source indices. Zero-count unbound Worker requests retain their explicit legacy test contract; missing required App bindings fail closed.

One owned JSON vector, at most 64 KiB, survives the job's execution. Each of the three schema-cache slots owns its independent JSON/key, and matching includes contributor order, exact names and token scalars. The primary Analysis is read from the reply after its normal ownership move; cache keys never borrow retired job strings. New schema/key/JSON staging is admitted before replacing the old cache. The browser-only interfaces and backend remain excluded from native execution; review code stays separately compile/link excluded from production.

The registered-name API protects immutable buffers sharing one database object. It does not infer undeclared files in custom SQL, detect raw database/file writes outside that API, unify separate database wrappers or establish identity across sessions. There is no browser workbook Save/Open, source-record formatting or new persistence claim.

## Actual App boundary

The profile uses normal C Open/Import controls and three private real CSVs: two same-name five-row files and an independent 600-row file. Ten phases cover primary registration, friendly rename, City filter/Undo, owned raw historical preview, both Tables, cached idle, mixed selection, revocation and recovery. No alternative JavaScript spreadsheet or fake C callback is used.

The final finite selection contains 8,596 cells in the zero-based model rectangle rows `[0,613]`, columns `[0,13]`. It spans the cached primary, cached same-name Table, uncached600-row Table and blank cells. The actual adapter instrumentation revokes the cached Table's real registration at idle admission. The normal trusted Copy still submits the original three-token vector, including the cached contributor. It fails with `SOURCE_INVALIDATED` before `connection.send` and before `latestId` advances: both remain 16. The workbook/pages/selection/dirty/history are retained, and the pre-loader memory clipboard remains exactly `owned clipboard baseline`, with no write/fallback attempt. [Actual failure frame](images/rejected-mixed-copy.png), [owned phase/token measurements](app/source-bindings.json).

A normal Amount-domain lookup then succeeds using only the original primary token. Export instrumentation is installed but this profile submits no export. The real drop is a deliberate invalidation control, not a physical user action or proof of arbitrary underlying-file mutation. The profile uses one engine, private fixtures and a headless profile; it touches no OS clipboard, user browser tab or physical file panel.

The old-`fe6f179` [negative](binding-natural-red/summary.json) fails because its chooser never returns the new session tokens. It is **not** a specific reproducer of cached-contributor omission. An intermediate `57d459a` App run passed through the rejection and failed only its recovery fixture: frozen columns left Amount outside the visible target, so its contextual click hit a Table. Normal Right navigation fixes that fixture in `a972913`, without changing the product or loosening the accepted recovery assertion.

## Real Worker staging and recovery

The original 192-stage Worker workload remains intact. Its separate inventories still record baseline 146 jobs /325 submitted SQL packets /21 counts; export43 /74 /0; verification8 /7 /0. The additional binding workload records 28 jobs,35 C-submitted packets/host requests, seven local rejections, four host rejections and one budget rejection. Across four sequential Workers, totals are 225 jobs,441 submitted SQL packets and21 counts. These counters do **not** mean 441 engine executions: host-admission rejections intentionally never send SQL to the connection. [Exact inventories](worker/worker-workloads.json), [binding totals](worker/binding-workloads.json), [strict terminal result](worker/worker-driver.json).

The actual owned mixed-statistics budget fixture has 3,089,505 definition bytes, a 61,113-byte relation,61,158-byte DESCRIBE SQL,884,223 manual-text bytes and98,305 bytes of headroom before cache accounting. It uses real owned allocations, not invented byte counters. Replacement DESCRIBE/schema/relation-key/JSON staging exceeds the combined 4 MiB admission: the job denies after one submitted SQL packet and preserves the previous schema/key/token cache. A separate one-packet recovery then reuses that old cache. This is declared retained-C staging accounting; JS/Worker transport and allocator/process peaks are separate. [Three exact C console records](worker/console-selection.txt).

The earlier native review400 regression remains separately scoped in its [pre-chrome summary](native-pre-chrome/summary.json). Its release exclusion passed only in the tool transcript; no durable artifact is invented for that command. The later `e22840d` four native variants and normal/review Wasm are rebuilt, with a new400-stage optimized/O1 pass and both linked native exclusions. [Current native summary](native-review/summary.json), [current release exclusion](build/current-native-exclusion.json), [current O1 exclusion](build/current-native-sanitize-exclusion.json), [current normal/review Wasm absence](build/current-web-absence.json). These are regression/exclusion gates, not a full browser-binding rerun at `e22840d`.

## Expanded inspector header

The pure inspector correction `21bf835`, integrated at `974119a`, moves Collapse/Dismiss left by four UI units and up by half a presented point. The three expected Collapse rectangles come from the retained frozen DOM measurements at wide100, wide150 and narrow100. Dismiss is derived from that measured anchor and the frozen CSS 23-unit button/two-unit gap; it was not separately exported by the old reference capture. All 1,349 existing geometry checks remain, with 138 additional reference/boundary/clip checks. These are pure geometry checks, not real pointer or GPU acceptance. [Pure summary](header-pure/summary.json), [frozen reference measurements](../2026-10-18-steps-sort-input/rail-reference/measurements.json).

The [natural old-product bounds](header-natural-red/pane-rail-wide-100-expanded-dismiss-bounds.json) and [frozen image](images/header-dismiss-natural-red.png) reproduce the displaced wide100 target. The first actual corrected-module run passed wide100 but exposed a separate App geometry bug at150%: panel y139.5/grid y181.5 instead of the reference139/181. The App formula scaled a one-point outer border. `e22840d` changes `(57 + ribbon_height) * scale` to `(56 + ribbon_height) * scale + 1`, preserving that border as one presented point. [Actual origin failure](header-origin-red/summary.json), [frame](images/header-origin-red.png). This is a product defect, separate from the recovery-navigation and earlier Worker fixture corrections.

Both current515-check real-C gates retain the earlier rail color/border/full-any coverage assertions and add six expanded-header frozen hover/semantic proofs: Collapse and Dismiss at wide100, wide150 and narrow100. Normal Collapse/Expand/Dismiss and capture/cancel preserve model/pages/selection and zero presentation-only jobs. The untransformed run owns exact eligible compositor ink; the transformed run retains its pinned Chrome mapping and explicit coverage/boundary exclusions rather than claiming a universal filtering rule. [Untransformed measurements](rail-untransformed/measurements.json), [transformed measurements](rail-transformed/measurements.json), [wide150 Dismiss](images/header-wide150-dismiss.png), [transformed narrow100 Collapse](images/header-transformed-narrow100-collapse.png). The expanded short narrow150 controls remain excluded; the retained rail remains tested in all four cases. Expanded border/shadow parity is not claimed by the header check.

Short narrow150 remains a distinct open layout: the reference has an outer inspector scrollbar and sticky title in addition to the inner Steps list. Its 13-presented-point lane under 150% zoom is not a universal fixed inset. C still compresses short content instead. Expanded border/shadow, the outer scroller/focus lifecycle, full inspector/menu parity, physical native input and broader browser acceptance remain open.

The measurement-only [Steps reference provider](steps-scroll-reference.json) retains four default anatomies and the short150 outer-end, inner-end and reopened states, six actual reference rows, raw DOM clips/metrics/CDP content-padding-border quads, exact source/prebuilt asset hashes and all20 capture identities. The original frozen capture `041544-reference-steps-scroll-24743` took20.355s total. Its derived `boxModelRightBand` originally reported zero by subtracting padding-right from border-right; the raw quads were valid. The corrected `3473b35`/main `71e4d2d` derivation adds actual CSS padding to the content edge: the short150 allocated outer band is627–640 and inner band614–627. This is post-capture raw-quad derivation/replay, not a new browser capture or native Steps acceptance. Integer CSSOM extents do not prove painted scrollbar-thumb/track pixels. Pure layout tests alone do not establish integrated App scrolling behavior.

## Current distribution pointers

The signed local bundle is quietly refreshed to `e22840d` in0.715s, with source/signature verification and no launch. [Package identity](package.json) records the current native review/header gates separately from the earlier App680 binding and Worker221 artifacts at `57d459a`. The unchanged source scopes are intentional.

Only the owned8767 service is refreshed, from the previously recorded owned PID18379 to PID27211, serving review artifact `e1d15a1d91da9953f356` at `e22840d`. Existing queue IDs are retained with zero reports before/after; no user browser tab is reloaded and no native app is spawned. [Service identity](service.json). This proves accepted artifact serving and queue retention, not new end-to-end report delivery, automatic tab refresh or a new focused binding pass on the later artifact.

## Reproduction and retention

```sh
node --test web/official_worker.test.mjs web/engine_host.test.mjs web/official_worker_probe.test.mjs
python3 tools/build_web.py --worker-driver
python3 tools/test_web_app.py --no-build --worker-driver
python3 tools/test_web_app.py --no-build --review --source tests/fixtures/transactions.csv --source-bindings-only --dpr 2 --canvas-transform
python3 tools/test.py fast --modules inspector --config both
python3 tools/test_web_app.py --no-build --review --source tests/fixtures/transactions.csv --pane-rail-only --dpr 2
python3 tools/test_web_app.py --no-build --review --source tests/fixtures/transactions.csv --pane-rail-only --dpr 2 --canvas-transform
```

Build a matching immutable App artifact before `--no-build`; runner and product source hashes are recorded separately. Actual engine/desktop tests remain serialized under the existing lock. One attempted native review run refused that lock while the reference capture owned it; no native app was spawned, and the subsequent full run passed. This is desktop ownership working, not an App failure.

`retention.json` lists unchanged source copies and bounded measurement/console extractions with original hashes. Captured registration IDs are the tested non-authentication session identities. Authentication/capability tokens, input/event streams, generated databases, report bundles and repeated historical passes are excluded. Long compositor boundary/exclusion/mismatch diagnostics retain counts, canonical hashes and first/last samples alongside exact ink/full/any envelopes; their original result hashes identify the full run. `files.sha256.json` seals the retained files except itself. No runtime or compiler was launched during documentation curation.

The earlier `build/web-absence.json` was copied and hash-checked before the shared `build/web/review-absence.json` pointer was refreshed. It keeps the binding artifact's earlier hashes; `build/current-web-absence.json` retains the later `e22840d` result. The changed shared pointer is explicit in `retention.json` rather than relabeling either snapshot.
