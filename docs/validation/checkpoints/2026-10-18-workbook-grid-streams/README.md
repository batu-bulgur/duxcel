# Bounded Columns/Cells Header and acknowledged metadata streams

The shared C partial Header now owns column presentation and sparse editable-cell records, populated through one official DuckDB-Wasm execution per streamed family. Actual acceptance at `2f2429c` observes **79,551 C checks / 282 external checks**, four immutable native archives, six completed Headers and 62 metadata requests. It does not construct or publish a complete Workbook, activate browser Open/Save, execute stored source SQL or establish App UI parity.

## Source and evidence boundaries

| Layer | Tested source | Result and elapsed scope | Receipt |
| --- | --- | --- | --- |
| Columns component | private `3bd9b55` | 20,988 checks/configuration; optimized/O1 ASan+UBSan; 12.203 s including builds | [manifest](pure-columns/manifest.json), [original seal](pure-columns/SHA256.json) |
| Cells component | private `5933eef`, with Columns prerequisite | 21,980 checks/configuration; optimized/O1 ASan+UBSan; 11.118 s including builds | [manifest](pure-cells/manifest.json), [original seal](pure-cells/SHA256.json) |
| Integrated pure reader | root `d49a720` | 21,980/configuration; 12.438 s total | [manifest](root-pure/manifest.json), [summary](root-pure/summary.json) |
| Real native metadata comparison | private `cccfba9` → root `aa31ab0` | 10,567 checks / 87 cursor metadata executions / 101 typed feeds per configuration; optimized 2.755 s, O1 sanitizer 10.931 s runtime | [manifest](native/manifest.json), [50-file closure](native/preprocessor-closure.json), [source comparison](native/root-source-comparison.json) |
| Callback helper, initial | private `e77418b` → root `d5045f5` | 180 Node groups: 17 helper + 163 adjacent; 3.973 s | [manifest](helper-initial/manifest.json), [log](helper-initial/node.txt) |
| Failed-retirement correction | private `fe4a766` → root `2f2429c` | natural old-helper failure, then 181 Node groups: 18 helper + 163 adjacent; 4.280 s | [natural red](helper-retirement/natural-red.json), [passed manifest](helper-retirement/manifest.json) |
| Actual official Worker + C consumer | root `2f2429c`; driver artifact `9d8ae57beaa41eb640b3` | 79,551 C / 282 external; 7.937 s browser, 17.975 s CLI, 18.331 s enclosing invocation | [C report](actual-grid/driver.extract.json), [wrapper](actual-grid/summary.extract.json), [transport](actual-grid/codec-read-transport.json) |

The final enclosing batch takes **22.307 s**, including nine harness tests, 32 runner tests, a cached strong exclusion check and the actual Grid command. [Exact commands and times](actual-grid/manifest.json) distinguish these from browser runtime. The driver build takes 1.231 s with no C source rebuilt; the wrapper's 8.334 s setup build field includes its own prerequisite work and is not that driver-build duration.

The native receipt retains separate optimized/sanitizer build times (4.862/11.460 s), ordinary 1,611-check Header controls (0.602/2.714 s), and 3.554 s emitter publication. Its 36.878 s total is not an App runtime. Native uses SDK DuckDB 1.5.6; the actual official Worker reports `v1.4.3` in the transport receipt. The 87 cursor executions are not every SQL operation in the native process. The 62 actual requests exclude archive attachment/detachment administration.

## Owned C metadata

`WORKBOOK_CODEC_COLUMNS` and `WORKBOOK_CODEC_CELLS` append family IDs 15 and 16. Each can be armed once at Header readiness, independently of the other analysis continuations. Existing constructors and unarmed readiness behavior remain unchanged. Columns are mandatory for writer versions 12–14. Missing Cells defaults to an empty ready family for 12/13 and rejects for 14.

The named, ordered projections are:

```sql
SELECT sheet_id,column_id,name,width
FROM _duxcel_workspace.columns ORDER BY sheet_id,column_id LIMIT 1048577

SELECT sheet_id,row_id,column_id,text
FROM _duxcel_workspace.cells ORDER BY sheet_id,row_id,column_id LIMIT 1280001
```

Columns require contiguous zero-based indexes, at most 16,384 per worksheet, and finite original DOUBLE widths in [52, 1200] before float conversion. Empty and duplicate names are valid; the reader adds no arbitrary name-length cap. Cell records require strictly increasing `(sheet,row,column)`, row below 2^40, column below 16,384, at most 20,000 records and 4 MiB text including terminators per worksheet, with at most 65,536 bytes per value. Empty text remains an actual sparse record; missing coordinates remain absent; literal `NULL` remains text. Historical native CTAS duplicate acceptance is explicitly outside this strict known-writer decoder's compatibility claim.

Capped geometric arrays charge their capacity. Preflight reserves retained old output, old/new arrays, staging and all new strings before allocation; Columns also defers its unbounded UTF-8 scan until budget admission. Failed chunks preserve cursor, owned bytes, graph/array/string identities and caller input. Successful growth preserves existing string identities. Explicit empty EOF remains necessary after full or short nonfinal chunks. Header destruction frees these owned arrays without linking native View/CellStore history implementations.

The pure receipts preserve all 20,092 previous assertion definitions before adding 896 Columns and 992 Cells checks. Tests cover irregular chunk boundaries, 16,384 columns, 20,000 cells, large names under explicit budgets, text/raw-byte boundaries, finite-width rounding, invalid order/types/UTF-8/NUL, exact/one-under growth budgets, stale-before-borrow, poisoned inputs and old-output retention. They do not enumerate the entire 1,048,576-column / 1,280,000-cell global maxima.

## Closed native archives and actual stream workflow

The native factory creates three small writer-version 12/13/14 archives with worksheet counts `[0,3,4]` for each grid family, and a physically reordered version-14 archive with counts `[257,28]`. Each large family has one actual result split into 256+29 data rows. The version-14 small case includes a 65,537-byte column name and a 65,536-byte cell value; final coordinates include row 2^40−1 / column 16,383. Native comparison checks the exact shared-C values after result/connection/native model closure. [Emitter checks](native/emitter-results.json) retain immutability, independent schema inspection and failure cleanup.

The actual gate creates a fresh closed four-file set with its own file hashes; database container bytes need not equal the earlier private emission. The [native creation protocol](actual-grid/codec-native-create.txt), [sealed fixture inventory](actual-grid/codec-fixture-manifest.json) and [unchanged served-file receipt](actual-grid/codec-native-provenance.json) bind the actual run to those bytes. No archives or generated databases are copied here. The root also independently compares the 50 native compilation inputs and two SDK files in [its integration witness](initial-integration/native-root-comparison.json). A [selected committed-input witness](source-witness.json) separately proves that the final actual reader and pure test bytes equal the accepted Cells component; it is not a substitute for a complete compilation closure.

The Worker checks original schemas before using C-built VARCHAR projections with original type sidecars. Each stream delivers owned DXP1 chunks of at most 256 rows, then a separate empty EOF. The helper reacquires the heap after allocations, copies temporary wire/scalar strings, calls C synchronously, and frees every temporary buffer **before** returning exact numeric-1 acknowledgement. C completion occurs after EOF acknowledgement and reader/adapter retirement; EOF alone cannot publish a Header.

The actual inventory is:

- 12 Meta, 12 Sheets, 12 Columns, 10 Cells and 16 DESCRIBE requests: **62** total; each normal archive uses eight.
- 53 chunk callbacks, 47 accepted acknowledgements and 22 completion callbacks; successful streams are 10 Columns + 6 Cells, with two Columns + three Cells error streams.
- Five rejected feeds at first rows `[256,256,256,256,0]`: original DOUBLE just above 1200 despite float rounding, required column-name NULL, required cell-text NULL, late duplicate cell coordinate and concrete retained-input budget pressure.
- One held second Cell chunk cancels after the first 256 rows. The old completion and one explicitly replayed old chunk are rejected by cookie/ticket before borrowed reads. Only after drain/replay does the new cancellation acknowledgement permit a whole-candidate retry. A further fresh candidate recovers.
- 19 accepted Columns and 12 accepted Cells data chunks are poisoned after adoption. Prior independent Header graph/text/array identities remain intact through failure and final archive detachment.

The maximum completed Header is **169,576 bytes**, within the 2 MiB Header limit. The peak accounted C-boundary coexistence is **2,097,214 bytes**, within the separate 4 MiB input/retained limit. This counts the test's owned C structures, wire, scalars, staged data and old Header; it is not an engine, Arrow, allocator, browser, whole-process or RSS bound. The actual representative archives do not establish full-limit streaming for every allowed dataset.

## Natural failures and corrections

The retained native initial failure was a test assumption: `cells_get()` uses empty display text for a missing coordinate. The corrected test inspects sparse record presence separately and retains both present-empty and absent checks. No product change or normalization was made. [Original failure](native/initial-fixture-failure/manifest.json).

The initial callback-helper test run collided result-array properties with the required `done`/`held` callbacks and expected the wrong error class for malformed dedicated requests. Only test fixtures changed; the original helper source remained unchanged for that retry. [Original receipt/log](helper-initial/first-fixture-failure/manifest.json).

Two actual synthetic transport probes at `1b0772b` and `aa32037` rejected raw BOOLEAN streaming: expected `true`, received **109**. The diagnostic observed physical Arrow `Int8`; this is not accepted BOOLEAN parity. The later `f1b3694` probe uses an explicit BOOLEAN-to-VARCHAR projection with the original descriptor sidecar. It passes 6,600 probe / 18 external checks, six SQL executions and six posted turns, proving exact values, cleanup-before-EOF/final acknowledgement, suspended-ACK cancellation, false-ACK rejection and two ordinary-query recoveries. Its 5.530 s CLI / 5.919 s enclosing invocation and 9.260 s batch remain transport-only, separate from the C/native Grid gate. [Raw failure](probe-raw-boolean/metadata-stream-probe.json), [physical schema](probe-physical-schema/metadata-stream-probe.json), [canonical proof](probe-canonical/metadata-stream-probe.json).

The read-only integration audit then found a real helper error path: `cancel()` discarded an error completion when the adapter reported failed reader retirement. The added mock-reader regression fails naturally on the old helper in 0.402 s. The correction rejects cancellation after the single error completion/drain, preventing the host from acknowledging success under new request IDs. The test verifies poisoned registration remains denied, the held chunk stays owned until close discards it, and the caller still owns engine closure. This is simulated reader-cleanup failure using the actual adapter, not a failure injection into the real Worker. [Red](helper-retirement/natural-red.txt), [181-test green](helper-retirement/node.txt).

## Shipping and remaining scope

Fresh normal/review Wasm builds at **`7a1c7d3`** take 16.377/16.774 s; artifacts are `7f7de0534d342fa08f73` / `ba727756ed340db23ba6`. Their [build extracts](initial-integration/app-build.extract.json) and [review counterpart](initial-integration/review-build.extract.json) retain exact original source/hash provenance. The [fresh exclusion record](initial-integration/shipping-exclusion.json) covers native symbols and normal/review Wasm markers. A stronger cached check at `2f2429c` checks those same artifacts, including absent standalone stream-test assets; it does not relabel them as fresh `2f2429c` App builds. The earlier standalone driver at `7a1c7d3` is likewise a separate [5.444 s build receipt](initial-integration/first-driver-build.extract.json).

There is no new actual App UI or Columns172 run, original Worker247 rerun, native package/service refresh, user-tab reload, clipboard/file-panel action or review-queue delivery. Product distribution pointers remain outside this capsule. Final object placement, complete candidate validation, owner counters and source-format payloads, historical migration, writer/output adapters and browser workbook Open/Save remain unfinished.

## Retention

[provenance.json](provenance.json) records each original path, byte count and SHA-256. Exact copies and bounded JSON extracts are distinguished; extracts retain the original file hash and describe omitted duplicate transport/vendor fields. The component's original seals refer to their original relative receipt layout. [hashes.json](hashes.json) seals this capsule's payload layout. No binary, database, vendor asset, screenshot, report archive or full browser event stream is included.
