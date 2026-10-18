# Dormant current-owner source bindings

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

This checkpoint accepts family20 of the shared C **partial Header**: complete current-owner binding metadata after owned source definitions, queries/options, Table definitions, independent counters and the RAW ledger. Binding versions and logical-locator comparison status live in separate PODs. Every Header `Analysis.source_version` remains zero. This is not source activation, a complete Workbook or browser App Open/Save.

The actual first-pass official Worker gate at `383527c` passes **174,408 C /717 external checks**, four immutable native archives, five completed Headers and166 metadata requests. The adjacent ledger gate passes on the same immutable artifact. Fresh normal/review Wasm builds and stronger test-only marker/export exclusion pass at later tool-only revision `e7c75be`; their scope is separate from runtime acceptance.

## Tested layers

| Layer | Source | Result and elapsed scope | Receipt |
| --- | --- | --- | --- |
| Pure family20 | private `4e188e7` → root `8d49c26` | 38,778/configuration, including35,033 byte-preserved prior checks;17.673 s builds/runs | manifest (archived `pure/manifest.json`), preservation (archived `pure/original-test-preservation.json`) |
| Root pure linkage | `8d49c26` | 38,778/configuration;18.657 s build +0.886/1.267 s runs;20.983 s enclosing runner | manifest (archived `root-pure/manifest.json`), summary (archived `root-pure/summary.json`) |
| C Worker controller authoring | private `72deff9` | static-only receipt; no compile or runtime claim | receipt (archived `static-controller/manifest.json`) |
| Native SDK oracle | private `8a10ad8`, pure dependency `4e188e7` | 186,344 checks /286 metadata cursor executions /359 feeds per configuration;4.421 s optimized /32.703 s O1 ASan+UBSan | manifest (archived `native/manifest.json`), closure (archived `native/preprocessor-closure.json`) |
| Actual official Worker + C | `383527c`, artifact `9505f7e11a465548330e` | 174,408 C /717 external;9.360 s browser /19.726 s CLI | report (archived `actual-worker/result.extract.json`), wrapper (archived `actual-worker/summary.extract.json`), transport (archived `actual-worker/transport.packed.json`) |
| Adjacent ledger | same source/artifact | 176,738 C /460 external;98 requests, five Headers;8.002 s browser /9.718 s CLI | manifest (archived `adjacent-ledger/manifest.extract.json`), report (archived `adjacent-ledger/result.extract.json`) |

The native control separately retains1,611 Header checks /21 cursor executions /35 feeds per configuration, optimized0.572/O12.677 s. Its final focused batch takes53.342 s including12.955 s O1 build and controls. A separately retained actual optimized compile takes5.498 s and its run4.926 s; these are separate invocations, not extra feature acceptance totals. Four-file emission takes1.307/6.045 s; all emission, inspection, refusal and late-conflict checks take10.592 s. Original native records (archived `native/emitter-results.json`)

Private pure tests link declaration headers without an engine library. Native tests use the actual SDK `v1.5.6`; the official Worker reports `v1.4.3`. ASan+UBSan coverage is retained; LeakSanitizer is disabled on the macOS toolchain. Neither logical admission nor copied-batch measurements establish allocator, Arrow, engine or process RSS bounds.

## Source/provenance boundary

The native root comparison (archived `native/root-source-comparison.json`) matches all54 actual `clang -MM` project inputs plus installed SDK files between tested `8a10ad8` and root integration `eaea747`. The accepted-source comparison (archived `source-closure-comparison.json`) independently matches those54 native inputs and the pure24-input closure to committed `383527c`, without a new compile/runtime. System SDK files omitted by `-MM` are outside that comparison.

The fresh durable harness guard receipt (archived `harness/codec-guards-recorded.json`) records21 tests (1.332 s unit /1.488 s wrapper) at later tool-only `e7c75be`, including corrupted binding-report guards. Earlier passing stdout is not retroactively relabeled. The existing 32 runner guards (archived `harness/runner.txt`) retain their original2.200 s scope.

The original driver build (archived `driver/build.json`) honestly records dirty pre-commit `7ebb53d`,6.095 s,13 rebuilt C sources and the immutable signature. Its source fingerprint is identical to accepted clean runtime `383527c`. The reconstruction (archived `source-fingerprint.json`) verifies that exact322-file fingerprint from319 committed files plus hashes of three unchanged pre-existing `tools/ui-review` files; those three are not driver C inputs. The source witness (archived `source-witness.json`) separately inventories declared driver C sources, conservative project headers/includes and selected host/test/build files. It is not an exact browser preprocessor closure. No original build revision is relabeled.

Provenance (archived `provenance.json`) retains original paths, byte lengths, SHA256 and extraction recipes. The summary/aggregate extracts omit only the identical artifact object retained once in `driver/build.json`. Result extracts omit their duplicate transport; `codec-read-driver.json` is the same full report. Transport packing interns request templates and binding sets while preserving every parsed query value and sequence. Reconstruct it with the recipe inside each packed file. Original formatting and omitted full browser event streams stay identified by their original hashes. No databases, binaries, SDK/vendor payloads, screenshots, queue data or full event streams are copied here.

## Family20 contract

Arming occurs once after complete Tables and source versions, before Header claim. Table completion implies objects/queries/options; the ledger implies both independent counters. Versions12/13 forbid a present binding table and default all valid current owners to zero without binding SQL. Version14 requires the table and exact current-owner coverage:

```sql
SELECT sheet_id,owner,version
FROM _duxcel_workspace.source_bindings
ORDER BY sheet_id,owner LIMIT 2113
```

Original INTEGER/UBIGINT/UBIGINT types and non-NULL values are required. At most2,112 current owners are ordered strictly by `(sheet,owner)`, with a2,113-row overflow sentinel. Every worksheet has primary owner0, including SOURCE_NONE, followed only by its live actual Table owners. Analytical, retired, foreign and nonexistent owners cannot substitute for a current slot. Owner/version retain exact full uint64 values.

Version0 is unbound with `locator_matched=false`. A nonzero binding must resolve the exact same-sheet ledger owner/version and source kind; pending and complete RAW entries are both allowed. Two nonempty current/ledger logical locators must match exactly; a match reports true without proving file content, schema, engine compatibility or capability. Either unavailable locator defers comparison and reports false; empty-equals-empty is never proof. The getter preserves its output on unready/invalid/analytical/retired ownership. No `Analysis.source_version`, source issuer or activation token is assigned.

Arming/feed allocate no heap or parser scratch. Existing sizeof-based Header/Object ownership charges the fixed POD storage. Allocation-free operations still admit all caller-retained input plus the reader/output under the explicit budget. Two-pass feeds validate the whole batch, ordered coverage and terminal EOF before publishing PODs. Failure leaves prior accepted prefix, cursor/SQL, bytes, borrowed input and previous output graph unchanged. Stale request keys reject before borrowed-pointer access.

## Native writer fixtures and dormant RAW

The four files are `source-bindings-v12.dxls`, `source-bindings-v13.dxls`, `source-bindings-v14.dxls` and `source-bindings-v14-reordered.dxls`, each with64 sheets named `Source bindings 'İ' s`,56 opaque object descriptors and18 actual Tables. Therefore each has82 current binding slots. Three file-backed primaries are CSV on sheet0 and Parquet on sheets1/2; others are SOURCE_NONE. Tables are CSV except sheet6's owner9007199254740995, which is Parquet. All live sources store `SELECT * FROM __BP1_SOURCE_BINDINGS_MUST_NOT_EXECUTE`, empty schema and literal `NULL` table; blank primary query/schema/table remain NULL. All unique flags are false. Queries21, source options0, Table sources18 and empty Table filters/sorts are actual owned metadata families. No Pivot/Marks continuation is armed for the opaque analytical descriptors.

Older files have zero binding/source-version defaults and no forbidden ledger/binding queries. The small14 ledger has8 entries (4 pending/4 complete), while reordered14 has288 (97 pending/191 complete), fed256+32. Five nonzero current bindings remain:

| Sheet/owner | Version | Current kind and archived state |
| --- | --- | --- |
| 0/0 | 1 | CSV, pending |
| 1/0 | 9007199254740995 small /9007199254741020 large | Parquet, complete |
| 2/0 | 1 | Parquet, pending |
| 5/9007199254740993 | UINT64_MAX | CSV, complete |
| 6/9007199254740995 | 7 | Parquet, pending |

Native logical locators compare five matches. Separate both/current-only/ledger-only unavailable callback cases retain those versions with deferred false status. The actual browser callback returns independently owned empty current and ledger locators; all five nonzero versions survive with `locator_matched=false`. Current `Analysis.source_version` remains zero throughout both oracles.

Each complete147-byte RAW packet uses the previously accepted codec declarations: `Id UBIGINT`, `City 'İ' VARCHAR`, `Amount DECIMAL(38,4)`, native engine `v1.5.6`, file-size declaration9007199254741043 and deterministic SHA bytes `(7*i+3)&255`. These are fixture declarations, not observed source-file hashes. Pending entries retain NULL/zero packet state. Exact archive references, copied RAW bytes, Table/query pointers and POD identities survive input poisoning, source result/connection closure and native model cleanup. The native SaveAs test separates archive references from independent logical locators; no source SQL executes.

## Natural test corrections

The first native failure (archived `native/first-failure/initial-manifest.json`) stopped at Sheets: the new fixture has file-backed current primaries, whose existing decoder requires an explicit bounded resolver callback. A NULL callback was valid in the earlier all-SOURCE_NONE ledger fixture, and was the wrong unavailable test here. The correction returns independent owned empty strings and adds current-only/ledger-only unavailable cases. No production change was needed. The diagnostic (archived `native/diagnostic-release.txt`), original failure source hashes and reconstruction patch (archived `native/first-failure/reconstruct-initial.patch`) preserve the cause.

The pressure allocation source was corrected statically before that test path executed; it is not an observed pressure-test runtime failure. Final tests actually allocate the additional byte before proving budget denial, then retry at the exact admitted boundary. A closure witness script initially used old ledger-removal names; correcting that postprocessor recovered the unchanged wrapper. This was a file-only witness correction, with no C source drift or new runtime. Preserved wrapper witness (archived `native/unchanged-native-prefix.json`)

## Actual callback/ownership acceptance

The four archives are freshly emitted native files served privately to one owned headless official Worker. Creation protocol (archived `actual-worker/codec-native-create.txt`), sealed inventory (archived `actual-worker/codec-fixture-manifest.json`) and post-run provenance (archived `actual-worker/codec-native-provenance.json`) identify each invocation's immutable container bytes; private emission hashes need not equal these newly generated archives.

Actual normal requests are `[16,18,24,24]`; including failures/recovery they are `[16,18,24,108]`,166 total. Inventories are11 each of Meta, Sheets, Objects, Queries, Source Options, Table Sources/Filters/Sorts,10 Object Counters,9 Source Counters,9 Source Versions,9 Source Bindings and41 DESCRIBE. Metadata request counters exclude native Save/Load/inspection and browser attach/detach administration SQL. Every submitted request preserves its original registration token; the stored source query sentinel never executes.

The ledger dependency retains original seven-column sidecars including BLOB. Its C SQL hex projection has numeric qualified ordering; all26 observed stream chunks are physical Utf8 with original SQL types. JavaScript treats hex as opaque text. C preflights the whole chunk's length/parity/digits and atom/decoded arena budget before allocation, then passes owned BLOB staging through shared validation. Nine streams deliver17 data chunks +9 explicit empty EOFs,26 numeric-1 ACKs and9 done/retirement callbacks; no held stream/cancel occurs in this profile. All streams finish before the corresponding ordinary binding request. Seventeen nonempty dependency chunks are poisoned/freed after feeds.

Binding results82 rows are fed32+32+18 from one immutable result. Five rejections preserve prefixes `[64,64,64,64,0]`: required version NULL, foreign owner, missing final owner, dangling version and concrete **allocated** retained pressure at2MiB+1. Three binding results complete; eight adapted results, including rejected inputs, are poisoned. An additional genuinely completed ordinary binding reply is held, superseded and released. Its old callback reserves1,992 wire bytes +21 total-scalar bytes and rejects before Page/pointer adaptation. A whole fresh candidate then becomes the fifth completed Header. All prior Header/POD/query/Table/raw pointer/scalar/byte identities survive, including final catalog detach.

The actual Header maximum is116,014 bytes under2MiB; peak accounted C-boundary category is2,097,216 bytes under4MiB. Wire/Page/atom/decoded staging, current read, old Header, schemas, SQL and diagnostics are admitted; the reader separately reserves prospective copies and RAW scratch. These are bounded logical categories, not a precise transient allocator/stack peak or engine/Arrow/browser/RSS measure. Large pure/full-owner tests use explicitly larger budgets and do not promise every allowed archive fits this representative2MiB/4MiB driver. Independent retained-receipt audit (archived `receipt-audit.json`)

The adjacent ledger rerun preserves its original98-query failure/recovery workload: five Headers, eight rejections, stale old chunk and done,25 chunks/16 ACKs/12 done, exact RAW/NULL/empty and hex-adaptation proofs. Its Header maximum105,746 reflects the new fixed binding POD size, separately from the prior103,826-byte ledger source. Its peak remains2,097,216. This does not claim a fresh Grid/Worker247/App UI acceptance.

## Shipping and remaining scope

Fresh normal/review Wasm builds at tool-only `e7c75be` produce `c65fb47c307acc877cd7` /`c623f797fec07f86b881` in24.679/27.130 s, rebuilding58/62 objects in cold, newly separated mode namespaces. These are compile/absence receipts, not a cache speedup or new UI runtime claim. Shipping manifest (archived `shipping/manifest.json`), normal build (archived `shipping/app-build.extract.json`), review build (archived `shipping/review-build.extract.json`) and Git comparison (archived `shipping/source-comparison.json`) keep that later tooling scope separate; the source tree and original pure/native compiled closures remain byte-identical to runtime `383527c`.

Strong exclusion (archived `shipping/exclusion.json`) verifies both fresh Wasm packages omit the binding controller include/mode, codec callbacks and test-only markers; linked native shipping binaries retain their separately hashed symbol-absence scope. Normal Wasm excludes review exports while review Wasm includes the intended review callback. No new App/window execution, signed native bundle, service, queue or user browser tab is promoted by this checkpoint.

Pending and complete bindings remain dormant metadata. Native RAW `v1.5.6` bytes are not certified against Worker `v1.4.3`. File/content verification, compatible current source publication, issuer state, formats/migration, remaining partial families, final placement/model validation, whole-Workbook ownership/transactional adoption and browser App Open/Save remain open. The earlier [source-ledger checkpoint](../2026-10-18-workbook-source-ledger/) retains its own scopes; this capsule adds current-owner binding metadata only.
