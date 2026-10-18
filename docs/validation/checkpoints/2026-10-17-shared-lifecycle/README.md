# Shared App lifetime and owned pages — 17 October 2026

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

This integration preserves native behavior while giving a future browser host a persistent start/frame/finish contract. The native runner uses that same contract. App/options/render-batch state now has explicit heap lifetime; option strings are copied. One live session is enforced. Before closing GLFW, macOS input monitors are detached, queues cleared, installed menu targets removed and the prior delegate restored. Two successive hidden sessions accept a locally addressed Control+Tab through the actual Cocoa monitor without foregrounding either window. Embedded scenario injection flags remain one-run-per-process test state; ordinary sessions may restart.

`lifecycle.json` records **26 checks per optimized/ASan+UBSan configuration**, plus the **477-check** hidden-window probe. Tests release and overwrite caller options before yielding frames, require exactly three frames and stable false completion, verify owned screenshot output, reject a concurrent session, and finish early with query work pending. This does not establish browser event-loop/IME behavior.

`review.json` records the integrated **26-stage reporting flow per optimized/sanitizer configuration** and the background probe. Review assertions now require nonzero timing samples for every capture/frozen frame. The performance observations in the earlier native-review checkpoint remain scoped to their own source/binaries, not this newer heap lifetime.

`native.json` records **65 focused sanitizer stages** in **32.129 s**: base calculation6, workbook4, document10 and real PIVOT45. This is not a whole-inventory regression or exact-UI sign-off. The source and unchanged binary hashes are retained in each manifest. All windows were hidden before creation.

The Page model/helpers are extracted from engine execution. C-owned text defaults to free; native result pages explicitly retain duckdb_free, including nested totals and page moves. Pure **40** and real-engine **55** allocator checks passed per configuration. The independent page lane also passed native core/workbook **1,661 /1,683** per release and ASan/UBSan; its source/results are retained in `page-agent.json`. The pure model still uses DuckDB's header-only type enum, and browser backend integration remains pending.
