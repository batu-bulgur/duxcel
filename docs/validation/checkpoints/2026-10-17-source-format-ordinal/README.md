# Native source-ordinal feasibility measurement

17 October 2026, plan branch `ac569a4`, standalone C99 benchmark. **No production code, workspace schema, source-format feature, App, browser or GUI was changed or accepted.** All limits and semantics in [the design plan](../../../design/SOURCE_FORMAT_IDENTITY.md) remain proposed.

On this 3,475,226-row NYC Parquet, a pre-filter `row_number() OVER ()` has a material filtered-query cost. The retained `EXPLAIN` plans put the date predicate above `STREAMING_WINDOW`; baseline and Parquet `file_row_number` retain the predicate inside `READ_PARQUET`. The measured metadata candidate preserved the same original record IDs and business values with much smaller warm overhead. This supports testing that candidate for single-file native Parquet, rather than adopting a universal window on the strength of an unfiltered first page.

| Query shape | Business only, fresh / warm median ms | Pre-filter window, fresh / warm median ms | Parquet metadata, fresh / warm median ms | Window / business warm |
| --- | --- | --- | --- | --- |
| First 256 rows | 18.788 / 18.437 | 21.294 / 14.972 | 17.942 / 15.688 | 0.81× |
| January 15 date filter, first page | 50.225 / 45.707 | 108.589 / 101.123 | 45.022 / 45.110 | 2.21× |
| Unfiltered offset 1,000,000 | 44.888 / 44.644 | 77.962 / 73.072 | 49.037 / 45.432 | 1.64× |
| Same date filter, offset 50,000 | 21.888 / 21.433 | 106.812 / 104.515 | 22.869 / 21.726 | 4.88× |
| Sorted first page | 276.349 / 273.758 | 422.435 / 427.465 | 383.659 / 286.137 | 1.56× |
| Same date filter, sorted first page | 50.243 / 49.379 | 269.087 / 268.824 | 51.530 / 49.482 | 5.44× |

Each fresh sample used a separate private in-memory database after a schema `LIMIT 0` and settings query; five subsequent identical queries supplied the warm median. This is **engine-session cold**, not cold disk or application startup. OS cache was uncontrolled and the file had been used previously. Metadata warm ratios were 0.85–1.05× baseline, but one fresh sorted-first metadata sample was 1.39× baseline; do not turn warm medians into an unconditional latency claim. CPU/I/O contention with the integration owner's authorized compiler/native-engine lane was possible.

Settings were installed DuckDB **v1.5.6**, `memory_limit=512MB` (reported as 488.2 MiB), two threads, insertion-order preservation true and the default external file cache true. The source was read-only `tests/downloads/yellow_tripdata_2025-01.parquet`, 59,158,238 bytes, SHA-256 `9af277e4c0d3f9deb30644da822981e1e7df6af58313170fd3aa8a474485488a`. Its full file identity and digest matched before/after the run. Spill directories were private to each child and removed after it exited.

All 18 measurement sessions returned exactly 256 rows. Their twenty business column names/types and every exact VARCHAR conversion/SQL NULL flag matched baseline. The optional 21st metadata column stayed outside that business projection. Ordinal variants kept the ID selected in their engine result, so the optimizer could not simply prune the window. This is standalone conversion validation, not acceptance of production Page integration.

The full-source inspection proved 3,475,226 unique original window ordinals, range 1–3,475,226, equal to `file_row_number+1` for every row. Raw metadata ranged 0–3,475,225. Both variants produced the same IDs after all measured filters/sorts, with no duplicate IDs inside any returned page. Sorted cases ordered all twenty business fields, then the identity variants used ID as the final tie-breaker; equal business duplicates therefore remained comparable to baseline. The data inspection and 108 timed queries took **13.889 s** total across 19 sequential child processes. There was at most one compiler/engine process in this worktree at any time.

Measurement-child process high-water RSS was about **41.0–82.0 MiB**; exact byte samples and before/after resident snapshots are in the artifact. RSS includes the library/allocator, the six query runs, validation and final EXPLAIN; it is not engine-only memory or a per-query peak. The full-source distinct-ordinal inspection peaked separately at 228.1 MiB. Lower window RSS does not establish a memory win or explain all of its latency. C string conversions and correctness checks were outside `query_ms`; the separate conversion timer runs two complete digest passes and is not an App Page timing.

The [self-contained JSON record](feasibility.json) includes exact query SQL, settings, all first/warm samples, selected plans, metadata/ID hashes, source/binary/library hashes, compiler command and the exact standalone C/Python source used. The larger raw result pages remain under the recorded ignored worktree build path. To reproduce using the existing installed toolchain and the same input:

```sh
python3 - <<'PY'
from pathlib import Path
import json
data = json.loads(Path('docs/validation/checkpoints/2026-10-17-source-format-ordinal/feasibility.json').read_text())
out = Path('build/format-ordinal')
out.mkdir(parents=True, exist_ok=True)
(out / 'benchmark.c').write_text(data['reproduction']['harness_source'])
(out / 'run.py').write_text(data['reproduction']['runner_source'])
PY
cc -std=c99 -O2 -Wall -Wextra -Wpedantic \
  -I/opt/homebrew/opt/duckdb/include build/format-ordinal/benchmark.c \
  -L/opt/homebrew/opt/duckdb/lib -Wl,-rpath,/opt/homebrew/opt/duckdb/lib \
  -lduckdb -o build/format-ordinal/benchmark
python3 build/format-ordinal/run.py
```

The runner uses the original absolute dataset path stated above; adjust it only if the identical input resides elsewhere, and record that change. It executes no shared writable fixture/database, installs nothing, and opens no app/window. Native and official Wasm engine compatibility, CSV identity cost, reader-option/user-column collisions, globs/multiple files, refreshed files, arbitrary SQL grain, source-version verification, format persistence, and frame/input latency are **not measured or accepted** here. Normal application prepared-query performance may differ from this direct-query benchmark. There was no sanitizer acceptance for this disposable measurement harness.

The installed reader option and filter/projection behavior are documented by the official [DuckDB 1.5 Parquet documentation](https://duckdb.org/docs/current/data/parquet/overview). This experiment selects no production architecture; it supplies the missing cost/order evidence before S1 implementation.
