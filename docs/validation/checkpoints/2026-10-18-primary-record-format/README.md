# Captured primary record styles and verified reopen

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

Runtime integration `ba79581` includes `49ef2ce`/`11632a9` captured commands, `b315694` automatic primary restoration and `309fe37`/`ba79581` typed GUARDED codec support. This is a narrow native primary CSV/Parquet feature. The packaged native app and persistent browser origin remain at `7c451c3`; no user window or tab was launched/reloaded.

Actual interaction and analytical semantics: the30-stage native profile [734,764) creates600 private Parquet records, including physically identical pairs, tied sort keys, SQL NULL/empty/literal text, BIGINT above2^53 and exact DECIMAL. A captured110-record selection crosses a256-row cache boundary, then selection moves before completion. Original physical IDs and field ordinals retain the styles through both sort directions, filtering/removal/restoration and both-page eviction. One duplicate is colored independently. First binding plus style is one owned composite action; Undo/Redo retains issued version high-water, and no-op formatting preserves redo/dirty/history. Explicit cancellation, superseded Page work and a changed format revision cannot apply stale styles.

Actual v14 Save/Open re-verifies the primary file/schema/reader/engine in the worker before exposing RAW record tokens. This restoration adds no format action, ledger issuance or dirty change. Explicit cancellation remains terminal for its observed context; returning to the sheet verifies anew. Replacing the file at the identical locator with the same schema but different content leaves styles suspended, preserves whole-column defaults, rejects later finite commands atomically and does not rehash on idle frames. Saving the suspended metadata retains its selectors. RESULT/Table/PIVOT/mixed activation and automatic finite legacy/history migration remain open.

| Gate | Exact source | Result and elapsed |
| --- | --- | --- |
| Primary native30 optimized | `ba79581` |15.163s App /24.504s total;29 jobs,19 counted SQL,5 counts |
| Primary native30 ASan/UBSan | `ba79581` |17.677s App /22.012s total |
| Adjacent source-format43 ASan/UBSan | `b315694` |31.900s App /33.117s total; RAW restoration versus suspended RESULT/Table/matrix/preview/totals, manual/broad defaults and real Save/Open/overwrite |
| Adjacent prepared journal4 ASan/UBSan | `b315694` |2.662s App /4.327s total |
| Native review400 optimized | `ba79581` |17.302s App; three actual frame-bound reports and owned source/binary identity |
| Official Worker192 | `ba79581` |13.744s total; native verification/collection unsupported without disturbing cached query recovery |
| Integrated models, optimized and ASan/UBSan | `ba79581` |1,289 Worker digest/reservation;85,492 collector;438 Worker collection;9,876 composite journal;1,537 aggregate budget checks/configuration. Per-executable elapsed was not instrumented in this batch. |
| Typed GUARDED codec component | original `0603337`/`3385dff`, integrated `309fe37`/`ba79581` |1,003 checks/configuration:11.174s optimized /82.165s sanitizer; legacy635 and v13 owners393 also pass. Exact compiled component hashes match integration. Synthetic complete proofs; no App migration claim. |

Normal/review/Worker Wasm builds and native optimized/sanitizer plus Wasm production-exclusion checks pass. Browser workload counters remain146/325/21 (baseline jobs/SQL/counts),43/74/0 (export),8/7/0 (verification/recovery), with three rejected native-only jobs. The192 stages ran across three sequential official Workers. This is not browser record identity or workbook persistence.

Visual evidence: the four PNGs are actual hidden native ASan frames at1440×988 logical/DPR2,100% UI/canvas scale. `returned.png` shows separate physical-duplicate color; `reopened.png` shows verified saved formatting; `changed.png` shows actual new source values and suspended styles. These are formatting/workflow evidence, not a new full exact-prototype UI parity sign-off. The optimized draw-submission CPU p95 was1.016ms, excluding swap/pacing/GPU. Whole-frame p95 was17.88ms/max407.11ms across query/load/capture work; neither is end-to-end input latency.

Pinned NYC benchmark: source `11632a9`,59,158,238-byte `yellow_tripdata_2025-01.parquet`, exact SHA/provenance in `nyc-manifest.json` and two passes in `nyc-run.jsonl`. SDK digest60.408ms first /28.675ms repeat;20,000-ID collections6.027/4.725ms unsorted,6.107/5.839ms at offset1,000,000,32.421/30.237ms with January15 timestamp filter plus descending amount/tie sort. Each result is160,000bytes, one counted execution and no count query; no ordinary business Page is requested. Source remains unchanged; private engine uses384MiB/two threads. Engine scan/sort/temp remains possible. This is not a cold-OS-cache, Worker/UI-latency, allocator-cap or before/after speedup claim.

Reservations are explicit conservative categories: captured schema/Analysis<=256KiB combined, complete-format current/history/ledger, active+pending+published Worker ownership, C SQL staging, Page IDs and I/O snapshot allowance. Canceled active work remains reserved until actual cleanup. Full engine diagnostic allocations are not capped by the reservation allowance; allocator/RSS/absolute active-error peak is not established. No LSan claim is made on this macOS SDK.

The initial native producer test stopped at stage747 because the new cancellation fixture used invalid palette color5. It was corrected to valid color2 in`11632a9`; all assertions and waits were retained. Later30-stage passes include automatic restore and the complete stale/save/reopen path. Current inventory is1,197 application stages; no new full-inventory pass is claimed.
