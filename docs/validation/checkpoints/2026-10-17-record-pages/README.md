# Native raw Parquet Page provenance

Accepted native/data code through `38601b7`, 17 October 2026. This is the Parquet portion of S1 in the [formatting plan](../../../design/SOURCE_FORMAT_IDENTITY.md). Formatting still uses coordinates; record selectors, captured format intents and verified save/reopen are unfinished.

## Working behavior and ownership

`record_sql` builds an opt-in raw business projection with a collision-free final UBIGINT identity. The connected native path uses single regular-file Parquet metadata (`file_row_number+1`) before filtering. Requested sorts gain an ID tie-breaker; unsorted data gains no global sort. CSV/demo window candidates have SQL tests but are not enabled in native Pages. Queries, DISTINCT, grouping, database relations, unverified/multiple files and a business `file_row_number` collision retain ordinary business SQL.

Pages own their bounded IDs and reader scope with `free`; DuckDB cell text keeps `duckdb_free`. Geometry-only canvas views clear provenance; borrowed 12-row previews bound its count. Whole Page moves and wire replacement preserve allocator ownership. Incoming browser Pages remain identity-free. Optional metadata is omitted when it would make an otherwise valid near-budget business page fail; the reader stamp is retained. These stamps/keys are session hints, not content digests or persistent formatting bindings.

Business column indexes/types/text remain unchanged, including 256-column tables. Hidden IDs never enter DISTINCT/PIVOT grain, column filters, copied fields, statistics or exports. Positional consumers use the same tie ordering. An observed file change rejects publication into the live/spare main cache, imported table or historical preview; explicit refresh can accept the new version. The synchronous fetch checks again after exact counting. Separate asynchronous COUNT/copy/statistics still lack an expected-source-version contract; no snapshot isolation or verified immutable source claim is made.

## Focused acceptance

| Gate | Result / elapsed | Evidence |
| --- | --- | --- |
| SQL + integrated Page, release and ASan/UBSan |48,904 +31,467 checks each; warm total1.666s|[record gate](records/summary.json)|
| Native source-record workflow, O1 ASan/UBSan |10 stages,7.078s App /8.860s total;477 background checks|[native summary](native-records/summary.json), [App log](native-records/layout-record-pages.txt)|
| Existing table + public NYC/PIVOT workflows, same sanitizer binary |73+8 stages,34.347s+6.401s;47.567s including fixture preparation|[summary](table-nyc/summary.json)|
| All19 read-only modules, both configurations |46.110s including30.500s build|[headless summary](headless/summary.json)|
| Core→workbook, native allocator and browser wire ownership, both configurations |65.648s; workbook sanitizer41.331s retained|[integration summary](engine-integration/summary.json), [scoped driver](integration_checks.py)|

The Page suite compares exact fields and IDs over offsets0/256/512, duplicate physical records, NULL/empty/literal text, BIGINT beyond2^53 and DECIMAL. It checks cache preparation/query reuse, typed filtering/sort ties, copy/statistics/export ordering, unsupported fallbacks, 256 business fields, empty pages, optional budget fallback, source replacement, moves and retained ownership after engine close. Wire tests preserve populated old records on malformed decode and clear them on valid/zero-row replacement, including a source-buffer alias into an old ID allocation. Preview tests reject changed provenance without consuming the prior pages.

The native test additionally proves the App rejects a replacement while retaining the last valid page, then refreshes and uses the query fallback. Captures were inspected. All windows stayed hidden under the desktop lock; the normal user process was untouched. The first attempts exposed a stale App object, a missing focused-bootstrap jump and an incorrect test assumption about arbitrary query tie order. They remain failed local runs (`154405`, `154543`, `154702`), not acceptance. Bootstrap unit checks now require the actual jump branch; the query assertion requests an explicit secondary sort. No assertion/animation wait was removed.

## Complete Page-fetch measurement

[Measurement](performance.json) records the binary/source and public file SHA,12 fresh private engine sessions and72 full Page fetches (one first +five warm each). DuckDB1.5.6 uses512MB, two engine threads and insertion-order preservation. The baseline uses an explicit identity source query to keep the ordinary business-only path. Engine-session first fetch is not OS-cache cold; one compiler ran concurrently.

| Case | Business-only warm median | With records | Ratio |
| --- | ---: | ---: | ---: |
| First page |18.947ms|16.830ms|0.89×|
| Filtered |20.000ms|16.133ms|0.81×|
| Deep |47.456ms|46.675ms|0.98×|
| Filtered deep |48.834ms|47.923ms|0.98×|
| Sorted |173.218ms|190.808ms|1.10×|
| Filtered sorted |177.816ms|185.267ms|1.04×|

Each case reused one prepared page statement for six queries; metadata added2,220bytes/Page in this worktree. The identified filtered first fetch was172.598ms versus24.065ms baseline and is retained, not explained away. Sorted IDs impose an extra deterministic key. These observations include conversion/ownership and worker-side allocations, not rendering/input latency or a universal speedup. No database/filesystem work was added to repaint.

The separate [SQL seam record](sql-seam.json) includes earlier release/sanitizer checks and exact ordinary SQL/allocation compatibility across60 combinations. It is not a second whole-App acceptance claim. Browser input/UI, the complete native exact port and record formatting remain separately scoped.
