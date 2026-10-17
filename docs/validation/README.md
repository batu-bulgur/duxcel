# Validation and measurement

Evidence belongs to a particular source and binary, not to the project forever. The initial Git snapshot includes changes made after the accepted pre-Git binary; it must not inherit that binary's native acceptance automatically. [Current feature status](../STATUS.md) records that boundary.

## Preserved full checkpoint

The [17 October author-menu checkpoint](checkpoints/2026-10-17-author-menu/) preserves the full scenario summary, source/binary SHA-256 manifests, pure log and newer focused unit logs. The original per-scenario logs and screenshots remain locally under ignored `tests/results/native-authoring/`; they were not deleted or added wholesale to Git.

| Evidence | Scope / result |
| --- | --- |
| `author-menu-regression-summary.txt` | 18 native application scenarios, 988 stages, all exit 0; separate GPU probe passed |
| `author-menu-binary-sha256.txt` | Frozen executable SHA `98724ccaf6a57102667c0cb305e8a94db3048ce7e4ef394ff7d782ddad77eac4` |
| `author-menu-source-sha256.txt` | Pre-Git source manifest, including metadata files present at capture; not the new Git tree |
| `author-menu-full-pure.txt` | 40,575 assertions per release/ASan+UBSan configuration at that checkpoint |
| `active-cell-grid.txt` | Later grid/focus changes, 16,812 checks per configuration; not a native integration pass |
| `active-readout-pure.txt` | Later bounded snapshot helper, 133 checks/configuration; application integration unfinished |

Total native checkpoint time was approximately **860 seconds (14m20s)** including the GPU probe. Layout took 330.26s, filter 113.03s, query 66.15s and table 60.62s. Fixed 24-frame stage pacing and continuing to the frame budget after completion explain much of this cost. Seventeen warm read-only module binaries took approximately 0.26s total during the audit; compilation was excluded. Assertion volume is not itself the bottleneck.

## Performance interpretation

The full 555-stage layout workload recorded 13,897 frames, draw-submission CPU p95 **3.204ms**, paced mean **23.63ms**, p95 **17.99ms** and max **1,377.84ms**. These include loading, queries and automation. A separate interactive owned QA session observed draw CPU p95 **2.349ms**. Neither value measures GPU execution or end-to-end input latency, and they are not a controlled before/after comparison.

Earlier matched renderer repeat pairs, dataset timings and limits remain in the [benchmark archive](../archive/BENCHMARKS-2026-10-17.md). Public data includes a 3,475,226-row/20-column NYC Parquet plus full CSV and a checked Parquet round trip. Downloads are reproducible with `tests/download_data.sh` and remain local/ignored. This audit did not redownload them or claim fresh dataset benchmarks.

## New records

Use the committed [test workflow](../TESTING.md) for named tiers and result manifests. Record commit, dirty-state/source hash, binary hash, command/profile, expected and observed stages, exit status, duration and log locations. A missing dataset, skipped test, timed-out frame budget or changed binary must remain explicit. Never count a partial run as a full pass.

Matching-state visual acceptance additionally records viewport, UI scale, canvas zoom, selected source/object, fixture, reference state, native capture and pointer/keyboard result. Engine regression complements this evidence; it does not establish exact interface fidelity. See [the roadmap's parity states](../ROADMAP.md).
