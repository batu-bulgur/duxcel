# Pivot and Marks definitions

Accepted source-scoped metadata evidence, 18 October 2026. Pure, native, dedicated transport and actual Worker gates reconstruct persisted Pivot and Marks definitions; existing metadata profiles, fresh Wasm exclusion and broader Worker247 pass separately. Browser Open/Save remains unavailable. This does not establish App publication, analytical execution or visual parity.

The earlier Table-source chain retains its [own accepted scope](../2026-10-18-workbook-table-sources/). This continuation adds no Plot request and executes no stored source SQL. It exposes borrowed const definitions from an independently owned opaque Header after the complete chain succeeds; it does not expose a partial Workbook.

| Available scope | Source | Result |
| --- | --- | --- |
| Private pure reader | `2fdda31` → `8acae02`, three byte-identical files | 19,150 checks/configuration: all 13,675 earlier checks retained plus 5,475 Pivot/Marks checks; optimized/O1 ASan+UBSan, 8.943 s including builds |
| Integrated pure reader | `8acae02` | Both configurations pass; nested runner 10.340 s including 8.564 s build, enclosing subprocess 10.772 s |
| Native comparison/emission | `3b1ad3d` → `b7e269e`, exact 48-file native closure | 20,702 checks/configuration, 250 cursor metadata executions/259 typed feeds; 3.062/12.734 s optimized/O1; 38.284 s including builds, cheap Header controls and supplementary emission |
| Adjacent native Table chain | Already-built `3b1ad3d` binaries, same 48-file closure | Original 28,602 checks/configuration /193 metadata executions /202 feeds; 2.946/13.440 s optimized/O1, 16.387 s combined; no rebuild |
| Integrated dedicated transport | `66c0de4` | 61,274 C checks/configuration, preserving 60,863 earlier checks plus 411 new; 131 Node groups, 9.413 s including build; synthetic memory-only transport |
| Actual official Worker | Frozen `66c0de4`, artifact `8ea98279dccec9a2fce8` | 132,097 C /690 external checks; four archives, six Headers, 176 requests, eight rejections and one stale callback; 21.194 s CLI /21.526 s parent |
| Shipping and broader retention | Frozen `66c0de4` | Fresh normal/review Wasm exclusion plus Worker247, 55.741 s; four original metadata profiles, 50.418 s; no new GUI, bundle, service or delivery claim |

[Source comparison](source-comparison.json) identifies named component equality and the native compiler closure at `b7e269e`; the [frozen 66c0de4 comparison](source-comparison-66.json) confirms those same 48 compiled native files and separately names browser wiring/transport inputs. [Provenance](provenance.json) records unmodified receipt/log paths, sizes and SHA256 values. No executable, workbook, generated database, source dataset or event stream is copied here. The named native, transport and browser gates retain separate execution receipts; source comparison does not imply another runtime pass.

## Definition and ownership boundary

`workbook_codec_read_add_pivot_marks` arms once after completed descriptors, raw queries, source options and the Table source/filter/sort chain. It requests Pivot fields followed by Marks from one immutable result per family, with atomic feeds of at most 256 rows. Only complete-chain getters return a genuine Header-owned `PivotDefinition` or `MarkDefinition`; invalid, Table-owner or not-ready lookups preserve the caller's output. Empty analytical objects return genuine empty definitions.

The exact original Pivot schema is `(sheet_id INTEGER, object_id UBIGINT, role INTEGER, position INTEGER, name VARCHAR, aggregate INTEGER, filter_op INTEGER, value VARCHAR, grain INTEGER)`, with only `value` nullable. Marks uses `(sheet_id INTEGER, object_id UBIGINT, role INTEGER, position INTEGER, name VARCHAR, is_measure BOOLEAN, aggregate INTEGER, grain INTEGER)`, all nonnull. Named projections order by sheet, owner, role and position. Full uint64 object identity, including values above 2^53 and `UINT64_MAX` Table bindings, remains exact.

With an objects table present, even empty, both child tables are required for writer versions 12–14. Absent v12 objects can default empty and ignore the children, matching native loading. Later object packages require their descriptor table. The decoder intentionally checks original types, required NULLs, UTF-8 and embedded NUL rather than inheriting permissive historical numeric coercions. It never infers a source schema or runs stored queries.

Pivot shelves allow eight fields each, enforce contiguous positions, preserve date grains and validate existing filter grammar. A same-name/date-grain dimension cannot appear on both Rows and Columns; Values can retain distinct aggregates of the same field. Nonzero presentation measure indices must fit the completed Values shelf, while zero is valid for an empty definition. That arity check completes with Pivot fields, before Marks. Marks keeps six roles: Size and Shape accept one field each, the other four accept eight. Dimension duplicate identity ignores aggregate; measure duplicate identity includes aggregate. Actual Table owners may own neither family.

Pivot's coarse cap is 65,536 rows plus its 65,537th sentinel. Marks' native coarse model cap is `64*32*6*8 = 98,304`; the new canonical request uses 98,305 including the sentinel. The tighter Size/Shape rules permit at most 69,632 valid Marks rows. Native's historical `LIMIT 196609` fetch is a loose SQL ceiling, not a valid model capacity or a claim of accepted 196,608-row data.

Names are bounded at 4,096 bytes; Pivot values retain their native 8 MiB individual limit rather than a query's 64 KiB or a Table filter's 4 MiB limit. Each family's raw text has a separate 8 MiB per-sheet bound. Owned admission counts the reader, retained independent output/input, staged structures/text, old/new prospective definitions and conservative native-validator parser scratch. Known native-current accounting now includes completed Table definitions/queries, object/name terms, Pivot names/values and Marks names. Empty unavailable browser locators remain a lower bound on future native path ownership. Cells, complete placement, allocation counters, format/source ledgers and final Workbook validation remain open. These are logical owned-buffer limits, excluding allocator, diagnostics, JavaScript, engine and RSS peaks.

The [private pure receipt](components/private-reader/manifest.json) records full valid 65,536-Pivot/69,632-Marks model cases separately from the compact native gate. The [unchanged prior-test witness](components/private-reader/unchanged-table-prefix.json) preserves 141,125 existing assertion/helper bytes; it is not a native compiler closure or a runtime claim for another configuration.

## Dedicated transport acceptance

The explicit `workbook-pivot-fields` envelope admits 65,537 rows ×9 columns and `workbook-marks` admits 98,305 ×8, offset 0, unknown total and exactly one captured archive registration. The wrappers reuse the existing DXP1 parser; normal 256-row, copy and earlier metadata envelopes retain their bounds. Source-family semantics and original schemas remain C cursor/adapter responsibilities, not permission inferred from SQL or a wire-selected generic row limit.

The [integrated transport receipt](transport/integrated/manifest.json) passes 61,274 C checks per optimized/O1 configuration and 131 Node groups, including 86 focused adapter/wire groups plus 45 adjacent groups. It retains the original 60,863 C assertions and adds 411. Build and checks take 9.413 s. These are memory-only transport checks, not another official engine or App run.

Sparse maximum envelopes preserve exact uint64/raw numeric sidecars, nullable/empty/literal text and independent owned Pages, with exact-limit/one-under admission, wrong shape and stale captured-registration recovery tests. The [C log](transport/integrated/release.txt) measures 14,157,153 Page-owned /2,949,601 wire bytes for 65,537×9 and 18,875,602 Page-owned /3,932,599 wire bytes for 98,305×8. They explicitly use a 32 MiB caller output reservation. They do not demonstrate that a full semantic dataset fits the actual Worker's combined 4 MiB input bound, a natural working set or process/RSS peak. The 4 MiB text/6 MiB wire ceilings remain unchanged.

## Native comparison and closed fixture publication

The [native receipt](native/manifest.json) accepts 20,702 checks per optimized/O1 ASan+UBSan configuration, 250 cursor metadata executions and 259 feeds. Three small archives contain one worksheet with an empty Pivot, a forward Table-bound Pivot, a primary-bound Chart and one Table. The reordered v14 archive has two worksheets, each with 16 analytical objects and one Table. Its 288 Pivot rows feed 256+32; 320 Marks rows feed 256+64. This is representative composition under the native test's explicit 2 MiB reader/input admission, not full-capacity transport or process-memory proof.

Full definitions include Year/Quarter dimensions of the same field, a Month column, AVG/distinct values of the same measure, real NULL/empty/literal `NULL` filter values and a BETWEEN predicate. A pre-Save assertion protects the nullable factory contract; no `dx_strdup(NULL)` normalization is used. Both source-bound and primary-bound views retain exact source IDs, presentation indices, query bytes and true/default-false flags. The source files are deliberately absent and stored sentinel SQL never executes.

The native oracle loads actual native-writer archives, compares every definition property and metadata identity after DuckDB handles close, then frees the native model and verifies Header ownership. Original archives and the old independent Header survive required-family, NULL, NUL, wrong-owner, schema, owned-budget and stale failures. Late Pivot position/Rows–Columns/measure-arity failures and late Marks gap/duplicate failures retain accepted 256-row prefixes. A separate fresh valid read demonstrates recovery. Empty-present families cover all three writer versions; absent v12 descriptors exercise the native early-return default. Full maximum row/sentinel cases remain in pure/transport scope, not the 288/320-row native workload.

[Immutable four-file publication](native/emitter-results.json) verifies actual native `pivot-marks-v12.dxls`, `pivot-marks-v13.dxls`, `pivot-marks-v14.dxls` and `pivot-marks-v14-reordered.dxls`. It checks independent read-only closed files, exact projected Pivot/Marks/object/query/options values, the nullable triad, physical column reordering, unchanged bytes after inspection and refusal to overwrite existing files/symlinks/source placeholders. Late conflict cleanup removes only the emitter's own published inode and retains the independent conflicting file. Emission takes 0.364/2.107 s optimized/O1, with the optimized 30 s ceiling preserved. Independent emissions retain their own storage hashes; no byte equality between separately created archives is assumed.

Native builds and gates take 34.262 s; supplementary publication/inspection/refusal checks take 4.021 s, 38.284 s combined. Optimized/O1 Header controls retain the original 1,611 checks/21 executions/35 feeds. Expensive maximal query/options gates were not repeated. Source changes to their instrumentation are conditional only on the two new families. ASan/UBSan is run with leak detection disabled on this macOS; no leak-sanitizer claim is made.

[Exact compiler closure](native/preprocessor-closure.json) verifies all 48 native project inputs at tested `3b1ad3d` and integrated root `b7e269e`, plus installed DuckDB 1.5.6 header/library hashes. Dependency extraction takes a separate 1.575 s. Browser-only tests/build wiring and unused headers are outside that closure; system/Apple SDK headers omitted by `clang -MM` remain excluded.

The first strict test compilation rejected misleading indentation in the new stamp helper; its [original diagnostic](native/initial-compile-rejected.txt) and [receipt](native/initial-compile-rejected.json) are retained. Moving the return onto its own line preceded the passing gates. This is a test implementation correction, not a product natural red or weakened semantic assertion.

The [adjacent native Table receipt](adjacent/native-tables/manifest.json) separately reruns the original 64-Table/512-filter/448-sort workload with these already-built binaries. Both configurations retain 28,602 checks /193 executions /202 feeds in 2.946/13.440 s. This verifies the changed shared Header ownership/current-accounting path without rebuilding or repeating maximal old query/options modes; the primary native receipt above remains unchanged.

## Actual official Worker

The [complete terminal C report](worker/c-report.json) observes four independently emitted native archives, six complete Headers and the exact 176-request workload: 41 original-schema DESCRIBEs; 14 each of meta, sheets, descriptors, source options, raw queries, Table sources, Table filters, Table sorts and Pivot fields; and nine Marks executions. The original schemas are checked before canonical casts. Stored source SQL is absent from every executed request, and the same captured archive epoch/registration token remains attached to each request.

Full 288-field and 320-Mark results feed as 256+32 and 256+64 from one execution. The observed accepted feeds retain nine Pivot256/six Pivot32/three Pivot18 and five Marks256/three Marks64/three Marks20 batches across positive, failed-prefix and recovery cases. Seven reasoned C feed failures plus one original-type schema failure give eight rejections; the rejected prefix positions remain `[256,0,256,256,256,256,0]`. Required Pivot NULL, Table ownership, cross-grain, measure arity, required Marks BOOLEAN, duplicate Marks, owned budget and original-type flags all pass. One already-completed held callback is superseded and released; old independent Header identity/contents survive before valid recovery.

Owned temporary results are poisoned after decoding: 14 descriptors, 11 source-options, 14 query, 14 Table-source, nine Pivot and six Marks results. Empty Table child results contain no poisonable cells. Completed definitions survive Page/catalog retirement; browser locators remain independently owned empty unavailable strings rather than guessed source availability.

[Lossless transport deduplication](worker/transport-deduplicated.json) retains 27 exact SQL strings, four binding vectors and reconstructs every original request dictionary. The [independent audit](worker/receipt-audit.json) observes 69 normal, 14 objects, 28 owners, 14 each Table-source/filter/sort and Pivot-field, plus nine Marks requests. Per-registration counts are 20/20/20/116. Native SDK 1.5.6 and official engine v1.4.3 remain separate scopes.

The [four Worker inputs](worker/codec-native-provenance.json) are real closed native-writer archives of 4,206,592 /4,468,736 /5,517,312 /8,663,040 bytes. Their storage hashes belong to this independent emission and are preserved after teardown; archives themselves are omitted here. Maximum completed Header ownership is 104,026 bytes. Peak observed owned input 2,097,214 includes deliberate pressure; this is not natural working set, allocator, JavaScript, engine or RSS peak. Actual Header/input limits remain 2 MiB/4 MiB, separate from the sparse transport's 32 MiB reservation.

The [runtime summary](worker/summary.json) pins `66c0de4` and its then-observed untracked `tools/ui-review/` status rather than relabeling it clean. Artifact `8ea98279dccec9a2fce8` is freshly built from that source; source/artifact bytes remain unchanged through the run. CLI total 21.194 s comprises 10.906 s helper/build preparation, 0.777 s server setup and 8.940 s browser work plus receipt overhead; the standalone artifact compiler reports 4.377 s within that preparation. Parent subprocess is 21.526 s. The [integration batch](worker/integration/manifest.json), including harness5/runner32 tests, takes 24.334 s. This is an isolated metadata driver with a private headless Worker, not an App, visual, physical-input or queue-delivery gate.

## Fresh exclusion and adjacent retention

The [fresh shipping batch](shipping/manifest.json) passes in 55.741 s: normal/review Wasm build/exclusion 37.051 s and the broader Worker subprocess 18.689 s. Fresh normal `d3f486612c7af4fc59df` and review `795e3df7534d6e0071e6` builds report 15.406/18.501 s compilation. [Artifact verification](shipping/artifact-scope.json) independently matches actual Wasm hashes, source `66c0de4`, source inventories and test-only callback/driver marker absence. The partial reader remains absent from application source inventories. The [absence report](shipping/review-absence.json) also inspects existing native symbols, without a new native build, GUI or package-source attribution.

[Four original metadata workflows](adjacent/metadata/manifest.json) pass serially in 50.418 s using the same frozen driver. The [bounded terminal reports](adjacent/metadata/reports.json) preserve:

| Adjacent mode | C /external checks | Requests | CLI /parent seconds |
| --- | --- | --- | --- |
| Table sources | 133,686 /498 | 126 | 8.675 /9.082 |
| Source options | 1,090,277 /308 | 79 | 16.999 /17.414 |
| Raw queries | 1,045,788 /341 | 85 | 13.737 /13.954 |
| Object descriptors | 700,762 /221 | 58 | 9.557 /9.965 |

These retain their original archive/Header/rejection contracts while exercising the changed shared Header ownership; their older native max-query/options acceptance is not rerun or relabeled.

The [actual Worker247 terminal report](adjacent/worker/worker-driver.json) observes 125,641 C checks and retains the exact 249 jobs/529 SQL/21 counts across the sequential Worker scopes. The [Other workload](adjacent/worker/other-workloads.json) reports one pending-start release and one pending check in this run, plus source-drop/native-stamp rejection. Timed polling can vary assertion totals; the recorded workload/barrier values are not copied from an earlier run. CLI/browser durations are 18.108/15.449 s; its parent subprocess is 18.689 s.

No native GUI, bundle refresh, service replacement, user tab reload, live queue delivery or App172 gate occurs in this slice. Primary secondary filters, column/cell families, allocation counters, complete format/source ledgers, historical migration, final candidate validation and browser Open/Save remain open. Full valid metadata sets may exceed the representative input budget; bounded streaming/admission and later production publication need their own acceptance. Logical owned-buffer limits remain distinct from allocator, JavaScript, engine and RSS peaks.

The checkpoint seal is [hashes.json](hashes.json). Earlier Table acceptance remains scoped to `8770a29`; this checkpoint records the new `66c0de4` definition and retention gates without rewriting that history.
